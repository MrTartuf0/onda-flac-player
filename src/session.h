// Timer della sessione (es. highline): un cronometro con un limite. Allo scadere
// tre bip sopra la musica e un avviso, poi due bip ogni 5 minuti finché non lo
// fermi. Si comanda da Impostazioni o con 3 clic sul tasto BOOT (in tasca,
// a schermo spento: 1 bip = avviato, 2 bip = fermato).
#pragma once
#include <stdint.h>
#include <stddef.h>

void session_start(bool beep = false);
void session_stop(bool beep = false);
void session_toggle(bool beep = false);
bool session_running();
uint32_t session_elapsed_ms();
bool session_over();              // oltre il limite
int session_limit_min();          // 0 = nessun limite
void session_set_limit_min(int m);
void session_poll();              // dall'interfaccia, spesso (anche a schermo spento)
void session_fmt_limit(int min, char *buf, size_t n);   // "30 min", "1 h 30", "off"
uint32_t session_last_ms();       // durata dell'ultima sessione fermata (0 = nessuna)
