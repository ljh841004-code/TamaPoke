# Reproduce TamaPoke v1.19

Base fork: https://github.com/ljh841004-code/TamaPoke
Base revision: 67aa88932ed81cfe12e391597b38c694fd52bcd6
Personal v1.18 source: source_changes.patch from the user's TamaPoke_Update_1.18 package.
The v1.19 delta preserves that patch and changes audio streaming/routing and packaging.

Pinned build dependencies:
- Arduino ESP32 core 3.3.10
- GFX Library for Arduino 1.6.8
- SensorLib 0.5.0
- XPowersLib 0.3.3
- U8g2 2.36.19

```sh
arduino-cli config init --additional-urls https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
arduino-cli core update-index
arduino-cli core install esp32:esp32@3.3.10
arduino-cli lib install "GFX Library for Arduino@1.6.8" "SensorLib@0.5.0" "XPowersLib@0.3.3" "U8g2@2.36.19"
arduino-cli compile --warnings=all --export-binaries --fqbn "esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB" .
```

Run inside the TamaPoke sketch directory. On Windows, use a short checkout and
Arduino data directory (or a temporary drive mapping) to avoid toolchain path limits.
The app-only image is built at `build/esp32.esp32.esp32s3/TamaPoke.ino.bin`.
CI also merges bootloader, partitions, boot_app0 and app for a fresh-install image.
Neither CI nor the source archive needs the user's music to build.

Tests on Linux: `./test/run_tests.sh --asan`, then `ruff check tools/` and
`python3 tools/test_i18n_formats.py`. Set PYTHONUTF8=1 on Windows.
The POSIX executable-bit test is skipped on Windows because it has no matching
file-permission semantics. Hangul display checks allow only ASCII and the bundled
modern Hangul syllable range, retaining rejection of unsupported literal glyphs.

Runtime:
- A single audio task owns the streaming file and I2S writes.
- RIFF chunks are bounded and validated; metadata/padding are skipped.
- 8 KiB read-ahead + 256-sample mixing blocks bound BGM memory independent of song length.
- SD access is serialized against sprites, cries and USB transfer. Cached audio can
  continue during contention; if it runs out, music waits without discarding its cursor.
- USB PUT closes music before replacement and resumes after releasing the card lock.
- BGM control bypasses the SFX queue, so a full effects queue cannot lose a transition.
- Wild position is bookmarked per battle run, including result and trainer-wave transitions.
- Missing/invalid music silences only BGM; existing cries and synthesized effects remain.
  Replace the WAV and reboot, or use the USB transfer path, to retry loading it.

Physical validation still needed: listen through a full loop, fight/escape/next-wave,
check all three volume sliders after reboot, and exercise USB transfer on the device.
Long SD stalls or flash-save stalls may cause an audible gap; host tests cannot measure
this board/card behavior. No hardware was flashed during preparation.
