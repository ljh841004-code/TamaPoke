// Pokemon en 3D pintados por software (ver m3d.h)
#include "m3d.h"
#include <math.h>
#include <string.h>
#include <algorithm>

// ---------------------------------------------------------------- matematicas (4x4 columna mayor)
struct Mat { float m[16]; };
static Mat matMul(const Mat &a, const Mat &b) {
  Mat r;
  for (int c = 0; c < 4; c++)
    for (int rr = 0; rr < 4; rr++) {
      float s = 0;
      for (int k = 0; k < 4; k++) s += a.m[k * 4 + rr] * b.m[c * 4 + k];
      r.m[c * 4 + rr] = s;
    }
  return r;
}
static void quatMul(const float *a, const float *b, float *o) {  // o = a * b
  float x = a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1];
  float y = a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0];
  float z = a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3];
  float w = a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2];
  o[0] = x; o[1] = y; o[2] = z; o[3] = w;
}
static Mat trs(const float *t, const float *q, const float *s) {
  float x = q[0], y = q[1], z = q[2], w = q[3];
  float n = sqrtf(x * x + y * y + z * z + w * w);
  if (n > 0) { x /= n; y /= n; z /= n; w /= n; }
  Mat M;
  M.m[0] = (1 - 2 * (y * y + z * z)) * s[0]; M.m[1] = (2 * (x * y + z * w)) * s[0]; M.m[2] = (2 * (x * z - y * w)) * s[0]; M.m[3] = 0;
  M.m[4] = (2 * (x * y - z * w)) * s[1]; M.m[5] = (1 - 2 * (x * x + z * z)) * s[1]; M.m[6] = (2 * (y * z + x * w)) * s[1]; M.m[7] = 0;
  M.m[8] = (2 * (x * z + y * w)) * s[2]; M.m[9] = (2 * (y * z - x * w)) * s[2]; M.m[10] = (1 - 2 * (x * x + y * y)) * s[2]; M.m[11] = 0;
  M.m[12] = t[0]; M.m[13] = t[1]; M.m[14] = t[2]; M.m[15] = 1;
  return M;
}

// ---------------------------------------------------------------- cargar
template <class T> static bool rd(const std::vector<uint8_t> &d, size_t &o, T &v) {
  if (o + sizeof(T) > d.size()) return false;
  memcpy(&v, d.data() + o, sizeof(T));
  o += sizeof(T);
  return true;
}
bool M3dModel::load(const std::vector<uint8_t> &d) {
  size_t o = 0;
  if (d.size() < 12 || memcmp(d.data(), "TPM3", 4)) return false;
  o = 4;
  uint32_t ver; uint16_t nj, np;
  if (!rd(d, o, ver) || !rd(d, o, nj) || !rd(d, o, np) || ver != 1) return false;
  for (int i = 0; i < 16; i++) if (!rd(d, o, root[i])) return false;
  joints.resize(nj);
  for (auto &j : joints) {
    if (!rd(d, o, j.parent) || o + 22 > d.size()) return false;
    memcpy(j.name, d.data() + o, 22); j.name[22] = 0; o += 22;
    for (float &f : j.t) rd(d, o, f);
    for (float &f : j.r) rd(d, o, f);
    for (float &f : j.s) rd(d, o, f);
    for (float &f : j.invBind) if (!rd(d, o, f)) return false;
  }
  prims.resize(np);
  minY = 1e9f; maxY = -1e9f;
  float minX = 1e9f, maxX = -1e9f, minZ = 1e9f, maxZ = -1e9f;
  for (auto &p : prims) {
    uint32_t nv, ni; uint16_t tw, th;
    if (!rd(d, o, nv) || !rd(d, o, ni) || !rd(d, o, tw) || !rd(d, o, th)) return false;
    size_t vs = 8 * 4 + 4 + 4 * 4;
    if (o + (size_t)nv * vs + (size_t)ni * 4 + (size_t)tw * th * 4 > d.size()) return false;
    p.v.resize(nv);
    for (auto &v : p.v) {
      memcpy(v.p, d.data() + o, 12); memcpy(v.n, d.data() + o + 12, 12); memcpy(v.uv, d.data() + o + 24, 8);
      memcpy(v.j, d.data() + o + 32, 4); memcpy(v.w, d.data() + o + 36, 16);
      o += vs;
      for (int k = 0; k < 4; k++) if (v.j[k] >= nj) { v.j[k] = 0; v.w[k] = 0; }
    }
    p.idx.resize(ni);
    memcpy(p.idx.data(), d.data() + o, (size_t)ni * 4); o += (size_t)ni * 4;
    for (auto &i : p.idx) if (i >= nv) i = 0;
    p.tw = tw; p.th = th;
    p.tex.assign(d.data() + o, d.data() + o + (size_t)tw * th * 4); o += (size_t)tw * th * 4;
  }
  // caja en reposo (ya en el espacio de la escena: piel con la postura de reposo)
  M3dPose rest; rest.reset(joints.size());
  std::vector<Mat> G(joints.size());
  std::vector<char> done(joints.size(), 0);
  // (se calcula igual que al pintar: ver skinMats)
  extern void m3dSkinMats(const M3dModel &, const M3dPose &, std::vector<Mat> &);
  m3dSkinMats(*this, rest, G);
  for (auto &p : prims)
    for (auto &v : p.v) {
      float P[3] = { 0, 0, 0 };
      for (int k = 0; k < 4; k++) {
        if (v.w[k] <= 0) continue;
        const float *M = G[v.j[k]].m;
        for (int r = 0; r < 3; r++) P[r] += v.w[k] * (M[r] * v.p[0] + M[4 + r] * v.p[1] + M[8 + r] * v.p[2] + M[12 + r]);
      }
      minY = std::min(minY, P[1]); maxY = std::max(maxY, P[1]);
      minX = std::min(minX, P[0]); maxX = std::max(maxX, P[0]);
      minZ = std::min(minZ, P[2]); maxZ = std::max(maxZ, P[2]);
    }
  height = maxY - minY > 1e-4f ? maxY - minY : 1;
  cx = (minX + maxX) / 2; cz = (minZ + maxZ) / 2;
  return true;
}

// por nombre, sin el sufijo numerico que ponen algunos exportadores ("Head_50" = "Head")
int M3dModel::find(const char *name) const {
  size_t n = strlen(name);
  for (size_t i = 0; i < joints.size(); i++) {
    const char *j = joints[i].name;
    if (strncasecmp(j, name, n)) continue;
    if (!j[n]) return (int)i;
    if (j[n] == '_') {
      const char *k = j + n + 1;
      while (*k >= '0' && *k <= '9') k++;
      if (!*k && k > j + n + 1) return (int)i;
    }
  }
  return -1;
}

void M3dPose::rot(int j, float ax, float ay, float az, float ang) {
  if (j < 0 || (size_t)j * 4 + 3 >= q.size()) return;
  float n = sqrtf(ax * ax + ay * ay + az * az);
  if (n <= 0) return;
  float s = sinf(ang / 2) / n, add[4] = { ax * s, ay * s, az * s, cosf(ang / 2) }, o[4];
  quatMul(&q[j * 4], add, o);
  memcpy(&q[j * 4], o, 16);
}

// matrices de piel = global(hueso) * inversa de enlace
void m3dSkinMats(const M3dModel &m, const M3dPose &pose, std::vector<Mat> &S) {
  size_t n = m.joints.size();
  std::vector<Mat> G(n);
  std::vector<char> done(n, 0);
  Mat R; memcpy(R.m, m.root, 64);
  // los padres pueden venir despues: varias pasadas
  for (size_t pass = 0; pass < n + 1; pass++) {
    bool any = false;
    for (size_t i = 0; i < n; i++) {
      if (done[i]) continue;
      const M3dJoint &j = m.joints[i];
      if (j.parent >= 0 && !done[j.parent]) { any = true; continue; }
      float q[4];
      quatMul(j.r, &pose.q[i * 4], q);
      Mat L = trs(j.t, q, j.s);
      G[i] = j.parent >= 0 ? matMul(G[j.parent], L) : matMul(R, L);
      done[i] = 1;
    }
    if (!any) break;
  }
  S.resize(n);
  for (size_t i = 0; i < n; i++) {
    Mat IB; memcpy(IB.m, m.joints[i].invBind, 64);
    S[i] = matMul(G[i], IB);
  }
}

// ---------------------------------------------------------------- pintar
static inline uint32_t spread565(uint16_t c) { return ((uint32_t)c | ((uint32_t)c << 16)) & 0x07E0F81Fu; }
static inline uint16_t pack565(uint32_t v) { v &= 0x07E0F81Fu; return (uint16_t)(v | (v >> 16)); }

void m3dDraw(const M3dModel &m, const M3dPose &pose, const M3dView &v, uint16_t *fb, int fw, int fh) {
  const int SS = 2;
  std::vector<Mat> S;
  m3dSkinMats(m, pose, S);
  float cy = cosf(v.yaw), sy = sinf(v.yaw), cp = cosf(v.pitch), sp = sinf(v.pitch);
  // luz (en el espacio de la camara): arriba a la izquierda y algo delante
  float L[3] = { -0.45f, 0.75f, 0.55f };
  { float n = sqrtf(L[0] * L[0] + L[1] * L[1] + L[2] * L[2]); L[0] /= n; L[1] /= n; L[2] /= n; }
  struct SV { float x, y, z, shade, u, vv; };
  // caja de pantalla
  int bx0 = 1 << 30, by0 = 1 << 30, bx1 = -(1 << 30), by1 = -(1 << 30);
  std::vector<std::vector<SV>> sv(m.prims.size());
  for (size_t pi = 0; pi < m.prims.size(); pi++) {
    const M3dPrim &p = m.prims[pi];
    sv[pi].resize(p.v.size());
    for (size_t k = 0; k < p.v.size(); k++) {
      const M3dPrim::V &a = p.v[k];
      float P[3] = { 0, 0, 0 }, N[3] = { 0, 0, 0 };
      for (int w = 0; w < 4; w++) {
        if (a.w[w] <= 0) continue;
        const float *M = S[a.j[w]].m;
        for (int r = 0; r < 3; r++) {
          P[r] += a.w[w] * (M[r] * a.p[0] + M[4 + r] * a.p[1] + M[8 + r] * a.p[2] + M[12 + r]);
          N[r] += a.w[w] * (M[r] * a.n[0] + M[4 + r] * a.n[1] + M[8 + r] * a.n[2]);
        }
      }
      // al origen: centro de la caja, pies en y = 0
      float x = P[0] - m.cx, y = P[1] - m.minY + pose.rootLift, z = P[2] - m.cz;
      // giro (yaw alrededor de Y), luego inclinacion de la camara (pitch alrededor de X)
      float x1 = x * cy + z * sy, z1 = -x * sy + z * cy;
      float y2 = y * cp - z1 * sp, z2 = y * sp + z1 * cp;
      float nx = N[0] * cy + N[2] * sy, nz1 = -N[0] * sy + N[2] * cy;
      float ny = N[1] * cp - nz1 * sp, nz = N[1] * sp + nz1 * cp;
      float nl = sqrtf(nx * nx + ny * ny + nz * nz);
      if (nl > 0) { nx /= nl; ny /= nl; nz /= nl; }
      float dif = nx * L[0] + ny * L[1] + nz * L[2];
      // luz "de juego": suave con un escalon, sombra no muy oscura, borde iluminado y un brillo
      float t = (dif + 0.15f) / 0.45f; t = t < 0 ? 0 : t > 1 ? 1 : t; t = t * t * (3 - 2 * t);
      float rim = 1 - fabsf(nz); rim = rim * rim * rim;
      float hx = L[0], hy = L[1], hz = L[2] + 1; float hl = sqrtf(hx * hx + hy * hy + hz * hz);
      float spec = (nx * hx + ny * hy + nz * hz) / hl; spec = spec > 0 ? powf(spec, 24) : 0;
      float shade = 0.66f + 0.40f * t + 0.22f * rim + 0.18f * spec;
      SV &o = sv[pi][k];
      o.x = (v.ox + x1 * v.scale) * SS;
      o.y = (v.oy - y2 * v.scale) * SS;
      o.z = z2;
      o.shade = shade;
      o.u = a.uv[0]; o.vv = a.uv[1];
      bx0 = std::min(bx0, (int)floorf(o.x)); bx1 = std::max(bx1, (int)ceilf(o.x));
      by0 = std::min(by0, (int)floorf(o.y)); by1 = std::max(by1, (int)ceilf(o.y));
    }
  }
  bx0 -= 4 * SS; by0 -= 4 * SS; bx1 += 4 * SS; by1 += 4 * SS;
  bx0 = std::max(bx0, 0); by0 = std::max(by0, 0);
  bx1 = std::min(bx1, fw * SS); by1 = std::min(by1, fh * SS);
  int W = bx1 - bx0, H = by1 - by0;
  if (W <= 0 || H <= 0) return;
  std::vector<float> zb((size_t)W * H, -1e30f);
  std::vector<uint32_t> cb((size_t)W * H, 0);  // 0 = vacio; si no, 0xFF000000 | RGB
  for (size_t pi = 0; pi < m.prims.size(); pi++) {
    const M3dPrim &p = m.prims[pi];
    const std::vector<SV> &V = sv[pi];
    for (size_t t = 0; t + 2 < p.idx.size(); t += 3) {
      const SV &a = V[p.idx[t]], &b = V[p.idx[t + 1]], &c = V[p.idx[t + 2]];
      float area = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
      if (fabsf(area) < 1e-6f) continue;
      int x0 = std::max((int)floorf(std::min({ a.x, b.x, c.x })) - bx0, 0);
      int x1 = std::min((int)ceilf(std::max({ a.x, b.x, c.x })) - bx0, W - 1);
      int y0 = std::max((int)floorf(std::min({ a.y, b.y, c.y })) - by0, 0);
      int y1 = std::min((int)ceilf(std::max({ a.y, b.y, c.y })) - by0, H - 1);
      float ia = 1.0f / area;
      for (int y = y0; y <= y1; y++) {
        float py = y + by0 + 0.5f;
        for (int x = x0; x <= x1; x++) {
          float px = x + bx0 + 0.5f;
          float w0 = ((b.x - px) * (c.y - py) - (b.y - py) * (c.x - px)) * ia;
          float w1 = ((c.x - px) * (a.y - py) - (c.y - py) * (a.x - px)) * ia;
          float w2 = 1 - w0 - w1;
          if (w0 < 0 || w1 < 0 || w2 < 0) continue;
          float z = w0 * a.z + w1 * b.z + w2 * c.z;
          size_t o = (size_t)y * W + x;
          if (z <= zb[o]) continue;
          float u = w0 * a.u + w1 * b.u + w2 * c.u, vv = w0 * a.vv + w1 * b.vv + w2 * c.vv;
          // textura (bilineal, repetida)
          float fu = (u - floorf(u)) * p.tw - 0.5f, fv = (vv - floorf(vv)) * p.th - 0.5f;
          int iu = (int)floorf(fu), iv = (int)floorf(fv);
          float tu = fu - iu, tv = fv - iv;
          float col[4] = { 0, 0, 0, 0 };
          for (int k = 0; k < 4; k++) {
            int qx = (iu + (k & 1)) % p.tw, qy = (iv + (k >> 1)) % p.th;
            if (qx < 0) qx += p.tw;
            if (qy < 0) qy += p.th;
            float wt = ((k & 1) ? tu : 1 - tu) * ((k >> 1) ? tv : 1 - tv);
            const uint8_t *tx = &p.tex[((size_t)qy * p.tw + qx) * 4];
            col[0] += tx[0] * wt; col[1] += tx[1] * wt; col[2] += tx[2] * wt; col[3] += tx[3] * wt;
          }
          if (col[3] < 110) continue;  // recortes (pestanas, aletas)
          float sh = w0 * a.shade + w1 * b.shade + w2 * c.shade;
          int r = (int)(col[0] * sh), g = (int)(col[1] * sh), bl = (int)(col[2] * sh);
          r = r * v.mulR / 256 + v.add; g = g * v.mulG / 256 + v.add; bl = bl * v.mulB / 256 + v.add;
          if (v.silhouette) r = g = bl = 24;
          r = std::min(std::max(r, 0), 255); g = std::min(std::max(g, 0), 255); bl = std::min(std::max(bl, 0), 255);
          zb[o] = z;
          cb[o] = 0xFF000000u | (r << 16) | (g << 8) | bl;
        }
      }
    }
  }
  // contorno: pixeles del borde (o con salto de profundidad) mas oscuros, como en los juegos
  std::vector<uint32_t> out = cb;
  const float dz = 0.05f * m.height;
  for (int y = 1; y < H - 1; y++)
    for (int x = 1; x < W - 1; x++) {
      size_t o = (size_t)y * W + x;
      if (!cb[o]) continue;
      bool edge = false;
      for (int k = 0; k < 4 && !edge; k++) {
        size_t q = k == 0 ? o - 1 : k == 1 ? o + 1 : k == 2 ? o - W : o + W;
        if (!cb[q] || zb[o] - zb[q] > dz) edge = true;
      }
      if (edge) {
        uint32_t c = cb[o];
        int r = ((c >> 16) & 255) * 45 / 100, g = ((c >> 8) & 255) * 45 / 100, bl = (c & 255) * 45 / 100;
        out[o] = 0xFF000000u | (r << 16) | (g << 8) | bl;
      }
    }
  // reducir 2x2 y mezclar sobre el lienzo
  int X0 = bx0 / SS, Y0 = by0 / SS;
  for (int y = 0; y < H / SS; y++) {
    int fy = Y0 + y;
    if (fy < 0 || fy >= fh) continue;
    for (int x = 0; x < W / SS; x++) {
      int fx = X0 + x;
      if (fx < 0 || fx >= fw) continue;
      int n = 0, r = 0, g = 0, bl = 0;
      for (int j = 0; j < SS; j++)
        for (int i = 0; i < SS; i++) {
          uint32_t c = out[(size_t)(y * SS + j) * W + x * SS + i];
          if (!c) continue;
          n++; r += (c >> 16) & 255; g += (c >> 8) & 255; bl += c & 255;
        }
      if (!n) continue;
      r /= n; g /= n; bl /= n;
      uint16_t c565 = (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (bl >> 3));
      uint16_t &d = fb[(size_t)fy * fw + fx];
      if (n == SS * SS) { d = c565; continue; }
      uint32_t a5 = (uint32_t)(n * 32 / (SS * SS));
      d = pack565((spread565(c565) * a5 + spread565(d) * (32 - a5)) >> 5);
    }
  }
}

// ---------------------------------------------------------------- animacion por codigo (por nombre de hueso)
// Los huesos de los modelos de los juegos tienen nombres parecidos (Waist, Head, Jaw, LArm, RArm, LThigh...,
// Tail1). Ejes medidos en el de Squirtle: brazos y piernas giran en su Z local (los derechos al reves),
// la cabeza asiente en Y y gira en X.
struct M3dRig { int waist, head, jaw, larm, rarm, lfore, rfore, lthigh, rthigh, lleg, rleg, tail[3]; };
static M3dRig rigOf(const M3dModel &m) {
  M3dRig r;
  auto f = [&](std::initializer_list<const char *> names) {
    for (const char *n : names) { int i = m.find(n); if (i >= 0) return i; }
    return -1;
  };
  r.waist = f({ "Waist", "Hips", "Spine", "Root" });
  r.head = f({ "Head" });
  r.jaw = f({ "Jaw", "Mouth" });
  r.larm = f({ "LArm", "LShoulder", "L_Arm" });
  r.rarm = f({ "RArm", "RShoulder", "R_Arm" });
  r.lfore = f({ "LForeArm", "LElbow" });
  r.rfore = f({ "RForeArm", "RElbow" });
  r.lthigh = f({ "LThigh", "LLegUpper", "LHip" });
  r.rthigh = f({ "RThigh", "RLegUpper", "RHip" });
  r.lleg = f({ "LLeg", "LKnee" });
  r.rleg = f({ "RLeg", "RKnee" });
  r.tail[0] = f({ "Tail1", "Tail" });
  r.tail[1] = f({ "Tail2" });
  r.tail[2] = f({ "Tail3" });
  return r;
}

enum { A_IDLE = 0, A_WALKL, A_WALKR, A_SLEEP, A_EAT, A_HURT, A_ATTACK, A_POSE, A_HOP, A_NOD, A_BREATH, A_SIT,
       A_ROTATE, A_CHARGE, A_SHOOT, A_LAYING, A_IDLE_UR, A_ATTACK_UR, A_HURT_UR, A_IDLE_DL, A_ATTACK_DL, A_HURT_DL };

void m3dAnimate(const M3dModel &m, uint8_t act, uint32_t t, M3dPose &p, M3dView &v) {
  static const M3dModel *last = nullptr;
  static M3dRig R;
  if (last != &m) { R = rigOf(m); last = &m; }
  p.reset(m.joints.size());
  const float TAU = 6.2831853f, ph = (float)t;
  auto S = [&](float period, float off = 0) { return sinf(ph * TAU / period + off); };
  // postura base: brazos abajo y un poco hacia delante, codos algo doblados
  float armDown = 0.75f, armFwd = 0.15f, legL = 0, legR = 0, waistB = 0, headN = 0, headT = 0, jaw = 0, tailW = 0;
  float breath = S(1800);
  waistB = 0.03f * breath;
  headN = 0.04f * S(1800, 0.6f);
  tailW = 0.18f * S(1400);
  armDown += 0.05f * breath;
  switch (act) {
    case A_WALKL:
    case A_WALKR: {
      float w = S(560);
      legL = 0.55f * w; legR = 0.55f * w;          // ejes en espejo: mismo signo = piernas alternas
      armFwd = 0.15f + 0.35f * w;
      p.rootLift = -0.012f * m.height * fabsf(w) + 0.02f * m.height * fabsf(S(560, 1.57f)) * 0.5f;
      waistB = 0.06f;
      tailW = 0.35f * S(560);
      headN = 0.05f * S(280);
      v.yaw += (act == A_WALKR ? 1.0f : -1.0f) * 1.15f;  // de lado hacia donde anda
      break;
    }
    case A_SLEEP: {
      float b = S(3600);
      waistB = 0.22f + 0.03f * b;   // encogido hacia delante
      headN = -0.35f + 0.04f * b;   // cabeza caida
      armDown = 0.95f; armFwd = 0.45f;
      legL = 0.9f; legR = 0.9f;      // sentado
      p.rootLift = -0.08f * m.height;
      tailW = 0.05f * b;
      v.mulR = v.mulG = 175; v.mulB = 205;
      break;
    }
    case A_EAT: {
      float c = fabsf(S(520));
      headN = -0.25f * c; jaw = 0.35f * c;
      armFwd = 0.7f; armDown = 0.5f;  // las manos al frente con la comida
      waistB = 0.08f * c;
      break;
    }
    case A_HURT: {
      float k = (float)(t % 700) / 700.0f;
      waistB = -0.25f * (1 - k);  // echado hacia atras
      headT = 0.25f * S(120) * (1 - k);
      armDown = 0.2f; armFwd = -0.2f;
      int pulse = (int)(45 * (1 - k));
      v.mulG = v.mulB = 256 - pulse; v.add = pulse / 3;
      jaw = 0.25f;
      break;
    }
    case A_CHARGE: {
      legL = -0.35f; legR = -0.35f;  // agachado
      p.rootLift = -0.06f * m.height;
      armDown = 0.4f; armFwd = 0.6f;
      waistB = 0.15f + 0.03f * S(80);
      headN = -0.1f;
      v.add = (int)(45 * (0.5f + 0.5f * S(300)));
      break;
    }
    case A_ATTACK:
    case A_SHOOT:
    case A_ATTACK_DL:
    case A_ATTACK_UR: {
      float k = fmodf(ph, 700.0f) / 700.0f;
      float lunge = k < 0.3f ? -k / 0.3f * 0.4f : k < 0.5f ? -0.4f + (k - 0.3f) / 0.2f * 1.4f : 1.0f - (k - 0.5f) / 0.5f;
      waistB = 0.3f * lunge;
      armFwd = 0.2f + 1.0f * (lunge > 0 ? lunge : 0); armDown = 0.4f;
      jaw = 0.4f * (lunge > 0 ? lunge : 0);
      headN = 0.1f * lunge;
      if (k > 0.3f && k < 0.6f) v.add = 45;
      break;
    }
    case A_POSE:
    case A_HOP: {
      float k = fmodf(ph, 760.0f) / 760.0f, j = sinf(k * 3.14159f);
      p.rootLift = 0.28f * m.height * j;
      armDown = -0.5f * j + 0.6f * (1 - j);  // brazos arriba en el aire
      legL = legR = -0.25f * j;
      headN = 0.15f * j; jaw = 0.25f * j;
      tailW = 0.4f * S(380);
      break;
    }
    case A_ROTATE: {
      v.yaw += ph * TAU / 900.0f;
      p.rootLift = 0.06f * m.height * fabsf(S(450));
      armDown = 0.2f;
      break;
    }
    case A_LAYING:
    case A_SIT: {
      legL = legR = 1.0f;
      p.rootLift = -0.15f * m.height;
      waistB = act == A_LAYING ? -0.25f : 0.05f;
      armDown = 0.9f;
      headN = act == A_LAYING ? 0.15f : 0;
      break;
    }
    case A_NOD: headN = 0.18f * S(800); break;
    case A_HURT_DL:
    case A_HURT_UR: {
      waistB = -0.2f; headT = 0.2f * S(120); jaw = 0.2f;
      v.mulG = v.mulB = 200;
      break;
    }
    default: break;
  }
  p.rot(R.larm, 0, 0, 1, -armDown); p.rot(R.rarm, 0, 0, 1, armDown);
  p.rot(R.larm, 0, 1, 0, armFwd); p.rot(R.rarm, 0, 1, 0, armFwd);
  p.rot(R.lfore, 0, 0, 1, -0.25f); p.rot(R.rfore, 0, 0, 1, 0.25f);
  p.rot(R.lthigh, 0, 0, 1, legL); p.rot(R.rthigh, 0, 0, 1, legR);
  if (legL > 0.5f) { p.rot(R.lleg, 0, 0, 1, -legL * 0.8f); p.rot(R.rleg, 0, 0, 1, -legR * 0.8f); }
  p.rot(R.waist, 0, 0, 1, -waistB);
  p.rot(R.head, 0, 1, 0, headN);
  p.rot(R.head, 1, 0, 0, headT);
  p.rot(R.jaw, 0, 0, 1, -jaw);
  for (int i = 0; i < 3; i++) p.rot(R.tail[i], 0, 1, 0, tailW * (0.6f + 0.3f * i) * (i ? cosf(ph * TAU / 1400 - i * 0.6f) / (fabsf(cosf(ph * TAU / 1400)) + 1) + 0.5f : 1));
}
