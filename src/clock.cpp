#include "clock.h"
#include "i18n.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "config.h"
#include "platform.h"

#ifndef TIME_ZONE
#define TIME_ZONE "CET-1CEST,M3.5.0,M10.5.0/3"   // Italia: ora solare/legale automatica
#endif

namespace {
constexpr time_t kValidAfter = 1700000000;   // novembre 2023: prima di così l'ora non è stata impostata
const char *kDays[] = {"domenica", "lunedì", "martedì", "mercoledì", "giovedì", "venerdì", "sabato"};
const char *kMonths[] = {"gennaio", "febbraio", "marzo",     "aprile",  "maggio",   "giugno",
                         "luglio",  "agosto",   "settembre", "ottobre", "novembre", "dicembre"};
const char *kDaysEn[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
const char *kMonthsEn[] = {"January", "February", "March",     "April",   "May",      "June",
                           "July",    "August",   "September", "October", "November", "December"};
}  // namespace

void clock_fmt_time(const struct tm &t, char *buf, size_t n) { snprintf(buf, n, "%d:%02d", t.tm_hour, t.tm_min); }

void clock_fmt_date(const struct tm &t, char *buf, size_t n) {
  if (i18n_lang() == LANG_EN)   // "Thursday, October 15, 2026"
    snprintf(buf, n, "%s, %s %d, %d", kDaysEn[t.tm_wday % 7], kMonthsEn[t.tm_mon % 12], t.tm_mday, t.tm_year + 1900);
  else   // "giovedì 15 ottobre 2026"
    snprintf(buf, n, "%s %d %s %d", kDays[t.tm_wday % 7], t.tm_mday, kMonths[t.tm_mon % 12], t.tm_year + 1900);
}

bool clock_valid() { return time(nullptr) > kValidAfter; }

bool clock_local(struct tm *out) {
  time_t now = time(nullptr);
  if (now <= kValidAfter) return false;
  localtime_r(&now, out);
  return true;
}

#ifdef ARDUINO
#include <Arduino.h>
#include <esp_attr.h>
#include <esp_sntp.h>

// sopravvive a riavvii e deep sleep (come l'ora del chip)
struct ClockInfo {
  uint32_t magic;
  uint8_t src;
  int64_t at;
};
RTC_NOINIT_ATTR static ClockInfo s_ci;
constexpr uint32_t kMagic = 0x434C4B31;

void clock_begin() {
  setenv("TZ", TIME_ZONE, 1);
  tzset();
  if (s_ci.magic != kMagic || !clock_valid()) {
    s_ci.magic = kMagic;
    s_ci.src = CLOCK_NONE;
    s_ci.at = 0;
  }
}

void clock_set(int64_t unix_ms, ClockSource src) {
  if (unix_ms / 1000 <= kValidAfter) return;
  struct timeval tv;
  tv.tv_sec = (time_t)(unix_ms / 1000);
  tv.tv_usec = (suseconds_t)(unix_ms % 1000) * 1000;
  settimeofday(&tv, nullptr);
  s_ci.magic = kMagic;
  s_ci.src = src;
  s_ci.at = tv.tv_sec;
  plat_event(src == CLOCK_NTP ? "ora da internet" : "ora dal browser");
}

ClockSource clock_source() { return s_ci.magic == kMagic ? (ClockSource)s_ci.src : CLOCK_NONE; }
int64_t clock_synced_at() { return s_ci.magic == kMagic ? s_ci.at : 0; }

static void ntp_cb(struct timeval *tv) {
  s_ci.magic = kMagic;
  s_ci.src = CLOCK_NTP;
  s_ci.at = tv ? tv->tv_sec : time(nullptr);
}

void clock_ntp_start() {
  if (esp_sntp_enabled()) esp_sntp_stop();
  sntp_set_time_sync_notification_cb(ntp_cb);
  configTzTime(TIME_ZONE, "pool.ntp.org", "time.google.com", "ntp1.inrim.it");
}

void clock_ntp_stop() {
  if (esp_sntp_enabled()) esp_sntp_stop();
}

#else  // simulatore: l'ora del PC
static ClockSource s_src = CLOCK_NONE;
static int64_t s_at = 0;
void clock_begin() {
  setenv("TZ", TIME_ZONE, 1);
  tzset();
  s_src = CLOCK_BROWSER;
  s_at = time(nullptr);
}
void clock_set(int64_t, ClockSource src) {
  s_src = src;
  s_at = time(nullptr);
}
ClockSource clock_source() { return s_src; }
int64_t clock_synced_at() { return s_at; }
void clock_ntp_start() {}
void clock_ntp_stop() {}
#endif
