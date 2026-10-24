// Display ILI9341 (SPI + DMA via esp_lcd), touch FT6336U e porting LVGL 9.
#pragma once
#include <stdint.h>

bool display_begin();                 // inizializza LCD, touch e LVGL
void display_set_brightness(uint8_t pct);   // toglie anche l'attenuazione
uint8_t display_brightness();          // luminosità effettiva (0 = schermo spento, attenuata compresa)
uint8_t display_brightness_setting();  // luminosità scelta, anche a schermo spento
void display_dim(bool on);             // attenua (dissolvenza con display_tick) / torna subito alla luminosità scelta
void display_tick(uint32_t now_ms);    // dal loop: fa avanzare la dissolvenza
void display_panel(bool on);           // spegne/riaccende pannello e retroilluminazione
void display_sleep();                 // spegne retroilluminazione e pannello (prima del deep sleep)
// Schermo girato di 180° (es. agganciato all'imbracatura a testa in giù): immagine e
// touch. display_begin lo legge dalle impostazioni ("flip"); questa lo cambia subito.
void display_set_flip(bool on);
bool display_flipped();
