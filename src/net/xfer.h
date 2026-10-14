// Trasferimento file via Wi-Fi: server web per caricare e gestire i file
// della microSD dal browser (computer o telefono).
//
// Rete: se c'è una rete di casa salvata il lettore si collega a quella
// (indirizzo http://onda.local o l'IP); altrimenti, o se la connessione
// fallisce, crea un suo hotspot "onda-XXXX" (http://192.168.4.1).
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <vector>

enum XferState : uint8_t {
  XF_OFF,          // spento
  XF_CONNECTING,   // collegamento alla rete di casa in corso
  XF_STA,          // collegato alla rete di casa
  XF_AP,           // hotspot del lettore attivo
  XF_ERROR,        // non è stato possibile avviare il Wi-Fi
};

struct XferStatus {
  XferState state = XF_OFF;
  char ssid[33] = "";       // rete di casa (STA) o nome dell'hotspot (AP)
  char ap_pass[16] = "";    // password dell'hotspot
  char ip[16] = "";
  char note[72] = "";       // es. "Impossibile collegarsi a Casa: uso l'hotspot"
  char cur_name[64] = "";   // file in ricezione ("" = nessuno)
  uint32_t cur_done = 0, cur_total = 0;
  uint32_t files_in = 0;    // file ricevuti
  uint64_t bytes_in = 0;
  uint32_t changes = 0;     // modifiche ai file (caricamenti, cancellazioni, spostamenti...)
  bool done_req = false;    // dal browser: "Ho finito" -> chiudere e aggiornare la libreria
};

struct XferNet {
  char ssid[33];
  int8_t rssi;
  bool secure;
};

void xfer_start();                                    // avvia Wi-Fi + server (asincrono)
void xfer_stop();                                     // spegne tutto (attende al massimo ~3 s)
XferStatus xfer_status();
void xfer_connect(const char *ssid, const char *pass);   // prova una rete; se va bene la salva
void xfer_use_hotspot();                              // dimentica la rete di casa, usa l'hotspot
void xfer_saved_network(char *ssid, size_t n);        // rete di casa salvata ("" = nessuna)
void xfer_scan_start();
bool xfer_scan_result(std::vector<XferNet> &out);     // true = scansione finita
