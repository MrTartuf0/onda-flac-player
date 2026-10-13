// Display ILI9341 (SPI + DMA via esp_lcd), touch FT6336U e porting LVGL 8.
#pragma once
#include <stdint.h>

bool display_begin();                 // inizializza LCD, touch e LVGL
void display_set_brightness(uint8_t pct);
