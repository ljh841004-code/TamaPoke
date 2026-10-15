#!/usr/bin/env bash
# TamaPoke para PC (Windows x64): el mismo codigo que la app de Android (modo HD) con ventana Win32.
#   pc/build_win.sh   -> pc/build/TamaPoke.exe   (necesita mingw-w64 y arduino-cli; el sketch lo prepara igual que android)
set -euo pipefail
cd "$(dirname "$0")"
HERE=$PWD
SK=$HERE/../TamaPoke
AN=$HERE/../android
R=$SK/test/render
LIBS="${ARDUINO_LIBS:-$HOME/Arduino/libraries}"
GFX="$LIBS/GFX_Library_for_Arduino/src"
CLI="${ARDUINO_CLI:-arduino-cli}"
FQBN="esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB"
CXX=x86_64-w64-mingw32-g++-posix
CC=x86_64-w64-mingw32-gcc-posix
B=$HERE/build
mkdir -p "$B/obj"
echo "== sketch"
"$CLI" compile --preprocess --fqbn "$FQBN" "$SK" > "$B/sketch_raw.cpp"
sed 's/Arduino_DataBus \*bus = new Arduino_ESP32QSPI(/Arduino_ESP32QSPI *bus = new Arduino_ESP32QSPI(/' "$B/sketch_raw.cpp" > "$B/sketch.cpp"
[ -f "$R/build/fonts.c" ] || python3 "$R/extract_fonts.py" "$LIBS/U8g2/src/clib/u8g2_fonts.c" "$R/build/fonts.c"
sed -n '/Preferences en memoria/,$p' "$SK/test/shim/shim.cpp" > "$B/prefs_impl.inc"
echo "== compilar"
INC="-I $AN/jni -I $AN/jni/shim -I $R/shim -I $SK/test/shim -I $SK -I $GFX -I $B"
CXXF="-std=gnu++17 -O2 -w -DHOST_REAL_AUDIO -DHOST_HD -DTP_FONT_PATH=\"$AN/assets/NotoSansKR-Medium-hd.otf\" $INC"
rm -f "$B"/obj/*.o
for f in "$HERE/host_win.cpp" "$HERE/audio_win.cpp" "$AN/jni/host_hd.cpp" "$AN/jni/m3d.cpp" "$AN/jni/host_stubs.cpp" "$SK/pet.cpp" "$SK/box.cpp" "$SK/battle.cpp" \
         "$SK/i18n.cpp" "$SK/i18n_ext.cpp" "$SK/story_ko.cpp" "$SK/sdmon.cpp" "$SK/fxanim.cpp" "$SK/sdcheck.cpp" "$SK/pak_core.cpp" \
         "$GFX/Arduino_GFX.cpp" "$GFX/Arduino_G.cpp" "$GFX/canvas/Arduino_Canvas.cpp"; do
  "$CXX" $CXXF -c "$f" -o "$B/obj/$(basename "$f").o" &
done
"$CXX" -std=gnu++17 -O2 -w -c "$HERE/win_main.cpp" -o "$B/obj/win_main.o" &
"$CXX" -std=gnu++17 -O2 -w -c "$HERE/audio_wave.cpp" -o "$B/obj/audio_wave.o" &
"$CC" -O2 -w -c "$R/build/fonts.c" -o "$B/obj/fonts.o" &
for j in $(jobs -p); do wait "$j"; done
"$CXX" -mwindows -static -o "$B/TamaPoke.exe" "$B"/obj/*.o -lwinmm -lgdi32 -lcomdlg32 -luser32 -lshell32 -s
ls -la "$B/TamaPoke.exe"
