#pragma once
// ko12.10.13: "수원시 영통구", "강남구", "서울 중구", "해운대" -> el 시·군·구 de kr_places.h (sin internet).
// Cada palabra (sin el "시/군/구/동..." del final si quedan 2 silabas o mas) se busca en "provincia + nombre";
// gana el que tiene mas palabras; a igualdad, el que tiene la ULTIMA palabra en su nombre (no solo en la provincia)
#include <string.h>
#include <stdio.h>
#include "kr_places.h"

static inline void krRoot(char *t) {
  static const char *const SUF[] = { "특별자치시", "특별자치도", "특별시", "광역시", "시", "군", "구", "도", "동", "읍", "면" };
  size_t n = strlen(t);
  for (const char *s : SUF) {
    size_t k = strlen(s);
    if (n >= k + 6 && !strcmp(t + n - k, s)) { t[n - k] = 0; return; }  // quedan 2 silabas (6 bytes) o mas
  }
}

// true y el sitio (lat/lon, "경기도 수원시영통구") si alguna palabra coincide
static inline bool krFind(const char *q, float *la, float *lo, char *found, size_t fn) {
  char tok[6][32];
  int nt = 0;
  const char *p = q;
  while (*p && nt < 6) {
    while (*p == ' ' || *p == ',') p++;
    if (!*p) break;
    size_t k = 0;
    while (*p && *p != ' ' && *p != ',' && k < sizeof(tok[0]) - 1) tok[nt][k++] = *p++;
    while (*p && *p != ' ' && *p != ',') p++;
    tok[nt][k] = 0;
    krRoot(tok[nt]);
    if (tok[nt][0]) nt++;
  }
  if (!nt) return false;
  int best = -1, bestScore = 0;
  for (unsigned i = 0; i < KR_PLACES_N; i++) {
    const KrPlace &pl = KR_PLACES[i];
    char hay[96];
    snprintf(hay, sizeof(hay), "%s %s", KR_PROV[pl.prov], pl.name);
    char pr[40];
    snprintf(pr, sizeof(pr), "%s", KR_PROV[pl.prov]);
    krRoot(pr);
    int sc = 0;
    for (int t = 0; t < nt; t++) {
      if (strstr(hay, tok[t])) sc += 10;
      if (!strcmp(pr, tok[t])) sc += 6;  // "광주" = 광주광역시 (no 경기도 광주시), "서울", "제주"...
    }
    if (sc && strstr(pl.name, tok[nt - 1])) sc += 5;
    if (sc > bestScore) { bestScore = sc; best = (int)i; }
  }
  if (best < 0) return false;
  const KrPlace &b = KR_PLACES[best];
  *la = b.lat100 / 100.0f;
  *lo = b.lon100 / 100.0f;
  snprintf(found, fn, "%s %s", KR_PROV[b.prov], b.name);
  return true;
}
