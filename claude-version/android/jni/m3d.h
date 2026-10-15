#pragma once
// Pokemon en 3D de verdad (PC / movil): modelo con esqueleto (.m3d, ver pc/tools/glb_to_m3d.py), animado por
// codigo moviendo los huesos (respirar, andar, comer, dormir, atacar...) y pintado por software (sin GPU):
// piel por huesos, texturas, luz suave "de juego", contorno oscuro, antialias 2x.
#include <stdint.h>
#include <string>
#include <vector>

struct M3dJoint {
  int16_t parent;
  char name[23];
  float t[3], r[4], s[3];
  float invBind[16];
};
struct M3dPrim {
  struct V { float p[3], n[3], uv[2]; uint8_t j[4]; float w[4]; };
  std::vector<V> v;
  std::vector<uint32_t> idx;
  int tw = 0, th = 0;
  std::vector<uint8_t> tex;  // RGBA
};
struct M3dModel {
  float root[16];
  std::vector<M3dJoint> joints;
  std::vector<M3dPrim> prims;
  float minY = 0, maxY = 1, height = 1, cx = 0, cz = 0;  // caja en postura de reposo (modelo)
  bool load(const std::vector<uint8_t> &d);
  int find(const char *name) const;  // indice de hueso por nombre (o -1)
};

// postura: rotacion extra por hueso (eje-angulo en el espacio local del hueso)
struct M3dPose {
  std::vector<float> q;  // 4 por hueso (x y z w), identidad = 0 0 0 1
  float rootY = 0, rootLift = 0;
  void reset(size_t n) { q.assign(n * 4, 0.0f); for (size_t i = 0; i < n; i++) q[i * 4 + 3] = 1.0f; rootY = rootLift = 0; }
  void rot(int j, float ax, float ay, float az, float ang);  // suma un giro
};

struct M3dView {
  float yaw = 0, pitch = 0.18f;  // radianes: 0 = de frente
  float scale = 1;               // pixeles por unidad del modelo
  float ox = 0, oy = 0;          // pixel donde van los pies (centro)
  int mulR = 256, mulG = 256, mulB = 256, add = 0;  // tinte
  bool silhouette = false;
};

// pinta sobre un lienzo RGB565 (w x h) mezclando por cobertura
void m3dDraw(const M3dModel &m, const M3dPose &pose, const M3dView &v, uint16_t *fb, int w, int h);
// accion PMD (sdmon.h) + tiempo -> postura y ajustes de vista (giro al andar, salto...)
void m3dAnimate(const M3dModel &m, uint8_t act, uint32_t t, M3dPose &pose, M3dView &view);
