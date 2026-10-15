// Display ILI9341 (SPI + DMA via esp_lcd), touch FT6336U e porting LVGL 9.
#pragma once
#include <stdint.h>

bool display_begin();                 // inizializza LCD, touch e LVGL
void display_set_brightness(uint8_t pct);
uint8_t display_brightness();          // luminosità effettiva (0 = schermo spento)
uint8_t display_brightness_setting();  // luminosità scelta, anche a schermo spento
void display_panel(bool on);           // spegne/riaccende pannello e retroilluminazione
void display_sleep();                 // spegne retroilluminazione e pannello (prima del deep sleep)
