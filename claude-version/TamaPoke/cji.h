#pragma once
// fork KO (ko8): teclado coreano CHEONJIIN (천지인), el de los moviles de 12 teclas.
// Logica pura (sin pantalla): test/test_box.cpp la prueba en el PC.
//
//   ㅣ   ㆍ   ㅡ        vocales: se construyen con trazos (ㅣ+ㆍ = ㅏ, ㆍ+ㅡ = ㅗ...)
//   ㄱㅋ ㄴㄹ ㄷㅌ       consonantes: pulsar otra vez la misma tecla cambia
//   ㅂㅍ ㅅㅎ ㅈㅊ       (ㄱ -> ㅋ -> ㄲ -> ㄱ ...)
//   띄움 ㅇㅁ 지움      띄움: separa dos consonantes de la misma tecla (각각) o
//                        pone un espacio
//
// Se guarda la secuencia de "piezas" (consonantes y grupos de trazos de vocal)
// y el texto se recompone entero cada vez: asi borrar deshace exactamente el
// ultimo trazo, y una consonante final pasa sola a la silaba siguiente cuando
// llega una vocal (각 + ㅏ = 가가, 닭 + ㅏ = 달가).
#include <stdint.h>
#include <string.h>

#define CJI_MAX_TOK 40
#define CJI_MAX_BYTES 18   // 6 silabas (UTF-8: 3 bytes cada una)

enum : uint8_t { CJI_CONS = 1, CJI_VOW, CJI_SPACE };
enum : uint8_t { CJI_K_I = 0, CJI_K_DOT, CJI_K_EU,       // ㅣ ㆍ ㅡ
                 CJI_K_G, CJI_K_N, CJI_K_D,              // ㄱㅋ ㄴㄹ ㄷㅌ
                 CJI_K_B, CJI_K_S, CJI_K_J,              // ㅂㅍ ㅅㅎ ㅈㅊ
                 CJI_K_SPACE, CJI_K_O, CJI_K_DEL,        // 띄움 ㅇㅁ 지움
                 CJI_K_COUNT };

struct CjiTok {
  uint8_t kind;
  uint8_t cho;      // consonante: indice de choseong (0..18)
  uint8_t group;    // consonante: tecla que la escribio
  char st[6];       // vocal: trazos 'i' '.' '-'
};

struct Cji {
  CjiTok t[CJI_MAX_TOK];
  uint8_t n = 0;
  bool sep = false;  // "띄움" tras una consonante: la misma tecla empieza otra

  void clear() { n = 0; sep = false; }
};

// ---- tablas ----
// ciclo de cada tecla de consonante (indices de choseong)
static const int8_t CJI_CYCLE[7][3] = {
  { 0, 15, 1 },   // ㄱ ㅋ ㄲ
  { 2, 5, -1 },   // ㄴ ㄹ
  { 3, 16, 4 },   // ㄷ ㅌ ㄸ
  { 7, 17, 8 },   // ㅂ ㅍ ㅃ
  { 9, 18, 10 },  // ㅅ ㅎ ㅆ
  { 12, 14, 13 }, // ㅈ ㅊ ㅉ
  { 11, 6, -1 },  // ㅇ ㅁ
};
// choseong -> jamo de compatibilidad (para mostrarla suelta)
static const uint16_t CJI_CHO_COMPAT[19] = {
  0x3131, 0x3132, 0x3134, 0x3137, 0x3138, 0x3139, 0x3141, 0x3142, 0x3143, 0x3145,
  0x3146, 0x3147, 0x3148, 0x3149, 0x314A, 0x314B, 0x314C, 0x314D, 0x314E,
};
// choseong -> jongseong (0 = no puede ir de final: ㄸ ㅃ ㅉ)
static const uint8_t CJI_CHO_JONG[19] = { 1, 2, 4, 7, 0, 8, 16, 17, 0, 19, 20, 21, 22, 0, 23, 24, 25, 26, 27 };

// trazos -> jungseong (-1 = aun incompleta: "ㆍ" o "ㆍㆍ")
struct CjiVow { const char *st; int8_t jung; };
static const CjiVow CJI_VOWELS[] = {
  { "i", 20 }, { "-", 18 }, { ".", -1 }, { "..", -1 },
  { "i.", 0 }, { "i..", 2 }, { ".i", 4 }, { "..i", 6 }, { ".-", 8 }, { "..-", 12 },
  { "-.", 13 }, { "-..", 17 }, { "-i", 19 },
  { "i.i", 1 }, { "i..i", 3 }, { ".ii", 5 }, { "..ii", 7 },
  { ".-i", 11 }, { ".-i.", 9 }, { ".-i.i", 10 }, { "-.i", 16 }, { "-..i", 14 }, { "-..ii", 15 },
};

static inline bool cjiVowel(const char *st, int8_t *jung) {
  for (const CjiVow &v : CJI_VOWELS)
    if (!strcmp(v.st, st)) { if (jung) *jung = v.jung; return true; }
  return false;
}

// jongseong doble (0 si no se combinan)
static inline uint8_t cjiDoubleJong(uint8_t jong, uint8_t cho) {
  switch (jong) {
    case 1: return cho == 9 ? 3 : 0;                        // ㄳ
    case 4: return cho == 12 ? 5 : cho == 18 ? 6 : 0;       // ㄵ ㄶ
    case 8:                                                 // ㄺ ㄻ ㄼ ㄽ ㄾ ㄿ ㅀ
      return cho == 0 ? 9 : cho == 6 ? 10 : cho == 7 ? 11 : cho == 9 ? 12
           : cho == 16 ? 13 : cho == 17 ? 14 : cho == 18 ? 15 : 0;
    case 17: return cho == 9 ? 18 : 0;                      // ㅄ
    default: return 0;
  }
}

static inline int cjiPutUtf8(char *out, int at, int max, uint32_t cp) {
  if (cp < 0x80) {
    if (at + 1 > max) return at;
    out[at++] = (char)cp;
  } else {
    if (at + 3 > max) return at;
    out[at++] = (char)(0xE0 | (cp >> 12));
    out[at++] = (char)(0x80 | ((cp >> 6) & 63));
    out[at++] = (char)(0x80 | (cp & 63));
  }
  return at;
}

static inline bool cjiIsVowelDone(const Cji &c, int i) {
  int8_t j;
  return i < c.n && c.t[i].kind == CJI_VOW && cjiVowel(c.t[i].st, &j) && j >= 0;
}

// recompone el texto. final = true: sin los trazos a medias (para guardar)
static inline int cjiCompose(const Cji &c, char *out, int max, bool final) {
  int at = 0;
  int i = 0;
  while (i < c.n) {
    const CjiTok &k = c.t[i];
    if (k.kind == CJI_SPACE) { at = cjiPutUtf8(out, at, max, ' '); i++; continue; }
    if (k.kind == CJI_CONS && cjiIsVowelDone(c, i + 1)) {
      int8_t v;
      cjiVowel(c.t[i + 1].st, &v);
      uint8_t L = k.cho, T = 0;
      i += 2;
      // final: una consonante que NO va seguida de vocal (si no, empieza la siguiente)
      if (i < c.n && c.t[i].kind == CJI_CONS && CJI_CHO_JONG[c.t[i].cho] && !cjiIsVowelDone(c, i + 1)) {
        T = CJI_CHO_JONG[c.t[i].cho];
        i++;
        if (i < c.n && c.t[i].kind == CJI_CONS && !cjiIsVowelDone(c, i + 1)) {
          uint8_t d = cjiDoubleJong(T, c.t[i].cho);
          if (d) { T = d; i++; }
        }
      }
      at = cjiPutUtf8(out, at, max, 0xAC00 + ((uint32_t)L * 21 + (uint32_t)v) * 28 + T);
      continue;
    }
    if (k.kind == CJI_CONS) {
      at = cjiPutUtf8(out, at, max, CJI_CHO_COMPAT[k.cho]);
    } else {
      int8_t v = -1;
      cjiVowel(k.st, &v);
      if (v >= 0) at = cjiPutUtf8(out, at, max, 0x314F + v);
      else if (!final)  // ㆍ / ㆍㆍ mientras se escribe
        for (const char *p = k.st; *p; p++) at = cjiPutUtf8(out, at, max, 0x318D);
    }
    i++;
  }
  if (at < max) out[at] = 0;
  else if (max > 0) out[max - 1] = 0;
  return at;
}

// aplica una tecla (sin comprobar la longitud: ver cjiPress)
static inline void cjiApply(Cji &c, uint8_t key) {
  bool sep = c.sep;
  c.sep = false;
  if (key == CJI_K_DEL) {
    if (!c.n) return;
    CjiTok &l = c.t[c.n - 1];
    size_t len = l.kind == CJI_VOW ? strlen(l.st) : 0;
    if (len > 1) l.st[len - 1] = 0;  // quita el ultimo trazo
    else c.n--;
    return;
  }
  if (key == CJI_K_SPACE) {
    // tras una consonante, la primera vez solo separa (각각); si no, espacio
    if (!sep && c.n && c.t[c.n - 1].kind == CJI_CONS) { c.sep = true; return; }
    if (c.n && c.t[c.n - 1].kind == CJI_SPACE) return;  // sin espacios dobles
    if (c.n < CJI_MAX_TOK) { c.t[c.n] = CjiTok(); c.t[c.n++].kind = CJI_SPACE; }
    return;
  }
  if (key <= CJI_K_EU) {
    char s = key == CJI_K_I ? 'i' : key == CJI_K_DOT ? '.' : '-';
    if (c.n && c.t[c.n - 1].kind == CJI_VOW) {
      CjiTok &l = c.t[c.n - 1];
      size_t len = strlen(l.st);
      if (len + 1 < sizeof(l.st)) {
        char tryst[sizeof(l.st)];
        memcpy(tryst, l.st, len);
        tryst[len] = s;
        tryst[len + 1] = 0;
        if (cjiVowel(tryst, nullptr)) { memcpy(l.st, tryst, len + 2); return; }
      }
    }
    if (c.n >= CJI_MAX_TOK) return;
    CjiTok &v = c.t[c.n++];
    v = CjiTok();
    v.kind = CJI_VOW;
    v.st[0] = s;
    return;
  }
  // consonantes
  uint8_t g = key == CJI_K_O ? 6 : (uint8_t)(key - CJI_K_G);
  if (g > 6) return;
  if (!sep && c.n && c.t[c.n - 1].kind == CJI_CONS && c.t[c.n - 1].group == g) {
    CjiTok &l = c.t[c.n - 1];  // misma tecla: siguiente del ciclo
    int pos = 0;
    for (int k = 0; k < 3; k++) if (CJI_CYCLE[g][k] == l.cho) pos = k;
    int nx = (pos + 1) % 3;
    if (CJI_CYCLE[g][nx] < 0) nx = 0;
    l.cho = (uint8_t)CJI_CYCLE[g][nx];
    return;
  }
  if (c.n >= CJI_MAX_TOK) return;
  CjiTok &t = c.t[c.n++];
  t = CjiTok();
  t.kind = CJI_CONS;
  t.group = g;
  t.cho = (uint8_t)CJI_CYCLE[g][0];
}

// tecla con tope de longitud: si el nombre pasaria de CJI_MAX_BYTES, no entra
static inline bool cjiPress(Cji &c, uint8_t key) {
  Cji before = c;
  cjiApply(c, key);
  char buf[64];
  if (cjiCompose(c, buf, sizeof(buf), false) > CJI_MAX_BYTES) { c = before; return false; }
  return true;
}
