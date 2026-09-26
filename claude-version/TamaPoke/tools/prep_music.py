#!/usr/bin/env python3
"""Prepara la musica de fondo para la SD (fork KO, ko10.4).

El firmware toca /mons/bgm.wav (normal) y /mons/battle_wild.wav (batalla) de
principio a fin y vuelve a empezar. Tienen que ser WAV PCM 16 kHz, mono, 16 bit;
si no, no suenan. Este script convierte un WAV cualquiera (otra frecuencia,
estereo, 24/32 bit...) SIN recortarlo (los gritos se cortan a 30 s; la musica no).

Uso:
    python tools/prep_music.py CANCION.wav            -> sd_music/mons/bgm.wav
    python tools/prep_music.py CANCION.wav battle     -> sd_music/mons/battle_wild.wav
Luego copia el fichero a la carpeta mons/ de la SD (sustituye al anterior).
Solo Python 3. Para MP3, conviertelo antes a WAV (o pasalo por ffmpeg:
    ffmpeg -i cancion.mp3 -ac 1 -ar 16000 -sample_fmt s16 bgm.wav).
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import prep_cries  # noqa: E402  (lector/remuestreo/escritor de WAV)


def resample_hq(mono, rate):
    """con numpy+scipy: filtro anti-aliasing (resample_poly); si no, el lineal
    de prep_cries (vale, pero a 16 kHz los agudos de la musica suenan asperos)"""
    try:
        from math import gcd

        import numpy as np
        from scipy.signal import resample_poly
    except ImportError:
        return prep_cries.resample(mono, rate)
    if rate == prep_cries.RATE:
        return list(mono)
    g = gcd(prep_cries.RATE, rate)
    y = resample_poly(np.asarray(mono, dtype=np.float64), prep_cries.RATE // g, rate // g)
    return y.tolist()


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 1
    src = argv[1]
    name = 'battle_wild.wav' if len(argv) > 2 and argv[2].startswith('b') else 'bgm.wav'
    out_dir = os.path.join('sd_music', 'mons')
    os.makedirs(out_dir, exist_ok=True)
    mono, rate = prep_cries.read_wav(src)
    pcm = resample_hq(mono, rate)
    # suave: -1 dB de techo para que al mezclar con gritos y efectos no sature
    peak = max((abs(v) for v in pcm), default=0)
    if peak > 0.89:
        pcm = [v * 0.89 / peak for v in pcm]
    keep = prep_cries.MAX_SECONDS
    prep_cries.MAX_SECONDS = 24 * 3600  # sin recorte
    try:
        prep_cries.write_wav(os.path.join(out_dir, name), pcm)
    finally:
        prep_cries.MAX_SECONDS = keep
    sec = len(pcm) // prep_cries.RATE
    print(f'{os.path.join(out_dir, name)}: {sec // 60}:{sec % 60:02d} (16 kHz mono 16 bit)')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
