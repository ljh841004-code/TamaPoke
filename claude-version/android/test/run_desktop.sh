#!/usr/bin/env bash
# Prueba en el PC del codigo de la app (ver desktop_check.cpp). Antes: android/build_apk.sh (prepara build/sketch.cpp)
set -euo pipefail
cd "$(dirname "$0")/.."
SK=$PWD/../TamaPoke
R=$SK/test/render
LIBS="${ARDUINO_LIBS:-$HOME/Arduino/libraries}"
GFX="$LIBS/GFX_Library_for_Arduino/src"
mkdir -p build
gcc -c -O1 -w "$R/build/fonts.c" -o build/desk_fonts.o
g++ -std=gnu++17 -O1 -g -w -fsanitize=address,undefined -DHOST_REAL_AUDIO -DHOST_HD -I jni -I jni/shim -I "$R/shim" -I "$SK/test/shim" -I "$SK" -I "$GFX" -I build -o build/desktop_check \
  test/desktop_check.cpp test/audio_desktop.cpp jni/host_hd.cpp jni/host_stubs.cpp "$SK/pet.cpp" "$SK/box.cpp" "$SK/battle.cpp" "$SK/i18n.cpp" "$SK/i18n_ext.cpp" \
  "$SK/story_ko.cpp" "$SK/sdmon.cpp" "$SK/fxanim.cpp" "$SK/sdcheck.cpp" "$SK/pak_core.cpp" \
  "$GFX/Arduino_GFX.cpp" "$GFX/Arduino_G.cpp" "$GFX/canvas/Arduino_Canvas.cpp" build/desk_fonts.o -lpthread
ASAN_OPTIONS=detect_leaks=0 ./build/desktop_check "$@"
python3 - <<'PY'
import glob, os
from PIL import Image
for raw in sorted(glob.glob('build/desk_*.raw')):
    d = open(raw, 'rb').read()
    W = 1398
    im = Image.new('RGB', (W, len(d) // (2 * W)))
    im.putdata([(((v >> 11) & 31) * 255 // 31, ((v >> 5) & 63) * 255 // 63, (v & 31) * 255 // 31)
                for v in (d[i] | d[i + 1] << 8 for i in range(0, len(d), 2))])
    im.save(raw[:-4] + '.png'); os.remove(raw)
PY
