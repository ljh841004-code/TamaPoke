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
  // ko4: objetos y captura
  X_POTION_FMT, X_BALL_FMT, X_NO_POTION, X_NO_BALL, X_THREW, X_CAUGHT, X_BROKE, X_HEALED,
  X_TO_BOX, X_BOX_FULL, X_REWARD_ITEMS, X_GOTCHA,
  // ko4: caja
  X_BOX_TITLE_FMT, X_BOX_BTN, X_BOX_EMPTY, X_BOX_HINT, X_BOX_NEXT, X_RELEASE, X_CLOSE,
  X_CAUGHT_TAG, X_WON_TAG, X_FROM_BOX, X_ITEMS_FMT, X_RELEASE_Q,
  // ko4: entrenamiento
  X_TRAIN_BTN, X_TRAIN_TITLE, X_TR_ATK, X_TR_DEF, X_TR_SPE, X_TR_PLAY, X_BEST_FMT,
  X_TR_DEF_HINT, X_TR_SPE_HINT, X_BLOCKED_FMT, X_SPE_RESULT_FMT, X_DEF_GAIN_FMT, X_SPE_GAIN_FMT,
  X_NICE, X_MISS,
  // ko4: sonido
  X_SOUND_TITLE, X_VOL_BGM, X_VOL_CRY, X_VOL_SYS, X_SOUND_ON, X_SOUND_OFF, X_VOL_DONE,
  // ko4: pokedex
  X_RARITY_EVO, X_RARITY_COMMON, X_RARITY_RARE, X_RARITY_LEGEND, X_BASE_FMT, X_EVO_FMT,
  X_EVO_FINAL, X_FIRST_FMT, X_SEEN_FMT, X_RAISED, X_UNKNOWN,
  // ko5
  X_JOINED, X_UPD_BTN, X_UPD_TITLE, X_UPD_NOFILE, X_UPD_FULLIMG, X_UPD_SIZE_FMT, X_UPD_GO,
  X_UPD_CANCEL, X_UPD_WRITING, X_UPD_DONE, X_UPD_FAIL, X_UPD_HINT, X_DEX_EXIT,
  X_UPD_CUR_FMT, X_UPD_FILE_FMT, X_UPD_SAME,
  // ko7: experiencia
  X_EXP_NEXT_FMT, X_EXP_GAIN_FMT, X_LVUP_FMT, X_MAX_LVL,
  // ko8: nuevo comienzo
  X_RESET_BTN, X_RESET_TITLE, X_RESET_L1, X_RESET_L2, X_RESET_L3, X_RESET_HOLD, X_RESET_HINT,
  X_RESET_DONE,
  // ko8: varias WiFi
  X_QR_HINT, X_QR_WIFI, X_QR_PASS, X_QR_ADDR,
  X_KB_SPACE, X_KB_DEL, X_KB_HANGUL,
  X_FOOD_CANDY, X_CARE_LEFT_HM, X_CARE_LEFT_M, X_TRAIN_QUIT_HINT,
  X_ST_SCAN, X_AUTO_S_ON, X_AUTO_S_OFF, X_OPEN_ON, X_OPEN_OFF, X_SAVED_MORE_FMT,
  X_COUNT
};

const char *XT(XId id);
// nombre del tipo PT_* (fork KO, ko4: ficha de la pokedex)
const char *typeName(uint8_t type);
// nombre del movimiento: BA_TACKLE / BA_TYPE (este segun el tipo PT_*)
const char *moveName(uint8_t move, uint8_t type);
// rellena out con la plantilla id, sustituyendo {1}/{2} y las particulas
void txFmt(char *out, size_t n, XId id, const char *a1, const char *a2 = nullptr);
// lo mismo con una plantilla cualquiera (para los tests)
void txFmtRaw(char *out, size_t n, const char *tpl, const char *a1, const char *a2);
