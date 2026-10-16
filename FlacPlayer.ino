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
 *    USB Mode: Hardware CDC and JTAG (predefinito: il disco USB cambia porta da solo)
 *  Librerie: nessuna da installare. LVGL 9.5 è incluso nello sketch (src/lvgl,
 *  configurazione in src/lv_conf.h) e non va in conflitto con una LVGL 8 installata.
 */
#include <Arduino.h>
#include "src/i18n.h"
#include <Wire.h>
#include <SD_MMC.h>
#include <esp_heap_caps.h>
#include <esp_sleep.h>
#include <driver/gpio.h>
#include <driver/rtc_io.h>
#include <driver/usb_serial_jtag.h>
#include <sys/stat.h>
#include "src/lvgl/lvgl.h"
#include "src/config.h"
#include "src/platform.h"
#include "src/hal/display.h"
#include "src/hal/battery.h"
#include "src/net/xfer.h"
#include "src/power.h"
#include "src/clock.h"
#include "src/audio/audio_out.h"
#include "src/audio/player.h"
#include "src/lib/library.h"
#include "src/ui/ui.h"

// La UI LVGL gira nel loop(): lo stack predefinito (8 KB) è troppo piccolo.
SET_LOOP_TASK_STACK_SIZE(32 * 1024);

#define BOOT_STEP(msg) do { plat_mark(0, msg); Serial.printf("[boot] %s\n", msg); } while (0)

#include "src/fonts/fonts.h"

// ---------------------------------------------------------------------------
// Dopo un crash/blocco: il resoconto va sulla seriale e in /.onda/crash.log e il
// lettore riparte da dove eri. Solo se si blocca di nuovo appena riavviato
// (prima dei 20 s di avvio sano) si ferma sulla schermata con il resoconto,
// per non finire in un ciclo di riavvii.
// ---------------------------------------------------------------------------
static char s_crash_msg[1600];
static volatile bool s_crash_log_pending = false;
static bool s_crash_reprint = false;

static void build_crash_report(uint32_t crashes, const char *reason) {
  snprintf(s_crash_msg, sizeof s_crash_msg,
           "Riavvio dopo: %s (x%u)\nFirmware del " __DATE__ " " __TIME__ "\n%s\n%s\n"
           "Interfaccia: %s\nScansione SD: %s\nPlayer: %s",
           reason, (unsigned)crashes, plat_panic_text(), plat_blackbox_text(),
           plat_mark_get(0)[0] ? plat_mark_get(0) : "-",
           plat_mark_get(1)[0] ? plat_mark_get(1) : "-",
           plat_mark_get(2)[0] ? plat_mark_get(2) : "-");
}

static void print_crash_report() {
  Serial.print("\n=== RIAVVIO DOPO UN BLOCCO ===\n");
  for (const char *p = s_crash_msg; *p;) {
    const char *e = strchr(p, '\n');
    size_t len = e ? (size_t)(e - p) : strlen(p);
    Serial.write((const uint8_t *)p, len);
    Serial.print("\n");
    p += len + (e ? 1 : 0);
  }
  Serial.print("=== fine resoconto (anche in /.onda/crash.log) ===\n");
}

// In fondo a /.onda/crash.log (dal task della scansione, appena la SD è montata)
static void write_crash_log() {
  mkdir(APP_DIR, 0777);
  struct stat st;
  bool big = stat(APP_DIR "/crash.log", &st) == 0 && st.st_size > 64 * 1024;
  FILE *f = fopen(APP_DIR "/crash.log", big ? "w" : "a");
  if (!f) return;
  fprintf(f, "\n==========\n%s\n", s_crash_msg);
  fclose(f);
}

static void safe_mode() {
  // al prossimo avvio (RESET) non si riprende niente: è l'unico modo di uscire dal giro
  plat_kv_set("ctx_t", CTX_NONE);
  plat_kv_set("trk", 0);
  plat_kv_set("pos", 0);
  const char *msg = s_crash_msg;
  bool lcd = strncmp(plat_mark_get(0), "display", 7) != 0;
  if (lcd && display_begin()) {
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x121212), 0);
    lv_obj_t *t = lv_label_create(scr);
    lv_obj_set_style_text_font(t, &font_b16, 0);
    lv_obj_set_style_text_color(t, lv_color_hex(0xFF5F5F), 0);
    lv_label_set_text(t, TR("Avvio interrotto"));
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
    lv_label_set_text(h, TR("Fai una foto e premi RESET per riprovare"));
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

// Cosa sta consumando.
//  - sig: quando cambia, il salto di tensione della batteria non è il cavo
//    collegato o staccato (vedi battery_poll);
//  - ma: consumo stimato con valori tipici di questa scheda; serve per
//    l'autonomia e viene corretto da solo durante la scarica (battery.cpp).
static void load_probe(uint32_t *sig, int *ma) {
  OutputDev d = audio_out_active();
  bool playing = player_status().playing;
  bool wifi = xfer_status().state != XF_OFF;
  uint8_t br = display_brightness();
  *sig = (playing ? 1u : 0u) | (uint32_t)d << 1 | (uint32_t)(audio_out_volume(d) / 8) << 2 |
         (uint32_t)wifi << 7 | (uint32_t)(br / 10) << 8;
  uint32_t mhz = getCpuFrequencyMhz();
  *sig |= (mhz / 80) << 12;
  float m = 30 + 40 * mhz / 240.0f;   // ESP32-S3 (70 mA a 240 MHz) con PSRAM, logica del display, touch
  if (playing) m += 12;          // decodifica FLAC e letture dalla microSD
  m += 4 + 0.66f * br;           // retroilluminazione (~70 mA al massimo)
  if (d == DEV_SPEAKER) {
    m += 6;                      // codec e amplificatore accesi
    if (playing) {
      float g = audio_out_gain_q16() / 65536.0f;   // la potenza cresce col quadrato dell'ampiezza
      m += 150 * g * g;
    }
  } else {
    m += 12;                     // DAC PCM5102
  }
  if (wifi) m += 110;
  *ma = (int)m;
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
      if (s_sd_ok && s_crash_log_pending) {
        s_crash_log_pending = false;
        write_crash_log();
      }
      if (!lib_scan(r == 2)) s_sd_ok = false;   // nessuna SD: alla prossima richiesta rimonta
    }
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

static void request_rescan(bool force) { s_scan_req = force ? 2 : 1; }

void setup() {
  // dopo lo spegnimento (deep sleep): libera i pin tenuti fermi (retroilluminazione,
  // amplificatore) e riporta tasto e touch a GPIO normali
  bool woke = esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT1;
  gpio_deep_sleep_hold_dis();
  gpio_hold_dis((gpio_num_t)LCD_PIN_BL);
  gpio_hold_dis((gpio_num_t)AMP_PIN_EN);
  rtc_gpio_deinit((gpio_num_t)BTN_BOOT_PIN);
  rtc_gpio_deinit((gpio_num_t)TP_PIN_INT);
  clock_begin();   // fuso orario; l'ora (se impostata) sopravvive a deep sleep e riavvii
  Serial.begin(115200);
  // Se il Mac/PC non sta leggendo la porta, le stampe vengono scartate invece di
  // bloccare il programma (di default aspetterebbero fino a 100 ms ciascuna).
  Serial.setTxTimeoutMs(0);
  if (!woke) delay(1500);   // tempo per aprire il monitor seriale USB (non quando lo riaccendi col tasto)
  Serial.println("\n== " APP_NAME " - lettore FLAC (firmware del " __DATE__ " " __TIME__ ") ==");
  Serial.printf("PSRAM: %u KB liberi\n", (unsigned)(ESP.getFreePsram() / 1024));

  // Le allocazioni piccole (oggetti LVGL, indice della libreria, stringhe...) vanno
  // in PSRAM: la RAM interna (~300 KB) resta per DMA, stack e sistema operativo.
  heap_caps_malloc_extmem_enable(16);

  // lingua: quella scelta; al primo avvio quella di config.h, ma chi aveva già
  // la versione precedente (impostazioni salvate) resta in italiano
  int32_t lang = plat_kv_get("lang", -1);
  if (lang < 0) {
    bool old = plat_kv_get("vol_s", -1) >= 0 || plat_kv_get("vol_h", -1) >= 0 || plat_kv_get("ctx_t", -1) >= 0 ||
               plat_kv_get("bat_k", -1) >= 0;
    lang = old ? LANG_IT : UI_LANG_DEFAULT;
    plat_kv_set("lang", lang);
  }
  i18n_set((Lang)lang);

  const char *reason = "";
  uint32_t crashes = plat_boot_check(&reason);
  Serial.printf("Motivo dell'ultimo riavvio: %s\n", reason);
  bool early = crashes > 0 && plat_boot_early_crash();
  if (crashes > 0) {
    build_crash_report(crashes, reason);
    if (early) {
      // Caduto nei primi secondi: quasi sempre per il brano (o il punto) ripreso
      // all'avvio. Riprovarlo vorrebbe dire ricadere ogni volta: si riparte dal
      // primo brano, in pausa; alla seconda volta senza riprendere nulla.
      plat_kv_set("trk", 0);
      plat_kv_set("pos", 0);
      if (crashes >= 2) plat_kv_set("ctx_t", CTX_NONE);
    }
    if (crashes >= 3) safe_mode();   // ancora, anche senza riprendere nulla: ci si ferma qui
    print_crash_report();
    s_crash_log_pending = true;
    s_crash_reprint = true;
  }
  plat_monitor_start();

  BOOT_STEP("I2C");
  Wire.begin(I2C_PIN_SDA, I2C_PIN_SCL, I2C_HZ);

  BOOT_STEP("display");
  if (!display_begin()) Serial.println("[lcd] init fallita");

  // batteria troppo scarica per partire (a meno che sia collegato un computer)
  battery_begin();
  battery_set_load_factor(plat_kv_get("bat_k", 1000) / 1000.0f);
  int bmv = battery_read_mv();
  Serial.printf("[bat] %d mV all'avvio\n", bmv);
  if (bmv >= 2800 && bmv < BAT_BOOT_MIN_MV && !usb_serial_jtag_is_connected()) power_off(PWR_BATTERY_BOOT);
  BOOT_STEP("interfaccia");
  ui_init(request_rescan, display_set_brightness);
  if (crashes > 0) ui_crash_notice(!early);
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
  power_begin();   // tasto BOOT, spegnimento automatico dello schermo
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
  } else if (stage == 1 && s_crash_reprint && millis() - t0 > 4000) {
    s_crash_reprint = false;
    print_crash_report();   // dopo il riavvio il monitor seriale si ricollega con un po' di ritardo
  } else if (stage == 1 && millis() - t0 > 20000) {
    stage = 2;
    plat_boot_ok();   // 20 s senza problemi
    Serial.println("[boot] avvio completato senza errori");
  }
  audio_out_poll();
  static uint32_t last_sig_t = 0, sig = 0, last_k_t = 0;
  static int load_ma = 0;
  if (millis() - last_sig_t >= 250) {
    last_sig_t = millis();
    load_probe(&sig, &load_ma);
  }
  plat_where(0, 4);   // 4: batteria (lettura ADC)
  battery_poll(sig, load_ma);
  if (millis() - last_k_t >= 60000) {   // correzione del consumo imparata: si conserva
    last_k_t = millis();
    ui_pref_set("bat_k", (int32_t)(battery_load_factor() * 1000 + 0.5f));
  }
  if (stage == 2 && battery_info().critical) power_off(PWR_BATTERY);
  plat_where(0, 6);   // 6: tasto BOOT, schermo e risparmio energetico
  power_poll();
  plat_where(0, 5);   // 5: diagnostica RAM
  plat_heap_check();
  static uint32_t last_perf = 0;
  if (millis() - last_perf > 3000) {
    last_perf = millis();
    if (player_status().playing) Serial.printf("[player] %s\n", player_perf_text());
  }
  plat_where(0, 3);   // 3: attesa
  delay(4);
}
