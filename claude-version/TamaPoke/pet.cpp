#include "pet.h"
#include "dex.h"
#include "audio.h"
#include "battle.h"
#include <string.h>

static_assert(PET_DEX_MAX == DEX_COUNT, "pet.h y dex.h no cuadran");

static uint8_t clampGene(uint8_t g) { return g < 90 ? 90 : (g > 110 ? 110 : g); }

void Pet::begin() {
  prefs.begin("tamapoke", false);
  if (!prefs.getBool("init", false)) {
    prefs.putBool("init", true);
    bdayM = prefs.getUChar("bdm", 0);  // ko12.6: ajuste que sobrevive a [nuevo comienzo]
    bdayD = prefs.getUChar("bdd", 0);
    newEgg();
  } else {
    bool mig = false;
    load(&mig);
    if (mig) save();  // ko11.9.2: records pasados a historicos: guardarlo ya
  }
  lastTick = millis();
}

// ---- ko12.5: barras, educar, rutina, caracter ----
static uint8_t dropTo(uint8_t v, uint8_t d, uint8_t fl);
void Pet::gaugeTap(uint8_t which) {
  if (isEgg() || ceremony != CER_NONE) return;
  uint8_t *v = which == 0 ? &fullness : which == 1 ? &joy : which == 2 ? &energy : which == 3 ? &hygiene : nullptr;
  if (!v) return;
  *v = clamp100(*v + GAUGE_TAP_GAIN);
  if (which == 3 && poops) poops--;  // limpiar de verdad: una caca menos
  pendingSave = true;
}

bool Pet::scold() {
  if (isEgg() || ceremony != CER_NONE) return false;
  if (tantrum) {  // berrinche: aprende
    tantrum = 0;
    discipline = clamp100(discipline + 20);
    joy = dropTo(joy, 5, 0);
    save();
    return true;
  }
  joy = dropTo(joy, 10, 0);  // sin motivo: no entiende por que
  discipline = dropTo(discipline, 5, 0);
  save();
  return false;
}

bool Pet::soothe() {
  if (!tantrum) return false;
  tantrum = 0;
  joy = clamp100(joy + 5);
  discipline = dropTo(discipline, 8, 0);
  save();
  return true;
}

static uint8_t hourOf(uint32_t e) { return (uint8_t)(e / 3600 % 24); }
static bool rtWindow(uint8_t what, uint8_t h) {
  return what == RT_MEAL ? (h >= 6 && h < 11) : what == RT_PLAY ? (h >= 12 && h < 19) : (h >= 20 && h < 24);
}
uint8_t Pet::routineToday() {
  uint32_t day = lastSeenEpoch / 86400;
  if (!lastSeenEpoch || day == rtDay) return rtBits;
  if (rtDay && day != rtDay + 1 && rtStreak) rtStreak = 0;           // dias sin rutina entera: se corta
  if (rtDay && day == rtDay + 1 && rtBits != (1 << RT_COUNT) - 1) rtStreak = 0;  // ayer no la completo
  rtDay = day;
  rtBits = 0;
  return 0;
}
void Pet::routineDo(uint8_t what) {
  if (isEgg() || !lastSeenEpoch || what >= RT_COUNT) return;
  routineToday();
  if (rtBits & (1 << what) || !rtWindow(what, hourOf(lastSeenEpoch))) return;
  rtBits |= (uint8_t)(1 << what);
  rtNote = (uint8_t)(1 + what);
  if (rtBits == (1 << RT_COUNT) - 1) {  // el dia entero: premio
    joy = clamp100(joy + 10);
    addBond(3);
    rtStreak++;
    if (rtStreak > rtBest) rtBest = rtStreak;
    rtNote = 9;
  }
  pendingSave = true;
}

// el caracter sale de como lo cuidas (lo que mas haces, con pesos: dar de comer es lo normal)
uint8_t personalityOf(const LifeLog &l, uint8_t discipline) {
  uint32_t total = (uint32_t)l.meals + l.snacks + l.cleans + l.pets + l.plays + l.trains;
  if (total < PERS_MIN_ACTS) return PERS_NONE;
  if (discipline >= 80) return PERS_CALM;
  struct { uint32_t v; uint8_t p; } c[5] = {
    { (uint32_t)l.snacks * 3, PERS_GLUTTON }, { (uint32_t)l.plays * 2, PERS_PLAYFUL }, { (uint32_t)l.pets, PERS_CUDDLY },
    { (uint32_t)l.trains * 5 / 2, PERS_HARDWORK }, { (uint32_t)l.cleans * 2, PERS_TIDY },
  };
  uint8_t best = 0;
  for (uint8_t i = 1; i < 5; i++) if (c[i].v > c[best].v) best = i;
  return c[best].v * 4 >= total ? c[best].p : PERS_NONE;  // nada destaca: aun no se sabe
}

// ---- ko12.4: habitacion y paseo ----
bool Pet::decoUnlocked(uint8_t item, uint16_t dexCount) const {
  switch (item) {
    case DECO_CUSHION: return true;
    case DECO_PLANT: return bestStreak >= 7 || streak >= 7;       // 7 dias seguidos cuidandolo
    case DECO_BALL: return allGameHi >= 20 || gameHi >= 20;       // pelota: 20 puntos
    case DECO_LAMP: return walk.total >= DECO_LAMP_STEPS || wildWins >= 50;
    case DECO_TROPHY: return champWins >= 1;
    case DECO_DOLL: return dexCount >= 100;
    default: return false;
  }
}

// el dia cambio: los de antes se corren una casilla por dia (7 dias)
static void walkRoll(Pet::Walk &w, uint32_t dayIdx) {
  if (w.day == dayIdx) return;
  if (!w.day || dayIdx < w.day || dayIdx - w.day >= 7) {
    memset(w.days, 0, sizeof(w.days));
  } else {
    uint32_t sh = dayIdx - w.day;
    for (int i = 6; i >= 0; i--) w.days[i] = i >= (int)sh ? w.days[i - sh] : 0;
  }
  w.day = dayIdx;
  w.rw = 0;
}

uint16_t Pet::stepsToday(uint32_t dayIdx) {
  walkRoll(walk, dayIdx);
  return walk.days[0];
}

uint8_t Pet::addSteps(uint16_t n, uint32_t dayIdx) {
  if (!n) return 0;
  walkRoll(walk, dayIdx);
  uint32_t t = (uint32_t)walk.days[0] + n;
  walk.days[0] = t > 65535 ? 65535 : (uint16_t)t;
  walk.total += n;
  uint8_t got = 0;
  static const uint16_t GOAL[3] = { WALK_GOAL1, WALK_GOAL2, WALK_GOAL3 };
  for (uint8_t k = 0; k < 3; k++)
    if (walk.days[0] >= GOAL[k] && !(walk.rw & (1 << k))) { walk.rw |= (uint8_t)(1 << k); got |= (uint8_t)(1 << k); }
  if (got & 1) { if (!isEgg()) { joy = clamp100(joy + 20); addBond(2); } }
  if (got & 2) { if (!isEgg()) addCandy(speciesId, 1); else addShards(2); }
  if (got & 4) addShards(2);
  if (got) save(); else pendingSave = true;
  return got;
}

// ---- ko12.4: diario de la crianza ----
static uint8_t popcount8(uint8_t v) { uint8_t c = 0; while (v) { c += v & 1; v >>= 1; } return c; }
static uint8_t popcount16(uint16_t v) { uint8_t c = 0; while (v) { c += v & 1; v >>= 1; } return c; }

void Pet::lifeStart(uint8_t from) {
  memset(&life, 0, sizeof(life));
  if (from != LF_UPDATE) { discipline = DISC_START; tantrum = 0; }  // ko12.5: educacion del individuo
  life.start = lastSeenEpoch;
  life.from = from;
  life.firstDex = speciesId;
  life.wins0 = wildWins;
  life.link0 = linkWins;
  life.daily0 = dailyClears;
  life.champ0 = champWins;
  life.badges0 = popcount8(badges);
}

static uint16_t lifeDiff(uint16_t now, uint16_t at) { return now >= at ? (uint16_t)(now - at) : 0; }

void Pet::lifeMemory(MemRec &m, uint32_t endEpoch) const {
  memset(&m, 0, sizeof(m));
  m.life = life;
  m.end = endEpoch;
  m.lvl = level();
  uint32_t d = ageMinutes / (24 * 60);
  m.days = d > 65535 ? 65535 : (uint16_t)d;
  m.bond = bond;
  m.mistakes = careMistakes;
  m.medals = popcount16(medals);
  uint8_t b = popcount8(badges);
  m.badges = b > life.badges0 ? (uint8_t)(b - life.badges0) : 0;
  m.wins = lifeDiff(wildWins, life.wins0);
  m.link = lifeDiff(linkWins, life.link0);
  m.daily = lifeDiff(dailyClears, life.daily0);
  m.champ = lifeDiff(champWins, life.champ0);
  m.gameHi = gameHi; m.strHi = strHi; m.defHi = defHi; m.speHi = speHi; m.vbBest = vbBest;
  memcpy(m.nick, nick, sizeof(m.nick));
  m.nick[sizeof(m.nick) - 1] = 0;
}

// ---- ko11.16: orbes de tipo ----
bool Pet::orbFits(uint16_t o) const {
  return orbValid(o) && !isEgg() && speciesId >= 1 && orbType(o) == DEX_TBL[speciesId].ptype;
}

static void orbBagPush(Pet &p, uint16_t o) {
  if (!orbValid(o)) return;
  if (p.orbN < ORB_BAG_MAX) p.orbBag[p.orbN++] = o;
}

uint8_t Pet::gainOrb(uint16_t o) {
  if (!orbValid(o)) return 0;
  int16_t candyDex = isEgg() ? -1 : speciesId;
  // uno igual (tipo y clase) equipado o en la bolsa: se queda el mejor
  uint16_t *old = orbSame(orb, o) ? &orb : nullptr;
  for (uint8_t i = 0; i < orbN && !old; i++)
    if (orbSame(orbBag[i], o)) old = &orbBag[i];
  uint8_t res;
  if (old) {
    if (orbPct(o) > orbPct(*old)) { *old = o; res = 2; }
    else res = 3;
    if (candyDex > 0) addCandy(candyDex, 1);
  } else if (orbN < ORB_BAG_MAX) {
    orbBag[orbN++] = o;
    res = 1;
  } else {
    if (candyDex > 0) addCandy(candyDex, 1);
    res = 3;
  }
  save();
  return res;
}

bool Pet::equipOrb(uint8_t i) {
  if (i >= orbN || !orbFits(orbBag[i])) return false;
  uint16_t o = orbBag[i];
  for (uint8_t k = i; k + 1 < orbN; k++) orbBag[k] = orbBag[k + 1];
  orbN--;
  if (orbValid(orb)) orbBagPush(*this, orb);
  orb = o;
  save();
  return true;
}

void Pet::unequipOrb() {
  if (!orbValid(orb)) return;
  orbBagPush(*this, orb);
  orb = 0;
  save();
}

void Pet::newEgg() {
  // ko11.16: el orbe del que se va vuelve a la bolsa
  if (orbValid(orb)) { orbBagPush(*this, orb); orb = 0; }
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
  eggCharm = shinyCharm;  // ko10.10: se recuerda para devolverlo si no llega a nacer
  if (shinyCharm) { shinyBase /= 4; shinyCharm = false; }  // ko10.4: caramelos x10
  if (shinyBase < 2) shinyBase = 2;
  eggShiny = (random(shinyBase) == 0);
  eggTaps = 0;
  memset(&life, 0, sizeof(life));  // ko12.4: se escribe al nacer
  fullness = 80;
  joy = 80;
  energy = 80;
  hygiene = 100;
  poops = 0;
  ageMinutes = 0;
  exp = 0;
  careMistakes = 0;
  mistakeCooldown = 0;
  mistWhy = MW_NONE;
  mistEpoch = 0;
  sleeping = false;
  autoSleep = false;
  sick = sickDoses = sickWait = 0;  // ko12.6
  sickMin = 0;
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

  // ko11.19: con la placa apagada, la noche (22-7 h) cuenta como dormida aunque no
  // la acostaras (antes amanecia con todo a 15), y de dia el suelo sube a 30
  uint32_t nightMins = 0, awakeMins = 0;
  for (uint32_t i = 0; i < mins; i++) {
    ageMinutes++;
    if (isEgg()) {
      if (ageMinutes >= 3) hatch();  // eclosiona en tu ausencia
      continue;
    }
    uint8_t hr = (uint8_t)(((seen + (i + 1) * 60UL) / 3600UL) % 24);
    bool night = hr >= OFF_NIGHT_FROM || hr < OFF_NIGHT_TO;
    if (night && !sleeping) nightMins++;
    if (sleeping || night) {  // descanso: baja lento y con suelo, igual que en vivo
      energy = clamp100(energy + 6);
      if (ageMinutes % 2 == 0) {
        fullness = dropTo(fullness, 1, 30);
        joy = dropTo(joy, 1, 35);
      }
      if (ageMinutes % 3 == 0) hygiene = dropTo(hygiene, 1, 45);
      continue;
    }
    awakeMins++;
    fullness = dropTo(fullness, 2, OFF_AWAKE_FLOOR);
    energy = dropTo(energy, 1, OFF_AWAKE_FLOOR);
    hygiene = dropTo(hygiene, 1, OFF_AWAKE_FLOOR);
    joy = dropTo(joy, 1, OFF_AWAKE_FLOOR);
  }
  if (!isEgg()) {
    if (!sleeping) {  // durmiendo no ensucia (ko11.19: la noche tampoco)
      uint8_t p = poops + awakeMins / 240;
      poops = p > 3 ? 3 : p;
    }
    if (nightMins >= 120) sleptOffline = 1;  // al menos 2 h de noche: "잘 잤어요!"
    // la evolucion NO se aplica offline: queda lista y la dispara el usuario
    // tocando al bicho cuando vuelve (para que vea la transformacion)
  }
  Serial.printf("offline: %u min aplicados (nv.%u)\n", (unsigned)mins, level());
  save();
}

void Pet::update(uint32_t nowMs) {
  // fin de ceremonia: la criatura se va y queda un huevo nuevo. fork KO (ko4):
  // tras una despedida (ciclo completo) el siguiente sale de la caja si hay
  if (ceremony != CER_NONE && !timeLeft(ceremonyUntil)) {
    // ko10.5: despedida o soltarlo = criado (su familia no vuelve en los huevos);
    // la escapada no cuenta. La interfaz lo guarda en la caja y deja elegir
    uint8_t how = ceremony;
    if (how != CER_RUNAWAY && !isEgg() && !(how == CER_RELEASE && shortRelease)) markFamRaised(speciesId);
    if (endHook) endHook(*this, how);
    shortRelease = false;
    newEgg();
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
    if (sick && sickWait) sickWait--;  // ko12.6.1: la espera de la medicina corre tambien dormido (sin aviso)
    checkMedals();
    // ko12.2: dormida sola: se despierta con hambre (y si nadie la cuida, ya no vuelve a dormirse)
    if (autoSleep && fullness <= AUTO_SLEEP_MIN_FULL) { sleeping = false; autoSleep = false; }
    pendingSave = true;  // fork KO (ko4): se guarda cada minuto
    return;
  }

  careTick();  // ko9: EXP por tiempo de crianza
  // ko12.5: berrinches: sin necesitar nada, a veces llama "por llamar". Si nadie lo educa, se acostumbra
  routineToday();
  if (tantrum) {
    if (--tantrum == 0) discipline = dropTo(discipline, 3, 0);
  } else if (!sick && fullness >= 40 && joy >= 40 && energy >= 40 && hygiene >= 40 && !poops) {
    int pm = 7 - discipline / 20;                     // por mil y minuto: ~1 cada 2-5 h despierto
    if (personality() == PERS_CALM) pm = 2;
    if ((int)random(1000) < pm) tantrum = TANTRUM_MIN;
  }

  // ko12.6: resfriado. Riesgo por mil y minuto: sucio +3 (y +2 si ademas hay 2+ cacas), hambre +2
  if (sick) {
    if (sickWait && --sickWait == 0) sickNote = 4;  // ko12.6.1: aviso "ya toca"
    if (ageMinutes & 1) joy = clamp100(joy - 1);
    if (++sickMin >= SICK_MISTAKE_MIN) {
      sickMin = 0;
      careMistakes++;
      mistWhy = MW_SICK;
      mistEpoch = lastSeenEpoch;
      if (bond > 1) bond--;
      sickNote = 2;
    }
  } else {
    int pm = (hygiene < 30 ? (poops > 1 ? 5 : 3) : 0) + (fullness < 20 ? 2 : 0);
    if (pm && (int)random(1000) < pm) catchCold();
  }

  fullness = clamp100(fullness - 2);
  energy = clamp100(energy - 1);
  if (fullness > 40 && poops < 3 && !holdPoop && random(100) < 15) poops++;  // ko11.8: no en combate

  hygiene = clamp100(hygiene - 1 - 4 * poops);
  // el sobrepeso da pereza: la energia cae el doble
  if (weight > 50) energy = clamp100(energy - 1);
  if (weight > 0 && ageMinutes % 3 == 0) weight--;

  // la disciplina forja la defensa: 12 h seguidas bien cuidado = +1 DEF.
  // ko10.6: y ademas perdona un descuido (el buen cuidado repara el malo)
  if (lowestStat() >= 40 && !sick) {
    if (++goodTicks >= GOOD_CARE_TICKS) {
      goodTicks = 0;
      if (trDef < 100) trDef++;
      if (careMistakes) {
        careMistakes--;
        heartUntil = millis() + HEART_MS;
      }
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
  // ko11.18: la energia ya NO cuenta como descuido (baja sola jugando y entrenando);
  // solo comida, animo e higiene
  uint8_t lo3 = fullness < joy ? fullness : joy;
  if (hygiene < lo3) lo3 = hygiene;
  if (lo3 <= 10 && mistakeCooldown == 0) {
    careMistakes++;
    mistakeCooldown = 60;
    // ko10.9: apunta la causa (la barra mas baja; empate: comida, animo, higiene)
    uint8_t lo = lo3;
    mistWhy = fullness == lo ? MW_FOOD : joy == lo ? MW_JOY : MW_HYGIENE;
    mistEpoch = lastSeenEpoch;
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
  prefs.putUChar("mwhy", mistWhy);
  prefs.putUInt("mwhen", mistEpoch);
  prefs.putUShort("good", goodTicks);  // ko10.6: la racha sobrevive a un reinicio
  prefs.putBool("sleep", sleeping);
  prefs.putBool("aslp", autoSleep);  // ko12.2
  prefs.putBytes("life", &life, sizeof(life));  // ko12.4
  prefs.putBytes("walk", &walk, sizeof(walk));  // ko12.4
  prefs.putUChar("disc", discipline);  // ko12.5
  prefs.putUChar("tant", tantrum);
  prefs.putUChar("sick", sick);  // ko12.6
  prefs.putUChar("sickd", sickDoses);
  prefs.putUShort("sickm", sickMin);
  prefs.putUChar("sickw", sickWait);
  prefs.putUInt("rtd", rtDay);
  prefs.putUChar("rtb", rtBits);
  prefs.putUShort("rts", rtStreak);
  prefs.putUShort("rtx", rtBest);
  prefs.putUChar("bond", bond);
  if (lastSeenEpoch) prefs.putUInt("seen", lastSeenEpoch);
}

// quedan miembros sin registrar en la linea evolutiva de esta base?
// ko10: recorre las ramas (Eevee, Tyrogue, Slowpoke...) con dexEvoOptions
bool Pet::lineHasUnregistered(int16_t base) const {
  int16_t stack[16];
  int n = 0;
  stack[n++] = base;
  for (int guard = 0; n > 0 && guard < 32; guard++) {
    int16_t cur = stack[--n];
    if (cur < 1 || cur > DEX_COUNT) continue;
    if (!isRegistered(cur)) return true;
    int16_t opts[8];
    int k = dexEvoOptions(cur, opts);
    for (int i = 0; i < k && n < 16; i++) stack[n++] = opts[i];
  }
  return false;
}

uint8_t Pet::eggRarity() const {
  return (eggTarget >= 1 && eggTarget <= DEX_COUNT) ? DEX_TBL[eggTarget].rarity : (uint8_t)R_COMUN;
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
  // ko10.5: pases 0-1 sin familias ya criadas; el 2 (todo criado) sin limite
  for (int pass = 0; pass < 3; pass++) {
    for (int t = tier; t >= R_COMUN; t--) {
      int16_t cand[DEX_COUNT];
      int n = 0;
      for (int16_t d = 1; d <= DEX_COUNT; d++) {
        if (DEX_TBL[d].rarity != t) continue;
        if (pass < 2 && isFamRaised(d)) continue;
        if (pass == 0 && !lineHasUnregistered(d)) continue;
        cand[n++] = d;
      }
      if (n > 0) return cand[random(n)];
    }
  }
  return CLASSIC_DEX[random(NUM_CLASSIC_DEX)];  // inalcanzable, por si acaso
}

bool Pet::isFamRaised(int16_t dex) const {
  if (dex < 1 || dex > DEX_COUNT) return false;
  int f = DEX_FAM[dex];
  return famRaised[(f - 1) >> 3] & (1 << ((f - 1) & 7));
}

void Pet::markFamRaised(int16_t dex) {
  if (dex < 1 || dex > DEX_COUNT) return;
  int f = DEX_FAM[dex];
  famRaised[(f - 1) >> 3] |= (uint8_t)(1 << ((f - 1) & 7));
  pendingSave = true;
}

bool Pet::allFamsRaised() const {
  for (int16_t d = 1; d <= DEX_COUNT; d++)
    if (DEX_FAM[d] == d && !isFamRaised(d)) return false;
  return true;
}

void Pet::registerSpecies(int16_t dex) {
  if (dex < 1 || dex > DEX_COUNT) return;
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
  for (int i = 1; i <= DEX_COUNT; i++)
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
  shortRelease = isShortStay();  // ko11.9.2
  // soltarlo recien nacido no da la suerte de una crianza: el huevo siguiente es
  // como tras una escapada (comun), para que no sirva para "tirar" huevos
  lastEnd = shortRelease ? CER_RUNAWAY : CER_RELEASE;
  ceremony = CER_RELEASE;
  ceremonyUntil = millis() + CEREMONY_MS;
  heartUntil = ceremonyUntil;
  sfxPlay(SFX_BYE);
  save();
}

void Pet::hatch() {
  speciesId = eggTarget;
  shiny = eggShiny;
  eggCharm = false;  // ko10.10: el shiny UP ya se uso en este huevo
  // genes del individuo: 90-110% por stat (cada crianza es unica)
  geneAtk = 90 + random(21);
  geneDef = 90 + random(21);
  geneSpe = 90 + random(21);
  trAtk = trDef = trSpe = 0;
  resetTrainRecords();  // ko11.9.2
  evolvedHere = false;
  berryKnown = false;
  movesNew();        // ko11.31: los de su especie y nivel
  bond = 0;          // vinculo, medallas y nombre son del individuo
  bondToday = 0;
  medals = 0;
  newMedal = 0;
  nick[0] = 0;
  registerSpecies(speciesId);  // criado = registrado en la pokedex
  lifeStart(LF_EGG);  // ko12.4
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

static bool friendEvoDex(int16_t d) {
  return d == 172 || d == 173 || d == 174 || d == 175 || d == 42 || d == 113;
}
bool Pet::needsFriendship() const { return !isEgg() && friendEvoDex(speciesId); }

bool Pet::canEvolveNow() const {
  if (isEgg() || sleeping || ceremony != CER_NONE) return false;
  if (needsFriendship() && bond < FRIEND_EVO_BOND) return false;  // ko11.7
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
  if (sick && (ageMinutes & 1)) return;  // ko12.6: malo: la mitad
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

// ko10.7: entrenar siempre da algo: 5% de lo que pide el nivel en EXP; batir el
// record da 20% y un caramelo de su familia. Cuesta energia (12), asi que no se
// puede abusar: con la energia llena salen unas 8 sesiones.
// ko11.9.2: los historicos siempre >= los del bicho actual
static inline void keepMax(uint16_t &all, uint16_t cur) { if (cur > all) all = cur; }
#define SHORT_STAY_MIN (24u * 60u)
bool Pet::isShortStay() const { return !isEgg() && !evolvedHere && ageMinutes < SHORT_STAY_MIN; }

void Pet::resetTrainRecords() {
  keepMax(allStrHi, strHi); keepMax(allDefHi, defHi); keepMax(allSpeHi, speHi);
  keepMax(allGameHi, gameHi); keepMax(allVbBest, vbBest);
  strHi = defHi = speHi = gameHi = 0;
  vbStreak = vbBest = 0;  // el voleibol vuelve a empezar contra rivales faciles
}

void Pet::trainBonus(bool scored, bool record) {
  lastTrainExp = 0;
  lastTrainCandy = 0;
  if (!isEgg()) LIFE_INC(life.trains);  // ko12.4
  if (!scored || isEgg()) return;
  uint16_t L = level();
  if (L < LEVEL_MAX) {
    uint32_t step = expForLevel(L + 1) - expForLevel(L);
    lastTrainExp = step * (record ? TRAIN_EXP_PCT_HI : TRAIN_EXP_PCT) / 100;
    if (discipline >= 60 || personality() == PERS_HARDWORK) lastTrainExp = lastTrainExp * 5 / 4;  // ko12.5: bien educado / trabajador
    if (!lastTrainExp) lastTrainExp = 1;
    addExp(lastTrainExp);
  }
  if (record) {
    addCandy(speciesId, 1);
    lastTrainCandy = 1;
  }
}

// ---- ko11.31: movimientos
uint8_t Pet::moveMain() const {
  uint8_t ty = DEX_TBL[speciesId].ptype;
  for (uint8_t i = 0; i < 4; i++) {
    uint8_t t;
    if (moveDecode(mv[i], &t, nullptr, nullptr) && t == ty) return mv[i];
  }
  return moveIdTyped(ty, moveTier(speciesId), 0);
}
uint8_t Pet::moveVar() const {
  uint8_t v = 0;
  moveDecode(moveMain(), nullptr, nullptr, &v);
  return v;
}
void Pet::movesNew() {
  movesDefault(speciesId, level(), mv);
  ppRefill();
  moveLv = (uint8_t)(level() / 5 * 5);
  moveOffer = 0;
}
void Pet::ppRefill() {
  for (uint8_t i = 0; i < 4; i++) pp[i] = mv[i] ? movePP(mv[i]) : 0;
}
bool Pet::ppFull() const {
  for (uint8_t i = 0; i < 4; i++) if (mv[i] && pp[i] < movePP(mv[i])) return false;
  return true;
}
uint8_t Pet::moveRandomNew() const {
  uint8_t pool[200];
  uint8_t n = movePool(speciesId, pool, sizeof(pool)), k = 0;
  for (uint8_t i = 0; i < n; i++) if (!movesHas(mv, pool[i])) pool[k++] = pool[i];
  return k ? pool[random(k)] : 0;
}
void Pet::moveOfferNew(uint8_t id) {
  if (!id || movesHas(mv, id)) return;
  for (uint8_t i = 0; i < 4; i++)
    if (!mv[i]) {
      mv[i] = id;
      pp[i] = movePP(id);
      moveLearned = id;
      pendingSave = true;
      return;
    }
  moveOffer = id;
  pendingSave = true;
}
void Pet::moveAccept(uint8_t slot) {
  if (moveOffer && slot < 4 && !movesHas(mv, moveOffer)) {
    mv[slot] = moveOffer;
    pp[slot] = movePP(moveOffer);
    moveLearned = moveOffer;
  }
  moveOffer = 0;
  pendingSave = true;
}

uint16_t Pet::addExp(uint32_t x) {
  if (isEgg() || !x) return 0;
  uint16_t before = level();
  uint32_t top = expForLevel(LEVEL_MAX);
  exp = (exp >= top || x >= top - exp) ? top : exp + x;
  uint16_t up = level() - before;
  if (up) {
    // ko11.31: cada 5 niveles le ofrecen uno que aun no sabe (de los que puede aprender)
    uint16_t L = level();
    if (L / 5 > moveLv / 5 && !moveOffer) {
      moveLv = (uint8_t)(L / 5 * 5);
      uint8_t id = moveRandomNew();
      if (id) moveOfferNew(id);
    }
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
  // ramas (Eevee, Tyrogue, Slowpoke, Poliwhirl, Gloom): al azar, prefiriendo
  // la que falte en la pokedex (ko10: generico, antes solo Eevee)
  int16_t opts[8], nuevas[8];
  int n = dexEvoOptions(speciesId, opts), m = 0;
  if (speciesId == 133) {  // ko11.7: Eevee. Vinculo alto: Espeon (dia) / Umbreon (noche)
    if (bond >= FRIEND_EVO_BOND) {
      uint8_t h = lastSeenEpoch ? (uint8_t)(lastSeenEpoch / 3600 % 24) : 12;
      opts[0] = (h >= 20 || h < 6) ? 197 : 196;
      n = 1;
    } else {  // sin amistad: solo las de piedra (Vaporeon, Jolteon, Flareon)
      int k = 0;
      for (int i = 0; i < n; i++) if (opts[i] != 196 && opts[i] != 197) opts[k++] = opts[i];
      n = k;
    }
    next = opts[0];
  }
  for (int i = 0; i < n; i++)
    if (!isRegistered(opts[i])) nuevas[m++] = opts[i];
  if (m) next = nuevas[random(m)];
  else if (n > 1) next = opts[random(n)];
  speciesId = next;
  evolvedHere = true;  // ko11.9.2
  {  // ko12.4: al diario (dos evoluciones como mucho; una tercera ocupa la ultima)
    uint8_t k = life.evoDex[0] ? 1 : 0;
    life.evoDex[k] = speciesId;
    life.evoT[k] = lastSeenEpoch;
  }
  // ko11.31: con la forma nueva, uno de los 3 ataques de su tipo de la nueva fase
  {
    uint8_t ty = DEX_TBL[speciesId].ptype, tr = moveTier(speciesId), v0 = (uint8_t)random(3);
    for (uint8_t k = 0; k < 3; k++) {
      uint8_t id = moveIdTyped(ty, tr, (uint8_t)((v0 + k) % 3));
      if (!movesHas(mv, id)) { moveOfferNew(id); break; }
    }
  }
  // ko11.16: si cambia de tipo, el orbe ya no le sirve: 3 caramelos (o, a veces, 1 raro)
  orbEvoNote = 0;
  if (orbValid(orb) && !orbFits(orb)) {
    bool dual = orbDual(orb);
    orb = 0;
    if (dual) {  // ko12.2.1: el arcoiris vale mas
      addCandy(speciesId, ORB_EVO_DUAL_CANDY);
      orbEvoNote = 3;
      if ((int)random(100) < ORB_EVO_DUAL_RARE_PCT) { if (rareCandy < 999) rareCandy++; orbEvoNote = 4; }
    } else if ((int)random(100) < ORB_EVO_RARE_PCT) { if (rareCandy < 999) rareCandy++; orbEvoNote = 2; }
    else { addCandy(speciesId, ORB_EVO_CANDY); orbEvoNote = 1; }
  }
  registerSpecies(speciesId);
  sfxPlay(SFX_EVOLVE);
  evolveUntil = millis() + EVOLVE_ANIM_MS;
  save();
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
  LIFE_INC(life.meals);  // ko12.4
  routineDo(RT_MEAL);    // ko12.5
  if (personality() == PERS_GLUTTON) fullness = clamp100(fullness + 5);
  registerCare();
  save();
}

// ko10: la familia es el dex mas bajo de toda la linea (DEX_FAM): con los bebes
// de gen 2 (Pichu, Cleffa...) la favorita sigue siendo la de siempre
uint8_t Pet::favFood() const {
  if (speciesId < 1 || speciesId > DEX_COUNT) return 0;
  return (uint8_t)(DEX_FAM[speciesId] % 4);
}

void Pet::feedCandy() {
  if (ceremony != CER_NONE) return;
  if (isEgg() || sleeping) return;
  LIFE_INC(life.snacks);  // ko12.4
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

// ---------------------------------------------------------------- ko10.4: caramelos

uint16_t Pet::candyOf(int16_t dex) const {
  if (dex < 1 || dex > DEX_COUNT) return 0;
  return candy[DEX_FAM[dex]];
}

void Pet::addCandy(int16_t dex, uint16_t n) {
  if (dex < 1 || dex > DEX_COUNT || !n) return;
  uint16_t &c = candy[DEX_FAM[dex]];
  c = (uint32_t)c + n > CANDY_MAX ? CANDY_MAX : c + n;
  pendingSave = true;
}

// cambiar un repetido por caramelos: 3, +2 si es shiny, +1 si es de nivel 30 o mas
// ko10.11: cambiar caramelos de otra familia por los de la que crias (3 -> 1)
// ko11.15.1: los caramelos sueltos de otras familias (1 o 2 de muchas, que nunca
// llegaban a 3 para cambiar) se vuelven trozos; cada 10 trozos, un caramelo raro
// (que vale para cualquier Pokemon que cries despues)
uint16_t Pet::candyToShards() {
  if (isEgg()) return 0;
  uint8_t mine = DEX_FAM[speciesId];
  uint32_t n = 0;
  for (int f = 1; f <= DEX_COUNT; f++) {
    if (f == mine || !candy[f]) continue;
    n += candy[f];
    candy[f] = 0;
  }
  if (!n) return 0;
  addShards(n > 65535 ? 65535 : (uint16_t)n);
  save();
  return (uint16_t)(n > 65535 ? 65535 : n);
}

bool Pet::candyTrade(int16_t famDex, uint16_t times) {
  if (isEgg() || famDex < 1 || famDex > DEX_COUNT || !times) return false;
  uint8_t fam = DEX_FAM[famDex], mine = DEX_FAM[speciesId];
  if (fam == mine || candy[fam] < (uint32_t)times * CANDY_TRADE_RATE) return false;
  candy[fam] -= times * CANDY_TRADE_RATE;
  addCandy(speciesId, times);
  save();
  return true;
}

bool Pet::useRareCandy() {
  if (isEgg() || !rareCandy) return false;
  rareCandy--;
  addCandy(speciesId, RARE_CANDY_VALUE);
  save();
  return true;
}

uint16_t Pet::dupCandy(bool shinyMon, uint16_t lvl) {
  return 3 + (shinyMon ? 2 : 0) + (lvl >= 30 ? 1 : 0);
}

bool Pet::candyCanUse(uint8_t use) const {
  if (use >= CU_COUNT || isEgg() || ceremony != CER_NONE) return false;
  if (candyOf(speciesId) < CANDY_COST[use]) return false;
  switch (use) {
    case CU_EXP:   return level() < LEVEL_MAX;
    case CU_GAUGE: return joy < 100 || energy < 100 || fullness < 100;
    case CU_GENES: return geneAtk < CANDY_GENE_MAX || geneDef < CANDY_GENE_MAX || geneSpe < CANDY_GENE_MAX;
    case CU_SHINY: return !shinyCharm;
    case CU_EVO:   return careMistakes > 0;
  }
  return false;
}

bool Pet::candyUse(uint8_t use) {
  if (!candyCanUse(use)) return false;
  candy[DEX_FAM[speciesId]] -= CANDY_COST[use];
  switch (use) {
    case CU_EXP: {  // media subida de nivel
      uint16_t L = level();
      addExp((expForLevel(L + 1) - expForLevel(L) + 1) / 2);
      break;
    }
    case CU_GAUGE:
      joy = clamp100(joy + 20);
      energy = clamp100(energy + 20);
      fullness = clamp100(fullness + 20);
      break;
    case CU_GENES: {
      auto up = [](uint8_t &g) { g = g + 2 > CANDY_GENE_MAX ? CANDY_GENE_MAX : g + 2; };
      up(geneAtk); up(geneDef); up(geneSpe);
      break;
    }
    case CU_SHINY: shinyCharm = true; break;
    case CU_EVO:   careMistakes--; break;  // la evolucion llega un nivel antes
  }
  heartUntil = millis() + HEART_MS;
  save();
  return true;
}

bool Pet::playResult(uint8_t score) {
  if (ceremony != CER_NONE || isEgg()) return false;
  // fork KO (ko9.2): 30 s y 3 vidas; el premio grande (animo + energia) solo al
  // batir el record. ko10.3: sin record, si ha dado al menos un toque, un poco de
  // energia (GAME_SMALL_ENERGY). Nunca cansa. El ejercicio si quema peso.
  int burn = (int)weight - score * 2;
  weight = burn > 0 ? burn : 0;
  LIFE_INC(life.plays);  // ko12.4
  routineDo(RT_PLAY);    // ko12.5
  bool record = score > gameHi;
  lastAllTime = record && score > allGameHi;
  if (record) {
    gameHi = score;
    joy = clamp100(joy + 10 + (score > 15 ? 30 : score * 2));
    energy = clamp100(energy + 10 + (score > 20 ? 10 : score / 2));
    heartUntil = millis() + HEART_MS;
    addBond(2);
  } else if (score > 0) {
    energy = clamp100(energy + GAME_SMALL_ENERGY);
  }
  registerCare();
  save();
  return record;
}

// saco de entrenamiento: los golpes entrenan la fuerza. Devuelve la subida.
uint8_t Pet::trainStrength(uint16_t hits, uint16_t bags) {
  if (ceremony != CER_NONE || isEgg()) return 0;
  trainBonus(bags > 0, bags > strHi);
  lastAllTime = bags > strHi && bags > allStrHi;
  uint8_t gain = hits / 4;          // ~4 golpes = 1 punto de entrenamiento
  if (gain > 18) gain = 18;         // tope por sesion: la FUE se forja a fuego lento
  if (hits > 0 && gain < TRAIN_MIN_GAIN) gain = TRAIN_MIN_GAIN;  // ko11.7: jugar siempre da algo
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
  if (bags > strHi) strHi = bags;   // ko10.7: record de sacos rotos (antes golpes en 10 s)
  addBond(2);
  registerCare();
  save();
  return gain;
}

// fork KO (ko4): comun a los entrenamientos: sube la stat con tope por sesion
// y cansa igual que el saco. Devuelve lo que de verdad subio.
static uint8_t trainGain(uint8_t &tr, uint16_t raw) {
  uint8_t gain = raw > 18 ? 18 : (uint8_t)raw;
  if (raw > 0 && gain < TRAIN_MIN_GAIN) gain = TRAIN_MIN_GAIN;  // ko11.7: minimo por sesion jugada
  uint8_t antes = tr;
  uint16_t v = (uint16_t)tr + gain;
  tr = v > 100 ? 100 : (uint8_t)v;
  return tr - antes;
}

// ko11.9: voleibol. Ganar = racha +1 (record -> premio grande + caramelo), EXP y
// caramelo; perder corta la racha. Los puntos hechos entrenan la VEL (minimo 3).
uint8_t Pet::volleyResult(bool won, uint8_t myPoints) {
  if (ceremony != CER_NONE || isEgg()) return 0;
  bool record = false;
  if (won) {
    if (vbStreak < 65535) vbStreak++;
    record = vbStreak > vbBest;
    lastAllTime = vbStreak > allVbBest;
    if (record) vbBest = vbStreak;
  } else {
    vbStreak = 0;
    lastAllTime = false;
  }
  trainBonus(won, record);
  if (won && !record) {  // ganar siempre da un caramelo (el record ya lo da trainBonus)
    addCandy(speciesId, 1);
    lastTrainCandy = 1;
  }
  uint8_t gain = trainGain(trSpe, (uint16_t)myPoints * 2);
  energy = dropTo(energy, 12, 5);
  fullness = dropTo(fullness, 5, 5);
  joy = clamp100(joy + (won ? 10 : 4));
  if (won) { heartUntil = millis() + HEART_MS; addBond(2); }
  registerCare();
  save();
  return gain;
}

uint8_t Pet::trainDefense(uint16_t blocked) {
  if (ceremony != CER_NONE || isEgg()) return 0;
  trainBonus(blocked > 0, blocked > defHi);
  lastAllTime = blocked > defHi && blocked > allDefHi;
  uint8_t gain = trainGain(trDef, blocked ? (blocked / 2 ? blocked / 2 : 1) : 0);  // ~2 paradas = 1 punto (ko11.7: min 3)
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

uint8_t Pet::trainSpeed(uint16_t hits, uint16_t points) {
  if (ceremony != CER_NONE || isEgg()) return 0;
  trainBonus(points > 0, points > speHi);
  lastAllTime = points > speHi && points > allSpeHi;
  uint8_t gain = trainGain(trSpe, hits);  // 1 acierto = 1 punto (15 rondas)
  energy = dropTo(energy, 12, 5);
  fullness = dropTo(fullness, 5, 5);
  int burn = (int)weight - hits;
  weight = burn > 0 ? burn : 0;
  joy = clamp100(joy + 6);
  if (hits >= 10) heartUntil = millis() + HEART_MS;
  if (points > speHi) speHi = points;  // ko10.6: el record son los puntos (reflejos)
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
  LIFE_INC(life.plays);  // ko12.4
  routineDo(RT_PLAY);    // ko12.5
  if (personality() == PERS_PLAYFUL) joy = clamp100(joy + 5);
  addBond(2);
  registerCare();
  save();
}

void Pet::toggleLight() {
  if (ceremony != CER_NONE) return;
  if (isEgg()) return;
  sleeping = !sleeping;
  autoSleep = false;  // ko12.2: acostada (o despertada) a mano
  if (sleeping) { routineDo(RT_BED); tantrum = 0; }  // ko12.5
  save();
}

void Pet::clean() {
  if (ceremony != CER_NONE) return;
  if (poops && !isEgg()) { LIFE_INC(life.cleans); if (personality() == PERS_TIDY) joy = clamp100(joy + 5); }  // ko12.4 / ko12.5
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
  LIFE_INC(life.pets);  // ko12.4
  if (personality() == PERS_CUDDLY) joy = clamp100(joy + 3);  // ko12.5
  registerCare();
}

// ---- ko12.6: resfriado ----
bool Pet::catchCold() {
  if (isEgg() || sick || ceremony != CER_NONE || starterPick) return false;
  sick = 1;
  sickDoses = (uint8_t)(1 + random(2));
  sickMin = 0;
  tantrum = 0;
  sickWait = 0;
  sickNote = 1;
  save();
  return true;
}

void Pet::setBirthday(uint8_t m, uint8_t d) {
  if (m < 1 || m > 12 || d < 1 || d > 31) m = d = 0;
  bdayM = m;
  bdayD = d;
  save();
}

bool Pet::birthdayGift(uint16_t year) {
  if (!bdayM || bdayYear == year) return false;
  bdayYear = year;
  if (!isEgg()) { joy = clamp100(joy + 30); addBond(2); }
  if (rareCandy < CANDY_MAX) rareCandy++;
  save();
  return true;
}

uint8_t Pet::giveMedicine() {
  if (!sick) return 0;
  if (sickWait) return 3;
  joy = clamp100(joy - 3);  // amarga
  registerCare();
  if (sickDoses > 1) { sickDoses--; sickWait = SICK_DOSE_GAP; sickMin = 0; save(); return 1; }
  sick = sickDoses = sickWait = 0;
  sickMin = 0;
  heartUntil = millis() + HEART_MS;
  addBond(1);
  save();
  return 2;
}

void Pet::friendPlay() {
  if (isEgg() || sleeping || ceremony != CER_NONE) return;
  joy = clamp100(joy + 10);
  heartUntil = millis() + HEART_MS;
  addBond(1);
  registerCare();
  save();
}

void Pet::eggTap() {
  if (!isEgg()) return;
  if (++eggTaps >= 3) hatch();
  else save();
}

PetMood Pet::mood() const {
  if (sleeping) return MOOD_SLEEPING;
  if (eating()) return MOOD_EATING;
  if (lowestStat() < 25 || sick) return MOOD_SAD;
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
  prefs.putBytes("candy", candy, sizeof(candy));  // ko10.4
  prefs.putBytes("famr", famRaised, sizeof(famRaised));  // ko10.5
  prefs.putBool("scharm", shinyCharm);
  prefs.putBool("echarm", eggCharm);
  prefs.putUChar("badge", badges);  // ko10.4
  prefs.putUInt("dday", dailyDoneDay);
  prefs.putUShort("dclr", dailyClears);
  prefs.putBytes("gymd", gymDay, sizeof(gymDay));  // ko10.11
  prefs.putBytes("gymw", gymWins, sizeof(gymWins));
  prefs.putUShort("chw", champWins);
  prefs.putUShort("chs", champStreak);  // ko11.6.1
  prefs.putUShort("chb", champBest);
  prefs.putBytes("fstk", fameStreak, sizeof(fameStreak));
  prefs.putBytes("exped", &exped, sizeof(exped));  // ko11.7
  prefs.putUChar("dxrw", dexRewards);
  prefs.putUShort("rcandy", rareCandy);
  prefs.putUShort("rshd", rareShards);  // ko11.15.1
  prefs.putUShort("orb", orb);           // ko11.16
  {  // ko11.31: los 4 movimientos y sus PP
    uint8_t b[8];
    memcpy(b, mv, 4);
    memcpy(b + 4, pp, 4);
    prefs.putBytes("mv4", b, 8);
  }
  prefs.putUChar("mvl", moveLv);
  prefs.putUChar("mvo2", moveOffer);
  prefs.putUChar("orbn", orbN);
  if (orbN) prefs.putBytes("orbs", orbBag, orbN * sizeof(uint16_t));
  else prefs.remove("orbs");
  prefs.putUInt("age", ageMinutes);
  prefs.putUInt("exp", exp);
  prefs.putShort("dexn", speciesId);
  prefs.putShort("eggT2", eggTarget);
  prefs.putUChar("crack", eggTaps);
  prefs.putUChar("mist", careMistakes);
  prefs.putUChar("mwhy", mistWhy);
  prefs.putUInt("mwhen", mistEpoch);
  prefs.putUShort("good", goodTicks);  // ko10.6: la racha sobrevive a un reinicio
  prefs.putBool("sleep", sleeping);
  prefs.putBool("aslp", autoSleep);  // ko12.2
  prefs.putBytes("life", &life, sizeof(life));  // ko12.4
  prefs.putBytes("walk", &walk, sizeof(walk));  // ko12.4
  prefs.putUChar("disc", discipline);  // ko12.5
  prefs.putUChar("tant", tantrum);
  prefs.putUChar("sick", sick);  // ko12.6
  prefs.putUChar("sickd", sickDoses);
  prefs.putUShort("sickm", sickMin);
  prefs.putUChar("sickw", sickWait);
  prefs.putUInt("rtd", rtDay);
  prefs.putUChar("rtb", rtBits);
  prefs.putUShort("rts", rtStreak);
  prefs.putUShort("rtx", rtBest);
  prefs.putUChar("room", roomOn);
  prefs.putUChar("bgask", bgAsked);
  prefs.putBytes("deco", deco, sizeof(deco));
  prefs.putUChar("lend", lastEnd);
  prefs.putUShort("evdl", evoDeclinedLv);  // ko12.5.1
  prefs.putUChar("bdm", bdayM);  // ko12.6
  prefs.putUChar("bdd", bdayD);
  prefs.putUShort("bdy", bdayYear);
  prefs.putUInt("fdcl", farDeclinedAge);
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
  prefs.putUShort("sb2", strHi);  // ko11.14: carga y golpe, records desde 0  // ko10.7: clave nueva (sacos rotos); el record viejo (golpes) no vale
  prefs.putString("nick", nick);
  prefs.putUShort("wwin", wildWins);
  prefs.putUShort("lwin", linkWins);
  prefs.putUShort("lbat", linkBattles);
  prefs.putUShort("trd", trades);
  prefs.putUChar("balls", balls);
  prefs.putUChar("potn", potions);
  prefs.putUShort("dh2", defHi);  // ko11.14: defensa por timing, records nuevos desde 0
  prefs.putUShort("vbs", vbStreak);  // ko11.9
  prefs.putUShort("vbb", vbBest);
  prefs.putUShort("vp2", speHi);  // ko11.14: en orden, records desde 0  // ko10.6: clave nueva (puntos); el record viejo (aciertos) no vale
  // ko11.9.2: historicos (del jugador) y marca de "records por bicho"
  keepMax(allStrHi, strHi); keepMax(allDefHi, defHi); keepMax(allSpeHi, speHi);
  keepMax(allGameHi, gameHi); keepMax(allVbBest, vbBest);
  prefs.putUShort("as2", allStrHi);
  prefs.putUShort("ad2", allDefHi);
  prefs.putUShort("ap2", allSpeHi);
  prefs.putUShort("agh", allGameHi);
  prefs.putUShort("avb", allVbBest);
  prefs.putUChar("rpp", 1);
  prefs.putBool("evh", evolvedHere);
}

void Pet::load(bool *migrated) {
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
  if (prefs.getBytes("candy", candy, sizeof(candy)) != sizeof(candy)) memset(candy, 0, sizeof(candy));
  for (auto &c : candy) if (c > CANDY_MAX) c = CANDY_MAX;
  shinyCharm = prefs.getBool("scharm", false);
  eggCharm = prefs.getBool("echarm", false);
  badges = prefs.getUChar("badge", 0);
  dailyDoneDay = prefs.getUInt("dday", 0);
  dailyClears = prefs.getUShort("dclr", 0);
  if (prefs.getBytes("gymd", gymDay, sizeof(gymDay)) != sizeof(gymDay)) memset(gymDay, 0, sizeof(gymDay));
  if (prefs.getBytes("gymw", gymWins, sizeof(gymWins)) != sizeof(gymWins)) memset(gymWins, 0, sizeof(gymWins));
  champWins = prefs.getUShort("chw", 0);
  champStreak = prefs.getUShort("chs", 0);  // ko11.6.1
  champBest = prefs.getUShort("chb", 0);
  if (champBest < champStreak) champBest = champStreak;
  if (prefs.getBytes("fstk", fameStreak, sizeof(fameStreak)) != sizeof(fameStreak)) memset(fameStreak, 0, sizeof(fameStreak));
  if (prefs.getBytes("exped", &exped, sizeof(exped)) != sizeof(exped) || exped.dex < 1 || exped.dex > DEX_COUNT)
    memset(&exped, 0, sizeof(exped));  // ko11.7
  dexRewards = prefs.getUChar("dxrw", 0);
  rareCandy = prefs.getUShort("rcandy", 0);
  rareShards = prefs.getUShort("rshd", 0);
  orb = prefs.getUShort("orb", 0);  // ko11.16
  orbN = prefs.getUChar("orbn", 0);
  if (orbN > ORB_BAG_MAX) orbN = 0;
  if (orbN && prefs.getBytes("orbs", orbBag, orbN * sizeof(uint16_t)) != orbN * sizeof(uint16_t)) orbN = 0;
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
  // ko11.31: partidas de antes: sigue con el ataque de siempre (0) y sin oferta hasta el proximo multiplo de 5
  moveLv = prefs.isKey("mvl") ? prefs.getUChar("mvl", 0) : (uint8_t)(level() / 5 * 5);
  {  // ko11.31: los 4 movimientos. Guardado de antes: los de su especie, con su ataque de tipo de siempre
    uint8_t b[8] = { 0 };
    if (!isEgg() && prefs.getBytes("mv4", b, 8) == 8) {
      memcpy(mv, b, 4);
      memcpy(pp, b + 4, 4);
      for (uint8_t i = 0; i < 4; i++) {
        if (!moveValid(mv[i])) mv[i] = 0;
        if (mv[i] && pp[i] > movePP(mv[i])) pp[i] = movePP(mv[i]);
        if (!mv[i]) pp[i] = 0;
      }
    } else {
      uint8_t k = prefs.getUChar("mvk", 0);
      uint8_t keepLv = moveLv;
      if (!isEgg()) {
        movesNew();
        uint8_t mine = moveIdTyped(DEX_TBL[speciesId].ptype, moveTier(speciesId), k > 2 ? 0 : k);
        if (!movesHas(mv, mine)) { mv[0] = mine; pp[0] = movePP(mine); }
        else for (uint8_t i = 1; i < 4; i++) if (mv[i] == mine) { mv[i] = mv[0]; uint8_t q = pp[i]; pp[i] = pp[0]; pp[0] = q; mv[0] = mine; }
      }
      moveLv = keepLv;
      prefs.remove("mvk");
      prefs.remove("mvo");
    }
    if (!isEgg() && !moveCount(mv)) movesNew();
    moveOffer = prefs.getUChar("mvo2", 0);
    if (!moveValid(moveOffer) || movesHas(mv, moveOffer)) moveOffer = 0;
  }
  eggTaps = prefs.getUChar("crack", 0);
  careMistakes = prefs.getUChar("mist", 0);
  mistWhy = prefs.getUChar("mwhy", MW_NONE);
  mistEpoch = prefs.getUInt("mwhen", 0);
  goodTicks = prefs.getUShort("good", 0);
  if (goodTicks >= GOOD_CARE_TICKS) goodTicks = 0;
  sleeping = prefs.getBool("sleep", false);
  autoSleep = sleeping && prefs.getBool("aslp", false);  // ko12.2
  // ko12.4: el diario de la crianza; si ya se criaba antes de esta version, empieza hoy
  memset(&life, 0, sizeof(life));
  if (prefs.getBytesLength("life") == sizeof(life)) prefs.getBytes("life", &life, sizeof(life));
  else if (!isEgg() && speciesId >= 1) lifeStart(LF_UPDATE);
  memset(&walk, 0, sizeof(walk));
  if (prefs.getBytesLength("walk") == sizeof(walk)) prefs.getBytes("walk", &walk, sizeof(walk));
  discipline = prefs.getUChar("disc", DISC_START);  // ko12.5
  tantrum = prefs.getUChar("tant", 0);
  if (tantrum > TANTRUM_MIN) tantrum = 0;
  sick = prefs.getUChar("sick", 0) ? 1 : 0;  // ko12.6
  sickDoses = prefs.getUChar("sickd", 0);
  sickMin = prefs.getUShort("sickm", 0);
  sickWait = prefs.getUChar("sickw", 0);
  if (sickWait > SICK_DOSE_GAP) sickWait = SICK_DOSE_GAP;
  bdayM = prefs.getUChar("bdm", 0);
  bdayD = prefs.getUChar("bdd", 0);
  bdayYear = prefs.getUShort("bdy", 0);
  if (sick && !sickDoses) sickDoses = 1;
  if (!sick) sickDoses = 0;
  rtDay = prefs.getUInt("rtd", 0);
  rtBits = prefs.getUChar("rtb", 0);
  rtStreak = prefs.getUShort("rts", 0);
  rtBest = prefs.getUShort("rtx", 0);
  roomOn = prefs.getUChar("room", 0);
  bgAsked = prefs.getUChar("bgask", 0);
  memset(deco, 0, sizeof(deco));
  if (prefs.getBytesLength("deco") == sizeof(deco)) prefs.getBytes("deco", deco, sizeof(deco));
  for (uint8_t &d : deco) if (d > DECO_COUNT) d = 0;
  lastEnd = prefs.getUChar("lend", CER_NONE);
  evoDeclinedLv = prefs.getUShort("evdl", 0);  // ko12.5.1
  farDeclinedAge = prefs.getUInt("fdcl", 0);
  prefs.getBytes("dexreg", dexReg, sizeof(dexReg));
  // ko10.5: familias criadas. Guardados de antes: lo registrado (criado) cuenta
  // como criado, salvo la familia del que se esta criando ahora
  if (prefs.getBytes("famr", famRaised, sizeof(famRaised)) != sizeof(famRaised)) {
    memset(famRaised, 0, sizeof(famRaised));
    int16_t cur = prefs.isKey("dexn") ? prefs.getShort("dexn", -1) : -1;
    for (int16_t d = 1; d <= DEX_COUNT; d++)
      if (isRegistered(d) && !(cur >= 1 && cur <= DEX_COUNT && DEX_FAM[d] == DEX_FAM[cur])) {
        int f = DEX_FAM[d];
        famRaised[(f - 1) >> 3] |= (uint8_t)(1 << ((f - 1) & 7));
      }
  }
  streak = prefs.getUShort("strk", 0);
  bestStreak = prefs.getUShort("bstrk", 0);
  lastCareDay = prefs.getUInt("cday", 0);
  bond = prefs.getUChar("bond", 0);
  medals = prefs.getUShort("medal", 0);
  totalMedals = prefs.getUShort("tmedal", 0);
  lastMilestone = prefs.getUShort("mstone", 0);
  gameHi = prefs.getUShort("ghi", 0);
  strHi = prefs.getUShort("sb2", 0);
  prefs.getString("nick", nick, sizeof(nick));
  wildWins = prefs.getUShort("wwin", 0);
  linkWins = prefs.getUShort("lwin", 0);
  linkBattles = prefs.getUShort("lbat", 0);
  trades = prefs.getUShort("trd", 0);
  balls = prefs.getUChar("balls", 5);   // partidas anteriores a ko4: kit inicial
  potions = prefs.getUChar("potn", 2);
  defHi = prefs.getUShort("dh2", 0);  // ko11.14 (la clave "dhi" era del juego viejo)
  vbStreak = prefs.getUShort("vbs", 0);  // ko11.9
  vbBest = prefs.getUShort("vbb", 0);
  speHi = prefs.getUShort("vp2", 0);
  allStrHi = prefs.getUShort("as2", 0);  // ko11.9.2
  allDefHi = prefs.getUShort("ad2", 0);
  allSpeHi = prefs.getUShort("ap2", 0);
  allGameHi = prefs.getUShort("agh", 0);
  allVbBest = prefs.getUShort("avb", 0);
  evolvedHere = prefs.getBool("evh", false);
  if (!prefs.getUChar("rpp", 0)) {
    // partida de antes de ko11.9.2: los records eran de siempre. Pasan a historicos
    // y el bicho actual empieza los suyos de cero (asi puede ganar el premio de record)
    resetTrainRecords();
    if (migrated) *migrated = true;
  }
  // ko11.5: nada fuera de rango entra en juego aunque la NVS venga danada
  // (una placa se quedo reiniciando en bucle al arrancar y solo volvio
  // borrando la partida entera: mejor corregir el valor que perderlo todo)
  if (speciesId < -1 || speciesId > DEX_COUNT || speciesId == 0) speciesId = -1;
  if (eggTarget < 1 || eggTarget > DEX_COUNT) eggTarget = 4;
  if (mistWhy > MW_HYGIENE) mistWhy = MW_NONE;
  if (lastEnd > CER_RELEASE) lastEnd = CER_NONE;
  if (fullness > 100) fullness = 100;
  if (joy > 100) joy = 100;
  if (energy > 100) energy = 100;
  if (hygiene > 100) hygiene = 100;
  if (trAtk > 100) trAtk = 100;
  if (trDef > 100) trDef = 100;
  if (trSpe > 100) trSpe = 100;
  nick[sizeof(nick) - 1] = 0;
  // siembra: la mascota actual cuenta como criada (guardados antiguos)
  if (speciesId >= 1) registerSpecies(speciesId);
}

void Pet::wipeGameKeepSettings() {
  // ko11.8: "bgmMask" = fondos elegidos en la pantalla de sonido (tambien es ajuste)
  // ko11.23.3: "bri" (brillo) y "chg" (limite de carga) tambien son ajustes
  static const char *const KEEP_U8[] = { "volBgm", "volCry", "volSfx", "lang", "bgmMask", "bri", "chg", "vib", "vibLv", "bdm", "bdd" };  // ko12.6: cumpleanos  // ko11.26: fuerza de la vibracion
  const int NK = sizeof(KEEP_U8) / sizeof(KEEP_U8[0]);
  uint8_t u8[NK];
  bool has[NK];
  for (int i = 0; i < NK; i++) {
    has[i] = prefs.isKey(KEEP_U8[i]);
    u8[i] = prefs.getUChar(KEEP_U8[i], 0);
  }
  bool hasSnd = prefs.isKey("snd"), snd = prefs.getBool("snd", true);
  uint32_t seen = prefs.getUInt("seen", 0);
  prefs.clear();
  for (int i = 0; i < NK; i++)
    if (has[i]) prefs.putUChar(KEEP_U8[i], u8[i]);
  if (hasSnd) prefs.putBool("snd", snd);
  if (seen) prefs.putUInt("seen", seen);
}

// ---------------------------------------------------------------- batallas

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
  if ((won || caught) && !life.firstWin) life.firstWin = lastSeenEpoch ? lastSeenEpoch : 1;  // ko12.4
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
      wildWins++;  // ko11.1: los objetos los da la pantalla (wildWinItems / premios propios)
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

// ko11.19: lo que no cabe ya no se pierde: una ball de mas se vuelve pocion (y al
// reves) si hay sitio, y si las dos estan llenas, un trozo de caramelo raro
void Pet::giveItems(uint8_t b, uint8_t p) {
  uint16_t shards = 0;
  for (uint8_t i = 0; i < b; i++) {
    if (balls < BALL_MAX) balls++;
    else if (potions < POTION_MAX) potions++;
    else shards++;
  }
  for (uint8_t i = 0; i < p; i++) {
    if (potions < POTION_MAX) potions++;
    else if (balls < BALL_MAX) balls++;
    else shards++;
  }
  if (shards) addShards(shards);
}

void Pet::addShards(uint16_t n) {
  uint32_t sh = (uint32_t)rareShards + n;
  while (sh >= SHARDS_PER_RARE) {
    sh -= SHARDS_PER_RARE;
    if (rareCandy < 999) rareCandy++;
  }
  rareShards = (uint16_t)sh;
}

uint8_t Pet::releaseGift(int16_t dex, uint16_t *orbOut) {
  if (orbOut) *orbOut = 0;
  uint32_t r = random(100);
  uint8_t g;
  if (r < 35) g = RG_BALL;
  else if (r < 65) g = RG_POTION;
  else if (r < 90) g = RG_SHARD;
  else if (r < 97) g = RG_EXP;
  else g = RG_ORB;
  if (g == RG_EXP && isEgg()) g = RG_SHARD;
  // lleno: ball <-> pocion; las dos llenas: trozo
  if (g == RG_BALL && balls >= BALL_MAX) g = potions < POTION_MAX ? RG_POTION : RG_SHARD;
  else if (g == RG_POTION && potions >= POTION_MAX) g = balls < BALL_MAX ? RG_BALL : RG_SHARD;
  switch (g) {
    case RG_BALL: balls++; break;
    case RG_POTION: potions++; break;
    case RG_SHARD: addShards(1); break;
    case RG_EXP: addExp(20 + level() * 2); break;
    case RG_ORB: {
      uint16_t o = orbMake(DEX_TBL[dex].ptype, random(2) != 0,
                           (uint8_t)(ORB_MIN_PCT + random(ORB_MAX_PCT - ORB_MIN_PCT + 1)));
      gainOrb(o);
      if (orbOut) *orbOut = o;
      break;
    }
  }
  save();
  return g;
}

uint8_t Pet::synthOrbs(const uint8_t idx[3], uint16_t &out) {
  out = 0;
  if (isEgg() || speciesId < 1) return 0;
  uint8_t a = idx[0], b = idx[1], c = idx[2];
  if (a >= orbN || b >= orbN || c >= orbN || a == b || a == c || b == c) return 0;
  uint16_t sum = orbPct(orbBag[a]) + orbPct(orbBag[b]) + orbPct(orbBag[c]);
  // quitar los 3 (de mayor a menor posicion para no mover los otros)
  uint8_t s[3] = { a, b, c };
  for (int i = 0; i < 2; i++)
    for (int j = i + 1; j < 3; j++)
      if (s[j] > s[i]) { uint8_t t = s[i]; s[i] = s[j]; s[j] = t; }
  for (int i = 0; i < 3; i++) {
    for (uint8_t k = s[i]; k + 1 < orbN; k++) orbBag[k] = orbBag[k + 1];
    orbN--;
  }
  if ((int)random(100) < ORB_SYNTH_FAIL_PCT) {  // fallo: consuelo de trozos
    addShards(ORB_SYNTH_FAIL_SHARDS);
    save();
    return 0;
  }
  bool great = (int)random(100) < ORB_SYNTH_GREAT_PCT;
  int pct = great ? ORB_MAX_PCT : (int)(sum / 3) - 3 + (int)random(9);  // media -3 .. +5
  if (pct < ORB_MIN_PCT) pct = ORB_MIN_PCT;
  if (pct > ORB_MAX_PCT) pct = ORB_MAX_PCT;
  bool dual = (int)random(100) < ORB_SYNTH_DUAL_PCT;  // ko12.2.1
  out = dual ? orbMakeDual(DEX_TBL[speciesId].ptype, (uint8_t)pct) : orbMake(DEX_TBL[speciesId].ptype, random(2) != 0, (uint8_t)pct);
  gainOrb(out);  // (guarda)
  return dual ? 3 : great ? 2 : 1;
}

uint8_t Pet::wildWinItems() {
  uint8_t bits = 0;
  if (balls < BALL_MAX && random(100) < WILD_BALL_PCT) { balls++; bits |= 1; }
  if (potions < POTION_MAX && random(100) < WILD_POTION_PCT) { potions++; bits |= 2; }
  return bits;
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
  if (dex < 1 || dex > DEX_COUNT) { newEgg(); return; }
  ceremony = CER_NONE;
  neglectTicks = 0;
  // ko10.10: el huevo que se descarta habia gastado el "shiny UP" de caramelos:
  // se devuelve para el proximo huevo (antes se perdia al elegir de la caja)
  if (eggCharm) { shinyCharm = true; eggCharm = false; }
  speciesId = dex;
  prevSpeciesId = -1;
  shiny = isShiny;
  if (lvl < 1) lvl = 1;
  if (lvl > LEVEL_MAX) lvl = LEVEL_MAX;
  exp = expForLevel(lvl);
  movesNew();  // ko11.31 (si viene de la caja con los suyos, adoptMon los pone despues)
  ageMinutes = 0;
  geneAtk = clampGene(gA);
  geneDef = clampGene(gD);
  geneSpe = clampGene(gS);
  trAtk = trDef = trSpe = 0;
  resetTrainRecords();  // ko11.9.2
  evolvedHere = false;
  fullness = 80; joy = 80; energy = 80; hygiene = 100;
  poops = 0; weight = 0;
  careMistakes = 0; mistakeCooldown = 0;
  mistWhy = MW_NONE; mistEpoch = 0;
  sleeping = false;
  autoSleep = false;
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
  lifeStart(LF_BOX);  // ko12.4
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
  if (t.dex < 1 || t.dex > DEX_COUNT) return false;
  if (!canBattle()) return false;
  speciesId = t.dex;
  prevSpeciesId = -1;
  shiny = t.shiny != 0;
  ageMinutes = t.ageMinutes > 999UL * 60 ? 999UL * 60 : t.ageMinutes;
  exp = expForLevel(lvl > LEVEL_MAX ? LEVEL_MAX : (lvl ? lvl : 1));  // fork KO (ko7)
  movesNew();  // ko11.31
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
  mistWhy = MW_NONE;
  mistEpoch = 0;
  neglectTicks = 0;
  goodTicks = 0;
  evoDeclinedLv = 0;
  farDeclinedAge = 0;
  eatUntil = heartUntil = 0;
  trades++;
  registerSpecies(speciesId);
  lifeStart(LF_TRADE);  // ko12.4
  // evolucion por intercambio (Kadabra, Machoke, Graveler, Haunter y los de gen 2)
  if (tradeEvolves(speciesId)) {
    prevSpeciesId = speciesId;
    speciesId = tradeTarget(speciesId);
    life.evoDex[0] = speciesId;  // ko12.4
    life.evoT[0] = lastSeenEpoch;
    movesNew();  // ko11.31
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
