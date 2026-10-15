// pc/tools/m3d_test.cpp: pinta un .m3d en PNG (raw 565) para comprobar el modelo y las animaciones
//   m3d_test model.m3d out.raw yaw pitch [bone axis ang]...
#include "../../android/jni/m3d.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
void m3dAnimate(const M3dModel &, uint8_t, uint32_t, M3dPose &, M3dView &) __attribute__((weak));
int main(int argc, char **argv) {
  FILE *f = fopen(argv[1], "rb");
  std::vector<uint8_t> d;
  uint8_t b[65536]; size_t n;
  while ((n = fread(b, 1, sizeof b, f)) > 0) d.insert(d.end(), b, b + n);
  fclose(f);
  M3dModel m;
  if (!m.load(d)) { printf("load fail\n"); return 1; }
  printf("joints %zu height %.3f minY %.3f cx %.3f cz %.3f\n", m.joints.size(), m.height, m.minY, m.cx, m.cz);
  const int W = 600, H = 600;
  std::vector<uint16_t> fb(W * H, 0x8E7F);
  M3dPose p; p.reset(m.joints.size());
  M3dView v; v.yaw = atof(argv[3]); v.pitch = atof(argv[4]); v.scale = 480 / m.height; v.ox = 300; v.oy = 560;
  for (int i = 5; i + 2 < argc; i += 3) {
    int j = m.find(argv[i]);
    char ax = argv[i + 1][0];
    if (!strcmp(argv[i], "ACT")) { if (m3dAnimate) m3dAnimate(m, (uint8_t)atoi(argv[i + 1]), (uint32_t)atoi(argv[i + 2]), p, v); continue; }
    p.rot(j, ax == 'x', ax == 'y', ax == 'z', atof(argv[i + 2]));
  }
  m3dDraw(m, p, v, fb.data(), W, H);
  FILE *o = fopen(argv[2], "wb"); fwrite(fb.data(), 2, W * H, o); fclose(o);
}
