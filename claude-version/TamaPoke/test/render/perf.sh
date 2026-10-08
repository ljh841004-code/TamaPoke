#!/usr/bin/env bash
# ko12.1: mide cuanto cuesta cada parte del combate en el PC (perf_main.cpp). Mismo uso que run.sh
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
g++ -std=gnu++17 -O2 -w -I shim -I ../shim -I ../.. -I "$GFX" -o build/perf \
  perf_main.cpp stubs.cpp ../../pet.cpp ../../box.cpp ../../battle.cpp ../../i18n.cpp \
  ../../i18n_ext.cpp ../../story_ko.cpp ../../sdmon.cpp ../../fxanim.cpp ../../sdcheck.cpp "$GFX/Arduino_GFX.cpp" "$GFX/Arduino_G.cpp" \
  "$GFX/canvas/Arduino_Canvas.cpp" build/fonts.o
./build/perf "$SD"
