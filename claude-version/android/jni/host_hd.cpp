// Android (modo HD): lo que el lienzo 3x (host_hd_canvas.h) no puede sacar solo ampliando:
//  - texto coreano y ASCII con Noto Sans KR de verdad (stb_truetype) a 3x, en las MISMAS posiciones y
//    avances que font_ko.h (la maquetacion del firmware no cambia: solo se ve nitido)
//  - la mascota de la pantalla principal con el dibujo grande (mons/hd/fNNN.png, sNNN.png = shiny)
//    animado por codigo segun la accion PMD (respirar, andar, dormir, comer, dolor, cargar, atacar...)
// Si falta la fuente o el dibujo, el firmware dibuja como siempre (devuelven false).
#include "host_hd_canvas.h"
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

// ---------------------------------------------------------------- dibujo grande de la mascota
struct HdImg { int w = 0, h = 0; std::vector<uint8_t> rgba; uint32_t tried = 0; bool ok = false; };
static std::unordered_map<int, HdImg> gImgs;

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

static HdImg *petImg(int dex, bool shiny) {
  int key = dex * 2 + (shiny ? 1 : 0);
  HdImg &im = gImgs[key];
  if (im.ok) return &im;
  uint32_t now = millis();
  if (im.tried && now - im.tried < 5000) return nullptr;  // no estaba: se vuelve a mirar (importar hd.pak)
  im.tried = now ? now : 1;
  if (!gSdRoot) return nullptr;
  char name[64];
  snprintf(name, sizeof(name), "/mons/hd/%c%03d.png", shiny ? 's' : 'f', dex);
  std::vector<uint8_t> d;
  if (!readFile(std::string(gSdRoot) + name, d)) {
    if (!shiny) return nullptr;
    HdImg *n = petImg(dex, false);  // sin dibujo shiny: el normal
    return n;
  }
  int w, h, ch;
  uint8_t *px = stbi_load_from_memory(d.data(), (int)d.size(), &w, &h, &ch, 4);
  if (!px) return nullptr;
  // alfa premultiplicado (la mezcla bilineal no ensucia los bordes)
  im.rgba.assign(px, px + (size_t)w * h * 4);
  stbi_image_free(px);
  for (size_t i = 0; i < im.rgba.size(); i += 4) {
    unsigned a = im.rgba[i + 3];
    im.rgba[i] = (uint8_t)(im.rgba[i] * a / 255);
    im.rgba[i + 1] = (uint8_t)(im.rgba[i + 1] * a / 255);
    im.rgba[i + 2] = (uint8_t)(im.rgba[i + 2] * a / 255);
  }
  im.w = w; im.h = h; im.ok = true;
  if (gImgs.size() > 12) {  // solo hacen falta la actual y poco mas
    for (auto it = gImgs.begin(); it != gImgs.end();)
      it = (it->first != key) ? gImgs.erase(it) : std::next(it);
  }
  return &gImgs[key];
}

// Dibuja la imagen con su punto (ax, ay) de la imagen [0..1] en (dx, dy) del lienzo HD, escalas sx/sy
// (negativa = espejo), giro rot (rad), color multiplicado por mul (0..256) y sumado add (0..255)
static void drawAffine(const HdImg &im, float dx, float dy, float ax, float ay, float sx, float sy, float rot,
                       int mulR, int mulG, int mulB, int add, int alpha) {
  uint16_t *fb = gfx->hdfb();
  int W = gfx->hdw(), H = gfx->hdh();
  float c = cosf(rot), s = sinf(rot);
  float pw = im.w * fabsf(sx), ph = im.h * fabsf(sy);
  float ex = ax * pw, ey = ay * ph;  // el ancla en px de destino (sin girar)
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
  float isx = 1.0f / sx, isy = 1.0f / sy;
  const uint8_t *P = im.rgba.data();
  int iw = im.w, ih = im.h;
  for (int y = y0; y < y1; y++) {
    uint16_t *row = fb + (size_t)y * W;
    float ry = y + 0.5f - dy;
    for (int x = x0; x < x1; x++) {
      float rx = x + 0.5f - dx;
      // al sistema de la imagen: girar al reves, quitar escala
      float ux = rx * c + ry * s, uy = -rx * s + ry * c;
      float fx = (sx < 0 ? (pw - (ux + ex)) : (ux + ex)) / fabsf(sx) - 0.5f;
      float fy = (sy < 0 ? (ph - (uy + ey)) : (uy + ey)) / fabsf(sy) - 0.5f;
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
      // premultiplicado -> color
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
       A_ROTATE, A_CHARGE, A_SHOOT, A_LAYING };

bool gHdArt = false;  // [메뉴] "HD 그림": la mascota con el dibujo grande (quieto) en vez del PMD animado
bool hdArtOn() { return gHdArt; }

bool hdPetDraw(int16_t dex, bool shiny, uint8_t act, int x, int ground, uint32_t t, bool) {
  if (!gfx || dex <= 0) return false;
  HdImg *im = petImg(dex, shiny);
  if (!im) return false;
  const float TAU = 6.2831853f;
  // tamano: cabe en 180 x 146 logicos (el PMD x6 ocupa mas o menos eso; mas grande tapa el reloj)
  float fit = fminf(146.0f * HDK / im->h, 180.0f * HDK / im->w);
  float sx = fit, sy = fit, rot = 0, ox = 0, oy = 0;
  int mr = 256, mg = 256, mb = 256, add = 0;
  float ph = (float)t;
  switch (act) {
    case A_WALKL:
    case A_WALKR: {
      float b = fabsf(sinf(ph * TAU / 520.0f));
      oy = -b * 7;
      rot = sinf(ph * TAU / 1040.0f) * 0.05f;
      sy *= 1.0f + 0.02f * b;
      if (act == A_WALKR) sx = -sx;  // el dibujo mira a la izquierda
      break;
    }
    case A_SLEEP: {
      float b = sinf(ph * TAU / 3200.0f);
      sy *= 0.94f + 0.025f * b; sx *= 1.03f - 0.012f * b;
      mr = mg = 175; mb = 205;
      break;
    }
    case A_EAT: {
      float b = fabsf(sinf(ph * TAU / 420.0f));
      sy *= 1.0f - 0.07f * b; sx *= 1.0f + 0.045f * b;
      rot = 0.04f * b;
      break;
    }
    case A_HURT: {
      ox = sinf(ph * TAU / 110.0f) * 4 * (((t / 700) % 2) ? 0.2f : 1.0f);
      int pulse = (int)(60 * (0.5f + 0.5f * sinf(ph * TAU / 700.0f)));
      mg = mb = 256 - pulse;
      sy *= 0.97f;
      break;
    }
    case A_CHARGE: {
      ox = sinf(ph * TAU / 70.0f) * 1.5f;
      add = (int)(55 * (0.5f + 0.5f * sinf(ph * TAU / 300.0f)));
      sy *= 0.96f; sx *= 1.03f;
      break;
    }
    case A_ATTACK:
    case A_SHOOT: {
      float k = fmodf(ph, 700.0f) / 700.0f;  // retroceder y lanzarse
      float lunge = k < 0.3f ? -k / 0.3f * 0.4f : k < 0.5f ? -0.4f + (k - 0.3f) / 0.2f * 1.4f : 1.0f - (k - 0.5f) / 0.5f;
      int dir = x < 233 ? 1 : -1;
      ox = lunge * 22 * dir;
      rot = lunge * 0.08f * dir;
      if (dir > 0) sx = -sx;
      if (k > 0.3f && k < 0.6f) add = 50;
      break;
    }
    case A_POSE:
    case A_HOP: {
      float k = fmodf(ph, 760.0f) / 760.0f;
      float j = sinf(k * 3.14159f);
      oy = -j * 26;
      sy *= k < 0.08f || k > 0.92f ? 0.92f : 1.0f + 0.04f * j;
      sx *= k < 0.08f || k > 0.92f ? 1.06f : 1.0f;
      break;
    }
    case A_NOD:
      rot = sinf(ph * TAU / 900.0f) * 0.08f;
      break;
    case A_ROTATE: {
      float c = cosf(ph * TAU / 900.0f);
      sx *= c >= 0 ? fmaxf(c, 0.06f) : fminf(c, -0.06f);
      oy = -fabsf(sinf(ph * TAU / 450.0f)) * 6;
      break;
    }
    case A_LAYING: {
      float b = sinf(ph * TAU / 2600.0f);
      sy *= 0.82f + 0.02f * b; sx *= 1.1f;
      rot = 0.06f;
      break;
    }
    case A_SIT:
      sy *= 0.93f; sx *= 1.03f;
      break;
    default: {  // quieto / respirar
      float b = sinf(ph * TAU / 1700.0f);
      sy *= 1.0f + 0.022f * b; sx *= 1.0f - 0.01f * b;
      break;
    }
  }
  drawAffine(*im, (x + ox) * HDK + HDK / 2.0f, (ground + oy) * HDK, 0.5f, 1.0f, sx, sy, rot, mr, mg, mb, add, 255);
  return true;
}
