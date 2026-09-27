#!/bin/sh
# 다마포케 사용 설명서 PDF 만들기
#   ./build.sh                  # fonts/ 에 있는 글꼴로
#   ./build.sh <fontsource/files>  # 글자를 바꿨으면 글꼴도 다시 (tools/make_fonts.py)
# 필요한 것: Chromium(또는 Chrome), python3 + pypdf (+ fonttools: 글꼴 다시 만들 때)
set -e
cd "$(dirname "$0")"
if [ -n "$1" ]; then python3 tools/make_fonts.py "$1"; fi
CHROME=${CHROME:-$(ls -d /opt/pw-browsers/chromium-*/chrome-linux/chrome 2>/dev/null | head -1)}
[ -n "$CHROME" ] || CHROME=$(command -v chromium || command -v google-chrome)
"$CHROME" --headless --no-sandbox --disable-gpu --no-pdf-header-footer \
  --print-to-pdf="$PWD/TamaPoke_사용설명서_A5.pdf" "file://$PWD/manual.html"
python3 tools/impose.py TamaPoke_사용설명서_A5.pdf TamaPoke_사용설명서_A4접지용.pdf
