// Schermata "In riproduzione" a tutto schermo + coda.
#include "ui_internal.h"
#include <stdio.h>
#include <string.h>
#include "../lib/library.h"
#include "../audio/player.h"
#include "../audio/audio_out.h"

namespace {

constexpr int CV = 140;

lv_obj_t *np, *np_ctx_type, *np_ctx_name, *np_cover, *np_title, *np_artist, *np_heart, *np_slider,
    *np_t_pos, *np_t_dur, *np_play, *np_shuffle, *np_repeat, *np_repeat_one, *np_dev_icon, *np_dev_lbl,
    *np_fmt;
bool np_open = false;
bool np_dragging = false;
int np_track = -2;

lv_obj_t *q_view = nullptr;
VList q_list;
std::vector<uint16_t> q_items;
size_t q_manual = 0;
uint32_t q_ver = 0;
lv_obj_t *q_now_row, *q_head_next, *q_head_ctx;

void anim_y_cb(void *o, int32_t v) { lv_obj_set_y((lv_obj_t *)o, v); }
void anim_hide_cb(lv_anim_t *a) { lv_obj_add_flag((lv_obj_t *)a->var, LV_OBJ_FLAG_HIDDEN); }

void slide(lv_obj_t *o, bool in) {
  lv_anim_delete(o, anim_y_cb);
  if (in) lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, o);
  lv_anim_set_exec_cb(&a, anim_y_cb);
  lv_anim_set_values(&a, in ? SCR_H : lv_obj_get_y(o), in ? 0 : SCR_H);
  lv_anim_set_duration(&a, in ? 260 : 220);
  lv_anim_set_path_cb(&a, in ? lv_anim_path_ease_out : lv_anim_path_ease_in);
  if (!in) lv_anim_set_ready_cb(&a, anim_hide_cb);
  lv_anim_start(&a);
}

// --------------------------------------------------------------- eventi ----
void close_cb(lv_event_t *) { ui_nowplaying_close(); }
void play_cb(lv_event_t *) { player_toggle(); }
void next_cb(lv_event_t *) { player_next(); }
void prev_cb(lv_event_t *) { player_prev(); }
void shuffle_cb(lv_event_t *) { player_set_shuffle(!player_status().shuffle); }
void repeat_cb(lv_event_t *) {
  RepeatMode r = player_status().repeat;
  player_set_repeat((RepeatMode)((r + 1) % 3));
}
void dev_cb(lv_event_t *) { ui_device_sheet(); }
void queue_cb(lv_event_t *) { ui_queue_open(); }
void heart_cb(lv_event_t *) {
  int t = ui_current_track();
  if (t < 0) return;
  bool on = !lib_is_liked(t);
  lib_set_liked(t, on);
  ui_toast(on ? "Aggiunto a Brani che ti piacciono" : "Rimosso da Brani che ti piacciono");
  ui_nowplaying_refresh(RF_LIKED);
}
void title_cb(lv_event_t *) {
  int t = ui_current_track();
  if (t >= 0) ui_open_album(lib_tracks()[t].album);
}
void artist_cb(lv_event_t *) {
  int t = ui_current_track();
  if (t >= 0) ui_open_artist(lib_albums()[lib_tracks()[t].album].artist_idx);
}
void more_cb(lv_event_t *) {
  int t = ui_current_track();
  if (t >= 0) ui_track_menu(t);
}
void gesture_cb(lv_event_t *) {
  lv_dir_t d = lv_indev_get_gesture_dir(lv_indev_active());
  if (d == LV_DIR_BOTTOM) ui_nowplaying_close();
  else if (d == LV_DIR_LEFT) player_next();
  else if (d == LV_DIR_RIGHT) player_prev();
}
void slider_cb(lv_event_t *e) {
  lv_event_code_t c = lv_event_get_code(e);
  if (c != LV_EVENT_PRESSED && c != LV_EVENT_VALUE_CHANGED && c != LV_EVENT_RELEASED && c != LV_EVENT_PRESS_LOST)
    return;   // il cursore riceve molti altri eventi (anche durante la creazione)
  if (c == LV_EVENT_PRESSED) {
    np_dragging = true;
    return;
  }
  if (!np_dragging) return;
  PlayerStatus s = player_status();
  if (c == LV_EVENT_VALUE_CHANGED) {
    char b[16];
    fmt_duration((uint64_t)lv_slider_get_value(np_slider) * s.dur_ms / 1000, b, sizeof b);
    lv_label_set_text(np_t_pos, b);
  } else {
    np_dragging = false;
    player_seek_ms((uint64_t)lv_slider_get_value(np_slider) * s.dur_ms / 1000);
  }
}

lv_obj_t *ctrl(lv_obj_t *p, const lv_font_t *f, const char *ic, lv_event_cb_t cb, int x, int y) {
  lv_obj_t *b = ui_icon_btn(p, f, ic, C_TEXT, cb);
  lv_obj_align(b, LV_ALIGN_TOP_MID, x, y);
  return b;
}

// ---------------------------------------------------------------- coda ----
void q_close_cb(lv_event_t *) { slide(q_view, false); }
void q_row_cb(lv_event_t *e) {
  int i = ui_row_index(lv_event_get_current_target_obj(e));
  if (i >= 0 && i < (int)q_items.size()) player_jump(i);
}

void q_bind(lv_obj_t *row, int i) {
  const Track &t = lib_tracks()[q_items[i]];
  RowParts p = ui_row_parts(row);
  ui_cover_set(p.cover, COVER_ALBUM, t.album);
  lv_label_set_text(p.title, t.title);
  lv_label_set_text(p.sub, t.artist);
}

void q_rebuild() {
  if (!q_view) return;
  PlayerStatus s = player_status();
  player_upcoming(q_items, 200, &q_manual);
  q_ver = s.queue_version;
  // brano attuale
  RowParts p = ui_row_parts(q_now_row);
  if (s.track >= 0) {
    const Track &t = lib_tracks()[s.track];
    ui_cover_set(p.cover, COVER_ALBUM, t.album);
    lv_label_set_text(p.title, t.title);
    lv_label_set_text(p.sub, t.artist);
  }
  PlayContext c = player_context();
  lv_label_set_text_fmt(q_head_ctx, "Successivi da: %s", c.name);
  // la coda manuale (se presente) viene prima: la mostriamo con la sua intestazione
  if (q_manual) {
    lv_obj_remove_flag(q_head_next, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(q_head_ctx, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text_fmt(q_head_next, "Prossimi in coda (%u), poi da %s", (unsigned)q_manual, c.name);
  } else {
    lv_obj_add_flag(q_head_next, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(q_head_ctx, LV_OBJ_FLAG_HIDDEN);
  }
  q_list.set_count(q_items.size());
}

void build_queue(lv_obj_t *scr) {
  q_view = ui_box(scr);
  lv_obj_set_size(q_view, SCR_W, SCR_H);
  lv_obj_set_style_bg_color(q_view, C_BG, 0);
  lv_obj_set_style_bg_opa(q_view, LV_OPA_COVER, 0);
  lv_obj_t *top = ui_box(q_view);
  lv_obj_set_size(top, SCR_W, 44);
  lv_obj_t *x = ui_icon_btn(top, &icons_18, IC_DOWN, C_TEXT, q_close_cb);
  lv_obj_align(x, LV_ALIGN_LEFT_MID, 14, 0);
  lv_obj_t *t = ui_label(top, &font_b16, C_TEXT, "Coda");
  lv_obj_center(t);

  lv_obj_t *sc = ui_box(q_view);
  lv_obj_set_size(sc, SCR_W, SCR_H - 44);
  lv_obj_set_y(sc, 44);
  lv_obj_add_flag(sc, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(sc, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(sc, LV_SCROLLBAR_MODE_OFF);
  lv_obj_t *h1 = ui_label(sc, &font_b14, C_TEXT, "In riproduzione");
  lv_obj_set_pos(h1, 12, 4);
  q_now_row = ui_row(sc, 56, 44, false, false);
  lv_obj_set_y(q_now_row, 26);
  lv_obj_set_style_text_color(ui_row_parts(q_now_row).title, C_GREEN, 0);
  q_head_next = ui_label(sc, &font_b14, C_TEXT, "");
  ui_dot(q_head_next);
  lv_obj_set_width(q_head_next, SCR_W - 24);
  lv_obj_set_pos(q_head_next, 12, 92);
  q_head_ctx = ui_label(sc, &font_b14, C_TEXT, "");
  ui_dot(q_head_ctx);
  lv_obj_set_width(q_head_ctx, SCR_W - 24);
  lv_obj_set_pos(q_head_ctx, 12, 92);
  q_list.create = [](lv_obj_t *parent) {
    lv_obj_t *r = ui_row(parent, 56, 44, false, false);
    lv_obj_add_event_cb(r, q_row_cb, LV_EVENT_CLICKED, nullptr);
    return r;
  };
  q_list.bind = q_bind;
  q_list.init(sc, 118, 56);
  lv_obj_add_flag(q_view, LV_OBJ_FLAG_HIDDEN);
}

}  // namespace

// ================================================================= API ==

void ui_nowplaying_build(lv_obj_t *scr) {
  np = ui_box(scr);
  lv_obj_set_size(np, SCR_W, SCR_H);
  lv_obj_set_style_bg_color(np, lv_color_hex(0x404040), 0);
  lv_obj_set_style_bg_grad_color(np, lv_color_hex(0x101010), 0);
  lv_obj_set_style_bg_grad_dir(np, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_opa(np, LV_OPA_COVER, 0);
  lv_obj_add_flag(np, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(np, gesture_cb, LV_EVENT_GESTURE, nullptr);

  lv_obj_t *dn = ui_icon_btn(np, &icons_18, IC_DOWN, C_TEXT, close_cb);
  lv_obj_set_pos(dn, 14, 10);
  np_ctx_type = ui_label(np, &font_b10, C_TEXT, "");
  lv_obj_set_style_text_letter_space(np_ctx_type, 1, 0);
  lv_obj_align(np_ctx_type, LV_ALIGN_TOP_MID, 0, 5);
  np_ctx_name = ui_label(np, &font_b12, C_TEXT, "");
  ui_dot(np_ctx_name);
  lv_obj_set_width(np_ctx_name, SCR_W - 96);
  lv_obj_set_style_text_align(np_ctx_name, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(np_ctx_name, LV_ALIGN_TOP_MID, 0, 18);
  lv_obj_t *more = ui_icon_btn(np, &icons_18, IC_MORE, C_TEXT, more_cb);
  lv_obj_align(more, LV_ALIGN_TOP_RIGHT, -14, 10);

  np_cover = ui_cover(np, CV, 4);
  lv_obj_align(np_cover, LV_ALIGN_TOP_MID, 0, 40);
  lv_obj_add_flag(np_cover, LV_OBJ_FLAG_CLICKABLE);

  np_title = ui_label(np, &font_b16, C_TEXT, "");
  lv_label_set_long_mode(np_title, LV_LABEL_LONG_MODE_SCROLL_CIRCULAR);
  lv_obj_set_width(np_title, SCR_W - 24 - 34);
  lv_obj_set_pos(np_title, 12, 188);
  lv_obj_add_flag(np_title, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(np_title, title_cb, LV_EVENT_CLICKED, nullptr);
  np_artist = ui_label(np, &font_r12, C_SUB, "");
  ui_dot(np_artist);
  lv_obj_set_width(np_artist, SCR_W - 24 - 34);
  lv_obj_set_pos(np_artist, 12, 209);
  lv_obj_add_flag(np_artist, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(np_artist, artist_cb, LV_EVENT_CLICKED, nullptr);
  np_heart = ui_icon_btn(np, &icons_18, IC_HEART_O, C_TEXT, heart_cb);
  lv_obj_set_pos(np_heart, SCR_W - 12 - 20, 194);

  np_slider = lv_slider_create(np);
  lv_obj_set_size(np_slider, SCR_W - 28, 4);
  lv_obj_set_pos(np_slider, 14, 234);
  lv_slider_set_range(np_slider, 0, 1000);
  lv_obj_set_ext_click_area(np_slider, 12);
  lv_obj_set_style_bg_color(np_slider, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(np_slider, LV_OPA_30, LV_PART_MAIN);
  lv_obj_set_style_bg_color(np_slider, C_TEXT, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(np_slider, C_TEXT, LV_PART_KNOB);
  lv_obj_set_style_pad_all(np_slider, 4, LV_PART_KNOB);
  lv_obj_set_style_bg_color(np_slider, C_GREEN, LV_PART_INDICATOR | LV_STATE_PRESSED);
  lv_obj_set_style_anim_duration(np_slider, 0, 0);
  lv_obj_remove_flag(np_slider, LV_OBJ_FLAG_GESTURE_BUBBLE);
  lv_obj_add_event_cb(np_slider, slider_cb, LV_EVENT_ALL, nullptr);
  np_t_pos = ui_label(np, &font_r10, C_SUB, "0:00");
  lv_obj_set_pos(np_t_pos, 14, 243);
  np_t_dur = ui_label(np, &font_r10, C_SUB, "0:00");
  lv_obj_align(np_t_dur, LV_ALIGN_TOP_RIGHT, -14, 243);
  np_fmt = ui_label(np, &font_r10, C_DIM, "");
  lv_obj_align(np_fmt, LV_ALIGN_TOP_MID, 0, 243);

  np_shuffle = ctrl(np, &icons_18, IC_SHUFFLE, shuffle_cb, -96, 266);
  ctrl(np, &icons_22, IC_PREV, prev_cb, -50, 264);
  np_play = ui_box(np);
  lv_obj_set_size(np_play, 46, 46);
  lv_obj_set_style_radius(np_play, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(np_play, C_TEXT, 0);
  lv_obj_set_style_bg_opa(np_play, LV_OPA_COVER, 0);
  lv_obj_align(np_play, LV_ALIGN_TOP_MID, 0, 252);
  lv_obj_add_flag(np_play, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_transform_scale(np_play, 235, LV_STATE_PRESSED);
  lv_obj_set_style_transform_pivot_x(np_play, 23, 0);
  lv_obj_set_style_transform_pivot_y(np_play, 23, 0);
  lv_obj_add_event_cb(np_play, play_cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *pi = ui_icon(np_play, &icons_22, IC_PLAY, C_BLACK);
  lv_obj_center(pi);
  ctrl(np, &icons_22, IC_NEXT, next_cb, 50, 264);
  np_repeat = ctrl(np, &icons_18, IC_REPEAT, repeat_cb, 96, 266);
  np_repeat_one = ui_label(np, &font_b10, C_GREEN, "1");
  lv_obj_align_to(np_repeat_one, np_repeat, LV_ALIGN_OUT_RIGHT_TOP, -2, -6);

  // riga dispositivo (in basso) + coda
  lv_obj_t *dev = ui_box(np);
  lv_obj_set_size(dev, 160, 18);
  lv_obj_set_pos(dev, 12, SCR_H - 19);
  lv_obj_add_flag(dev, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_opa(dev, LV_OPA_60, LV_STATE_PRESSED);
  lv_obj_add_event_cb(dev, dev_cb, LV_EVENT_CLICKED, nullptr);
  np_dev_icon = ui_icon(dev, &icons_14, IC_SPEAKER, C_SUB);
  lv_obj_align(np_dev_icon, LV_ALIGN_LEFT_MID, 2, 0);
  np_dev_lbl = ui_label(dev, &font_b10, C_SUB, "");
  lv_obj_align(np_dev_lbl, LV_ALIGN_LEFT_MID, 22, 0);
  lv_obj_t *qb = ui_icon_btn(np, &icons_14, IC_QUEUE, C_SUB, queue_cb);
  lv_obj_align(qb, LV_ALIGN_BOTTOM_RIGHT, -14, -3);

  lv_obj_add_flag(np, LV_OBJ_FLAG_HIDDEN);
  build_queue(scr);
}

bool ui_nowplaying_visible() { return np_open; }

void ui_nowplaying_open() {
  if (ui_current_track() < 0) return;
  np_open = true;
  np_track = -2;
  ui_nowplaying_refresh(RF_ALL);
  lv_obj_move_to_index(np, -1);
  slide(np, true);
}

void ui_nowplaying_close() {
  if (q_view && !lv_obj_has_flag(q_view, LV_OBJ_FLAG_HIDDEN)) slide(q_view, false);
  if (!np_open) return;
  np_open = false;
  slide(np, false);
}

void ui_queue_open() {
  q_rebuild();
  lv_obj_move_to_index(q_view, -1);
  slide(q_view, true);
}

void ui_nowplaying_refresh(uint32_t what) {
  if (q_view && !lv_obj_has_flag(q_view, LV_OBJ_FLAG_HIDDEN) && player_status().queue_version != q_ver) q_rebuild();
  if (!np_open) return;
  PlayerStatus s = player_status();
  auto &T = lib_tracks();
  if (s.track < 0 || s.track >= (int)T.size()) {
    ui_nowplaying_close();
    return;
  }
  const Track &t = T[s.track];
  if (s.track != np_track) {
    np_track = s.track;
    lv_color_t c = album_color(t.album);
    lv_obj_set_style_bg_color(np, col_darken(c, 175), 0);
    lv_obj_set_style_bg_grad_color(np, col_darken(c, 40), 0);
    ui_cover_set(np_cover, COVER_ALBUM, t.album);
    lv_label_set_text(np_title, t.title);
    lv_label_set_text(np_artist, t.artist);
    PlayContext ctx = player_context();
    lv_label_set_text_fmt(np_ctx_type, "IN RIPRODUZIONE DA %s", ctx_type_label(ctx.type));
    lv_label_set_text(np_ctx_name, ctx.type == CTX_ALBUM || ctx.type == CTX_NONE ? lib_albums()[t.album].title : ctx.name);
    char d[16];
    fmt_duration(s.dur_ms, d, sizeof d);
    lv_label_set_text(np_t_dur, d);
    if (s.bits) lv_label_set_text_fmt(np_fmt, "FLAC %u bit \xE2\x80\xA2 %u.%u kHz", s.bits,
                                      (unsigned)(s.sample_rate / 1000), (unsigned)(s.sample_rate % 1000 / 100));
    else lv_label_set_text(np_fmt, "");
  }
  bool liked = lib_is_liked(s.track);
  lv_label_set_text(np_heart, liked ? IC_HEART : IC_HEART_O);
  lv_obj_set_style_text_color(np_heart, liked ? C_GREEN : C_TEXT, 0);
  ui_play_btn_set(np_play, s.playing);
  lv_obj_set_style_translate_x(lv_obj_get_child(np_play, 0), s.playing ? 0 : 2, 0);
  lv_obj_set_style_text_color(np_shuffle, s.shuffle ? C_GREEN : C_TEXT, 0);
  lv_obj_set_style_text_color(np_repeat, s.repeat != REPEAT_OFF ? C_GREEN : C_TEXT, 0);
  if (s.repeat == REPEAT_ONE) lv_obj_remove_flag(np_repeat_one, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(np_repeat_one, LV_OBJ_FLAG_HIDDEN);
  bool hp = audio_out_active() == DEV_HEADPHONES;
  lv_label_set_text(np_dev_icon, hp ? IC_HEADPHONES : IC_SPEAKER);
  lv_obj_set_style_text_color(np_dev_icon, hp ? C_GREEN : C_SUB, 0);
  lv_label_set_text(np_dev_lbl, hp ? "CUFFIE (PCM5102)" : "ALTOPARLANTE INTEGRATO");
  lv_obj_set_style_text_color(np_dev_lbl, hp ? C_GREEN : C_SUB, 0);
  (void)what;
  ui_nowplaying_tick();
}

void ui_nowplaying_tick() {
  if (!np_open || np_dragging) return;
  PlayerStatus s = player_status();
  // diagnostica temporanea: formato + carico di decodifica/SD + buchi audio
  static char last_perf[96] = "";
  const char *perf = player_perf_text();
  if (perf[0] && strcmp(perf, last_perf) != 0 && s.bits) {
    strncpy(last_perf, perf, sizeof last_perf - 1);
    lv_label_set_text(np_fmt, perf);
  }
  char b[16];
  fmt_duration(s.pos_ms, b, sizeof b);
  lv_label_set_text(np_t_pos, b);
  lv_slider_set_value(np_slider, s.dur_ms ? (int)((uint64_t)s.pos_ms * 1000 / s.dur_ms) : 0, LV_ANIM_OFF);
}
