"""Back up ESP32-S3 flash, update app only, then install audio over USB.
Run from the extracted release directory. Never erases NVS or partitions.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import time

import serial
from serial.tools import list_ports

ROOT = Path(__file__).resolve().parent
VERSION = '1.20'


def esptool(port, *args):
    subprocess.run([sys.executable, '-m', 'esptool', '--chip', 'esp32s3',
                    '--port', port, '--baud', '460800', *map(str, args)], check=True)


def wait_line(ser, expected, timeout=20):
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        line = ser.readline().decode(errors='replace').strip()
        if line == expected:
            return
        if line == 'ERR':
            raise RuntimeError('Device reported ERR; installation stopped.')
    raise TimeoutError(f'Device did not reply {expected!r}.')


def open_serial(port):
    ser = serial.Serial()
    ser.port = port
    ser.baudrate = 115200
    ser.timeout = 1
    ser.write_timeout = 10
    ser.dtr = False
    ser.rts = False
    ser.open()
    time.sleep(2)
    ser.reset_input_buffer()
    return ser


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--port', help='Optional explicit COM port')
    ap.add_argument('--with-audio', action='store_true', help='Also send the separate mons BGM folder over USB')
    ap.add_argument('--audio-only', action='store_true', help='Retry audio without reflashing')
    args = ap.parse_args()
    send_audio = args.with_audio or args.audio_only
    files = [ROOT / 'mons' / name for name in ('bgm.wav', 'battle_wild.wav')] if send_audio else []
    manifest = json.loads((ROOT / 'SHA256.json').read_text())
    for name, digest in manifest.items():
        if name.startswith('mons/') and not send_audio:
            continue
        path = ROOT / name
        if hashlib.sha256(path.read_bytes()).hexdigest() != digest:
            raise RuntimeError(f'Damaged installation file: {name}')
    for path in files:
        if not path.is_file():
            raise RuntimeError('Extract the separate SD ZIP here before requesting USB audio transfer.')
    ports = [p for p in list_ports.comports() if p.vid == 0x303A]
    if args.port:
        port = args.port
    elif len(ports) == 1:
        port = ports[0].device
    else:
        raise RuntimeError('Connect exactly one Espressif board, or run update.py --port COMx.')
    print(f'Target: {port}', flush=True)
    if not args.audio_only:
        backups = ROOT / 'backups'
        backups.mkdir(exist_ok=True)
        backup = backups / (time.strftime('%Y%m%d-%H%M%S') + '-flash.bin')
        print('Backing up all 16 MB of flash. Keep the cable connected.', flush=True)
        esptool(port, 'read-flash', '0', '0x1000000', backup)
        if backup.stat().st_size != 0x1000000:
            raise RuntimeError('Incomplete backup. Nothing flashed.')
        expected = (ROOT / 'partitions.bin').read_bytes()
        with backup.open('rb') as f:
            f.seek(0x8000)
            actual = f.read(len(expected))
        if actual != expected:
            raise RuntimeError('Partition table differs. Nothing flashed; keep backup and ask for review.')
        app = ROOT / 'TamaPoke.ino.bin'
        if app.stat().st_size > 0x300000:
            raise RuntimeError('App exceeds its partition. Nothing flashed.')
        print('Updating application only; saved game is retained.', flush=True)
        esptool(port, 'write-flash', '0x10000', app)
        esptool(port, 'verify-flash', '0x10000', app)
        time.sleep(4)
    with open_serial(port) as ser:
        ser.write(b'VERSION\n')
        wait_line(ser, 'VERSION ' + VERSION)
        wait_line(ser, 'DONE')
        for n, path in enumerate(files, 1):
            print(f'Audio {n}/{len(files)}: {path.name}', flush=True)
            ser.write(f'PUT mons/{path.name} {path.stat().st_size}\n'.encode())
            wait_line(ser, 'OK')
            with path.open('rb') as f:
                while chunk := f.read(2048):
                    ser.write(chunk)
                    wait_line(ser, '#')
            wait_line(ser, 'DONE')
        ser.write(b'REBOOT\n')
        wait_line(ser, 'DONE')
    time.sleep(5)
    with open_serial(port) as ser:
        ser.write(b'VERSION\n')
        wait_line(ser, 'VERSION ' + VERSION)
        wait_line(ser, 'DONE')
    print(f'SUCCESS: v{VERSION} verified, {len(files)} BGM files acknowledged, reboot confirmed.')


if __name__ == '__main__':
    try:
        main()
    except Exception as exc:
        print(f'UPDATE STOPPED: {exc}', file=sys.stderr)
        sys.exit(1)
