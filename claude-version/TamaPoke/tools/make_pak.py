#!/usr/bin/env python3
"""ko12.8: empaqueta la carpeta mons/ de la SD en UN fichero cifrado (mons.pak).

Uso:
  python3 tools/make_pak.py <carpeta mons> <salida mons.pak> [--pass FRASE]
  python3 tools/make_pak.py --verify mons.pak [--pass FRASE]
  python3 tools/make_pak.py --extract mons.pak <carpeta> [--pass FRASE]

La frase por defecto es la misma que trae el firmware ("tamapoke-ko"). Si usas otra,
escribela tambien en el aparato (consola serie: PAKPASS <frase>).
Formato: ver pak_core.h (AES-128 CTR por posicion absoluta).
Rapido con el paquete "cryptography" (pip install cryptography); si no esta, usa un
AES en Python puro (funciona, pero tarda varios minutos con ~270 MB).
"""
import argparse
import hashlib
import os
import struct
import sys

MAGIC = b'TPAK'
HDR = 64
NAME_MAX = 40
DEFAULT_PASS = 'tamapoke-ko'
CHECK = b'TAMAPOKE-PAK-OK!'
SKIP = ('update.bin', 'update_done.bin')

# ------------------------------------------------------------------ AES-128 (python puro)
_SBOX = bytes.fromhex(
    '637c777bf26b6fc53001672bfed7ab76ca82c97dfa5947f0add4a2af9ca472c0b7fd9326363ff7cc34a5e5f171d8311504c723c31896059a071280e2eb27b27509832c1a1b6e5aa0523bd6b329e32f8453d100ed20fcb15b6acbbe394a4c58cfd0efaafb434d338545f9027f503c9fa851a3408f929d38f5bcb6da2110fff3d2cd0c13ec5f974417c4a77e3d645d197360814fdc222a908846eeb814de5e0bdbe0323a0a4906245cc2d3ac629195e479e7c8376d8dd54ea96c56f4ea657aae08ba78252e1ca6b4c6e8dd741f4bbd8b8a703eb5664803f60e613557b986c11d9ee1f8981169d98e949b1e87e9ce5528df8ca1890dbfe6426841992d0fb054bb16')


def _xt(x):
    return ((x << 1) ^ 0x1b) & 0xff if x & 0x80 else x << 1


class _PyAes:
    def __init__(self, key):
        rk = list(key)
        rcon = [1, 2, 4, 8, 16, 32, 64, 128, 0x1b, 0x36]
        r = 0
        while len(rk) < 176:
            t = rk[-4:]
            if len(rk) % 16 == 0:
                t = [_SBOX[t[1]] ^ rcon[r], _SBOX[t[2]], _SBOX[t[3]], _SBOX[t[0]]]
                r += 1
            rk += [rk[-16 + k] ^ t[k] for k in range(4)]
        self.rk = rk

    def block(self, b):
        rk = self.rk
        s = [b[i] ^ rk[i] for i in range(16)]
        for r in range(1, 11):
            t = [_SBOX[x] for x in s]
            u = [t[((c + row) % 4) * 4 + row] for c in range(4) for row in range(4)]
            if r < 10:
                for c in range(4):
                    a0, a1, a2, a3 = u[c * 4:c * 4 + 4]
                    al = a0 ^ a1 ^ a2 ^ a3
                    u[c * 4] ^= al ^ _xt(a0 ^ a1)
                    u[c * 4 + 1] ^= al ^ _xt(a1 ^ a2)
                    u[c * 4 + 2] ^= al ^ _xt(a2 ^ a3)
                    u[c * 4 + 3] ^= al ^ _xt(a3 ^ a0)
            s = [u[i] ^ rk[r * 16 + i] for i in range(16)]
        return bytes(s)


class Cipher:
    """AES-128 ECB de bloques sueltos + CTR por posicion absoluta (como pak_core.cpp)."""

    def __init__(self, key, force_py=False):
        self.key = key
        self.fast = None
        if not force_py:
            try:
                from cryptography.hazmat.primitives.ciphers import Cipher as C, algorithms, modes
                self.fast = (C, algorithms, modes)
            except Exception:
                self.fast = None
        self.py = _PyAes(key)

    def block(self, b):
        if self.fast:
            C, algorithms, modes = self.fast
            e = C(algorithms.AES(self.key), modes.ECB()).encryptor()
            return e.update(b) + e.finalize()
        return self.py.block(b)

    def crypt(self, salt, abs_off, data):
        if not data:
            return b''
        blk, skip = divmod(abs_off, 16)
        if self.fast:
            C, algorithms, modes = self.fast
            iv = salt + struct.pack('>Q', blk)
            e = C(algorithms.AES(self.key), modes.CTR(iv)).encryptor()
            ks = e.update(b'\0' * (skip + len(data)))[skip:]
            return bytes(a ^ b for a, b in zip(data, ks)) if len(data) < 64 else _xor(data, ks)
        out = bytearray(data)
        i = 0
        while i < len(out):
            ks = self.py.block(salt + struct.pack('>Q', blk))
            for k in range(skip, 16):
                if i >= len(out):
                    break
                out[i] ^= ks[k]
                i += 1
            skip = 0
            blk += 1
        return bytes(out)


def _xor(a, b):
    return (int.from_bytes(a, 'little') ^ int.from_bytes(b, 'little')).to_bytes(len(a), 'little')


def derive_key(passphrase):
    return hashlib.sha256(b'TamaPoke-pak:' + passphrase.encode('utf-8')).digest()[:16]


def collect(mons):
    files = []
    for root, dirs, names in os.walk(mons):
        dirs[:] = sorted(d for d in dirs if not d.startswith('.'))
        for n in names:
            if n.startswith('.') or n.endswith('.tmp') or n in SKIP:
                continue
            full = os.path.join(root, n)
            rel = os.path.relpath(full, mons).replace(os.sep, '/')
            if len(rel.encode('utf-8')) > NAME_MAX:
                print('  (se salta, nombre largo):', rel)
                continue
            files.append((rel, full))
    files.sort(key=lambda t: t[0].encode('utf-8'))
    return files


def build(mons, out, passphrase, salt=None, quiet=False, force_py=False):
    key = derive_key(passphrase)
    c = Cipher(key, force_py)
    salt = salt or os.urandom(8)
    files = collect(mons)
    entries = []
    tmp = out + '.tmp'
    with open(tmp, 'wb') as f:
        f.write(b'\0' * HDR)
        pos = HDR
        total = sum(os.path.getsize(p) for _, p in files)
        done = 0
        for i, (rel, full) in enumerate(files):
            size = os.path.getsize(full)
            entries.append((rel, pos, size))
            with open(full, 'rb') as src:
                off = 0
                while True:
                    chunk = src.read(1 << 20)
                    if not chunk:
                        break
                    f.write(c.crypt(salt, pos + off, chunk))
                    off += len(chunk)
            pos += size
            done += size
            if not quiet and (i % 50 == 0 or i + 1 == len(files)):
                print(f'  {i + 1}/{len(files)}  {done * 100 // max(1, total)}%', end='\r')
        tbl = b''.join(struct.pack('<IIB', o, s, len(r.encode())) + r.encode() for r, o, s in entries)
        idx_off = pos
        f.write(c.crypt(salt, idx_off, tbl))
        hdr = bytearray(HDR)
        hdr[0:4] = MAGIC
        struct.pack_into('<HHIII', hdr, 4, 1, 0, len(entries), idx_off, len(tbl))
        hdr[20:28] = salt
        hdr[28:44] = c.block(CHECK)
        struct.pack_into('<I', hdr, 44, HDR)
        f.seek(0)
        f.write(bytes(hdr))
    os.replace(tmp, out)
    if not quiet:
        print(f'\n{out}: {len(entries)} ficheros, {os.path.getsize(out) / 1e6:.1f} MB')
    return entries


def read_index(path, passphrase, force_py=False):
    key = derive_key(passphrase)
    c = Cipher(key, force_py)
    with open(path, 'rb') as f:
        h = f.read(HDR)
        if h[:4] != MAGIC:
            raise SystemExit('no es un mons.pak')
        ver, _, count, idx_off, idx_size = struct.unpack_from('<HHIII', h, 4)
        salt, check = h[20:28], h[28:44]
        if c.block(CHECK) != check:
            raise SystemExit('frase incorrecta (no se puede descifrar)')
        f.seek(idx_off)
        tbl = c.crypt(salt, idx_off, f.read(idx_size))
    out, p = [], 0
    for _ in range(count):
        o, s, l = struct.unpack_from('<IIB', tbl, p)
        p += 9
        out.append((tbl[p:p + l].decode('utf-8'), o, s))
        p += l
    return c, salt, out


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('paths', nargs='*')
    ap.add_argument('--pass', dest='pw', default=DEFAULT_PASS)
    ap.add_argument('--verify', action='store_true')
    ap.add_argument('--extract', action='store_true')
    a = ap.parse_args()
    if a.verify and len(a.paths) == 1:
        _, _, idx = read_index(a.paths[0], a.pw)
        print(f'OK: {len(idx)} ficheros')
        return
    if a.extract and len(a.paths) == 2:
        c, salt, idx = read_index(a.paths[0], a.pw)
        with open(a.paths[0], 'rb') as f:
            for name, o, s in idx:
                f.seek(o)
                dst = os.path.join(a.paths[1], *name.split('/'))
                os.makedirs(os.path.dirname(dst), exist_ok=True)
                with open(dst, 'wb') as g:
                    g.write(c.crypt(salt, o, f.read(s)))
        print(f'{len(idx)} ficheros en {a.paths[1]}')
        return
    if len(a.paths) != 2:
        ap.print_help()
        sys.exit(1)
    build(a.paths[0], a.paths[1], a.pw)


if __name__ == '__main__':
    main()
