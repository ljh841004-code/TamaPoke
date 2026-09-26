// TamaPoke - tamagotchi pixel art inspirado en la gen 1
// para Waveshare ESP32-S3-Touch-AMOLED-1.75
//
// Librerias (Library Manager o repo de Waveshare):
//   - "GFX Library for Arduino" (moononournation), con soporte CO5300 QSPI
//   - "SensorLib" (Lewis He), driver tactil CST9217
//
// Placa: ESP32S3 Dev Module | Flash 16MB | PSRAM: OPI PSRAM | USB CDC On Boot: Enabled
//
// Los sprites y la tabla de especies se generan con tools/sprites.py (emit).

#include <Arduino.h>
#include <Wire.h>
#include "Arduino_GFX_Library.h"
#include "TouchDrvCSTXXX.hpp"
#include <U8g2lib.h>  // fuentes CJK (japones); ver applyLangFont()
#include "pin_config.h"
#include "species.h"
#include "dex.h"
#include "pet.h"
#include "sdmon.h"
#include "rtcbat.h"
#include "i18n.h"
#include "audio.h"
#include "battle.h"    // fork KO: batallas
#include "i18n_ext.h"  // fork KO: textos nuevos (KO/EN)
#include "net.h"       // fork KO: WiFi + NTP
#include "link.h"      // fork KO: tongsin ESP-NOW
#include "cji.h"        // fork KO (ko8): teclado coreano cheonjiin
#include "font_ko.h"    // fork KO (ko8): Noto Sans KR suavizada (hangul + ASCII), 16-60 px
#include "weather.h"    // fork KO (ko10.1): estaciones y tiempo segun la fecha
#include "box.h"        // fork KO (ko4): bogwanham y registro de la pokedex
#include "sdupdate.h"   // fork KO (ko5): actualizar desde /update.bin de la SD
#include <qrcode.h>     // fork KO (ko8): QR del portal WiFi (componente espressif/qrcode del core)

// Version del firmware. Subir este numero en cada release (y manifest.json para
// el instalador web). Se muestra en la pantalla de ajustes y por serie al arrancar.
#define FW_VERSION "1.17-ko10.3"
// ko6.2: marca que la pantalla de SD UPDATE busca dentro de update.bin para
// mostrar que version trae el fichero antes de instalarlo (sdUpdateFileVersion)
extern const char TP_VERSION_TAG[];
__attribute__((used)) const char TP_VERSION_TAG[] = UPD_TAG FW_VERSION;

Arduino_DataBus *bus = new Arduino_ESP32QSPI(
  LCD_CS, LCD_SCLK, LCD_SDIO0, LCD_SDIO1, LCD_SDIO2, LCD_SDIO3);
Arduino_CO5300 *panel = new Arduino_CO5300(
  bus, LCD_RESET, 0 /*rotation*/, LCD_WIDTH, LCD_HEIGHT, 6, 0, 0, 0);
// Framebuffer completo en PSRAM: dibujamos todo y hacemos flush() (sin parpadeo).
// fork KO (ko4): subclase solo para leer el color de texto (el hangul Noto se
// pinta a mano en printT() y tiene que usar el mismo color que print()).
class TPCanvas : public Arduino_Canvas {
public:
  using Arduino_Canvas::Arduino_Canvas;
  uint16_t ink() const { return textcolor; }
};
TPCanvas *gfx = new TPCanvas(LCD_WIDTH, LCD_HEIGHT, panel);

TouchDrvCST92xx touch;
// fuente CJK activa: de momento siempre false, la carga llegara con el
// soporte de japones/coreano/chino (ver PR #13)
bool gCjkFont = false;
#define TOUCH_ADDR 0x5A  // CST9217
Pet pet;
Box box;        // fork KO (ko4): Pokemon ganados/capturados
DexLog dexLog;  // fork KO (ko4): historial de la pokedex

// sprite animado de la SD para la especie actual (si existe el archivo)
SdMon mon;          // sprite B/N (respaldo y minijuego si no hay PMD)
PmdMon pmd;         // sprite PMD multi-accion (pantalla principal)
PmdMon evoPmd;      // forma anterior, solo durante el parpadeo de evolucion
int16_t monFor = -2;
bool monShinyFor = false;

// comportamiento del bicho en pantalla
struct {
  uint8_t mode = 0;     // 0 idle, 1 paseo, 2 gesto one-shot
  uint8_t act = PMD_IDLE;
  uint32_t t0 = 0;      // inicio de la animacion en curso
  uint32_t until = 0;   // fin del estado actual
  float x = 233, targetX = 233;
} beh;
#define PET_GROUND 304  // linea de suelo de la mascota
PmdMon galleryPmd;  // sprite grande de la vista detalle de la galeria (PMD/TPK2, legal)

// galeria pokedex
bool galleryOpen = false;
bool galleryDirty = false;
int galleryPage = 0;        // 16 paginas de 16 (ko10: 251 especies)
#define GAL_PAGES ((DEX_COUNT + 15) / 16)
int16_t galleryDetail = 0;  // dex en vista detalle, 0 = rejilla

bool screenOff = false;       // pulsacion corta del boton PWR
bool cardOpen = false;        // ficha del bicho (deslizar vertical)
bool kbOpen = false;          // teclado para renombrar al bicho
char nameBuf[20] = "";   // ko8: lo ya escrito (apodo en hangul: hasta 18 bytes)
uint8_t nameLen = 0;
Cji kbCji;               // ko8: silabas en construccion (teclado cheonjiin)
bool kbKo = true;        // ko8: teclado coreano (true) o alfabeto (false)
uint8_t cardPage = 0;         // 0 perfil, 1 stats+medallas
#define CARD_PAGES 5          // ko10.4: + pagina de caramelos
const char *cardMsg = nullptr;  // ko10.4: aviso breve en la pagina de caramelos
uint32_t cardMsgUntil = 0;
bool clockOpen = false;       // pantalla de ajuste de hora (deslizar abajo)
int clockH = 12, clockM = 0;  // hora en edicion
int clockY = 2026, clockMo = 1, clockD = 1;  // ko10.4: fecha en edicion
bool clockDateMode = false;  // los botones +/- cambian mes/dia en vez de hora/minuto

// escena de bano: espuma sobre el bicho y limpieza al reventar
uint32_t bathUntil = 0;
bool bathPending = false;
struct { int16_t x, y; uint8_t r, ph; } bubbles[14];
uint32_t feedMenuUntil = 0;   // selector de comida abierto hasta este millis

// minijuego "toques": mantener la pokeball en el aire
bool gameOpen = false;
uint32_t gameOverUntil = 0;
uint32_t gameStartMs = 0;        // ko9.2: el juego dura 30 s (y 3 vidas)
#define GAME_MS 30000UL
// ko10.4: 3 pelotas a la vez (antes 1: demasiado facil)
#define GAME_BALLS 3
float ballX[GAME_BALLS], ballY[GAME_BALLS], ballVX[GAME_BALLS], ballVY[GAME_BALLS], gamePetX;
uint32_t lastGameStep = 0;  // ultima llamada a stepGame(): fisica por tiempo real, no por frame
uint8_t gameScore, gameMisses;
float hitX, hitY;             // ultimo golpe (anillo de impacto)
uint32_t hitTime = 0;
bool gameNewHi = false;

// saco de entrenamiento (entrena la fuerza)
bool sackOpen = false;
uint32_t sackUntil = 0, sackOverUntil = 0;
uint16_t sackHits = 0;
float sackShake = 0;
uint8_t sackGain = 0;
bool sackNewHi = false;

// las 9 especies con sprite propio en flash (respaldo sin SD): dex -> indice
int flashIdxForDex(int16_t dex) {
  static const int8_t IDX[10] = { -1, 3, 4, 5, 0, 1, 2, 6, 7, 8 };
  return (dex >= 1 && dex <= 9) ? IDX[dex] : -1;
}

#define CX 233  // centro de la pantalla redonda
#define CY 233
#define PET_CY 202  // centro vertical del sprite

static const uint16_t INK_K = 0x18C4;  // spriteColor('k')

// botones de icono siguiendo el arco inferior de la pantalla redonda
// (los exteriores van mas altos para no salirse del circulo)
struct Btn {
  int16_t cx, cy;
  const char *const *icon;
};
Btn buttons[4] = {
  { 140, 390, SPR_ICON_FOOD },   // comer
  { 202, 404, SPR_ICON_PLAY },   // jugar
  { 264, 404, SPR_ICON_LIGHT },  // luz
  { 326, 390, SPR_ICON_CLEAN },  // bano
};
#define BTN_HALF 26  // boton de 52x52
// fork KO: botones de la pagina de combate de la ficha (ko4: rejilla 2x2)
#define CARD_ROW1_Y 222
#define CARD_ROW2_Y 270
#define CARD_COL1_X 96
#define CARD_COL2_X 236
#define CARD_COL_W 134
#define CARD_BTN_H 40
#define BTN_HIT 36   // radio tactil (un poco mas generoso)

// grietas del huevo (pixeles 'k' sobre el sprite)
static const uint8_t CRACK1[][2] = { {15,8},{16,9},{15,10} };
static const uint8_t CRACK2[][2] = { {11,13},{12,14},{11,15},{20,12},{19,13},{20,14} };
// estrellas del modo noche
static const uint16_t STARS[][2] = { {120,140},{330,120},{370,210},{95,230},{280,90},{160,95} };

bool wasPressed = false;
#define TRAIN_QUIT_MS 2000UL  // ko9.1: mantener 2 s = abandonar un juego de entrenamiento
// eleccion de inicial (primera partida): Bulbasaur / Charmander / Squirtle, 3 filas
static const int16_t STARTER_DEX[3] = { 1, 4, 7 };
#define STARTER_ROW_Y 110
#define STARTER_ROW_H 70
#define STARTER_ROW_GAP 8
// boton-CTA de evolucion (centrado, mitad de pantalla)
#define EVO_BTN_W 256
#define EVO_BTN_H 64
#define EVO_BTN_X (CX - EVO_BTN_W / 2)
#define EVO_BTN_Y 172
// boton-CTA de despedida (mas ancho: lleva el nombre + frase)
#define FAR_BTN_W 408
#define FAR_BTN_H 58
#define FAR_BTN_X (CX - FAR_BTN_W / 2)
#define FAR_BTN_Y 176
// el CST9217 avisa por el pin INT cuando hay datos tactiles; lo usamos para no
// leer el bus I2C mientras el chip esta dormido (esa lectura se colgaba ~1s)
volatile bool gTouchIrq = false;
bool gRtcWasLost = false;  // el RTC arranco sin hora: el NTP aplicara el tiempo apagado
// ko10.4: la hora es de fiar (RTC con hora al arrancar, NTP, puesta a mano o de un
// amigo por tongsin). Solo una hora de fiar se pasa a otro TamaPoke
bool gClockTrusted = false;
void IRAM_ATTR touchIsr() { gTouchIrq = true; }
uint32_t lastRender = 0;
// proteccion del AMOLED: atenuado por inactividad
uint32_t lastInteract = 0;
uint8_t dimStage = 0;        // 0 despierto, 1 atenuado (90s), 2 casi apagado (5min)
bool swallowGesture = false; // el toque que despierta no acciona nada
uint32_t holdStart = 0;     // pulsacion larga sobre el bicho
uint32_t confirmUntil = 0;  // dialogo "soltar?" activo hasta este millis
uint8_t choiceKind = 0;     // dialogo de decision: 0 ninguno, 1 evolucion, 2 despedida
uint32_t choiceUntil = 0;   // se cierra solo a este millis
int16_t tX0, tY0, tXl, tYl; // gesto en curso (inicio y ultima posicion)
uint32_t tStart = 0;
bool holdFired = false;

void setup() {
  Serial.setRxBufferSize(8192);  // la transferencia a SD llega en bloques de 2 KB
  Serial.begin(115200);
  // CRITICO: sin esto, Serial.print BLOQUEA el juego cuando no hay un
  // monitor serie abierto en el host (el bufer TX del USB CDC se llena
  // y nadie lo vacia) -> con timeout 0 los mensajes se descartan
  Serial.setTxTimeoutMs(0);
  // TP_VERSION_TAG + 6 = FW_VERSION; usarla aqui evita que el enlazador la quite
  Serial.printf("TamaPoke fw v%s\n", TP_VERSION_TAG + sizeof(UPD_TAG) - 1);
  loadLang();  // idioma guardado (KO por defecto)
  Wire.begin(IIC_SDA, IIC_SCL);
  // CST9217 (tactil), AXP2101 (PMU) y PCF85063 (RTC) comparten este bus I2C.
  // Red de seguridad para PMU/RTC (SensorLib NO respeta este timeout en el
  // tactil; el cuelgue del tactil dormido se resuelve gateando por INT, ver
  // handleTouch).
  Wire.setTimeOut(50);

  // CRITICO: encender la alimentacion del panel (BLDO1=OLED VDD 3.3V) ANTES de
  // inicializar el display. Si el PMU se reseteo (drenaje total), este rail
  // queda OFF y la pantalla se ve negra aunque el resto de la placa funcione.
  pmuEnablePanel();

  // QSPI a 80MHz (por defecto 40): el flush del framebuffer es el cuello de
  // botella del fps (~56ms a 40MHz). Si el panel mostrara basura, bajar a 40M.
  if (!gfx->begin(80000000)) Serial.println("gfx->begin() fallo");
  panel->setBrightness(180);
  applyLangFont();  // fuente del idioma guardado (clasica salvo CJK)

  touch.setPins(TP_RESET, TP_INT);
  bool touchOk = false;
  for (int i = 0; i < 3 && !touchOk; i++) {  // a veces falla al primer intento
    touchOk = touch.begin(Wire, 0x5A, IIC_SDA, IIC_SCL);
    if (!touchOk) delay(150);
  }
  if (!touchOk) Serial.println("CST9217 no detectado");
  // begin() deja el chip en modo comando (lee la identidad y no sale);
  // hace falta un reset por hardware para que vuelva a reportar toques
  touch.reset();
  touch.setMaxCoordinates(LCD_WIDTH, LCD_HEIGHT);
  touch.setMirrorXY(true, true);  // el panel esta montado girado 180 grados
  // INT activo-bajo: salta cuando hay datos. Gatea las lecturas I2C (ver loop)
  pinMode(TP_INT, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(TP_INT), touchIsr, FALLING);

  pet.begin();
  box.begin();
  dexLog.begin();
  pet.nextPetHook = nextFromBox;  // tras la despedida, el siguiente sale de la caja
  sdBegin();
  thumbs.load();

  // reloj real: aplica el tiempo que estuvo apagado
  rtcBegin();
  batBegin();
  pwrSetup();
  uint32_t e = rtcEpoch();
  gClockTrusted = e != 0;
  if (e == 0) {
    // Sin pila de respaldo, el PCF85063 pierde la hora al cortar la
    // alimentacion. Sembrar siempre la fecha fija haria RETROCEDER el tiempo de
    // juego respecto a lo ya guardado, y eso rompe la racha, la edad y la hora
    // de la escena. Se siembra con la ultima hora vista, si es posterior, para
    // que el tiempo no vaya nunca hacia atras. Los minutos apagado se pierden
    // igual (sin RTC no hay forma de saberlos), pero nada se descuadra.
    uint32_t seed = 1767225600UL;             // RTC virgen de verdad
    uint32_t seen = pet.savedEpoch();         // usa la NVS que pet ya tiene abierta
    if (seen > seed) seed = seen;
    rtcSetEpoch(seed);
    e = rtcEpoch();
    // fork KO (ko7): solo si habia una hora guardada de verdad se sabe desde
    // cuando aplicar el tiempo apagado. Con la fecha fija de siembra, el NTP
    // aplicaba meses de "ausencia" (tope 2 semanas = Lv338 con el nivel viejo)
    gRtcWasLost = seen > 1767225600UL;
    Serial.printf("RTC sin hora: sembrado en %u%s\n", seed,
                  seen > 1767225600UL ? " (desde la ultima hora guardada)" : "");
  }
  pet.syncClock(e);

  netBegin();    // WiFi/NTP: la primera sincronizacion va sola a los pocos segundos
  audioBegin();  // ES8311 + I2S + amplificador (suena un jingle de arranque)
  if (sdReady) audioLoadMusic();  // /mons/bgm.wav y /mons/battle_wild.wav si existen

  lastInteract = millis();
}

// carga/descarga el sprite de SD cuando cambia la especie
void ensureMon() {
  if (pet.speciesId == monFor && monShinyFor == pet.shiny && !sdDirty) return;
  // los ficheros recien recibidos pueden incluir un thumbs.bin nuevo, que se
  // cargaba solo en setup(): sin esto no se veia en la galeria hasta reiniciar
  if (sdDirty) { thumbs.unload(); thumbs.load(); }
  sdDirty = false;
  monFor = pet.speciesId;
  monShinyFor = pet.shiny;
  mon.unload();
  pmd.unload();
  beh.x = beh.targetX = 233;
  beh.mode = 0;
  beh.until = 0;
  if (pet.speciesId >= 1 && pet.speciesId <= DEX_COUNT) {
    pmd.load(pet.speciesId, pet.shiny);          // principal: PMD
    if (!pmd.loaded) mon.load(pet.speciesId, pet.shiny);  // respaldo: B/N
  }
}

// ko9: aviso sonoro UNA vez cuando hace caca o cuando una barra (comida,
// animo, energia, limpieza) baja a 30 o menos. Solo con el sonido activado,
// despierto y con la pantalla encendida: dormido o con la pantalla apagada
// (boton PWR) no suena, y ese aviso no se repite luego. Una barra vuelve a
// poder avisar cuando sube de 30.
#define ALERT_LOW 30
void careAlert() {
  static bool init = false;
  static uint8_t lastPoops = 0, lowMask = 0;
  uint8_t v[4] = { pet.fullness, pet.joy, pet.energy, pet.hygiene };
  uint8_t mask = 0;
  for (int i = 0; i < 4; i++)
    if (v[i] <= ALERT_LOW) mask |= 1 << i;
  bool quiet = pet.isEgg() || pet.ceremony != CER_NONE || pet.sleeping || screenOff;
  bool fire = init && !quiet && (pet.poops > lastPoops || (mask & ~lowMask));
  init = true;
  lastPoops = pet.poops;
  lowMask = mask;
  if (fire && audioEnabled()) sfxPlay(SFX_ALERT);
}

void loop() {
  uint32_t now = millis();
  pet.update(now);

  // el amplificador sigue al estado de sueno (la llamada sale sola si no cambia).
  // Aqui cubre todas las vias: el boton de luz, el sueno nocturno y el que llega
  // aplicado desde la progresion offline.
  audioSetSleeping(pet.sleeping);

  // avisa con un sonido cuando el bicho pasa a estar listo para evolucionar
  // (incluye el caso de cumplir al despertar). canEvolveNow es false durmiendo.
  static bool wasEvoReady = false;
  bool evoReady = pet.wantEvolveButton();
  if (evoReady && !wasEvoReady) sfxPlay(SFX_MEDAL);
  wasEvoReady = evoReady;
  // aviso sombrio cuando el bicho esta a punto de escaparse por abandono
  static bool wasRunReady = false;
  bool runReady = pet.canRunawayNow();
  if (runReady && !wasRunReady) sfxPlay(SFX_DENY);
  wasRunReady = runReady;
  careAlert();  // ko9: caca nueva o barra baja: un aviso

  handleTouch();
  handleSerial();
  extraLoop(now);  // fork KO: red, tongsin, batallas (ui_extra.ino)
  ensureMon();
  static int16_t crySpecies = -1;  // grito al nacer/evolucionar/cambiar (/mons/cryNNN.wav)
  if (!pet.isEgg() && pet.speciesId != crySpecies) {
    crySpecies = pet.speciesId; audioCry(pet.speciesId);
  }

  // pulsacion corta del PWR: pantalla on/off. fork KO (ko4): las dos guardan
  // ya (la larga llega antes de que el PMU corte la corriente a los 4 s)
  static uint32_t lastPwr = 0;
  if (now - lastPwr > 250) {
    lastPwr = now;
    uint8_t pw = pwrPoll();
    if (pw & 1) {
      screenOff = !screenOff;
      if (!screenOff) lastInteract = now;
      pet.saveNow();
    }
    if (pw & 2) {
      pet.lastSeenEpoch = clockEpoch();
      pet.saveNow();
      Serial.println("PWR largo: guardado");
    }
  }

  // musica de combate mientras dura una batalla (salvaje o tongsin); el
  // resultado, la huida y la pantalla apagada vuelven a la BGM normal
  audioSetBattleMusic(battleMusicActive() && !screenOff);
  audioSetMusicPaused(screenOff);  // ko5: pantalla apagada = sin musica
  updateBrightness(now);

  // fork KO (ko4): guardado en tiempo real. tick() lo marca cada minuto de
  // juego y aqui se vuelca en el acto, con la pantalla encendida o no. Antes
  // esperaba a que se atenuara, y un corte de luz en pleno uso perdia minutos.
  // Solo escribe las ~12 claves que cambia el paso del tiempo (ver flushSave):
  // el paron es de milisegundos y el desgaste de la flash, asumible.
  if (pet.savePending()) pet.flushSave();

  // anota la hora real cada 30 s (se persiste en cada save del juego)
  static uint32_t lastClock = 0;
  if (now - lastClock > 30000) {
    lastClock = now;
    uint32_t e = rtcEpoch();
    if (e) pet.lastSeenEpoch = e;
  }

  // latido de salud cada 5 min (para el soak test; se descarta si no hay monitor)
  static uint32_t lastHealth = 0;
  if (now - lastHealth > 300000) {
    lastHealth = now;
    Serial.printf("HEALTH up=%lus heap=%u min=%u bat=%d%% mv=%d chg=%d usb=%d dim=%u off=%d\n",
                  (unsigned long)(now / 1000), ESP.getFreeHeap(), ESP.getMinFreeHeap(),
                  batPercent(), batMillivolts(), batCharging() ? 1 : 0,
                  usbPresent() ? 1 : 0, dimStage, screenOff ? 1 : 0);
  }

  // 85 ms en juego/saco: margen seguro para que el redibujado no pise el envio
  // DMA del frame anterior (a 40-65 ms solapaba y causaba flashes negros; con
  // sprites grandes el dibujo tarda mas, asi que se deja colchon)
  // Con la pantalla apagada no se dibuja: a brillo 0 no se ve nada, pero el
  // redibujado y el flush DMA del framebuffer de 466x466 seguian corriendo a
  // 10 fps, y medido son 70 ms de trabajo por cada 100 (46 de ellos solo el
  // volcado). Es la mayor carga evitable de la placa. Cada render repinta la
  // escena entera, asi que no queda nada a medias; y al quedarse lastRender
  // congelado, el primer frame tras despertar sale en el acto.
  if (!screenOff && now - lastRender >= (uint32_t)((gameOpen || sackOpen || trainingFast()) ? 85 : 100)) {
    lastRender = now;
    render();
  }
}

// brillo segun sueno + inactividad (proteccion del AMOLED)
void updateBrightness(uint32_t now) {
  // los eventos visibles despiertan la pantalla solos
  if (pet.evolving() || pet.ceremony || pet.eating() || pet.showHeart()) {
    lastInteract = now;
  }
  uint32_t idle = now - lastInteract;
  dimStage = (idle > 300000) ? 2 : (idle > 90000) ? 1 : 0;
  uint8_t target = pet.sleeping ? 25 : (usbPresent() ? 180 : 145);
  if (dimStage == 1) target = pet.sleeping ? 10 : 60;
  else if (dimStage == 2) target = 8;
  if (screenOff) target = 0;
  static uint8_t current = 255;
  if (target != current) {
    current = target;
    panel->setBrightness(target);
  }
}

// ---------- consola serie (provision de SD + depuracion) ----------

void handleSerial() {
  if (!Serial.available()) return;
  String line = Serial.readStringUntil('\n');
  line.trim();
  if (line.length() == 0) return;
  if (sdSerialCommand(line)) return;
  if (netSerialCommand(line)) return;

  if (line.startsWith("VOL")) {  // fork KO: VOL [bgm cry sfx] en 0..100
    int v[3], n = sscanf(line.c_str() + 3, "%d %d %d", &v[0], &v[1], &v[2]);
    if (n == 3)
      for (int i = 0; i < 3; i++) audioSetVolume(i, (uint8_t)constrain(v[i], 0, 100));
    Serial.printf("vol bgm=%u cry=%u sfx=%u\n", audioVolume(0), audioVolume(1), audioVolume(2));
    Serial.println(n == 3 || n <= 0 ? "DONE" : "ERR");
    return;
  }
  if (line == "WILD") {         // fork KO: batalla salvaje ya
    startWild();
    Serial.println("DONE");
  } else if (line.startsWith("WILD ")) {  // ko10.1: WILD n = salvaje en la region n (0-15)
    startWildIn((uint8_t)line.substring(5).toInt());
    Serial.println("DONE");
  } else if (line == "ALERT") {  // fork KO: aviso de salvaje en pantalla
    triggerWildAlert();
    Serial.println("DONE");
  } else if (line == "HATCH") {
    pet.eggTap(); pet.eggTap(); pet.eggTap();
    Serial.println("DONE");
  } else if (line.startsWith("SPEC ")) {
    int n = line.substring(5).toInt();
    if (n >= 1 && n <= DEX_COUNT) {
      pet.prevSpeciesId = pet.speciesId;
      pet.speciesId = n;
      Serial.printf("especie #%d %s\n", n, DEX_TBL[n].name);
    }
    Serial.println("DONE");
  } else if (line.startsWith("LVL ")) {
    pet.exp = expForLevel((uint16_t)line.substring(4).toInt());  // fork KO (ko7)
    Serial.println("DONE");
  } else if (line.startsWith("TIME ")) {
    uint32_t e = (uint32_t)line.substring(5).toInt();
    rtcSetEpoch(e);
    pet.setClock(e);
    Serial.printf("rtc=%u\n", rtcEpoch());
    Serial.println("DONE");
  } else if (line.startsWith("RTCSET ")) {  // solo RTC (simular apagados en pruebas)
    rtcSetEpoch((uint32_t)line.substring(7).toInt());
    Serial.printf("rtc=%u\n", rtcEpoch());
    Serial.println("DONE");
  } else if (line == "TIME") {
    Serial.printf("rtc=%u\n", rtcEpoch());
    Serial.println("DONE");
  } else if (line == "GAL") {
    galleryOpen = !galleryOpen;
    galleryDetail = 0;
    galleryDirty = true;
    if (!galleryOpen) galleryPmd.unload();
    Serial.println("DONE");
  } else if (line == "EGGS") {
    // simula 20 tiradas de huevo (no cambia el estado del juego)
    for (int i = 0; i < 20; i++) {
      int16_t d = pet.pickEggSpecies();
      Serial.printf("%d:%s(r%u) ", d, DEX_TBL[d].name, DEX_TBL[d].rarity);
    }
    Serial.println();
    Serial.println("DONE");
  } else if (line == "SHINY") {  // alterna shiny del actual (pruebas)
    pet.shiny = !pet.shiny;
    Serial.printf("shiny=%d\n", pet.shiny);
    Serial.println("DONE");
  } else if (line.startsWith("NICK ")) {
    pet.rename(line.substring(5).c_str());
    Serial.printf("nick=%s\n", pet.nick);
    Serial.println("DONE");
  } else if (line == "CAREDAY") {  // simula un dia nuevo cuidado (pruebas)
    pet.setClock(pet.lastSeenEpoch + 86400);
    pet.caress();
    Serial.printf("streak=%u bond=%u medals=0x%X\n", pet.streak, pet.bond, pet.medals);
    Serial.println("DONE");
  } else if (line == "BYE") {
    pet.startFarewell();
    Serial.println("DONE");
  } else if (line == "RUN") {
    pet.startRunaway();
    Serial.println("DONE");
  } else if (line.startsWith("CRY ")) {  // ko9.1: probar un grito de la SD (CRY 25)
    int n = line.substring(4).toInt();
    Serial.printf("/mons/cry%03d.wav\n", n);  // si no suena: LS para ver si esta y su tamano
    audioCry((uint16_t)n);
    Serial.println("DONE");
  } else if (line == "BEEP") {
    sfxPlay(SFX_HATCH);  // prueba de audio
    Serial.println("DONE");
  } else if (line == "ABANDON") {
    pet.dbgRunawayReady();  // fuerza el estado "lista para escaparse" (test del boton)
    Serial.println("DONE");
  } else if (line == "WIPE") {
    pet.factoryReset();     // borra NVS y reinicia -> partida nueva (eleccion de inicial)
    Serial.println("DONE");
    delay(100);
    ESP.restart();
  } else if (line == "REG") {
    Serial.printf("pokedex %u/%u:", pet.registeredCount(), (unsigned)DEX_COUNT);
    for (int i = 1; i <= DEX_COUNT; i++)
      if (pet.isRegistered(i)) Serial.printf(" %d", i);
    Serial.println();
    Serial.println("DONE");
  } else if (line == "HEALTH") {
    Serial.printf("up=%lus heap=%u min=%u sd=%d mon=%d\n",
                  (unsigned long)(millis() / 1000), ESP.getFreeHeap(),
                  ESP.getMinFreeHeap(), sdReady, pmd.loaded || mon.loaded);
    Serial.println("DONE");
  } else if (line == "STATS") {
    Serial.printf("spec=%d nv=%u com=%u fel=%u ene=%u lim=%u desc=%u sd=%d mon=%d bat=%d usb=%d rtc=%u\n",
                  pet.speciesId, pet.level(), pet.fullness, pet.joy, pet.energy,
                  pet.hygiene, pet.careMistakes, sdReady, mon.loaded,
                  batPercent(), usbPresent(), rtcEpoch());
    Serial.printf("peso=%u fue=%u def=%u vel=%u genes=%u/%u/%u tr=%u/%u/%u baya=%d\n",
                  pet.weight, pet.atkStat(), pet.defStat(), pet.speStat(),
                  pet.geneAtk, pet.geneDef, pet.geneSpe,
                  pet.trAtk, pet.trDef, pet.trSpe, pet.berryKnown);
    Serial.printf("shiny=%d streak=%u/%u bond=%u medals=0x%X(%u) nick=%s\n",
                  pet.shiny, pet.streak, pet.bestStreak, pet.bond, pet.medals,
                  pet.totalMedals, pet.nick);
    Serial.println("DONE");
  }
}

// ---------- entrada tactil ----------

bool inPetZone(int16_t x, int16_t y) {
  return x > 110 && x < 356 && y > 95 && y < 310;
}

// el toque se resuelve al LEVANTAR el dedo para distinguir tap de deslizar
void handleTouch() {
  static uint32_t lastPoll = 0;
  if (millis() - lastPoll < 20) return;  // 50 Hz le sobra a un dedo
  lastPoll = millis();
  // solo tocamos el bus si el chip aviso por INT o si el dedo sigue abajo (hay
  // que detectar el levantamiento). Leer el CST9217 dormido se colgaba ~1s y
  // congelaba el loop entero; SensorLib no respeta el timeout de Wire.
  if (!gTouchIrq && !wasPressed) return;
  gTouchIrq = false;
  // Un ciclo de solo direccion antes de leer. Sin esto, getPoint() se colgaba
  // exactamente 1000 ms (el timeout por defecto del driver I2C, que SensorLib no
  // acota) y congelaba el loop entero: es lo que se percibia como "el minijuego
  // se congela 2-3 segundos al tocar la bola" (issue #16), porque ahi los toques
  // son rapidos y seguidos.
  //
  // Medido en placa, jugando lo mismo (~470 lecturas, marcador ~50):
  //   sin esta linea: 5 parones de 1000 ms en 60 s
  //   con ella:       0
  // El contador de rechazos salio 0 en ambos casos, asi que NO funciona
  // saltandose lecturas cuando el chip no contesta: lo que hace es despertarlo,
  // para que la lectura siguiente no se encuentre el CST9217 dormido.
  Wire.beginTransmission(TOUCH_ADDR);
  if (Wire.endTransmission() != 0) return;  // no responde: se reintenta en 20 ms
  int16_t x, y;
  bool pressed = touch.getPoint(&x, &y, 1) > 0;

  // ko9.1: en los juegos de entrenamiento se abandona MANTENIENDO el dedo 2 s
  // quieto (antes: tocar la franja de arriba, y<72). La pokeball de las 12 del
  // juego de velocidad, las que caen en el de defensa y el saco llegan a esa
  // franja, asi que un toque normal a veces cerraba el juego sin guardar nada.
  static uint32_t fastT0 = 0;
  static int16_t fastX0 = 0, fastY0 = 0;
  bool fastGame = sackOpen || trainingFast();
  if (fastGame && pressed && !wasPressed) { fastT0 = millis(); fastX0 = x; fastY0 = y; }
  if (fastGame && pressed && fastT0 && millis() - fastT0 > TRAIN_QUIT_MS &&
      abs(x - fastX0) < 40 && abs(y - fastY0) < 40) {
    fastT0 = 0;
    sackOpen = false;
    trainingQuit();
    sfxPlay(SFX_DENY);
    wasPressed = pressed;
    return;
  }
  if (!pressed) fastT0 = 0;

  // saco de entrenamiento: cada toque cuenta al instante (aporrear rapido)
  if (sackOpen) {
    if (pressed && !wasPressed) {
      lastInteract = millis();
      sackTap();
    }
    wasPressed = pressed;
    return;
  }
  // fork KO (ko4): juegos de defensa y velocidad, tambien al apoyar el dedo
  if (trainingFast()) {
    if (pressed && !wasPressed) {
      lastInteract = millis();
      trainingPress(x, y);
    }
    wasPressed = pressed;
    return;
  }

  if (pressed && !wasPressed) {  // empieza el gesto
    tX0 = tXl = x;
    tY0 = tYl = y;
    tStart = millis();
    holdFired = false;
    swallowGesture = (dimStage > 0) || screenOff;  // si estaba a oscuras, solo despierta
    screenOff = false;
    lastInteract = millis();
    // ko10.3: la pelota se golpea al APOYAR el dedo (antes al levantarlo: con la
    // bola moviendose, el golpe llegaba tarde o se perdia y parecia que el juego
    // se trababa). Mantener 2 s sigue sirviendo para salir.
    if (gameOpen && !swallowGesture && !gameOverUntil) gameTap(x, y);
  } else if (pressed) {  // sigue apoyado
    tXl = x;
    tYl = y;
    // ko9.1: juego de pelota: mantener 2 s = abandonar (antes tocar arriba)
    if (gameOpen && !holdFired && !gameOverUntil && millis() - tStart > TRAIN_QUIT_MS &&
        abs(tXl - tX0) < 40 && abs(tYl - tY0) < 40) {
      holdFired = true;
      gameOpen = false;
      sfxPlay(SFX_DENY);
    }
    // pulsacion larga sin moverse sobre el bicho -> dialogo de soltar
    if (!holdFired && !swallowGesture && !galleryOpen && !cardOpen && !kbOpen && !clockOpen && !extraOpen() &&
        !trainingOpen() &&
        millis() - tStart > 3000 &&
        abs(tXl - tX0) < 30 && abs(tYl - tY0) < 30 && inPetZone(tX0, tY0) &&
        !pet.isEgg() && !confirmUntil && !pet.ceremony) {
      confirmUntil = millis() + 10000;
      holdFired = true;
    }
  } else if (wasPressed) {  // levanta el dedo: resolver gesto
    lastInteract = millis();
    int dx = tXl - tX0, dy = tYl - tY0;
    uint32_t dt = millis() - tStart;
    if (!holdFired && !swallowGesture) {
      if (abs(dx) > 80 && abs(dy) < 70 && dt < 800) onSwipe(dx > 0 ? 1 : -1);
      else if (abs(dy) > 80 && abs(dx) < 70 && dt < 800) onSwipeV(dy > 0 ? 1 : -1);
      else if (dt < 1500 && abs(dx) < 40 && abs(dy) < 40) onTap(tX0, tY0);
    }
  }
  wasPressed = pressed;
}

// deslizar vertical: abre/cierra la ficha del bicho
void openClock();  // prototipo

void onSwipeV(int dir) {
  if (pet.awaitingStarter()) return;  // bloqueado durante la eleccion de inicial
  if (extraSwipe()) return;           // fork KO: pantallas nuevas
  if (trainingSwipe()) return;        // fork KO (ko4): entrenamiento
  if (gameOpen || galleryOpen || kbOpen || sackOpen || pet.ceremony) return;
  if (clockOpen) { clockOpen = false; return; }
  if (cardOpen) {
    if (dir < 0) cardOpen = false;  // arriba cierra la ficha
    return;
  }
  if (dir > 0) {                    // deslizar abajo: ajustar hora
    if (!confirmUntil && !feedMenuUntil) openClock();
  } else if (!pet.isEgg() && !confirmUntil && !feedMenuUntil) {
    cardOpen = true;                // deslizar arriba: ficha
    cardPage = 0;
  }
}

// deslizar: dir +1 = hacia la derecha
void onSwipe(int dir) {
  if (pet.awaitingStarter()) return;  // bloqueado durante la eleccion de inicial
  if (regionSwipe(dir)) return;       // ko10.1: paginas de regiones
  if (gymSwipe(dir)) return;          // ko10.4: paginas de gimnasios
  if (extraSwipe()) return;           // fork KO: pantallas nuevas
  if (trainMenuSwipe(dir)) return;    // ko9.1: menu de entrenamiento <-> batallas
  if (trainingSwipe()) return;        // fork KO (ko4): entrenamiento
  if (gameOpen || kbOpen || clockOpen) return;
  if (cardOpen) {  // dentro de la ficha: cambiar entre las 4 paginas
    int p = (int)cardPage + (dir > 0 ? -1 : 1);  // izquierda avanza
    cardPage = p < 0 ? 0 : (p > CARD_PAGES - 1 ? CARD_PAGES - 1 : p);
    return;
  }
  if (!galleryOpen) {
    if (!pet.ceremony && !confirmUntil) {
      galleryOpen = true;
      galleryPage = 0;
      galleryDetail = 0;
      galleryDirty = true;
    }
    return;
  }
  if (galleryDetail) {  // en detalle: volver a la rejilla
    galleryDetail = 0;
    galleryPmd.unload();
    galleryDirty = true;
    return;
  }
  int np = galleryPage - dir;  // deslizar a la izquierda avanza pagina
  if (np < 0) {                // retroceder desde la primera = salir
    galleryOpen = false;
    galleryPmd.unload();
    return;
  }
  if (np > GAL_PAGES - 1) np = GAL_PAGES - 1;
  if (np != galleryPage) {
    galleryPage = np;
    galleryDirty = true;
  }
}

void onTap(int16_t x, int16_t y) {
  // Serial.printf("TOUCH %d %d\n", x, y);  // diagnostico (silenciado: satura el log)
  if (pet.awaitingStarter()) {  // primera partida: elegir inicial
    for (int i = 0; i < 3; i++) {
      int ry = STARTER_ROW_Y + i * (STARTER_ROW_H + STARTER_ROW_GAP);
      if (x >= 70 && x <= 396 && y >= ry && y <= ry + STARTER_ROW_H) {
        pet.chooseStarter(STARTER_DEX[i]);
        sfxPlay(SFX_TAP);
        break;
      }
    }
    return;
  }
  if (extraTap(x, y)) return;  // fork KO: red / batalla / tongsin
  if (trainingTap(x, y)) return;  // fork KO (ko4): menu de entrenamiento
  if (galleryOpen) {
    galleryTap(x, y);
    return;
  }
  if (kbOpen) {
    keyboardTap(x, y);
    return;
  }
  if (clockOpen) {
    clockTap(x, y);
    return;
  }
  if (pet.ceremony) return;  // durante la despedida no hay botones
  if (cardOpen) {
    if (cardPage == 4) cardCandyTap(x, y);        // ko10.4: caramelos
    else if (cardPage == 0 && y < 84) openKeyboard();  // tocar el nombre = renombrar
    else if (cardPage == 1 && y >= CARD_ROW1_Y && y < CARD_ROW2_Y + CARD_BTN_H &&
             x >= CARD_COL1_X && x < CARD_COL2_X + CARD_COL_W) {
      bool right = x >= CARD_COL2_X - 3;
      bool row2 = y >= CARD_ROW2_Y - 4;
      if (!row2 && !right) { cardOpen = false; openRegionPick(); }  // salvaje (ko10.1: region)
      else if (!row2) openLinkMenu();                          // tongsin (cierra la ficha)
      else if (!right) openTrainMenu();                        // ko4: entrenamiento
      else openBox();                                          // ko4: caja
    } else {
      cardOpen = false;
    }
    return;
  }
  if (gameOpen) return;  // ko10.3: el golpe ya se dio al apoyar el dedo
  if (choiceKind) {          // dialogo de decision: boton accion (arriba) / mantener (abajo)
    bool b1 = (x >= 93 && x <= 373 && y >= 206 && y <= 258);  // accion
    bool b2 = (x >= 93 && x <= 373 && y >= 268 && y <= 320);  // mantener / quedaros
    if (choiceKind == 1) {                 // evolucion
      if (b1) { int16_t old = pet.speciesId; pet.evolve(); evoPmd.load(old, pet.shiny); }
      else if (b2) pet.declineEvolve();
    } else if (choiceKind == 2) {          // despedida
      if (b1) pet.startFarewell();
      else if (b2) pet.declineFarewell();
    }
    choiceKind = 0;
    return;
  }
  if (confirmUntil) {        // dialogo "soltar?": SI / NO
    if (timeLeft(confirmUntil) && x >= 118 && x <= 218 && y >= 252 && y <= 304) {
      pet.release();
    }
    confirmUntil = 0;
    return;
  }
  if (feedMenuUntil) {       // selector de comida
    if (timeLeft(feedMenuUntil) && y >= 288 && y <= 352 && x >= 101 && x <= 365) {
      int item = (x - 101) / 66;
      if (item == 3) pet.feedCandy();
      else pet.feedBerry(item);
      sfxPlay(SFX_EAT);
    }
    feedMenuUntil = 0;
    return;
  }
  if (pet.isEgg()) {
    pet.eggTap();
    sfxPlay(SFX_TAP);
    return;
  }
  // boton de evolucion: abre el dialogo evolucionar/mantener
  if (pet.wantEvolveButton() && x >= EVO_BTN_X && x <= EVO_BTN_X + EVO_BTN_W &&
      y >= EVO_BTN_Y && y <= EVO_BTN_Y + EVO_BTN_H) {
    choiceKind = 1; choiceUntil = millis() + 12000;
    return;
  }
  // botones de final (mismo recuadro): escapada directa; despedida abre dialogo
  if (x >= FAR_BTN_X && x <= FAR_BTN_X + FAR_BTN_W &&
      y >= FAR_BTN_Y && y <= FAR_BTN_Y + FAR_BTN_H) {
    if (pet.canRunawayNow()) { pet.startRunaway(); return; }
    if (pet.wantFarewellButton()) { choiceKind = 2; choiceUntil = millis() + 12000; return; }
  }
  if (wildAlertTap(x, y)) return;  // fork KO: aviso de salvaje -> batalla
  for (int i = 0; i < 4; i++) {
    int dx = x - buttons[i].cx, dy = y - buttons[i].cy;
    if (dx * dx + dy * dy <= BTN_HIT * BTN_HIT) {
      Serial.printf("BTN %d\n", i);
      sfxPlay(SFX_TAP);
      if (i == 0) {
        if (!pet.sleeping) feedMenuUntil = millis() + 6000;
      } else if (i == 1) {
        openTrainMenu();  // fork KO (ko4): elegir entrenamiento (antes: la pelota)
      } else if (i == 2) {
        pet.toggleLight();
      } else {
        startBath();
      }
      return;
    }
  }
  // tocar al bicho = caricia
  if (inPetZone(x, y)) {
    Serial.println("PET");
    pet.caress();
    if (!pet.sleeping) { sfxPlay(SFX_HEART); audioCry(pet.speciesId); }
  }
}

// ---------- render ----------

bool gNight = false;  // noche real (por hora) o durmiendo: lo fija render()
uint16_t inkColor() { return gNight ? UI_INK_NIGHT : UI_INK; }

// ---------- escena de fondo: bioma del tipo + hora real del RTC ----------

#define C565(r, g, b) ((uint16_t)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3)))
#define HORIZON 232  // linea donde el cielo se encuentra con el suelo

uint16_t lerp565(uint16_t a, uint16_t b, int i, int n) {
  if (n <= 0) return a;
  int ar = (a >> 11) & 31, ag = (a >> 5) & 63, ab = a & 31;
  int br = (b >> 11) & 31, bg = (b >> 5) & 63, bb = b & 31;
  return (uint16_t)((((ar + (br - ar) * i / n) << 11)) |
                    (((ag + (bg - ag) * i / n) << 5)) | (ab + (bb - ab) * i / n));
}

// hora del dia 0-23 (de la hora real cacheada cada 30s; 13 si no hay reloj)
int sceneHour() {
  uint32_t e = pet.lastSeenEpoch;
  return e ? (int)((e / 3600) % 24) : 13;
}

// suelo de cada escenario de dia (de noche se mezcla hacia el azul nocturno).
// ko10.1: uno por tipo (tools/gen_dex.py TYPE_BIOME)
#define BIOME_N 16
static const uint16_t BIOME_SOIL[BIOME_N] = {
  C565(0x7e, 0xc0, 0x7f),  //  0 pradera (normal)
  C565(0xdc, 0xca, 0x94),  //  1 playa (agua)
  C565(0x4f, 0x8a, 0x55),  //  2 bosque (planta)
  C565(0x8a, 0x55, 0x44),  //  3 volcan (fuego)
  C565(0xa8, 0x90, 0x6a),  //  4 montana (roca)
  C565(0xe6, 0xee, 0xf5),  //  5 nieve (hielo)
  C565(0xa6, 0xb0, 0x78),  //  6 central electrica (electrico)
  C565(0xc4, 0xa5, 0x74),  //  7 dojo (lucha)
  C565(0x76, 0x6c, 0x84),  //  8 pantano (veneno)
  C565(0xe8, 0xc6, 0x80),  //  9 desierto (tierra)
  C565(0xd4, 0xc4, 0xdc),  // 10 ruinas (psiquico)
  C565(0x8c, 0xcc, 0x66),  // 11 jardin (bicho)
  C565(0x5e, 0x5a, 0x6e),  // 12 cementerio (fantasma)
  C565(0x6a, 0x96, 0x78),  // 13 valle del dragon (dragon)
  C565(0x86, 0x88, 0x92),  // 14 ciudad (siniestro)
  C565(0x7a, 0x6e, 0x66),  // 15 mina (acero)
};

void drawClouds(uint32_t now, uint16_t col) {
  for (int k = 0; k < 2; k++) {
    int cx = (int)((now / 50 + k * 250) % 560) - 40;
    int cy = 70 + k * 34;
    gfx->fillCircle(cx, cy, 16, col);
    gfx->fillCircle(cx + 18, cy + 3, 13, col);
    gfx->fillCircle(cx - 15, cy + 4, 12, col);
  }
}

uint16_t gSkyTop = 0, gSkyBot = 0;  // fork KO (ko4): el reloj grande se tine con el cielo

// ko10.1: tiempo de ahora (lluvia a ratos, nieve solo en invierno, sol de verano)
uint8_t sceneWeather() { return weatherAt(pet.lastSeenEpoch); }

static uint16_t nightDim(uint16_t c, bool night) {
  return night ? lerp565(c, C565(0x16, 0x1c, 0x30), 9, 16) : c;
}

// colores del cielo segun la hora y el tiempo
void skyColors(int h, bool night, uint8_t wx, uint16_t &top, uint16_t &bot) {
  bool wet = wx == WX_RAIN || wx == WX_SNOW;
  if (night) {
    if (wet) { top = C565(0x10, 0x12, 0x18); bot = C565(0x26, 0x2a, 0x36); }
    else     { top = C565(0x0c, 0x12, 0x24); bot = C565(0x1e, 0x26, 0x46); }
  } else if (wx == WX_RAIN) { top = C565(0x6e, 0x7a, 0x88); bot = C565(0xa8, 0xb2, 0xbc); }
  else if (wx == WX_SNOW)   { top = C565(0xa4, 0xb0, 0xc0); bot = C565(0xe2, 0xe8, 0xee); }
  else if (h < 8)  { top = C565(0xd1, 0x6a, 0x86); bot = C565(0xf3, 0xb8, 0x7c); }  // amanecer
  else if (h < 18) {
    if (wx == WX_SUNNY) { top = C565(0x3a, 0x9c, 0xe8); bot = C565(0xb8, 0xe4, 0xf6); }  // verano
    else                { top = C565(0x8f, 0xc8, 0xea); bot = C565(0xdc, 0xee, 0xe6); }  // dia
  } else { top = C565(0xc7, 0x5a, 0x4a); bot = C565(0xf0, 0xae, 0x64); }  // atardecer
}

// cielo: bandas de color y, si "astros", sol/luna/estrellas (en batalla no:
// ahi estan las fichas de vida y el rival)
void drawSky(int hor, int h, bool night, uint8_t wx, uint32_t now, bool astros) {
  uint16_t top, bot;
  skyColors(h, night, wx, top, bot);
  gSkyTop = top; gSkyBot = bot;
  for (int y = 0; y < hor; y += 8)
    gfx->fillRect(0, y, 466, (y + 8 > hor) ? hor - y : 8, lerp565(top, bot, y, hor));
  if (wx == WX_RAIN || wx == WX_SNOW) {  // nubarrones, sin sol ni luna
    uint16_t c1 = night ? C565(0x2c, 0x30, 0x3a) : (wx == WX_RAIN ? C565(0x5c, 0x64, 0x70) : C565(0xc4, 0xcc, 0xd6));
    drawClouds(now / 2 + 9000, lerp565(c1, top, 6, 16));
    drawClouds(now, c1);
    return;
  }
  if (!astros) {
    if (!night && h >= 8 && h < 18) drawClouds(now, C565(0xff, 0xff, 0xff));
    return;
  }
  if (night) {
    gfx->fillCircle(360, 78, 24, C565(0xe8, 0xee, 0xf5));
    gfx->fillCircle(370, 72, 22, lerp565(top, bot, 78, hor));  // creciente
    for (auto &st : STARS)
      if (st[1] + 4 < hor) gfx->fillRect(st[0], st[1], 4, 4, UI_WHITE);
  } else if (h < 18) {
    if (wx == WX_SUNNY && h >= 8) {  // sol de verano: grande y con rayos que giran
      uint16_t ray = C565(0xff, 0xe0, 0x70);
      float a0 = (now % 12000) / 12000.0f * 6.2832f;
      for (int k = 0; k < 12; k++) {
        float a = a0 + k * 0.5236f;
        float c = cosf(a), s = sinf(a);
        int x0 = 360 + (int)(c * 40), y0 = 84 + (int)(s * 40);
        int x1 = 360 + (int)(c * 56), y1 = 84 + (int)(s * 56);
        gfx->drawLine(x0, y0, x1, y1, ray);
        gfx->drawLine(x0 + 1, y0, x1 + 1, y1, ray);
        gfx->drawLine(x0, y0 + 1, x1, y1 + 1, ray);
      }
      gfx->fillCircle(360, 84, 34, C565(0xff, 0xf0, 0xa0));
      gfx->fillCircle(360, 84, 30, C565(0xff, 0xd6, 0x4a));
    } else {
      gfx->fillCircle(360, 84, 26, h < 8 ? C565(0xff, 0xd9, 0x8a) : C565(0xff, 0xe7, 0x9f));
      drawClouds(now, C565(0xff, 0xff, 0xff));
    }
  } else {
    gfx->fillCircle(233, hor - 6, 34, C565(0xff, 0xf1, 0xc8));  // sol poniente
  }
}

static void drawPine(int x, int hor, int hgt, uint16_t c) {
  gfx->fillTriangle(x, hor - hgt * 46 / 60, x - 16, hor, x + 16, hor, c);
  gfx->fillTriangle(x, hor - hgt, x - 12, hor - hgt * 28 / 60, x + 12, hor - hgt * 28 / 60, c);
}

static void drawBareTree(int x, int base, uint16_t c) {
  gfx->fillRect(x - 3, base - 50, 6, 52, c);
  gfx->drawLine(x, base - 34, x - 18, base - 52, c); gfx->drawLine(x + 1, base - 34, x - 17, base - 52, c);
  gfx->drawLine(x, base - 42, x + 16, base - 60, c); gfx->drawLine(x + 1, base - 42, x + 17, base - 60, c);
  gfx->drawLine(x, base - 24, x + 14, base - 34, c); gfx->drawLine(x, base - 23, x + 14, base - 33, c);
}

// suelo y detalles de cada escenario, entre "hor" (horizonte) y "bottom"
void drawBiome(uint8_t biome, int hor, int bottom, uint32_t now, bool night, uint8_t wx) {
  if (biome >= BIOME_N) biome = 0;
  uint8_t season = wxSeason(wxMonth(pet.lastSeenEpoch));
  uint16_t soil = BIOME_SOIL[biome];
  if (wx == WX_RAIN) soil = lerp565(soil, C565(0x50, 0x58, 0x60), 4, 16);  // mojado
  if (wx == WX_SNOW) soil = lerp565(soil, C565(0xf4, 0xf8, 0xff), 8, 16);  // nevado
  soil = nightDim(soil, night);
  uint16_t dk = lerp565(soil, C565(0x10, 0x18, 0x20), night ? 11 : 7, 16);

  // ---- al fondo, detras del suelo
  if (biome == 1) {  // playa: franja de mar con olas
    uint16_t sea = night ? C565(0x1c, 0x34, 0x52) : C565(0x4f, 0x96, 0xc4);
    gfx->fillRect(0, hor - 26, 466, 26, sea);
    for (int i = 0; i < 3; i++) {
      int wy = hor - 22 + i * 7;
      uint16_t fc = night ? C565(0x3a, 0x58, 0x78) : C565(0xbf, 0xe6, 0xf5);
      gfx->fillRect(60 + ((now / 60 + i * 30) % 60), wy, 26, 2, fc);
      gfx->fillRect(300 - ((now / 60 + i * 20) % 60), wy, 26, 2, fc);
    }
  } else if (biome == 3) {  // volcan: cono con lava y humo
    uint16_t cone = nightDim(C565(0x5a, 0x3a, 0x34), night);
    gfx->fillTriangle(330, hor - 84, 220, hor, 440, hor, cone);
    gfx->fillRect(316, hor - 84, 28, 6, C565(0xff, 0x6a, 0x2a));
    gfx->fillTriangle(322, hor - 78, 338, hor - 78, 330, hor - 54, C565(0xff, 0x9b, 0x3a));
    uint16_t smoke = night ? C565(0x3a, 0x3e, 0x48) : C565(0x9a, 0x94, 0x94);
    for (int k = 0; k < 3; k++) {
      int t = (int)((now / 40 + k * 26) % 78);
      gfx->fillCircle(330 + t / 3 - k * 4, hor - 92 - t / 2, 7 + t / 10, smoke);
    }
  } else if (biome == 4) {  // montana: cumbres (nevadas en invierno)
    gfx->fillTriangle(140, hor - 50, 60, hor, 220, hor, dk);
    gfx->fillTriangle(330, hor - 38, 250, hor, 410, hor, dk);
    if (season == SEASON_WINTER) {
      uint16_t cap = nightDim(C565(0xf2, 0xf6, 0xfa), night);
      gfx->fillTriangle(140, hor - 50, 124, hor - 40, 156, hor - 40, cap);
      gfx->fillTriangle(330, hor - 38, 314, hor - 30, 346, hor - 30, cap);
    }
  } else if (biome == 6) {  // central electrica: torres y cables
    uint16_t steel = nightDim(C565(0x5a, 0x60, 0x6a), night);
    for (int x : { 90, 380 }) {
      gfx->drawLine(x - 14, hor + 4, x, hor - 80, steel); gfx->drawLine(x - 13, hor + 4, x + 1, hor - 80, steel);
      gfx->drawLine(x + 14, hor + 4, x, hor - 80, steel); gfx->drawLine(x + 13, hor + 4, x - 1, hor - 80, steel);
      gfx->fillRect(x - 22, hor - 62, 44, 3, steel);
      gfx->fillRect(x - 16, hor - 42, 32, 3, steel);
      gfx->drawLine(x - 10, hor - 42, x + 6, hor - 62, steel);
      gfx->drawLine(x + 10, hor - 42, x - 6, hor - 62, steel);
    }
    uint16_t wire = nightDim(C565(0x30, 0x34, 0x3a), night);
    for (int dy : { -61, -41 }) {  // cables con comba
      int y0 = hor + dy;
      gfx->drawLine(0, y0 + 10, 68, y0, wire);
      gfx->drawLine(112, y0, 235, y0 + 16, wire);
      gfx->drawLine(235, y0 + 16, 358, y0, wire);
      gfx->drawLine(402, y0, 466, y0 + 10, wire);
    }
    if ((now / 700) % 4 == 0) {  // chispa
      uint16_t sp = C565(0xff, 0xe8, 0x40);
      int sx = 160, sy = hor - 50;
      gfx->drawLine(sx, sy, sx + 6, sy - 8, sp); gfx->drawLine(sx + 6, sy - 8, sx + 2, sy - 8, sp);
      gfx->drawLine(sx + 2, sy - 8, sx + 8, sy - 16, sp);
    }
  } else if (biome == 7) {  // dojo: casa de madera con tejado y poste
    uint16_t wall = nightDim(C565(0xec, 0xdc, 0xb4), night);
    uint16_t roof = nightDim(C565(0x7a, 0x34, 0x2a), night);
    uint16_t wood = nightDim(C565(0x6a, 0x46, 0x2c), night);
    gfx->fillRect(40, hor - 44, 120, 46, wall);
    gfx->fillTriangle(24, hor - 44, 60, hor - 70, 60, hor - 44, roof);
    gfx->fillRect(60, hor - 70, 80, 26, roof);
    gfx->fillTriangle(176, hor - 44, 140, hor - 70, 140, hor - 44, roof);
    gfx->fillRect(84, hor - 30, 32, 32, wood);  // puerta
    for (int x : { 48, 146 }) gfx->fillRect(x, hor - 44, 6, 46, wood);
    gfx->fillRoundRect(386, hor - 50, 16, 62, 5, wood);
    gfx->fillRect(370, hor - 38, 48, 5, wood);
    gfx->fillRect(374, hor - 22, 40, 5, wood);
  } else if (biome == 9) {  // desierto: dunas
    gfx->fillEllipse(120, hor + 6, 170, 30, lerp565(soil, C565(0xff, 0xf0, 0xc8), 4, 16));
    gfx->fillEllipse(380, hor + 8, 150, 24, lerp565(soil, C565(0x90, 0x60, 0x30), 3, 16));
  } else if (biome == 10) {  // ruinas: columnas de piedra
    uint16_t stone = nightDim(C565(0xf0, 0xea, 0xf4), night);
    uint16_t line = lerp565(stone, C565(0x80, 0x70, 0x90), 6, 16);
    static const int16_t COLX[4] = { 60, 110, 340, 390 };
    for (int k = 0; k < 4; k++) {
      int x = COLX[k];
      int top = hor - (k == 3 ? 36 : 70);  // la ultima, rota
      gfx->fillRect(x, top, 22, hor - top + 2, stone);
      gfx->drawFastVLine(x + 7, top + 4, hor - top - 4, line);
      gfx->drawFastVLine(x + 14, top + 4, hor - top - 4, line);
      gfx->fillRect(x - 4, top - 6, 30, 6, stone);
    }
    gfx->fillRect(52, hor - 84, 86, 8, stone);  // dintel sobre las dos primeras
  } else if (biome == 12) {  // cementerio: arbol seco
    drawBareTree(410, hor + 2, dk);
  } else if (biome == 13) {  // valle del dragon: acantilados y cascada
    uint16_t rock = nightDim(C565(0x7a, 0x84, 0x92), night);
    uint16_t edge = lerp565(rock, UI_WHITE, night ? 2 : 5, 16);
    gfx->fillRect(40, hor - 60, 164, 62, rock);
    gfx->fillTriangle(0, hor, 40, hor - 60, 40, hor, rock);
    gfx->fillRect(40, hor - 60, 164, 5, edge);
    gfx->fillRect(262, hor - 60, 164, 62, rock);
    gfx->fillTriangle(466, hor, 426, hor - 60, 426, hor, rock);
    gfx->fillRect(262, hor - 60, 164, 5, edge);
    uint16_t fall = night ? C565(0x3a, 0x5a, 0x86) : C565(0x9a, 0xd2, 0xf0);
    uint16_t foam = night ? C565(0x70, 0x8c, 0xb0) : UI_WHITE;
    gfx->fillRect(204, hor - 60, 58, 62, fall);
    for (int k = 0; k < 6; k++) {
      int y = hor - 60 + (int)((now / 12 + k * 17) % 52);
      gfx->fillRect(208 + k * 9, y, 3, 10, foam);
    }
  } else if (biome == 14) {  // ciudad: edificios con ventanas
    uint16_t bld = night ? C565(0x1a, 0x1e, 0x2e) : C565(0x5a, 0x60, 0x74);
    uint16_t win = night ? C565(0xff, 0xd8, 0x6a) : C565(0xb4, 0xc8, 0xd8);
    static const int16_t B[][3] = { {10,40,70}, {56,50,100}, {112,36,60}, {316,44,90}, {366,50,120}, {420,46,76} };
    for (auto &b : B) {
      gfx->fillRect(b[0], hor - b[2], b[1], b[2] + 2, bld);
      for (int wy = hor - b[2] + 8; wy < hor - 8; wy += 14)
        for (int wxp = b[0] + 6; wxp + 6 < b[0] + b[1]; wxp += 12)
          if (!night || ((wxp * 7 + wy * 3) % 5) < 3) gfx->fillRect(wxp, wy, 6, 7, win);
    }
  } else if (biome == 15) {  // mina: colina con boca de cueva
    uint16_t hillc = nightDim(C565(0x5c, 0x52, 0x4c), night);
    gfx->fillEllipse(340, hor, 130, 56, hillc);
    gfx->fillEllipse(80, hor, 110, 34, hillc);
    gfx->fillEllipse(340, hor + 2, 32, 34, C565(0x10, 0x0c, 0x0c));
    uint16_t beam = nightDim(C565(0x8a, 0x5c, 0x34), night);
    gfx->fillRect(304, hor - 34, 6, 36, beam);
    gfx->fillRect(370, hor - 34, 6, 36, beam);
    gfx->fillRect(300, hor - 38, 80, 6, beam);
  }

  // ---- suelo
  gfx->fillRect(0, hor, 466, bottom - hor, soil);
  if (biome != 13 && biome != 15) {
    uint16_t hill = lerp565(soil, night ? C565(0x0c, 0x12, 0x24) : C565(0xff, 0xff, 0xff), 3, 16);
    gfx->fillRoundRect(-60, hor - 14, 586, 60, 30, hill);
  }
  if (biome == 13) {  // valle: laguna donde cae la cascada
    uint16_t lake = night ? C565(0x2a, 0x44, 0x6a) : C565(0x6a, 0xb4, 0xdc);
    gfx->fillEllipse(233, hor + 8, 90, 12, lake);
    gfx->fillEllipse(233, hor + 2, 36, 5, night ? C565(0x70, 0x8c, 0xb0) : UI_WHITE);
  }

  // ---- detalles sobre el suelo
  if (biome == 0) {  // pradera: matas de hierba y florecillas
    for (int gx : { 80, 175, 300, 395 })
      for (int b = -1; b <= 1; b++)
        gfx->fillRect(gx + b * 5, hor + 6, 2, 8 + (b == 0 ? 4 : 0), dk);
    uint16_t fl = nightDim(C565(0xff, 0xf6, 0xd0), night);
    for (int fx : { 130, 240, 350 }) gfx->fillCircle(fx, hor + 12, 3, fl);
  } else if (biome == 2) {  // bosque: coniferas en silueta
    for (int tx : { 60, 150, 360, 416 }) drawPine(tx, hor, 60, dk);
  } else if (biome == 3) {  // volcan: rocas y brasas
    gfx->fillTriangle(70, hor, 40, hor + 30, 100, hor + 30, dk);
    gfx->fillTriangle(400, hor + 4, 372, hor + 30, 430, hor + 30, dk);
    for (int e = 0; e < 4; e++)
      gfx->fillRect(120 + e * 70, hor + 8 + (e % 2) * 6, 4, 4, C565(0xff, 0x9b, 0x3a));
  } else if (biome == 5) {  // nieve: abetos escarchados y bloques de hielo
    uint16_t frost = nightDim(C565(0x9c, 0xb8, 0xcc), night);
    for (int tx : { 70, 400 }) drawPine(tx, hor, 56, frost);
    uint16_t ice = nightDim(C565(0xb4, 0xdc, 0xf0), night);
    gfx->fillTriangle(150, hor + 14, 170, hor - 6, 186, hor + 14, ice);
    gfx->fillTriangle(300, hor + 12, 312, hor - 2, 326, hor + 12, ice);
  } else if (biome == 8) {  // pantano: charcos morados, burbujas y arboles secos
    uint16_t goo = nightDim(C565(0x7a, 0x44, 0x9a), night);
    uint16_t bub = nightDim(C565(0xd0, 0x9c, 0xf0), night);
    gfx->fillEllipse(140, hor + 16, 60, 9, goo);
    gfx->fillEllipse(330, hor + 22, 70, 10, goo);
    for (int k = 0; k < 4; k++) {
      int t = (int)((now / 30 + k * 30) % 120);
      if (t < 45) gfx->drawCircle((k & 1 ? 310 : 120) + k * 10, hor + 16 - t / 3, 2 + t / 15, bub);
    }
    drawBareTree(40, hor + 4, dk);
    drawBareTree(430, hor + 4, dk);
  } else if (biome == 9) {  // desierto: cactus
    uint16_t cac = nightDim(C565(0x4a, 0x8a, 0x4e), night);
    for (int x : { 70, 400 }) {
      gfx->fillRoundRect(x - 6, hor - 40, 12, 52, 6, cac);
      gfx->fillRoundRect(x - 20, hor - 26, 8, 18, 4, cac);
      gfx->fillRect(x - 20, hor - 12, 16, 6, cac);
      gfx->fillRoundRect(x + 12, hor - 32, 8, 16, 4, cac);
      gfx->fillRect(x + 4, hor - 20, 16, 6, cac);
    }
  } else if (biome == 10) {  // ruinas: orbes psiquicos flotando
    uint16_t orb = nightDim(C565(0xf0, 0x90, 0xd0), night);
    for (int k = 0; k < 3; k++) {
      int y = hor - 100 + (int)(8 * sinf(now / 600.0f + k * 2.1f));
      gfx->fillCircle(170 + k * 60, y, 5, orb);
    }
  } else if (biome == 11) {  // jardin: arbol y flores de colores
    uint16_t leaf = nightDim(C565(0x3e, 0x8a, 0x44), night);
    uint16_t trunk = nightDim(C565(0x6a, 0x46, 0x2c), night);
    gfx->fillRect(64, hor - 40, 10, 50, trunk);
    gfx->fillCircle(69, hor - 56, 26, leaf);
    gfx->fillCircle(50, hor - 42, 18, leaf);
    gfx->fillCircle(90, hor - 42, 18, leaf);
    static const uint16_t FL[3] = { C565(0xf0, 0x50, 0x60), C565(0xff, 0xd8, 0x40), C565(0xf4, 0x90, 0xd0) };
    for (int k = 0; k < 9; k++) {
      int fx = 130 + k * 34, fy = hor + 8 + (k % 3) * 8;
      gfx->fillCircle(fx, fy, 4, nightDim(FL[k % 3], night));
      gfx->fillCircle(fx, fy, 1, nightDim(C565(0xff, 0xf8, 0xe0), night));
    }
  } else if (biome == 12) {  // cementerio: lapidas y fuegos fatuos
    uint16_t tomb = nightDim(C565(0x9a, 0x98, 0xa6), night);
    for (int k = 0; k < 4; k++) {
      int x = 50 + k * 80, y = hor + (k % 2) * 8;
      gfx->fillRoundRect(x, y - 30, 26, 36, 10, tomb);
      gfx->fillRect(x + 11, y - 24, 4, 14, dk);
      gfx->fillRect(x + 6, y - 20, 14, 4, dk);
    }
    uint16_t wisp = C565(0x8a, 0x9c, 0xff);
    for (int k = 0; k < 2; k++) {
      int x = 150 + k * 170 + (int)(10 * sinf(now / 500.0f + k));
      int y = hor - 44 + (int)(6 * sinf(now / 330.0f + k * 3));
      gfx->fillCircle(x, y + 6, 3, lerp565(wisp, soil, 6, 16));
      gfx->fillCircle(x, y, 5, wisp);
    }
  } else if (biome == 14) {  // ciudad: marcas de la carretera
    uint16_t mark = nightDim(C565(0xf0, 0xe8, 0xc0), night);
    for (int x = 10; x < 466; x += 60) gfx->fillRect(x, hor + 22, 30, 4, mark);
  } else if (biome == 15) {  // mina: vias y brillos de metal
    uint16_t rail = nightDim(C565(0x9c, 0xa4, 0xae), night);
    uint16_t tie = nightDim(C565(0x5a, 0x40, 0x2c), night);
    for (int k = 0; k < 5; k++) {
      int y = hor + 6 + k * 7, half = 10 + k * 5;
      gfx->fillRect(340 - half - 6, y, 2 * half + 12, 3, tie);
    }
    gfx->drawLine(332, hor + 2, 310, hor + 40, rail); gfx->drawLine(333, hor + 2, 311, hor + 40, rail);
    gfx->drawLine(348, hor + 2, 370, hor + 40, rail); gfx->drawLine(347, hor + 2, 369, hor + 40, rail);
    for (int k = 0; k < 3; k++)
      if ((now / 400 + k) % 3 == 0) {
        int x = 60 + k * 70, y = hor + 14 + (k % 2) * 10;
        gfx->drawFastHLine(x - 4, y, 9, UI_WHITE);
        gfx->drawFastVLine(x, y - 4, 9, UI_WHITE);
      }
  }
}

// lluvia o nieve por encima de todo el escenario (0..bottom)
void drawWeather(uint8_t wx, int bottom, uint32_t now, bool night) {
  if (wx == WX_RAIN) {
    uint16_t c = night ? C565(0x6a, 0x80, 0x9a) : C565(0xdc, 0xe6, 0xf0);
    for (int i = 0; i < 36; i++) {
      int x = (int)((i * 53 + now / 12) % 486);
      int y = (int)((i * 97 + now / 2) % (uint32_t)bottom);
      if (y + 14 > bottom) continue;
      gfx->drawLine(x, y, x - 4, y + 14, c);
    }
  } else if (wx == WX_SNOW) {
    uint16_t c = night ? C565(0xb0, 0xba, 0xcc) : UI_WHITE;
    for (int f = 0; f < 28; f++) {
      int sz = (f % 4) ? 3 : 4;
      int fy = (int)((f * 90 + now / (18 + (f % 3) * 4)) % (uint32_t)bottom);
      int fx = (f * 53 + 466 + (int)(10 * sinf(now / 700.0f + f))) % 466;
      if (fy + sz > bottom) continue;
      gfx->fillRect(fx, fy, sz, sz, c);
    }
  } else if (wx == WX_BLOSSOM || wx == WX_LEAVES) {
    // ko10.1: petalos de cerezo (primavera) u hojas de otono, cayendo en diagonal
    static const uint16_t PET[3] = { C565(0xff, 0xb4, 0xd0), C565(0xf4, 0x8c, 0xb4), C565(0xff, 0xd4, 0xe4) };
    static const uint16_t LEAF[3] = { C565(0xe8, 0x6a, 0x2a), C565(0xc8, 0x3a, 0x2a), C565(0xf0, 0xb4, 0x30) };
    bool leaves = wx == WX_LEAVES;
    int n = leaves ? 16 : 24;
    for (int f = 0; f < n; f++) {
      uint32_t sp = leaves ? 26 + (f % 3) * 6 : 30 + (f % 4) * 5;  // ms por pixel
      int fy = (int)((f * 71 + now / sp) % (uint32_t)(bottom + 20)) - 10;
      int fx = (int)((f * 97 + now / (sp * 2) + (uint32_t)(14 * (1 + sinf(now / 600.0f + f)))) % 486) - 10;
      if (fy < 6 || fy + 7 > bottom) continue;
      uint16_t c = leaves ? LEAF[f % 3] : PET[f % 3];
      if (night) c = lerp565(c, C565(0x16, 0x1c, 0x30), 8, 16);
      bool flip = ((now / 350 + f) & 1) != 0;  // gira al caer
      if (leaves) {
        if (flip) { gfx->fillEllipse(fx, fy, 6, 3, c); gfx->drawFastHLine(fx - 6, fy, 12, lerp565(c, 0, 5, 16)); }
        else      { gfx->fillEllipse(fx, fy, 3, 6, c); gfx->drawFastVLine(fx, fy - 6, 12, lerp565(c, 0, 5, 16)); }
      } else {
        if (flip) gfx->fillEllipse(fx, fy, 5, 3, c);
        else      gfx->fillEllipse(fx, fy, 3, 5, c);
      }
    }
  }
}

void drawScene(uint8_t biome, uint32_t now, bool night) {
  uint8_t wx = sceneWeather();
  drawSky(HORIZON, sceneHour(), night, wx, now, true);
  drawBiome(biome, HORIZON, 466, now, night, wx);
  drawWeather(wx, 466, now, night);
}

// ---------- reloj grande de fondo (fork KO, ko4) ----------
// La hora real en el cielo, detras del bicho: digitos de 7 segmentos con trazo
// redondeado (la fuente 5x7 escalada a este tamano se veia a bloques).
static void drawSeg7(int x, int y, int w, int h, int t, uint8_t d, uint16_t col) {
  static const uint8_t SEG[10] = { 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F };
  uint8_t m = SEG[d % 10];
  int hh = h / 2, r = t / 2;
  if (m & 0x01) gfx->fillRoundRect(x + r, y, w - t, t, r, col);                  // arriba
  if (m & 0x02) gfx->fillRoundRect(x + w - t, y + r, t, hh - r + r / 2, r, col);  // arriba dcha
  if (m & 0x04) gfx->fillRoundRect(x + w - t, y + hh, t, hh - r, r, col);        // abajo dcha
  if (m & 0x08) gfx->fillRoundRect(x + r, y + h - t, w - t, t, r, col);          // abajo
  if (m & 0x10) gfx->fillRoundRect(x, y + hh, t, hh - r, r, col);                // abajo izda
  if (m & 0x20) gfx->fillRoundRect(x, y + r, t, hh - r + r / 2, r, col);         // arriba izda
  if (m & 0x40) gfx->fillRoundRect(x + r, y + hh - r, w - t, t, r, col);         // centro
}

#define BIGCLK_Y 134   // ko10.1: bajo la fecha (y 114), que va bajo el mensaje de estado (y 90)
#define BIGDATE_Y 113
#define BIGCLK_W 44
#define BIGCLK_H 76

uint32_t clockEpoch() {  // hora del RTC, leida como mucho una vez por segundo
  static uint32_t at = 0, e = 0;
  uint32_t now = millis();
  if (!at || now - at >= 1000) { at = now ? now : 1; uint32_t r = rtcEpoch(); if (r) e = r; }
  return e ? e : pet.lastSeenEpoch;
}

void drawBigClock(bool night) {
  uint32_t e = clockEpoch();
  if (!e) return;
  int hh = (e / 3600) % 24, mm = (e / 60) % 60;
  uint16_t sky = lerp565(gSkyTop, gSkyBot, BIGCLK_Y + BIGCLK_H / 2, HORIZON);
  // claro y algo transparente: se lee bien, pero sigue siendo "fondo"
  uint16_t col = night ? lerp565(sky, UI_INK_NIGHT, 7, 16) : lerp565(sky, UI_WHITE, 12, 16);
  const int gap = 12, colon = 20, t = 10;
  int total = 4 * BIGCLK_W + 2 * gap + colon + 2 * gap - gap;  // HH : MM
  int x = CX - total / 2, y = BIGCLK_Y;
  drawSeg7(x, y, BIGCLK_W, BIGCLK_H, t, hh / 10, col); x += BIGCLK_W + gap;
  drawSeg7(x, y, BIGCLK_W, BIGCLK_H, t, hh % 10, col); x += BIGCLK_W + gap / 2;
  if ((e % 2) == 0) {  // los dos puntos parpadean al segundo
    gfx->fillCircle(x + colon / 2, y + BIGCLK_H / 3, 5, col);
    gfx->fillCircle(x + colon / 2, y + BIGCLK_H * 2 / 3, 5, col);
  }
  x += colon + gap / 2;
  drawSeg7(x, y, BIGCLK_W, BIGCLK_H, t, mm / 10, col); x += BIGCLK_W + gap;
  drawSeg7(x, y, BIGCLK_W, BIGCLK_H, t, mm % 10, col);
  // ko10.1: fecha encima, en letra pequena ("2026.09.25 (금)"); debajo la taparian
  // los brillos de alegria del Pokemon
  int yy; uint8_t mo, dd, wd;
  wxDate(e, &yy, &mo, &dd, &wd);
  char wday[4];
  memcpy(wday, XT(X_WDAYS) + wd * 3, 3);
  wday[3] = 0;
  char buf[24];
  snprintf(buf, sizeof(buf), XT(X_DATE_FMT), yy, (unsigned)mo, (unsigned)dd, wday);
  uint16_t dsky = lerp565(gSkyTop, gSkyBot, BIGDATE_Y + 8, HORIZON);
  setSize(1);
  int dx = centerX(buf, 1), dy = BIGDATE_Y;
  // tinta suave (mas discreta que el mensaje de estado, pero legible)
  gfx->setTextColor(night ? lerp565(UI_INK_NIGHT, dsky, 4, 16) : lerp565(UI_INK, dsky, 5, 16));
  setCur(dx, dy);
  printT(buf);
}

// primera partida: elige inicial entre Bulbasaur / Charmander / Squirtle
void renderStarterSelect() {
  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, 231, UI_BG_DAY);
  const char *t = T(S_CHOOSE_STARTER);
  gfx->setTextColor(UI_INK);
  setSize(2);
  setCur(centerX(t, 2), 68);
  printT(t);
  for (int i = 0; i < 3; i++) {
    int16_t d = STARTER_DEX[i];
    const DexEntry &de = DEX_TBL[d];
    int ry = STARTER_ROW_Y + i * (STARTER_ROW_H + STARTER_ROW_GAP);
    gfx->fillRoundRect(70, ry, 326, STARTER_ROW_H, 14, lerp565(de.accent, UI_WHITE, 6, 8));
    gfx->drawRoundRect(70, ry, 326, STARTER_ROW_H, 14, de.accent);
    const uint8_t *th = thumbs.get(d);     // miniatura del inicial (si la SD esta lista)
    if (th) drawThumb(th, 76, ry - 5, 3, false);
    gfx->setTextColor(UI_INK);
    setSize(3);
    setCur(178, ry + 24);
    printT(dexName(d));
  }
  gfx->flush();
}

// ---------- texto: tamano y cursor (preparado para fuentes CJK) ----------
// La fuente clasica ancla el cursor en la ESQUINA SUPERIOR del texto, y las 74
// coordenadas Y del codigo estan escritas asi. Las fuentes U8g2 lo anclan en la
// LINEA BASE (drawChar hace curY = y - baseline*size), asi que al cambiar de
// fuente todo el texto subiria. setCur() compensa esa diferencia en un solo
// sitio, para que la Y siga significando "arriba" con cualquier fuente.
//
// setSize() existe solo porque la libreria tiene textsize_x protegido y setCur()
// necesita saber la escala activa para multiplicar el ascenso.
//
// Con la fuente clasica gFontAscent es 0, asi que setCur(x,y) es exactamente
// setCur(x,y): esto no mueve un pixel en los seis idiomas actuales.
uint8_t gTextSize = 1;
int gFontAscent = 0;  // px del borde superior a la linea base, 0 = fuente clasica

// --- ajuste de la fuente CJK (los dos valores van juntos) ---
// La fuente clasica mide 8 px de alto a escala 1. Si la CJK mide mas, hay que
// dividir la escala para que los tamanos cuadren... pero dividir ADELGAZA el
// trazo: a escala 1 la unifont pinta lineas de 1 px mientras el latino a escala
// 2 las pinta de 2, y el japones se ve tenue aunque mida igual.
//
// Por eso conviene una fuente base pequena Y EN NEGRITA (_b_), que permita usar
// la MISMA escala que el latino: asi el grosor coincide y no hay que dividir.
//
// Se probo una fuente realmente negrita (b10_b) y NO sirve a este tamano: los
// trazos se juntan y los kana pierden detalle, ilegibles incluso para un lector
// nativo. Se vuelve a la unifont, que es la mas legible, y el trazo fino se
// compensa en printT() repintando 1 px desplazado (pseudo-negrita).
// japanese3 y no japanese1: los subconjuntos 1 y 2 traen kana y kanji pero
// NINGUNA puntuacion CJK, y Arduino_GFX no dibuja nada cuando le falta el
// glifo (ni avanza el cursor), asi que los 25 signos ！ y ？ de las cadenas
// desaparecian. Es la misma familia unifont: cabecera de metricas identica y
// superconjunto estricto, comprobado glifo a glifo sobre los 226 codepoints
// que usa el firmware. Cuesta 102 KB mas de flash y no mueve un pixel.
// El coreano necesita una fuente con hangul, y CJK_FONT era un solo #define.
// En vez de dar por hecho que un subconjunto sirve para las dos escrituras, la
// fuente se elige por idioma. korean2 y no korean1: el coreano de aqui usa 296
// silabas distintas y todas estan en KS X 1001, el conjunto de 2350 que es el
// tamano medido para korean2; korean1 trae 478. La comprobacion mira la
// pertenencia a KS X 1001, no recorre la tabla de la fuente, asi que se apoya
// en que el conjunto de korean2 sea ese; el ASCII se da por presente.
// Sin comprobar en placa: si CJK_SIZE_DIV 2 le sienta a korean2 igual que a la
// japonesa. El ascenso si se mide en tiempo de ejecucion en applyLangFont().
#define CJK_FONT_JA u8g2_font_unifont_t_japanese3
#define CJK_FONT_KO u8g2_font_unifont_t_korean2
#define CJK_SIZE_DIV 2

uint8_t gReqSize = 1;  // fork KO (ko4): tamano PEDIDO (el hangul 3 usa el glifo de 20 px)

void setSize(uint8_t n) {
  // gTextSize guarda la escala REALMENTE aplicada, no la pedida: setCur()
  // multiplica el ascenso por ella y tiene que cuadrar con lo que se pinta.
  gReqSize = n;
  gTextSize = gCjkFont ? (n >= CJK_SIZE_DIV ? n / CJK_SIZE_DIV : 1) : n;
  gfx->setTextSize(gTextSize);
}

static bool koNoto();
static const FkoTier &fkoTier();

void setCur(int x, int y) {
  if (koNoto()) { gfx->setCursor(x, y + fkoTier().base); return; }  // ko8: y = borde superior
  gfx->setCursor(x, y + gFontAscent * gTextSize);
}

// Fija la fuente del idioma activo. Es el UNICO sitio que la toca: el resto del
// codigo dibuja igual para todos los idiomas gracias a textW()/setCur().
//
// El ascenso se mide en vez de codificarlo: getTextBounds() devuelve y1 como
// desplazamiento del borde superior respecto al cursor, negativo con las fuentes
// U8g2 (que anclan en la linea base). Asi setCur() puede seguir tratando la Y
// como "arriba" sea cual sea la fuente, sin constantes magicas por fuente.
void applyLangFont() {
  gCjkFont = LANG_IS_CJK(gLang);
  if (!gCjkFont) {
    gfx->setFont();            // 5x7 clasica, CP437: la Y ya es el borde superior
    gfx->setUTF8Print(false);
    gFontAscent = 0;
    return;
  }
  gfx->setFont((gLang == LANG_KO) ? CJK_FONT_KO : CJK_FONT_JA);
  gfx->setUTF8Print(true);     // las cadenas CJK son UTF-8 multibyte
  int16_t x1, y1;
  uint16_t w, h;
  uint8_t antes = gTextSize;
  gfx->setTextSize(1);
  gfx->getTextBounds("A", 0, 0, &x1, &y1, &w, &h);
  gFontAscent = -y1;           // y1 negativo: subir desde la linea base
  gfx->setTextSize(antes);
}

// ---------- medida de texto (preparado para fuentes CJK) ----------
// La fuente clasica de Arduino_GFX avanza 6 px por caracter a tamano 1, y la UI
// centraba con strlen(s)*6*n a pelo. Eso solo vale para un byte por caracter y
// ancho fijo: con una fuente U8g2 para japones/coreano/chino, ni strlen() cuenta
// caracteres (UTF-8 es multibyte) ni todos miden lo mismo. Centralizarlo aqui es
// lo que permitira anadir esos idiomas sin tocar cada punto de dibujado.
//
// Para la fuente clasica devuelve EXACTAMENTE la misma cuenta que habia antes,
// asi que este cambio no mueve un pixel en los seis idiomas actuales.
// ---------- fuente coreana (fork KO, ko8) ----------
// En coreano TODO el texto (hangul y ASCII) sale de font_ko.h: Noto Sans KR
// Medium con antialias de 2 bits, que se mezcla con lo que ya hay en el lienzo.
// Tamano pedido -> px: 1:16  2:20  3:26  4-5:36  6:48  7:60. Antes (ko4-ko7)
// el hangul era de 16/20 px a 1 bit y el ASCII la unifont pixelada.
// El resto de idiomas CJK (japones) sigue con la unifont.
static bool koNoto() { return gCjkFont && gLang == LANG_KO; }

static const FkoTier &fkoTier() {
  uint8_t n = gReqSize;
  int t = n <= 1 ? 0 : n == 2 ? 1 : n == 3 ? 2 : n <= 5 ? 3 : n == 6 ? 4 : 5;
  return FKO_TIERS[t];
}

static int fkoFind(const uint16_t *tab, int count, uint32_t cp) {
  int lo = 0, hi = count - 1;
  while (lo <= hi) {
    int mid = (lo + hi) / 2;
    if (tab[mid] == cp) return mid;
    if (tab[mid] < cp) lo = mid + 1; else hi = mid - 1;
  }
  return -1;
}

// glifo hangul del tamano actual; si ese tamano no lo trae (36 px solo tiene las
// silabas de los textos del firmware, 48/60 ninguna) baja al siguiente que si
static const FkoTier *fkoHangul(uint32_t cp, int *idx) {
  int t = (int)(&fkoTier() - FKO_TIERS);
  for (; t >= 0; t--) {
    const FkoTier &T = FKO_TIERS[t];
    int i = T.hangul == 1 ? fkoFind(FKO_KS_CP, FKO_KS_COUNT, cp)
          : T.hangul == 2 ? fkoFind(FKO_SUB_CP, FKO_SUB_COUNT, cp) : -1;
    if (i >= 0) { *idx = i; return &T; }
  }
  return nullptr;
}

// mezcla 0..3 de col sobre el pixel del lienzo
static inline void fkoPut(uint16_t *fb, int x, int y, uint8_t a, uint16_t col) {
  if ((unsigned)x >= LCD_WIDTH || (unsigned)y >= LCD_HEIGHT) return;
  uint16_t &d = fb[y * LCD_WIDTH + x];
  if (a >= 3) { d = col; return; }
  uint16_t b = d;
  uint8_t na = 3 - a;
  uint16_t r = (((col >> 11) & 31) * a + ((b >> 11) & 31) * na) / 3;
  uint16_t g = (((col >> 5) & 63) * a + ((b >> 5) & 63) * na) / 3;
  uint16_t bl = ((col & 31) * a + (b & 31) * na) / 3;
  d = (r << 11) | (g << 5) | bl;
}

static void fkoBlit(const uint8_t *bits, int w, int h, int x0, int y0, uint16_t col) {
  uint16_t *fb = gfx->getFramebuffer();
  if (!fb) return;
  int rb = (w + 3) / 4;
  for (int y = 0; y < h; y++) {
    const uint8_t *row = bits + y * rb;
    for (int x = 0; x < w; x++) {
      uint8_t a = (row[x >> 2] >> (6 - 2 * (x & 3))) & 3;
      if (a) fkoPut(fb, x0 + x, y0 + y, a, col);
    }
  }
}

// avance de un caracter (y lo pinta si draw); base = linea base
static int fkoChar(uint32_t cp, int x, int base, bool draw, uint16_t col) {
  const FkoTier &T = fkoTier();
  if ((cp >= 0xAC00 && cp <= 0xD7A3) || (cp >= 0x3131 && cp <= 0x318E)) {  // silaba o jamo
    int idx;
    const FkoTier *H = fkoHangul(cp, &idx);
    if (!H) return T.px;  // fuera de KS X 1001: hueco del ancho de una silaba
    if (draw) fkoBlit(H->hg + (size_t)idx * ((H->px + 3) / 4) * H->px, H->px, H->px, x, base - H->base, col);
    return H->px;
  }
  if (cp >= 32 && cp <= 126) {
    const FkoGlyph &g = T.ascii[cp - 32];
    if (draw && g.w) fkoBlit(T.abits + g.off, g.w, g.h, x + g.xo, base + g.yo, col);
    return g.adv;
  }
  return T.px / 2;
}

// siguiente caracter UTF-8: devuelve cuantos bytes ocupa y su codepoint
static int utf8Next(const char *s, uint32_t *cp) {
  unsigned char c = (unsigned char)s[0];
  if (c < 0x80) { *cp = c; return 1; }
  int n = c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : c >= 0xC0 ? 2 : 1;
  uint32_t v = n == 4 ? (c & 7) : n == 3 ? (c & 15) : (c & 31);
  for (int i = 1; i < n; i++) {
    if (((unsigned char)s[i] & 0xC0) != 0x80) { *cp = c; return 1; }  // roto: 1 byte
    v = (v << 6) | ((unsigned char)s[i] & 63);
  }
  *cp = v;
  return n;
}

// ancho de un caracter que NO es hangul en la fuente CJK activa
static uint16_t cjkGlyphW(const char *g) {
  if ((unsigned char)g[0] < 0x80) return 8 * gTextSize;  // unifont: medio ancho
  int16_t x1, y1;
  uint16_t w, h;
  gfx->getTextBounds(g, 0, 0, &x1, &y1, &w, &h);
  return w ? w : 16 * gTextSize;
}

uint16_t textW(const char *s, uint8_t size) {
  if (!gCjkFont) return (uint16_t)strlen(s) * 6 * size;
  if (!koNoto()) {
    int16_t x1, y1;
    uint16_t w, h;
    gfx->getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
    return w;
  }
  uint8_t keep = gReqSize;  // el tamano que se mide, sin cambiar el activo
  gReqSize = size;
  uint16_t w = 0;
  while (*s) {
    uint32_t cp;
    s += utf8Next(s, &cp);
    w += fkoChar(cp, 0, 0, false, 0);
  }
  gReqSize = keep;
  return w;
}

// alto de una linea de texto del tamano dado (para centrar en botones)
int textH(uint8_t size) {
  if (koNoto()) {
    uint8_t keep = gReqSize;
    gReqSize = size;
    int h = fkoTier().px;
    gReqSize = keep;
    return h;
  }
  if (gCjkFont) return 16 * (size >= CJK_SIZE_DIV ? size / CJK_SIZE_DIV : 1);
  return 8 * size;
}

// x del cursor para dejar el texto centrado en CX
int centerX(const char *s, uint8_t size) { return CX - textW(s, size) / 2; }

// Imprime respetando la fuente activa. Con la CJK repinta el texto desplazado
// 1 px: la unifont a escala reducida deja trazos de 1 px y se ve tenue, y una
// fuente de verdad negrita a este tamano junta los trazos y estropea los kana.
// Repintar engorda el trazo sin deformar el glifo. Probado en placa por
// usakomint, que comparo las dos opciones en japones.
void printT(char c) {
  if (koNoto()) { char b[2] = { c, 0 }; printT(b); return; }  // ko8: misma fuente
  gfx->print(c);
}

void printT(const char *s) {
  if (koNoto()) {  // fork KO (ko8): Noto suavizada, hangul y ASCII
    uint16_t col = gfx->ink();
    int x = gfx->getCursorX(), base = gfx->getCursorY();
    while (*s) {
      uint32_t cp;
      s += utf8Next(s, &cp);
      x += fkoChar(cp, x, base, true, col);
    }
    gfx->setCursor(x, base);
    return;
  }
  if (gCjkFont) {
    int16_t x = gfx->getCursorX(), y = gfx->getCursorY();
    gfx->print(s);
    gfx->setCursor(x + 1, y);
  }
  gfx->print(s);
}

void render() {
  if (pet.awaitingStarter()) {  // primera partida: elegir inicial (prioridad total)
    renderStarterSelect();
    return;
  }
  if (extraRender()) return;  // fork KO: red / batalla / tongsin
  if (trainingRender()) return;  // fork KO (ko4): entrenamiento
  if (galleryOpen) {
    renderGallery();
    return;
  }
  if (gameOpen) {
    renderGame();
    return;
  }
  if (sackOpen) {
    renderSack();
    return;
  }
  if (kbOpen) {
    renderKeyboard();
    return;
  }
  if (clockOpen) {
    renderClock();
    return;
  }
  if (cardOpen) {
    renderCard();
    return;
  }
  int h = sceneHour();
  gNight = pet.sleeping || h < 6 || h >= 20;
  // drawScene cubre los 466x466 completos: sin fillScreen(NEGRO) previo para
  // que un flush DMA solapado nunca capture negro a medias (anti-parpadeo)
  drawScene(pet.isEgg() ? 0 : DEX_TBL[pet.speciesId].biome, millis(), gNight);
  if (!pet.ceremony) drawBigClock(gNight);  // fork KO (ko4): hora grande de fondo

  if (pet.ceremony) {
    const DexEntry &d = DEX_TBL[pet.speciesId];
    const char *msg = (pet.ceremony == CER_FAREWELL) ? T(S_FAREWELL)
                      : (pet.ceremony == CER_RUNAWAY) ? T(S_RUNAWAY)
                                                      : T(S_GOODBYE);
    drawHeader(dexName(pet.speciesId), d.accent, msg);
    drawCeremony();
    gfx->flush();
    return;
  }

  if (pet.isEgg()) {
    drawHeader(T(S_EGG_HDR), inkColor(), eggMsg());
    int s = 5, x = CX - 16 * s, y = PET_CY - 16 * s;
    drawMap(SPR_EGG, SPRITE_H, x, y, s, false);
    if (pet.eggCracks() >= 1)
      for (auto &c : CRACK1) gfx->fillRect(x + c[0] * s, y + c[1] * s, s, s, INK_K);
    if (pet.eggCracks() >= 2)
      for (auto &c : CRACK2) gfx->fillRect(x + c[0] * s, y + c[1] * s, s, s, INK_K);
    if (pet.eggRarity() >= R_RARO) {
      const char *rar = (pet.eggRarity() == R_LEGENDARIO) ? T(S_EGG_LEGEND) : T(S_EGG_RARE);
      gfx->setTextColor(pet.eggRarity() == R_LEGENDARIO ? UI_BAR_WARN : 0x4C98);
      setSize(2);
      setCur(centerX(rar, 2), 316);
      printT(rar);
    }
    char reg[24];
    snprintf(reg, sizeof(reg), T(S_POKEDEX_FMT), dexDiscoveredCount());
    gfx->fillRect(0, 312, 466, 154, gNight ? UI_BG_NIGHT : UI_BG_DAY);
    gfx->setTextColor(inkColor());
    setSize(2);
    setCur(centerX(reg, 2), 348);
    printT(reg);
  } else {
    const DexEntry &d = DEX_TBL[pet.speciesId];
    char name[44];  // ko8: apodo en hangul (hasta 18 bytes)
    const char *base = pet.nick[0] ? pet.nick : dexName(pet.speciesId);
    snprintf(name, sizeof(name), T(S_NAME_FMT), pet.shiny ? "*" : "", base, pet.level());
    drawHeader(name, gNight ? UI_INK_NIGHT : d.accent, statusMsg());
    drawStreakBadge();
    drawPet();
    drawBath();
    drawPoops();
    // panel inferior: base limpia para barras y botones sobre el paisaje
    gfx->fillRect(0, 312, 466, 154, gNight ? UI_BG_NIGHT : UI_BG_DAY);
    drawBars();
    drawButtons();
    drawCelebration();
    if (pet.wantEvolveButton()) drawEvolveButton();        // CTA rojo: evolucionar
    else if (pet.canRunawayNow()) drawRunawayButton();     // CTA sombrio: escapada (abandono)
    else if (pet.wantFarewellButton()) drawFarewellButton();  // CTA dorado: despedida
    else if (wildAlertActive()) drawWildAlert();           // fork KO: salvaje a la vista
  }

  if (pet.sleeping) {
    gfx->setTextColor(UI_INK_NIGHT);
    setSize(3);
    setCur(320, 130);
    printT("Zz");
  }

  // selector de comida
  if (feedMenuUntil) {
    if (!timeLeft(feedMenuUntil)) {
      feedMenuUntil = 0;
    } else {
      gfx->fillRoundRect(101, 288, 264, 64, 14, UI_WHITE);
      gfx->drawRoundRect(101, 288, 264, 64, 14, inkColor());
      drawMap(SPR_ICON_FOOD, 16, 110, 296, 3, false);
      drawMap(SPR_ICON_BERRY_B, 16, 176, 296, 3, false);
      drawMap(SPR_ICON_BERRY_G, 16, 242, 296, 3, false);
      drawMap(SPR_ICON_CANDY, 16, 308, 296, 3, false);
      // ko9: ya sabe cual le gusta: corazon sobre su favorita
      if (pet.berryKnown) drawMap(SPR_HEART, 32, 110 + pet.favFood() * 66 + 8, 258, 1, false);
    }
  }

  // dialogo "soltar?" (pulsacion larga sobre el bicho)
  if (confirmUntil) {
    if (!timeLeft(confirmUntil)) {
      confirmUntil = 0;
    } else {
      gfx->fillRoundRect(94, 168, 278, 152, 16, UI_WHITE);
      gfx->drawRoundRect(94, 168, 278, 152, 16, UI_INK);
      // 48 y no 28: el buffer se dimensiono cuando toda cadena era de un byte
      // por caracter. Con una fila UTF-8, "%s 놓아줄까요?" mas el nombre pasa
      // de 28 y snprintf corta a mitad de una secuencia de 3 bytes, que la
      // fuente ya no sabe dibujar.
      char q[48];
      snprintf(q, sizeof(q), T(S_RELEASE_FMT), dexName(pet.speciesId));
      gfx->setTextColor(UI_INK);
      setSize(2);
      setCur(centerX(q, 2), 196);
      printT(q);
      gfx->fillRoundRect(118, 252, 100, 52, 12, UI_BAR_OK);
      gfx->setTextColor(UI_WHITE);
      setCur(118 + (100 - textW(T(S_YES), 2)) / 2, 270);
      printT(T(S_YES));
      gfx->fillRoundRect(248, 252, 100, 52, 12, UI_BAR_BAD);
      setCur(248 + (100 - textW(T(S_NO), 2)) / 2, 270);
      printT(T(S_NO));
    }
  }

  // dialogo de decision (evolucionar/mantener, despedirse/quedaros)
  if (choiceKind) {
    if (!timeLeft(choiceUntil)) choiceKind = 0;
    else drawChoiceDialog();
  }

  drawToast();  // fork KO: avisos breves
  gfx->flush();
}

// ---------- minijuego: toques con la pokeball ----------

void startGame() {
  if (pet.isEgg() || pet.sleeping || pet.ceremony) return;
  gameOpen = true;
  gameOverUntil = 0;
  gameScore = 0;
  gameMisses = 0;
  gameNewHi = false;
  gameStartMs = millis();
  hitTime = 0;
  gamePetX = 233;
  lastGameStep = millis();
  // caen del cielo a alturas distintas para que no lleguen las tres a la vez
  for (int i = 0; i < GAME_BALLS; i++) respawnBall(i, -30 - i * 90);
}

// una pelota nueva cae desde arriba (y < 0: aun fuera de la pantalla)
void respawnBall(int i, float y) {
  static const int16_t LANE[GAME_BALLS] = { 130, 233, 336 };
  ballX[i] = LANE[i] - 30 + random(61);
  ballY[i] = y;
  float sp = 0.6f + gameScore * 0.03f;  // mas viva segun avanzas
  if (sp > 2.5f) sp = 2.5f;
  ballVX[i] = random(2) ? sp : -sp;
  ballVY[i] = 0;
}

void gameTap(int16_t x, int16_t y) {
  if (gameOverUntil) return;
  // la pelota mas cercana al dedo (solo una por toque)
  int best = -1;
  float bd = 74 * 74;
  for (int i = 0; i < GAME_BALLS; i++) {
    float dx = ballX[i] - x, dy = ballY[i] - y;
    float d = dx * dx + dy * dy;
    if (ballY[i] > 0 && d < bd) { bd = d; best = i; }
  }
  if (best < 0) return;
  float dx = ballX[best] - x;
  gameScore++;
  sfxPlay(SFX_PLAY);
  // golpe mas suave: impulso moderado que crece poco a poco con la puntuacion
  float lift = 6.6f + (gameScore > 16 ? 3.5f : gameScore * 0.22f);
  ballVY[best] = -lift;
  ballVX[best] += dx * 0.12f;
  if (ballVX[best] > 6.5f) ballVX[best] = 6.5f;
  if (ballVX[best] < -6.5f) ballVX[best] = -6.5f;
  hitX = ballX[best];
  hitY = ballY[best];
  hitTime = millis();
}

void stepGame() {
  uint32_t now = millis();
  // paso relativo a 85 ms, el periodo de frame con el que se afinaron estas
  // constantes (ver el planificador de render en loop()): sin esto la fisica
  // avanzaba "un paso por frame" y el juego iba mas rapido o mas lento segun
  // lo que tardara en dibujarse el frame anterior, que depende del tamano del
  // sprite cargado. Con un Charizard la bola caia mas despacio que con un
  // Diglett, asi que el record no era comparable entre especies.
  float k = lastGameStep ? (now - lastGameStep) / 85.0f : 1.0f;
  if (k > 3.0f) k = 3.0f;  // frame anormalmente tardio: no dar un salto enorme
  lastGameStep = now;

  // ko10.4: con 3 pelotas cae algo mas despacio que con una
  float grav = 0.30f + gameScore * 0.008f;  // cae un poco mas rapido cada vez
  if (grav > 0.65f) grav = 0.65f;
  bool timeUp = now - gameStartMs >= GAME_MS;
  int low = 0;  // la pelota mas baja (la que persigue el bicho)
  for (int i = 0; i < GAME_BALLS; i++) {
    ballVY[i] += grav * k;
    ballX[i] += ballVX[i] * k;
    ballY[i] += ballVY[i] * k;
    // rebote en la pared circular (solo ya dentro de la pantalla)
    float dx = ballX[i] - CX, dy = ballY[i] - CY;
    float d = sqrtf(dx * dx + dy * dy);
    if (ballY[i] > 0 && d > 205) {
      float nx = dx / d, ny = dy / d;
      float dot = ballVX[i] * nx + ballVY[i] * ny;
      if (dot > 0) {
        ballVX[i] = (ballVX[i] - 2 * dot * nx) * 0.85f;
        ballVY[i] = (ballVY[i] - 2 * dot * ny) * 0.85f;
      }
      ballX[i] = CX + nx * 205;
      ballY[i] = CY + ny * 205;
    } else if (ballX[i] < 40 || ballX[i] > 426) {  // aun arriba: que no se salga por los lados
      ballVX[i] = -ballVX[i];
      ballX[i] = ballX[i] < 40 ? 40 : 426;
    }
    // ko9.2: cada pelota que toca el suelo gasta una vida y vuelve a caer del cielo
    if (ballY[i] > 384 && !timeUp) {
      if (++gameMisses >= 3) timeUp = true;
      else respawnBall(i, -40);
    }
    if (ballY[i] > ballY[low]) low = i;
  }
  // ko9.2: fin por tiempo (30 s) o por 3 caidas
  if (timeUp) {
    gameNewHi = pet.playResult(gameScore);  // record: animo + energia; si no, algo de energia
    sfxPlay(gameNewHi ? SFX_MEDAL : SFX_LEVEL);
    gameOverUntil = millis() + 4000;
    return;
  }
  // el bicho sigue por abajo a la pelota mas baja
  float chase = (ballX[low] - gamePetX) * 0.12f;
  if (chase > 7) chase = 7;
  if (chase < -7) chase = -7;
  gamePetX += chase * k;
}

// ---------- saco de entrenamiento (entrena la fuerza) ----------

void startSack() {
  if (pet.isEgg() || pet.sleeping || pet.ceremony) return;
  sackOpen = true;
  sackUntil = millis() + 10000;
  sackOverUntil = 0;
  sackHits = 0;
  sackShake = 0;
  sackNewHi = false;
}

void sackTap() {
  if (!timeLeft(sackUntil)) return;  // ya termino el tiempo
  sackHits++;
  sackShake = 16;  // sacude el saco
}

void drawGameScene();  // prototipo (definida mas abajo)

void renderSack() {
  uint32_t now = millis();
  drawGameScene();  // fondo del habitat
  bool night = sceneHour() < 6 || sceneHour() >= 20;
  uint16_t ink = night ? UI_INK_NIGHT : UI_INK;

  // pantalla de resultado
  if (sackOverUntil) {
    if (!timeLeft(sackOverUntil)) { sackOpen = false; return; }
    char b[20];
    snprintf(b, sizeof(b), T(S_HITS_FMT), sackHits);
    gfx->setTextColor(ink);
    setSize(4);
    setCur(centerX(b, 4), 150);
    printT(b);
    char g[18];
    snprintf(g, sizeof(g), T(S_STR_GAIN_FMT), sackGain);
    gfx->setTextColor(UI_BAR_BAD);
    setSize(3);
    setCur(centerX(g, 3), 210);
    printT(g);
    setSize(2);
    if (sackNewHi && sackHits > 0) {
      gfx->setTextColor(UI_BAR_WARN);
      setCur(centerX(T(S_NEW_RECORD), 2), 256);
      printT(T(S_NEW_RECORD));
    } else {
      char r[18];
      snprintf(r, sizeof(r), T(S_RECORD_FMT), pet.strHi);
      gfx->setTextColor(ink);
      setCur(centerX(r, 2), 256);
      printT(r);
    }
    gfx->flush();
    return;
  }

  // se acabaron los 10 s: aplicar entrenamiento
  if (!timeLeft(sackUntil)) {
    sackNewHi = (sackHits > pet.strHi);
    sackGain = pet.trainStrength(sackHits);
    sfxPlay(sackNewHi ? SFX_MEDAL : SFX_PLAY);
    sackOverUntil = now + 3500;
    gfx->flush();
    return;
  }

  // aporreo activo
  sackShake *= 0.84f;
  int off = (int)(sackShake * sinf(now * 0.05f));
  int sx = CX + off, top = 86, sy = 150;
  gfx->fillRect(CX - 3, 56, 6, top - 56, ink);          // gancho/cuerda
  gfx->fillRect(sx - 4, top - 30, 8, 34, ink);          // cadena
  gfx->fillRoundRect(sx - 42, top, 84, 150, 26, C565(0xb5, 0x3a, 0x3a));  // saco
  gfx->fillRoundRect(sx - 42, top, 84, 22, 18, C565(0x7e, 0x28, 0x28));   // tapa
  gfx->drawRoundRect(sx - 42, top, 84, 150, 26, ink);
  gfx->fillRect(sx - 42, top + 70, 84, 4, C565(0x7e, 0x28, 0x28));        // costura

  // contador de golpes
  char buf[8];
  snprintf(buf, sizeof(buf), "%u", sackHits);
  gfx->setTextColor(ink);
  setSize(6);
  setCur(centerX(buf, 6), 268);
  printT(buf);

  setSize(2);
  setCur(centerX(T(S_HIT_FAST), 2), 322);
  printT(T(S_HIT_FAST));

  // barra de tiempo
  uint32_t left = sackUntil - now;
  int bw = 280, fw = (int)((uint32_t)bw * left / 10000);
  gfx->fillRoundRect(CX - bw / 2, 350, bw, 16, 5, UI_TRACK);
  if (fw > 2) gfx->fillRoundRect(CX - bw / 2, 350, fw, 16, 5, UI_BAR_OK);

  gfx->flush();
}

// fondo del minijuego: hatibat del bicho (cielo por hora + suelo del bioma)
void drawGameScene() {
  int hh = sceneHour();
  bool night = hh < 6 || hh >= 20;
  uint16_t top, bot;
  if (night)       { top = C565(0x0c, 0x12, 0x24); bot = C565(0x1e, 0x26, 0x46); }
  else if (hh < 8) { top = C565(0xd1, 0x6a, 0x86); bot = C565(0xf3, 0xb8, 0x7c); }
  else if (hh < 18){ top = C565(0x8f, 0xc8, 0xea); bot = C565(0xdc, 0xee, 0xe6); }
  else             { top = C565(0xc7, 0x5a, 0x4a); bot = C565(0xf0, 0xae, 0x64); }
  int hor = 376;
  for (int y = 0; y < hor; y += 8)
    gfx->fillRect(0, y, 466, 8, lerp565(top, bot, y, hor));
  if (night)
    for (auto &st : STARS) gfx->fillRect(st[0], st[1], 4, 4, UI_WHITE);
  uint8_t bio = pet.isEgg() ? 0 : DEX_TBL[pet.speciesId].biome;
  uint16_t soil = BIOME_SOIL[bio < BIOME_N ? bio : 0];
  if (night) soil = lerp565(soil, C565(0x16, 0x1c, 0x30), 9, 16);
  gfx->fillRect(0, hor, 466, 466 - hor, soil);
}

void renderGame() {
  // sin fillScreen(NEGRO): drawGameScene cubre los 466x466 completos. Si el
  // DMA del flush anterior aun lee el buffer, vera contenido valido (no negro
  // a medio pintar), que era el parpadeo a 25 fps.
  bool night = sceneHour() < 6 || sceneHour() >= 20;
  uint16_t ink = night ? UI_INK_NIGHT : UI_INK;

  if (gameOverUntil) {
    drawGameScene();
    if (!timeLeft(gameOverUntil)) {
      gameOpen = false;
      return;
    }
    char buf[22];
    snprintf(buf, sizeof(buf), T(S_SCORE_FMT), gameScore);
    gfx->setTextColor(ink);
    setSize(4);
    setCur(centerX(buf, 4), 160);
    printT(buf);
    setSize(2);
    if (gameNewHi) {
      gfx->setTextColor(UI_BAR_WARN);
      setCur(centerX(T(S_NEW_RECORD), 2), 214);
      printT(T(S_NEW_RECORD));
    } else {
      char rec[20];
      snprintf(rec, sizeof(rec), T(S_RECORD_FMT), pet.gameHi);
      gfx->setTextColor(ink);
      setCur(centerX(rec, 2), 214);
      printT(rec);
    }
    // ko9.2: el premio grande solo con record; ko10.3: si no, un poco de energia
    const char *msg = gameNewHi ? XT(X_GAME_REWARD) : gameScore ? XT(X_GAME_SMALL) : XT(X_GAME_NO_REWARD);
    drawFit(msg, 250, 330, gameNewHi ? UI_BAR_OK : ink, 2);
    gfx->flush();
    return;
  }

  drawGameScene();
  stepGame();

  // marcador, record y vidas
  char buf[8];
  snprintf(buf, sizeof(buf), "%u", gameScore);
  gfx->setTextColor(ink);
  setSize(4);
  setCur(centerX(buf, 4), 30);
  printT(buf);
  // 24 y no 12: en japones "きろく %u" son 10 bytes antes de la cifra y el
  // record se quedaba en su primer digito ("きろく 2" con 219). Las tallas de
  // los buffers de texto las vigila TestBuffersDeTexto en test/test_tools.py.
  char rec[24];
  snprintf(rec, sizeof(rec), T(S_REC_FMT), pet.gameHi);
  setSize(2);
  setCur(centerX(rec, 2), 76);
  printT(rec);
  for (int i = 0; i < 3; i++) {
    if (i < 3 - gameMisses) gfx->fillCircle(180 + i * 28, 104, 6, UI_BAR_BAD);
    else gfx->drawCircle(180 + i * 28, 104, 6, UI_TRACK);
  }
  // ko9.2: tiempo que queda (30 s)
  uint32_t el = millis() - gameStartMs;
  drawTimeBar(el < GAME_MS ? GAME_MS - el : 0, GAME_MS, 120);

  if (pmd.loaded) {
    int low = 0;
    for (int i = 1; i < GAME_BALLS; i++) if (ballY[i] > ballY[low]) low = i;
    uint8_t act = (ballX[low] > gamePetX + 4) ? PMD_WALKR : (ballX[low] < gamePetX - 4) ? PMD_WALKL : PMD_IDLE;
    if (!pmd.has(act)) act = PMD_IDLE;
    drawPmdAct(act, (int)gamePetX, 394, millis(), true, false, 3);
  } else if (mon.loaded) {
    int s = (mon.h * 2 > 130) ? 1 : 2;
    int w = mon.w * s, h = mon.h * s;
    uint16_t fm = mon.frameMs ? mon.frameMs : 100;
    uint16_t fi = (millis() / fm) % mon.frames;
    const uint8_t *fr = mon.data + (uint32_t)fi * mon.w * mon.h;
    int px = (int)gamePetX - w / 2, py = 394 - h;
    for (int r = 0; r < mon.h; r++)
      for (int c = 0; c < mon.w; c++) {
        uint8_t idx = fr[r * mon.w + c];
        if (idx == 0xFF) continue;
        gfx->fillRect(px + c * s, py + r * s, s, s, mon.pal[idx]);
      }
  }

  // anillo de impacto que se expande y desvanece (feedback suave del golpe)
  uint32_t ht = millis() - hitTime;
  if (hitTime && ht < 260) {
    int rad = 22 + (int)(ht / 6);
    gfx->drawCircle((int)hitX, (int)hitY, rad, C565(0xff, 0xe7, 0x9f));
    gfx->drawCircle((int)hitX, (int)hitY, rad - 2, C565(0xff, 0xd9, 0x8a));
  }

  // las pokeballs (las que aun caen desde fuera se ven al entrar)
  for (int i = 0; i < GAME_BALLS; i++)
    if (ballY[i] > -24) drawMap(SPR_ICON_PLAY, 16, (int)ballX[i] - 24, (int)ballY[i] - 24, 3, false);

  gfx->flush();
}

// ---------- ficha del bicho (deslizar vertical) ----------

// x donde arrancan las barras de la ficha. Estaba fijo en 150, que daba de sobra
// para etiquetas latinas de 3 caracteres pero no para las japonesas, mas anchas:
// こうげき se metia dentro de la barra. Se calcula a partir de la etiqueta mas
// larga para que las cuatro barras sigan alineadas en cualquier idioma, con el
// valor original como suelo (asi en los idiomas latinos no cambia nada).
// Requiere tener ya puesto el tamano de texto 2, porque textW() lo necesita.
static int statBarX() {
  const StrId ids[] = { S_STAT_ATK, S_STAT_DEF, S_STAT_SPE, S_STAT_WGT, S_VIN };
  int ancho = 0;
  for (StrId id : ids) {
    int w = textW(T(id), 2);
    if (w > ancho) ancho = w;
  }
  int x = 96 + ancho + 12;   // 12 px de aire entre etiqueta y barra
  return x < 150 ? 150 : x;
}

void drawCardStat(int y, const char *label, uint16_t val, uint16_t maxBar, uint16_t color) {
  gfx->setTextColor(UI_INK);
  setSize(2);
  setCur(96, y);
  printT(label);
  char num[8];
  snprintf(num, sizeof(num), "%u", val);
  setCur(330, y);
  printT(num);
  int bx = statBarX();
  int bw = 310 - bx;   // la barra siempre acaba en 310, dejando aire hasta el numero
  int fw = (int)val * bw / maxBar;
  if (fw > bw) fw = bw;
  gfx->fillRoundRect(bx, y + 2, bw, 11, 3, UI_TRACK);
  if (fw > 2) gfx->fillRoundRect(bx, y + 2, fw, 11, 3, color);
}

// ---------- ajuste de hora en pantalla (deslizar abajo) ----------
// El usuario pone su hora LOCAL a ojo; el firmware la usa tal cual, asi que
// no hay que gestionar zona horaria. Preserva el dia (no rompe racha/edad).

void openClock() {
  uint32_t e = pet.lastSeenEpoch ? pet.lastSeenEpoch : rtcEpoch();
  clockH = (e / 3600) % 24;
  clockM = (e / 60) % 60;
  uint8_t mo, d;
  wxDate(e ? e : 1767225600u, &clockY, &mo, &d, nullptr);  // sin hora: 1-1-2026
  clockMo = mo; clockD = d;
  clockDateMode = false;
  clockOpen = true;
}

void applyClock() {
  // ko10.4: fecha y hora puestas a mano (antes solo la hora, sobre el dia guardado)
  uint32_t e = wxDaysFromDate(clockY, (uint8_t)clockMo, (uint8_t)clockD) * 86400u +
               (uint32_t)clockH * 3600 + (uint32_t)clockM * 60;
  rtcSetEpoch(e);
  pet.setClock(e);
  gRtcWasLost = false;
  gClockTrusted = true;  // la hora ya es de fiar (tambien para pasarla por tongsin)
  clockOpen = false;
}

void drawClockBtn(int x, int y, const char *l) {
  gfx->fillRoundRect(x, y, 58, 58, 12, UI_WHITE);
  gfx->drawRoundRect(x, y, 58, 58, 12, UI_INK);
  gfx->setTextColor(UI_INK);
  setSize(4);
  setCur(x + 17, y + 15);
  printT(l);
}

// pildoras de idioma centradas en y; rellena la activa
#define LANG_PILL_Y 272   // ko10.3: todo 20-30 px mas arriba (la version se cortaba abajo)
#define LANG_PILL_H 30
#define LANG_PILL_X 336          // pildora de idioma (cicla LANG_COUNT al tocar)
#define LANG_PILL_W 96
#define WIFI_PILL_X 178   // fork KO
#define WIFI_PILL_W 110
#define RST_PILL_X 163    // fork KO (ko8): [nuevo comienzo]
#define RST_PILL_Y 374
#define RST_PILL_H 30
#define CLK_BTN_Y 170   // botones +/- (antes 190)
#define CLK_PILL_X 143  // ko10.4: pildora hora/fecha bajo el titulo
#define CLK_PILL_Y 66
#define CLK_PILL_W 180
#define CLK_PILL_H 30
#define CLK_OK_Y 316    // [OK] (antes 340)
#define CLK_VER_Y 414   // version (antes 436: con "ko10.2" ya rozaba el borde)
#define RST_PILL_W 140
static const char *const LANG_CODES[LANG_COUNT] = { "ES", "EN", "FR", "DE", "IT", "PT", "JA", "KO" };

void renderClock() {
  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, 231, UI_BG_DAY);
  gfx->setTextColor(UI_INK);
  setSize(3);
  setCur(centerX(T(S_SET_TIME), 3), 30);  // ko10.4: sitio para la pildora de fecha
  printT(T(S_SET_TIME));

  // ko10.4: pildora que cambia entre hora y fecha (muestra la otra)
  char pill[32];
  if (clockDateMode) snprintf(pill, sizeof(pill), "%d  %02d:%02d", clockY, clockH, clockM);
  else snprintf(pill, sizeof(pill), "%d.%02d.%02d", clockY, clockMo, clockD);
  drawBtn(CLK_PILL_X, CLK_PILL_Y, CLK_PILL_W, CLK_PILL_H, clockDateMode ? UI_BAR_WARN : UI_WHITE, UI_INK, pill);
  char t[8];
  if (clockDateMode) snprintf(t, sizeof(t), "%02d/%02d", clockMo, clockD);
  else snprintf(t, sizeof(t), "%02d:%02d", clockH, clockM);
  gfx->setTextColor(UI_INK);
  setSize(7);
  setCur(centerX(t, 7), 104);  // ko8: centrado de verdad con cualquier fuente
  printT(t);

  drawClockBtn(104, CLK_BTN_Y, "-");  // hora -
  drawClockBtn(170, CLK_BTN_Y, "+");  // hora +
  drawClockBtn(252, CLK_BTN_Y, "-");  // min -
  drawClockBtn(318, CLK_BTN_Y, "+");  // min +
  setSize(2);
  gfx->setTextColor(UI_INK);
  setCur(120, CLK_BTN_Y + 64);
  printT(clockDateMode ? XT(X_MONTH) : T(S_HOUR));
  setCur(276, CLK_BTN_Y + 64);
  printT(clockDateMode ? XT(X_DAY) : T(S_MIN));

  // interruptor de sonido (izquierda de la fila de idioma)
  bool snd = audioEnabled();
  const char *sl = snd ? T(S_SND_ON) : T(S_SND_OFF);
  gfx->fillRoundRect(34, LANG_PILL_Y, 96, LANG_PILL_H, 8, snd ? UI_BAR_OK : UI_WHITE);
  gfx->drawRoundRect(34, LANG_PILL_Y, 96, LANG_PILL_H, 8, UI_INK);
  gfx->setTextColor(snd ? UI_BG_DAY : UI_INK);
  setSize(2);
  setCur(34 + (96 - textW(sl, 2)) / 2, LANG_PILL_Y + 8);
  printT(sl);

  // selector de idioma: una pildora que cicla los 6 idiomas al tocar
  gfx->fillRoundRect(LANG_PILL_X, LANG_PILL_Y, LANG_PILL_W, LANG_PILL_H, 8, UI_WHITE);
  gfx->drawRoundRect(LANG_PILL_X, LANG_PILL_Y, LANG_PILL_W, LANG_PILL_H, 8, UI_INK);
  char lp[10];
  snprintf(lp, sizeof(lp), "%s >", LANG_CODES[gLang]);
  gfx->setTextColor(UI_INK);
  setSize(2);
  setCur(LANG_PILL_X + (LANG_PILL_W - textW(lp, 2)) / 2, LANG_PILL_Y + 8);
  printT(lp);

  // fork KO: pildora WiFi (red + hora por NTP) entre sonido e idioma
  gfx->fillRoundRect(WIFI_PILL_X, LANG_PILL_Y, WIFI_PILL_W, LANG_PILL_H, 8,
                     netConfigured() ? 0x4C98 : UI_WHITE);
  gfx->drawRoundRect(WIFI_PILL_X, LANG_PILL_Y, WIFI_PILL_W, LANG_PILL_H, 8, UI_INK);
  gfx->setTextColor(netConfigured() ? UI_WHITE : UI_INK);
  setSize(2);
  setCur(WIFI_PILL_X + (WIFI_PILL_W - textW("WiFi", 2)) / 2, LANG_PILL_Y + 8);
  printT("WiFi");

  gfx->fillRoundRect(133, CLK_OK_Y, 200, 48, 14, UI_BAR_OK);
  gfx->setTextColor(UI_BG_DAY);
  setSize(3);
  setCur(CX - 18, CLK_OK_Y + 12);
  printT("OK");

  // fork KO (ko8): [nuevo comienzo] (abre su propia pantalla de confirmacion)
  drawBtn(RST_PILL_X, RST_PILL_Y, RST_PILL_W, RST_PILL_H, UI_WHITE, UI_BAR_BAD, XT(X_RESET_BTN));

  // version del firmware (discreta, abajo del todo)
  // ko6.2: 20 no bastaba para "TamaPoke v1.17-ko6.1" (se veia "ko6."): que no vuelva a pasar
  char ver[40];
  static_assert(sizeof("TamaPoke v" FW_VERSION) <= sizeof(ver), "la version no cabe en pantalla");
  snprintf(ver, sizeof(ver), "TamaPoke v%s", FW_VERSION);
  gfx->setTextColor(UI_INK);
  setSize(1);
  setCur(centerX(ver, 1), CLK_VER_Y);
  printT(ver);
  gfx->flush();
}

void clockTap(int16_t x, int16_t y) {
  if (y >= CLK_PILL_Y - 4 && y < CLK_PILL_Y + CLK_PILL_H + 4 && x >= CLK_PILL_X && x < CLK_PILL_X + CLK_PILL_W) {
    clockDateMode = !clockDateMode;  // ko10.4: hora <-> fecha
    sfxPlay(SFX_TAP);
    return;
  }
  if (y >= CLK_BTN_Y && y <= CLK_BTN_Y + 58) {  // fila de botones +/-
    if (clockDateMode) {  // ko10.4: mes (con el ano al dar la vuelta) y dia
      if (x >= 104 && x < 162) { if (--clockMo < 1) { clockMo = 12; clockY--; } }
      else if (x >= 170 && x < 228) { if (++clockMo > 12) { clockMo = 1; clockY++; } }
      else if (x >= 252 && x < 310) { if (--clockD < 1) clockD = wxDaysInMonth(clockY, (uint8_t)clockMo); }
      else if (x >= 318 && x < 376) { if (++clockD > wxDaysInMonth(clockY, (uint8_t)clockMo)) clockD = 1; }
      if (clockY < 2025) clockY = 2025;
      if (clockY > 2099) clockY = 2099;
      uint8_t dim = wxDaysInMonth(clockY, (uint8_t)clockMo);
      if (clockD > dim) clockD = dim;
      return;
    }
    if (x >= 104 && x < 162) clockH = (clockH + 23) % 24;
    else if (x >= 170 && x < 228) clockH = (clockH + 1) % 24;
    else if (x >= 252 && x < 310) clockM = (clockM + 59) % 60;
    else if (x >= 318 && x < 376) clockM = (clockM + 1) % 60;
    return;
  }
  if (y >= LANG_PILL_Y && y <= LANG_PILL_Y + LANG_PILL_H) {
    if (x >= 34 && x < 130) {                  // fork KO (ko4): ajustes de sonido
      sfxPlay(SFX_TAP);
      openSound();
      return;
    }
    if (x >= WIFI_PILL_X && x < WIFI_PILL_X + WIFI_PILL_W) {  // fork KO: red / NTP
      clockOpen = false;
      openNet();
      sfxPlay(SFX_TAP);
      return;
    }
    if (x >= LANG_PILL_X && x < LANG_PILL_X + LANG_PILL_W) {  // cicla idioma
      setLang((Lang)((gLang + 1) % LANG_COUNT));
      applyLangFont();  // la fuente cambia con el idioma
      sfxPlay(SFX_TAP);
      return;
    }
  }
  if (y >= CLK_OK_Y && y <= CLK_OK_Y + 48 && x >= 133 && x <= 333) { applyClock(); return; }
  if (y >= RST_PILL_Y && y < RST_PILL_Y + RST_PILL_H && x >= RST_PILL_X && x < RST_PILL_X + RST_PILL_W) {
    openReset();
    return;
  }
}

// llama + numero de racha arriba a la izquierda
void drawStreakBadge() {
  if (pet.streak < 1) return;
  int x = 26, y = 16;
  gfx->fillTriangle(x + 8, y, x + 1, y + 17, x + 15, y + 17, UI_BAR_BAD);
  gfx->fillTriangle(x + 8, y + 7, x + 4, y + 17, x + 12, y + 17, UI_BAR_WARN);
  char s[6];
  snprintf(s, sizeof(s), "%u", pet.streak);
  gfx->setTextColor(inkColor());
  setSize(2);
  setCur(x + 22, y + 2);
  printT(s);
}

// banner temporal: medalla nueva o hito de racha
void drawCelebration() {
  const char *l1 = nullptr, *l2 = nullptr;
  // 32 y no 20: "%u にちれんぞく！" ya pasa de 20 con una sola cifra, y snprintf
  // cortaba a mitad de un caracter de 3 bytes.
  char buf[32];
  if (pet.showMedal()) {
    for (int i = 0; i < MED_COUNT; i++)
      if (pet.newMedal & (1 << i)) { l2 = medalName(i); break; }
    l1 = T(S_MEDAL_BANNER);
  } else if (pet.showMilestone()) {
    snprintf(buf, sizeof(buf), T(S_STREAK_DAYS_FMT), pet.streak);
    l1 = T(S_GREAT);
    l2 = buf;
  }
  if (!l1) return;
  gfx->fillRoundRect(73, 150, 320, 96, 16, UI_BAR_WARN);
  gfx->drawRoundRect(73, 150, 320, 96, 16, UI_INK);
  gfx->setTextColor(UI_INK);
  setSize(3);
  setCur(centerX(l1, 3), 176);
  printT(l1);
  setSize(2);
  setCur(centerX(l2, 2), 212);
  printT(l2);
}

// medallas en la ficha: badge con etiqueta, color si conseguida
void drawMedalBadge(int x, int y, int i) {
  bool got = pet.hasMedal(1 << i);
  gfx->fillRoundRect(x, y, 100, 24, 6, got ? UI_BAR_OK : UI_TRACK);
  if (!got) gfx->drawRoundRect(x, y, 100, 24, 6, UI_TRACK);
  gfx->setTextColor(got ? UI_BG_DAY : 0x4208);
  setSize(2);
  setCur(x + (100 - textW(medalLabel(i), 2)) / 2, y + 5);
  printT(medalLabel(i));
}

// pagina 0: perfil (retrato grande, identidad, racha, vinculo, baya)
void renderCardProfile() {
  const DexEntry &d = DEX_TBL[pet.speciesId];
  const char *nm = pet.nick[0] ? pet.nick : dexName(pet.speciesId);
  char head[44];
  snprintf(head, sizeof(head), T(S_NAME_FMT), pet.shiny ? "*" : "", nm, pet.level());
  gfx->setTextColor(d.accent);
  // auto-encoge: a tamano 3 los nombres largos no caben en la franja estrecha de
  // arriba de la pantalla redonda, asi que se cortaban por el borde. El ancho se
  // MIDE con textW(): strlen() cuenta bytes, y en una fila UTF-8 no hay un byte
  // por caracter ni todos miden lo mismo. Con la fuente clasica textW() devuelve
  // strlen()*6*size, o sea exactamente la cuenta anterior: no mueve un pixel en
  // los seis idiomas latinos (11 caracteres a tamano 3 son esos 198 px).
  setSize(3);
  int hts = (textW(head, 3) <= 198) ? 3 : 2;
  setSize(hts);
  setCur(centerX(head, hts), hts == 3 ? 34 : 40);
  printT(head);
  if (pet.nick[0]) {  // especie real bajo el apodo
    const char *sp = dexName(pet.speciesId);
    char par[32];
    snprintf(par, sizeof(par), "(%s)", sp);
    gfx->setTextColor(UI_INK);
    setSize(2);
    setCur(centerX(par, 2), 64);   // medido, no contado en bytes
    printT(par);
  }

  // ko9: retrato mas grande (x6) y letra de 26 px en todo el perfil
  if (pmd.loaded) drawPmdActFit(PMD_IDLE, CX, 214, millis(), 6, 230);

  // racha con llama
  char rl[40];  // en japones pierde cifras con 30 a partir de 100 dias
  snprintf(rl, sizeof(rl), T(S_STREAK_FMT), pet.streak, pet.bestStreak);
  int rw = 24 + textW(rl, 3);
  int sx = CX - rw / 2, sy = 222;
  gfx->fillTriangle(sx + 9, sy + 1, sx + 1, sy + 23, sx + 17, sy + 23, UI_BAR_BAD);
  gfx->fillTriangle(sx + 9, sy + 10, sx + 5, sy + 23, sx + 13, sy + 23, UI_BAR_WARN);
  gfx->setTextColor(UI_INK);
  setSize(3);
  setCur(sx + 24, sy);
  printT(rl);

  // vinculo: etiqueta, barra y numero en grande
  {
    int y = 260;
    gfx->setTextColor(UI_INK);
    setSize(3);
    setCur(70, y);
    printT(T(S_VIN));
    int bx = 70 + textW(T(S_VIN), 3) + 12, bw = 350 - bx;
    char num[8];
    snprintf(num, sizeof(num), "%u", pet.bond);
    setCur(362, y);
    printT(num);
    gfx->fillRoundRect(bx, y + 8, bw, 14, 5, UI_TRACK);
    int fw = (int)pet.bond * bw / 100;
    if (fw > 2) gfx->fillRoundRect(bx, y + 8, fw, 14, 5, C565(0xd4, 0x52, 0x7e));
  }

  uint8_t fav = pet.favFood();
  const char *berry = !pet.berryKnown ? T(S_BERRY_UNK)
                      : fav == 0 ? T(S_BERRY_RED)
                      : fav == 1 ? T(S_BERRY_BLUE)
                      : fav == 2 ? T(S_BERRY_GREEN) : XT(X_FOOD_CANDY);
  char info[48];
  // ko9: dia de crianza (1, 2, 3...) en vez de la edad en dias
  snprintf(info, sizeof(info), T(S_INFO_FMT), berry,
           (unsigned long)(pet.ageMinutes / 1440 + 1));
  drawFit(info, 298, 380, UI_INK, 3);

  drawFit(T(S_RENAME_HINT), 338, 300, UI_INK, 2);
}

// pagina 1: combate (4 barras + botones: salvaje, tongsin, entrenar)
// fork KO: las barras suben para hacer sitio a dos botones nuevos (CARD_*_Y)
void renderCardStats() {
  gfx->setTextColor(UI_INK);
  setSize(3);
  setCur(centerX(T(S_BATTLE), 3), 44);
  printT(T(S_BATTLE));

  drawCardStat(88, T(S_STAT_ATK), pet.atkStat(), 260, UI_BAR_BAD);
  drawCardStat(120, T(S_STAT_DEF), pet.defStat(), 260, 0x4C98);
  drawCardStat(152, T(S_STAT_SPE), pet.speStat(), 260, UI_BAR_WARN);
  drawCardStat(184, T(S_STAT_WGT), pet.weight, 100, 0xB3C8);

  // fork KO (ko4): rejilla 2x2 (batalla, tongsin, entrenar, caja) + objetos
  drawBtn(CARD_COL1_X, CARD_ROW1_Y, CARD_COL_W, CARD_BTN_H, C565(0x2e, 0x7d, 0x32), UI_WHITE, XT(X_WILD_BTN));
  drawBtn(CARD_COL2_X, CARD_ROW1_Y, CARD_COL_W, CARD_BTN_H, 0x4C98, UI_WHITE, XT(X_LINK_BTN));
  drawBtn(CARD_COL1_X, CARD_ROW2_Y, CARD_COL_W, CARD_BTN_H, UI_BAR_BAD, UI_WHITE, XT(X_TRAIN_BTN));
  char bx[24];
  snprintf(bx, sizeof(bx), "%s %u", XT(X_BOX_BTN), box.count());
  drawBtn(CARD_COL2_X, CARD_ROW2_Y, CARD_COL_W, CARD_BTN_H, UI_BAR_WARN, UI_INK, bx);
  char it[40];
  snprintf(it, sizeof(it), XT(X_ITEMS_FMT), pet.balls, pet.potions);
  gfx->setTextColor(UI_INK);
  setSize(2);
  setCur(centerX(it, 2), 322);
  printT(it);
}

// pagina 2: medallas con etiqueta descriptiva
void renderCardMedals() {
  int got = 0;
  for (int i = 0; i < MED_COUNT; i++)
    if (pet.hasMedal(1 << i)) got++;
  char head[24];
  snprintf(head, sizeof(head), T(S_MEDALS_FMT), got, MED_COUNT);
  gfx->setTextColor(UI_INK);
  setSize(3);
  setCur(centerX(head, 3), 48);
  printT(head);

  for (int i = 0; i < MED_COUNT; i++) {
    int x = 28 + (i % 2) * 206, y = 104 + (i / 2) * 54;
    bool g = pet.hasMedal(1 << i);
    gfx->fillRoundRect(x, y, 196, 44, 10, g ? UI_BAR_OK : UI_TRACK);
    if (g) {  // marca de conseguida
      gfx->fillCircle(x + 22, y + 22, 11, UI_BG_DAY);
      gfx->setTextColor(UI_BAR_OK);
      setSize(2);
      setCur(x + 16, y + 13);
      printT("v");
    }
    gfx->setTextColor(g ? UI_BG_DAY : 0x4208);
    setSize(2);
    setCur(x + 44, y + 14);
    printT(medalDesc(i));
  }
}

// pagina 3: progreso (nivel, evolucion, descuidos) — saca a la luz mecanicas
// que antes eran invisibles (cuanto falta para subir/evolucionar y por que)
void renderCardProgress() {
  const DexEntry &d = DEX_TBL[pet.speciesId];
  gfx->setTextColor(UI_INK);
  setSize(3);
  setCur(centerX(T(S_PROGRESS), 3), 44);
  printT(T(S_PROGRESS));

  // nivel grande
  char lv[10];
  snprintf(lv, sizeof(lv), T(S_LVL_FMT), pet.level());
  setSize(5);
  setCur(centerX(lv, 5), 86);
  printT(lv);

  // barra de EXP hasta el siguiente nivel (fork KO, ko7: batallas + cuidado)
  uint16_t L = pet.level();
  uint32_t lo = expForLevel(L), hi = expForLevel(L + 1);
  uint32_t into = pet.exp > lo ? pet.exp - lo : 0, span = hi > lo ? hi - lo : 1;
  int bx = 93, bw = 280, by = 158, bh = 22;
  gfx->fillRoundRect(bx, by, bw, bh, 6, UI_TRACK);
  int fw = L >= LEVEL_MAX ? bw - 4 : (int)((uint64_t)(bw - 4) * into / span);
  if (fw > 0) gfx->fillRoundRect(bx + 2, by + 2, fw, bh - 4, 5, UI_EXP);
  char nx[40];
  if (L >= LEVEL_MAX) snprintf(nx, sizeof(nx), "%s", XT(X_MAX_LVL));
  else snprintf(nx, sizeof(nx), XT(X_EXP_NEXT_FMT), (unsigned long)(span - into));
  gfx->setTextColor(UI_INK);
  setSize(2);
  setCur(centerX(nx, 2), by + 30);
  printT(nx);
  // ko9: o solo con el tiempo de crianza (ko10.2: 15 min x nivel)
  if (L < LEVEL_MAX) {
    uint32_t m = pet.careMinutesLeft();
    char tl[48];
    if (m >= 60) snprintf(tl, sizeof(tl), XT(X_CARE_LEFT_HM), (unsigned long)(m / 60), (unsigned long)(m % 60));
    else snprintf(tl, sizeof(tl), XT(X_CARE_LEFT_M), (unsigned long)m);
    drawFit(tl, by + 56, 340, C565(0x60, 0x68, 0x70), 2);
  }

  // estado de evolucion
  gfx->setTextColor(UI_INK);
  setSize(2);
  setCur(centerX(T(S_EVO_LABEL), 2), 250);
  printT(T(S_EVO_LABEL));
  char evoBuf[32];
  const char *evo;
  uint16_t evoCol = UI_INK;
  if (d.evolvesTo == 0) {
    evo = T(S_FINAL_FORM);
  } else {
    int needed = pet.evolveNeed();
    if (pet.level() >= needed) {
      if (pet.lowestStat() >= 40) { evo = T(S_EVO_READY); evoCol = UI_BAR_OK; }
      else { evo = T(S_EVO_BLOCKED); evoCol = UI_BAR_BAD; }
    } else {
      snprintf(evoBuf, sizeof(evoBuf), T(S_EVO_IN_FMT), needed - pet.level());
      evo = evoBuf;
    }
  }
  gfx->setTextColor(evoCol);
  setCur(centerX(evo, 2), 276);
  printT(evo);

  // descuidos (retrasan la evolucion)
  char ms[24];
  snprintf(ms, sizeof(ms), T(S_MISTAKES_FMT), pet.careMistakes);
  gfx->setTextColor(pet.careMistakes > 0 ? UI_BAR_BAD : UI_INK);
  setCur(centerX(ms, 2), 318);
  printT(ms);
}

// ---- ko10.4: pagina de caramelos (de la familia del Pokemon que crias)
#define CANDY_ROW_X 83
#define CANDY_ROW_Y 112
#define CANDY_ROW_W 300
#define CANDY_ROW_H 40
#define CANDY_ROW_GAP 6

void renderCardCandy() {
  gfx->setTextColor(UI_INK);
  setSize(3);
  setCur(centerX(XT(X_CANDY_TITLE), 3), 36);
  printT(XT(X_CANDY_TITLE));
  char have[48], nb[8];
  snprintf(nb, sizeof(nb), "%u", pet.candyOf(pet.speciesId));
  txFmt(have, sizeof(have), X_CANDY_HAVE, dexName(DEX_FAM[pet.speciesId]), nb);
  drawFit(have, 78, 320, C565(0xc8, 0x3c, 0x78), 2);
  static const XId LBL[CU_COUNT] = { X_CU_EXP, X_CU_GAUGE, X_CU_GENES, X_CU_SHINY, X_CU_EVO };
  for (int i = 0; i < CU_COUNT; i++) {
    int y = CANDY_ROW_Y + i * (CANDY_ROW_H + CANDY_ROW_GAP);
    bool ok = pet.candyCanUse((uint8_t)i);
    char b[48];
    if (i == CU_SHINY && pet.shinyCharm) snprintf(b, sizeof(b), "%s", XT(X_CU_SHINY_ON));
    else snprintf(b, sizeof(b), "%s  (%u)", XT(LBL[i]), CANDY_COST[i]);
    drawBtn(CANDY_ROW_X, y, CANDY_ROW_W, CANDY_ROW_H, ok ? C565(0xf0, 0x7a, 0xa8) : UI_TRACK,
            ok ? UI_WHITE : 0x8410, b);
  }
  if (cardMsg && timeLeft(cardMsgUntil)) drawFit(cardMsg, 346, 300, UI_INK, 1);
}

static void cardCandyTap(int16_t x, int16_t y) {
  if (x < CANDY_ROW_X || x >= CANDY_ROW_X + CANDY_ROW_W || y < CANDY_ROW_Y) { cardOpen = false; return; }
  int i = (y - CANDY_ROW_Y) / (CANDY_ROW_H + CANDY_ROW_GAP);
  if (i >= CU_COUNT) { cardOpen = false; return; }
  if ((y - CANDY_ROW_Y) % (CANDY_ROW_H + CANDY_ROW_GAP) >= CANDY_ROW_H) return;  // hueco
  if (pet.candyUse((uint8_t)i)) { sfxPlay(SFX_HEART); cardMsg = XT(X_CANDY_USED); }  // subir de nivel ya suena en addExp
  else { sfxPlay(SFX_DENY); cardMsg = XT(X_CANDY_NO); }
  cardMsgUntil = millis() + 2000;
}

void renderCard() {
  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, 231, UI_BG_DAY);
  if (cardPage == 0) renderCardProfile();
  else if (cardPage == 1) renderCardStats();
  else if (cardPage == 2) renderCardMedals();
  else if (cardPage == 3) renderCardProgress();
  else renderCardCandy();

  // indicador de paginas + ayuda
  for (int i = 0; i < CARD_PAGES; i++) {
    int x = CX - (CARD_PAGES - 1) * 13 + i * 26;
    if (i == cardPage) gfx->fillCircle(x, 374, 5, UI_INK);
    else gfx->drawCircle(x, 374, 4, UI_INK);
  }
  gfx->setTextColor(UI_INK);
  setSize(2);
  setCur(centerX(T(S_BACK), 2), 398);
  printT(T(S_BACK));
  gfx->flush();
}

// ---------- teclado para renombrar ----------
// ko8: dos teclados. Coreano cheonjiin (12 teclas, cji.h) y alfabeto A-Z.
// nameBuf = lo ya confirmado; kbCji = lo que se esta escribiendo en coreano
// (se recompone entero a cada tecla, ver cji.h). Tope: 18 bytes (6 silabas).

// alfabeto: 7x4 (A-Z . -)
static const char KB_KEYS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ.-";
#define KB_COLS 7
#define KB_X 51
#define KB_Y 126
#define KB_W 52
#define KB_H 52
// cheonjiin: 3x4
#define CJ_X 83
#define CJ_Y 124
#define CJ_W 100
#define CJ_H 56
static const char *const CJ_LABEL[CJI_K_COUNT] = {
  "\xE3\x85\xA3", "\xE3\x86\x8D", "\xE3\x85\xA1",                   // ㅣ ㆍ ㅡ
  "\xE3\x84\xB1\xE3\x85\x8B", "\xE3\x84\xB4\xE3\x84\xB9", "\xE3\x84\xB7\xE3\x85\x8C",  // ㄱㅋ ㄴㄹ ㄷㅌ
  "\xE3\x85\x82\xE3\x85\x8D", "\xE3\x85\x85\xE3\x85\x8E", "\xE3\x85\x88\xE3\x85\x8A",  // ㅂㅍ ㅅㅎ ㅈㅊ
  "", "\xE3\x85\x87\xE3\x85\x81", "",                               // (띄움) ㅇㅁ (지움)
};
// fila de abajo: [cambiar teclado] [borrar, solo alfabeto] [OK]
#define KBB_Y 358
#define KBB_H 44

void openKeyboard() {
  kbOpen = true;
  strncpy(nameBuf, pet.nick, sizeof(nameBuf) - 1);
  nameBuf[sizeof(nameBuf) - 1] = 0;
  nameLen = strlen(nameBuf);
  kbCji.clear();
  kbKo = gLang == LANG_KO;
}

// texto completo (confirmado + lo que se esta escribiendo)
static int kbText(char *out, int max, bool final) {
  int n = snprintf(out, max, "%s", nameBuf);
  if (n >= max) n = max - 1;
  return n + cjiCompose(kbCji, out + n, max - n, final);
}

// pasa lo escrito en coreano a nameBuf
static void kbCommit() {
  char t[64];
  kbText(t, sizeof(t), true);
  strncpy(nameBuf, t, sizeof(nameBuf) - 1);
  nameBuf[sizeof(nameBuf) - 1] = 0;
  nameLen = strlen(nameBuf);
  kbCji.clear();
}

// quita las silabas que no tiene la fuente (fuera de KS X 1001) y los jamo sueltos
static void kbCleanName(char *s) {
  char out[sizeof(nameBuf)];
  int j = 0;
  for (const char *p = s; *p;) {
    uint32_t cp;
    int n = utf8Next(p, &cp);
    bool ok = (cp >= 32 && cp < 127) ||
              (cp >= 0xAC00 && cp <= 0xD7A3 && fkoFind(FKO_KS_CP, FKO_KS_COUNT, cp) >= 0);
    if (ok && !(j == 0 && cp == ' ') && j + n < (int)sizeof(out)) { memcpy(out + j, p, n); j += n; }
    p += n;
  }
  while (j > 0 && out[j - 1] == ' ') j--;  // sin espacios al final
  out[j] = 0;
  strcpy(s, out);
}

void renderKeyboard() {
  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, 231, UI_BG_DAY);
  drawFit(T(S_NAME), 36, 200, UI_INK, 2);
  // lo escrito
  char t[64];
  kbText(t, sizeof(t), false);
  gfx->fillRoundRect(83, 68, 300, 46, 10, UI_WHITE);
  gfx->drawRoundRect(83, 68, 300, 46, 10, UI_INK);
  gfx->setTextColor(UI_INK);
  setSize(3);
  int tw = textW(t, 3);
  setCur(tw > 280 ? 373 - tw : 95, 78);  // si no cabe, se ve el final
  printT(t);
  if ((millis() / 500) & 1) gfx->fillRect(gfx->getCursorX() + 2, 76, 2, 30, UI_INK);  // cursor

  if (kbKo) {
    for (int i = 0; i < CJI_K_COUNT; i++) {
      int x = CJ_X + (i % 3) * CJ_W, y = CJ_Y + (i / 3) * CJ_H;
      bool special = i == CJI_K_SPACE || i == CJI_K_DEL;
      if (special) {
        drawBtn(x, y, CJ_W - 6, CJ_H - 6, UI_TRACK, UI_INK, XT(i == CJI_K_SPACE ? X_KB_SPACE : X_KB_DEL));
        continue;
      }
      drawBtn(x, y, CJ_W - 6, CJ_H - 6, UI_WHITE, UI_INK, "");
      int kx = x + (CJ_W - 6) / 2, ky = y + (CJ_H - 6) / 2;
      if (i == CJI_K_DOT) {  // el punto (아래아), bien visible
        gfx->fillCircle(kx, ky, 5, UI_INK);
        continue;
      }
      gfx->setTextColor(UI_INK);
      setSize(3);  // jamo grandes: se leen de un vistazo
      setCur(kx - textW(CJ_LABEL[i], 3) / 2, ky - textH(3) / 2);
      printT(CJ_LABEL[i]);
    }
    drawBtn(108, KBB_Y, 120, KBB_H, 0x4C98, UI_WHITE, "ABC");
    drawBtn(238, KBB_Y, 120, KBB_H, UI_BAR_OK, UI_WHITE, "OK");
  } else {
    for (int i = 0; i < 28; i++) {
      int x = KB_X + (i % KB_COLS) * KB_W, y = KB_Y + (i / KB_COLS) * KB_H;
      char k[2] = { KB_KEYS[i], 0 };
      drawBtn(x, y, KB_W - 5, KB_H - 5, UI_WHITE, UI_INK, k);
    }
    drawBtn(78, KBB_Y - 16, 100, KBB_H, 0x4C98, UI_WHITE, XT(X_KB_HANGUL));
    drawBtn(184, KBB_Y - 16, 98, KBB_H, UI_TRACK, UI_INK, XT(X_KB_DEL));
    drawBtn(288, KBB_Y - 16, 100, KBB_H, UI_BAR_OK, UI_WHITE, "OK");
  }
  gfx->flush();
}

// borra el ultimo caracter confirmado (UTF-8: hasta 3 bytes)
static void kbBackspace() {
  while (nameLen && ((uint8_t)nameBuf[nameLen - 1] & 0xC0) == 0x80) nameBuf[--nameLen] = 0;
  if (nameLen) nameBuf[--nameLen] = 0;
}

void keyboardTap(int16_t x, int16_t y) {
  int by = kbKo ? KBB_Y : KBB_Y - 16;
  if (y >= by && y < by + KBB_H) {  // fila de abajo
    bool ok = kbKo ? x >= 238 : x >= 288;
    bool mode = kbKo ? (x >= 108 && x < 228) : (x >= 78 && x < 178);
    if (ok) {
      kbCommit();
      kbCleanName(nameBuf);
      pet.rename(nameBuf);
      kbOpen = false;
    } else if (mode) {
      kbCommit();
      kbKo = !kbKo;
    } else if (!kbKo && x >= 184 && x < 282) {
      kbBackspace();
    }
    sfxPlay(SFX_TAP);
    return;
  }
  if (kbKo) {
    if (x < CJ_X || y < CJ_Y) return;
    int col = (x - CJ_X) / CJ_W, row = (y - CJ_Y) / CJ_H;
    if (col >= 3 || row >= 4) return;
    uint8_t key = (uint8_t)(row * 3 + col);
    if (key == CJI_K_DEL && !kbCji.n) { kbBackspace(); sfxPlay(SFX_TAP); return; }
    Cji before = kbCji;
    if (!cjiPress(kbCji, key)) { sfxPlay(SFX_DENY); return; }
    char t[64];
    if (kbText(t, sizeof(t), false) > CJI_MAX_BYTES) { kbCji = before; sfxPlay(SFX_DENY); return; }
    sfxPlay(SFX_TAP);
    return;
  }
  // alfabeto
  if (x < KB_X || y < KB_Y) return;
  int col = (x - KB_X) / KB_W, row = (y - KB_Y) / KB_H;
  if (col >= KB_COLS || row >= 4) return;
  int i = row * KB_COLS + col;
  if (nameLen < CJI_MAX_BYTES && nameLen < sizeof(nameBuf) - 1) {
    nameBuf[nameLen++] = KB_KEYS[i];
    nameBuf[nameLen] = 0;
    sfxPlay(SFX_TAP);
  } else {
    sfxPlay(SFX_DENY);
  }
}

// ---------- galeria pokedex ----------

#define GAL_X 73
#define GAL_Y 84
#define GAL_CELL 80

// dibuja una miniatura centrada en su celda; sil=true la pinta en tinta
void drawThumb(const uint8_t *b, int x, int y, int s, bool sil) {
  uint8_t w = b[0], h = b[1], n = b[2];
  const uint8_t *pal = b + 3;
  const uint8_t *d = pal + n * 2;
  int ox = x + (GAL_CELL - w * s) / 2;
  int oy = y + (GAL_CELL - h * s) / 2;
  for (int r = 0; r < h; r++) {
    for (int c = 0; c < w; c++) {
      uint8_t idx = d[r * w + c];
      if (idx == 0xFF) continue;
      uint16_t col = sil ? INK_K : (uint16_t)(pal[idx * 2] | (pal[idx * 2 + 1] << 8));
      gfx->fillRect(ox + c * s, oy + r * s, s, s, col);
    }
  }
}

// fork KO (ko4): descubierto = criado, visto en batalla o capturado
bool dexDiscovered(int16_t dex) { return pet.isRegistered(dex) || dexLog.wasSeen(dex); }

uint16_t dexDiscoveredCount() {
  uint16_t n = 0;
  for (int16_t d = 1; d <= DEX_COUNT; d++)
    if (dexDiscovered(d)) n++;
  return n;
}

// ficha de la pokedex (fork KO, ko4): datos basicos + historial
void renderDexDetail() {
  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, 231, UI_BG_DAY);
  int16_t dx = galleryDetail;
  const DexEntry &d = DEX_TBL[dx];
  bool disc = dexDiscovered(dx);
  char head[40];
  snprintf(head, sizeof(head), "No.%03d %s%s", dx, pet.isShinyRegistered(dx) ? "*" : "",
           disc ? dexName(dx) : "???");
  drawFit(head, 36, 300, disc ? d.accent : UI_INK, 3);
  if (galleryPmd.loaded) {
    // animado y a color si se conoce; silueta estatica si no (estilo "?")
    drawPmdActM(galleryPmd, PMD_IDLE, CX, 196, disc ? millis() : 0, true, !disc, 4, 170);
  } else {
    const uint8_t *t = thumbs.get(dx);
    if (t) drawThumb(t, CX - GAL_CELL / 2, 96, 2, !disc);
  }
  if (!disc) {
    drawFit(XT(X_UNKNOWN), 250, 340, UI_INK, 2);
  } else {
    char l[64];
    static const XId RAR[4] = { X_RARITY_EVO, X_RARITY_COMMON, X_RARITY_RARE, X_RARITY_LEGEND };
    snprintf(l, sizeof(l), "%s  /  %s", typeName(d.ptype), XT(RAR[d.rarity < 4 ? d.rarity : 1]));
    drawFit(l, 208, 340, UI_INK, 2);
    snprintf(l, sizeof(l), XT(X_BASE_FMT), d.bHp, d.bAtk, d.bDef, d.bSpe);
    drawFit(l, 234, 360, UI_INK, 2);
    if (d.evolvesTo) {
      int16_t nx = d.evolvesTo;
      snprintf(l, sizeof(l), XT(X_EVO_FMT), dexDiscovered(nx) ? dexName(nx) : "???", evoLevel(dx));
    } else {
      strncpy(l, XT(X_EVO_FINAL), sizeof(l) - 1);
      l[sizeof(l) - 1] = 0;
    }
    drawFit(l, 260, 360, UI_INK, 2);
    uint32_t fs = dexLog.firstSeen(dx);
    if (fs) {
      int32_t z = (int32_t)(fs / 86400) + 719468;  // civil_from_days
      int32_t era = z / 146097, doe = z - era * 146097;
      int32_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
      int32_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100), mp = (5 * doy + 2) / 153;
      unsigned dd = doy - (153 * mp + 2) / 5 + 1, mm = mp < 10 ? mp + 3 : mp - 9;
      snprintf(l, sizeof(l), XT(X_FIRST_FMT), mm, dd);
      drawFit(l, 290, 340, UI_INK, 2);
    }
    snprintf(l, sizeof(l), XT(X_SEEN_FMT), dexLog.seenCount(dx), dexLog.caughtCount(dx));
    drawFit(l, 316, 340, UI_INK, 2);
    if (pet.isRegistered(dx)) drawFit(XT(X_RAISED), 342, 300, C565(0x2e, 0x7d, 0x32), 2);
  }
  drawFit(T(S_DETAIL_BACK), 386, 260, UI_INK, 2);
  drawFit(XT(X_DEX_EXIT), 412, 220, UI_INK, 1);  // ko5
  gfx->flush();
}

void renderGallery() {
  if (galleryDetail) {  // vista detalle: se redibuja siempre (animada)
    renderDexDetail();
    return;
  }

  if (!galleryDirty) return;  // la rejilla es estatica
  galleryDirty = false;

  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, 231, UI_BG_DAY);
  char head[24];
  snprintf(head, sizeof(head), T(S_POKEDEX_FMT), dexDiscoveredCount());
  gfx->setTextColor(UI_INK);
  setSize(3);
  setCur(centerX(head, 3), 36);
  printT(head);

  for (int r = 0; r < 4; r++) {
    for (int c = 0; c < 4; c++) {
      int16_t dex = galleryPage * 16 + r * 4 + c + 1;
      if (dex > DEX_COUNT) break;
      int x = GAL_X + c * GAL_CELL, y = GAL_Y + r * GAL_CELL;
      const uint8_t *t = thumbs.get(dex);
      if (t) {
        drawThumb(t, x, y, 2, !dexDiscovered(dex));
        if (pet.isShinyRegistered(dex)) {
          gfx->setTextColor(UI_BAR_WARN);
          setSize(2);
          setCur(x + 62, y + 4);
          printT("*");
        }
      } else {
        char num[6];
        snprintf(num, sizeof(num), "%d", dex);
        gfx->setTextColor(UI_INK);
        setSize(2);
        setCur(x + 24, y + 32);
        printT(num);
      }
    }
  }
  drawFit(XT(X_DEX_EXIT), 410, 220, UI_INK, 1);  // ko5
  // puntos de pagina (ko10: 16 paginas; mas juntos para caber abajo del circulo;
  // los de gen 2 en color de acento)
  for (int i = 0; i < GAL_PAGES; i++) {
    int x = CX - (GAL_PAGES - 1) * 5 + i * 10;
    uint16_t col = i * 16 + 1 > 151 ? UI_BAR_WARN : UI_INK;
    if (i == galleryPage) gfx->fillCircle(x, 436, 4, col);
    else gfx->drawCircle(x, 436, 2, col);
  }
  gfx->flush();
}

void galleryTap(int16_t x, int16_t y) {
  // fork KO (ko5): doble toque = salir de la pokedex (antes habia que deslizar
  // hasta la primera pagina y una vez mas)
  static uint32_t lastTap = 0;
  uint32_t now = millis();
  if (lastTap && now - lastTap < 450) {
    lastTap = 0;
    galleryOpen = false;
    galleryDetail = 0;
    galleryPmd.unload();
    sfxPlay(SFX_TAP);
    return;
  }
  lastTap = now;
  if (galleryDetail) {
    // ko9.1: tocar al Pokemon repite su grito (solo si ya se conoce)
    if (y >= 100 && y < 230 && x >= 120 && x < 346 && dexDiscovered(galleryDetail)) {
      audioCry(galleryDetail);
      return;
    }
    galleryDetail = 0;  // volver a la rejilla
    galleryPmd.unload();
    galleryDirty = true;
    return;
  }
  if (y < 72) {  // tocar la cabecera = salir
    galleryOpen = false;
    galleryPmd.unload();
    return;
  }
  // mismo motivo que en el teclado: filtrar antes de dividir
  if (x < GAL_X || y < GAL_Y) return;
  int c = (x - GAL_X) / GAL_CELL, r = (y - GAL_Y) / GAL_CELL;
  if (c > 3 || r > 3) return;
  int16_t dex = galleryPage * 16 + r * 4 + c + 1;
  if (dex > DEX_COUNT) return;
  galleryDetail = dex;
  galleryPmd.load(dex, pet.isShinyRegistered(dex));
  // ko9.1: los ya vistos o criados dicen su nombre; los "???" no
  if (dexDiscovered(dex)) audioCry(dex);
  else sfxPlay(SFX_TAP);
}

void drawBattery() {
  int pc = batPercent();
  if (pc < 0) return;  // sin bateria conectada
  int x = CX - 14, y = 12, w = 24, h = 11;
  bool charging = batCharging();
  uint16_t col = charging ? UI_BAR_OK
                 : (pc >= 40) ? inkColor()
                 : (pc >= 15) ? UI_BAR_WARN
                              : UI_BAR_BAD;
  gfx->drawRoundRect(x, y, w, h, 2, col);
  gfx->fillRect(x + w, y + 3, 3, 5, col);  // borne
  if (charging) {
    // rayo de carga (zigzag) en vez de la barra de nivel
    uint16_t bolt = C565(0xff, 0xd9, 0x4a);
    int bx = x + w / 2;
    gfx->fillTriangle(bx + 3, y + 1, bx - 4, y + 6, bx + 1, y + 6, bolt);
    gfx->fillTriangle(bx - 1, y + 5, bx + 4, y + 5, bx - 3, y + 10, bolt);
  } else {
    int fw = (w - 4) * pc / 100;
    if (fw > 0) gfx->fillRect(x + 2, y + 2, fw, h - 4, col);
  }
}

// ko10.4: texto con contorno de 1-2 px (se lee sobre cielo nublado, nieve o nubes)
void printOutlined(int x, int y, const char *s, uint8_t size, uint16_t fg, uint16_t edge, int w) {
  setSize(size);
  gfx->setTextColor(edge);
  for (int dy = -w; dy <= w; dy++)
    for (int dx = -w; dx <= w; dx++) {
      if (!dx && !dy) continue;
      if (dx * dx + dy * dy > w * w + 1) continue;  // redondeado
      setCur(x + dx, y + dy);
      printT(s);
    }
  gfx->setTextColor(fg);
  setCur(x, y);
  printT(s);
}

void drawHeader(const char *name, uint16_t nameColor, const char *msg) {
  drawBattery();
  // ko10.4: contorno que contrasta con el color del texto (letra clara -> borde
  // oscuro; letra oscura -> borde blanco): se lee con nubes, nieve o de noche
  auto edgeFor = [](uint16_t c) -> uint16_t {
    int lum = ((c >> 11) & 31) * 2 + ((c >> 5) & 63) * 2 + (c & 31);  // aprox. 0..250
    return lum > 140 ? C565(0x1c, 0x22, 0x30) : UI_WHITE;
  };
  printOutlined(centerX(name, 3), 52, name, 3, nameColor, edgeFor(nameColor), 2);
  printOutlined(centerX(msg, 2), 90, msg, 2, inkColor(), edgeFor(inkColor()), 1);
}

// animacion de la ceremonia (10s): despedida = reverencia con corazones y se
// aleja caminando; escapada = se asusta y sale corriendo. Sustituye al idle.
void drawCeremony() {
  if (!pmd.loaded) { drawPet(); return; }  // respaldo si no hay sprite PMD
  uint32_t now = millis();
  float t = pet.ceremonyT();               // 0..1 a lo largo de los 10s
  bool panic = (pet.ceremony == CER_RUNAWAY);
  int x = CX, y = PET_GROUND;
  uint8_t act = PMD_IDLE;

  if (panic) {
    // final triste: penumbra azulada + lluvia
    for (int i = 0; i < 46; i++) {
      int rx = (i * 47 + now / 3) % 466;
      int ry = (i * 91 + now / 2) % 470;
      gfx->drawLine(rx, ry, rx - 3, ry + 12, C565(0x6a, 0x84, 0xb0));
    }
    bool fade = false;
    if (t < 0.30f) {                       // cabizbajo, temblando
      act = pmd.has(PMD_HURT) ? PMD_HURT : PMD_IDLE;
      x = CX + (int)(4 * sinf(now * 0.04f));
    } else {                               // se aleja despacio y se desvanece
      act = pmd.has(PMD_WALKL) ? PMD_WALKL : PMD_IDLE;
      x = CX - (int)(((t - 0.30f) / 0.70f) * (CX + 120));
      fade = (t > 0.6f) && ((now / 160) % 2 == 0);  // parpadea hacia la silueta
    }
    drawPmdAct(act, x, y, now, true, fade, 5);  // fade=silueta: se difumina al irse
    // lagrima cayendo del bicho
    if (t < 0.55f) {
      int ty = y - 150 + (int)((now / 6) % 40);
      gfx->fillRect(x + 6, ty, 3, 6, C565(0x9a, 0xc4, 0xe8));
    }
    return;
  }

  // despedida epica: halo dorado pulsante + chispas y corazones que ascienden
  int gcy = PET_GROUND - 96;
  for (int k = 0; k < 4; k++) {
    int r = 60 + k * 34 + (int)(10 * sinf(now * 0.02f));
    gfx->drawCircle(CX, gcy, r, C565(0xff, 0xdf, 0x8a));
  }
  for (int i = 0; i < 16; i++) {
    int px = (i * 71 + 28) % 466;
    int py = 410 - (int)((now / 8 + i * 70) % 360);   // suben y reaparecen abajo
    if (py < 30) continue;
    if (i % 4 == 0) drawMap(SPR_HEART, 32, px - 8, py - 8, 1, false);  // corazoncito
    else gfx->fillRect(px, py, 4, 4, (i % 2) ? C565(0xff, 0xe7, 0x9f) : C565(0xff, 0x9a, 0xc0));
  }

  if (t < 0.45f) {                         // reverencia / pose de despedida
    act = pmd.has(PMD_POSE) ? PMD_POSE : (pmd.has(PMD_NOD) ? PMD_NOD : PMD_IDLE);
  } else {                                 // se aleja por la derecha
    act = pmd.has(PMD_WALKR) ? PMD_WALKR : PMD_IDLE;
    x = CX + (int)(((t - 0.45f) / 0.55f) * (CX + 140));
  }
  drawPmdAct(act, x, y, now, true, false, 5);
  if (pet.showHeart())                     // corazon grande siguiendo al bicho
    drawMap(SPR_HEART, 32, x + 50, y - 190, 2, false);
}

// dialogo de decision (2 botones apilados): evolucionar/mantener o despedirse/quedaros
void drawChoiceDialog() {
  const char *q, *o1, *o2;
  uint16_t c1, c2, t1, t2;
  if (choiceKind == 1) {  // evolucion
    q = T(S_EVO_Q); o1 = T(S_EVO_TAP); o2 = T(S_EVO_KEEP);
    c1 = UI_BAR_BAD; t1 = UI_WHITE; c2 = UI_TRACK; t2 = UI_INK;
  } else {                // despedida
    q = T(S_FAR_Q); o1 = T(S_FAR_GO); o2 = T(S_FAR_STAY);
    c1 = UI_BAR_WARN; t1 = UI_INK; c2 = UI_BAR_OK; t2 = UI_WHITE;
  }
  gfx->fillRoundRect(73, 156, 320, 188, 16, UI_WHITE);
  gfx->drawRoundRect(73, 156, 320, 188, 16, UI_INK);
  gfx->setTextColor(UI_INK);
  setSize(2);
  setCur(centerX(q, 2), 176);
  printT(q);
  gfx->fillRoundRect(93, 206, 280, 52, 12, c1);     // boton accion
  gfx->setTextColor(t1);
  setCur(centerX(o1, 2), 224);
  printT(o1);
  gfx->fillRoundRect(93, 268, 280, 52, 12, c2);     // boton mantener/quedaros
  gfx->setTextColor(t2);
  setCur(centerX(o2, 2), 286);
  printT(o2);
}

// boton-CTA rojo y grande para evolucionar (pulsa para llamar la atencion)
void drawEvolveButton() {
  uint32_t now = millis();
  int p = (int)(5 * sinf(now * 0.006f));  // late: -5..5
  int x = EVO_BTN_X - p, y = EVO_BTN_Y - p, w = EVO_BTN_W + 2 * p, h = EVO_BTN_H + 2 * p;
  gfx->fillRoundRect(x, y, w, h, 18, UI_BAR_BAD);
  gfx->drawRoundRect(x, y, w, h, 18, UI_WHITE);
  gfx->drawRoundRect(x + 2, y + 2, w - 4, h - 4, 16, UI_WHITE);
  gfx->setTextColor(UI_WHITE);
  setSize(3);
  const char *t = T(S_EVO_TAP);
  setCur(centerX(t, 3), y + h / 2 - 11);
  printT(t);
}

// boton-CTA dorado de despedida: "<nombre> quiere decirte algo..."
void drawFarewellButton() {
  uint32_t now = millis();
  int p = (int)(4 * sinf(now * 0.005f));
  int x = FAR_BTN_X - p, y = FAR_BTN_Y - p, w = FAR_BTN_W + 2 * p, h = FAR_BTN_H + 2 * p;
  gfx->fillRoundRect(x, y, w, h, 16, UI_BAR_WARN);
  gfx->drawRoundRect(x, y, w, h, 16, UI_INK);
  char buf[64];
  const char *nm = pet.nick[0] ? pet.nick : dexName(pet.speciesId);
  snprintf(buf, sizeof(buf), T(S_FAREWELL_BTN), nm);
  gfx->setTextColor(UI_INK);
  setSize(2);
  setCur(centerX(buf, 2), y + h / 2 - 8);
  printT(buf);
}

// boton-CTA sombrio de escapada por abandono: "<nombre> se siente abandonado..."
// (final triste: azul-gris oscuro, latido lento y apagado)
void drawRunawayButton() {
  uint32_t now = millis();
  int p = (int)(3 * sinf(now * 0.003f));
  int x = FAR_BTN_X - p, y = FAR_BTN_Y - p, w = FAR_BTN_W + 2 * p, h = FAR_BTN_H + 2 * p;
  gfx->fillRoundRect(x, y, w, h, 16, C565(0x3a, 0x44, 0x5a));
  gfx->drawRoundRect(x, y, w, h, 16, C565(0x70, 0x80, 0x98));
  char buf[64];
  const char *nm = pet.nick[0] ? pet.nick : dexName(pet.speciesId);
  snprintf(buf, sizeof(buf), T(S_RUNAWAY_BTN), nm);
  gfx->setTextColor(C565(0xc8, 0xd2, 0xe0));
  setSize(2);
  setCur(centerX(buf, 2), y + h / 2 - 8);
  printT(buf);
}

// animacion epica de evolucion: halo radial + rayos giratorios + parpadeo del
// sprite acelerando + chispas que salen disparadas + fogonazo final
void drawEvolveFX(uint32_t now) {
  float t = pet.evolveT();          // 0..1
  int cx = CX, cy = PET_GROUND - 96;

  // halo radial que crece y pulsa
  int halo = 36 + (int)(t * 150) + (int)(8 * sinf(now * 0.02f));
  for (int k = 0; k < 4; k++) {
    int r = halo - k * 7;
    if (r > 0) gfx->drawCircle(cx, cy, r, UI_WHITE);
  }
  // rayos giratorios desde el centro del bicho
  float base = now * 0.004f;
  for (int i = 0; i < 12; i++) {
    float a = base + i * (float)(PI / 6);
    int len = 90 + (int)(70 * (0.5f + 0.5f * sinf(now * 0.012f + i)));
    gfx->drawLine(cx, cy, cx + (int)(cosf(a) * len), cy + (int)(sinf(a) * len), UI_WHITE);
  }
  // parpadeo entre la forma ANTERIOR y la NUEVA (siluetas), acelerando; al
  // final (t>0.9) se queda fija en la nueva para el fogonazo de revelado
  int period = 60 + (int)(220 * (1.0f - t));
  bool showOld = t < 0.9f && evoPmd.loaded && ((now / period) % 2) == 0;
  if (showOld) drawPmdActM(evoPmd, PMD_IDLE, cx, PET_GROUND, 0, true, true, 5, 170);
  else drawPmdAct(PMD_IDLE, cx, PET_GROUND, 0, true, true, 5);
  // chispas que salen disparadas
  for (int i = 0; i < 10; i++) {
    float a = i * (float)(PI / 5) + t * 4.0f;
    int d = (int)((now / 14 + i * 33) % 200);
    int sx = cx + (int)(cosf(a) * d), sy = cy + (int)(sinf(a) * d);
    gfx->fillRect(sx - 2, sy - 2, 5, 5, (i & 1) ? C565(0xff, 0xe0, 0x70) : UI_WHITE);
  }
  // fogonazo final antes de revelar la forma nueva
  if (t > 0.9f) gfx->fillCircle(cx, cy, (int)(300 * (t - 0.9f) / 0.1f), UI_WHITE);
}

void drawPet() {
  if (pmd.loaded) {
    drawPetPMD();
    return;
  }
  if (mon.loaded) {
    drawPetSD();
    return;
  }
  int fi = flashIdxForDex(pet.speciesId);
  if (fi < 0) {
    // sin SD y sin sprite de flash: aviso claro de que faltan sprites
    gfx->setTextColor(inkColor());
    setSize(6);
    setCur(CX - 18, PET_CY - 80);
    printT("?");
    setSize(2);
    const char *l1 = T(S_NO_SPRITES);
    setCur(centerX(l1, 2), PET_CY - 4);
    printT(l1);
    const char *l2 = T(S_LOAD_SPRITES);
    setCur(centerX(l2, 2), PET_CY + 20);
    printT(l2);
    return;
  }
  const Species &sp = SPECIES[fi];
  int s = sp.scale;
  int x = CX - 16 * s;
  int y = PET_CY - 16 * s;

  // animacion de evolucion: alterna la silueta de la forma anterior y la nueva
  if (pet.evolving()) {
    bool flash = (millis() / 300) % 2;
    int16_t showDex = (flash && pet.prevSpeciesId >= 0) ? pet.prevSpeciesId : pet.speciesId;
    int sfi = flashIdxForDex(showDex);
    if (sfi >= 0) {
      const Species &show = SPECIES[sfi];
      drawMap(show.sprite, SPRITE_H, CX - 16 * show.scale, PET_CY - 16 * show.scale, show.scale, flash);
    }
    return;
  }

  PetMood m = pet.mood();
  if (m == MOOD_HAPPY && (millis() / 500) % 2) y -= 6;  // saltito

  drawMap(sp.sprite, SPRITE_H, x, y, s, false);

  // expresiones superpuestas usando las anclas de la especie
  bool blink = (millis() % 3500 < 300);
  if (m == MOOD_SLEEPING || blink) {
    overlayEye(sp, x, y, s, sp.eyeColL);
    overlayEye(sp, x, y, s, sp.eyeColR);
  }
  if (m == MOOD_EATING) overlayMouth(sp, x, y, s, true);
  else if (m == MOOD_SAD) overlayMouth(sp, x, y, s, false);

  if (pet.showHeart()) drawMap(SPR_HEART, 32, x + 20 * s, y - 2 * s, 2, false);
}

// ---------- escena de bano ----------

void startBath() {
  if (pet.isEgg() || pet.sleeping || pet.ceremony || bathUntil) return;
  bathUntil = millis() + 3000;
  bathPending = true;
  int cx = (int)beh.x;
  for (auto &b : bubbles) {
    b.x = cx - 70 + random(140);
    b.y = PET_GROUND - random(150);
    b.r = 8 + random(16);
    b.ph = random(64);
  }
}

void drawBath() {
  uint32_t now = millis();
  if (!timeLeft(bathUntil)) {
    bathUntil = 0;
    if (bathPending) {
      bathPending = false;
      pet.clean();
      // pose de alegria al quedar limpio
      if (pmd.has(PMD_POSE)) {
        beh.mode = 2;
        beh.act = PMD_POSE;
        beh.t0 = now;
        beh.until = now + pmdActTotalMs(pmd.acts[PMD_POSE]) * 2;
      }
    }
    return;
  }
  uint32_t left = bathUntil - now;
  if (left > 800) {
    // espuma: pompas meciendose y subiendo poco a poco
    float t = now / 220.0f;
    for (auto &b : bubbles) {
      int bx = b.x + (int)(sinf(t + b.ph) * 6);
      int by = b.y - (int)((3000 - left) / 90);
      gfx->fillCircle(bx, by, b.r, UI_WHITE);
      gfx->drawCircle(bx, by, b.r, 0x7E3D);
      gfx->fillCircle(bx - b.r / 3, by - b.r / 3, b.r / 4, UI_BG_DAY);
    }
  } else {
    // las pompas revientan: destellos
    for (int i = 0; i < 8; i++) {
      auto &b = bubbles[i];
      int sx = b.x + (i % 3) * 6 - 6, sy = b.y - 18;
      uint16_t col = (i % 2) ? UI_BAR_WARN : UI_WHITE;
      gfx->fillRect(sx - 6, sy - 1, 13, 3, col);
      gfx->fillRect(sx - 1, sy - 6, 3, 13, col);
    }
  }
}

// ---------- mascota PMD: comportamiento ----------

uint32_t pmdActTotalMs(const PmdAct &a) {
  uint32_t t = 0;
  for (uint8_t i = 0; i < a.frames; i++) t += a.ms[i];
  return t ? t : 100;
}

uint8_t pmdFrameAt(const PmdAct &a, uint32_t t, bool loop) {
  uint32_t total = pmdActTotalMs(a);
  if (!loop && t >= total) return a.frames - 1;
  t %= total;
  uint8_t i = 0;
  // guarda: nunca mas vueltas que frames, por si un .bin corrupto trae ms[]=0
  // pese al saneo de la carga (defensa en profundidad)
  for (uint8_t guard = 0; guard < a.frames && t >= a.ms[i]; guard++) {
    t -= a.ms[i];
    i = (i + 1) % a.frames;
  }
  return i;
}

// dibuja una accion anclada por la base (centro-x, suelo) y devuelve su escala
// dibuja una accion de un PmdMon concreto (m); drawPmdAct usa el global pmd
// fitH: alto objetivo del lienzo IDLE en px (170 = el de siempre; ko9: el perfil
// usa mas para que el retrato se vea grande)
void drawPmdActM(PmdMon &m, uint8_t actId, int cx, int groundY, uint32_t t, bool loop, bool sil, uint8_t maxS,
                 uint16_t fitH) {
  const PmdAct &a = m.acts[actId];
  if (!a.frames) return;
  uint8_t sBase = m.acts[PMD_IDLE].h ? fitH / m.acts[PMD_IDLE].h : 5;
  if (sBase < 2) sBase = 2;
  if (sBase > maxS) sBase = maxS;
  uint8_t s = sBase;
  while (s > 2 && a.h * s > (fitH > 170 ? fitH + 80 : 250)) s--;  // acciones con frame grande (ataque)
  uint8_t fi = pmdFrameAt(a, t, loop);
  const uint8_t *fr = a.data + (uint32_t)fi * a.w * a.h;
  // anclar por los pies (a.base), no por el alto del lienzo: asi las acciones
  // con padding distinto (Hurt, Eat...) quedan todas a la misma altura de suelo
  int x0 = cx - a.w * s / 2, y0 = groundY - (a.base ? a.base : a.h) * s;
  for (int r = 0; r < a.h; r++) {
    const uint8_t *row = fr + r * a.w;
    for (int c = 0; c < a.w; c++) {
      uint8_t idx = row[c];
      if (idx == 0xFF) continue;
      gfx->fillRect(x0 + c * s, y0 + r * s, s, s, sil ? INK_K : m.pal[idx]);
    }
  }
}
void drawPmdAct(uint8_t actId, int cx, int groundY, uint32_t t, bool loop, bool sil, uint8_t maxS) {
  drawPmdActM(pmd, actId, cx, groundY, t, loop, sil, maxS, 170);
}
// ko9: con alto objetivo propio (retrato grande del perfil)
void drawPmdActFit(uint8_t actId, int cx, int groundY, uint32_t t, uint8_t maxS, uint16_t fitH) {
  drawPmdActM(pmd, actId, cx, groundY, t, true, false, maxS, fitH);
}

// elige el siguiente capricho del bicho cuando esta contento
void behNext() {
  uint32_t now = millis();
  beh.t0 = now;
  int r = random(100);
  if (r < 35 && (pmd.has(PMD_WALKL) || pmd.has(PMD_WALKR))) {
    beh.mode = 1;  // paseo
    beh.targetX = 150 + random(176);
    beh.until = now + 15000;
  } else if (r < 60) {
    // gesto aleatorio entre los disponibles
    // (Hop fuera: salta demasiado alto; Sit fuera: mira hacia atras)
    static const uint8_t flair[] = { PMD_POSE, PMD_NOD, PMD_BREATH };
    uint8_t pick[3], n = 0;
    for (uint8_t f : flair)
      if (pmd.has(f)) pick[n++] = f;
    if (n) {
      beh.mode = 2;
      beh.act = pick[random(n)];
      beh.until = now + pmdActTotalMs(pmd.acts[beh.act]);
      return;
    }
    beh.mode = 0;
    beh.until = now + 2000 + random(3000);
  } else {
    beh.mode = 0;  // mirar al frente
    beh.until = now + 2000 + random(3000);
  }
}

void drawPetPMD() {
  uint32_t now = millis();

  if (pet.evolving()) {
    drawEvolveFX(now);
    return;
  }
  if (evoPmd.loaded) evoPmd.unload();  // termino la evolucion: libera la forma anterior

  PetMood m = pet.mood();
  uint8_t act;
  bool loop = true;
  if (m == MOOD_SLEEPING && pmd.has(PMD_SLEEP)) {
    act = PMD_SLEEP;
    beh.mode = 0;
  } else if (m == MOOD_EATING && pmd.has(PMD_EAT)) {
    act = PMD_EAT;
    beh.t0 = 0;
  } else if (m == MOOD_SAD && pmd.has(PMD_HURT)) {
    act = PMD_HURT;
  } else {
    // contento: el planificador decide (idle / paseo / gesto)
    if (now > beh.until) behNext();
    if (beh.mode == 1) {
      float d = beh.targetX - beh.x;
      if (fabsf(d) < 4) {
        behNext();
        act = PMD_IDLE;
      } else {
        beh.x += (d > 0 ? 3.0f : -3.0f);
        act = (d > 0) ? PMD_WALKR : PMD_WALKL;
      }
    } else {
      act = (beh.mode == 2) ? beh.act : PMD_IDLE;
      loop = false;
    }
    if (!pmd.has(act)) act = PMD_IDLE;
  }

  drawPmdAct(act, (int)beh.x, PET_GROUND, now - beh.t0, loop || act == PMD_IDLE, false, 5);

  if (pet.showHeart()) drawMap(SPR_HEART, 32, (int)beh.x + 50, PET_GROUND - 190, 2, false);
}

// sprite animado desde la SD: zoom entero por pixel, frames a su ritmo
void drawPetSD() {
  int s = mon.scale;
  int w = mon.w * s, h = mon.h * s;
  int x = CX - w / 2;
  int y = PET_CY - h / 2;

  bool sil = false;
  if (pet.evolving()) {
    sil = (millis() / 300) % 2;
  } else if (pet.mood() == MOOD_HAPPY && (millis() / 500) % 2) {
    y -= 6;  // saltito
  }

  uint16_t fm = mon.frameMs ? mon.frameMs : 100;
  uint16_t fi = pet.sleeping ? 0 : (millis() / fm) % mon.frames;
  const uint8_t *fr = mon.data + (uint32_t)fi * mon.w * mon.h;
  for (int r = 0; r < mon.h; r++) {
    const uint8_t *row = fr + r * mon.w;
    for (int c = 0; c < mon.w; c++) {
      uint8_t idx = row[c];
      if (idx == 0xFF) continue;
      gfx->fillRect(x + c * s, y + r * s, s, s, sil ? INK_K : mon.pal[idx]);
    }
  }

  // emotes en vez de expresiones (los sprites importados no tienen anclas)
  if (pet.showHeart()) drawMap(SPR_HEART, 32, x + w - 30, y - 50, 2, false);
}

// ojo cerrado: borra el ojo 3x4 y dibuja el parpado
void overlayEye(const Species &sp, int x, int y, int s, int col) {
  gfx->fillRect(x + col * s, y + sp.eyeRow * s, 3 * s, 4 * s, sp.bodyColor);
  gfx->fillRect(x + col * s, y + (sp.eyeRow + 2) * s, 3 * s, s, INK_K);
}

// borra la sonrisa base y pinta boca abierta (comer) o ceno (triste)
void overlayMouth(const Species &sp, int x, int y, int s, bool open) {
  int mc = sp.mouthCol, mr = sp.mouthRow;
  gfx->fillRect(x + (mc - 3) * s, y + mr * s, 7 * s, 2 * s, sp.bodyColor);
  if (open) {
    gfx->fillRect(x + (mc - 2) * s, y + mr * s, 5 * s, 2 * s, INK_K);
  } else {
    gfx->fillRect(x + (mc - 2) * s, y + mr * s, 5 * s, s, INK_K);
    gfx->fillRect(x + (mc - 3) * s, y + (mr + 1) * s, s, s, INK_K);
    gfx->fillRect(x + (mc + 3) * s, y + (mr + 1) * s, s, s, INK_K);
  }
}

void drawPoops() {
  for (int i = 0; i < pet.poops; i++) {
    drawMap(SPR_POOP, 32, 36 + i * 46, 244, 2, false);
  }
}

void drawBars() {
  drawBar(78, 318, T(S_BAR_FOOD), pet.fullness);
  drawBar(244, 318, T(S_BAR_JOY), pet.joy);
  drawBar(78, 346, T(S_BAR_ENE), pet.energy);
  drawBar(244, 346, T(S_BAR_HYG), pet.hygiene);
}

// Separacion entre la etiqueta y su barra en la fila de necesidades. Estaba fija
// en 48 px, justo lo que ocupan 4 letras latinas a escala 2, sin margen. Las
// etiquetas japonesas son mas anchas (ごきげん son 4 kana, ~64 px) y se metian
// dentro de la barra. Se deriva de la etiqueta mas larga, con el 48 de suelo
// para que en los idiomas latinos no cambie nada.
// Requiere el tamano de texto 2 ya puesto, porque textW() lo necesita.
static int barLabelGap() {
  const StrId ids[] = { S_BAR_FOOD, S_BAR_JOY, S_BAR_ENE, S_BAR_HYG };
  int ancho = 0;
  for (StrId id : ids) {
    int w = textW(T(id), 2);
    if (w > ancho) ancho = w;
  }
  // por debajo del umbral original se deja EXACTAMENTE 48, para no mover nada en
  // los idiomas latinos; por encima se anade aire para que la etiqueta respire
  return ancho <= 48 ? 48 : ancho + 8;
}

void drawBar(int x, int y, const char *label, uint8_t val) {
  gfx->setTextColor(inkColor());
  setSize(2);
  setCur(x, y);
  printT(label);
  int gap = barLabelGap();
  // las dos columnas empiezan en 78 y 244: la barra de la primera tiene que
  // acabar antes de la etiqueta de la segunda, y ambas se pintan igual de anchas
  int bw = 232 - (78 + gap);
  if (bw > 100) bw = 100;   // con etiquetas latinas sale 100, como estaba
  int bx = x + gap, bh = 16;
  uint16_t fill = (val >= 50) ? UI_BAR_OK : (val >= 25) ? UI_BAR_WARN : UI_BAR_BAD;
  gfx->fillRoundRect(bx, y, bw, bh, 4, UI_TRACK);
  int fw = (bw - 4) * val / 100;
  if (fw > 0) gfx->fillRoundRect(bx + 2, y + 2, fw, bh - 4, 3, fill);
  // fork KO (ko4): el valor (0-100) dentro de la barra, en negro
  char num[4];
  snprintf(num, sizeof(num), "%u", val > 100 ? 100 : val);
  gfx->setTextColor(UI_INK);
  setSize(1);
  setCur(bx + (bw - textW(num, 1)) / 2, gCjkFont ? y : y + 4);
  printT(num);
}

void drawButtons() {
  for (int i = 0; i < 4; i++) {
    bool off = pet.sleeping && i != 2;  // durmiendo solo funciona LUZ
    int bx = buttons[i].cx - BTN_HALF, by = buttons[i].cy - BTN_HALF;
    if (!pet.sleeping) gfx->fillRoundRect(bx, by, 2 * BTN_HALF, 2 * BTN_HALF, 14, UI_WHITE);
    gfx->drawRoundRect(bx, by, 2 * BTN_HALF, 2 * BTN_HALF, 14, inkColor());
    if (!off) drawMap(buttons[i].icon, 16, buttons[i].cx - 16, buttons[i].cy - 16, 2, false);
  }
}

const char *eggMsg() {
  switch (pet.eggCracks()) {
    case 0: return T(S_EGG_TOUCH);
    case 1: return T(S_EGG_MOVES);
    default: return T(S_EGG_ALMOST);
  }
}

const char *statusMsg() {
  if (pet.evolving()) return T(S_EVOLVING);
  if (bathUntil) return "Splish splash!";  // onomatopeya universal
  if (pet.sleeping) return "Zzz...";
  if (pet.eating()) return T(S_EATING);
  if (pet.showHeart()) return T(S_LIKES);
  if (pet.fullness < 25) return T(S_HUNGRY);
  if (pet.hygiene < 25) return T(S_NEEDS_BATH);
  if (pet.energy < 25) return T(S_EXHAUSTED);
  if (pet.joy < 25) return T(S_SAD);
  if (pet.weight > 60) return T(S_CHUBBY);
  if (pet.shiny && pet.ageMinutes < 15) return T(S_IS_SHINY);
  return T(S_HAPPY);
}

// dibuja un mapa de n x n pixeles escalado; silhouette=true lo pinta en tinta
void drawMap(const char *const *map, int n, int x, int y, int s, bool silhouette) {
  for (int r = 0; r < n; r++) {
    for (int c = 0; c < n; c++) {
      char ch = map[r][c];
      if (ch == '.') continue;
      gfx->fillRect(x + c * s, y + r * s, s, s, silhouette ? INK_K : spriteColor(ch));
    }
  }
}
