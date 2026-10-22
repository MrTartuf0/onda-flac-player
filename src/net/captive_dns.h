// Risposte DNS dell'hotspot (captive portal): qualunque nome -> l'indirizzo del
// lettore. Il telefono, appena collegato, prova a raggiungere la sua pagina di
// controllo (captive.apple.com, connectivitycheck.gstatic.com, ...), arriva qui,
// riceve un rimando alla pagina del lettore e la apre da solo.
// Senza dipendenze dall'ESP32: si prova anche sul PC.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

// Risposta alla domanda q (n byte) in out. Ai nomi di tipo A (e ANY) risponde con
// ip (4 byte, già in ordine di rete); agli altri tipi (AAAA, HTTPS, ...) con una
// risposta vuota, così il telefono usa l'IPv4. Ritorna la lunghezza della
// risposta, oppure 0 se il pacchetto non è una domanda da considerare.
inline size_t captive_dns_reply(const uint8_t *q, size_t n, uint8_t *out, size_t cap, const uint8_t ip[4]) {
  if (n < 12 || n + 16 > cap) return 0;
  if (q[2] & 0x80) return 0;                 // è una risposta, non una domanda
  if ((q[2] >> 3) & 0x0F) return 0;          // non è una QUERY normale
  if (q[4] != 0 || q[5] != 1) return 0;      // una domanda sola
  size_t p = 12;
  while (p < n && q[p]) {
    if (q[p] & 0xC0) return 0;               // niente compressione nella domanda
    p += q[p] + 1;
  }
  if (p + 5 > n) return 0;                   // nome troncato
  p++;                                       // zero finale del nome
  uint16_t qtype = (q[p] << 8) | q[p + 1];
  uint16_t qclass = ((q[p + 2] << 8) | q[p + 3]) & 0x7FFF;
  p += 4;
  memcpy(out, q, p);                         // intestazione e domanda; i record aggiuntivi (EDNS) no
  out[2] = 0x84 | (q[2] & 0x01);            // risposta, autorevole, RD come nella domanda
  out[3] = 0x80;                             // ricorsione disponibile, nessun errore
  memset(out + 6, 0, 6);                     // risposte, autorità, aggiuntivi: 0
  if ((qtype == 1 || qtype == 255) && qclass == 1) {
    static const uint8_t rr[12] = {0xC0, 0x0C,   // il nome della domanda
                                   0, 1, 0, 1,   // A, IN
                                   0, 0, 0, 30,  // 30 s: lasciato l'hotspot si dimentica presto
                                   0, 4};
    memcpy(out + p, rr, sizeof rr);
    memcpy(out + p + sizeof rr, ip, 4);
    p += sizeof rr + 4;
    out[7] = 1;
  }
  return p;
}
