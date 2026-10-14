#!/usr/bin/env bash
# TamaPoke para Android (APK personal, sin Play Store). El juego es el MISMO codigo del firmware
# (sketch preprocesado con arduino-cli, como test/render) + jni/host.cpp (ventana, tactil, NVS en fichero,
# importar mons.pak). Sin Gradle: NDK + javac + d8 + aapt2 + apksigner.
#   ANDROID_SDK=<sdk> android/build_apk.sh            -> android/build/TamaPoke.apk
# Necesita: SDK con platforms;android-34, build-tools;34.0.0, ndk;26.3.11579264, JDK 17+, arduino-cli
set -euo pipefail
cd "$(dirname "$0")"
HERE=$PWD
SKETCH=$HERE/../TamaPoke
SDK=${ANDROID_SDK:?ruta del Android SDK}
NDK=$SDK/ndk/26.3.11579264
BT=$SDK/build-tools/34.0.0
JAR=$SDK/platforms/android-34/android.jar
LIBS="${ARDUINO_LIBS:-$HOME/Arduino/libraries}"
GFX="$LIBS/GFX_Library_for_Arduino/src"
CLI="${ARDUINO_CLI:-arduino-cli}"
FQBN="esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB"
TC=$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin
CXX=$TC/aarch64-linux-android26-clang++
CC=$TC/aarch64-linux-android26-clang
B=$HERE/build
mkdir -p "$B/obj" "$B/apk/lib/arm64-v8a" "$B/classes" "$B/dex"

echo "== sketch"
"$CLI" compile --preprocess --fqbn "$FQBN" "$SKETCH" > "$B/sketch_raw.cpp"
sed 's/Arduino_DataBus \*bus = new Arduino_ESP32QSPI(/Arduino_ESP32QSPI *bus = new Arduino_ESP32QSPI(/' "$B/sketch_raw.cpp" > "$B/sketch.cpp"
R=$SKETCH/test/render
[ -f "$R/build/fonts.c" ] || python3 "$R/extract_fonts.py" "$LIBS/U8g2/src/clib/u8g2_fonts.c" "$R/build/fonts.c"
sed -n '/Preferences en memoria/,$p' "$SKETCH/test/shim/shim.cpp" > "$B/prefs_impl.inc"

echo "== nativo (arm64)"
INC="-I $HERE/jni/shim -I $R/shim -I $SKETCH/test/shim -I $SKETCH -I $GFX -I $B -I $NDK/sources/android/native_app_glue"
CXXF="-std=gnu++17 -O2 -fPIC -w -ffunction-sections -fdata-sections -DHOST_REAL_AUDIO $INC"
rm -f "$B"/obj/*.o
for f in "$HERE/jni/host.cpp" "$HERE/jni/audio_host.cpp" "$HERE/jni/host_stubs.cpp" "$SKETCH/pet.cpp" "$SKETCH/box.cpp" "$SKETCH/battle.cpp" "$SKETCH/i18n.cpp" \
         "$SKETCH/i18n_ext.cpp" "$SKETCH/story_ko.cpp" "$SKETCH/sdmon.cpp" "$SKETCH/fxanim.cpp" "$SKETCH/sdcheck.cpp" \
         "$SKETCH/pak_core.cpp" "$GFX/Arduino_GFX.cpp" "$GFX/Arduino_G.cpp" "$GFX/canvas/Arduino_Canvas.cpp"; do
  "$CXX" $CXXF -c "$f" -o "$B/obj/$(basename "$f").o" &
done
"$CC" -O2 -fPIC -w -c "$R/build/fonts.c" -o "$B/obj/fonts.o" &
"$CC" -O2 -fPIC -w -c "$NDK/sources/android/native_app_glue/android_native_app_glue.c" -o "$B/obj/glue.o" &
for j in $(jobs -p); do wait "$j"; done
"$CXX" -shared -o "$B/apk/lib/arm64-v8a/libtamapoke.so" "$B"/obj/*.o -static-libstdc++ -landroid -llog -laaudio -lm \
  -Wl,--gc-sections -Wl,-u,ANativeActivity_onCreate -Wl,--build-id=sha1 -s

echo "== java"
javac -source 8 -target 8 -Xlint:-options -classpath "$JAR" -d "$B/classes" "$HERE/java/com/tamapoke/ko/MainActivity.java"
"$BT/d8" --release --min-api 26 --lib "$JAR" --output "$B/dex" $(find "$B/classes" -name "*.class")

echo "== apk"
rm -rf "$B/res_c" && mkdir -p "$B/res_c"
"$BT/aapt2" compile --dir "$HERE/res" -o "$B/res_c"
"$BT/aapt2" link -o "$B/base.apk" --manifest "$HERE/AndroidManifest.xml" -I "$JAR" "$B"/res_c/*.flat
cp "$B/dex/classes.dex" "$B/apk/"
(cd "$B/apk" && zip -q -r "$B/base.apk" classes.dex lib)
"$BT/zipalign" -f -p 4 "$B/base.apk" "$B/aligned.apk"
KS=${TP_KEYSTORE:-$HERE/debug.keystore}
[ -f "$KS" ] || keytool -genkeypair -keystore "$KS" -storepass android -keypass android -alias tamapoke \
  -keyalg RSA -keysize 2048 -validity 10000 -dname "CN=TamaPoke debug" >/dev/null 2>&1
"$BT/apksigner" sign --ks "$KS" --ks-pass pass:android --key-pass pass:android --out "$B/TamaPoke.apk" "$B/aligned.apk"
"$BT/apksigner" verify "$B/TamaPoke.apk" && ls -la "$B/TamaPoke.apk"
