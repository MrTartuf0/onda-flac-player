// Centro di controllo in stile iPhone: si apre scorrendo dal bordo in basso
// verso l'alto e segue il dito; si chiude trascinando giù la maniglia in alto
// (o toccandola), scorrendo giù, toccando fuori dai riquadri o col gesto "indietro".
//
//                         [ ==== ]  maniglia
//   [ Wi-Fi  | Altoparlante ]  [   anello batteria     ]
//   [ Cuffie | Automatico   ]  [   stato di carica     ]
//   [        volume         ]  [      luminosità       ]
//   [                       ]  [                       ]
//   [ ⏱ 12:34  sessione            30 min   ( ▶ / ■ )  ]   timer della sessione
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
#include "../session.h"

#define IOS_BLUE lv_color_hex(0x0A84FF)
#define IOS_GREEN lv_color_hex(0x30D158)
#define IOS_RED lv_color_hex(0xFF453A)
#define IOS_GLYPH lv_color_hex(0x6E6E73)   // icona grigia sopra il riempimento bianco

namespace {

constexpr int M = 14, GAP = 10;
constexpr int TW = (SCR_W - 2 * M - GAP) / 2;   // lato dei riquadri (101)
constexpr int Y_ROW1 = 28;                      // sopra: la maniglia
constexpr int Y_ROW2 = Y_ROW1 + TW + GAP;
constexpr int TM_H = 48;                        // riga del timer della sessione
constexpr int SL_H = SCR_H - 14 - Y_ROW2 - TM_H - GAP;   // altezza dei cursori (109)
constexpr int Y_ROW3 = Y_ROW2 + SL_H + GAP;
constexpr int BG_OPA = 255;                     // da aperto copre tutto (durante il gesto sfuma)
constexpr int RB = 40;                          // pulsanti rotondi

struct Slider {
  lv_obj_t *box, *fill, *icon, *val;
  int v = 0, v0 = 0, y0 = 0, min = 0;
  bool drag = false;    // dito sul cursore
  bool moved = false;   // il dito si è spostato: trascinamento (relativo), non tocco
  bool anim = false;    // riempimento in animazione verso il valore toccato
};

lv_obj_t *cc = nullptr, *panel;
lv_obj_t *grip;                 // la "maniglia" disegnata
int h_y0 = 0;                   // trascinamento della maniglia
bool h_drag = false;
lv_obj_t *b_wifi, *b_spk, *b_hp, *b_auto;
lv_obj_t *tm_time, *tm_sub, *tm_lim, *tm_btn;   // timer della sessione
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
  if (!s_vol.drag && !s_vol.anim && (force || d != last_dev || s_vol.v != audio_out_volume(d))) {
    s_vol.v = audio_out_volume(d);
    slider_show(s_vol);
  }
  last_dev = d;
  if (!s_bri.drag && !s_bri.anim && (force || s_bri.v != ui_brightness())) {
    s_bri.v = ui_brightness();
    slider_show(s_bri);
  }
}

// ------------------------------------------------- timer sessione ----
void fmt_limit(int m, char *b, size_t n) {
  if (!m) snprintf(b, n, "off");
  else if (m % 60 == 0) snprintf(b, n, "%d h", m / 60);
  else if (m > 60) snprintf(b, n, "%d h %d", m / 60, m % 60);
  else snprintf(b, n, "%d min", m);
}

void refresh_session(bool force = false) {
  static int32_t last_s = -2, last_lim = -1;
  static bool last_over = false;
  int32_t sec = session_running() ? (int32_t)(session_elapsed_ms() / 1000) : -1;
  int lim = session_limit_min();
  bool over = session_over();
  if (!force && sec == last_s && lim == last_lim && over == last_over) return;
  bool run_changed = (sec < 0) != (last_s < 0) || force;
  last_s = sec;
  last_lim = lim;
  last_over = over;
  char b[24];
  if (sec < 0) {
    lv_label_set_text(tm_time, "Timer");
    lv_label_set_text(tm_sub, "sessione");
  } else {
    if (sec >= 3600) snprintf(b, sizeof b, "%d:%02d:%02d", (int)(sec / 3600), (int)(sec / 60 % 60), (int)(sec % 60));
    else snprintf(b, sizeof b, "%d:%02d", (int)(sec / 60), (int)(sec % 60));
    lv_label_set_text(tm_time, b);
    lv_label_set_text(tm_sub, over ? "superato" : "in corso");
  }
  lv_obj_set_style_text_color(tm_time, over ? IOS_RED : C_TEXT, 0);
  lv_obj_set_style_text_color(tm_sub, over ? IOS_RED : C_SUB, 0);
  fmt_limit(lim, b, sizeof b);
  lv_label_set_text(lv_obj_get_child(tm_lim, 0), b);
  if (run_changed) {
    bool run = sec >= 0;
    lv_obj_set_style_bg_color(tm_btn, run ? IOS_RED : IOS_GREEN, 0);
    lv_label_set_text(lv_obj_get_child(tm_btn, 0), run ? IC_STOP : IC_PLAY);
    lv_obj_set_style_translate_x(lv_obj_get_child(tm_btn, 0), run ? 0 : 2, 0);
  }
}

void tm_btn_cb(lv_event_t *) {
  session_toggle(false);
  refresh_session(true);
}

// tocco sul limite: passa al valore successivo
void tm_lim_cb(lv_event_t *) {
  static const int opts[] = {10, 15, 20, 30, 45, 60, 90, 120, 0};
  int cur = session_limit_min(), n = sizeof opts / sizeof opts[0], i = 0;
  while (i < n && opts[i] != cur) i++;
  session_set_limit_min(opts[(i + 1) % n]);
  refresh_session(true);
}

void timer_cb(lv_timer_t *) {
  refresh_status();
  refresh_sliders();
  refresh_session();
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
    refresh_session(true);
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

// Applica il valore (volume dell'uscita in uso o luminosità), senza salvare
void slider_apply(Slider &S, int v) {
  if (&S == &s_vol) audio_out_set_volume(audio_out_active(), v);
  else ui_set_brightness(v, false);
}

// Salvataggio al rilascio: niente scritture in flash a ogni passo
void slider_save(Slider &S) {
  if (&S == &s_vol) {
    OutputDev d = audio_out_active();
    plat_event("cc volume");
    ui_pref_set(d == DEV_HEADPHONES ? "vol_h" : "vol_s", audio_out_volume(d));
  } else {
    plat_event("cc luminosita");
    ui_set_brightness(S.v, true);
  }
}

void fill_anim_cb(void *var, int32_t v) {
  Slider &S = *(Slider *)var;
  S.v = v;
  slider_show(S);
}
void fill_anim_done(lv_anim_t *a) { ((Slider *)a->var)->anim = false; }

// Un tocco porta il cursore dove hai toccato (arrotondato al 5%); trascinando
// invece si sposta di quanto muovi il dito, da qualsiasi punto parti.
void slider_cb(lv_event_t *e) {
  Slider &S = *(Slider *)lv_event_get_user_data(e);
  lv_event_code_t c = lv_event_get_code(e);
  lv_indev_t *in = lv_indev_active();
  if (!in) return;
  lv_point_t p;
  lv_indev_get_point(in, &p);
  constexpr int kMove = 6;   // oltre questi pixel è un trascinamento
  if (c == LV_EVENT_PRESSED) {
    if (cc_closing) return;
    lv_anim_delete(&S, fill_anim_cb);
    S.anim = false;
    S.drag = true;
    S.moved = false;
    S.y0 = p.y;
    S.v0 = S.v;
  } else if (c == LV_EVENT_PRESSING && S.drag) {
    if (!S.moved) {
      if (LV_ABS(p.y - S.y0) <= kMove) return;
      S.moved = true;
      S.y0 = p.y;   // si parte da qui: nessun salto quando inizia il trascinamento
    }
    int v = S.v0 + (S.y0 - p.y) * 100 / SL_H;
    v = v < S.min ? S.min : v > 100 ? 100 : v;
    if (v == S.v) return;
    S.v = v;
    slider_apply(S, v);
    slider_show(S);
  } else if ((c == LV_EVENT_RELEASED || c == LV_EVENT_PRESS_LOST) && S.drag) {
    S.drag = false;
    if (!S.moved && c == LV_EVENT_RELEASED) {
      lv_area_t a;
      lv_obj_get_coords(S.box, &a);
      int v = (a.y2 - p.y) * 100 / SL_H;
      v = (v + 2) / 5 * 5;
      v = v < S.min ? S.min : v > 100 ? 100 : v;
      // Sicurezza per le orecchie: in cuffia un tocco non alza oltre il 75%.
      // Più su si arriva solo trascinando (abbassare col tocco va sempre bene).
      constexpr int kHpTapMax = 75;
      if (&S == &s_vol && audio_out_active() == DEV_HEADPHONES && v > kHpTapMax) {
        v = S.v > kHpTapMax ? S.v : kHpTapMax;
        ui_toast("Cuffie: oltre il 75% trascina");
      }
      if (v == S.v) {
        slider_save(S);
        return;
      }
      slider_apply(S, v);   // il volume ha già la sua rampa: niente salti bruschi
      // il riempimento scorre fino al punto toccato
      S.anim = true;
      lv_anim_t an;
      lv_anim_init(&an);
      lv_anim_set_var(&an, &S);
      lv_anim_set_exec_cb(&an, fill_anim_cb);
      lv_anim_set_values(&an, S.v, v);
      lv_anim_set_duration(&an, 160);
      lv_anim_set_path_cb(&an, lv_anim_path_ease_out);
      lv_anim_set_completed_cb(&an, fill_anim_done);
      lv_anim_start(&an);
      S.v = v;   // valore vero subito (l'animazione lo ridisegna passo passo)
    }
    slider_save(S);
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

  // timer della sessione (es. highline): tempo trascorso, limite, avvia/ferma
  lv_obj_t *t3 = tile(M, Y_ROW3, SCR_W - 2 * M, TM_H, TM_H / 2);
  lv_obj_t *sw = ui_icon(t3, &icons_18, IC_STOPWATCH, C_TEXT);
  lv_obj_align(sw, LV_ALIGN_LEFT_MID, 16, 0);
  tm_time = ui_label(t3, &font_b16, C_TEXT, "");
  lv_obj_align(tm_time, LV_ALIGN_LEFT_MID, 42, -7);
  tm_sub = ui_label(t3, &font_r10, C_SUB, "");
  lv_obj_align(tm_sub, LV_ALIGN_LEFT_MID, 42, 10);
  constexpr int TB = 36;
  tm_btn = ui_box(t3);
  lv_obj_set_size(tm_btn, TB, TB);
  lv_obj_align(tm_btn, LV_ALIGN_RIGHT_MID, -6, 0);
  lv_obj_set_style_radius(tm_btn, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(tm_btn, LV_OPA_COVER, 0);
  lv_obj_add_flag(tm_btn, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_ext_click_area(tm_btn, 6);
  lv_obj_set_style_transform_scale(tm_btn, 232, LV_STATE_PRESSED);
  lv_obj_set_style_transform_pivot_x(tm_btn, TB / 2, 0);
  lv_obj_set_style_transform_pivot_y(tm_btn, TB / 2, 0);
  lv_obj_add_event_cb(tm_btn, tm_btn_cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_center(ui_icon(tm_btn, &icons_14, IC_PLAY, C_BLACK));
  tm_lim = ui_box(t3);
  lv_obj_set_size(tm_lim, 54, 28);
  lv_obj_align(tm_lim, LV_ALIGN_RIGHT_MID, -6 - TB - 8, 0);
  lv_obj_set_style_radius(tm_lim, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(tm_lim, lv_color_white(), 0);
  lv_obj_set_style_bg_opa(tm_lim, 52, 0);
  lv_obj_set_style_bg_opa(tm_lim, 90, LV_STATE_PRESSED);
  lv_obj_add_flag(tm_lim, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_ext_click_area(tm_lim, 6);
  lv_obj_add_event_cb(tm_lim, tm_lim_cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_center(ui_label(tm_lim, &font_b12, C_TEXT, ""));
  refresh_session(true);

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
