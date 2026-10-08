#!/bin/sh
# 다마포케 손바닥 가이드 (A7 64쪽) PDF 만들기
#   ./build_pocket.sh <SD 그림이 든 렌더 결과 폴더> [fontsource/files]
#     예: ./build_pocket.sh ../../TamaPoke/test/render/build/shots ~/noto/package/files
# 화면 사진은 PC 렌더러(test/render/run.sh <SD>)로 그린 실제 펌웨어 화면이에요.
# 포켓몬 그림이 들어 있어서 img/ 와 PDF는 저장소에 올리지 않아요 (.gitignore).
# 필요한 것: Chromium, python3 + pypdf (+ fonttools: 글꼴 다시 만들 때)
set -e
cd "$(dirname "$0")"
SHOTS=${1:?렌더 결과 폴더 (test/render/build/shots)}
python3 gen_pocket.py
mkdir -p img
for f in $(grep -o 'img/[^"]*\.png' pocket.html | sort -u); do cp "$SHOTS/$(basename "$f")" "img/"; done
if [ -n "$2" ]; then python3 ../tools/make_fonts.py "$2" --out pocket/fonts pocket/pocket.html pocket/pocket.css; fi
CHROME=${CHROME:-$(ls -d /opt/pw-browsers/chromium-*/chrome-linux/chrome 2>/dev/null | head -1)}
[ -n "$CHROME" ] || CHROME=$(command -v chromium || command -v google-chrome)
"$CHROME" --headless --no-sandbox --disable-gpu --no-pdf-header-footer \
  --print-to-pdf="$PWD/TamaPoke_손바닥가이드_A7.pdf" "file://$PWD/pocket.html"
python3 impose_a7.py TamaPoke_손바닥가이드_A7.pdf TamaPoke_손바닥가이드_A4인쇄용.pdf
