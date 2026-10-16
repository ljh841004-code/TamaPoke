#!/bin/sh
# 펌 배포 묶음 만들기 (ko12.10.1부터): 펌 파일 4개 + QR 설명서(manual.pdf) + A6 설명서
#
#   tools/pack_release.sh <버전 ko..> <결과 폴더> <렌더 결과 shots> [fontsource/files]
#     예: tools/pack_release.sh ko12.10.1 ~/rel test/render/build/shots ~/noto/package/files
#
# 먼저: TamaPoke.ino FW_VERSION을 올리고 컴파일 + merge-bin으로 claude-version/ 에
#   update.bin, tamapoke-ko-v1.17-<V>-app-0xe000.bin, tamapoke-ko-v1.17-<V>.bin, TamaPoke_v1.17-<V>_정리.txt
# 가 있어야 해요. 렌더 결과는 SD 그림이 든 test/render/run.sh <SD> 로 새로 그린 것.
# 설명서 표지의 버전은 FW_VERSION에서 저절로 들어가요 (manual/pocket/gen_pocket.py).
#
# 결과: <결과 폴더>/TamaPoke_<V>.zip  (폴더 v.1.17_<V> 안에)
#   update.bin / ...-app-0xe000.bin / ....bin / ..._정리.txt   (펌 4개, 전과 같음)
#   manual.pdf                     QR 설명서 (손바닥 가이드). /fw 화면에서 update.bin과 같이 올리면 SD에 들어가요
#   TamaPoke_사용설명서_A6_인쇄용.pdf  A6 60쪽 인쇄소용 (CMYK, 도련 3mm)
# 설명서 PDF에는 포켓몬 그림이 들어 있어서 저장소에는 올리지 않아요 (개인용).
set -e
V=${1:?버전 (예: ko12.10.1)}
OUT=${2:?결과 폴더}
SHOTS=${3:?렌더 결과 shots 폴더}
FONTS=${4:-}
HERE=$(cd "$(dirname "$0")/.." && pwd)    # claude-version/TamaPoke
ROOT=$(cd "$HERE/.." && pwd)              # claude-version
SHOTS=$(cd "$SHOTS" && pwd)

grep -q "#define FW_VERSION \"1.17-$V\"" "$HERE/TamaPoke.ino" || { echo "FW_VERSION이 1.17-$V 가 아니에요"; exit 1; }
for f in update.bin "tamapoke-ko-v1.17-$V-app-0xe000.bin" "tamapoke-ko-v1.17-$V.bin" "TamaPoke_v1.17-${V}_정리.txt"; do
  [ -f "$ROOT/$f" ] || { echo "없어요: $ROOT/$f"; exit 1; }
done

# 설명서: 손바닥 가이드(A7) -> A6
"$ROOT/manual/pocket/build_pocket.sh" "$SHOTS" $FONTS
python3 "$ROOT/manual/a6/make_a6.py"
A7="$ROOT/manual/pocket/TamaPoke_손바닥가이드_A7.pdf"
A6="$ROOT/manual/a6/인쇄용_A6_전체_60쪽.pdf"
[ -f "$A7" ] && [ -f "$A6" ]

mkdir -p "$OUT"
D="$OUT/v.1.17_$V"
rm -rf "${D:?}"
mkdir -p "$D"
cp "$ROOT/update.bin" "$ROOT/tamapoke-ko-v1.17-$V-app-0xe000.bin" "$ROOT/tamapoke-ko-v1.17-$V.bin" \
   "$ROOT/TamaPoke_v1.17-${V}_정리.txt" "$D/"
cp "$A7" "$D/manual.pdf"
cp "$A6" "$D/TamaPoke_사용설명서_A6_인쇄용.pdf"
(cd "$OUT" && rm -f "TamaPoke_$V.zip" && zip -qr "TamaPoke_$V.zip" "v.1.17_$V")
unzip -l "$OUT/TamaPoke_$V.zip"
