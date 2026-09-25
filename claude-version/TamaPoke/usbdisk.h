#pragma once
// usbdisk - fork KO: la SD de la placa como unidad USB en el PC (USB MSC).
//
// Mientras esta activa, la placa no toca la SD (sdExternal hace fallar todos los
// SdCardLock): el PC es el unico que escribe y la FAT no se corrompe. Al salir
// se remonta la SD y se recargan sprites y musica.
//
// Necesita compilar con "USB Mode: USB-OTG (TinyUSB)". Con el USB CDC/JTAG por
// hardware no hay MSC: usbDiskSupported() devuelve false.

#include <stdint.h>

bool usbDiskSupported();
bool usbDiskStart();      // false si no hay SD, no hay soporte o el audio no suelta la SD
void usbDiskStop();       // remonta la SD; llamar desde el loop principal
bool usbDiskActive();
bool usbDiskHostSeen();   // el PC ya ha leido algo en esta sesion
bool usbDiskEjected();    // el PC ha expulsado la unidad (hay que llamar a usbDiskStop)
void usbDiskStats(uint32_t *readKB, uint32_t *writeKB, uint32_t *errors);  // ko5.1: diagnostico en pantalla
