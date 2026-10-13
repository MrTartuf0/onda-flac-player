#include <Arduino.h>
#include "audio_out.h"
#include <Wire.h>
#include <driver/i2s_std.h>
#include <driver/gpio.h>
#include <esp_rom_gpio.h>
#include <soc/gpio_sig_map.h>
#include "es8311.h"
#include "../config.h"
#include "../platform.h"

namespace {

i2s_chan_handle_t s_tx = nullptr;
volatile uint32_t s_underruns = 0;

bool IRAM_ATTR on_underrun(i2s_chan_handle_t, i2s_event_data_t *, void *) {
  s_underruns++;
  return false;
}
uint32_t s_rate = 0;
volatile OutputMode s_mode = OUT_AUTO;
volatile OutputDev s_active = DEV_SPEAKER;
volatile bool s_hp = false;
volatile uint32_t s_events = 0;
volatile uint8_t s_vol[2] = {60, 45};
volatile bool s_muted = false;
bool s_invert = false;
bool s_pause_on_unplug = PAUSE_ON_UNPLUG_DEFAULT;
portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

// stato del debounce
int s_last_raw = -1;
uint32_t s_raw_since = 0;

void route_mirror_pins() {
  // Duplica BCK / WS / DATA dell'I2S0 sui pin del PCM5102.
  const struct { int pin; uint32_t sig; } map[] = {
    {DAC_PIN_BCK, I2S0O_BCK_OUT_IDX},
    {DAC_PIN_LRCK, I2S0O_WS_OUT_IDX},
    {DAC_PIN_DIN, I2S0O_SD_OUT_IDX},
  };
  for (auto &m : map) {
    if (m.pin < 0) continue;
    esp_rom_gpio_pad_select_gpio(m.pin);
    gpio_set_direction((gpio_num_t)m.pin, GPIO_MODE_OUTPUT);
    gpio_set_drive_capability((gpio_num_t)m.pin, GPIO_DRIVE_CAP_2);
    esp_rom_gpio_connect_out_signal(m.pin, m.sig, false, false);
  }
}

void apply_device(OutputDev d) {
  if (d == DEV_SPEAKER) {
    es8311_mute(false);
    digitalWrite(AMP_PIN_EN, AMP_ON_LEVEL);
  } else {
    digitalWrite(AMP_PIN_EN, !AMP_ON_LEVEL);
    es8311_mute(true);   // meno consumi/rumore: lo speaker non serve
  }
}

void recompute_active() {
  OutputDev want;
  if (s_mode == OUT_SPEAKER) want = DEV_SPEAKER;
  else if (s_mode == OUT_HEADPHONES) want = DEV_HEADPHONES;
  else want = s_hp ? DEV_HEADPHONES : DEV_SPEAKER;
  if (want != s_active) {
    s_active = want;
    apply_device(want);
    portENTER_CRITICAL(&s_mux);
    s_events |= AOUT_EV_DEVICE_CHANGED;
    portEXIT_CRITICAL(&s_mux);
  }
}

int read_raw_hp() {
#if HP_DETECT_PIN >= 0
  int lvl = digitalRead(HP_DETECT_PIN);
  bool plugged = (lvl == HP_DETECT_PLUGGED);
  if (s_invert) plugged = !plugged;
  return plugged ? 1 : 0;
#else
  return 0;
#endif
}

i2s_std_clk_config_t clk_for(uint32_t rate) {
  i2s_std_clk_config_t c = I2S_STD_CLK_DEFAULT_CONFIG(rate);
  c.mclk_multiple = es8311_mclk_multiple(rate) == 128 ? I2S_MCLK_MULTIPLE_128 : I2S_MCLK_MULTIPLE_256;
  return c;
}

}  // namespace

bool audio_out_begin() {
  pinMode(AMP_PIN_EN, OUTPUT);
  digitalWrite(AMP_PIN_EN, !AMP_ON_LEVEL);   // ampli spento finché non è tutto pronto
#if HP_DETECT_PIN >= 0
  pinMode(HP_DETECT_PIN, INPUT_PULLUP);
#endif

  plat_mark(0, "audio: I2S");
  i2s_chan_config_t cc = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  cc.dma_desc_num = 8;   // ~90 ms di buffer a 44.1 kHz
  cc.dma_frame_num = 511;       // 511 frame * 8 byte = 4088 byte per descrittore
  cc.auto_clear = true;         // silenzio in caso di underrun / pausa
  if (i2s_new_channel(&cc, &s_tx, nullptr) != ESP_OK) return false;
  i2s_event_callbacks_t cbs = {};
  cbs.on_send_q_ovf = on_underrun;
  i2s_channel_register_event_callback(s_tx, &cbs, nullptr);

  s_rate = 44100;
  i2s_std_config_t sc = {
    .clk_cfg = clk_for(s_rate),
    .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO),
    .gpio_cfg = {
      .mclk = (gpio_num_t)I2S_PIN_MCLK,
      .bclk = (gpio_num_t)I2S_PIN_BCLK,
      .ws = (gpio_num_t)I2S_PIN_WS,
      .dout = (gpio_num_t)I2S_PIN_DOUT,
      .din = I2S_GPIO_UNUSED,
      .invert_flags = {false, false, false},
    },
  };
  if (i2s_channel_init_std_mode(s_tx, &sc) != ESP_OK) return false;
  if (i2s_channel_enable(s_tx) != ESP_OK) return false;
  route_mirror_pins();

  plat_mark(0, "audio: codec ES8311");
  // Il codec vuole MCLK attivo prima della configurazione
  delay(5);
  bool codec_ok = es8311_begin(s_rate, s_rate * es8311_mclk_multiple(s_rate));
  if (!codec_ok) Serial.println("[audio] ES8311 non risponde su I2C");

  s_last_raw = read_raw_hp();
  s_hp = s_last_raw;
  s_raw_since = millis();
  s_active = (s_mode == OUT_HEADPHONES || (s_mode == OUT_AUTO && s_hp)) ? DEV_HEADPHONES : DEV_SPEAKER;
  apply_device(s_active);
  return true;
}

uint32_t audio_out_rate() { return s_rate; }
uint32_t audio_out_underruns() { return s_underruns; }
uint32_t audio_out_max_rate() { return 96000; }

bool audio_out_set_rate(uint32_t rate) {
  if (!s_tx) return false;
  if (rate == s_rate) return true;
  if (!es8311_rate_supported(rate)) return false;
  bool spk = (s_active == DEV_SPEAKER);
  if (spk) digitalWrite(AMP_PIN_EN, !AMP_ON_LEVEL);   // niente "pop" durante il cambio
  i2s_channel_disable(s_tx);
  i2s_std_clk_config_t c = clk_for(rate);
  esp_err_t e = i2s_channel_reconfig_std_clock(s_tx, &c);
  i2s_channel_enable(s_tx);
  if (e == ESP_OK) {
    s_rate = rate;
    es8311_set_rate(rate, rate * es8311_mclk_multiple(rate));
  }
  if (spk) {
    delay(2);
    digitalWrite(AMP_PIN_EN, AMP_ON_LEVEL);
  }
  return e == ESP_OK;
}

size_t audio_out_write(const int32_t *frames, size_t n, uint32_t timeout_ms) {
  if (!s_tx) return 0;
  size_t written = 0;
  i2s_channel_write(s_tx, frames, n * 8, &written, pdMS_TO_TICKS(timeout_ms));
  return written / 8;
}

void audio_out_poll() {
#if HP_DETECT_PIN >= 0
  int raw = read_raw_hp();
  uint32_t now = millis();
  if (raw != s_last_raw) {
    s_last_raw = raw;
    s_raw_since = now;
    return;
  }
  if ((bool)raw == s_hp || now - s_raw_since < HP_DETECT_DEBOUNCE) return;

  s_hp = raw;
  uint32_t ev = raw ? AOUT_EV_HP_PLUGGED : AOUT_EV_HP_UNPLUGGED;
  if (!raw && s_mode == OUT_AUTO && s_pause_on_unplug && s_active == DEV_HEADPHONES) {
    // Muto subito, prima di accendere lo speaker: niente musica "sparata" in faccia.
    s_muted = true;
  }
  portENTER_CRITICAL(&s_mux);
  s_events |= ev;
  portEXIT_CRITICAL(&s_mux);
  recompute_active();
#endif
}

uint32_t audio_out_take_events() {
  portENTER_CRITICAL(&s_mux);
  uint32_t e = s_events;
  s_events = 0;
  portEXIT_CRITICAL(&s_mux);
  return e;
}

OutputDev audio_out_active() { return s_active; }
bool audio_out_hp_present() { return s_hp; }
bool audio_out_hp_detect_available() { return HP_DETECT_PIN >= 0; }

void audio_out_set_mode(OutputMode m) {
  s_mode = m;
  recompute_active();
}
OutputMode audio_out_mode() { return s_mode; }

void audio_out_set_detect_inverted(bool inv) {
  if (inv == s_invert) return;
  s_invert = inv;
  s_last_raw = read_raw_hp();
  s_hp = s_last_raw;
  recompute_active();
}
bool audio_out_detect_inverted() { return s_invert; }
void audio_out_set_pause_on_unplug(bool en) { s_pause_on_unplug = en; }
bool audio_out_pause_on_unplug() { return s_pause_on_unplug; }

void audio_out_set_volume(OutputDev d, uint8_t v) { s_vol[d] = v > 100 ? 100 : v; }
uint8_t audio_out_volume(OutputDev d) { return s_vol[d]; }

int32_t audio_out_gain_q16() {
  if (s_muted) return 0;
  float v = s_vol[s_active] / 100.0f;
  // curva ~logaritmica: -60 dB .. 0 dB
  float g = v <= 0.0f ? 0.0f : v * v * v;
  return (int32_t)(g * 65536.0f + 0.5f);
}

bool audio_out_muted() { return s_muted; }
void audio_out_clear_mute() { s_muted = false; }
