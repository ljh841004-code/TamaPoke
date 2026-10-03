#pragma once
#include <stdint.h>
#include <stddef.h>

// Efectos de sonido del juego (cola, no bloqueante). El orden coincide con la
// tabla SFX de audio.cpp.
enum Sfx : uint8_t {
  SFX_TAP = 0,  // tocar / boton
  SFX_EAT,      // comer
  SFX_PLAY,     // punto del minijuego / golpe
  SFX_HEART,    // le gusta / mimo
  SFX_HATCH,    // eclosion
  SFX_EVOLVE,   // evolucion
  SFX_MEDAL,    // medalla / hito
  SFX_DENY,     // accion no permitida
  SFX_BYE,      // despedida
  SFX_LEVEL,    // sube de nivel
  SFX_ALERT,    // ko9: aviso (caca nueva o una barra en 30 o menos)
  SFX_COUNT
};

void audioBegin();          // init ES8311 + I2S + amplificador + tarea de audio
void sfxPlay(uint8_t id);   // encola un efecto (no bloquea el loop)
void audioSetEnabled(bool on);
bool audioEnabled();
void audioSetSleeping(bool sleeping);  // dormida: amplificador apagado

// Independent 0..100 levels, saved to NVS. 0=BGM, 1=cry, 2=system.
void audioSetVolume(uint8_t channel, uint8_t percent);
uint8_t audioVolume(uint8_t channel);
void audioLoadMusic(); // enable/reload streaming music after SD mount
void audioSetBattleMusic(bool active, bool newSession = false);
// ko11: que cancion toca. En combate: MT_GYM -> battle_gym.wav, MT_CHAMP ->
// battle_champ.wav (si no estan, battle_wild.wav). Fuera: MT_FAME -> fame.wav o
// fame2.wav al azar (si no estan, bgm.wav); normal: bgm.wav o bgm2.wav al azar
enum : uint8_t { MT_NORMAL = 0, MT_GYM, MT_CHAMP, MT_FAME,
  // ko11.24: historia. Cada una prueba sus ficheros en orden y si no hay ninguno suena la de siempre
  MT_STORY,      // escenas del juego: story.wav -> bgm2.wav (Pallet Town)
  MT_STORY_A,    // escenas del anime: story_anime.wav -> story.wav -> bgm2.wav
  MT_STORY_END,  // final: story_end.wav -> fame2.wav -> fame.wav
  MT_SBATTLE,    // combate normal / oleada: story_battle.wav -> battle_wild.wav
  MT_SROCKET,    // Team Rocket: story_rocket.wav -> story_battle.wav
  MT_SGYM,       // lider / Alto Mando / jefe de oleada: story_gym.wav -> battle_gym.wav
  MT_SLEAGUE,    // ko11.31.4: escenas del juego en la Liga (13장 사천왕, 14장): story_league.wav -> story.wav -> bgm2.wav
};
void audioSetMusicTrack(uint8_t track);
// ko11.8: fondos normales elegibles: /mons/bgm.wav, bgm2.wav ... bgm8.wav. Suenan al
// azar solo los activados (mascara guardada en "bgmMask"; por defecto todos).
#define BGM_MAX 8
void audioBgmPath(uint8_t i, char *out, size_t n);  // i=0 -> /mons/bgm.wav, i -> /mons/bgm{i+1}.wav
void audioScanBgm();                  // mira en la SD cuales hay (loop principal; bloquea la SD un momento)
uint8_t audioBgmAvail();              // bits: ficheros validos encontrados
uint8_t audioBgmMask();               // bits: activados por el usuario
void audioSetBgmMask(uint8_t mask);   // guarda y, si la que suena se desactiva, cambia
uint16_t audioBgmSecondsOf(uint8_t i);
const char *audioBgmTitle(uint8_t i); // titulo (INAM del WAV) o "" si no lleva
int8_t audioBgmNow();                 // la que suena ahora (-1 si ninguna normal)
void audioBgmPlay(uint8_t i);         // escucharla ya (luego sigue el sorteo)
bool audioPauseForUpload(); // closes streaming file before PUT; false on timeout
void audioResumeAfterUpload();
// ko11.31.3: true si la musica tiene bastante leido por adelantado (o no suena): se puede usar la SD
// un rato sin que se corte. Las cargas grandes (efectos, sprites) lo miran antes de cada trozo
bool audioSdFree();
void audioCry(uint16_t dex); // main loop only
// fork KO (ko5): pantalla apagada con el PWR -> la musica se pausa (y sigue
// donde iba al encenderla). Voces y efectos siguen sonando.
void audioSetMusicPaused(bool paused);
