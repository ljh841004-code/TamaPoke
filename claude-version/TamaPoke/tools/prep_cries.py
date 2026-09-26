#!/usr/bin/env python3
"""Prepara tus propios gritos para la SD: mons/cry001.wav ... (fork KO, ko9.1).

Coge una carpeta con WAV nombrados por numero de Pokedex ("1.wav", "001.wav",
"25 pikachu.wav", "cry_133.wav"...) y los deja como los lee el firmware:
    mons/cryNNN.wav  -  WAV PCM 16 kHz, mono, 16 bit, cabecera de 44 bytes
(el cargador de gritos de audio.cpp rechaza cualquier otra cosa en silencio:
otra frecuencia, estereo, 24 bit o bloques extra como LIST).

Solo Python 3 (sin ffmpeg ni librerias). Acepta PCM de 8/16/24/32 bit y float
de 32 bit, mono o estereo, cualquier frecuencia.

Uso:
    python tools/prep_cries.py CARPETA_CON_WAV [CARPETA_SALIDA]
    -> CARPETA_SALIDA/mons/cry001.wav ...   (por defecto ./sd_cries)
Luego copia la carpeta mons/ a la raiz de la tarjeta SD (se mezcla con la que
ya hay; los gritos del mismo numero se sustituyen).
"""
import array
import os
import re
import struct
import sys

RATE = 16000
MAX_DEX = 251
MAX_SECONDS = 30


def read_wav(path):
    """devuelve (muestras mono en float -1..1, frecuencia)"""
    with open(path, 'rb') as fh:
        data = fh.read()
    if data[:4] != b'RIFF' or data[8:12] != b'WAVE':
        raise ValueError('no es un WAV')
    pos, fmt, pcm = 12, None, None
    while pos + 8 <= len(data):
        cid, size = data[pos:pos + 4], struct.unpack('<I', data[pos + 4:pos + 8])[0]
        body = data[pos + 8:pos + 8 + size]
        if cid == b'fmt ':
            fmt = body
        elif cid == b'data':
            pcm = body
        pos += 8 + size + (size & 1)
    if fmt is None or pcm is None:
        raise ValueError('WAV sin fmt/data')
    tag, ch, rate = struct.unpack('<HHI', fmt[:8])
    bits = struct.unpack('<H', fmt[14:16])[0]
    if tag == 0xFFFE and len(fmt) >= 26:          # WAVE_FORMAT_EXTENSIBLE
        tag = struct.unpack('<H', fmt[24:26])[0]
    if tag not in (1, 3) or ch < 1:
        raise ValueError(f'formato no soportado (tag {tag})')
    step = bits // 8
    frames = len(pcm) // (step * ch)
    if tag == 3 and bits == 32:
        vals = array.array('f', pcm[:frames * ch * 4])
    elif bits == 8:
        vals = [(b - 128) / 128.0 for b in pcm[:frames * ch]]
    elif bits == 16:
        a = array.array('h', pcm[:frames * ch * 2])
        vals = [v / 32768.0 for v in a]
    elif bits == 24:
        raw = pcm[:frames * ch * 3]
        vals = [int.from_bytes(raw[i:i + 3], 'little', signed=True) / 8388608.0
                for i in range(0, len(raw), 3)]
    elif bits == 32:
        a = array.array('i', pcm[:frames * ch * 4])
        vals = [v / 2147483648.0 for v in a]
    else:
        raise ValueError(f'{bits} bit no soportado')
    mono = [sum(vals[i * ch:(i + 1) * ch]) / ch for i in range(frames)]
    return mono, rate


def resample(x, src):
    if src == RATE or not x:
        return list(x)
    n = int(len(x) * RATE / src)
    out = []
    ratio = src / RATE
    last = len(x) - 1
    for i in range(n):
        p = i * ratio
        j = int(p)
        f = p - j
        a = x[j] if j <= last else x[last]
        b = x[j + 1] if j + 1 <= last else a
        out.append(a + (b - a) * f)
    return out


def write_wav(path, samples):
    samples = samples[:RATE * MAX_SECONDS]
    pcm = array.array('h', (max(-32768, min(32767, int(round(v * 32767)))) for v in samples))
    if sys.byteorder != 'little':
        pcm.byteswap()
    body = pcm.tobytes()
    head = struct.pack('<4sI4s4sIHHIIHH4sI', b'RIFF', 36 + len(body), b'WAVE', b'fmt ', 16, 1, 1,
                       RATE, RATE * 2, 2, 16, b'data', len(body))
    with open(path, 'wb') as fh:
        fh.write(head + body)


def dex_number(name):
    m = re.search(r'\d+', os.path.splitext(name)[0])
    return int(m.group()) if m else None


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 1
    src = argv[1]
    out = os.path.join(argv[2] if len(argv) > 2 else 'sd_cries', 'mons')
    os.makedirs(out, exist_ok=True)
    done, skipped = [], []
    for name in sorted(os.listdir(src)):
        if not name.lower().endswith('.wav'):
            continue
        n = dex_number(name)
        if not n or n > MAX_DEX:
            skipped.append((name, 'sin numero de Pokedex (1-%d)' % MAX_DEX))
            continue
        try:
            mono, rate = read_wav(os.path.join(src, name))
        except (ValueError, struct.error) as e:
            skipped.append((name, str(e)))
            continue
        write_wav(os.path.join(out, f'cry{n:03d}.wav'), resample(mono, rate))
        done.append(n)
    for name, why in skipped:
        print(f'  omitido: {name} ({why})')
    missing = [n for n in range(1, 152) if n not in done]
    print(f'{len(done)} gritos listos en {os.path.abspath(out)}')
    if missing:
        print('  faltan (1-151): ' + ', '.join(map(str, missing)))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
