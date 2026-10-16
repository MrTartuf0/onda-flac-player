// Disco USB: col cavo USB-C il computer vede la microSD come una chiavetta.
//
// Di solito la porta USB dell'ESP32-S3 è la seriale USB/JTAG (log e caricamento
// del firmware). Per il disco la si passa al controller USB-OTG (TinyUSB, classe
// Mass Storage) e il computer legge e scrive direttamente i settori della
// scheda: nel frattempo il lettore non deve toccare la SD. Si torna indietro
// riavviando: la seriale USB riappare e la libreria si aggiorna da sola.
#pragma once
#include <stdint.h>

enum UsbDiskState : uint8_t {
  UD_OFF,       // spento: la porta USB è la seriale
  UD_WAITING,   // scheda pronta, il computer non l'ha (ancora) aperta o è in standby
  UD_MOUNTED,   // il computer la sta usando
  UD_EJECTED,   // espulsa dal computer: si può staccare il cavo
};

struct UsbDiskStatus {
  UsbDiskState state = UD_OFF;
  uint64_t bytes_read = 0, bytes_written = 0;
  uint32_t idle_ms = 0;   // da quanto il computer non legge né scrive
  bool writing = false;   // l'ultimo accesso era una scrittura
  bool error = false;     // un accesso alla scheda è fallito
};

// Un computer (non un caricatore) è collegato alla USB-C: manda un "frame"
// ogni millisecondo. Prima del disco lo si vede dalla seriale USB/JTAG.
bool usb_disk_host_present();
// Espone la microSD al computer. La scheda deve essere montata e nessuno deve
// usarla (musica ferma, scansione finita). false: nessuna scheda o USB non avviato.
bool usb_disk_start();
UsbDiskStatus usb_disk_status();
bool usb_disk_active();   // il disco è stato avviato (fino al riavvio)
// Stacca il disco dal computer e ridà la porta alla seriale USB/JTAG.
// Da chiamare prima di un riavvio o dello spegnimento (deep sleep).
void usb_disk_release();
// Fine: stacca il disco e riavvia (sull'ESP32 non ritorna).
void usb_disk_finish();
