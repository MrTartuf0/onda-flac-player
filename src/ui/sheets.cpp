// Fogli dal basso: dispositivo di uscita (stile "Connetti a un dispositivo")
// e menu del brano.
#include "ui_internal.h"
#include <stdio.h>
#include "../lib/library.h"
#include "../audio/player.h"
#include "../audio/audio_out.h"
#include "../platform.h"

namespace {

lv_obj_t *s_backdrop = nullptr;
lv_obj_t *s_sheet = nullptr;

void sheet_y_cb(void *o, int32_t v) { lv_obj_set_y((lv_obj_t *)o, v); }
void backdrop_opa_cb(void *o, int32_t v) { lv_obj_set_style_bg_opa((lv_obj_t *)o, v, 0); }
void sheet_deleted_cb(lv_anim_t *a) {
  lv_obj_delete((lv_obj_t *)a->var);   // elimina il backdrop (e con lui il foglio)
}

void sheet_close() {
  if (!s_backdrop) return;
  lv_obj_t *b = s_backdrop;
  lv_obj_t *sh = s_sheet;
  s_backdrop = s_sheet = nullptr;
  lv_obj_remove_flag(b, LV_OBJ_FLAG_CLICKABLE);
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, sh);
  lv_anim_set_exec_cb(&a, sheet_y_cb);
  lv_anim_set_values(&a, lv_obj_get_y(sh), SCR_H);
  lv_anim_set_duration(&a, 180);
  lv_anim_start(&a);
  lv_anim_t f;
  lv_anim_init(&f);
  lv_anim_set_var(&f, b);
  lv_anim_set_exec_cb(&f, backdrop_opa_cb);
  lv_anim_set_values(&f, LV_OPA_50, 0);
  lv_anim_set_duration(&f, 190);
  lv_anim_set_ready_cb(&f, sheet_deleted_cb);
  lv_anim_start(&f);
}

void backdrop_cb(lv_event_t *e) {
  if (lv_event_get_target_obj(e) == s_backdrop) sheet_close();
}

lv_obj_t *sheet_open() {
  if (s_backdrop) lv_obj_delete(s_backdrop);
  s_backdrop = ui_box(lv_layer_top());
  lv_obj_set_size(s_backdrop, SCR_W, SCR_H);
  lv_obj_set_style_bg_color(s_backdrop, C_BLACK, 0);
  lv_obj_set_style_bg_opa(s_backdrop, 0, 0);
  lv_obj_add_flag(s_backdrop, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(s_backdrop, backdrop_cb, LV_EVENT_CLICKED, nullptr);
  s_sheet = ui_box(s_backdrop);
  lv_obj_set_size(s_sheet, SCR_W, LV_SIZE_CONTENT);
  lv_obj_set_style_bg_color(s_sheet, C_ELEV, 0);
  lv_obj_set_style_bg_opa(s_sheet, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(s_sheet, 12, 0);
  lv_obj_set_style_pad_top(s_sheet, 14, 0);
  lv_obj_set_style_pad_bottom(s_sheet, 18, 0);
  lv_obj_set_flex_flow(s_sheet, LV_FLEX_FLOW_COLUMN);
  lv_obj_add_flag(s_sheet, LV_OBJ_FLAG_CLICKABLE);   // i tocchi sul foglio non lo chiudono
  lv_obj_t *handle = ui_box(s_sheet);
  lv_obj_set_size(handle, 36, 4);
  lv_obj_set_style_radius(handle, 2, 0);
  lv_obj_set_style_bg_color(handle, lv_color_hex(0x6A6A6A), 0);
  lv_obj_set_style_bg_opa(handle, LV_OPA_COVER, 0);
  lv_obj_set_style_align(handle, LV_ALIGN_TOP_MID, 0);
  lv_obj_add_flag(handle, LV_OBJ_FLAG_IGNORE_LAYOUT);
  lv_obj_set_y(handle, -8);
  return s_sheet;
}

void sheet_show() {
  lv_obj_update_layout(s_sheet);
  int h = lv_obj_get_height(s_sheet);
  lv_obj_set_y(s_sheet, SCR_H);
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, s_sheet);
  lv_anim_set_exec_cb(&a, sheet_y_cb);
  lv_anim_set_values(&a, SCR_H, SCR_H - h + 12);
  lv_anim_set_duration(&a, 240);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
  lv_anim_start(&a);
  lv_anim_t f;
  lv_anim_init(&f);
  lv_anim_set_var(&f, s_backdrop);
  lv_anim_set_exec_cb(&f, backdrop_opa_cb);
  lv_anim_set_values(&f, 0, LV_OPA_50);
  lv_anim_set_duration(&f, 200);
  lv_anim_start(&f);
}

lv_obj_t *option_row(lv_obj_t *p, const char *ic, const char *txt, const char *sub, bool checked,
                     lv_event_cb_t cb, void *ud, lv_color_t icol = C_SUB) {
  lv_obj_t *r = ui_box(p);
  lv_obj_set_size(r, SCR_W, sub ? 52 : 44);
  ui_pressable(r);
  if (cb) lv_obj_add_event_cb(r, cb, LV_EVENT_CLICKED, ud);
  lv_obj_t *i = ui_icon(r, &icons_18, ic, checked ? C_GREEN : icol);
  lv_obj_align(i, LV_ALIGN_LEFT_MID, 16, 0);
  lv_obj_t *t = ui_label(r, &font_r14, checked ? C_GREEN : C_TEXT, txt);
  ui_dot(t);
  lv_obj_set_width(t, SCR_W - 52 - 36);
  if (sub) {
    lv_obj_align(t, LV_ALIGN_LEFT_MID, 48, -8);
    lv_obj_t *s = ui_label(r, &font_r12, C_SUB, sub);
    ui_dot(s);
    lv_obj_set_width(s, SCR_W - 52 - 36);
    lv_obj_align(s, LV_ALIGN_LEFT_MID, 48, 9);
  } else {
    lv_obj_align(t, LV_ALIGN_LEFT_MID, 48, 0);
  }
  if (checked) {
    lv_obj_t *c = ui_icon(r, &icons_14, IC_CHECK, C_GREEN);
    lv_obj_align(c, LV_ALIGN_RIGHT_MID, -16, 0);
  }
  return r;
}

// --------------------------------------------------------- dispositivo ----
void device_build();

void mode_cb(lv_event_t *e) {
  OutputMode m = (OutputMode)(intptr_t)lv_event_get_user_data(e);
  audio_out_set_mode(m);
  plat_kv_set("mode", m);
  device_build();   // ricostruisce il contenuto del foglio aperto
}

void vol_cb(lv_event_t *e) {
  lv_obj_t *s = lv_event_get_target_obj(e);
  OutputDev d = audio_out_active();
  audio_out_set_volume(d, lv_slider_get_value(s));
  if (lv_event_get_code(e) == LV_EVENT_RELEASED)
    plat_kv_set(d == DEV_HEADPHONES ? "vol_h" : "vol_s", lv_slider_get_value(s));
}

void device_fill(lv_obj_t *sh) {
  bool hp = audio_out_active() == DEV_HEADPHONES;
  lv_obj_t *cur = ui_box(sh);
  lv_obj_set_size(cur, SCR_W, 58);
  lv_obj_t *ic = ui_icon(cur, &icons_28, hp ? IC_HEADPHONES : IC_SPEAKER, C_GREEN);
  lv_obj_align(ic, LV_ALIGN_LEFT_MID, 16, 0);
  lv_obj_t *l1 = ui_label(cur, &font_b12, C_TEXT, "Dispositivo attuale");
  lv_obj_align(l1, LV_ALIGN_LEFT_MID, 60, -10);
  lv_obj_t *l2 = ui_label(cur, &font_b16, C_GREEN, hp ? "Cuffie (PCM5102)" : "Altoparlante integrato");
  lv_obj_align(l2, LV_ALIGN_LEFT_MID, 60, 9);

  lv_obj_t *vr = ui_box(sh);
  lv_obj_set_size(vr, SCR_W, 34);
  lv_obj_t *vl = ui_icon(vr, &icons_14, IC_VOL_LOW, C_SUB);
  lv_obj_align(vl, LV_ALIGN_LEFT_MID, 16, 0);
  lv_obj_t *vh = ui_icon(vr, &icons_14, IC_SPEAKER, C_SUB);
  lv_obj_align(vh, LV_ALIGN_RIGHT_MID, -16, 0);
  lv_obj_t *sl = lv_slider_create(vr);
  lv_obj_set_size(sl, SCR_W - 90, 4);
  lv_obj_align(sl, LV_ALIGN_CENTER, 0, 0);
  lv_slider_set_range(sl, 0, 100);
  lv_slider_set_value(sl, audio_out_volume(audio_out_active()), LV_ANIM_OFF);
  lv_obj_set_ext_click_area(sl, 14);
  lv_obj_set_style_bg_color(sl, lv_color_hex(0x5A5A5A), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(sl, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(sl, C_GREEN, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(sl, C_TEXT, LV_PART_KNOB);
  lv_obj_set_style_pad_all(sl, 5, LV_PART_KNOB);
  lv_obj_add_event_cb(sl, vol_cb, LV_EVENT_VALUE_CHANGED, nullptr);
  lv_obj_add_event_cb(sl, vol_cb, LV_EVENT_RELEASED, nullptr);

  lv_obj_t *h = ui_label(sh, &font_b14, C_TEXT, "Seleziona un dispositivo");
  lv_obj_set_style_pad_left(h, 16, 0);
  lv_obj_set_style_pad_ver(h, 8, 0);
  OutputMode m = audio_out_mode();
  const char *hp_state = !audio_out_hp_detect_available() ? "Rilevamento non configurato"
                         : audio_out_hp_present()         ? "Collegate"
                                                          : "Non collegate";
  option_row(sh, IC_HEADPHONES, "Automatico", "Cuffie quando collegate, altrimenti altoparlante",
             m == OUT_AUTO, mode_cb, (void *)(intptr_t)OUT_AUTO);
  option_row(sh, IC_SPEAKER, "Altoparlante integrato", nullptr, m == OUT_SPEAKER, mode_cb,
             (void *)(intptr_t)OUT_SPEAKER);
  option_row(sh, IC_HEADPHONES, "Cuffie (PCM5102)", hp_state, m == OUT_HEADPHONES, mode_cb,
             (void *)(intptr_t)OUT_HEADPHONES);
}

void device_build() {
  if (!s_sheet) return;
  // svuota tutto tranne la maniglia (primo figlio)
  while (lv_obj_get_child_count(s_sheet) > 1) lv_obj_delete(lv_obj_get_child(s_sheet, 1));
  device_fill(s_sheet);
}

// ------------------------------------------------------------ menu brano ----
int s_menu_track = -1;

void menu_like_cb(lv_event_t *) {
  bool on = !lib_is_liked(s_menu_track);
  lib_set_liked(s_menu_track, on);
  ui_toast(on ? "Aggiunto a Brani che ti piacciono" : "Rimosso da Brani che ti piacciono");
  sheet_close();
  ui_nowplaying_refresh(RF_LIKED);
}
void menu_queue_cb(lv_event_t *) {
  if (ui_current_track() < 0) {
    std::vector<uint16_t> one{(uint16_t)s_menu_track};
    PlayContext c;
    c.type = CTX_SEARCH;
    snprintf(c.name, sizeof c.name, "%s", lib_tracks()[s_menu_track].title);
    player_play_list(one, 0, c);
  } else {
    player_enqueue(s_menu_track);
    ui_toast("Aggiunto alla coda");
  }
  sheet_close();
}
void menu_album_cb(lv_event_t *) {
  int a = lib_tracks()[s_menu_track].album;
  sheet_close();
  ui_open_album(a);
}
void menu_artist_cb(lv_event_t *) {
  int a = lib_albums()[lib_tracks()[s_menu_track].album].artist_idx;
  sheet_close();
  ui_open_artist(a);
}

}  // namespace

bool ui_sheet_close() {
  if (!s_backdrop) return false;
  sheet_close();
  return true;
}

void ui_device_sheet() {
  lv_obj_t *sh = sheet_open();
  device_fill(sh);
  sheet_show();
}

void ui_track_menu(int track) {
  if (track < 0 || track >= (int)lib_tracks().size()) return;
  s_menu_track = track;
  const Track &t = lib_tracks()[track];
  lv_obj_t *sh = sheet_open();
  lv_obj_t *hd = ui_box(sh);
  lv_obj_set_size(hd, SCR_W, 62);
  lv_obj_t *cv = ui_cover(hd, 44, 3);
  ui_cover_set(cv, COVER_ALBUM, t.album);
  lv_obj_align(cv, LV_ALIGN_LEFT_MID, 16, 0);
  lv_obj_t *ti = ui_label(hd, &font_b14, C_TEXT, t.title);
  ui_dot(ti);
  lv_obj_set_width(ti, SCR_W - 90);
  lv_obj_align(ti, LV_ALIGN_LEFT_MID, 70, -9);
  lv_obj_t *ar = ui_label(hd, &font_r12, C_SUB, t.artist);
  ui_dot(ar);
  lv_obj_set_width(ar, SCR_W - 90);
  lv_obj_align(ar, LV_ALIGN_LEFT_MID, 70, 10);
  lv_obj_t *div = ui_box(sh);
  lv_obj_set_size(div, SCR_W, 1);
  lv_obj_set_style_bg_color(div, lv_color_hex(0x3E3E3E), 0);
  lv_obj_set_style_bg_opa(div, LV_OPA_COVER, 0);

  bool liked = lib_is_liked(track);
  option_row(sh, liked ? IC_HEART : IC_HEART_O,
             liked ? "Rimuovi da Brani che ti piacciono" : "Aggiungi a Brani che ti piacciono", nullptr, false,
             menu_like_cb, nullptr, liked ? C_GREEN : C_SUB);
  option_row(sh, IC_QUEUE, ui_current_track() < 0 ? "Riproduci" : "Aggiungi in coda", nullptr, false, menu_queue_cb,
             nullptr);
  option_row(sh, IC_DISC, "Vai all'album", nullptr, false, menu_album_cb, nullptr);
  option_row(sh, IC_USER, "Vai all'artista", nullptr, false, menu_artist_cb, nullptr);
  char info[96], d[16];
  fmt_duration(t.dur_ms, d, sizeof d);
  snprintf(info, sizeof info, "FLAC %u bit \xE2\x80\xA2 %u.%u kHz \xE2\x80\xA2 %s", t.bits,
           (unsigned)(t.sample_rate / 1000), (unsigned)(t.sample_rate % 1000 / 100), d);
  lv_obj_t *ir = option_row(sh, IC_SLIDERS, info, nullptr, false, nullptr, nullptr);
  lv_obj_remove_flag(ir, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_text_color(lv_obj_get_child(ir, 1), C_SUB, 0);
  sheet_show();
}
