#pragma once
// fork KO (ko8): a que WiFi intentar conectarse para poner la hora.
// Logica pura (sin radio): test/test_box.cpp la prueba en el PC.
//
// Orden de intento:
//   1. las WiFi guardadas que se ven ahora, de la senal mas fuerte a la mas debil
//   2. la guardada mas reciente aunque no se vea (red oculta o escaneo que no la pillo)
//   3. si se permite, las WiFi abiertas (sin contrasena) que se ven, las mas fuertes primero
// Las abiertas suelen pedir registro (portal cautivo) y entonces el NTP falla:
// por eso van al final y se prueba la siguiente si una falla.
#include <stdint.h>
#include <string.h>

#define NET_MAX_SAVED 5
#define NET_MAX_CAND 8
#define NET_MAX_OPEN 3

struct NetSeen {
  const char *ssid;
  int16_t rssi;
  bool open;
};

struct NetCand {
  int8_t saved;      // indice en la lista guardada, -1 = red abierta
  int8_t seen;       // indice en el escaneo, -1 = no se vio
};

// saved[i] = SSID guardado (el 0 es el mas reciente). Devuelve cuantos candidatos.
static inline int netPickCandidates(const char (*saved)[33], int nSaved, const NetSeen *seen, int nSeen,
                                    bool allowOpen, NetCand *out, int maxOut) {
  int n = 0;
  // mejor senal de cada guardada (un mismo SSID puede tener varios puntos de acceso)
  int best[NET_MAX_SAVED];
  for (int i = 0; i < nSaved && i < NET_MAX_SAVED; i++) {
    best[i] = -1;
    if (!saved[i][0]) continue;
    for (int j = 0; j < nSeen; j++)
      if (seen[j].ssid && !strcmp(seen[j].ssid, saved[i]) && (best[i] < 0 || seen[j].rssi > seen[best[i]].rssi))
        best[i] = j;
  }
  // 1. guardadas visibles, por senal
  bool used[NET_MAX_SAVED] = { false };
  for (;;) {
    int pick = -1;
    for (int i = 0; i < nSaved && i < NET_MAX_SAVED; i++)
      if (!used[i] && best[i] >= 0 && (pick < 0 || seen[best[i]].rssi > seen[best[pick]].rssi)) pick = i;
    if (pick < 0 || n >= maxOut) break;
    used[pick] = true;
    out[n].saved = (int8_t)pick;
    out[n].seen = (int8_t)best[pick];
    n++;
  }
  // 2. la mas reciente aunque no se vea
  if (nSaved > 0 && saved[0][0] && !used[0] && n < maxOut) {
    out[n].saved = 0;
    out[n].seen = -1;
    n++;
  }
  // 3. abiertas visibles (que no esten guardadas), por senal, sin repetir SSID
  if (allowOpen) {
    int opens = 0;
    bool taken[64] = { false };
    while (opens < NET_MAX_OPEN && n < maxOut) {
      int pick = -1;
      for (int j = 0; j < nSeen && j < 64; j++) {
        if (taken[j] || !seen[j].open || !seen[j].ssid || !seen[j].ssid[0]) continue;
        if (pick < 0 || seen[j].rssi > seen[pick].rssi) pick = j;
      }
      if (pick < 0) break;
      // el mismo SSID (varios puntos de acceso) o uno ya guardado: se descarta
      bool skip = false;
      for (int i = 0; i < nSaved && i < NET_MAX_SAVED && !skip; i++) skip = !strcmp(saved[i], seen[pick].ssid);
      for (int k = 0; k < n && !skip; k++)
        skip = out[k].saved < 0 && !strcmp(seen[out[k].seen].ssid, seen[pick].ssid);
      for (int j = 0; j < nSeen && j < 64; j++)
        if (seen[j].ssid && !strcmp(seen[j].ssid, seen[pick].ssid)) taken[j] = true;
      if (skip) continue;
      out[n].saved = -1;
      out[n].seen = (int8_t)pick;
      n++;
      opens++;
    }
  }
  return n;
}

// guarda un SSID al principio de la lista (mas reciente). Si ya estaba, sube
// arriba y se actualiza la contrasena; si no cabe, se pierde la mas antigua.
static inline void netRememberFront(char (*ssids)[33], char (*passes)[65], uint8_t &n, const char *ssid,
                                    const char *pass) {
  int at = -1;
  for (int i = 0; i < n; i++)
    if (!strcmp(ssids[i], ssid)) at = i;
  if (at < 0) at = n < NET_MAX_SAVED ? n++ : NET_MAX_SAVED - 1;
  for (int i = at; i > 0; i--) {
    memcpy(ssids[i], ssids[i - 1], 33);
    memcpy(passes[i], passes[i - 1], 65);
  }
  strncpy(ssids[0], ssid, 32);
  ssids[0][32] = 0;
  strncpy(passes[0], pass ? pass : "", 64);
  passes[0][64] = 0;
}

static inline void netForget(char (*ssids)[33], char (*passes)[65], uint8_t &n, int idx) {
  if (idx < 0 || idx >= n) return;
  for (int i = idx; i + 1 < n; i++) {
    memcpy(ssids[i], ssids[i + 1], 33);
    memcpy(passes[i], passes[i + 1], 65);
  }
  n--;
  ssids[n][0] = passes[n][0] = 0;
}
