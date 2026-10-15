// Batteria nell'interfaccia: indicatore sempre visibile in alto a destra,
// avvisi di batteria scarica e schermata di spegnimento.
#include "ui.h"
#include "ui_internal.h"
#include <stdio.h>
#include "../hal/battery.h"
#include "../audio/player.h"
#include "../platform.h"

#define C_RED lv_color_hex(0xF15E6C)

namespace {

lv_obj_t *s_root, *s_bolt, *s_pct, *s_icon;
BatteryInfo s_last;
bool s_have = false;
bool s_show_pct = true;
bool s_warn10 = false, s_warn5 = false;

void place() { lv_obj_align(s_root, LV_ALIGN_TOP_RIGHT, -5, 0); }

}  // namespace

// Pila disegnata: contorno, livello e polo. w x h è il corpo (il polo è in più).
lv_obj_t *ui_battery_icon(lv_obj_t *parent, int w, int h) {
  lv_obj_t *wrap = ui_box(parent);
  lv_obj_set_size(wrap, w + 3, h);
  lv_obj_t *body = ui_box(wrap);
  lv_obj_set_size(body, w, h);
  lv_obj_set_style_border_width(body, 1, 0);
  lv_obj_set_style_border_color(body, C_TEXT, 0);
  lv_obj_set_style_border_opa(body, LV_OPA_70, 0);
  lv_obj_set_style_radius(body, h >= 16 ? 4 : 2, 0);
  lv_obj_t *fill = ui_box(body);
  int in = h >= 16 ? 3 : 2;
  lv_obj_set_pos(fill, in, in);
  lv_obj_set_size(fill, 0, h - 2 * in);
  lv_obj_set_style_radius(fill, h >= 16 ? 2 : 1, 0);
  lv_obj_set_style_bg_opa(fill, LV_OPA_COVER, 0);
  lv_obj_t *tip = ui_box(wrap);
  int th = h / 2 < 3 ? 3 : h / 2;
  lv_obj_set_size(tip, 2, th);
  lv_obj_set_pos(tip, w + 1, (h - th) / 2);
  lv_obj_set_style_bg_color(tip, C_TEXT, 0);
  lv_obj_set_style_bg_opa(tip, LV_OPA_70, 0);
  lv_obj_set_style_radius(tip, 1, 0);
  return wrap;
}

void ui_battery_icon_set(lv_obj_t *icon, int pct, lv_color_t c) {
  lv_obj_t *body = lv_obj_get_child(icon, 0);
  lv_obj_t *fill = lv_obj_get_child(body, 0);
  int in = lv_obj_get_x(fill);
  int full = lv_obj_get_width(body) - 2 * in;
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;
  int w = (full * pct + 50) / 100;
  if (pct > 0 && w < 1) w = 1;
  lv_obj_set_width(fill, w);
  lv_obj_set_style_bg_color(fill, c, 0);
}

const char *ui_battery_state_text(const BatteryInfo &b) {
  switch (b.state) {
    case BAT_CHARGING: return "In carica";
    case BAT_FULL: return "Carica completa";
    case BAT_DISCHARGING: return b.low ? "Batteria scarica" : "A batteria";
    default: return "Batteria non rilevata";
  }
}

void ui_battery_set_show_pct(bool on) {
  s_show_pct = on;
  plat_kv_set("bat_pct", on);
  s_have = false;   // ridisegna
  ui_battery_tick();
}

bool ui_battery_show_pct() { return s_show_pct; }

void ui_battery_build() {
  s_show_pct = plat_kv_get("bat_pct", 1) != 0;
  s_root = ui_box(lv_layer_top());
  lv_obj_set_size(s_root, LV_SIZE_CONTENT, 11);
  lv_obj_set_flex_flow(s_root, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(s_root, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(s_root, 3, 0);
  lv_obj_remove_flag(s_root, LV_OBJ_FLAG_CLICKABLE);
  s_bolt = ui_label(s_root, &icons_10, C_GREEN, IC_BOLT);
  s_pct = ui_label(s_root, &font_b10, C_TEXT, "");
  s_icon = ui_battery_icon(s_root, 17, 8);
  lv_obj_add_flag(s_root, LV_OBJ_FLAG_HIDDEN);
  place();
}

void ui_battery_tick() {
  if (!s_root) return;
  BatteryInfo b = battery_info();
  if (s_have && b.state == s_last.state && b.pct == s_last.pct && b.low == s_last.low) return;
  s_have = true;
  s_last = b;
  if (b.state == BAT_NONE || b.pct < 0) {
    lv_obj_add_flag(s_root, LV_OBJ_FLAG_HIDDEN);
    return;
  }
  bool chg = b.state == BAT_CHARGING || b.state == BAT_FULL;
  lv_color_t c = chg ? C_GREEN : (b.low ? C_RED : C_TEXT);
  if (chg) lv_obj_remove_flag(s_bolt, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(s_bolt, LV_OBJ_FLAG_HIDDEN);
  if (s_show_pct) {
    lv_label_set_text_fmt(s_pct, "%d%%", b.pct);
    lv_obj_remove_flag(s_pct, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(s_pct, LV_OBJ_FLAG_HIDDEN);
  }
  lv_obj_set_style_text_color(s_pct, b.low ? C_RED : C_TEXT, 0);
  ui_battery_icon_set(s_icon, b.pct, c);
  lv_obj_remove_flag(s_root, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_to_index(s_root, 0);   // sotto a notifiche e gesti, sopra alle schermate
  place();

  // ---- avvisi (una volta per ciclo di scarica) ----
  if (b.state != BAT_DISCHARGING || b.pct > 15) {
    s_warn10 = s_warn5 = false;
  } else if (b.pct <= 5 && !s_warn5) {
    s_warn5 = s_warn10 = true;
    ui_toast("Batteria quasi esaurita: collega il caricatore");
  } else if (b.pct <= 10 && !s_warn10) {
    s_warn10 = true;
    ui_toast("Batteria al 10%");
  }
}

// Schermata a tutto schermo prima dello standby (o all'accensione con la batteria
// troppo scarica). Non serve il resto dell'interfaccia.
void ui_battery_empty_screen(bool at_boot) {
  if (!at_boot) {
    player_pause();
    ui_session_save();
  }
  lv_obj_t *o = ui_box(lv_layer_top());
  lv_obj_set_size(o, SCR_W, SCR_H);
  lv_obj_set_style_bg_color(o, C_BLACK, 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
  lv_obj_add_flag(o, LV_OBJ_FLAG_CLICKABLE);   // blocca i tocchi sotto
  lv_obj_t *ic = ui_battery_icon(o, 64, 28);
  ui_battery_icon_set(ic, 6, C_RED);
  lv_obj_align(ic, LV_ALIGN_CENTER, 0, -46);
  lv_obj_t *t = ui_label(o, &font_b20, C_TEXT, "Batteria scarica");
  lv_obj_align(t, LV_ALIGN_CENTER, 0, 0);
  lv_obj_t *s = ui_label(o, &font_r12, C_SUB,
                         at_boot ? "Collega il caricatore. Se è già in carica,\nriprova tra qualche minuto."
                                 : "Ho salvato il punto in cui eri.\nCollega il caricatore e tocca lo\nschermo per riaccendere.");
  lv_obj_set_style_text_align(s, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(s, LV_ALIGN_CENTER, 0, 40);
}
