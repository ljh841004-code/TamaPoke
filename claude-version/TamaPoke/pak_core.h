#pragma once
// ko12.8: mons.pak = toda la carpeta /mons de la SD en UN fichero cifrado.
// Logica pura (compila en el PC para los tests): AES-128 en modo CTR por posicion
// absoluta (se puede leer cualquier trozo sin descifrar lo anterior), clave sacada
// de una frase, cabecera y tabla de nombres. Lo que toca la SD esta en pak.cpp.
//
// Formato (little endian):
//   0  "TPAK"           4  u16 version (1)      6  u16 flags (0)
//   8  u32 ficheros     12 u32 off. de la tabla  16 u32 tamano de la tabla
//   20 u8 sal[8]        28 u8 comprobacion[16] = AES_k("TAMAPOKE-PAK-OK!")
//   44 u32 off. de los datos (64)   48..63 a cero
//   datos: los ficheros seguidos, cifrados
//   tabla (cifrada): por fichero u32 off, u32 tamano, u8 largo, nombre (relativo a /mons/,
//                    p. ej. "p001.bin" o "fx/f0100.bin"), ordenada por nombre
// Bloque de clave i (bytes 16*i .. 16*i+15 del fichero) = AES_k(sal || i en big endian de 8 bytes)
#include <stddef.h>
#include <stdint.h>

#define PAK_MAGIC "TPAK"
#define PAK_VERSION 1
#define PAK_HDR 64
#define PAK_NAME_MAX 40
#define PAK_DEFAULT_PASS "tamapoke-ko"
#define PAK_CHECK_TEXT "TAMAPOKE-PAK-OK!"

struct PakAes {  // AES-128 (solo cifrar: en CTR descifrar es lo mismo)
  uint8_t rk[176];
  void setKey(const uint8_t key[16]);
  void block(const uint8_t in[16], uint8_t out[16]) const;
};

void pakSha256(const uint8_t *data, size_t n, uint8_t out[32]);
void pakDeriveKey(const char *pass, uint8_t key[16]);  // SHA-256("TamaPoke-pak:" + frase)[0..15]

struct PakHdr {
  uint32_t count = 0, indexOff = 0, indexSize = 0, dataOff = PAK_HDR;
  uint8_t salt[8] = {};
  uint8_t check[16] = {};
};
bool pakParseHeader(const uint8_t h[PAK_HDR], PakHdr &out);
void pakWriteHeader(const PakHdr &h, uint8_t out[PAK_HDR]);
void pakMakeCheck(const PakAes &aes, uint8_t out[16]);
bool pakKeyOk(const PakAes &aes, const PakHdr &h);

// XOR con el flujo de clave a partir de la posicion absoluta abs del fichero
void pakCrypt(const PakAes &aes, const uint8_t salt[8], uint32_t abs, uint8_t *buf, size_t n);

// tabla de nombres ya descifrada: se valida y se indexa (sin copiar los nombres)
struct PakEntry {
  uint32_t off, size;
  const char *name;  // apunta dentro de la tabla (sin terminar en 0: usar len)
  uint8_t len;
};
struct PakIndex {
  PakEntry *e = nullptr;
  uint32_t n = 0;
  // fileSize: tamano del .pak (para acotar off+size)
  bool parse(const uint8_t *tbl, uint32_t tblSize, uint32_t count, uint32_t fileSize);
  void clear();
  const PakEntry *find(const char *rel) const;  // nombre relativo a /mons/
  bool hasPrefix(const char *prefix) const;     // p. ej. "fx/"
  ~PakIndex() { clear(); }
};

// ---- ko12.8.3: mapa de sectores del .pak (lectura sin recorrer la cadena FAT) ----
// FatFs (sin "fast seek" en este core) recorre la cadena de clusteres desde el principio en cada
// fichero abierto y en cada salto atras: en un .pak de 270 MB con clusteres pequenos eso son
// cientos de sectores de la FAT por fichero (el arranque se quedaba en 19/22). La cadena se
// recorre UNA vez al montar y se guarda como tramos contiguos; luego cada lectura va directa.
struct PakExt {
  uint32_t off;    // byte del fichero donde empieza el tramo (multiplo del cluster)
  uint32_t sect;   // primer sector del tramo
  uint32_t nsect;  // sectores seguidos
};
// lee 'count' sectores de 512 bytes a partir de 'sector'; true si fue bien
typedef bool (*PakSectorRead)(uint32_t sector, uint32_t count, uint8_t *buf, void *ctx);
struct PakFatGeo {
  bool fat32;         // si no, FAT16 (FAT12 / exFAT: no soportado)
  uint32_t fatbase;   // primer sector de la FAT
  uint32_t database;  // primer sector del cluster 2
  uint32_t csize;     // sectores por cluster
  uint32_t nFatent;   // clusteres + 2
};
// recorre la cadena desde sclust para un fichero de 'size' bytes. Devuelve el numero de tramos
// (<= maxOut), -1 si la cadena no cuadra (rota, en bucle, corta) o -2 si no caben en maxOut.
// 'win' es un buffer de 8 sectores (4 KB) para leer la FAT por ventanas.
int pakChainExtents(const PakFatGeo &g, uint32_t sclust, uint32_t size, PakSectorRead rd, void *ctx, uint8_t *win,
                    PakExt *out, int maxOut);
// tramo que contiene el byte 'off' (nullptr si fuera)
const PakExt *pakExtFind(const PakExt *e, int n, uint32_t off);
// lee 'len' bytes desde 'off' por los tramos, usando 'bounce' (8 sectores, 4 KB) para leer sectores
// enteros. Devuelve los bytes leidos (len si fue bien)
uint32_t pakExtRead(const PakExt *e, int n, uint32_t off, uint8_t *dst, uint32_t len, PakSectorRead rd, void *ctx,
                    uint8_t *bounce);
