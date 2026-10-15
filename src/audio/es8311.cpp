#include "es8311.h"
#include <Arduino.h>
#include <Wire.h>
#include "../config.h"

namespace {

struct Coeff {
  uint32_t mclk, rate;
  uint8_t pre_div, pre_multi, adc_div, dac_div, fs_mode, lrck_h, lrck_l, bclk_div, adc_osr, dac_osr;
};

// Solo le combinazioni MCLK = 256*fs (fino a 64 kHz) e 128*fs (88.2/96 kHz)
const Coeff kCoeff[] = {
  { 2048000,  8000, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
  { 2822400, 11025, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
  { 3072000, 12000, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
  { 4096000, 16000, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
  { 5644800, 22050, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
  { 6144000, 24000, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
  { 8192000, 32000, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
  {11289600, 44100, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
  {12288000, 48000, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
  {16384000, 64000, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
  {11289600, 88200, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
  {12288000, 96000, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
};

bool wr(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(ES8311_ADDR);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

bool rd(uint8_t reg, uint8_t &val) {
  Wire.beginTransmission(ES8311_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((uint16_t)ES8311_ADDR, (uint8_t)1) != 1) return false;
  val = Wire.read();
  return true;
}

const Coeff *find(uint32_t mclk, uint32_t rate) {
  for (auto &c : kCoeff)
    if (c.mclk == mclk && c.rate == rate) return &c;
  return nullptr;
}

}  // namespace

uint32_t es8311_mclk_multiple(uint32_t sr) { return sr > 64000 ? 128 : 256; }

bool es8311_rate_supported(uint32_t sr) {
  return find(sr * es8311_mclk_multiple(sr), sr) != nullptr;
}

bool es8311_set_rate(uint32_t rate, uint32_t mclk) {
  const Coeff *c = find(mclk, rate);
  if (!c) return false;
  uint8_t v = 0;
  bool ok = rd(0x02, v);
  v &= 0x07;
  v |= (c->pre_div - 1) << 5;
  v |= c->pre_multi << 3;
  ok &= wr(0x02, v);
  ok &= wr(0x03, (c->fs_mode << 6) | c->adc_osr);
  ok &= wr(0x04, c->dac_osr);
  ok &= wr(0x05, ((c->adc_div - 1) << 4) | (c->dac_div - 1));
  ok &= rd(0x06, v);
  v &= 0xE0;
  v |= (c->bclk_div < 19) ? (c->bclk_div - 1) : c->bclk_div;
  ok &= wr(0x06, v);
  ok &= rd(0x07, v);
  v &= 0xC0;
  v |= c->lrck_h;
  ok &= wr(0x07, v);
  ok &= wr(0x08, c->lrck_l);
  return ok;
}

bool es8311_begin(uint32_t rate, uint32_t mclk) {
  bool ok = wr(0x00, 0x1F);            // reset
  delay(20);
  ok &= wr(0x00, 0x00);
  ok &= wr(0x00, 0x80);                // power on, slave mode
  if (!ok) return false;
  wr(0x01, 0x3F);                      // tutti i clock attivi, MCLK dal pin
  uint8_t r6 = 0;
  rd(0x06, r6);
  wr(0x06, r6 & ~0x20);                // SCLK non invertito
  es8311_set_rate(rate, mclk);
  // Seriale: I2S standard, parola a 32 bit (DAC in / ADC out)
  wr(0x09, (4 << 2));
  wr(0x0A, (4 << 2));
  wr(0x0D, 0x01);                      // power up analogico
  wr(0x0E, 0x02);
  wr(0x12, 0x00);                      // power up DAC
  wr(0x13, 0x10);                      // abilita uscita HP (va all'ampli SC8002B)
  wr(0x1C, 0x6A);
  wr(0x37, 0x08);                      // bypass EQ DAC
  wr(0x16, 0x00);                      // mic gain minimo (non usato)
  wr(0x17, 0x00);                      // volume ADC a zero
  es8311_set_dac_volume(0xBF);         // 0 dB: il volume è gestito in digitale
  return true;
}

void es8311_set_dac_volume(uint8_t v) { wr(0x32, v); }

void es8311_mute(bool mute) {
  uint8_t v = 0;
  if (!rd(0x31, v)) return;
  if (mute) v |= 0x60; else v &= ~0x60;
  wr(0x31, v);
}

void es8311_mic(bool on, uint8_t pga) {
  if (pga > 10) pga = 10;
  if (on) {
    wr(0x14, 0x10 | pga);   // ingresso MIC1P/N analogico, guadagno PGA
    wr(0x16, 0x00);         // nessuna amplificazione digitale aggiuntiva
    wr(0x17, 0xC8);         // volume ADC +4,5 dB (come negli esempi Freenove)
    wr(0x1C, 0x6A);         // EQ ADC escluso, offset DC tolto in digitale
  } else {
    wr(0x17, 0x00);
    wr(0x14, 0x00);
  }
}
