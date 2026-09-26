#!/usr/bin/env python3
"""Descarga los gritos actuales (no los retro) de los 151 y los deja listos
para la SD: mons/cry001.wav ... mons/cry151.wav (WAV 16 kHz mono 16 bit, con
la cabecera de 44 bytes que exige el cargador de gritos del firmware).

Origen: https://github.com/PokeAPI/cries (carpeta cries/pokemon/latest, .ogg).
OJO: son los sonidos de los juegos (c) Nintendo / Game Freak, NO son libres.
Solo para uso personal; no los subas a ningun repositorio publico.

Necesita ffmpeg (en el PATH, o `pip install imageio-ffmpeg`).
Uso:  python3 tools/get_cries.py [carpeta_salida]   (por defecto ./sd_cries)
      (ko9: si ya tenias cry025.wav / cry133.wav de antes, borralos para bajar la voz)
      -> copia la carpeta mons/ resultante a la raiz de la SD
"""
import os
import shutil
import subprocess
import sys
import urllib.request

URL = 'https://raw.githubusercontent.com/PokeAPI/cries/main/cries/pokemon/latest/{}.ogg'

# ko9: Pikachu y Eevee dicen su nombre desde Let's Go ("Pi-ka-chu!", "Vui!").
# El grito "latest" normal (#25, #133) es el sintetizado de los juegos; el de
# sus formas companero (Let's Go, PokeAPI 10158 / 10159) es la voz.
VOICED = {25: 10158, 133: 10159}


def find_ffmpeg():
    exe = shutil.which('ffmpeg')
    if exe:
        return exe
    try:
        import imageio_ffmpeg
        return imageio_ffmpeg.get_ffmpeg_exe()
    except ImportError:
        raise SystemExit('falta ffmpeg: instalalo o ejecuta  pip install imageio-ffmpeg')


def main():
    out = os.path.join(sys.argv[1] if len(sys.argv) > 1 else 'sd_cries', 'mons')
    os.makedirs(out, exist_ok=True)
    ff = find_ffmpeg()
    tmp = os.path.join(out, '_tmp.ogg')
    for n in range(1, 152):
        dst = os.path.join(out, f'cry{n:03d}.wav')
        if os.path.exists(dst):
            continue
        with urllib.request.urlopen(URL.format(VOICED.get(n, n)), timeout=30) as r, open(tmp, 'wb') as f:
            f.write(r.read())
        # -map_metadata -1 + bitexact: sin chunk LIST, cabecera de 44 bytes exacta
        subprocess.run([ff, '-v', 'error', '-y', '-i', tmp, '-ac', '1', '-ar', '16000',
                        '-c:a', 'pcm_s16le', '-map_metadata', '-1', '-fflags', '+bitexact',
                        '-flags:a', '+bitexact', dst], check=True)
        print(f'cry{n:03d}.wav')
    if os.path.exists(tmp):
        os.remove(tmp)
    print('listo:', os.path.abspath(out))


if __name__ == '__main__':
    main()
