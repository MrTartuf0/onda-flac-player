// Interfaccia pubblica della UI (LVGL 9)
#pragma once
#include <stdint.h>

typedef void (*RescanFn)(bool force);
typedef void (*BrightnessFn)(uint8_t pct);

void ui_init(RescanFn rescan, BrightnessFn brightness);   // mostra la schermata di avvio
void ui_tick();             // chiamata da un lv_timer interno; esposta per il simulatore
// Batteria esaurita: salva la sessione, mette in pausa e mostra l'avviso a tutto
// schermo (at_boot: all'accensione, prima che il resto dell'interfaccia esista).
void ui_battery_empty_screen(bool at_boot);
// Spegnimento dal tasto o automatico: salva la sessione, pausa, avviso
void ui_power_off_screen();
// Salva una preferenza in flash; durante la riproduzione la scrittura si rimanda
// alla pausa o al cambio brano (non scrive se il valore non è cambiato).
void ui_pref_set(const char *key, int32_t v);
void ui_crash_notice(bool resumed);   // avvio dopo un blocco: avvisa quando l'interfaccia è pronta
