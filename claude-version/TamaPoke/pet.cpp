#include "pet.h"
#include "dex.h"
#include "audio.h"
#include "battle.h"
#include <string.h>

static uint8_t clampGene(uint8_t g) { return g < 90 ? 90 : (g > 110 ? 110 : g); }

void Pet::begin() {
  prefs.begin("tamapoke", false);
  if (!prefs.getBool("init", false)) {
    prefs.putBool("init", true);
    newEgg();
  } else {
    load();
  }
  lastTick = millis();
}

void Pet::newEgg() {
  ceremony = CER_NONE;
  neglectTicks = 0;
  weight = 0;
  speciesId = -1;
  prevSpeciesId = -1;
  eggTarget = pickEggSpecies();  // especie oculta segun rareza y pokedex
  starterPick = (registeredCount() == 0);  // primera partida: el jugador elige inicial
  // sorteo shiny: 1/48 base, mejor con despedida y con racha/vinculo altos
  int shinyBase = (lastEnd == CER_FAREWELL ? 24 : 48) - careBonus();
  if (shinyBase < 8) shinyBase = 8;
  eggShiny = (random(shinyBase) == 0);
  eggTaps = 0;
  fullness = 80;
  joy = 80;
  energy = 80;
  hygiene = 100;
  poops = 0;
  ageMinutes = 0;
  exp = 0;
  careMistakes = 0;
  mistakeCooldown = 0;
  sleeping = false;
  save();
}

// progresion offline: el tiempo paso aunque estuviera apagado, pero con
// piedad — las barras bajan con suelo en 15 (vuelve hambriento, no muerto),
// sin descuidos ni escapadas en ausencia
static uint8_t dropTo(uint8_t v, uint8_t d, uint8_t fl) {
  if (v <= fl) return v;
  return (v - fl > d) ? v - d : fl;
}

void Pet::setClock(uint32_t nowEpoch) {
  lastSeenEpoch = nowEpoch;
  if (nowEpoch) save();  // persiste ya: un corte de luz no pierde la referencia
}

void Pet::syncClock(uint32_t nowEpoch) {
  uint32_t seen = prefs.getUInt("seen", 0);
  lastSeenEpoch = nowEpoch;
  if (nowEpoch == 0) return;
  uint32_t mins = (seen && nowEpoch > seen) ? (nowEpoch - seen) / 60 : 0;
  if (mins < 2 || ceremony != CER_NONE || starterPick) {
    save();  // primera vez, sin tiempo que aplicar o aun eligiendo inicial
    return;
  }
  if (mins > 14UL * 24 * 60) mins = 14UL * 24 * 60;  // tope: 2 semanas

  for (uint32_t i = 0; i < mins; i++) {
    ageMinutes++;
    if (isEgg()) {
      if (ageMinutes >= 3) hatch();  // eclosiona en tu ausencia
      continue;
    }
    if (sleeping) {  // descanso: baja lento y con suelo, igual que en vivo
      energy = clamp100(energy + 6);
      if (ageMinutes % 2 == 0) {
        fullness = dropTo(fullness, 1, 30);
        joy = dropTo(joy, 1, 35);
      }
      if (ageMinutes % 3 == 0) hygiene = dropTo(hygiene, 1, 45);
      continue;
    }
    fullness = dropTo(fullness, 2, 15);
    energy = dropTo(energy, 1, 15);
    hygiene = dropTo(hygiene, 1, 15);
    joy = dropTo(joy, 1, 15);
  }
  if (!isEgg()) {
    if (!sleeping) {  // durmiendo no ensucia
      uint8_t p = poops + mins / 240;
      poops = p > 3 ? 3 : p;
    }
    // la evolucion NO se aplica offline: queda lista y la dispara el usuario
    // tocando al bicho cuando vuelve (para que vea la transformacion)
  }
  Serial.printf("offline: %u min aplicados (nv.%u)\n", mins, level());
  save();
}

void Pet::update(uint32_t nowMs) {
  // fin de ceremonia: la criatura se va y queda un huevo nuevo. fork KO (ko4):
  // tras una despedida (ciclo completo) el siguiente sale de la caja si hay
  if (ceremony != CER_NONE && !timeLeft(ceremonyUntil)) {
    bool fromBox = ceremony == CER_FAREWELL && nextPetHook && nextPetHook(*this);
    if (!fromBox) newEgg();
    return;
  }
  while (nowMs - lastTick >= PET_TICK_MS) {
    lastTick += PET_TICK_MS;
    tick();
  }
}

void Pet::tick() {
  if (ceremony != CER_NONE) return;  // el tiempo se detiene en la despedida
  if (starterPick) return;  // la partida no empieza hasta elegir inicial: si el
                            // tiempo corriera aqui, el huevo eclosionaria solo a
                            // los 3 min con la especie sorteada y se perderia la
                            // eleccion del jugador
  ageMinutes++;

  if (isEgg()) {
    if (ageMinutes >= 3) hatch();  // si no lo tocas, eclosiona solo a los 3 min
    return;
  }

  // el sueño es descanso: la energia se recupera y las necesidades bajan MUCHO
  // mas lento que despierto y con suelo (amanece pidiendo algo de mimo, no a
  // cero, sin descuidos ni escapadas). despierto: comida -2/min, hig/joy -1/min.
  // El peso aun se quema; la racha de buen cuidado (goodTicks) queda en pausa.
  if (sleeping) {
    energy = clamp100(energy + 6);
    if (weight > 0 && ageMinutes % 3 == 0) weight--;
    if (ageMinutes % 2 == 0) {                 // ~4x mas lento que despierto
      fullness = dropTo(fullness, 1, 30);
      joy = dropTo(joy, 1, 35);
    }
    if (ageMinutes % 3 == 0) hygiene = dropTo(hygiene, 1, 45);
    careTick();  // ko9: dormir tambien es tiempo de crianza
    checkMedals();
    pendingSave = true;  // fork KO (ko4): se guarda cada minuto
    return;
  }

  careTick();  // ko9: EXP por tiempo de crianza

  fullness = clamp100(fullness - 2);
  energy = clamp100(energy - 1);
  if (fullness > 40 && poops < 3 && random(100) < 15) poops++;

  hygiene = clamp100(hygiene - 1 - 4 * poops);
  // el sobrepeso da pereza: la energia cae el doble
  if (weight > 50) energy = clamp100(energy - 1);
  if (weight > 0 && ageMinutes % 3 == 0) weight--;

  // la disciplina forja la defensa: 12 h seguidas bien cuidado = +1 DEF
  if (lowestStat() >= 40) {
    if (++goodTicks >= 720) {
      goodTicks = 0;
      if (trDef < 100) trDef++;
    }
  } else {
    goodTicks = 0;
  }

  int dJoy = -1;
  if (fullness < 30) dJoy -= 2;
  if (hygiene < 30) dJoy -= 2;
  joy = clamp100(joy + dJoy);

  // Descuido: dejar una estadistica por los suelos cuenta como error de
  // cuidado (con enfriamiento para no contar el mismo descuido cada minuto)
  if (mistakeCooldown > 0) mistakeCooldown--;
  if (lowestStat() <= 10 && mistakeCooldown == 0) {
    careMistakes++;
    mistakeCooldown = 60;
    if (bond > 1) bond--;  // el descuido enfria el vinculo, pero sin arrasarlo:
                           // a -3 cada 30 min se perdia mucho mas de lo que se
                           // podia ganar en todo un dia y el vinculo se atascaba
  }

  checkMedals();  // la evolucion la dispara el usuario (canEvolveNow + tap), no el tick

  // abandono total: con TODO a cero durante una hora queda lista para escaparse;
  // NO se va sola, la dispara el usuario con el boton (final triste, lo presencia)
  if (fullness == 0 && joy == 0 && energy == 0 && hygiene == 0) {
    if (neglectTicks < RUNAWAY_TICKS) neglectTicks++;
  } else {
    neglectTicks = 0;  // un solo cuidado la salva
  }

  // ciclo completo (forma final + 7 dias): la despedida NO salta sola; queda
  // lista (canFarewellNow) y la dispara el usuario con el boton, para que la vea

  // autoguardado: NO escribir a flash aqui (corre dentro del loop); solo
  // marcar. fork KO (ko4): cada minuto, y el loop lo vuelca en el acto
  pendingSave = true;
}

// vuelca el guardado periodico pendiente. fork KO (ko4): cada minuto, solo las
// claves que cambia el paso del tiempo (~12 de las ~50): el guardado completo ya
// lo hace cada accion. Peor caso de desgaste, suponiendo que la NVS reescribiera
// todas: ~17k entradas/dia en 5 paginas de 126 -> ~27 borrados/pagina/dia, unos
// 10 anos para los 100k ciclos de la flash.
void Pet::flushSave() {
  if (!pendingSave) return;
  pendingSave = false;
  ticksSinceSave = 0;
  prefs.putUChar("full", fullness);
  prefs.putUChar("joy", joy);
  prefs.putUChar("ene", energy);
  prefs.putUChar("hyg", hygiene);
  prefs.putUChar("poop", poops);
  prefs.putUChar("wgt", weight);
  prefs.putUChar("tdef", trDef);
  prefs.putUInt("age", ageMinutes);
  prefs.putUInt("exp", exp);
  prefs.putUChar("mist", careMistakes);
  prefs.putBool("sleep", sleeping);
  prefs.putUChar("bond", bond);
  if (lastSeenEpoch) prefs.putUInt("seen", lastSeenEpoch);
}

// quedan miembros sin registrar en la linea evolutiva de esta base?
bool Pet::lineHasUnregistered(int16_t base) const {
  int16_t cur = base;
  for (int guard = 0; cur >= 1 && cur <= 151 && guard < 6; guard++) {
    if (!isRegistered(cur)) return true;
    if (cur == DEX_EEVEE) {
      for (int16_t b = 134; b <= 136; b++)
        if (!isRegistered(b)) return true;
      return false;
    }
    cur = DEX_TBL[cur].evolvesTo;
  }
  return false;
}

uint8_t Pet::eggRarity() const {
  return (eggTarget >= 1 && eggTarget <= 151) ? DEX_TBL[eggTarget].rarity : R_COMUN;
}

// elige la especie del huevo: tirada de rareza (mejorada por una despedida
// completa, castigada por una escapada) y sesgo hacia lineas incompletas
int16_t Pet::pickEggSpecies() {
  // primera partida: inicial clasico
  if (registeredCount() == 0) {
    return CLASSIC_DEX[random(NUM_CLASSIC_DEX)];
  }

  uint8_t tier = R_COMUN;
  if (lastEnd != CER_RUNAWAY) {
    bool blessed = (lastEnd == CER_FAREWELL);
    int rare = (blessed ? 45 : 27) + careBonus();
    int leg = (registeredCount() >= 25) ? (blessed ? 10 : 3) + careBonus() / 3 : 0;
    int r = random(100);
    if (r < leg) tier = R_LEGENDARIO;
    else if (r < leg + rare) tier = R_RARO;
  }

  // candidatos del tier con linea incompleta; si no hay, baja de tier;
  // si la pokedex del tier esta completa, vale cualquiera del tier
  for (int pass = 0; pass < 2; pass++) {
    for (int t = tier; t >= R_COMUN; t--) {
      int16_t cand[80];
      int n = 0;
      for (int16_t d = 1; d <= 151 && n < 80; d++) {
        if (DEX_TBL[d].rarity != t) continue;
        if (pass == 0 && !lineHasUnregistered(d)) continue;
        cand[n++] = d;
      }
      if (n > 0) return cand[random(n)];
    }
  }
  return CLASSIC_DEX[random(NUM_CLASSIC_DEX)];  // inalcanzable, por si acaso
}

void Pet::registerSpecies(int16_t dex) {
  if (dex < 1 || dex > 151) return;
  dexReg[(dex - 1) >> 3] |= (1 << ((dex - 1) & 7));
  if (shiny) dexShinyReg[(dex - 1) >> 3] |= (1 << ((dex - 1) & 7));
}

// la racha y el vinculo mejoran el sorteo del huevo (0..~14)
int Pet::careBonus() const {
  int s = streak > 30 ? 30 : streak;
  return s / 3 + bond / 25;
}

// primer cuidado del dia: avanza la racha y afianza el vinculo
void Pet::registerCare() {
  if (isEgg() || ceremony != CER_NONE) return;
  uint32_t d = today();
  if (d == 0 || d == lastCareDay) return;  // sin reloj, o ya conto hoy
  if (lastCareDay == 0 || d == lastCareDay + 1) {
    streak++;
  } else {
    streak = 1;        // hubo un hueco de dias
    lastMilestone = 0;
  }
  lastCareDay = d;
  bondToday = 0;
  if (streak > bestStreak) bestStreak = streak;
  bond = clamp100(bond + 4);
  uint16_t ms = (streak >= 100) ? 100 : (streak >= 30) ? 30
              : (streak >= 7)   ? 7   : (streak >= 3)  ? 3 : 0;
  if (ms > lastMilestone) {
    lastMilestone = ms;
    milestoneUntil = millis() + 4500;
  }
  checkMedals();
  save();
}

void Pet::addBond(uint8_t amt) {
  if (bondToday >= 20) return;  // tope diario: el vinculo no se farmea
  bond = clamp100(bond + amt);
  bondToday += amt;
}

void Pet::checkMedals() {
  if (isEgg()) return;
  uint16_t before = medals;
  if (level() >= 10) medals |= MED_LV10;
  if (level() >= 25) medals |= MED_LV25;
  if (level() >= 50) medals |= MED_LV50;
  if (berryKnown) medals |= MED_BERRY;
  if (streak >= 7) medals |= MED_STREAK7;
  if (bond >= 100) medals |= MED_BOND;
  if (DEX_TBL[speciesId].evolvesTo == 0) medals |= MED_FINAL;
  if (weight == 0 && level() >= 5 && careMistakes == 0) medals |= MED_FIT;
  uint16_t gained = medals & ~before;
  if (gained) {
    for (uint16_t m = gained; m; m &= (m - 1)) totalMedals++;
    newMedal = gained;
    medalUntil = millis() + 4000;
    if (!sleeping) sfxPlay(SFX_MEDAL);
    save();
  }
}

void Pet::rename(const char *name) {
  // ko8: hasta 18 bytes (6 silabas hangul) sin partir un caracter UTF-8
  size_t n = strlen(name);
  if (n > 18) {
    n = 18;
    while (n && ((uint8_t)name[n] & 0xC0) == 0x80) n--;
  }
  memcpy(nick, name, n);
  nick[n] = 0;
  save();
}

static uint16_t calcStat(uint8_t base, uint8_t gene, uint16_t lvl, uint8_t tr) {
  return (uint16_t)base * gene / 100 + lvl + tr;
}

uint16_t Pet::atkStat() const {
  return isEgg() ? 0 : calcStat(DEX_TBL[speciesId].bAtk, geneAtk, level(), trAtk);
}
uint16_t Pet::defStat() const {
  return isEgg() ? 0 : calcStat(DEX_TBL[speciesId].bDef, geneDef, level(), trDef);
}
uint16_t Pet::speStat() const {
  return isEgg() ? 0 : calcStat(DEX_TBL[speciesId].bSpe, geneSpe, level(), trSpe);
}

uint16_t Pet::registeredCount() const {
  uint16_t n = 0;
  for (int i = 1; i <= 151; i++)
    if (isRegistered(i)) n++;
  return n;
}

// forma final que ya cumplio su ciclo (7 dias): lista para despedirse. La
// despedida la dispara el usuario con el boton (no salta sola, para que la vea)
bool Pet::canFarewellNow() const {
  return !isEgg() && !sleeping && ceremony == CER_NONE &&
         DEX_TBL[speciesId].evolvesTo == 0 && ageMinutes >= FAREWELL_AGE_MIN;
}

// abandono total durante 1h: lista para escaparse. La dispara el usuario con el
// boton (final triste); cuidarla un solo tick la salva (neglectTicks se resetea)
bool Pet::canRunawayNow() const {
  return !isEgg() && !sleeping && ceremony == CER_NONE && neglectTicks >= RUNAWAY_TICKS;
}

void Pet::startFarewell() {
  if (isEgg() || ceremony != CER_NONE) return;
  lastEnd = CER_FAREWELL;
  ceremony = CER_FAREWELL;
  ceremonyUntil = millis() + CEREMONY_MS;
  heartUntil = ceremonyUntil;  // corazones durante toda la despedida
  sfxPlay(SFX_BYE);
  save();
}

void Pet::startRunaway() {
  if (isEgg() || ceremony != CER_NONE) return;
  lastEnd = CER_RUNAWAY;
  ceremony = CER_RUNAWAY;
  ceremonyUntil = millis() + CEREMONY_MS;
  sfxPlay(SFX_BYE);
  save();
}

void Pet::release() {
  if (isEgg() || ceremony != CER_NONE) return;
  lastEnd = CER_RELEASE;
  ceremony = CER_RELEASE;
  ceremonyUntil = millis() + CEREMONY_MS;
  heartUntil = ceremonyUntil;
  sfxPlay(SFX_BYE);
  save();
}

void Pet::hatch() {
  speciesId = eggTarget;
  shiny = eggShiny;
  // genes del individuo: 90-110% por stat (cada crianza es unica)
  geneAtk = 90 + random(21);
  geneDef = 90 + random(21);
  geneSpe = 90 + random(21);
  trAtk = trDef = trSpe = 0;
  berryKnown = false;
  bond = 0;          // vinculo, medallas y nombre son del individuo
  bondToday = 0;
  medals = 0;
  newMedal = 0;
  nick[0] = 0;
  registerSpecies(speciesId);  // criado = registrado en la pokedex
  checkMedals();     // por si nace ya en forma final (legendario)
  sfxPlay(SFX_HATCH);
  save();
}

// ¿se dan ya las condiciones para evolucionar? Cada descuido retrasa la
// evolucion 1 nivel, y ademas tiene que estar bien cuidado en ese momento
// (ninguna estadistica por debajo de 40). NO evoluciona sola: la dispara el
// usuario tocando al bicho (evolve()), para que vea la transformacion.
uint16_t Pet::evolveNeed() const {
  if (isEgg()) return 0;
  uint16_t lv = evoLevel(speciesId);
  if (!lv) return 0;
  lv += careMistakes;
  return lv > LEVEL_MAX ? LEVEL_MAX : lv;  // al tope sigue siendo alcanzable
}

bool Pet::canEvolveNow() const {
  if (isEgg() || sleeping || ceremony != CER_NONE) return false;
  uint16_t need = evolveNeed();
  return need && level() >= need && lowestStat() >= 40;
}

// ko9: cada minuto de crianza (despierto o dormido) da la parte de EXP que
// toca para que subir de L a L+1 cueste 30*L minutos. Con alguna barra en el
// suelo (<= 10, descuido) ese minuto no cuenta.
void Pet::careTick() {
  uint16_t L = level();
  uint32_t per = careMinutesForLevel(L);
  if (!per || isEgg() || lowestStat() <= 10) return;
  careAcc += expForLevel(L + 1) - expForLevel(L);
  if (careAcc >= per) {
    uint32_t q = careAcc / per;
    careAcc %= per;
    addExp(q);
  }
}

uint32_t Pet::careMinutesLeft() const {
  uint16_t L = level();
  uint32_t per = careMinutesForLevel(L);
  if (!per) return 0;
  uint32_t span = expForLevel(L + 1) - expForLevel(L);
  uint32_t into = exp - expForLevel(L);
  uint64_t need = (uint64_t)(span - into) * per;  // en unidades de careAcc
  need = need > careAcc ? need - careAcc : 0;
  return (uint32_t)((need + span - 1) / span);
}

uint16_t Pet::addExp(uint32_t x) {
  if (isEgg() || !x) return 0;
  uint16_t before = level();
  uint32_t top = expForLevel(LEVEL_MAX);
  exp = (exp >= top || x >= top - exp) ? top : exp + x;
  uint16_t up = level() - before;
  if (up) {
    if (!sleeping) sfxPlay(SFX_LEVEL);
    checkMedals();
    pendingSave = true;
  }
  return up;
}

void Pet::evolve() {
  if (!canEvolveNow()) return;
  const DexEntry &d = DEX_TBL[speciesId];
  prevSpeciesId = speciesId;
  int16_t next = d.evolvesTo;
  if (speciesId == DEX_EEVEE) {
    // rama de Eevee: prefiere la evolucion que falte en la pokedex
    int16_t opts[3];
    int n = 0;
    for (int16_t b = 134; b <= 136; b++)
      if (!isRegistered(b)) opts[n++] = b;
    next = n > 0 ? opts[random(n)] : (int16_t)(134 + random(3));
  }
  speciesId = next;
  registerSpecies(speciesId);
  sfxPlay(SFX_EVOLVE);
  evolveUntil = millis() + EVOLVE_ANIM_MS;
  save();
}

void Pet::feed() {
  feedBerry(0);
}

void Pet::feedBerry(uint8_t color) {
  if (ceremony != CER_NONE) return;
  if (isEgg() || sleeping) return;
  if (lovesBerry(color)) {
    fullness = clamp100(fullness + 35);
    joy = clamp100(joy + 10);
    heartUntil = millis() + HEART_MS;  // "le encanta!"
    berryKnown = true;                 // descubierto: se muestra en la ficha
    addBond(2);
  } else {
    fullness = clamp100(fullness + 25);
  }
  eatUntil = millis() + EAT_ANIM_MS;
  registerCare();
  save();
}

// forma base de la linea evolutiva (Eevee para 134-136)
static int16_t lineBase(int16_t dex) {
  for (int guard = 0; guard < 4; guard++) {
    int16_t prev = 0;
    for (int16_t d = 1; d <= 151 && !prev; d++)
      if (DEX_TBL[d].evolvesTo == dex || (d == DEX_EEVEE && dex >= 134 && dex <= 136)) prev = d;
    if (!prev) break;
    dex = prev;
  }
  return dex;
}

uint8_t Pet::favFood() const {
  if (speciesId < 1 || speciesId > 151) return 0;
  return (uint8_t)(lineBase(speciesId) % 4);
}

void Pet::feedCandy() {
  if (ceremony != CER_NONE) return;
  if (isEgg() || sleeping) return;
  if (lovesBerry(3)) {  // ko9: la chuche es su favorita: llena como la baya favorita
    fullness = clamp100(fullness + 35);
    joy = clamp100(joy + 12);
    weight = clamp100(weight + 6);
    heartUntil = millis() + HEART_MS;
    berryKnown = true;
    addBond(2);
    eatUntil = millis() + EAT_ANIM_MS;
    registerCare();
    save();
    return;
  }
  fullness = clamp100(fullness + 10);
  joy = clamp100(joy + 12);
  weight = clamp100(weight + 12);  // las chuches pasan factura
  eatUntil = millis() + EAT_ANIM_MS;
  registerCare();
  save();
}

void Pet::playResult(uint8_t score) {
  if (ceremony != CER_NONE || isEgg()) return;
  // fork KO (ko4): la velocidad ya se entrena con su juego propio (trainSpeed);
  // la pelota queda como juego de animo
  joy = clamp100(joy + 5 + (score > 15 ? 30 : score * 2));
  energy = dropTo(energy, 10 + score / 2, 5);
  fullness = dropTo(fullness, 5, 5);
  int burn = (int)weight - score * 2;  // el ejercicio quema peso
  weight = burn > 0 ? burn : 0;
  if (score >= 5) heartUntil = millis() + HEART_MS;
  if (score > gameHi) gameHi = score;  // nuevo record
  addBond(2);
  registerCare();
  save();
}

// saco de entrenamiento: los golpes entrenan la fuerza. Devuelve la subida.
uint8_t Pet::trainStrength(uint16_t hits) {
  if (ceremony != CER_NONE || isEgg()) return 0;
  uint8_t gain = hits / 4;          // ~4 golpes = 1 punto de entrenamiento
  if (gain > 18) gain = 18;         // tope por sesion: la FUE se forja a fuego lento
  uint8_t antes = trAtk;
  uint16_t v = (uint16_t)trAtk + gain;
  trAtk = v > 100 ? 100 : (uint8_t)v;
  gain = trAtk - antes;             // lo que de verdad entro: al topar en 100 la
                                    // pantalla anunciaba +18 aunque cupieran menos
  energy = dropTo(energy, 12, 5);   // cansa
  fullness = dropTo(fullness, 5, 5);
  int burn = (int)weight - hits / 3;  // tambien quema peso
  weight = burn > 0 ? burn : 0;
  joy = clamp100(joy + 6);
  if (hits >= 20) heartUntil = millis() + HEART_MS;
  if (hits > strHi) strHi = hits;   // record de golpes
  addBond(2);
  registerCare();
  save();
  return gain;
}

// fork KO (ko4): comun a los entrenamientos: sube la stat con tope por sesion
// y cansa igual que el saco. Devuelve lo que de verdad subio.
static uint8_t trainGain(uint8_t &tr, uint16_t raw) {
  uint8_t gain = raw > 18 ? 18 : (uint8_t)raw;
  uint8_t antes = tr;
  uint16_t v = (uint16_t)tr + gain;
  tr = v > 100 ? 100 : (uint8_t)v;
  return tr - antes;
}

uint8_t Pet::trainDefense(uint16_t blocked) {
  if (ceremony != CER_NONE || isEgg()) return 0;
  uint8_t gain = trainGain(trDef, blocked / 2);  // ~2 pokeballs paradas = 1 punto
  energy = dropTo(energy, 12, 5);
  fullness = dropTo(fullness, 5, 5);
  int burn = (int)weight - blocked / 2;
  weight = burn > 0 ? burn : 0;
  joy = clamp100(joy + 6);
  if (blocked >= 15) heartUntil = millis() + HEART_MS;
  if (blocked > defHi) defHi = blocked;
  addBond(2);
  registerCare();
  save();
  return gain;
}

uint8_t Pet::trainSpeed(uint16_t hits) {
  if (ceremony != CER_NONE || isEgg()) return 0;
  uint8_t gain = trainGain(trSpe, hits);  // 1 acierto = 1 punto (15 rondas)
  energy = dropTo(energy, 12, 5);
  fullness = dropTo(fullness, 5, 5);
  int burn = (int)weight - hits;
  weight = burn > 0 ? burn : 0;
  joy = clamp100(joy + 6);
  if (hits >= 10) heartUntil = millis() + HEART_MS;
  if (hits > speHi) speHi = hits;
  addBond(2);
  registerCare();
  save();
  return gain;
}

void Pet::play() {
  if (ceremony != CER_NONE) return;
  if (isEgg() || sleeping) return;
  joy = clamp100(joy + 25);
  energy = clamp100(energy - 10);
  fullness = clamp100(fullness - 5);
  heartUntil = millis() + HEART_MS;
  addBond(2);
  registerCare();
  save();
}

void Pet::toggleLight() {
  if (ceremony != CER_NONE) return;
  if (isEgg()) return;
  sleeping = !sleeping;
  save();
}

void Pet::clean() {
  if (ceremony != CER_NONE) return;
  poops = 0;
  hygiene = 100;
  addBond(1);
  registerCare();
  save();
}

void Pet::caress() {
  if (ceremony != CER_NONE) return;
  if (isEgg() || sleeping) return;
  joy = clamp100(joy + 5);
  heartUntil = millis() + HEART_MS;
  addBond(1);
  registerCare();
}

void Pet::eggTap() {
  if (!isEgg()) return;
  if (++eggTaps >= 3) hatch();
  else save();
}

PetMood Pet::mood() const {
  if (sleeping) return MOOD_SLEEPING;
  if (eating()) return MOOD_EATING;
  if (lowestStat() < 25) return MOOD_SAD;
  return MOOD_HAPPY;
}

void Pet::save() {
  ticksSinceSave = 0;
  pendingSave = false;
  prefs.putUChar("full", fullness);
  prefs.putUChar("joy", joy);
  prefs.putUChar("ene", energy);
  prefs.putUChar("hyg", hygiene);
  prefs.putUChar("poop", poops);
  prefs.putUChar("wgt", weight);
  prefs.putUChar("gatk", geneAtk);
  prefs.putUChar("gdef", geneDef);
  prefs.putUChar("gspe", geneSpe);
  prefs.putUChar("tatk", trAtk);
  prefs.putUChar("tdef", trDef);
  prefs.putUChar("tspe", trSpe);
  prefs.putBool("bk", berryKnown);
  prefs.putBool("shy", shiny);
  prefs.putBool("eshy", eggShiny);
  prefs.putBool("stpk", starterPick);
  prefs.putBytes("dexsh", dexShinyReg, sizeof(dexShinyReg));
  prefs.putUInt("age", ageMinutes);
  prefs.putUInt("exp", exp);
  prefs.putShort("dexn", speciesId);
  prefs.putShort("eggT2", eggTarget);
  prefs.putUChar("crack", eggTaps);
  prefs.putUChar("mist", careMistakes);
  prefs.putBool("sleep", sleeping);
  prefs.putUChar("lend", lastEnd);
  if (lastSeenEpoch) prefs.putUInt("seen", lastSeenEpoch);
  prefs.putBytes("dexreg", dexReg, sizeof(dexReg));
  prefs.putUShort("strk", streak);
  prefs.putUShort("bstrk", bestStreak);
  prefs.putUInt("cday", lastCareDay);
  prefs.putUChar("bond", bond);
  prefs.putUShort("medal", medals);
  prefs.putUShort("tmedal", totalMedals);
  prefs.putUShort("mstone", lastMilestone);
  prefs.putUShort("ghi", gameHi);
  prefs.putUShort("shi", strHi);
  prefs.putString("nick", nick);
  prefs.putUShort("wwin", wildWins);
  prefs.putUShort("lwin", linkWins);
  prefs.putUShort("lbat", linkBattles);
  prefs.putUShort("trd", trades);
  prefs.putUChar("balls", balls);
  prefs.putUChar("potn", potions);
  prefs.putUShort("dhi", defHi);
  prefs.putUShort("vhi", speHi);
}

void Pet::load() {
  fullness = prefs.getUChar("full", 80);
  joy = prefs.getUChar("joy", 80);
  energy = prefs.getUChar("ene", 80);
  hygiene = prefs.getUChar("hyg", 100);
  poops = prefs.getUChar("poop", 0);
  weight = prefs.getUChar("wgt", 0);
  geneAtk = prefs.getUChar("gatk", 0);
  geneDef = prefs.getUChar("gdef", 0);
  geneSpe = prefs.getUChar("gspe", 0);
  if (geneAtk == 0) {  // mascota anterior a los genes: tirada unica ahora
    geneAtk = 90 + random(21);
    geneDef = 90 + random(21);
    geneSpe = 90 + random(21);
  }
  trAtk = prefs.getUChar("tatk", 0);
  trDef = prefs.getUChar("tdef", 0);
  trSpe = prefs.getUChar("tspe", 0);
  berryKnown = prefs.getBool("bk", false);
  shiny = prefs.getBool("shy", false);
  eggShiny = prefs.getBool("eshy", false);
  starterPick = prefs.getBool("stpk", false);
  prefs.getBytes("dexsh", dexShinyReg, sizeof(dexShinyReg));
  ageMinutes = prefs.getUInt("age", 0);
  // fork KO (ko7): guardados de antes (nivel = horas, hasta Lv338+) empiezan
  // en Lv1 con la misma especie
  exp = prefs.getUInt("exp", 0);
  if (prefs.isKey("dexn")) {
    speciesId = prefs.getShort("dexn", -1);
    eggTarget = prefs.getShort("eggT2", 4);
  } else {
    // migracion desde la version con indices de flash (0-8)
    static const uint8_t OLD2DEX[9] = { 4, 5, 6, 1, 2, 3, 7, 8, 9 };
    int8_t old = prefs.getChar("spec", -1);
    speciesId = (old >= 0 && old < 9) ? OLD2DEX[old] : -1;
    int8_t oldT = prefs.getChar("eggT", 0);
    eggTarget = (oldT >= 0 && oldT < 9) ? OLD2DEX[oldT] : 4;
  }
  eggTaps = prefs.getUChar("crack", 0);
  careMistakes = prefs.getUChar("mist", 0);
  sleeping = prefs.getBool("sleep", false);
  lastEnd = prefs.getUChar("lend", CER_NONE);
  prefs.getBytes("dexreg", dexReg, sizeof(dexReg));
  streak = prefs.getUShort("strk", 0);
  bestStreak = prefs.getUShort("bstrk", 0);
  lastCareDay = prefs.getUInt("cday", 0);
  bond = prefs.getUChar("bond", 0);
  medals = prefs.getUShort("medal", 0);
  totalMedals = prefs.getUShort("tmedal", 0);
  lastMilestone = prefs.getUShort("mstone", 0);
  gameHi = prefs.getUShort("ghi", 0);
  strHi = prefs.getUShort("shi", 0);
  prefs.getString("nick", nick, sizeof(nick));
  wildWins = prefs.getUShort("wwin", 0);
  linkWins = prefs.getUShort("lwin", 0);
  linkBattles = prefs.getUShort("lbat", 0);
  trades = prefs.getUShort("trd", 0);
  balls = prefs.getUChar("balls", 5);   // partidas anteriores a ko4: kit inicial
  potions = prefs.getUChar("potn", 2);
  defHi = prefs.getUShort("dhi", 0);
  speHi = prefs.getUShort("vhi", 0);
  // siembra: la mascota actual cuenta como criada (guardados antiguos)
  if (speciesId >= 1) registerSpecies(speciesId);
}

void Pet::wipeGameKeepSettings() {
  static const char *const KEEP_U8[] = { "volBgm", "volCry", "volSfx", "lang" };
  uint8_t u8[4];
  bool has[4];
  for (int i = 0; i < 4; i++) {
    has[i] = prefs.isKey(KEEP_U8[i]);
    u8[i] = prefs.getUChar(KEEP_U8[i], 0);
  }
  bool hasSnd = prefs.isKey("snd"), snd = prefs.getBool("snd", true);
  uint32_t seen = prefs.getUInt("seen", 0);
  prefs.clear();
  for (int i = 0; i < 4; i++)
    if (has[i]) prefs.putUChar(KEEP_U8[i], u8[i]);
  if (hasSnd) prefs.putBool("snd", snd);
  if (seen) prefs.putUInt("seen", seen);
}

// ---------------------------------------------------------------- batallas

uint16_t Pet::hpStat() const {
  if (isEgg()) return 0;
  return battleHp(DEX_TBL[speciesId].bHp, level());
}

static uint8_t addCap(uint8_t v, uint8_t d) { return (v + d > 100) ? 100 : v + d; }

void Pet::battleResult(uint8_t kind, bool won, bool fled, bool caught,
                       int16_t foeDex, uint16_t foeLvl) {
  lastExpGain = 0;
  lastLvlUp = 0;
  if (isEgg() || ceremony != CER_NONE) return;
  if (kind == BATTLE_LINK) linkBattles++;
  // fork KO (ko4): huir o perder no tiene castigo (ni cansancio ni animo)
  if (fled || (!won && !caught && kind == BATTLE_WILD)) {
    save();
    return;
  }
  // pelear cansa y da hambre
  energy = dropTo(energy, kind == BATTLE_WILD ? 8 : 6, 0);
  fullness = dropTo(fullness, 4, 0);
  if (won || caught) {
    // la batalla entrena las tres stats un poco (el saco y el minijuego siguen
    // siendo la forma rapida de subir FUE y VEL)
    trAtk = addCap(trAtk, 2);
    trDef = addCap(trDef, 1);
    trSpe = addCap(trSpe, 1);
    joy = clamp100(joy + (kind == BATTLE_LINK ? 15 : 10));
    heartUntil = millis() + HEART_MS;
    addBond(kind == BATTLE_LINK ? 3 : 2);
    // fork KO (ko7): EXP del rival (en tongsin, la mitad: no se farmea)
    uint32_t gx = battleExp(foeDex, foeLvl);
    if (kind == BATTLE_LINK) gx /= 2;
    lastExpGain = gx;
    lastLvlUp = addExp(gx);
    if (kind == BATTLE_WILD && won) {
      wildWins++;
      balls = balls > 97 ? 99 : balls + 2;      // fork KO: +2 pokeballs
      potions = potions > 97 ? 99 : potions + 2;  // y +2 pociones
    } else if (kind == BATTLE_LINK) {
      linkWins++;
    }
  } else {
    // perder contra un amigo tambien divierte; contra un salvaje, desanima
    if (kind == BATTLE_LINK) joy = clamp100(joy + 5);
    else joy = clamp100(joy - 5);
    addBond(1);
  }
  registerCare();
  save();
}

bool Pet::useBall() {
  if (!balls) return false;
  balls--;
  save();
  return true;
}

bool Pet::usePotion() {
  if (!potions) return false;
  potions--;
  save();
  return true;
}

void Pet::adoptMon(int16_t dex, uint16_t lvl, bool isShiny, uint8_t gA, uint8_t gD, uint8_t gS) {
  if (dex < 1 || dex > 151) { newEgg(); return; }
  ceremony = CER_NONE;
  neglectTicks = 0;
  speciesId = dex;
  prevSpeciesId = -1;
  shiny = isShiny;
  if (lvl < 1) lvl = 1;
  if (lvl > LEVEL_MAX) lvl = LEVEL_MAX;
  exp = expForLevel(lvl);
  ageMinutes = 0;
  geneAtk = clampGene(gA);
  geneDef = clampGene(gD);
  geneSpe = clampGene(gS);
  trAtk = trDef = trSpe = 0;
  fullness = 80; joy = 80; energy = 80; hygiene = 100;
  poops = 0; weight = 0;
  careMistakes = 0; mistakeCooldown = 0;
  sleeping = false;
  berryKnown = false;
  bond = 0; bondToday = 0;
  medals = 0; newMedal = 0;
  nick[0] = 0;
  evoDeclinedLv = 0; farDeclinedAge = 0;
  goodTicks = 0;
  eggTaps = 0;
  eatUntil = 0;
  heartUntil = millis() + HEART_MS;
  registerSpecies(speciesId);  // criado = registrado en la pokedex
  checkMedals();
  sfxPlay(SFX_HATCH);
  save();
}

void Pet::exportTrade(TradePet &t) const {
  memset(&t, 0, sizeof(t));
  t.dex = speciesId;
  t.shiny = shiny ? 1 : 0;
  t.ageMinutes = ageMinutes;
  t.geneAtk = geneAtk; t.geneDef = geneDef; t.geneSpe = geneSpe;
  t.trAtk = trAtk; t.trDef = trDef; t.trSpe = trSpe;
  t.weight = weight;
  memcpy(t.nick, nick, sizeof(t.nick));
  t.nick[sizeof(t.nick) - 1] = 0;
}


bool Pet::importTrade(const TradePet &t, uint16_t lvl) {
  // lo que llega por radio no es de fiar: validar todo antes de tocar nada
  if (t.dex < 1 || t.dex > 151) return false;
  if (!canBattle()) return false;
  speciesId = t.dex;
  prevSpeciesId = -1;
  shiny = t.shiny != 0;
  ageMinutes = t.ageMinutes > 999UL * 60 ? 999UL * 60 : t.ageMinutes;
  exp = expForLevel(lvl > LEVEL_MAX ? LEVEL_MAX : (lvl ? lvl : 1));  // fork KO (ko7)
  geneAtk = clampGene(t.geneAtk);
  geneDef = clampGene(t.geneDef);
  geneSpe = clampGene(t.geneSpe);
  trAtk = t.trAtk > 100 ? 100 : t.trAtk;
  trDef = t.trDef > 100 ? 100 : t.trDef;
  trSpe = t.trSpe > 100 ? 100 : t.trSpe;
  weight = t.weight > 100 ? 100 : t.weight;
  // apodo: solo lo que escriben los teclados del juego (A-Z . - espacio y, desde
  // ko8, silabas hangul completas en UTF-8). Lo demas se descarta.
  char nk[sizeof(nick)] = "";
  uint8_t j = 0;
  for (uint8_t i = 0; i < sizeof(t.nick) && t.nick[i] && j < sizeof(nick) - 1;) {
    uint8_t c = (uint8_t)t.nick[i];
    if ((c >= 'A' && c <= 'Z') || c == '.' || c == '-' || c == ' ') {
      nk[j++] = (char)c;
      i++;
      continue;
    }
    if ((c & 0xF0) == 0xE0 && i + 2 < sizeof(t.nick) && ((uint8_t)t.nick[i + 1] & 0xC0) == 0x80 &&
        ((uint8_t)t.nick[i + 2] & 0xC0) == 0x80) {
      uint32_t cp = ((uint32_t)(c & 15) << 12) | ((uint32_t)(t.nick[i + 1] & 63) << 6) | (t.nick[i + 2] & 63);
      if (cp >= 0xAC00 && cp <= 0xD7A3 && j + 3 < sizeof(nick)) {
        memcpy(nk + j, t.nick + i, 3);
        j += 3;
      }
      i += 3;
      continue;
    }
    i++;
  }
  nk[j] = 0;
  memcpy(nick, nk, sizeof(nick));
  // lo del individuo empieza de cero con su nuevo entrenador
  berryKnown = false;
  bond = 0;
  bondToday = 0;
  medals = 0;
  newMedal = 0;
  careMistakes = 0;
  mistakeCooldown = 0;
  neglectTicks = 0;
  goodTicks = 0;
  evoDeclinedLv = 0;
  farDeclinedAge = 0;
  eatUntil = heartUntil = 0;
  trades++;
  registerSpecies(speciesId);
  // evolucion por intercambio (Kadabra, Machoke, Graveler, Haunter), como en gen 1
  if (tradeEvolves(speciesId)) {
    prevSpeciesId = speciesId;
    speciesId = DEX_TBL[speciesId].evolvesTo;
    registerSpecies(speciesId);
    sfxPlay(SFX_EVOLVE);
    evolveUntil = millis() + EVOLVE_ANIM_MS;
  } else {
    heartUntil = millis() + HEART_MS;
  }
  checkMedals();
  registerCare();
  save();
  return true;
}
