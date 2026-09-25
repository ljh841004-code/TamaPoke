#pragma once
// Cadenas de las funciones nuevas del fork (WiFi/NTP, batallas, tongsin).
// Solo coreano (idioma por defecto del fork) e ingles; el resto de idiomas cae
// al ingles, que es ASCII puro y se pinta bien con cualquier fuente.
//
// Plantillas: {1} y {2} se sustituyen por nombres, y las particulas coreanas
// {은} {이} {을} {와} eligen la forma correcta segun si la palabra anterior
// acaba en consonante (은/이/을/과) o en vocal (는/가/를/와).
#include <Arduino.h>
#include "i18n.h"

enum XId : uint8_t {
  // red / hora
  X_WIFI, X_NET_TITLE, X_NOT_SET, X_SYNC_NOW, X_SETUP_WIFI, X_AUTO_ON, X_AUTO_OFF,
  X_ST_CONNECTING, X_ST_NTP, X_ST_OK, X_ST_FAIL_WIFI, X_ST_FAIL_NTP, X_ST_BUSY,
  X_LAST_SYNC_FMT, X_NEVER, X_PORTAL_1, X_PORTAL_2, X_PORTAL_3, X_PORTAL_SAVED,
  X_TIMEZONE, X_TAP_CLOSE,
  // batalla
  X_WILD_BTN, X_LINK_BTN, X_WILD_APPEARS, X_WHAT_DO,
  X_M_TACKLE, X_GUARD, X_RUN,
  X_USED, X_MISSED, X_SUPER, X_NOTVERY, X_NOEFFECT, X_CRIT, X_GUARDS, X_FLED,
  X_CANT_RUN, X_FAINTED, X_WIN, X_LOSE, X_REWARD, X_TOO_TIRED, X_WILD_ALERT,
  X_RECORD_FMT,
  // tongsin
  X_LINK_TITLE, X_LINK_BATTLE, X_LINK_TRADE, X_LINK_SEARCH, X_LINK_HINT,
  X_PARTNER_FMT, X_TRADE_Q, X_TRADE_WAIT, X_TRADE_DONE, X_TRADE_NO, X_LINK_LOST,
  X_CANT_NOW, X_TRADE_WARN, X_PARTNER_OK,
  // unidad USB (SD en el PC)
  X_USB_BTN, X_USB_TITLE, X_USB_1, X_USB_2, X_USB_3, X_USB_WAIT, X_USB_SEEN,
  X_USB_DONE, X_USB_FAIL, X_USB_NA,
  X_COUNT
};

const char *XT(XId id);
// nombre del movimiento: BA_TACKLE / BA_TYPE (este segun el tipo PT_*)
const char *moveName(uint8_t move, uint8_t type);
// rellena out con la plantilla id, sustituyendo {1}/{2} y las particulas
void txFmt(char *out, size_t n, XId id, const char *a1, const char *a2 = nullptr);
// lo mismo con una plantilla cualquiera (para los tests)
void txFmtRaw(char *out, size_t n, const char *tpl, const char *a1, const char *a2);
