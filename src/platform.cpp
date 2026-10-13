#include "platform.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef ARDUINO
#include <Arduino.h>
#include <Preferences.h>
#include <esp_heap_caps.h>

static Preferences s_prefs;
static bool s_prefs_open = false;

static Preferences &prefs() {
  if (!s_prefs_open) {
    s_prefs.begin("onda", false);
    s_prefs_open = true;
  }
  return s_prefs;
}

void *plat_alloc_big(size_t n) {
  void *p = heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  return p ? p : malloc(n);
}
void *plat_alloc_fast(size_t n) {
  void *p = heap_caps_malloc(n, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  return p ? p : malloc(n);
}
void plat_free(void *p) { heap_caps_free(p); }
uint32_t plat_millis() { return millis(); }
void plat_yield() { vTaskDelay(1); }
void plat_log(const char *fmt, ...) {
  char buf[256];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof buf, fmt, ap);
  va_end(ap);
  Serial.println(buf);
}
int32_t plat_kv_get(const char *key, int32_t def) { return prefs().getInt(key, def); }

#include <esp_system.h>
#include <esp_attr.h>
#include <esp_task_wdt.h>
struct BootCrumbs {
  uint32_t magic;
  uint32_t crashes;
  char step[3][64];
  char panic_reason[40];
  uint32_t panic_core;
  uint32_t bt_len;
  uint32_t bt[12];
  uint32_t min_internal;
  char wdt_tasks[48];   // task in esecuzione sui due core quando scatta il watchdog
  char stall[96];       // ultimo blocco del core 0 visto dal monitor (core 1)
  char nvs[24];         // scrittura NVS (flash) in corso, vuoto se nessuna
};
RTC_NOINIT_ATTR static BootCrumbs s_bc;
static char s_prev[3][64];
constexpr uint32_t kMagic = 0x4F4E4443;

void plat_mark(int slot, const char *what, const char *detail) {
  if (slot < 0 || slot > 2) return;
  if (detail) snprintf(s_bc.step[slot], sizeof s_bc.step[slot], "%s: %s", what, detail);
  else snprintf(s_bc.step[slot], sizeof s_bc.step[slot], "%s", what);
}

const char *plat_mark_get(int slot) { return s_prev[slot]; }

// Chiamata dal gestore di panic di Arduino: salva motivo e backtrace in RTC.
static char s_panic_text[420];
static void panic_hook(arduino_panic_info_t *info, void *) {
  const char *r = info->reason ? info->reason : "?";
  size_t i = 0;
  for (; r[i] && i < sizeof s_bc.panic_reason - 1; i++) s_bc.panic_reason[i] = r[i];
  s_bc.panic_reason[i] = 0;
  s_bc.panic_core = info->core;
  s_bc.bt_len = info->backtrace_len < 12 ? info->backtrace_len : 12;
  for (uint32_t k = 0; k < s_bc.bt_len; k++) s_bc.bt[k] = info->backtrace[k];
}

const char *plat_panic_text() { return s_panic_text; }

// Chiamata dall'interrupt del watchdog dei task, prima del riavvio: annota quale
// task stava occupando ciascun core (il colpevole è quello sul core bloccato).
static void wdt_copy(char *&d, const char *end, const char *s) {
  while (s && *s && d < end) *d++ = *s++;
}
extern "C" void esp_task_wdt_isr_user_handler(void) {
  char *d = s_bc.wdt_tasks;
  const char *end = s_bc.wdt_tasks + sizeof s_bc.wdt_tasks - 1;
  for (int c = 0; c < 2; c++) {
    TaskHandle_t t = xTaskGetCurrentTaskHandleForCore(c);
    wdt_copy(d, end, c == 0 ? "core0=" : " core1=");
    wdt_copy(d, end, t ? pcTaskGetName(t) : "-");
  }
  *d = 0;
}

uint32_t plat_boot_check(const char **reason) {
  esp_reset_reason_t r = esp_reset_reason();
  const char *txt = "sconosciuto";
  bool crash = false;
  switch (r) {
    case ESP_RST_POWERON: txt = "accensione / tasto RESET"; break;
    case ESP_RST_EXT: txt = "reset esterno"; break;
    case ESP_RST_SW: txt = "riavvio software"; break;
    case ESP_RST_USB: txt = "reset da USB"; break;
    case ESP_RST_PANIC: txt = "CRASH (eccezione / panic)"; crash = true; break;
    case ESP_RST_INT_WDT: txt = "WATCHDOG interrupt"; crash = true; break;
    case ESP_RST_TASK_WDT: txt = "WATCHDOG task (blocco > 5 s)"; crash = true; break;
    case ESP_RST_WDT: txt = "WATCHDOG"; crash = true; break;
    case ESP_RST_BROWNOUT: txt = "CALO DI TENSIONE (brownout)"; crash = true; break;
    default: break;
  }
  if (reason) *reason = txt;
  if (s_bc.magic != kMagic) {
    memset(&s_bc, 0, sizeof s_bc);
    s_bc.magic = kMagic;
  }
  s_panic_text[0] = 0;
  if (crash && s_bc.bt_len > 0 && s_bc.bt_len <= 12) {
    s_bc.panic_reason[sizeof s_bc.panic_reason - 1] = 0;
    int n = snprintf(s_panic_text, sizeof s_panic_text, "%s (core %u), RAM int. min %u KB\n", s_bc.panic_reason,
                     (unsigned)s_bc.panic_core, (unsigned)(s_bc.min_internal / 1024));
    for (uint32_t k = 0; k < s_bc.bt_len && n < (int)sizeof s_panic_text - 12; k++)
      n += snprintf(s_panic_text + n, sizeof s_panic_text - n, "%08lx ", (unsigned long)s_bc.bt[k]);
  }
  if (crash && s_bc.wdt_tasks[0] && r == ESP_RST_TASK_WDT) {
    s_bc.wdt_tasks[sizeof s_bc.wdt_tasks - 1] = 0;
    size_t n = strlen(s_panic_text);
    snprintf(s_panic_text + n, sizeof s_panic_text - n, "Task attivi: %s", s_bc.wdt_tasks);
  }
  if (crash) {
    s_bc.stall[sizeof s_bc.stall - 1] = 0;
    s_bc.nvs[sizeof s_bc.nvs - 1] = 0;
    size_t n = strlen(s_panic_text);
    if (s_bc.stall[0]) n += snprintf(s_panic_text + n, sizeof s_panic_text - n, "%sMonitor: %s", n ? "\n" : "", s_bc.stall);
    if (n < sizeof s_panic_text && s_bc.nvs[0])
      snprintf(s_panic_text + n, sizeof s_panic_text - n, "%sScrittura flash in corso: %s", n ? "\n" : "", s_bc.nvs);
  }
  s_bc.stall[0] = 0;
  s_bc.nvs[0] = 0;
  s_bc.wdt_tasks[0] = 0;
  s_bc.bt_len = 0;
  s_bc.panic_reason[0] = 0;
  s_bc.min_internal = 0;
  set_arduino_panic_handler(panic_hook, nullptr);
  for (int i = 0; i < 3; i++) {
    s_bc.step[i][sizeof s_bc.step[i] - 1] = 0;
    memcpy(s_prev[i], s_bc.step[i], sizeof s_prev[i]);
    s_bc.step[i][0] = 0;
  }
  s_bc.crashes = crash ? s_bc.crashes + 1 : 0;
  return s_bc.crashes;
}

void plat_boot_ok() { s_bc.crashes = 0; }

void plat_heap_check() {
  s_bc.min_internal = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
  static uint32_t last = 0;
  uint32_t now = millis();
  if (now - last > 10000) {
    last = now;
    Serial.printf("[ram] interna libera %u KB (minimo %u KB, blocco max %u KB), PSRAM libera %u KB\n",
                  (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024),
                  (unsigned)(s_bc.min_internal / 1024),
                  (unsigned)(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL) / 1024),
                  (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024));
  }
}
void plat_kv_set(const char *key, int32_t v) {
  if (prefs().getInt(key, v ^ 1) == v) return;
  snprintf(s_bc.nvs, sizeof s_bc.nvs, "%s", key);   // traccia: scrittura in flash in corso
  prefs().putInt(key, v);
  s_bc.nvs[0] = 0;
}

// ---------------------------------------------------------------------------
// Monitor del core 0: gira sul core 1 e controlla che il task idle del core 0
// riesca a girare (è quello che sorveglia il watchdog). Se il core 0 resta
// occupato per più di 1 s annota chi lo sta occupando e lo stato dei task
// del player: se poi il watchdog riavvia la scheda, lo si legge al riavvio.
// ---------------------------------------------------------------------------
#include <esp_freertos_hooks.h>
static volatile uint32_t s_idle0 = 0;
static bool idle0_hook() {
  s_idle0 = s_idle0 + 1;
  return true;
}
static char task_state(const char *name) {
  TaskHandle_t h = xTaskGetHandle(name);
  if (!h) return '-';
  switch (eTaskGetState(h)) {
    case eRunning: return 'X';
    case eReady: return 'R';
    case eBlocked: return 'B';
    case eSuspended: return 'S';
    default: return '?';
  }
}
static void stall_monitor(void *) {
  uint32_t last = s_idle0, since = millis(), reported = 0;
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(100));
    uint32_t c = s_idle0, now = millis();
    if (c != last) {
      if (reported) Serial.printf("[monitor] core 0 di nuovo libero dopo %u ms\n", (unsigned)reported);
      last = c;
      since = now;
      reported = 0;
      continue;
    }
    uint32_t ms = now - since;
    if (ms < 1000) continue;
    TaskHandle_t t = xTaskGetCurrentTaskHandleForCore(0);
    snprintf(s_bc.stall, sizeof s_bc.stall, "core0 fermo %u ms, gira: %s (i2s %c flac %c scan %c)%s%s", (unsigned)ms,
             t ? pcTaskGetName(t) : "-", task_state("i2s"), task_state("flac"), task_state("scan"),
             s_bc.nvs[0] ? " nvs:" : "", s_bc.nvs);
    if (ms - reported >= 1000 || !reported) {
      reported = ms;
      Serial.printf("[monitor] %s\n", s_bc.stall);
    }
  }
}
void plat_monitor_start() {
  esp_register_freertos_idle_hook_for_cpu(idle0_hook, 0);
  xTaskCreatePinnedToCore(stall_monitor, "monitor", 3072, nullptr, configMAX_PRIORITIES - 2, nullptr, 1);
}

#else  // ---------------------------------------------------------- host ----
#include <chrono>
#include <map>
#include <string>

static std::map<std::string, int32_t> s_kv;
void *plat_alloc_big(size_t n) { return malloc(n); }
void *plat_alloc_fast(size_t n) { return malloc(n); }
void plat_free(void *p) { free(p); }
uint32_t plat_millis() {
  using namespace std::chrono;
  static auto t0 = steady_clock::now();
  return (uint32_t)duration_cast<milliseconds>(steady_clock::now() - t0).count();
}
void plat_yield() {}
void plat_log(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fputc('\n', stderr);
}
void plat_mark(int, const char *, const char *) {}
const char *plat_mark_get(int) { return ""; }
const char *plat_panic_text() { return ""; }
uint32_t plat_boot_check(const char **reason) {
  if (reason) *reason = "";
  return 0;
}
void plat_boot_ok() {}
void plat_heap_check() {}
int32_t plat_kv_get(const char *key, int32_t def) {
  auto it = s_kv.find(key);
  return it == s_kv.end() ? def : it->second;
}
void plat_kv_set(const char *key, int32_t v) { s_kv[key] = v; }
void plat_monitor_start() {}
#endif
