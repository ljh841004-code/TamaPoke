// Android (modo HD): lo que el lienzo 3x (host_hd_canvas.h) no puede sacar solo ampliando:
//  - texto coreano y ASCII con Noto Sans KR de verdad (stb_truetype) a 3x, en las MISMAS posiciones y
//    avances que font_ko.h (la maquetacion del firmware no cambia: solo se ve nitido)
//  - los Pokemon con relieve: animaciones de los modelos 3D (mons/ani/fNNN.gif de frente, bNNN.gif de
//    espaldas, sf/sb = shiny), en la pantalla principal (con el paseo, salto, sueno... del PMD encima) y en
//    el combate (el rival de frente, el tuyo de espaldas). Si falta el fichero, el PMD de siempre.
#include "host_hd_canvas.h"
#include "m3d.h"
#include <math.h>
#include <stdio.h>
#include <string>
#include <unordered_map>
#include <vector>

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "third_party/stb_truetype.h"
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#define STBI_ONLY_PNG
#define STBI_ONLY_GIF
#define STBI_NO_STDIO
#include "third_party/stb_image.h"

extern TPCanvas *gfx;
extern const char *gSdRoot;
uint32_t millis();

// ---------------------------------------------------------------- mezcla 565
static inline uint32_t spread(uint16_t c) { return ((uint32_t)c | ((uint32_t)c << 16)) & 0x07E0F81Fu; }
static inline uint16_t pack(uint32_t v) { v &= 0x07E0F81Fu; return (uint16_t)(v | (v >> 16)); }
static inline void blend(uint16_t &d, uint32_t s, int a) {  // s = spread(color), a 0..255
  if (a >= 250) { d = pack(s); return; }
  uint32_t a5 = (uint32_t)(a + 4) >> 3;
  uint32_t dd = spread(d);
  d = pack((s * a5 + dd * (32 - a5)) >> 5);
}

// ---------------------------------------------------------------- texto
static std::vector<uint8_t> gFontData;
static stbtt_fontinfo gFont;
static bool gFontOk = false;
struct HdGlyph { int w = 0, h = 0, xo = 0, yo = 0; std::vector<uint8_t> a; bool have = false; };
static std::unordered_map<uint64_t, HdGlyph> gGlyphs;

void hdSetFont(const uint8_t *d, size_t n) {
  gFontData.assign(d, d + n);
  gFontOk = stbtt_InitFont(&gFont, gFontData.data(), stbtt_GetFontOffsetForIndex(gFontData.data(), 0)) != 0;
  gGlyphs.clear();
}
bool hdFontLoaded() { return gFontOk; }

static const HdGlyph &glyph(uint32_t cp, int pxHd) {
  uint64_t key = (uint64_t)cp << 16 | (uint32_t)pxHd;
  auto it = gGlyphs.find(key);
  if (it != gGlyphs.end()) return it->second;
  if (gGlyphs.size() > 6000) gGlyphs.clear();
  HdGlyph &g = gGlyphs[key];
  int gi = stbtt_FindGlyphIndex(&gFont, (int)cp);
  if (!gi) return g;  // no esta en la fuente: lo pinta el firmware
  g.have = true;
  float sc = stbtt_ScaleForMappingEmToPixels(&gFont, (float)pxHd);
  int x0, y0, x1, y1;
  stbtt_GetGlyphBitmapBox(&gFont, gi, sc, sc, &x0, &y0, &x1, &y1);
  g.w = x1 - x0; g.h = y1 - y0; g.xo = x0; g.yo = y0;
  if (g.w > 0 && g.h > 0) {
    g.a.resize((size_t)g.w * g.h);
    stbtt_MakeGlyphBitmap(&gFont, g.a.data(), g.w, g.h, g.w, sc, sc, gi);
  }
  return g;
}

// un caracter con la linea base en (x, base) logicos; px = tamano de la fuente del firmware
bool hdGlyph(uint32_t cp, int x, int base, int px, int adv, uint16_t col) {
  (void)adv;
  if (!gFontOk || !gfx) return false;
  const HdGlyph &g = glyph(cp, px * HDK);
  if (!g.have) return false;
  if (g.a.empty()) return true;  // espacio
  uint16_t *fb = gfx->hdfb();
  int W = gfx->hdw(), H = gfx->hdh();
  int ox = x * HDK + g.xo, oy = base * HDK + g.yo;
  uint32_t s = spread(col);
  for (int yy = 0; yy < g.h; yy++) {
    int py = oy + yy;
    if (py < 0 || py >= H) continue;
    const uint8_t *row = g.a.data() + (size_t)yy * g.w;
    uint16_t *dst = fb + (size_t)py * W;
    for (int xx = 0; xx < g.w; xx++) {
      int a = row[xx];
      int px_ = ox + xx;
      if (a && px_ >= 0 && px_ < W) blend(dst[px_], s, a);
    }
  }
  return true;
}

// ---------------------------------------------------------------- Pokemon con relieve (animaciones 3D)
struct AniImg {
  int w = 0, h = 0, n = 0;
  std::vector<uint8_t> px;       // n fotogramas RGBA, alfa premultiplicado
  std::vector<uint32_t> at;      // inicio de cada fotograma (ms) y at[n] = duracion total
  int cx = 0, feet = 0, top = 0;  // centro, pies y cabeza (union de todos los fotogramas)
  uint32_t tried = 0;
  bool ok = false;
};
static std::unordered_map<int, AniImg> gAni;

static bool readFile(const std::string &p, std::vector<uint8_t> &out) {
  FILE *f = fopen(p.c_str(), "rb");
  if (!f) return false;
  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  fseek(f, 0, SEEK_SET);
  if (n <= 0) { fclose(f); return false; }
  out.resize((size_t)n);
  bool ok = fread(out.data(), 1, (size_t)n, f) == (size_t)n;
  fclose(f);
  return ok;
}

// back: de espaldas; si no hay shiny, el normal
static AniImg *aniGet(int dex, bool shiny, bool back) {
  int key = dex * 4 + (shiny ? 2 : 0) + (back ? 1 : 0);
  AniImg &im = gAni[key];
  if (im.ok) return &im;
  uint32_t now = millis();
  if (im.tried && now - im.tried < 5000) return shiny ? aniGet(dex, false, back) : nullptr;
  im.tried = now ? now : 1;
  char name[64];
  snprintf(name, sizeof(name), "/mons/ani/%s%c%03d.gif", shiny ? "s" : "", back ? 'b' : 'f', dex);
  std::vector<uint8_t> d;
  if (!gSdRoot || !readFile(std::string(gSdRoot) + name, d)) return shiny ? aniGet(dex, false, back) : nullptr;
  int *delays = nullptr, w = 0, h = 0, n = 0, ch = 0;
  uint8_t *p = stbi_load_gif_from_memory(d.data(), (int)d.size(), &delays, &w, &h, &n, &ch, 4);
  if (!p || n <= 0) { if (p) stbi_image_free(p); if (delays) STBI_FREE(delays); return nullptr; }
  if (gAni.size() > 8) {  // solo hacen falta unos pocos (la mascota, el rival, el tuyo)
    for (auto it = gAni.begin(); it != gAni.end();)
      it = (it->first != key && it->second.ok) ? gAni.erase(it) : std::next(it);
  }
  AniImg &a = gAni[key];
  a.w = w; a.h = h; a.n = n;
  a.px.assign(p, p + (size_t)w * h * 4 * n);
  stbi_image_free(p);
  a.at.resize(n + 1);
  uint32_t acc = 0;
  for (int i = 0; i < n; i++) { a.at[i] = acc; acc += delays && delays[i] > 10 ? delays[i] : 40; }
  a.at[n] = acc;
  if (delays) STBI_FREE(delays);
  int x0 = w, x1 = -1, y0 = h, y1 = -1;
  for (size_t i = 0; i < a.px.size(); i += 4) {
    unsigned al = a.px[i + 3];
    if (al > 24) {
      int k = (int)(i / 4) % (w * h), x = k % w, y = k / w;
      if (x < x0) x0 = x;
      if (x > x1) x1 = x;
      if (y < y0) y0 = y;
      if (y > y1) y1 = y;
    }
    a.px[i] = (uint8_t)(a.px[i] * al / 255);
    a.px[i + 1] = (uint8_t)(a.px[i + 1] * al / 255);
    a.px[i + 2] = (uint8_t)(a.px[i + 2] * al / 255);
  }
  if (x1 < 0) { x0 = 0; x1 = w - 1; y0 = 0; y1 = h - 1; }
  a.cx = (x0 + x1 + 1) / 2; a.feet = y1 + 1; a.top = y0;
  a.ok = true;
  return &a;
}

static const uint8_t *aniFrame(const AniImg &a, uint32_t t) {
  uint32_t m = a.at[a.n] ? t % a.at[a.n] : 0;
  int i = 0;
  while (i + 1 < a.n && a.at[i + 1] <= m) i++;
  return a.px.data() + (size_t)i * a.w * a.h * 4;
}

// Dibuja la imagen (iw x ih, RGBA premultiplicado) con su punto (ax, ay) [px de la imagen] en (dx, dy) del
// lienzo HD, escalas sx/sy (negativa = espejo), giro rot (rad), color * mul/256 + add, opacidad alpha
static void drawAffine(const uint8_t *P, int iw, int ih, float dx, float dy, float ax, float ay, float sx, float sy,
                       float rot, int mulR, int mulG, int mulB, int add, int alpha) {
  uint16_t *fb = gfx->hdfb();
  int W = gfx->hdw(), H = gfx->hdh();
  float c = cosf(rot), s = sinf(rot);
  float asx = fabsf(sx), asy = fabsf(sy);
  float pw = iw * asx, ph = ih * asy;
  float ex = (sx < 0 ? iw - ax : ax) * asx, ey = (sy < 0 ? ih - ay : ay) * asy;  // el ancla en destino
  float cxs[4] = { -ex, pw - ex, -ex, pw - ex }, cys[4] = { -ey, -ey, ph - ey, ph - ey };
  float minx = 1e9f, maxx = -1e9f, miny = 1e9f, maxy = -1e9f;
  for (int i = 0; i < 4; i++) {
    float X = dx + cxs[i] * c - cys[i] * s, Y = dy + cxs[i] * s + cys[i] * c;
    minx = fminf(minx, X); maxx = fmaxf(maxx, X); miny = fminf(miny, Y); maxy = fmaxf(maxy, Y);
  }
  int x0 = (int)floorf(minx), x1 = (int)ceilf(maxx), y0 = (int)floorf(miny), y1 = (int)ceilf(maxy);
  if (x0 < 0) x0 = 0;
  if (y0 < 0) y0 = 0;
  if (x1 > W) x1 = W;
  if (y1 > H) y1 = H;
  for (int y = y0; y < y1; y++) {
    uint16_t *row = fb + (size_t)y * W;
    float ry = y + 0.5f - dy;
    for (int x = x0; x < x1; x++) {
      float rx = x + 0.5f - dx;
      float ux = rx * c + ry * s + ex, uy = -rx * s + ry * c + ey;  // px de destino sin girar, desde la esquina
      float fx = (sx < 0 ? pw - ux : ux) / asx - 0.5f;
      float fy = (sy < 0 ? ph - uy : uy) / asy - 0.5f;
      if (fx < -1 || fy < -1 || fx >= iw || fy >= ih) continue;
      int ix = (int)floorf(fx), iy = (int)floorf(fy);
      float tx = fx - ix, ty = fy - iy;
      float acc[4] = { 0, 0, 0, 0 };
      for (int k = 0; k < 4; k++) {
        int qx = ix + (k & 1), qy = iy + (k >> 1);
        if (qx < 0 || qy < 0 || qx >= iw || qy >= ih) continue;
        float wgt = ((k & 1) ? tx : 1 - tx) * ((k >> 1) ? ty : 1 - ty);
        const uint8_t *q = P + ((size_t)qy * iw + qx) * 4;
        acc[0] += q[0] * wgt; acc[1] += q[1] * wgt; acc[2] += q[2] * wgt; acc[3] += q[3] * wgt;
      }
      int a = (int)(acc[3] * alpha / 255.0f + 0.5f);
      if (a <= 2) continue;
      float inv = 255.0f / acc[3];
      int r = (int)(acc[0] * inv) * mulR / 256 + add, g = (int)(acc[1] * inv) * mulG / 256 + add,
          b = (int)(acc[2] * inv) * mulB / 256 + add;
      r = r > 255 ? 255 : r; g = g > 255 ? 255 : g; b = b > 255 ? 255 : b;
      uint16_t col = (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
      blend(row[x], spread(col), a > 255 ? 255 : a);
    }
  }
}

// acciones PMD (sdmon.h)
enum { A_IDLE = 0, A_WALKL, A_WALKR, A_SLEEP, A_EAT, A_HURT, A_ATTACK, A_POSE, A_HOP, A_NOD, A_BREATH, A_SIT,
       A_ROTATE, A_CHARGE, A_SHOOT, A_LAYING, A_IDLE_UR, A_ATTACK_UR, A_HURT_UR, A_IDLE_DL, A_ATTACK_DL, A_HURT_DL };

bool gHdArt = true;  // [메뉴] "3D 움직임" (por defecto) / "도트"
bool hdArtOn() { return gHdArt; }

// ---------------------------------------------------------------- modelos 3D de verdad (mons/m3d/NNN.m3d)
struct M3dSlot { M3dModel m; uint32_t tried = 0; bool ok = false; };
static std::unordered_map<int, M3dSlot> gM3d;
static M3dModel *m3dGet(int dex) {
  M3dSlot &s = gM3d[dex];
  if (s.ok) return &s.m;
  uint32_t now = millis();
  if (s.tried && now - s.tried < 5000) return nullptr;
  s.tried = now ? now : 1;
  char name[48];
  snprintf(name, sizeof(name), "/mons/m3d/%03d.m3d", dex);
  std::vector<uint8_t> d;
  if (!gSdRoot || !readFile(std::string(gSdRoot) + name, d)) return nullptr;
  if (!s.m.load(d)) return nullptr;
  s.ok = true;
  if (gM3d.size() > 6)
    for (auto it = gM3d.begin(); it != gM3d.end();) it = (it->first != dex && it->second.ok) ? gM3d.erase(it) : std::next(it);
  return &gM3d[dex].m;
}
static bool m3dPet(int16_t dex, uint8_t act, int x, int ground, uint32_t t, float yaw, float hLogical, bool sil, int mulBoost) {
  M3dModel *m = m3dGet(dex);
  if (!m) return false;
  M3dPose pose;
  M3dView v;
  v.yaw = yaw;
  v.pitch = 0.16f;
  v.scale = hLogical * HDK / m->height;
  v.ox = x * HDK + HDK / 2.0f;
  v.oy = ground * HDK;
  m3dAnimate(*m, act, t, pose, v);
  v.silhouette = sil;
  if (mulBoost) v.add += mulBoost;
  m3dDraw(*m, pose, v, gfx->hdfb(), gfx->hdw(), gfx->hdh());
  return true;
}

// pantalla principal: la animacion 3D + lo que hace segun la accion del PMD (andar, saltar, dormir...)
bool hdPetDraw(int16_t dex, bool shiny, uint8_t act, int x, int ground, uint32_t t, bool) {
  if (!gfx || dex <= 0) return false;
  if (m3dPet(dex, act, x, ground, t, -0.35f, 150, false, 0)) return true;  // modelo 3D de verdad (si esta)
  AniImg *a = aniGet(dex, shiny, false);
  if (!a) return false;
  const float TAU = 6.2831853f;
  // tamano: de su altura de verdad hasta 150 x 190 logicos, sin ampliar mas de x1,7 (se veria borroso)
  int ch = a->feet - a->top;
  float fit = fminf(150.0f * HDK / (ch > 8 ? ch : 8), 190.0f * HDK / a->w);
  if (fit > 1.7f * HDK) fit = 1.7f * HDK;
  float sx = fit, sy = fit, rot = 0, ox = 0, oy = 0, speed = 1.0f;
  int mr = 256, mg = 256, mb = 256, add = 0;
  float ph = (float)t;
  switch (act) {
    case A_WALKL:
    case A_WALKR: {
      oy = -fabsf(sinf(ph * TAU / 520.0f)) * 6;
      rot = sinf(ph * TAU / 1040.0f) * 0.04f;
      speed = 1.4f;
      if (act == A_WALKR) sx = -sx;  // las de frente miran a la izquierda
      break;
    }
    case A_SLEEP: speed = 0.35f; mr = mg = 170; mb = 200; oy = 2; break;
    case A_EAT: oy = fabsf(sinf(ph * TAU / 420.0f)) * 5; rot = 0.05f * fabsf(sinf(ph * TAU / 420.0f)); break;
    case A_HURT: {
      ox = sinf(ph * TAU / 110.0f) * 4 * (((t / 700) % 2) ? 0.2f : 1.0f);
      int pulse = (int)(70 * (0.5f + 0.5f * sinf(ph * TAU / 700.0f)));
      mg = mb = 256 - pulse;
      break;
    }
    case A_CHARGE: ox = sinf(ph * TAU / 70.0f) * 1.5f; add = (int)(55 * (0.5f + 0.5f * sinf(ph * TAU / 300.0f))); speed = 1.8f; break;
    case A_ATTACK:
    case A_SHOOT: {
      float k = fmodf(ph, 700.0f) / 700.0f;  // retroceder y lanzarse
      float lunge = k < 0.3f ? -k / 0.3f * 0.4f : k < 0.5f ? -0.4f + (k - 0.3f) / 0.2f * 1.4f : 1.0f - (k - 0.5f) / 0.5f;
      int dir = x < 233 ? 1 : -1;
      ox = lunge * 22 * dir;
      if (dir > 0) sx = -sx;
      if (k > 0.3f && k < 0.6f) add = 50;
      speed = 1.5f;
      break;
    }
    case A_POSE:
    case A_HOP: {
      float k = fmodf(ph, 760.0f) / 760.0f;
      oy = -sinf(k * 3.14159f) * 26;
      break;
    }
    case A_ROTATE: {
      float c = cosf(ph * TAU / 900.0f);
      sx *= c >= 0 ? fmaxf(c, 0.06f) : fminf(c, -0.06f);
      oy = -fabsf(sinf(ph * TAU / 450.0f)) * 6;
      break;
    }
    case A_LAYING: sy *= 0.86f; sx *= 1.06f; speed = 0.6f; break;
    case A_NOD: rot = sinf(ph * TAU / 900.0f) * 0.06f; break;
    default: break;
  }
  const uint8_t *fr = aniFrame(*a, (uint32_t)(millis() * speed));
  drawAffine(fr, a->w, a->h, (x + ox) * HDK + HDK / 2.0f, (ground + oy) * HDK, (float)a->cx, (float)a->feet, sx, sy, rot,
             mr, mg, mb, add, 255);
  return true;
}

// combate: el rival de frente (back=false), el tuyo de espaldas. Escala fija como en los juegos (se nota quien es
// grande) con tope de alto; sil = desmayandose (sombra oscura)
bool hdBattlerDraw(int16_t dex, bool shiny, bool back, int x, int ground, bool sil, uint8_t act) {
  if (!gfx || !gHdArt || dex <= 0) return false;
  // 3D: el rival mira hacia abajo a la izquierda (a ti); el tuyo de espaldas hacia arriba a la derecha
  if (m3dPet(dex, act, x, ground, millis(), back ? 3.14159f - 0.75f : -0.75f, back ? 150 : 118, sil, 0)) return true;
  AniImg *a = aniGet(dex, shiny, back);
  if (!a) return false;
  int ch = a->feet - a->top;
  float sc = (back ? 1.9f : 1.5f) * HDK, maxH = (back ? 170.0f : 125.0f) * HDK;
  if (ch * sc > maxH) sc = maxH / (ch > 1 ? ch : 1);
  int m = sil ? 40 : 256, add = 0;
  if (act == A_ATTACK_DL || act == A_ATTACK_UR || act == A_ATTACK) add = 40;
  if (act == A_HURT_DL || act == A_HURT_UR || act == A_HURT) m = sil ? 40 : 256, add = 0;
  const uint8_t *fr = aniFrame(*a, millis() + (back ? 0 : 333));
  drawAffine(fr, a->w, a->h, x * HDK + HDK / 2.0f, ground * HDK, (float)a->cx, (float)a->feet, sc, sc, 0,
             m, m, m, add, 255);
  return true;
}
