#include "../session.h"
#include <stdio.h>
#include "ui.h"
#include "ui_internal.h"
#include "../platform.h"
#include "../audio/player.h"
#include "../lib/library.h"

namespace {
bool s_run = false;
uint32_t s_t0 = 0, s_next = 0;   // inizio e prossimo avviso (ms dall'inizio)
int s_limit = -1;                // -1 = da leggere dalle preferenze
constexpr uint32_t kRemindMs = 5 * 60000;

int limit() {
  if (s_limit < 0) s_limit = plat_kv_get("ses_lim", 30);
  return s_limit;
}

// primo avviso utile dopo `el`: il limite, poi ogni 5 minuti
uint32_t next_after(uint32_t el) {
  if (!limit()) return 0;
  uint32_t lim = (uint32_t)limit() * 60000;
  if (el < lim) return lim;
  return lim + ((el - lim) / kRemindMs + 1) * kRemindMs;
}
}  // namespace

bool session_running() { return s_run; }
uint32_t session_elapsed_ms() { return s_run ? plat_millis() - s_t0 : 0; }
bool session_over() { return s_run && limit() && session_elapsed_ms() >= (uint32_t)limit() * 60000; }
int session_limit_min() { return limit(); }

void session_set_limit_min(int m) {
  s_limit = m < 0 ? 0 : m;
  ui_pref_set("ses_lim", s_limit);
  if (s_run) s_next = next_after(session_elapsed_ms());
}

void session_start(bool beep) {
  if (s_run) return;
  s_run = true;
  s_t0 = plat_millis();
  s_next = next_after(0);
  plat_event("sessione avviata");
  if (beep) player_beep(1);
}

void session_stop(bool beep) {
  if (!s_run) return;
  s_run = false;
  char b[16];
  fmt_duration(plat_millis() - s_t0, b, sizeof b);
  plat_event("sessione fermata", b);
  if (beep) player_beep(2);
}

void session_toggle(bool beep) {
  if (s_run) session_stop(beep);
  else session_start(beep);
}

void session_poll() {
  if (!s_run || !s_next) return;
  uint32_t el = session_elapsed_ms();
  if (el < s_next) return;
  bool first = s_next == (uint32_t)limit() * 60000;
  player_beep(first ? 3 : 2);
  char b[48];
  snprintf(b, sizeof b, "Sessione: %u min", (unsigned)(el / 60000));
  ui_toast(b);
  plat_event(first ? "sessione: limite" : "sessione: promemoria");
  s_next = next_after(el);
}
