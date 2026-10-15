// Batteria nell'interfaccia: pila disegnata (usata dal centro di controllo e
// dalle impostazioni), avvisi di batteria scarica e schermata di spegnimento.
#include "ui.h"
#include "ui_internal.h"
#include <stdio.h>
#include "../hal/battery.h"
#include "../audio/player.h"
#include "../platform.h"

#define C_RED lv_color_hex(0xF15E6C)

namespace {

bool s_warn10 = false, s_warn5 = false;

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
  // il livello sta dentro al bordo (1 px) con lo stesso spazio su tutti i lati;
  // le coordinate dei figli partono già dall'interno del bordo
  lv_obj_t *fill = ui_box(body);
  int gap = h >= 16 ? 2 : 1;
  lv_obj_align(fill, LV_ALIGN_LEFT_MID, gap, 0);
  lv_obj_set_size(fill, 0, h - 2 - 2 * gap);
  lv_obj_set_style_radius(fill, h >= 16 ? 2 : 1, 0);
  lv_obj_set_style_bg_opa(fill, LV_OPA_COVER, 0);
  lv_obj_t *tip = ui_box(wrap);
  int th = h / 2 < 3 ? 3 : h / 2;
  lv_obj_set_size(tip, 2, th);
  lv_obj_set_pos(tip, w + 1, (h - th) / 2);
  lv_obj_set_style_bg_color(tip, C_TEXT, 0);
  lv_obj_set_style_bg_opa(tip, LV_OPA_70, 0);
  lv_obj_set_style_radius(tip, 1, 0);
  for (lv_obj_t *o : {wrap, body, fill, tip}) lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);   // solo disegno
  return wrap;
}

void ui_battery_icon_set(lv_obj_t *icon, int pct, lv_color_t c) {
  lv_obj_t *body = lv_obj_get_child(icon, 0);
  lv_obj_t *fill = lv_obj_get_child(body, 0);
  // dimensioni impostate (non serve aspettare il calcolo del layout)
  int gap = lv_obj_get_style_height(body, LV_PART_MAIN) >= 16 ? 2 : 1;
  int full = lv_obj_get_style_width(body, LV_PART_MAIN) - 2 - 2 * gap;
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;
  int w = (full * pct + 50) / 100;
  if (pct > 0 && w < 1) w = 1;
  lv_obj_set_width(fill, w);
  lv_obj_set_style_bg_color(fill, c, 0);
}

// "~45 min", "~3 h 20 min": arrotondato (è una stima)
void ui_fmt_minutes(int m, char *buf, size_t n) {
  if (m < 0) {
    buf[0] = 0;
  } else if (m < 10) {
    snprintf(buf, n, "~%d min", m < 1 ? 1 : m);
  } else if (m < 60) {
    snprintf(buf, n, "~%d min", (m + 2) / 5 * 5);
  } else if (m > 48 * 60) {
    snprintf(buf, n, "oltre 2 giorni");
  } else {
    int r = (m + 5) / 10 * 10;
    if (r % 60) snprintf(buf, n, "~%d h %02d min", r / 60, r % 60);
    else snprintf(buf, n, "~%d h", r / 60);
  }
}

const char *ui_battery_state_text(const BatteryInfo &b) {
  switch (b.state) {
    case BAT_CHARGING: return "In carica";
    case BAT_FULL: return "Carica completa";
    case BAT_DISCHARGING: return b.low ? "Batteria scarica" : "A batteria";
    default: return "Batteria non rilevata";
  }
}

// Avvisi di batteria scarica (una volta per ciclo di scarica)
void ui_battery_tick() {
  BatteryInfo b = battery_info();
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

void ui_power_off_screen() {
  player_pause();
  ui_session_save();
  lv_obj_t *o = ui_box(lv_layer_top());
  lv_obj_set_size(o, SCR_W, SCR_H);
  lv_obj_set_style_bg_color(o, C_BLACK, 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
  lv_obj_add_flag(o, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_t *t = ui_label(o, &font_b20, C_TEXT, "Spegnimento");
  lv_obj_align(t, LV_ALIGN_CENTER, 0, -10);
  lv_obj_t *s = ui_label(o, &font_r12, C_SUB, "Premi il tasto BOOT per riaccendere");
  lv_obj_align(s, LV_ALIGN_CENTER, 0, 18);
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
