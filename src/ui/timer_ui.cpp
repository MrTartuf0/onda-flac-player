// Schermata Timer (Impostazioni → Timer): imposti ore, minuti e secondi con le
// rotelle (o tocchi una durata recente) e lo avvii; mentre va mostra il tempo che
// manca dentro un anello, con Annulla, +1 min e Pausa. Allo scadere si apre da
// sola (ui_timer_alert) e il tempo diventa rosso e conta da quanto è scaduto.
#include "ui_internal.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "../session.h"
#include "../clock.h"
#include "../platform.h"

namespace {

constexpr int HEAD_H = 48;
constexpr int COL_W = 64, COL_GAP = 6;
constexpr int COL_X0 = (LCD_W - 3 * COL_W - 2 * COL_GAP) / 2;
constexpr int ROLL_Y = 68;
constexpr int RING = 188, RING_Y = 56;
constexpr int BTN = 60, BTN_Y = 250;
#define C_RED lv_color_hex(0xF15E6C)
#define C_ORANGE lv_color_hex(0xF5A623)

lv_obj_t *s_root = nullptr;
lv_obj_t *s_setup, *s_run;
// impostazione
lv_obj_t *s_roll[3], *s_hint, *s_recents, *s_start;
// in corso
lv_obj_t *s_arc, *s_time, *s_sub, *s_cancel, *s_plus, *s_pause, *s_pause_l;
lv_timer_t *s_tm = nullptr;
bool s_open = false;
char s_last_time[16];
int s_last_mode = -1;

void tick_cb(lv_timer_t *);

lv_obj_t *round_btn(lv_obj_t *parent, int d, lv_color_t bg) {
  lv_obj_t *b = ui_box(parent);
  lv_obj_set_size(b, d, d);
  lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(b, bg, 0);
  lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
  lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_ext_click_area(b, 6);
  lv_obj_set_style_transform_scale(b, 236, LV_STATE_PRESSED);
  lv_obj_set_style_transform_pivot_x(b, d / 2, 0);
  lv_obj_set_style_transform_pivot_y(b, d / 2, 0);
  return b;
}

lv_obj_t *text_btn(lv_obj_t *parent, int cx, lv_color_t bg, lv_color_t fg, const char *txt, lv_event_cb_t cb) {
  lv_obj_t *b = round_btn(parent, BTN, bg);
  lv_obj_set_pos(b, cx - BTN / 2, BTN_Y);
  lv_obj_t *l = ui_label(b, &font_b12, fg, txt);
  lv_obj_center(l);
  lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, nullptr);
  return b;
}

// ------------------------------------------------------- impostazione ----
uint32_t rollers_s() {
  return lv_roller_get_selected(s_roll[0]) * 3600 + lv_roller_get_selected(s_roll[1]) * 60 +
         lv_roller_get_selected(s_roll[2]);
}

// "Fine alle 13:42" (con l'ora impostata), oppure la durata scritta per esteso
void end_text(uint32_t remaining_s, char *b, size_t n) {
  struct tm t;
  if (clock_local(&t)) {
    time_t end = time(nullptr) + remaining_s;
    struct tm e;
    localtime_r(&end, &e);
    char hm[8];
    clock_fmt_time(e, hm, sizeof hm);
    snprintf(b, n, TR("Fine alle %s"), hm);
  } else {
    b[0] = 0;
  }
}

void update_hint() {
  uint32_t s = rollers_s();
  char b[48];
  if (!s) snprintf(b, sizeof b, "%s", TR("Senza durata conta in avanti"));
  else end_text(s, b, sizeof b);
  lv_label_set_text(s_hint, b);
}

void set_rollers(uint32_t s) {
  lv_roller_set_selected(s_roll[0], s / 3600, LV_ANIM_ON);
  lv_roller_set_selected(s_roll[1], s / 60 % 60, LV_ANIM_ON);
  lv_roller_set_selected(s_roll[2], s % 60, LV_ANIM_ON);
  update_hint();
}

void roller_cb(lv_event_t *) {
  session_set_duration_s(rollers_s());
  update_hint();
}

void recent_cb(lv_event_t *e) {
  uint32_t s = (uint32_t)(uintptr_t)lv_event_get_user_data(e);
  set_rollers(s);
  session_set_duration_s(s);
}

void fill_recents() {
  lv_obj_clean(s_recents);
  // le ultime usate, completate con qualche durata comune
  static const uint32_t kDefaults[] = {9 * 60, 30 * 60, 5 * 60, 12 * 60};
  uint32_t v[3];
  int n = 0;
  for (int i = 0; i < 4 && n < 3; i++)
    if (uint32_t s = session_recent_s(i)) v[n++] = s;
  for (uint32_t d : kDefaults) {
    if (n >= 3) break;
    bool dup = false;
    for (int k = 0; k < n; k++) dup |= v[k] == d;
    if (!dup) v[n++] = d;
  }
  for (int i = 0; i < n; i++) {
    char b[16];
    session_fmt(v[i], b, sizeof b);
    lv_obj_t *c = ui_chip(s_recents, b, recent_cb, (void *)(uintptr_t)v[i]);
    lv_obj_set_style_pad_hor(c, 14, 0);
  }
}

lv_obj_t *roller(lv_obj_t *parent, int i, const char *opts, bool infinite) {
  lv_obj_t *r = lv_roller_create(parent);
  lv_roller_set_options(r, opts, infinite ? LV_ROLLER_MODE_INFINITE : LV_ROLLER_MODE_NORMAL);
  lv_obj_set_width(r, COL_W);
  lv_obj_set_pos(r, COL_X0 + i * (COL_W + COL_GAP), ROLL_Y);
  lv_obj_set_style_bg_opa(r, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(r, 0, 0);
  lv_obj_set_style_pad_all(r, 0, 0);
  lv_obj_set_style_text_font(r, &font_b26, 0);
  lv_obj_set_style_text_color(r, lv_color_hex(0x5A5A5A), 0);
  lv_obj_set_style_text_line_space(r, 8, 0);
  lv_obj_set_style_text_align(r, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_bg_opa(r, LV_OPA_TRANSP, LV_PART_SELECTED);
  lv_obj_set_style_text_color(r, C_TEXT, LV_PART_SELECTED);
  lv_obj_set_style_text_font(r, &font_b26, LV_PART_SELECTED);
  lv_obj_set_style_anim_duration(r, 160, 0);
  // tre righe intere (visible_row_count taglia un po' l'ultima)
  lv_obj_set_height(r, 3 * (lv_font_get_line_height(&font_b26) + 8));
  lv_obj_add_event_cb(r, roller_cb, LV_EVENT_VALUE_CHANGED, nullptr);
  return r;
}

void start_cb(lv_event_t *) {
  session_set_duration_s(rollers_s());
  session_start(false);
  tick_cb(nullptr);
}

// ----------------------------------------------------------- in corso ----
void cancel_cb(lv_event_t *) {
  session_stop(false);
  tick_cb(nullptr);
}
void plus_cb(lv_event_t *) {
  session_add_s(60);
  tick_cb(nullptr);
}
void pause_cb(lv_event_t *) {
  if (session_over()) session_stop(false);
  else if (session_paused()) session_resume();
  else session_pause();
  tick_cb(nullptr);
}

// 0 = impostazione, 1 = conto alla rovescia, 2 = in pausa, 3 = scaduto, 4 = cronometro, 5 = cronometro in pausa
int mode() {
  if (!session_running()) return 0;
  if (!session_run_duration_s()) return session_paused() ? 5 : 4;
  if (session_over()) return 3;
  return session_paused() ? 2 : 1;
}

void apply_mode(int m) {
  s_last_mode = m;
  s_last_time[0] = 0;
  if (m == 0) {
    lv_obj_add_flag(s_run, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(s_setup, LV_OBJ_FLAG_HIDDEN);
    uint32_t d = session_duration_s();
    lv_roller_set_selected(s_roll[0], d / 3600, LV_ANIM_OFF);
    lv_roller_set_selected(s_roll[1], d / 60 % 60, LV_ANIM_OFF);
    lv_roller_set_selected(s_roll[2], d % 60, LV_ANIM_OFF);
    update_hint();
    fill_recents();
    return;
  }
  lv_obj_add_flag(s_setup, LV_OBJ_FLAG_HIDDEN);
  lv_obj_remove_flag(s_run, LV_OBJ_FLAG_HIDDEN);
  bool over = m == 3, paused = m == 2 || m == 5, watch = m >= 4;
  lv_obj_set_style_arc_color(s_arc, over ? C_RED : (paused ? C_ORANGE : C_GREEN), LV_PART_INDICATOR);
  lv_obj_set_style_text_color(s_time, over ? C_RED : C_TEXT, 0);
  lv_obj_set_style_text_color(s_sub, over ? C_RED : C_SUB, 0);
  // Annulla a sinistra (a tempo scaduto basta Stop), +1 min al centro (non nel cronometro)
  if (over) lv_obj_add_flag(s_cancel, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_remove_flag(s_cancel, LV_OBJ_FLAG_HIDDEN);
  if (watch) lv_obj_add_flag(s_plus, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_remove_flag(s_plus, LV_OBJ_FLAG_HIDDEN);
  // i pulsanti visibili distribuiti in larghezza: 3 (46/120/194) o 2 (80/160)
  lv_obj_t *vis[3];
  int n = 0;
  for (lv_obj_t *b : {s_cancel, s_plus, s_pause})
    if (!lv_obj_has_flag(b, LV_OBJ_FLAG_HIDDEN)) vis[n++] = b;
  for (int i = 0; i < n; i++) {
    int cx = n == 3 ? 46 + i * (SCR_W / 2 - 46) : SCR_W / 2 + (i ? 40 : -40);
    lv_obj_set_x(vis[i], cx - BTN / 2);
  }
  lv_obj_set_style_bg_color(s_pause, over ? C_RED : (paused ? C_GREEN : C_ORANGE), 0);
  lv_obj_set_style_text_color(s_pause_l, over ? C_TEXT : C_BLACK, 0);
  lv_label_set_text(s_pause_l, over ? TR("Stop") : (paused ? TR("Riprendi") : TR("Pausa")));
}

void tick_cb(lv_timer_t *) {
  if (!s_open) return;
  int m = mode();
  if (m != s_last_mode) apply_mode(m);
  if (m == 0) return;
  uint32_t dur = session_run_duration_s();
  int32_t rem = session_remaining_ms();
  uint32_t el = session_elapsed_ms();
  char b[16];
  uint32_t shown;
  if (m >= 4) shown = el / 1000;                                 // cronometro
  else if (rem > 0) shown = (uint32_t)(rem + 999) / 1000;        // 0:01 fino all'ultimo istante
  else shown = (uint32_t)(-rem) / 1000;
  session_fmt(shown, b, sizeof b, m == 3);
  if (strcmp(b, s_last_time)) {
    strcpy(s_last_time, b);
    lv_obj_set_style_text_font(s_time, strlen(b) > 5 ? &font_d36 : &font_d52, 0);   // "+24:58", "1:05:00"
    lv_label_set_text(s_time, b);
    char sub[48];
    if (m == 2 || m == 5) snprintf(sub, sizeof sub, "%s", TR("In pausa"));
    else if (m == 3) snprintf(sub, sizeof sub, "%s", TR("Tempo scaduto"));
    else if (m == 4) snprintf(sub, sizeof sub, "%s", TR("Cronometro"));
    else end_text(shown, sub, sizeof sub);
    if (m == 1 && !sub[0]) {   // senza orologio: la durata impostata
      char d[16];
      session_fmt(dur, d, sizeof d);
      snprintf(sub, sizeof sub, TR("Timer di %s"), d);
    }
    lv_label_set_text(s_sub, sub);
  }
  // anello: il tempo che resta; nel cronometro i secondi che girano
  int v;
  if (m >= 4) v = el % 60000 / 60;
  else if (m == 3) v = 1000;
  else if (uint32_t seg = session_segment_ms()) v = (int)((int64_t)rem * 1000 / seg);
  else v = 0;
  lv_arc_set_value(s_arc, v < 0 ? 0 : (v > 1000 ? 1000 : v));
  // con il timer che va e questa schermata aperta lo schermo resta acceso
  if (m == 1 || m == 3 || m == 4) lv_display_trigger_activity(nullptr);
}

void back_cb(lv_event_t *) { ui_timer_close(); }

void build() {
  s_root = ui_box(lv_layer_top());
  lv_obj_set_size(s_root, SCR_W, SCR_H);
  lv_obj_set_style_bg_color(s_root, C_BG, 0);
  lv_obj_set_style_bg_opa(s_root, LV_OPA_COVER, 0);
  lv_obj_add_flag(s_root, LV_OBJ_FLAG_CLICKABLE);   // niente tocchi alle pagine sotto

  lv_obj_t *back = ui_icon_btn(s_root, &icons_18, IC_BACK, C_TEXT, back_cb);
  lv_obj_set_pos(back, 16, 15);
  lv_obj_set_ext_click_area(back, 14);
  lv_obj_t *title = ui_label(s_root, &font_b16, C_TEXT, TR("Timer"));
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 15);

  // ---- impostazione: tre rotelle ore / min / sec, durate recenti, avvia
  s_setup = ui_box(s_root);
  lv_obj_set_size(s_setup, SCR_W, SCR_H);
  lv_obj_remove_flag(s_setup, LV_OBJ_FLAG_CLICKABLE);   // i tocchi arrivano anche a "indietro"
  const char *caps[3] = {TR("ore"), TR("min"), TR("sec")};
  for (int i = 0; i < 3; i++) {
    lv_obj_t *c = ui_label(s_setup, &font_r12, C_SUB, caps[i]);
    lv_obj_set_width(c, COL_W);
    lv_obj_set_style_text_align(c, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(c, COL_X0 + i * (COL_W + COL_GAP), ROLL_Y - 18);
  }
  // fascia dietro alla riga scelta
  lv_obj_t *band = ui_box(s_setup);
  lv_obj_set_style_bg_color(band, C_TILE, 0);
  lv_obj_set_style_bg_opa(band, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(band, 10, 0);
  static char mins[60 * 3 + 1];
  if (!mins[0]) {
    char *p = mins;
    for (int i = 0; i < 60; i++) p += sprintf(p, i ? "\n%02d" : "%02d", i);
  }
  s_roll[0] = roller(s_setup, 0, "0\n1\n2\n3\n4\n5\n6\n7\n8\n9", false);
  s_roll[1] = roller(s_setup, 1, mins, true);
  s_roll[2] = roller(s_setup, 2, mins, true);
  lv_obj_update_layout(s_roll[0]);
  int rh = lv_obj_get_height(s_roll[0]);
  int row = rh / 3;
  lv_obj_set_size(band, 3 * COL_W + 2 * COL_GAP + 12, row + 2);
  lv_obj_set_pos(band, COL_X0 - 6, ROLL_Y + row - 1);
  // separatori ":" tra le rotelle
  for (int i = 0; i < 2; i++) {
    lv_obj_t *c = ui_label(s_setup, &font_b26, C_TEXT, ":");
    lv_obj_update_layout(c);
    lv_obj_set_pos(c, COL_X0 + (i + 1) * COL_W + i * COL_GAP + (COL_GAP - lv_obj_get_width(c)) / 2,
                   ROLL_Y + row + (row - lv_obj_get_height(c)) / 2 - 2);
  }
  s_hint = ui_label(s_setup, &font_r12, C_SUB, "");
  lv_obj_set_width(s_hint, SCR_W);
  lv_obj_set_style_text_align(s_hint, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_pos(s_hint, 0, ROLL_Y + rh + 2);
  s_recents = ui_box(s_setup);
  lv_obj_set_size(s_recents, SCR_W, 30);
  lv_obj_set_pos(s_recents, 0, ROLL_Y + rh + 22);
  lv_obj_set_flex_flow(s_recents, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(s_recents, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_gap(s_recents, 8, 0);
  constexpr int SB = 54;
  s_start = round_btn(s_setup, SB, C_GREEN);
  lv_obj_align(s_start, LV_ALIGN_BOTTOM_MID, 0, -10);
  lv_obj_t *pi = ui_icon(s_start, &icons_22, IC_PLAY, C_BLACK);
  lv_obj_center(pi);
  lv_obj_set_style_translate_x(pi, 2, 0);
  lv_obj_add_event_cb(s_start, start_cb, LV_EVENT_CLICKED, nullptr);

  // ---- in corso: anello con il tempo, Annulla / +1 min / Pausa
  s_run = ui_box(s_root);
  lv_obj_set_size(s_run, SCR_W, SCR_H);
  lv_obj_remove_flag(s_run, LV_OBJ_FLAG_CLICKABLE);
  s_arc = lv_arc_create(s_run);
  lv_obj_set_size(s_arc, RING, RING);
  lv_obj_align(s_arc, LV_ALIGN_TOP_MID, 0, RING_Y);
  lv_arc_set_rotation(s_arc, 270);
  lv_arc_set_bg_angles(s_arc, 0, 360);
  lv_arc_set_range(s_arc, 0, 1000);
  lv_obj_remove_style(s_arc, nullptr, LV_PART_KNOB);
  lv_obj_remove_flag(s_arc, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_arc_width(s_arc, 8, 0);
  lv_obj_set_style_arc_color(s_arc, C_TILE, 0);
  lv_obj_set_style_arc_width(s_arc, 8, LV_PART_INDICATOR);
  lv_obj_set_style_arc_rounded(s_arc, true, LV_PART_INDICATOR);
  s_time = ui_label(s_arc, &font_d52, C_TEXT, "");
  lv_obj_align(s_time, LV_ALIGN_CENTER, 0, -8);
  s_sub = ui_label(s_arc, &font_r12, C_SUB, "");
  lv_obj_align(s_sub, LV_ALIGN_CENTER, 0, 30);
  s_cancel = text_btn(s_run, 46, C_TILE, C_TEXT, TR("Annulla"), cancel_cb);
  s_plus = text_btn(s_run, SCR_W / 2, C_TILE, C_TEXT, TR("+1 min"), plus_cb);
  s_pause = text_btn(s_run, SCR_W - 46, C_ORANGE, C_BLACK, TR("Pausa"), pause_cb);
  s_pause_l = lv_obj_get_child(s_pause, 0);

  lv_obj_add_flag(s_root, LV_OBJ_FLAG_HIDDEN);
}

}  // namespace

void ui_timer_open() {
  if (!s_root) build();
  if (!s_open) {
    s_open = true;
    s_last_mode = -1;
    s_tm = lv_timer_create(tick_cb, 100, nullptr);
  }
  lv_obj_move_to_index(s_root, -1);
  lv_obj_remove_flag(s_root, LV_OBJ_FLAG_HIDDEN);
  tick_cb(nullptr);
}

bool ui_timer_visible() { return s_open; }

bool ui_timer_close() {
  if (!s_open) return false;
  s_open = false;
  if (s_tm) {
    lv_timer_delete(s_tm);
    s_tm = nullptr;
  }
  lv_obj_add_flag(s_root, LV_OBJ_FLAG_HIDDEN);
  return true;
}

void ui_timer_alert() {
  ui_cc_close();
  ui_timer_open();
}
