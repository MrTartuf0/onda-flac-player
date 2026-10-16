// Timer: conto alla rovescia da una durata precisa (es. 9:00 per la pasta, 30:00
// sulla highline) oppure, con durata 0, cronometro che conta in avanti.
// Allo scadere: 3 bip sopra la musica (o da soli in pausa), lo schermo si
// riaccende e si apre la schermata del timer; poi di nuovo 3 bip ogni 30 secondi
// finché non lo fermi. Si può mettere in pausa e allungare di un minuto.
//
// Si comanda dalla schermata Timer (Impostazioni → Timer) o con 3 clic sul tasto
// BOOT (in tasca, a schermo spento: 1 bip = avviato con l'ultima durata, 2 bip = fermato).
// (Il file si chiama ancora "session": era il timer della sessione di highline.)
#pragma once
#include <stdint.h>
#include <stddef.h>

void session_start(bool beep = false);   // con la durata impostata
void session_stop(bool beep = false);
void session_toggle(bool beep = false);
void session_pause();
void session_resume();
void session_add_s(int s);               // +1 minuto (anche a tempo scaduto: riparte da lì)

bool session_running();                  // avviato: anche in pausa o scaduto
bool session_paused();
bool session_over();                     // oltre la fine (solo conto alla rovescia)
uint32_t session_elapsed_ms();           // tempo trascorso, pause escluse
int32_t session_remaining_ms();          // alla fine; negativo = da quanto è scaduto
uint32_t session_run_duration_s();       // durata della corsa attuale (0 = cronometro)
uint32_t session_segment_ms();           // lunghezza del tratto che l'anello rappresenta

uint32_t session_duration_s();           // durata impostata (0 = cronometro)
void session_set_duration_s(uint32_t s);
uint32_t session_recent_s(int i);        // durate usate di recente (0 = nessuna), i < 4
uint32_t session_last_ms();              // durata dell'ultima corsa fermata (0 = nessuna)

void session_poll();                     // dall'interfaccia, spesso (anche a schermo spento)
// "9:00", "1:05:00" (senza segno); con sign_plus "+0:23"
void session_fmt(uint32_t s, char *buf, size_t n, bool sign_plus = false);
