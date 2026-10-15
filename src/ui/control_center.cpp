// Centro di controllo in stile iPhone: si apre scorrendo dal bordo in basso
// verso l'alto e segue il dito; si chiude trascinando giù la maniglia in alto
// (o toccandola), scorrendo giù, toccando fuori dai riquadri o col gesto "indietro".
//
//                         [ ==== ]  maniglia
//   [ Wi-Fi  | Altoparlante ]  [   anello batteria     ]
//   [ Cuffie | Automatico   ]  [   stato di carica     ]
//   [                       ]  [                       ]
//   [        volume         ]  [      luminosità       ]
//   [                       ]  [                       ]
//
// I due cursori si usano come su iPhone: si trascina il dito su o giù partendo da
// un punto qualsiasi (movimento relativo, ~1,6 px per 1%).
#include "ui_internal.h"
#include <stdio.h>
#include "../hal/battery.h"
#include "../audio/audio_out.h"
#include "../net/xfer.h"
#include "../lib/library.h"
#include "../platform.h"

#define IOS_BLUE lv_color_hex(0x0A84FF)
#define IOS_GREEN lv_color_hex(0x30D158)
#define IOS_RED lv_color_hex(0xFF453A)
#define IOS_GLYPH lv_color_hex(0x6E6E73)   // icona grigia sopra il riempimento bianco

namespace {

constexpr int M = 14, GAP = 10;
constexpr int TW = (SCR_W - 2 * M - GAP) / 2;   // lato dei riquadri (101)
constexpr int Y_ROW1 = 28;                      // sopra: la maniglia
constexpr int Y_ROW2 = Y_ROW1 + TW + GAP;
constexpr int SL_H = SCR_H - 14 - Y_ROW2;       // altezza dei cursori (165)
constexpr int BG_OPA = 255;                     // da aperto copre tutto (durante il gesto sfuma)
constexpr int RB = 40;                          // pulsanti rotondi

struct Slider {
  lv_obj_t *box, *fill, *icon, *val;
  int v = 0, v0 = 0, y0 = 0, min = 0;
  bool drag = false;
};

lv_obj_t *cc = nullptr, *panel;
lv_obj_t *grip;                 // la "maniglia" disegnata
int h_y0 = 0;                   // trascinamento della maniglia
bool h_drag = false;
lv_obj_t *b_wifi, *b_spk, *b_hp, *b_auto;
lv_obj_t *bat_arc, *bat_pct, *bat_l1, *bat_l2;
Slider s_vol, s_bri;
bool cc_on = false;   // aperto, in apertura o trascinato
bool cc_closing = false;
lv_timer_t *cc_timer = nullptr;

// ------------------------------------------------------------ aspetto ----
lv_obj_t *tile(int x, int y, int w, int h, int r) {
  lv_obj_t *t = ui_box(panel);
  lv_obj_set_pos(t, x, y);
  lv_obj_set_size(t, w, h);
  lv_obj_set_style_radius(t, r, 0);
  lv_obj_set_style_bg_color(t, lv_color_white(), 0);
  lv_obj_set_style_bg_opa(t, 40, 0);
  lv_obj_add_flag(t, LV_OBJ_FLAG_CLICKABLE);   // un tocco sul riquadro non chiude
  return t;   // i gesti risalgono fino al centro di controllo: scorrere giù chiude anche da qui
}

lv_obj_t *round_btn(lv_obj_t *parent, int x, int y, lv_event_cb_t cb) {
  lv_obj_t *b = ui_box(parent);
  lv_obj_set_size(b, RB, RB);
  lv_obj_set_pos(b, x, y);
  lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
  lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_transform_scale(b, 232, LV_STATE_PRESSED);
  lv_obj_set_style_transform_pivot_x(b, RB / 2, 0);
  lv_obj_set_style_transform_pivot_y(b, RB / 2, 0);
  lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, nullptr);
  return b;
}

void round_set(lv_obj_t *b, bool on) {
  lv_obj_set_style_bg_color(b, on ? IOS_BLUE : lv_color_white(), 0);
  lv_obj_set_style_bg_opa(b, on ? LV_OPA_COVER : 52, 0);
}

const char *vol_icon(OutputDev d, int v) {
  if (d == DEV_HEADPHONES) return IC_HEADPHONES;
  return v == 0 ? IC_VOL_MUTE : v < 45 ? IC_VOL_LOW : IC_SPEAKER;
}

void slider_show(Slider &S) {
  int h = (S.v * SL_H + 50) / 100;
  lv_obj_set_height(S.fill, h);
  lv_label_set_text_fmt(S.val, "%d%%", S.v);
  if (&S == &s_vol) lv_label_set_text(S.icon, vol_icon(audio_out_active(), S.v));
  // come su iPhone: dove il riempimento bianco copre il simbolo, il simbolo diventa grigio
  lv_obj_set_style_text_color(S.icon, h > 32 ? IOS_GLYPH : C_TEXT, 0);
  lv_obj_set_style_text_color(S.val, h > SL_H - 22 ? IOS_GLYPH : C_TEXT, 0);
}

// ----------------------------------------------------------- aggiorna ----
// Ridisegna solo se qualcosa è cambiato
uint32_t s_sig = 0;

// seconda riga della batteria: tempo alla carica completa, autonomia o tensione
void bat_line2(const BatteryInfo &b, char *buf, size_t n) {
  char t[24];
  if (b.state == BAT_CHARGING) {
    ui_fmt_minutes(b.minutes_to_full, t, sizeof t);
    if (t[0]) snprintf(buf, n, "piena tra %s", t);
    else buf[0] = 0;
  } else if (b.state == BAT_DISCHARGING && b.minutes_left >= 0) {
    ui_fmt_minutes(b.minutes_left, t, sizeof t);
    snprintf(buf, n, "%s", t);
  } else {
    snprintf(buf, n, "%d,%02d V", b.mv / 1000, b.mv % 1000 / 10);
  }
}

void refresh_status(bool force = false) {
  BatteryInfo b = battery_info();
  char l2[40];
  bat_line2(b, l2, sizeof l2);
  uint32_t sig = 2166136261u;   // impronta di tutto ciò che si vede
  auto mix = [&](uint32_t v) { sig = (sig ^ v) * 16777619u; };
  mix(b.state); mix(b.pct); mix(b.low); mix(audio_out_mode()); mix(audio_out_active());
  mix(xfer_status().state != XF_OFF);
  for (const char *c = l2; *c; c++) mix((uint8_t)*c);
  if (!force && sig == s_sig) return;
  s_sig = sig;
  bool have = b.state != BAT_NONE && b.pct >= 0;
  bool chg = b.state == BAT_CHARGING || b.state == BAT_FULL;
  OutputMode m = audio_out_mode();

  // connessioni e uscita audio
  round_set(b_wifi, xfer_status().state != XF_OFF);
  round_set(b_spk, m == OUT_SPEAKER);
  round_set(b_hp, m == OUT_HEADPHONES);
  round_set(b_auto, m == OUT_AUTO);

  // batteria
  if (!have) {
    lv_arc_set_value(bat_arc, 0);
    lv_label_set_text(bat_pct, "--");
    lv_label_set_text(bat_l1, "Batteria");
    lv_obj_set_style_text_color(bat_l1, C_TEXT, 0);
    lv_label_set_text(bat_l2, "non rilevata");
    return;
  }
  lv_arc_set_value(bat_arc, b.pct);
  lv_obj_set_style_arc_color(bat_arc, b.low ? IOS_RED : IOS_GREEN, LV_PART_INDICATOR);
  lv_label_set_text_fmt(bat_pct, "%d%%", b.pct);
  lv_obj_set_style_text_color(bat_l1, chg ? IOS_GREEN : (b.low ? IOS_RED : C_TEXT), 0);
  if (b.state == BAT_CHARGING) lv_label_set_text(bat_l1, IC_BOLT " In carica");
  else lv_label_set_text(bat_l1, b.state == BAT_FULL ? "Carica completa" : b.low ? "Batteria scarica" : "A batteria");
  lv_label_set_text(bat_l2, l2);
}

void refresh_sliders(bool force = false) {
  static OutputDev last_dev = DEV_SPEAKER;
  OutputDev d = audio_out_active();
  if (!s_vol.drag && (force || d != last_dev || s_vol.v != audio_out_volume(d))) {
    s_vol.v = audio_out_volume(d);
    slider_show(s_vol);
  }
  last_dev = d;
  if (!s_bri.drag && (force || s_bri.v != ui_brightness())) {
    s_bri.v = ui_brightness();
    slider_show(s_bri);
  }
}

void timer_cb(lv_timer_t *) {
  refresh_status();
  refresh_sliders();
}

// ------------------------------------------------- apertura/chiusura ----
void set_pos(int32_t y) {
  if (y < 0) y = 0;
  if (y > SCR_H) y = SCR_H;
  lv_obj_set_y(panel, y);
  lv_obj_set_style_bg_opa(cc, (lv_opa_t)(BG_OPA * (SCR_H - y) / SCR_H), 0);
}

void anim_cb(void *, int32_t v) { set_pos(v); }

void anim_closed_cb(lv_anim_t *) {
  lv_obj_add_flag(cc, LV_OBJ_FLAG_HIDDEN);
  cc_on = false;
  cc_closing = false;
}

void animate(bool open) {
  lv_anim_delete(cc, anim_cb);
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, cc);
  lv_anim_set_exec_cb(&a, anim_cb);
  int32_t y = lv_obj_get_y(panel);
  lv_anim_set_values(&a, y, open ? 0 : SCR_H);
  int32_t dist = open ? y : SCR_H - y;
  lv_anim_set_duration(&a, 90 + dist * (open ? 160 : 130) / SCR_H);
  lv_anim_set_path_cb(&a, open ? lv_anim_path_ease_out : lv_anim_path_ease_in);
  if (!open) lv_anim_set_ready_cb(&a, anim_closed_cb);
  cc_closing = !open;
  lv_anim_start(&a);
}

// Sfondo: al posto della sfocatura dell'iPhone, una sfumatura col colore
// dell'album in riproduzione (o grigio scuro)
void tint() {
  int t = ui_current_track();
  lv_color_t c = t >= 0 ? col_darken(album_color(lib_tracks()[t].album), 110) : lv_color_hex(0x2A2A2A);
  lv_obj_set_style_bg_color(cc, c, 0);
  lv_obj_set_style_bg_grad_color(cc, C_BLACK, 0);
  lv_obj_set_style_bg_grad_dir(cc, LV_GRAD_DIR_VER, 0);
}

void show() {
  cc_closing = false;
  if (!cc_on) {
    cc_on = true;
    plat_event("centro controllo");
    tint();
    refresh_status(true);
    refresh_sliders(true);
    set_pos(SCR_H);
    lv_obj_remove_flag(cc, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_to_index(cc, -1);
  }
  if (!cc_timer) cc_timer = lv_timer_create(timer_cb, 500, nullptr);
}

void hide() {
  if (!cc_on) return;
  if (cc_timer) {
    lv_timer_delete(cc_timer);
    cc_timer = nullptr;
  }
  animate(false);
}

// ------------------------------------------------------------- eventi ----
// Un tocco vale solo se non è la fine di uno scorrimento (LVGL manda comunque
// "cliccato" al rilascio) e se il centro non si sta chiudendo.
bool real_click() {
  lv_indev_t *in = lv_indev_active();
  return cc_on && !cc_closing && !(in && lv_indev_get_gesture_dir(in) != LV_DIR_NONE);
}

void bg_cb(lv_event_t *e) {
  lv_event_code_t c = lv_event_get_code(e);
  if (c == LV_EVENT_CLICKED) {
    hide();
  } else if (c == LV_EVENT_GESTURE) {
    if (lv_indev_get_gesture_dir(lv_indev_active()) == LV_DIR_BOTTOM) hide();
  }
}

// Maniglia: un tocco chiude; trascinandola il pannello scende col dito e al
// rilascio si chiude (oltre ~40 px) o torna su
void handle_cb(lv_event_t *e) {
  lv_event_code_t c = lv_event_get_code(e);
  lv_indev_t *in = lv_indev_active();
  if (!in || !cc_on || cc_closing) return;
  lv_point_t p;
  lv_indev_get_point(in, &p);
  if (c == LV_EVENT_PRESSED) {
    h_drag = true;
    h_y0 = p.y;
    lv_anim_delete(cc, anim_cb);
    lv_obj_set_style_bg_opa(grip, LV_OPA_80, 0);
  } else if (c == LV_EVENT_PRESSING && h_drag) {
    int dy = p.y - h_y0;
    set_pos(dy < 0 ? 0 : dy);
  } else if ((c == LV_EVENT_RELEASED || c == LV_EVENT_PRESS_LOST) && h_drag) {
    h_drag = false;
    lv_obj_set_style_bg_opa(grip, LV_OPA_40, 0);
    int dy = p.y - h_y0;
    if (dy < 6 || dy > 40) hide();   // tocco o trascinamento deciso
    else animate(true);              // poco: torna su
  }
}

void wifi_cb(lv_event_t *) {
  if (!real_click()) return;
  plat_event("cc wifi");
  bool active = xfer_status().state != XF_OFF;
  hide();
  if (!active && !ui_transfer_visible()) ui_transfer_open();
}

void out_cb(lv_event_t *e) {
  if (!real_click()) return;
  lv_obj_t *b = lv_event_get_target_obj(e);
  OutputMode m = b == b_spk ? OUT_SPEAKER : b == b_hp ? OUT_HEADPHONES : OUT_AUTO;
  audio_out_set_mode(m);
  plat_event("cc uscita", m == OUT_SPEAKER ? "altoparlante" : m == OUT_HEADPHONES ? "cuffie" : "auto");
  ui_pref_set("mode", m);
  refresh_status(true);
  refresh_sliders(true);   // il volume è per uscita
}

void slider_cb(lv_event_t *e) {
  Slider &S = *(Slider *)lv_event_get_user_data(e);
  lv_event_code_t c = lv_event_get_code(e);
  lv_indev_t *in = lv_indev_active();
  if (!in) return;
  lv_point_t p;
  lv_indev_get_point(in, &p);
  if (c == LV_EVENT_PRESSED) {
    if (cc_closing) return;
    S.drag = true;
    S.y0 = p.y;
    S.v0 = S.v;
  } else if (c == LV_EVENT_PRESSING && S.drag) {
    int v = S.v0 + (S.y0 - p.y) * 100 / SL_H;
    v = v < S.min ? S.min : v > 100 ? 100 : v;
    if (v == S.v) return;
    S.v = v;
    if (&S == &s_vol) audio_out_set_volume(audio_out_active(), v);
    else ui_set_brightness(v, false);
    slider_show(S);
  } else if ((c == LV_EVENT_RELEASED || c == LV_EVENT_PRESS_LOST) && S.drag) {
    S.drag = false;
    // si salva solo al rilascio: niente scritture in flash a ogni passo
    if (&S == &s_vol) {
      OutputDev d = audio_out_active();
      plat_event("cc volume");
      ui_pref_set(d == DEV_HEADPHONES ? "vol_h" : "vol_s", audio_out_volume(d));
    } else {
      plat_event("cc luminosita");
      ui_set_brightness(S.v, true);
    }
  }
}

void slider_build(Slider &S, int x, const char *ic, int min) {
  S.min = min;
  S.box = tile(x, Y_ROW2, TW, SL_H, 24);
  lv_obj_set_style_clip_corner(S.box, true, 0);
  lv_obj_add_flag(S.box, LV_OBJ_FLAG_PRESS_LOCK);
  lv_obj_remove_flag(S.box, LV_OBJ_FLAG_GESTURE_BUBBLE);   // trascinare il cursore non chiude
  lv_obj_add_event_cb(S.box, slider_cb, LV_EVENT_ALL, &S);
  S.fill = ui_box(S.box);
  lv_obj_set_size(S.fill, TW, 0);
  lv_obj_align(S.fill, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_set_style_bg_color(S.fill, lv_color_white(), 0);
  lv_obj_set_style_bg_opa(S.fill, LV_OPA_COVER, 0);
  lv_obj_remove_flag(S.fill, LV_OBJ_FLAG_CLICKABLE);
  S.icon = ui_icon(S.box, &icons_22, ic, C_TEXT);
  lv_obj_align(S.icon, LV_ALIGN_BOTTOM_MID, 0, -14);
  S.val = ui_label(S.box, &font_b12, C_TEXT, "");
  lv_obj_align(S.val, LV_ALIGN_TOP_MID, 0, 12);
}

}  // namespace

// ================================================================= API ==

void ui_cc_build() {
  // il livello in primo piano non deve scorrere: altrimenti trascinare sopra al
  // centro di controllo (o a un foglio) sposterebbe tutto invece di generare il gesto
  lv_obj_remove_flag(lv_layer_top(), LV_OBJ_FLAG_SCROLLABLE);
  cc = ui_box(lv_layer_top());
  lv_obj_set_size(cc, SCR_W, SCR_H);
  lv_obj_set_style_bg_color(cc, C_BLACK, 0);
  lv_obj_add_flag(cc, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(cc, LV_OBJ_FLAG_GESTURE_BUBBLE);   // i gesti dei riquadri si fermano qui
  lv_obj_add_event_cb(cc, bg_cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_add_event_cb(cc, bg_cb, LV_EVENT_GESTURE, nullptr);

  panel = ui_box(cc);   // non cliccabile: i tocchi nei vuoti arrivano allo sfondo (chiude)
  lv_obj_set_size(panel, SCR_W, SCR_H);
  lv_obj_remove_flag(panel, LV_OBJ_FLAG_CLICKABLE);

  // maniglia in alto: zona larga e alta quanto basta per il dito, trattino al centro
  lv_obj_t *hz = ui_box(panel);
  lv_obj_set_size(hz, SCR_W, Y_ROW1);
  lv_obj_add_flag(hz, LV_OBJ_FLAG_PRESS_LOCK);
  lv_obj_remove_flag(hz, LV_OBJ_FLAG_GESTURE_BUBBLE);   // il trascinamento lo gestisce lei
  lv_obj_add_event_cb(hz, handle_cb, LV_EVENT_ALL, nullptr);
  grip = ui_box(hz);
  lv_obj_set_size(grip, 36, 5);
  lv_obj_align(grip, LV_ALIGN_TOP_MID, 0, 11);
  lv_obj_set_style_radius(grip, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(grip, lv_color_white(), 0);
  lv_obj_set_style_bg_opa(grip, LV_OPA_40, 0);
  lv_obj_remove_flag(grip, LV_OBJ_FLAG_CLICKABLE);
  static lv_font_t pct_font;   // testo + fulmine (dal font delle icone)
  pct_font = font_b12;
  pct_font.fallback = &icons_14;

  // connessioni + uscita audio (quattro pulsanti rotondi, come su iPhone)
  lv_obj_t *t1 = tile(M, Y_ROW1, TW, TW, 22);
  int p = (TW - 2 * RB - 7) / 2;
  b_wifi = round_btn(t1, p, p, wifi_cb);
  lv_obj_center(ui_icon(b_wifi, &icons_18, IC_WIFI, C_TEXT));
  b_spk = round_btn(t1, TW - p - RB, p, out_cb);
  lv_obj_center(ui_icon(b_spk, &icons_18, IC_SPEAKER, C_TEXT));
  b_hp = round_btn(t1, p, TW - p - RB, out_cb);
  lv_obj_center(ui_icon(b_hp, &icons_18, IC_HEADPHONES, C_TEXT));
  b_auto = round_btn(t1, TW - p - RB, TW - p - RB, out_cb);
  lv_obj_center(ui_label(b_auto, &font_b10, C_TEXT, "AUTO"));

  // batteria: anello come il widget Batterie dell'iPhone
  lv_obj_t *t2 = tile(M + TW + GAP, Y_ROW1, TW, TW, 22);
  bat_arc = lv_arc_create(t2);
  lv_obj_remove_style_all(bat_arc);
  lv_obj_set_size(bat_arc, 54, 54);
  lv_obj_align(bat_arc, LV_ALIGN_TOP_MID, 0, 8);
  lv_arc_set_rotation(bat_arc, 270);
  lv_arc_set_bg_angles(bat_arc, 0, 360);
  lv_arc_set_range(bat_arc, 0, 100);
  lv_obj_remove_flag(bat_arc, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_arc_width(bat_arc, 6, LV_PART_MAIN);
  lv_obj_set_style_arc_color(bat_arc, lv_color_white(), LV_PART_MAIN);
  lv_obj_set_style_arc_opa(bat_arc, 50, LV_PART_MAIN);
  lv_obj_set_style_arc_width(bat_arc, 6, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(bat_arc, IOS_GREEN, LV_PART_INDICATOR);
  lv_obj_set_style_arc_opa(bat_arc, LV_OPA_COVER, LV_PART_INDICATOR);
  lv_obj_set_style_arc_rounded(bat_arc, true, LV_PART_INDICATOR);
  bat_pct = ui_label(t2, &font_b14, C_TEXT, "");
  lv_obj_align(bat_pct, LV_ALIGN_TOP_MID, 0, 8 + 27 - 9);   // al centro dell'anello
  bat_l1 = ui_label(t2, &pct_font, C_TEXT, "");
  lv_obj_align(bat_l1, LV_ALIGN_TOP_MID, 0, 66);
  bat_l2 = ui_label(t2, &font_r10, C_SUB, "");
  lv_obj_align(bat_l2, LV_ALIGN_TOP_MID, 0, 82);

  // volume e luminosità
  slider_build(s_vol, M, IC_SPEAKER, 0);
  slider_build(s_bri, M + TW + GAP, IC_SUN, 10);

  lv_obj_add_flag(cc, LV_OBJ_FLAG_HIDDEN);
}

bool ui_cc_visible() { return cc_on; }

bool ui_cc_close() {
  if (!cc_on) return false;
  hide();
  return true;
}

void ui_cc_drag(int dist) {
  if (!cc) return;
  show();
  lv_anim_delete(cc, anim_cb);
  set_pos(SCR_H - dist);
}

void ui_cc_drag_end(bool commit, int speed) {
  (void)speed;
  if (!cc_on) return;
  if (commit) {
    animate(true);
  } else {
    if (cc_timer) {
      lv_timer_delete(cc_timer);
      cc_timer = nullptr;
    }
    animate(false);
  }
}

// il bordo sinistro sopra al cursore del volume serve al cursore, non al gesto "indietro"
bool ui_cc_blocks_edge(int x, int y) {
  (void)x;
  return cc_on && y >= Y_ROW2;
}
