#pragma once
// WiFi + NTP: pone el reloj en hora sola por internet (fork KO).
//
// - La red y la contrasena se guardan en NVS ("tpnet"). Se configuran desde el
//   movil (portal WiFi del propio TamaPoke, 192.168.4.1) o por serie:
//     WIFI mired|contrasena    NTP    TZ 540    WIFIOFF
// - La hora del juego es la hora LOCAL guardada como si fuera UTC (asi lo hace
//   todo el firmware), asi que al epoch UTC del NTP se le suma la zona horaria
//   (por defecto UTC+9, Corea).
// - La radio solo se enciende unos segundos para sincronizar (al arrancar y una
//   vez al dia) y se apaga despues: no gasta bateria ni molesta al tongsin.
#include <Arduino.h>

enum NetState : uint8_t {
  NET_IDLE = 0, NET_CONNECTING, NET_NTP, NET_OK, NET_FAIL_WIFI, NET_FAIL_NTP, NET_PORTAL,
  NET_SCAN,  // ko8: buscando WiFi (guardadas + abiertas)
};

void netBegin();
bool netConfigured();
const char *netSsid();
int16_t netTzMin();              // minutos respecto a UTC (Corea = +540)
void netSetTzMin(int16_t m);
bool netAuto();                  // sincronizar solo (al arrancar + cada 24 h)
void netSetAuto(bool on);
uint32_t netLastSync();          // hora local de la ultima sincronizacion (0 = nunca)
bool netSetCreds(const char *ssid, const char *pass);  // guarda (al principio de la lista)
void netClearCreds();            // olvida todas
// ko8: hasta 5 WiFi guardadas + WiFi abiertas como ultimo recurso
uint8_t netSavedCount();
const char *netSavedSsid(uint8_t i);
void netForgetSaved(uint8_t i);
bool netOpenAllowed();
void netSetOpenAllowed(bool on);

void netSyncNow();               // arranca una sincronizacion (no bloquea)
bool netBusy();                  // la radio esta en uso por la red
NetState netState();
void netStartPortal();           // punto de acceso + web de configuracion
void netStopPortal();
bool netPortalOn();
const char *netApName();

// llamar en cada loop(). Devuelve la hora LOCAL (epoch) en el instante en que
// termina una sincronizacion con exito; 0 el resto del tiempo.
uint32_t netPoll(uint32_t nowMs);

bool netSerialCommand(const String &line);  // WIFI / WIFIOFF / NTP / TZ / NET
