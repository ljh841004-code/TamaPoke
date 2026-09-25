"""Installer dry runs: no physical serial device or flash operation is used."""
import hashlib
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import types
import unittest
from unittest.mock import MagicMock, patch

# Allow CI without hardware serial dependencies.
serial_stub = types.ModuleType('serial')
serial_tools = types.ModuleType('serial.tools')
serial_tools.list_ports = types.SimpleNamespace(comports=lambda: [])
sys.modules.setdefault('serial', serial_stub)
sys.modules.setdefault('serial.tools', serial_tools)
spec = importlib.util.spec_from_file_location('installer_update', Path(__file__).parents[1] / 'installer/update.py')
update = importlib.util.module_from_spec(spec)
spec.loader.exec_module(update)


class InstallerTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        (self.root / 'mons').mkdir()
        self.entries = {'TamaPoke.ino.bin': b'app', 'partitions.bin': b'partition',
                        'mons/bgm.wav': b'normal', 'mons/battle_wild.wav': b'wild'}
        for name, data in self.entries.items():
            (self.root / name).write_bytes(data)
        (self.root / 'SHA256.json').write_text(json.dumps({n: hashlib.sha256(b).hexdigest() for n, b in self.entries.items()}))
        self.ser = MagicMock()
        self.ser.__enter__.return_value = self.ser
        self.calls = []

    def tearDown(self):
        self.temp.cleanup()

    def flash(self, port, *args):
        self.calls.append(args)
        if args[0] == 'read-flash':
            with Path(args[-1]).open('wb') as f:
                f.truncate(0x1000000)
                f.seek(0x8000)
                f.write(self.entries['partitions.bin'])

    def run_update(self, *args):
        with patch.object(update, 'ROOT', self.root), patch.object(update, 'esptool', side_effect=self.flash), \
                patch.object(update, 'open_serial', return_value=self.ser), patch.object(update, 'wait_line'), \
                patch.object(update.time, 'sleep'), patch.object(sys, 'argv', ['update.py', '--port', 'MOCK', *args]):
            update.main()

    def test_default_only_updates_app_and_keeps_cries(self):
        for path in (self.root / 'mons').iterdir():
            path.unlink()
        self.run_update()
        self.assertEqual([c[0] for c in self.calls], ['read-flash', 'write-flash', 'verify-flash'])
        self.assertEqual(self.calls[1][1], '0x10000')
        self.assertFalse(any(c.args[0].startswith(b'PUT') for c in self.ser.write.call_args_list))

    def test_audio_only_transfers_exactly_two_music_files(self):
        (self.root / 'mons/cry001.wav').write_bytes(b'leave-me')
        self.run_update('--audio-only')
        self.assertFalse(self.calls)
        commands = [c.args[0] for c in self.ser.write.call_args_list if c.args[0].startswith(b'PUT')]
        self.assertEqual(commands, [b'PUT mons/bgm.wav 6\n', b'PUT mons/battle_wild.wav 4\n'])
        self.assertEqual((self.root / 'mons/cry001.wav').read_bytes(), b'leave-me')

    def test_missing_or_changed_music_fails_before_flash(self):
        (self.root / 'mons/bgm.wav').write_bytes(b'wrong')
        with self.assertRaises(RuntimeError):
            self.run_update('--with-audio')
        self.assertFalse(self.calls)

    def test_partition_mismatch_stops_before_write(self):
        original = self.flash
        def mismatch(port, *args):
            original(port, *args)
            if args[0] == 'read-flash':
                with Path(args[-1]).open('r+b') as f:
                    f.seek(0x8000)
                    f.write(b'WRONG')
        self.flash = mismatch
        with self.assertRaises(RuntimeError):
            self.run_update()
        self.assertEqual([c[0] for c in self.calls], ['read-flash'])


if __name__ == '__main__':
    unittest.main()
