// Gestione dell'energia: schermo che si spegne da solo (la musica continua),
// tasto BOOT come tasto di accensione, spegnimento completo (deep sleep) a
// richiesta, dopo una lunga pausa o con la batteria esaurita.
//
// Tasto BOOT:  1 clic        schermo spento / acceso (la musica continua)
//              2 clic        play / pausa (anche a schermo spento, es. in tasca)
//              tenuto 1,2 s  spegnimento completo; si riaccende premendolo di nuovo
#pragma once
#include <stdint.h>

enum PowerOffReason : uint8_t {
  PWR_USER,           // tasto tenuto premuto
  PWR_AUTO,           // in pausa e inattivo da troppo tempo
  PWR_BATTERY,        // batteria esaurita durante l'uso
  PWR_BATTERY_BOOT,   // batteria troppo scarica all'accensione
};

void power_begin();                 // dopo display e interfaccia
void power_poll();                  // dal loop, fuori da lv_timer_handler
bool power_screen_on();
void power_screen_set(bool on);     // accende/spegne lo schermo (la musica continua)
void power_off(PowerOffReason r);   // spegnimento completo: non ritorna
void power_off_later();             // dagli eventi LVGL: lo esegue power_poll
bool power_woke_from_sleep();       // questo avvio è un risveglio dal tasto

// Dal driver del touch, a ogni lettura:
bool power_touch_needed();          // false = schermo bloccato: non serve nemmeno leggere il touch
bool power_touch_filter(bool down); // false = tocco da non passare all'interfaccia

// Impostazioni (salvate)
int power_screen_timeout_s();       // 0 = mai
void power_set_screen_timeout_s(int s);
bool power_touch_wake();            // a schermo spento da solo, un tocco lo riaccende
void power_set_touch_wake(bool on);
int power_auto_off_min();           // 0 = mai
void power_set_auto_off_min(int m);
