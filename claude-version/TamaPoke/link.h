#pragma once
// Tongsin: dos TamaPoke juntos por ESP-NOW (sin router, canal 1).
//
// Emparejamiento: los dos abren la misma pantalla (batalla o intercambio) y
// emiten HELLO por broadcast. Al oir al otro lo apuntan como pareja y lo dicen
// en su propio HELLO (peerMac); cuando cada uno se ve nombrado por el otro, el
// enlace queda LISTO.
//
// Batalla: no se envian turnos. Cada placa simula la MISMA batalla con los
// mismos datos y la misma semilla (battleAuto es determinista), ordenando los
// lados por MAC para que "a" sea el mismo en las dos. Solo cambia el punto de
// vista al dibujar.
//
// Intercambio: cada uno confirma en su pantalla y envia ACCEPT repetido; al
// tener el ACCEPT propio y el del otro se aplica el cambio (y se sigue enviando
// ACCEPT unos segundos para que el otro tambien lo complete).
#include <Arduino.h>
#include "pet.h"

enum LinkMode : uint8_t { LINK_NONE = 0, LINK_BATTLE = 1, LINK_TRADE = 2 };
enum LinkState : uint8_t {
  LS_OFF = 0, LS_SEARCH, LS_READY, LS_TRADE_WAIT, LS_TRADE_DONE, LS_DECLINED, LS_LOST, LS_ERROR,
};

// ko12.9.16: un luchador del equipo de tongsin (el que crias + hasta 2 de la caja), con SU nivel
// y sus movimientos. Las dos placas montan el combate con estos mismos datos (LINK_TEAM: battle.h)
struct __attribute__((packed)) LinkMon {
  int16_t dex;
  uint16_t lvl, atk, def, spe;
  uint8_t mv[4];
  uint8_t shiny;
};
struct __attribute__((packed)) LinkPet {
  TradePet t;                       // el individuo entero (para el intercambio)
  uint16_t lvl, atk, def, spe;      // stats de batalla tal como las calcula su dueno
  LinkMon team[LINK_TEAM];          // ko12.9.16: team[0] = el que crias
  uint8_t teamN;
};


// 2 (ko8): el apodo del intercambio pasa de 12 a 20 bytes (hangul). Con otro
// tamano de mensaje la version 1 ni se oye (se descarta por longitud).
// 3 (ko10): especies de gen 2 (152-251); un ko9 no sabria que hacer con ellas
// 4 (ko10.4): cada mensaje lleva la hora; el que la perdio la toma del otro
// 7 (ko12.9.16): equipo de 3 y combate por turnos (cada uno elige; se intercambian las acciones)
// 8 (ko12.10): movimientos originales de Cristal (otros ids) y dos tipos: no se mezcla con los de antes
#define LINK_PROTO_VER 8

void linkStart(LinkMode mode, const LinkPet &mine);
void linkStop();
bool linkActive();
LinkState linkState();
LinkMode linkMode();
const LinkPet &linkPartner();
void linkSetClock(uint32_t epoch, bool ok);      // ko10.4
bool linkPartnerClock(uint32_t *epoch);          // ko10.4: true si el otro tiene hora de fiar
const LinkPet &linkMine();  // lo que YO envie (la batalla se simula con esto)
bool linkPartnerAccepted();
bool linkIAmA();          // mi placa es el lado "a" de la simulacion
uint32_t linkSeed();      // semilla comun de la batalla
void linkTradeAccept();
void linkTradeDecline();
void linkPoll(uint32_t nowMs);
// true UNA vez cuando hay que aplicar el intercambio; out = Pokemon recibido
bool linkTakeTrade(TradePet &out);
// ko12.9.16: combate por turnos. Mi eleccion para el turno t (LA_WAIT = aun nada) y la del otro
void linkSetTurn(uint16_t turn, uint8_t act);
bool linkPartnerAct(uint16_t turn, uint8_t *act);
