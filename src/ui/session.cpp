// Timer (vedi session.h): stato, avvisi e durate recenti.
#include "../session.h"
#include <stdio.h>
#include "ui.h"
#include "ui_internal.h"
#include "../platform.h"
#include "../power.h"
#include "../audio/player.h"
#include "../lib/library.h"

namespace {
enum St : uint8_t { IDLE, RUN, PAUSE };
St s_st = IDLE;
int64_t s_dur = -1;               // durata impostata in s (-1 = da leggere dalle preferenze)
uint32_t s_run_dur = 0;           // durata della corsa in corso (cambia con +1 min)
uint32_t s_t0 = 0;                // inizio, spostato in avanti delle pause
uint32_t s_pause_at = 0;
uint32_t s_seg0 = 0;              // da dove riparte l'anello dopo "+1 min" a tempo scaduto
uint32_t s_next = 0;              // prossimo avviso (ms trascorsi), 0 = nessuno
uint32_t s_last = 0;
uint32_t s_rec[4];
bool s_rec_loaded = false;
constexpr uint32_t kRemindMs = 30000;

uint32_t dur() {
  if (s_dur < 0) {
    int32_t d = plat_kv_get("tm_dur", -1);
    if (d < 0) d = plat_kv_get("ses_lim", 30) * 60;   // dal vecchio "Avviso dopo" in minuti
    s_dur = d;
  }
  return (uint32_t)s_dur;
}

void load_recents() {
  if (s_rec_loaded) return;
  s_rec_loaded = true;
  char k[8];
  for (int i = 0; i < 4; i++) {
    snprintf(k, sizeof k, "tm_r%d", i);
    s_rec[i] = plat_kv_get(k, 0);
  }
}

void push_recent(uint32_t s) {
  if (!s) return;
  load_recents();
  int at = 3;
  for (int i = 0; i < 4; i++)
    if (s_rec[i] == s) at = i;
  for (int i = at; i > 0; i--) s_rec[i] = s_rec[i - 1];
  s_rec[0] = s;
  char k[8];
  for (int i = 0; i < 4; i++) {
    snprintf(k, sizeof k, "tm_r%d", i);
    ui_pref_set(k, s_rec[i]);
  }
}

uint32_t elapsed() {
  if (s_st == IDLE) return 0;
  return (s_st == PAUSE ? s_pause_at : plat_millis()) - s_t0;
}

// primo avviso dopo `el`: la fine, poi ogni 30 secondi
uint32_t next_after(uint32_t el) {
  if (!s_run_dur) return 0;
  uint32_t end = s_run_dur * 1000;
  if (el < end) return end;
  return end + ((el - end) / kRemindMs + 1) * kRemindMs;
}
}  // namespace

bool session_running() { return s_st != IDLE; }
bool session_paused() { return s_st == PAUSE; }
uint32_t session_elapsed_ms() { return elapsed(); }
uint32_t session_run_duration_s() { return s_st == IDLE ? dur() : s_run_dur; }
bool session_over() { return s_st != IDLE && s_run_dur && elapsed() >= s_run_dur * 1000; }
int32_t session_remaining_ms() {
  uint32_t d = session_run_duration_s();
  return (int32_t)(d * 1000) - (int32_t)elapsed();
}

uint32_t session_segment_ms() {
  uint32_t d = session_run_duration_s() * 1000;
  return d > s_seg0 ? d - s_seg0 : 0;
}

uint32_t session_duration_s() { return dur(); }
void session_set_duration_s(uint32_t s) {
  if (s > 10 * 3600 - 1) s = 10 * 3600 - 1;
  if (s == dur()) return;
  s_dur = s;
  ui_pref_set("tm_dur", (int32_t)s);
}

uint32_t session_recent_s(int i) {
  load_recents();
  return i >= 0 && i < 4 ? s_rec[i] : 0;
}

void session_start(bool beep) {
  if (s_st != IDLE) return;
  s_st = RUN;
  s_run_dur = dur();
  s_seg0 = 0;
  s_t0 = plat_millis();
  s_next = next_after(0);
  push_recent(s_run_dur);
  char b[16];
  session_fmt(s_run_dur, b, sizeof b);
  plat_event("timer avviato", b);
  if (beep) player_beep(1);
}

void session_stop(bool beep) {
  if (s_st == IDLE) return;
  s_last = elapsed();
  s_st = IDLE;
  s_next = 0;
  char b[16];
  fmt_duration(s_last, b, sizeof b);
  plat_event("timer fermato", b);
  if (beep) player_beep(2);
}

void session_toggle(bool beep) {
  if (s_st != IDLE) session_stop(beep);
  else session_start(beep);
}

void session_pause() {
  if (s_st != RUN) return;
  s_pause_at = plat_millis();
  s_st = PAUSE;
}

void session_resume() {
  if (s_st != PAUSE) return;
  s_t0 += plat_millis() - s_pause_at;
  s_st = RUN;
  s_next = next_after(elapsed());
}

void session_add_s(int s) {
  if (s_st == IDLE || !s_run_dur) return;
  uint32_t el = elapsed();
  if (el >= s_run_dur * 1000) {   // scaduto: si riparte da adesso
    s_run_dur = (el + 999) / 1000;
    s_seg0 = el;
  }
  s_run_dur += s;
  s_next = next_after(el);
}

uint32_t session_last_ms() { return s_last; }

void session_fmt(uint32_t s, char *b, size_t n, bool plus) {
  const char *sg = plus ? "+" : "";
  if (s >= 3600) snprintf(b, n, "%s%u:%02u:%02u", sg, (unsigned)(s / 3600), (unsigned)(s / 60 % 60), (unsigned)(s % 60));
  else snprintf(b, n, "%s%u:%02u", sg, (unsigned)(s / 60), (unsigned)(s % 60));
}

void session_poll() {
  if (s_st != RUN || !s_next) return;
  uint32_t el = elapsed();
  if (el < s_next) return;
  bool first = s_next == s_run_dur * 1000;
  player_beep(3);
  if (first) {
    power_wake_for_alert();   // riaccende lo schermo spento da solo (non quello bloccato col tasto)
    ui_timer_alert();
    plat_event("timer: tempo scaduto");
  } else {
    char t[16], b[48];
    session_fmt((el - s_run_dur * 1000) / 1000, t, sizeof t);
    snprintf(b, sizeof b, TR("Timer scaduto da %s"), t);
    ui_toast(b);
  }
  s_next = next_after(el);
}
