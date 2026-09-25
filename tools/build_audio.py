#!/usr/bin/env python3
"""Generate an original BGM loop and convert PokeAPI cries to bounded mono PCM WAV.
Requires ffmpeg; downloaded cries retain their original owners' rights.
"""
import concurrent.futures
import math
from pathlib import Path
import struct
import subprocess
import tempfile
import urllib.request
import wave

ROOT = Path(__file__).resolve().parent / 'sdcard' / 'mons'
ROOT.mkdir(parents=True, exist_ok=True)
RATE = 16000

def wav(path, pcm):
    with wave.open(str(path), 'wb') as out:
        out.setnchannels(1)
        out.setsampwidth(2)
        out.setframerate(RATE)
        out.writeframes(pcm)

def cry(dex):
    target = ROOT / f'cry{dex:03}.wav'
    if target.exists():
        return
    url = f'https://raw.githubusercontent.com/PokeAPI/cries/main/cries/pokemon/legacy/{dex}.ogg'
    with tempfile.TemporaryDirectory() as tmp:
        source = Path(tmp) / 'cry.ogg'
        with urllib.request.urlopen(url, timeout=60) as r:
            source.write_bytes(r.read())
        pcm = subprocess.check_output(['ffmpeg', '-v', 'error', '-i', str(source),
                                       '-t', '10', '-ac', '1', '-ar', str(RATE),
                                       '-f', 's16le', '-'])
        wav(target, pcm)

# Original 16-beat tune, 8 seconds; no soundtrack samples.
notes = [60, 64, 67, 71, 69, 67, 64, 62, 65, 69, 72, 69, 67, 64, 62, 60]
raw = bytearray()
for note in notes:
    hz = 440 * 2 ** ((note - 69) / 12)
    for i in range(RATE // 2):
        t = i / RATE
        env = min(1, t / .015, (.5 - t) / .08)
        value = int(4500 * env * (math.sin(2 * math.pi * hz * t) + .2 * math.sin(4 * math.pi * hz * t)))
        raw.extend(struct.pack('<h', value))
wav(ROOT / 'bgm.wav', raw)
with concurrent.futures.ThreadPoolExecutor(max_workers=6) as pool:
    list(pool.map(cry, range(1, 152)))
print('Generated BGM and 151 cries.')
