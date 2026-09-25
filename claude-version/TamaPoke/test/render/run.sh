#!/usr/bin/env bash
# Renderiza pantallas del firmware en el PC (fork KO, ko4). Necesita:
#   - arduino-cli con el core esp32 y las librerias del proyecto (preprocesa el sketch)
#   - una carpeta con la SD (mons/pNNN.bin + thumbs.bin), p. ej. desempaquetando web/sprites.pak
# Uso:  test/render/run.sh <carpeta_sd>     -> test/render/build/shots/*.png
set -euo pipefail
cd "$(dirname "$0")"
SD="${1:?carpeta de la SD (mons/...)}"
LIBS="${ARDUINO_LIBS:-$HOME/Arduino/libraries}"
GFX="$LIBS/GFX_Library_for_Arduino/src"
CLI="${ARDUINO_CLI:-arduino-cli}"
FQBN="esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB"
mkdir -p build/shots
"$CLI" compile --preprocess --fqbn "$FQBN" ../.. > build/sketch_raw.cpp
# el bus QSPI no existe en el PC: el lienzo solo necesita un "panel" de mentira
sed 's/Arduino_DataBus \*bus = new Arduino_ESP32QSPI(/Arduino_ESP32QSPI *bus = new Arduino_ESP32QSPI(/' \
  build/sketch_raw.cpp > build/sketch.cpp
[ -f build/fonts.c ] || python3 extract_fonts.py "$LIBS/U8g2/src/clib/u8g2_fonts.c" build/fonts.c
sed -n '/Preferences en memoria/,$p' ../shim/shim.cpp > build/prefs_impl.inc
gcc -c -O1 -w build/fonts.c -o build/fonts.o
g++ -std=gnu++17 -O1 -w -I shim -I ../shim -I ../.. -I "$GFX" -o build/render \
  render_main.cpp stubs.cpp ../../pet.cpp ../../box.cpp ../../battle.cpp ../../i18n.cpp \
  ../../i18n_ext.cpp ../../sdmon.cpp "$GFX/Arduino_GFX.cpp" "$GFX/Arduino_G.cpp" \
  "$GFX/canvas/Arduino_Canvas.cpp" build/fonts.o
rm -f build/shots/*.raw
./build/render "$SD"
python3 - <<'PY'
import glob, os
from PIL import Image
for raw in sorted(glob.glob('build/shots/*.raw')):
    d = open(raw, 'rb').read()
    im = Image.new('RGB', (466, 466))
    px = []
    for i in range(0, len(d), 2):
        v = d[i] | d[i + 1] << 8
        px.append((((v >> 11) & 31) * 255 // 31, ((v >> 5) & 63) * 255 // 63, (v & 31) * 255 // 31))
    im.putdata(px)
    im.save(raw[:-4] + '.png')
    os.remove(raw)
print('PNG en', os.path.abspath('build/shots'))
PY
