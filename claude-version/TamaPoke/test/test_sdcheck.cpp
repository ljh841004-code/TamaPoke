#include "framework.h"
#include "../sdcheck.h"
#include "../battle.h"
#include "../moves_data.h"
#include <stdio.h>
#include <string.h>

TEST(sdcheck, cuenta_y_falta) {
  SdInv v;
  char nm[24];
  for (int d = 1; d <= 251; d++) {
    snprintf(nm, sizeof(nm), "p%03d.bin", d); v.add(nm);
    snprintf(nm, sizeof(nm), "rs%03d.bin", d); v.add(nm);
    if (d != 152) { snprintf(nm, sizeof(nm), "cry%03d.wav", d); v.add(nm); }
  }
  v.add("thumbs.bin");
  v.add("bgm.wav");
  v.add("p000.bin"); v.add("p252.bin"); v.add("p01.bin"); v.add("px01.bin");  // no cuentan
  CHECK_EQ(v.have(SDC_SPR), (uint16_t)251);
  CHECK(v.complete(SDC_SPR));
  CHECK_EQ(v.have(SDC_SPRS), (uint16_t)0);
  CHECK_EQ(v.have(SDC_BATS), (uint16_t)251);
  CHECK_EQ(v.have(SDC_CRY), (uint16_t)250);
  CHECK(v.firstMissing(SDC_CRY, nm, sizeof(nm)));
  CHECK(!strcmp(nm, "cry152.wav"));
  CHECK(v.complete(SDC_THUMB));
  CHECK(!v.complete(SDC_STORY));
  CHECK_EQ(v.have(SDC_MUSIC), (uint16_t)1);
  CHECK(!v.firstMissing(SDC_SPR, nm, sizeof(nm)));
}

// los nombres de efecto que comprueba son los mismos que abre fxanim (fTTSV.bin / mNNN.bin)
TEST(sdcheck, efectos_como_el_firmware) {
  SdInv v;
  char nm[24];
  for (uint8_t id = 1; id < MOVE_N; id++) {
    uint8_t t, s, k;
    if (moveDecode(id, &t, &s, &k)) snprintf(nm, sizeof(nm), "fx/f%02u%u%u.bin", t, s, k);
    else snprintf(nm, sizeof(nm), id % 2 ? "fx/m%03u.bin" : "m%03u.bin", id);  // tambien sueltos en mons/
    if (id != 88) v.add(nm);
  }
  CHECK_EQ(SdInv::need(SDC_FX), (uint16_t)(MOVE_N - 1));
  CHECK_EQ(v.have(SDC_FX), (uint16_t)(MOVE_N - 2));
  CHECK(v.firstMissing(SDC_FX, nm, sizeof(nm)));
  uint8_t t, s, k;
  moveDecode(88, &t, &s, &k);
  char want[24];
  snprintf(want, sizeof(want), "fx/f%02u%u%u.bin", t, s, k);
  CHECK(!strcmp(nm, want));
}
