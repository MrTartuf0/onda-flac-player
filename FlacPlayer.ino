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
 *  Librerie: lvgl 8.4.0 (quella del pacchetto Freenove, con il suo lv_conf.h)
 */
#include <Arduino.h>
#include <Wire.h>
#include <SD_MMC.h>
#include <esp_heap_caps.h>
#include "lvgl.h"
#include "src/config.h"
#include "src/platform.h"
#include "src/hal/display.h"
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
  char msg[768];
  snprintf(msg, sizeof msg,
           "Riavvio dopo: %s (x%u)\n%s\n\n"
           "Interfaccia:\n%s\n\nScansione SD:\n%s\n\nPlayer:\n%s",
           reason, (unsigned)crashes, plat_panic_text(),
           plat_mark_get(0)[0] ? plat_mark_get(0) : "-",
           plat_mark_get(1)[0] ? plat_mark_get(1) : "-",
           plat_mark_get(2)[0] ? plat_mark_get(2) : "-");
  bool lcd = crashes < 4 && strncmp(plat_mark_get(0), "display", 7) != 0;
  if (lcd && display_begin()) {
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x121212), 0);
    lv_obj_t *t = lv_label_create(scr);
    lv_obj_set_style_text_font(t, &font_b16, 0);
    lv_obj_set_style_text_color(t, lv_color_hex(0xFF5F5F), 0);
    lv_label_set_text(t, "Avvio interrotto");
    lv_obj_set_pos(t, 10, 10);
    lv_obj_t *l = lv_label_create(scr);
    lv_obj_set_style_text_font(l, &font_r10, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
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
  for (uint32_t n = 0;; n++) {
    if (lcd) lv_timer_handler();
    if (n % 400 == 0) Serial.printf("\n=== AVVIO INTERROTTO ===\n%s\n", msg);
    delay(5);
  }
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
  Serial.begin(115200);
  delay(1500);   // tempo per aprire il monitor seriale USB
  Serial.println("\n== " APP_NAME " - lettore FLAC ==");
  Serial.printf("PSRAM: %u KB liberi\n", (unsigned)(ESP.getFreePsram() / 1024));

  // Le allocazioni piccole (oggetti LVGL, indice della libreria, stringhe...) vanno
  // in PSRAM: la RAM interna (~300 KB) resta per DMA, stack e sistema operativo.
  heap_caps_malloc_extmem_enable(64);

  const char *reason = "";
  uint32_t crashes = plat_boot_check(&reason);
  Serial.printf("Motivo dell'ultimo riavvio: %s\n", reason);
  if (crashes > 0) safe_mode(crashes, reason);

  BOOT_STEP("I2C");
  Wire.begin(I2C_PIN_SDA, I2C_PIN_SCL, I2C_HZ);

  BOOT_STEP("display");
  if (!display_begin()) Serial.println("[lcd] init fallita");
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
  xTaskCreatePinnedToCore(scan_task, "scan", 24 * 1024, nullptr, 2, nullptr, 0);
  BOOT_STEP("setup completato");
}

void loop() {
  static uint32_t t0 = millis();
  static uint8_t stage = 0;
  if (stage == 0) BOOT_STEP("loop: prima schermata");
  lv_timer_handler();
  if (stage == 0) {
    stage = 1;
    BOOT_STEP("loop: in esecuzione");
  } else if (stage == 1 && millis() - t0 > 20000) {
    stage = 2;
    plat_boot_ok();   // 20 s senza problemi
    Serial.println("[boot] avvio completato senza errori");
  }
  audio_out_poll();
  plat_heap_check();
  delay(4);
}
