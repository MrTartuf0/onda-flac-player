#include "power.h"
#include "config.h"
#include "platform.h"
#include "ui/ui.h"
#include "session.h"

namespace {
int s_timeout_s = 30;
bool s_touch_wake = true;
int s_auto_off_min = 10;
}  // namespace

int power_screen_timeout_s() { return s_timeout_s; }
bool power_touch_wake() { return s_touch_wake; }
int power_auto_off_min() { return s_auto_off_min; }

void power_set_screen_timeout_s(int s) {
  s_timeout_s = s < 0 ? 0 : s;
  ui_pref_set("scr_to", s_timeout_s);
}
void power_set_touch_wake(bool on) {
  s_touch_wake = on;
  ui_pref_set("scr_tw", on);
}
void power_set_auto_off_min(int m) {
  s_auto_off_min = m < 0 ? 0 : m;
  ui_pref_set("auto_off", s_auto_off_min);
}

#ifdef ARDUINO
#include <Arduino.h>
#include <stdio.h>
#include <SD_MMC.h>
#include <esp_sleep.h>
#include <driver/gpio.h>
#include <driver/rtc_io.h>
#include "lvgl/lvgl.h"
#include "hal/display.h"
#include "hal/battery.h"
#include "audio/audio_out.h"
#include "audio/player.h"
#include "net/xfer.h"

namespace {

enum OffBy : uint8_t { ON, OFF_BUTTON, OFF_TIMEOUT };
volatile OffBy s_off = ON;
volatile bool s_wake_req = false;   // chiesto dal touch (dentro LVGL): si esegue in power_poll
volatile bool s_off_req = false;    // "Spegni ora" (evento LVGL): idem
bool s_swallow = false;             // tocco che ha riacceso lo schermo: non va all'interfaccia
uint32_t s_last_busy = 0;           // ultimo momento in cui suonava o lo si usava
bool s_woke = false;

// tasto
bool s_btn = false, s_long = false;
uint32_t s_btn_t = 0, s_up_t = 0;
int s_clicks = 0;

constexpr uint32_t kLongMs = 1200, kDoubleMs = 320, kDebounceMs = 25;

uint32_t cpu_target() {
#if SCREEN_OFF_CPU_MHZ > 0
  if (s_off == ON || xfer_status().state != XF_OFF) return 240;
  // file oltre i 48 kHz: più lavoro per il decoder, si resta più alti
  PlayerStatus ps = player_status();
  return ps.sample_rate > 48000 ? 160 : SCREEN_OFF_CPU_MHZ;
#else
  return 240;
#endif
}

void apply_cpu() {
  uint32_t want = cpu_target();
  if (getCpuFrequencyMhz() == want) return;
  setCpuFrequencyMhz(want);
  char b[8];
  snprintf(b, sizeof b, "%u", (unsigned)want);
  plat_event("cpu MHz", b);
}

void screen_off(OffBy by) {
  if (s_off != ON) {
    s_off = by;   // da spento-per-inattività a bloccato col tasto
    return;
  }
  s_off = by;
  lv_display_enable_invalidation(lv_display_get_default(), false);
  display_panel(false);
  plat_event(by == OFF_BUTTON ? "schermo spento (tasto)" : "schermo spento");
  apply_cpu();
}

void screen_on() {
  if (s_off == ON) return;
  s_off = ON;
  apply_cpu();   // prima si torna veloci, poi si ridisegna
  display_panel(true);
  lv_display_t *d = lv_display_get_default();
  lv_display_enable_invalidation(d, true);
  lv_obj_invalidate(lv_screen_active());
  lv_obj_invalidate(lv_layer_top());
  lv_refr_now(d);
  display_set_brightness(display_brightness_setting());   // retroilluminazione solo a disegno fatto
  lv_display_trigger_activity(d);
  plat_event("schermo acceso");
}

void wait_release(uint32_t max_ms) {
  for (uint32_t t0 = millis(); millis() - t0 < max_ms && digitalRead(BTN_BOOT_PIN) == LOW;) {
    lv_timer_handler();
    delay(10);
  }
}

void button_poll(uint32_t now) {
  bool down = digitalRead(BTN_BOOT_PIN) == LOW;
  if (down != s_btn) {
    if (now - s_btn_t < kDebounceMs) return;
    s_btn = down;
    s_btn_t = now;
    if (down) {
      s_long = false;
      s_last_busy = now;
    } else if (!s_long) {
      s_clicks++;
      s_up_t = now;
    }
    return;
  }
  if (down && !s_long && now - s_btn_t >= kLongMs) {
    s_long = true;
    s_clicks = 0;
    power_off(PWR_USER);
  }
  if (!down && s_clicks && now - s_up_t > kDoubleMs) {
    if (s_clicks >= 3) {
      plat_event("tasto: timer sessione");
      session_toggle(true);   // timer: 1 bip = avviato, 2 bip = fermato
    } else if (s_clicks == 2) {
      plat_event("tasto: play/pausa");
      player_toggle();
    } else if (s_off == ON) {
      screen_off(OFF_BUTTON);
    } else {
      screen_on();
    }
    s_clicks = 0;
    lv_display_trigger_activity(nullptr);
  }
}

}  // namespace

bool power_woke_from_sleep() { return s_woke; }
bool power_screen_on() { return s_off == ON; }
void power_screen_set(bool on) {
  if (on) screen_on();
  else screen_off(OFF_BUTTON);
}
void power_wake_for_alert() {
  if (s_off == OFF_TIMEOUT) s_wake_req = true;   // lo accende power_poll; bloccato col tasto (in tasca) resta spento
}

void power_off_later() { s_off_req = true; }

void power_begin() {
  s_woke = esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT1;
  s_timeout_s = plat_kv_get("scr_to", 30);
  s_touch_wake = plat_kv_get("scr_tw", 1) != 0;
  s_auto_off_min = plat_kv_get("auto_off", 10);
  pinMode(BTN_BOOT_PIN, INPUT_PULLUP);
  s_btn = digitalRead(BTN_BOOT_PIN) == LOW;   // ancora premuto dal risveglio: non conta come clic
  s_long = s_btn;
  s_btn_t = millis();
  s_last_busy = millis();
}

bool power_touch_needed() {
  return s_off == ON || s_swallow || (s_off == OFF_TIMEOUT && s_touch_wake);
}

bool power_touch_filter(bool down) {
  if (s_swallow) {
    if (!down) s_swallow = false;
    return false;
  }
  if (s_off == ON) return true;
  if (down && s_off == OFF_TIMEOUT && s_touch_wake) {
    s_wake_req = true;
    s_swallow = true;
  }
  return false;
}

void power_poll() {
  uint32_t now = millis();
  button_poll(now);
  if (s_off_req) power_off(PWR_USER);
  if (s_wake_req) {
    s_wake_req = false;
    screen_on();
  }
  bool playing = player_status().playing;
  bool xfer = xfer_status().state != XF_OFF;
  if (playing || xfer) s_last_busy = now;
  lv_display_t *d = lv_display_get_default();
  uint32_t idle = lv_display_get_inactive_time(d);
  if (idle < now - s_last_busy) s_last_busy = now - idle;   // anche un tocco è attività

  // schermo spento dopo un po' (non durante il trasferimento file: c'è il codice QR)
  if (s_off == ON && s_timeout_s > 0 && !xfer && idle > (uint32_t)s_timeout_s * 1000) screen_off(OFF_TIMEOUT);

  // in pausa e a batteria da troppo tempo: spegnimento completo
  if (s_auto_off_min > 0 && !playing && !xfer && !session_running() &&
      now - s_last_busy > (uint32_t)s_auto_off_min * 60000 && battery_info().state == BAT_DISCHARGING)
    power_off(PWR_AUTO);

  // la frequenza giusta può cambiare col brano (es. un file a 96 kHz a schermo spento)
  static uint32_t last_cpu = 0;
  if (now - last_cpu > 1000) {
    last_cpu = now;
    apply_cpu();
  }
}

void power_off(PowerOffReason r) {
  pinMode(BTN_BOOT_PIN, INPUT_PULLUP);
  static const char *names[] = {"spegnimento", "spegnimento automatico", "batteria esaurita",
                                "batteria esaurita (avvio)"};
  Serial.printf("[power] %s (%d mV)\n", names[r], battery_read_mv());
  plat_event(names[r]);
  bool battery = r == PWR_BATTERY || r == PWR_BATTERY_BOOT;
  if (r != PWR_BATTERY_BOOT && s_off != ON) screen_on();
  if (battery) ui_battery_empty_screen(r == PWR_BATTERY_BOOT);   // salva la sessione e mette in pausa
  else ui_power_off_screen();
  uint32_t show = battery ? 5000 : 1200;
  for (uint32_t t0 = millis(); millis() - t0 < show;) {
    lv_timer_handler();
    delay(5);
  }
  wait_release(15000);   // col tasto ancora premuto si riaccenderebbe subito
  if (r != PWR_BATTERY_BOOT) {
    xfer_stop();   // chiude upload e Wi-Fi prima di staccare la scheda
    audio_out_suspend();
    SD_MMC.end();
  }
  display_sleep();
  pinMode(AMP_PIN_EN, OUTPUT);
  digitalWrite(AMP_PIN_EN, !AMP_ON_LEVEL);   // amplificatore spento
  gpio_hold_en((gpio_num_t)AMP_PIN_EN);
  gpio_deep_sleep_hold_en();

  // risveglio: sempre col tasto BOOT; con la batteria esaurita anche toccando lo
  // schermo (il touch abbassa INT). In tasca un tocco non deve riaccenderlo.
  uint64_t mask = 1ULL << BTN_BOOT_PIN;
  if (battery) {
    for (uint32_t t0 = millis(); millis() - t0 < 3000 && digitalRead(TP_PIN_INT) == LOW;) delay(10);
    if (digitalRead(TP_PIN_INT) == HIGH) mask |= 1ULL << TP_PIN_INT;
  }
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);   // tiene le resistenze di pull-up
  for (int pin : {BTN_BOOT_PIN, TP_PIN_INT}) {
    rtc_gpio_pullup_en((gpio_num_t)pin);
    rtc_gpio_pulldown_dis((gpio_num_t)pin);
  }
  esp_sleep_enable_ext1_wakeup(mask, ESP_EXT1_WAKEUP_ANY_LOW);
  Serial.flush();
  esp_deep_sleep_start();
}

#else  // ------------------------------------------------- simulatore (PC) ----
static bool s_scr = true;
void power_begin() {}
void power_poll() {}
bool power_screen_on() { return s_scr; }
void power_screen_set(bool on) { s_scr = on; }
void power_wake_for_alert() { s_scr = true; }
void power_off(PowerOffReason) {}
void power_off_later() {}
bool power_woke_from_sleep() { return false; }
bool power_touch_needed() { return true; }
bool power_touch_filter(bool) { return true; }
#endif
