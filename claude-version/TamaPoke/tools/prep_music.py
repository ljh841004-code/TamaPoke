#!/usr/bin/env python3
"""Prepara la musica de fondo para la SD (fork KO, ko10.4).

El firmware toca /mons/bgm.wav (normal) y /mons/battle_wild.wav (batalla) de
principio a fin y vuelve a empezar. Tienen que ser WAV PCM 16 kHz, mono, 16 bit;
si no, no suenan. Este script convierte un WAV cualquiera (otra frecuencia,
estereo, 24/32 bit...) SIN recortarlo (los gritos se cortan a 30 s; la musica no).

Uso:
    python tools/prep_music.py CANCION.wav            -> sd_music/mons/bgm.wav
    python tools/prep_music.py CANCION.wav battle     -> sd_music/mons/battle_wild.wav
    python tools/prep_music.py CANCION.mp3 fame2.wav  -> sd_music/mons/fame2.wav (ko11)
ko11: nombres de la SD: bgm.wav / bgm2.wav (normal, al azar), battle_wild.wav,
battle_gym.wav, battle_champ.wav, fame.wav / fame2.wav (salon de la fama, al azar).
MP3/FLAC/OGG: se leen con el modulo miniaudio (pip install miniaudio).
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


def read_any(path):
    """WAV con el lector de siempre; MP3/FLAC/OGG con miniaudio (ko11)"""
    if path.lower().endswith('.wav'):
        return prep_cries.read_wav(path)
    import miniaudio
    d = miniaudio.decode_file(path, output_format=miniaudio.SampleFormat.FLOAT32, nchannels=1)
    return list(d.samples), d.sample_rate


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 1
    src = argv[1]
    name = 'bgm.wav'
    if len(argv) > 2:
        name = argv[2] if argv[2].endswith('.wav') else ('battle_wild.wav' if argv[2].startswith('b') else 'bgm.wav')
    out_dir = os.path.join('sd_music', 'mons')
    os.makedirs(out_dir, exist_ok=True)
    mono, rate = read_any(src)
    pcm = resample_hq(mono, rate)
    # ko11: todas al mismo volumen aparente (RMS ~0,12) con techo de -1 dB para
    # que al mezclar con gritos y efectos no sature
    peak = max((abs(v) for v in pcm), default=0)
    rms = (sum(v * v for v in pcm) / max(1, len(pcm))) ** 0.5
    if peak > 0:
        gain = min(0.89 / peak, 0.12 / rms if rms > 0 else 1.0)
        pcm = [v * gain for v in pcm]
    # ko11: fundido corto al final y al principio: la vuelta (bucle) no da un golpe
    fin, fout = int(0.05 * prep_cries.RATE), int(1.5 * prep_cries.RATE)
    n = len(pcm)
    for i in range(min(fin, n)):
        pcm[i] *= i / fin
    for i in range(min(fout, n)):
        pcm[n - 1 - i] *= i / fout
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
