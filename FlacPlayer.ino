/*
 *  onda - lettore FLAC per Freenove ESP32-S3 Display (FNK0104B, 2.8" ILI9341 touch)
 *
 *  - Brani .flac dalla microSD integrata (SDMMC 4 bit)
 *  - Uscita: speaker integrato (ES8311 + SC8002B) oppure cuffie su DAC PCM5102 (I2S),
 *    con commutazione automatica quando inserisci il jack
 *  - Interfaccia LVGL ispirata a Spotify
 *
 *  Impostazioni Arduino IDE (core esp32 3.x, testato con 3.2.0):
 *    Scheda: ESP32S3 Dev Module   PSRAM: OPI PSRAM   Flash: 4MB o più
 *    Partition Scheme: Huge APP (3MB No OTA/1MB SPIFFS)   USB CDC On Boot: Enabled
 *  Librerie: nessuna da installare. LVGL 9.5 è incluso nello sketch (src/lvgl,
 *  configurazione in src/lv_conf.h) e non va in conflitto con una LVGL 8 installata.
 */
#include <Arduino.h>
#include <Wire.h>
#include <SD_MMC.h>
#include <esp_heap_caps.h>
#include <esp_sleep.h>
#include <driver/gpio.h>
#include <driver/rtc_io.h>
#include <driver/usb_serial_jtag.h>
#include "src/lvgl/lvgl.h"
#include "src/config.h"
#include "src/platform.h"
#include "src/hal/display.h"
#include "src/hal/battery.h"
#include "src/net/xfer.h"
#include "src/audio/audio_out.h"
#include "src/audio/player.h"
#include "src/lib/library.h"
#include "src/ui/ui.h"

// La UI LVGL gira nel loop(): lo stack predefinito (8 KB) è troppo piccolo.
SET_LOOP_TASK_STACK_SIZE(32 * 1024);

#define BOOT_STEP(msg) do { plat_mark(0, msg); Serial.printf("[boot] %s\n", msg); } while (0)

#include "src/fonts/fonts.h"

// ---------------------------------------------------------------------------
// Modalità diagnostica: se l'avvio precedente è finito in crash/watchdog,
// invece di ripartire in loop mostriamo sullo schermo dove si è fermato.
// ---------------------------------------------------------------------------
static void safe_mode(uint32_t crashes, const char *reason) {
  static char msg[1600];
  snprintf(msg, sizeof msg,
           "Riavvio dopo: %s (x%u)\nFirmware del " __DATE__ " " __TIME__ "\n%s\n%s\n"
           "Interfaccia: %s\nScansione SD: %s\nPlayer: %s",
           reason, (unsigned)crashes, plat_panic_text(), plat_blackbox_text(),
           plat_mark_get(0)[0] ? plat_mark_get(0) : "-",
           plat_mark_get(1)[0] ? plat_mark_get(1) : "-",
           plat_mark_get(2)[0] ? plat_mark_get(2) : "-");
  bool lcd = crashes < 4 && strncmp(plat_mark_get(0), "display", 7) != 0;
  if (lcd && display_begin()) {
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x121212), 0);
    lv_obj_t *t = lv_label_create(scr);
    lv_obj_set_style_text_font(t, &font_b16, 0);
    lv_obj_set_style_text_color(t, lv_color_hex(0xFF5F5F), 0);
    lv_label_set_text(t, "Avvio interrotto");
    lv_obj_set_pos(t, 10, 10);
    lv_obj_t *l = lv_label_create(scr);
    lv_obj_set_style_text_font(l, &font_r10, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_width(l, LCD_W - 20);
    lv_label_set_text(l, msg);
    lv_obj_set_pos(l, 10, 34);
    lv_obj_t *h = lv_label_create(scr);
    lv_obj_set_style_text_font(h, &font_r12, 0);
    lv_obj_set_style_text_color(h, lv_color_hex(0x1ED760), 0);
    lv_label_set_text(h, "Fai una foto e premi RESET per riprovare");
    lv_obj_align(h, LV_ALIGN_BOTTOM_LEFT, 10, -10);
    display_set_brightness(80);
  }
  // Stampa completa sulla seriale: si aspetta che il PC legga (fino a 200 ms a riga)
  Serial.setTxTimeoutMs(200);
  for (uint32_t n = 0;; n++) {
    if (lcd) lv_timer_handler();
    if (n % 1000 == 0) {
      Serial.print("\n=== AVVIO INTERROTTO ===\n");
      for (const char *p = msg; *p;) {
        const char *e = strchr(p, '\n');
        size_t len = e ? (size_t)(e - p) : strlen(p);
        Serial.write((const uint8_t *)p, len);
        Serial.print("\n");
        Serial.flush();
        p += len + (e ? 1 : 0);
      }
    }
    delay(5);
  }
}

// ---------------------------------------------------------------------------
// Batteria esaurita: avviso, poi standby profondo (consumo minimo). Si riaccende
// toccando lo schermo o col tasto BOOT; se la batteria è ancora troppo scarica
// mostra di nuovo l'avviso e torna in standby.
// ---------------------------------------------------------------------------
static void battery_power_off(bool at_boot) {
  Serial.printf("[bat] batteria esaurita (%d mV): standby\n", battery_read_mv());
  plat_event("batteria esaurita");
  ui_battery_empty_screen(at_boot);   // salva la sessione e mette in pausa
  for (uint32_t t0 = millis(); millis() - t0 < 5000;) {
    lv_timer_handler();
    delay(5);
  }
  if (!at_boot) {
    audio_out_suspend();
    SD_MMC.end();
  }
  display_sleep();
  pinMode(AMP_PIN_EN, OUTPUT);
  digitalWrite(AMP_PIN_EN, !AMP_ON_LEVEL);   // amplificatore spento
  gpio_hold_en((gpio_num_t)AMP_PIN_EN);
  gpio_deep_sleep_hold_en();

  // risveglio: tasto BOOT o tocco (il touch abbassa INT); se INT resta basso
  // (dito sullo schermo o touch in un'altra modalità) aspetta, poi solo BOOT
  uint64_t mask = 1ULL << BTN_BOOT_PIN;
  for (uint32_t t0 = millis(); millis() - t0 < 3000 && digitalRead(TP_PIN_INT) == LOW;) delay(10);
  if (digitalRead(TP_PIN_INT) == HIGH) mask |= 1ULL << TP_PIN_INT;
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);   // tiene le resistenze di pull-up
  for (int pin : {BTN_BOOT_PIN, TP_PIN_INT}) {
    rtc_gpio_pullup_en((gpio_num_t)pin);
    rtc_gpio_pulldown_dis((gpio_num_t)pin);
  }
  esp_sleep_enable_ext1_wakeup(mask, ESP_EXT1_WAKEUP_ANY_LOW);
  Serial.flush();
  esp_deep_sleep_start();
}

// Cosa sta consumando: quando cambia, il salto di tensione della batteria non è
// il cavo collegato o staccato (vedi battery_poll).
static uint32_t load_signature() {
  OutputDev d = audio_out_active();
  uint32_t sig = player_status().playing ? 1 : 0;
  sig |= (uint32_t)d << 1;
  sig |= (uint32_t)(audio_out_volume(d) / 8) << 2;   // gradini di volume
  sig |= (uint32_t)(xfer_status().state != XF_OFF) << 7;
  sig |= (uint32_t)(display_brightness() / 10) << 8;
  return sig;
}

static volatile uint8_t s_scan_req = 1;   // 1 = normale, 2 = forzata
static bool s_sd_ok = false;

static bool mount_sd() {
  SD_MMC.end();
  SD_MMC.setPins(SD_PIN_CLK, SD_PIN_CMD, SD_PIN_D0, SD_PIN_D1, SD_PIN_D2, SD_PIN_D3);
  if (!SD_MMC.begin(SD_MOUNT, false, false, SDMMC_FREQ_HIGHSPEED, 8)) {
    // alcune schede non reggono 40 MHz: riprova a 20 MHz
    if (!SD_MMC.begin(SD_MOUNT, false, false, SDMMC_FREQ_DEFAULT, 8)) return false;
  }
  Serial.printf("[sd] montata: %llu MB\n", SD_MMC.cardSize() / (1024ULL * 1024ULL));
  return true;
}

// La scansione gira in un task separato: l'interfaccia resta fluida e mostra l'avanzamento.
static void scan_task(void *) {
  for (;;) {
    uint8_t r = s_scan_req;
    if (r) {
      s_scan_req = 0;
      plat_mark(1, "SD: montaggio");
      if (!s_sd_ok) s_sd_ok = mount_sd();
      if (!lib_scan(r == 2)) s_sd_ok = false;   // nessuna SD: alla prossima richiesta rimonta
    }
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

static void request_rescan(bool force) { s_scan_req = force ? 2 : 1; }

void setup() {
  // dopo lo standby per batteria scarica: libera i pin tenuti fermi (retroilluminazione, amplificatore)
  gpio_deep_sleep_hold_dis();
  gpio_hold_dis((gpio_num_t)LCD_PIN_BL);
  gpio_hold_dis((gpio_num_t)AMP_PIN_EN);
  Serial.begin(115200);
  // Se il Mac/PC non sta leggendo la porta, le stampe vengono scartate invece di
  // bloccare il programma (di default aspetterebbero fino a 100 ms ciascuna).
  Serial.setTxTimeoutMs(0);
  delay(1500);   // tempo per aprire il monitor seriale USB
  Serial.println("\n== " APP_NAME " - lettore FLAC (firmware del " __DATE__ " " __TIME__ ") ==");
  Serial.printf("PSRAM: %u KB liberi\n", (unsigned)(ESP.getFreePsram() / 1024));

  // Le allocazioni piccole (oggetti LVGL, indice della libreria, stringhe...) vanno
  // in PSRAM: la RAM interna (~300 KB) resta per DMA, stack e sistema operativo.
  heap_caps_malloc_extmem_enable(16);

  const char *reason = "";
  uint32_t crashes = plat_boot_check(&reason);
  Serial.printf("Motivo dell'ultimo riavvio: %s\n", reason);
  if (crashes > 0) safe_mode(crashes, reason);
  plat_monitor_start();

  BOOT_STEP("I2C");
  Wire.begin(I2C_PIN_SDA, I2C_PIN_SCL, I2C_HZ);

  BOOT_STEP("display");
  if (!display_begin()) Serial.println("[lcd] init fallita");

  // batteria troppo scarica per partire (a meno che sia collegato un computer)
  battery_begin();
  int bmv = battery_read_mv();
  Serial.printf("[bat] %d mV all'avvio\n", bmv);
  if (bmv >= 2800 && bmv < BAT_BOOT_MIN_MV && !usb_serial_jtag_is_connected()) battery_power_off(true);
  BOOT_STEP("interfaccia");
  ui_init(request_rescan, display_set_brightness);
  lv_timer_handler();   // disegna subito la schermata di avvio

  BOOT_STEP("audio");
  if (!audio_out_begin()) Serial.println("[audio] init fallita");
  audio_out_set_volume(DEV_SPEAKER, plat_kv_get("vol_s", 60));
  audio_out_set_volume(DEV_HEADPHONES, plat_kv_get("vol_h", 45));
  audio_out_set_detect_inverted(plat_kv_get("hp_inv", 0));
  audio_out_set_pause_on_unplug(plat_kv_get("p_unpl", PAUSE_ON_UNPLUG_DEFAULT));
  audio_out_set_mode((OutputMode)plat_kv_get("mode", OUT_AUTO));

  BOOT_STEP("player");
  player_begin(lib_track_path);
  // Lo stack della scansione (24 KB) sta in PSRAM: la RAM interna serve a DMA e driver.
  if (xTaskCreatePinnedToCoreWithCaps(scan_task, "scan", 24 * 1024, nullptr, 2, nullptr, 0,
                                      MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) != pdPASS)
    xTaskCreatePinnedToCore(scan_task, "scan", 24 * 1024, nullptr, 2, nullptr, 0);
  BOOT_STEP("setup completato");
}

void loop() {
  static uint32_t t0 = millis();
  static uint8_t stage = 0;
  if (stage == 0) BOOT_STEP("loop: prima schermata");
  plat_beat(0);
  plat_where(0, 1);   // 1: LVGL (interfaccia)
  lv_timer_handler();
  plat_where(0, 2);   // 2: rilevamento cuffie / diagnostica
  if (stage == 0) {
    stage = 1;
    BOOT_STEP("loop: in esecuzione");
  } else if (stage == 1 && millis() - t0 > 20000) {
    stage = 2;
    plat_boot_ok();   // 20 s senza problemi
    Serial.println("[boot] avvio completato senza errori");
  }
  audio_out_poll();
  static uint32_t last_sig_t = 0, sig = 0;
  if (millis() - last_sig_t >= 250) {
    last_sig_t = millis();
    sig = load_signature();
  }
  battery_poll(sig);
  if (stage == 2 && battery_info().critical) battery_power_off(false);
  plat_heap_check();
  static uint32_t last_perf = 0;
  if (millis() - last_perf > 3000) {
    last_perf = millis();
    if (player_status().playing) Serial.printf("[player] %s\n", player_perf_text());
  }
  plat_where(0, 3);   // 3: attesa
  delay(4);
}
