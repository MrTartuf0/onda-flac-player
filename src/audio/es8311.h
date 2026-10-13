// Driver minimale ES8311 (solo DAC/riproduzione) per la scheda Freenove FNK0104.
// Tabella coefficienti di clock derivata dal driver Espressif (Apache-2.0).
#pragma once
#include <stdint.h>
#include <stdbool.h>

bool es8311_begin(uint32_t sample_rate, uint32_t mclk_hz);
bool es8311_set_rate(uint32_t sample_rate, uint32_t mclk_hz);
void es8311_set_dac_volume(uint8_t reg_value);   // 0xBF = 0 dB, passo 0.5 dB
void es8311_mute(bool mute);
bool es8311_rate_supported(uint32_t sample_rate);
// MCLK consigliato (multiplo di fs) per un dato sample-rate: 256 o 128
uint32_t es8311_mclk_multiple(uint32_t sample_rate);
