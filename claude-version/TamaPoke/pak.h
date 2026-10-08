#pragma once
// ko12.8: lectura de /mons.pak (toda la carpeta mons en un fichero cifrado, ver pak_core.h).
// Con el .pak en la SD, los ficheros de /mons/... se leen de ahi (si no estan, de la carpeta).
#include <Arduino.h>
#include <FS.h>

#define PAK_PATH "/mons.pak"

bool pakLoad();                        // la llama sdmon al montar la SD (con el cerrojo ya cogido o al arrancar)
void pakUnload();
bool pakActive();                      // hay un mons.pak valido y la frase es la buena
uint32_t pakCount();
int8_t pakState();                     // 0 no hay .pak, 1 cargado, -1 frase incorrecta, -2 roto
File monsOpen(const char *path);       // SD_MMC.open(path, FILE_READ) pero mirando primero en el .pak
bool monsExists(const char *path);
bool monsIsDir(const char *path);      // "/mons/fx": carpeta en la SD o prefijo "fx/" en el .pak
// recorre los nombres del .pak (relativos a /mons/, p. ej. "bgm2.wav")
void pakForEach(void (*cb)(const char *name, uint32_t size, void *ctx), void *ctx);

// frase (la por defecto, o la guardada con PAKPASS)
void pakSetPass(const char *pass);     // "" = volver a la por defecto
const char *pakPassLabel();            // "기본" o "사용자 지정" lo pone la UI segun esto: "" = por defecto

// ---- empaquetar en el aparato: /mons -> /mons.pak ----
struct PakBuildInfo { uint32_t files = 0; uint64_t bytes = 0; };
bool pakScan(PakBuildInfo &out);       // cuenta lo que se empaquetaria
// progress(hecho, total, ficheros hechos, ficheros) ; devuelve 0 ok, 1 sin espacio, 2 error de SD, 3 sin ficheros
uint8_t pakBuild(void (*progress)(uint64_t done, uint64_t total, uint32_t nf, uint32_t tf));
