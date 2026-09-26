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

enum XId : uint16_t {  // ko10.6: pasaron de 255
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
  X_GAME_REWARD, X_GAME_NO_REWARD, X_GAME_SMALL,
  X_NEXT_Q, X_NEXT_GO, X_NEXT_EXIT,
  X_ST_SCAN, X_AUTO_S_ON, X_AUTO_S_OFF, X_OPEN_ON, X_OPEN_OFF, X_SAVED_MORE_FMT,
  X_DATE_FMT, X_WDAYS,  // ko10.1: fecha bajo el reloj (dias de 3 bytes, domingo primero)
  // ko10.1: regiones del salvaje ({1} = region, {2} = Pokemon)
  X_REGION_Q, X_WILD_AT, X_WILD_RARE,
  // ko10.4: caramelos
  X_DUP_Q, X_DUP_KEEP, X_DUP_CANDY, X_CANDY_GOT, X_CANDY_HAVE, X_CANDY_TITLE,
  X_CU_EXP, X_CU_GAUGE, X_CU_GENES, X_CU_SHINY, X_CU_EVO, X_CU_SHINY_ON, X_CANDY_USED, X_CANDY_NO,
  // ko10.4: tiempo en batalla, gimnasios, reto del dia, regiones con medalla
  X_WX_RAIN, X_WX_SUNNY, X_WX_SNOW,
  X_GYM_BTN, X_DAILY_BTN, X_GYM_TITLE_FMT, X_GYM_GO, X_GYM_AGAIN, X_GYM_LOCK_FMT,
  X_GYM_INTRO, X_TRAINER_NEXT, X_TRAINER_NO, X_BADGE_GOT,
  X_LEADER_0, X_LEADER_1, X_LEADER_2, X_LEADER_3, X_LEADER_4, X_LEADER_5, X_LEADER_6, X_LEADER_7,
  X_BADGE_0, X_BADGE_1, X_BADGE_2, X_BADGE_3, X_BADGE_4, X_BADGE_5, X_BADGE_6, X_BADGE_7,
  X_REGION_LOCK_FMT, X_REGION_LOCKED,
  X_DAILY_INFO, X_DAILY_REWARD, X_DAILY_DONE_TODAY, X_DAILY_NOCLOCK, X_DAILY_INTRO, X_DAILY_WIN,
  X_DAILY_COUNT_FMT, X_DAILY_FOE,
  X_MONTH, X_DAY, X_TIME_FROM_LINK,  // ko10.4: fecha a mano y hora por tongsin
  X_BGM_LEN_FMT, X_BGM_NONE, X_DUP_COUNT_FMT,
  X_RAISED_TAG, X_NEXT_TITLE, X_NEXT_EGG, X_NEXT_FROM, X_NEXT_RAISED, X_NEXT_HINT,  // ko10.5
  X_HALL_TAB_FMT, X_HALL_EMPTY, X_HALL_NOTE,
  X_MIST_HEAL_HM, X_MIST_HEAL_M, X_MIST_HEAL_HINT,  // ko10.6
  X_SPE_PTS_FMT, X_SPE_AVG_FMT,
  X_TRAIN_EXP_FMT, X_TRAIN_EXP_CANDY_FMT, X_TRAIN_CANDY, X_SACK_BAGS_FMT, X_SACK_HITS_FMT,  // ko10.7
  X_BUB_BORED, X_BUB_HUNGRY, X_BUB_DIRTY, X_BUB_COLD,  // ko10.8
  X_MIST_TAP, X_MW_NONE, X_MW_FOOD, X_MW_JOY, X_MW_ENERGY, X_MW_HYGIENE, X_MW_LAST_FMT,  // ko10.9
  X_REG_0, X_REG_1, X_REG_2, X_REG_3, X_REG_4, X_REG_5, X_REG_6, X_REG_7,
  X_REG_8, X_REG_9, X_REG_10, X_REG_11, X_REG_12, X_REG_13, X_REG_14, X_REG_15,
  X_COUNT
};

const char *XT(XId id);
// nombre del tipo PT_* (fork KO, ko4: ficha de la pokedex)
const char *typeName(uint8_t type);
// nombre del movimiento: BA_TACKLE / BA_TYPE (este segun el tipo PT_*)
const char *moveName(uint8_t move, uint8_t type, uint8_t tier = 0);  // ko10.4: tier = moveTier(dex)
// rellena out con la plantilla id, sustituyendo {1}/{2} y las particulas
void txFmt(char *out, size_t n, XId id, const char *a1, const char *a2 = nullptr);
// lo mismo con una plantilla cualquiera (para los tests)
void txFmtRaw(char *out, size_t n, const char *tpl, const char *a1, const char *a2);
