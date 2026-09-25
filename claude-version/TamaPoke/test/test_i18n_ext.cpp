// Tests de las cadenas nuevas del fork (i18n_ext.cpp): particulas coreanas y
// que ninguna cadena falte.
#include "framework.h"
#include "shim/Arduino.h"
#include "../i18n_ext.h"
#include "../battle.h"
#include "../dex.h"
#include <string.h>

TEST(i18n_ext, ninguna_cadena_vacia_en_en_y_ko) {
  Lang antes = gLang;
  for (Lang l : { LANG_EN, LANG_KO, LANG_ES, LANG_JA }) {
    gLang = l;
    for (int i = 0; i < X_COUNT; i++) {
      CHECK(XT((XId)i) != nullptr);
      CHECK(strlen(XT((XId)i)) > 0);
    }
    for (int t = 0; t < PT_COUNT; t++) CHECK(strlen(moveName(BA_TYPE, t)) > 0);
  }
  gLang = antes;
}

TEST(i18n_ext, particulas_coreanas) {
  char b[64];
  txFmtRaw(b, sizeof(b), "{1}{은} 쓰러졌다!", "리자몽", nullptr);   // 몽: batchim
  CHECK(strcmp(b, "리자몽은 쓰러졌다!") == 0);
  txFmtRaw(b, sizeof(b), "{1}{은} 쓰러졌다!", "피카츄", nullptr);   // 츄: vocal
  CHECK(strcmp(b, "피카츄는 쓰러졌다!") == 0);
  txFmtRaw(b, sizeof(b), "앗! 야생 {1}{이} 나타났다!", "꼬부기", nullptr);
  CHECK(strcmp(b, "앗! 야생 꼬부기가 나타났다!") == 0);
  txFmtRaw(b, sizeof(b), "{1}{을}", "이상해씨", nullptr);
  CHECK(strcmp(b, "이상해씨를") == 0);
  txFmtRaw(b, sizeof(b), "{1}{와}", "잠만보", nullptr);
  CHECK(strcmp(b, "잠만보와") == 0);
  txFmtRaw(b, sizeof(b), "{1}{와}", "뮤츠", nullptr);
  CHECK(strcmp(b, "뮤츠와") == 0);
  txFmtRaw(b, sizeof(b), "{1}{와}", "망나뇽", nullptr);
  CHECK(strcmp(b, "망나뇽과") == 0);
  // apodo latino: forma vocal
  txFmtRaw(b, sizeof(b), "{1}{은}", "TORTU", nullptr);
  CHECK(strcmp(b, "TORTU는") == 0);
  txFmtRaw(b, sizeof(b), "{1}의 {2}!", "리자몽", "불꽃세례");
  CHECK(strcmp(b, "리자몽의 불꽃세례!") == 0);
}

TEST(i18n_ext, no_corta_utf8_a_medias) {
  char b[8];  // cabe "리자" (6 bytes) + 0, no el tercer caracter
  txFmtRaw(b, sizeof(b), "{1}", "리자몽", nullptr);
  CHECK(strcmp(b, "리자") == 0);
  txFmtRaw(b, 1, "{1}", "abc", nullptr);
  CHECK_EQ(b[0], (char)0);
  txFmtRaw(b, sizeof(b), "{1} {2}", nullptr, nullptr);
  CHECK(strcmp(b, " ") == 0);
}
