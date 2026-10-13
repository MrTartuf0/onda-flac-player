// Interfaccia pubblica della UI (LVGL 8.4)
#pragma once
#include <stdint.h>

typedef void (*RescanFn)(bool force);
typedef void (*BrightnessFn)(uint8_t pct);

void ui_init(RescanFn rescan, BrightnessFn brightness);   // mostra la schermata di avvio
void ui_tick();             // chiamata da un lv_timer interno; esposta per il simulatore
