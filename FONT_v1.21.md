# TamaPoke v1.21 font change

Korean body labels use native 20×20 bitmaps in `hangul_font20.h`, replacing the
16×16 to 20×20 rectangle scaling path. The advance remains 20 pixels. The
existing 16×16 bitmaps are retained for other sizes and languages.

The new glyphs cover U+AC00 through U+D7A3, generated from Noto Sans CJK KR
Regular OTF at 20 pixels. Source font SHA-256:
`6bcb2a0703aa137e874fc2dffa85f6c21ba9a67fa329e81b8c801663af7e992a`.
All 11,172 glyphs were checked against the 20×20 cell for clipping. See
`fonts-OFL.txt` for the font license.

The only firmware source change outside the generated font header is in
`TamaPoke.ino`: add the header, set `FW_VERSION` to `1.21`, and draw the
native glyph for the existing 20-pixel Korean path. The board build uses
Arduino ESP32 core 3.3.10 and the pinned libraries in `BUILD_v1.19.md`.

The `installer/` folder is retained from the v1.20 source archive for history.
Use the separate `TamaPoke_v1.21_Installer.zip` to install this release.
