#pragma once
#include <stdint.h>

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
bool audioPauseForUpload(); // closes streaming file before PUT; false on timeout
void audioResumeAfterUpload();
void audioCry(uint16_t dex); // main loop only
// fork KO (ko5): pantalla apagada con el PWR -> la musica se pausa (y sigue
// donde iba al encenderla). Voces y efectos siguen sonando.
void audioSetMusicPaused(bool paused);
