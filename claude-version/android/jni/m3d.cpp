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

std::vector<float> gM3dJointPos;
void M3dPose::rotW(const M3dModel &m, int j, float wx, float wy, float wz, float ang) {
  if (j < 0 || m.restRot.size() < (size_t)(j + 1) * 9) return;
  const float *R = &m.restRot[j * 9];  // columnas = ejes locales en el mundo: local = R^T * mundo
  float lx = R[0] * wx + R[1] * wy + R[2] * wz, ly = R[3] * wx + R[4] * wy + R[5] * wz, lz = R[6] * wx + R[7] * wy + R[8] * wz;
  rot(j, lx, ly, lz, ang);
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
  extern std::vector<float> gM3dJointPos;  // (pruebas) posicion de cada hueso
  gM3dJointPos.resize(n * 3);
  for (size_t i = 0; i < n; i++) for (int k = 0; k < 3; k++) gM3dJointPos[i * 3 + k] = G[i].m[12 + k];
  if (m.restRot.size() != n * 9) {  // la primera vez (reposo): giro global de cada hueso, normalizado
    M3dModel &mm = const_cast<M3dModel &>(m);
    mm.restRot.resize(n * 9);
    for (size_t i = 0; i < n; i++)
      for (int c = 0; c < 3; c++) {
        float l = sqrtf(G[i].m[c * 4] * G[i].m[c * 4] + G[i].m[c * 4 + 1] * G[i].m[c * 4 + 1] + G[i].m[c * 4 + 2] * G[i].m[c * 4 + 2]);
        for (int r = 0; r < 3; r++) mm.restRot[i * 9 + c * 3 + r] = l > 0 ? G[i].m[c * 4 + r] / l : 0;
      }
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
// Los huesos de los modelos de los juegos tienen nombres parecidos (Waist/Hips, Spine, Head, Jaw, LArm, RArm,
// LThigh..., Tail1). Los giros se dan en ejes del CUERPO (rotW: x = izquierda, y = arriba, z = delante), asi
// valen para cualquier modelo aunque cada hueso tenga sus ejes locales distintos.
struct M3dRig { int waist, spine, head, jaw, larm, rarm, lfore, rfore, lthigh, rthigh, lleg, rleg, tail[4]; };
static M3dRig rigOf(const M3dModel &m) {
  M3dRig r;
  auto f = [&](std::initializer_list<const char *> names) {
    for (const char *n : names) { int i = m.find(n); if (i >= 0) return i; }
    return -1;
  };
  r.waist = f({ "Waist", "Hips", "Origin", "Root" });
  r.spine = f({ "Spine2", "Spine1", "Spine", "Chest" });
  r.head = f({ "Head" });
  r.jaw = f({ "Jaw", "Mouth" });
  r.larm = f({ "LArm", "LShoulder" });
  r.rarm = f({ "RArm", "RShoulder" });
  r.lfore = f({ "LForeArm", "LElbow" });
  r.rfore = f({ "RForeArm", "RElbow" });
  r.lthigh = f({ "LThigh", "LLegUpper", "LHip" });
  r.rthigh = f({ "RThigh", "RLegUpper", "RHip" });
  r.lleg = f({ "LLeg", "LKnee" });
  r.rleg = f({ "RLeg", "RKnee" });
  r.tail[0] = f({ "Tail1", "Tail", "TailA01" });
  r.tail[1] = f({ "Tail2", "TailA02" });
  r.tail[2] = f({ "Tail3", "TailA03" });
  r.tail[3] = f({ "Tail4" });
  return r;
}

enum { A_IDLE = 0, A_WALKL, A_WALKR, A_SLEEP, A_EAT, A_HURT, A_ATTACK, A_POSE, A_HOP, A_NOD, A_BREATH, A_SIT,
       A_ROTATE, A_CHARGE, A_SHOOT, A_LAYING, A_IDLE_UR, A_ATTACK_UR, A_HURT_UR, A_IDLE_DL, A_ATTACK_DL, A_HURT_DL };

// ruido suave (suma de senos) para que el reposo no sea mecanico
static float soft(float t, float a, float b, float c) { return 0.55f * sinf(t * a) + 0.3f * sinf(t * b + 1.3f) + 0.15f * sinf(t * c + 2.1f); }

void m3dAnimate(const M3dModel &m, uint8_t act, uint32_t t, M3dPose &p, M3dView &v) {
  static const M3dModel *last = nullptr;
  static M3dRig R;
  if (last != &m) { R = rigOf(m); last = &m; }
  p.reset(m.joints.size());
  const float TAU = 6.2831853f, ts = (float)t / 1000.0f;
  float H = m.height;
  // valores de la postura (radianes, ejes del cuerpo)
  float armDown = 1.0f, armSwingL = 0, armSwingR = 0, armFwd = -0.35f, elbow = 0.3f;
  float thighL = 0, thighR = 0, kneeL = 0, kneeR = 0;
  float lean = 0, roll = 0, headNod = 0, headTurn = 0, headTilt = 0, jaw = 0, tail = 0, tailSpeed = 1.2f;
  // reposo vivo: respira (el pecho sube), mira un poco alrededor, balancea el peso, mueve la cola
  float breath = sinf(ts * TAU / 2.6f);
  lean = 0.025f * breath;
  armDown += 0.04f * breath;
  headNod = 0.04f * sinf(ts * TAU / 2.6f - 0.8f) + 0.05f * soft(ts, 0.5f, 0.31f, 0.17f);
  headTurn = 0.22f * soft(ts, 0.37f, 0.23f, 0.11f);
  headTilt = 0.06f * soft(ts, 0.29f, 0.41f, 0.13f);
  roll = 0.03f * soft(ts, 0.33f, 0.21f, 0.09f);
  tail = 0.2f * sinf(ts * TAU / 1.6f);
  switch (act) {
    case A_WALKL:
    case A_WALKR: {
      float ph = ts * TAU / 0.62f, w = sinf(ph);   // un paso por pierna en 0,62 s
      thighL = 0.42f * w; thighR = -0.42f * w;
      kneeL = 0.5f * fmaxf(0.0f, cosf(ph));        // dobla la rodilla al levantar
      kneeR = 0.5f * fmaxf(0.0f, -cosf(ph));
      armSwingL = -0.35f * w; armSwingR = 0.35f * w;  // brazos al reves que las piernas
      p.rootLift = 0.022f * H * fabsf(cosf(ph));    // sube en cada paso
      lean = 0.08f;
      roll = 0.05f * w;                             // se mece de lado a lado
      headNod = 0.03f * sinf(ph * 2);
      headTurn = 0;
      tail = 0.3f * sinf(ph + 1.0f);
      v.yaw += (act == A_WALKR ? 1.0f : -1.0f) * 1.2f;  // de lado hacia donde anda
      break;
    }
    case A_SLEEP: {
      float b = sinf(ts * TAU / 4.0f);
      lean = 0.3f + 0.04f * b;      // hecho una bola
      headNod = 0.45f + 0.04f * b;  // cabeza caida
      headTurn = 0; headTilt = 0.15f;
      armDown = 1.1f; armFwd = 0.5f; elbow = 0.8f;
      thighL = thighR = -0.9f; kneeL = kneeR = 1.3f;  // sentado
      p.rootLift = -0.07f * H;
      tail = 0.04f * b;
      v.mulR = v.mulG = 180; v.mulB = 210;
      break;
    }
    case A_EAT: {
      float c = 0.5f + 0.5f * sinf(ts * TAU / 0.5f);
      headNod = 0.25f * c; jaw = 0.35f * c;
      armFwd = 0.9f; armDown = 0.55f; elbow = 0.9f;  // la comida en las manos, delante
      lean = 0.1f * c;
      headTurn = 0;
      break;
    }
    case A_HURT: {
      float k = (float)(t % 700) / 700.0f, e = 1 - k;
      lean = -0.3f * e;  // echado hacia atras
      headNod = -0.2f * e; headTurn = 0.3f * sinf(ts * TAU / 0.12f) * e;
      armDown = 0.3f; armFwd = -0.3f;
      jaw = 0.3f * e;
      int pulse = (int)(45 * e);
      v.mulG = v.mulB = 256 - pulse; v.add = pulse / 3;
      break;
    }
    case A_CHARGE: {
      thighL = thighR = -0.45f; kneeL = kneeR = 0.8f;  // agachado
      p.rootLift = -0.05f * H;
      armDown = 0.45f; armFwd = 0.7f; elbow = 1.0f;
      lean = 0.2f + 0.02f * sinf(ts * TAU / 0.08f);
      headNod = 0.1f; headTurn = 0;
      v.add = (int)(45 * (0.5f + 0.5f * sinf(ts * TAU / 0.3f)));
      break;
    }
    case A_ATTACK:
    case A_SHOOT:
    case A_ATTACK_DL:
    case A_ATTACK_UR: {
      float k = fmodf((float)t, 700.0f) / 700.0f;
      float lunge = k < 0.3f ? -k / 0.3f * 0.4f : k < 0.5f ? -0.4f + (k - 0.3f) / 0.2f * 1.4f : 1.0f - (k - 0.5f) / 0.5f;
      float fw = lunge > 0 ? lunge : 0;
      lean = 0.35f * lunge;
      armFwd = 0.2f + 1.1f * fw; armDown = 0.5f - 0.3f * fw; elbow = 0.3f;
      jaw = 0.45f * fw;
      headNod = -0.1f * fw; headTurn = 0;
      thighL = 0.3f * fw; thighR = -0.2f * fw;
      if (k > 0.3f && k < 0.6f) v.add = 45;
      break;
    }
    case A_POSE:
    case A_HOP: {
      float k = fmodf((float)t, 760.0f) / 760.0f, j = sinf(k * 3.14159f);
      float squat = k < 0.12f ? 1 - k / 0.12f : k > 0.88f ? (k - 0.88f) / 0.12f : 0;
      p.rootLift = 0.28f * H * j - 0.04f * H * squat;
      armDown = 0.85f - 1.6f * j;  // brazos arriba en el aire
      thighL = thighR = -0.3f * squat + 0.25f * j; kneeL = kneeR = 0.6f * squat + 0.4f * j;
      headNod = -0.15f * j; jaw = 0.3f * j; headTurn = 0;
      tail = 0.4f * sinf(ts * TAU / 0.38f);
      break;
    }
    case A_ROTATE: {
      v.yaw += ts * TAU / 0.9f;
      p.rootLift = 0.06f * H * fabsf(sinf(ts * TAU / 0.45f));
      armDown = 0.3f;
      break;
    }
    case A_LAYING:
    case A_SIT: {
      thighL = thighR = -1.0f; kneeL = kneeR = 1.4f;
      p.rootLift = -0.08f * H;
      lean = act == A_LAYING ? -0.2f : 0.05f;
      armDown = 1.0f; elbow = 0.6f;
      break;
    }
    case A_NOD: headNod = 0.22f * sinf(ts * TAU / 0.8f); headTurn = 0; break;
    case A_HURT_DL:
    case A_HURT_UR: {
      lean = -0.2f; headTurn = 0.2f * sinf(ts * TAU / 0.12f); jaw = 0.2f;
      v.mulG = v.mulB = 200;
      break;
    }
    default: break;
  }
  // aplicar (cuerpo: x izquierda, y arriba, z delante)
  int torso = R.spine >= 0 ? R.spine : R.waist;
  p.rotW(m, torso, 1, 0, 0, lean);
  p.rotW(m, R.waist, 0, 0, 1, roll);
  p.rotW(m, R.larm, 0, 0, 1, -armDown); p.rotW(m, R.rarm, 0, 0, 1, armDown);
  p.rotW(m, R.larm, 0, 1, 0, -(armFwd + armSwingL)); p.rotW(m, R.rarm, 0, 1, 0, armFwd + armSwingR);
  p.rotW(m, R.lfore, 0, 1, 0, -elbow); p.rotW(m, R.rfore, 0, 1, 0, elbow);
  p.rotW(m, R.lthigh, 1, 0, 0, -thighL); p.rotW(m, R.rthigh, 1, 0, 0, -thighR);
  p.rotW(m, R.lleg, 1, 0, 0, kneeL); p.rotW(m, R.rleg, 1, 0, 0, kneeR);
  p.rotW(m, R.head, 1, 0, 0, headNod);
  p.rotW(m, R.head, 0, 1, 0, headTurn);
  p.rotW(m, R.head, 0, 0, 1, headTilt);
  p.rotW(m, R.jaw, 1, 0, 0, jaw);
  for (int i = 0; i < 4; i++) p.rotW(m, R.tail[i], 0, 1, 0, tail * (1.0f + 0.3f * i) * cosf(i * 0.5f) * (i ? 0.7f : 1.0f));
  (void)tailSpeed;
}

// paso entre posturas: se acerca a la nueva poco a poco (sin saltos al cambiar de accion)
void m3dBlend(M3dPose &cur, const M3dPose &target, float k) {
  if (cur.q.size() != target.q.size()) { cur = target; return; }
  for (size_t i = 0; i + 3 < cur.q.size(); i += 4) {
    float *a = &cur.q[i];
    const float *b = &target.q[i];
    float d = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
    float sg = d < 0 ? -1.0f : 1.0f;
    for (int c = 0; c < 4; c++) a[c] = a[c] + (sg * b[c] - a[c]) * k;
    float n = sqrtf(a[0] * a[0] + a[1] * a[1] + a[2] * a[2] + a[3] * a[3]);
    if (n > 0) for (int c = 0; c < 4; c++) a[c] /= n;
  }
  cur.rootLift += (target.rootLift - cur.rootLift) * k;
}
