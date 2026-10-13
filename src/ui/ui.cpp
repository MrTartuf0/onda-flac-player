// Struttura principale: pagine con navigazione a schede (Home / Cerca / Libreria),
// mini player, barra di navigazione, schermata di avvio/scansione, notifiche.
#include "ui.h"
#include "ui_internal.h"
#include <stdio.h>
#include <string.h>
#include "../lib/library.h"
#include "../lib/artwork.h"
#include "../audio/player.h"
#include "../audio/audio_out.h"
#include "../platform.h"

namespace {

RescanFn s_rescan_fn = nullptr;
BrightnessFn s_bright_fn = nullptr;

lv_obj_t *s_scr;
lv_obj_t *s_pages;
lv_obj_t *s_nav;
lv_obj_t *s_nav_items[3];
lv_obj_t *s_mini;
lv_obj_t *s_mini_cover, *s_mini_title, *s_mini_artist, *s_mini_dev, *s_mini_play, *s_mini_bar;
lv_obj_t *s_boot;
lv_obj_t *s_boot_bar, *s_boot_lbl, *s_boot_btn;
lv_obj_t *s_toast;
bool s_main_built = false;
bool s_chrome = true;

std::vector<Page *> s_stack[3];
Tab s_tab = TAB_HOME;

PlayerStatus s_ps;
int s_last_track = -2;
bool s_last_playing = false;
uint32_t s_last_save = 0;
uint32_t s_last_pos_save = 0;
LibState s_last_lib = LIB_IDLE;

// ------------------------------------------------------------- sessione ----
void save_session(bool with_pos) {
  plat_event("salva sessione");
  PlayContext c = player_context();
  plat_kv_set("ctx_t", c.type);
  plat_kv_set("ctx_k", (int32_t)c.key);
  if (s_ps.track >= 0 && s_ps.track < (int)lib_tracks().size())
    plat_kv_set("trk", (int32_t)lib_tracks()[s_ps.track].path_hash);
  if (with_pos) plat_kv_set("pos", s_ps.pos_ms);
  plat_kv_set("shuf", s_ps.shuffle);
  plat_kv_set("rep", s_ps.repeat);
}

void restore_session() {
  plat_mark(0, "interfaccia: ripristino sessione");
  PlayContext c;
  c.type = (CtxType)plat_kv_get("ctx_t", CTX_NONE);
  c.key = (uint32_t)plat_kv_get("ctx_k", 0);
  if (c.type == CTX_NONE) return;
  int i = -1;
  switch (c.type) {
    case CTX_ALBUM: if ((i = lib_album_by_key(c.key)) >= 0) c = lib_ctx_album(i); break;
    case CTX_ARTIST: if ((i = lib_artist_by_key(c.key)) >= 0) c = lib_ctx_artist(i); break;
    case CTX_PLAYLIST: if ((i = lib_playlist_by_key(c.key)) >= 0) c = lib_ctx_playlist(i); break;
    case CTX_GENRE: if ((i = lib_genre_by_key(c.key)) >= 0) c = lib_ctx_genre(i); break;
    case CTX_LIKED: c = lib_ctx_liked(); break;
    case CTX_ALL: c = lib_ctx_all(); break;
    default: return;
  }
  if (!lib_ctx_valid(c)) return;
  std::vector<uint16_t> tr;
  lib_context_tracks(c, tr);
  if (tr.empty()) return;
  uint32_t th = (uint32_t)plat_kv_get("trk", 0);
  int start = 0;
  for (size_t k = 0; k < tr.size(); k++)
    if (lib_tracks()[tr[k]].path_hash == th) start = k;
  player_set_shuffle(plat_kv_get("shuf", 0));
  player_set_repeat((RepeatMode)plat_kv_get("rep", 0));
  uint32_t pos = (uint32_t)plat_kv_get("pos", 0);
  if (lib_tracks()[tr[start]].path_hash != th) pos = 0;
  player_play_list(tr, start, c, true, pos);
}

// ----------------------------------------------------------- navigazione ----
Page *top_page() { return s_stack[s_tab].empty() ? nullptr : s_stack[s_tab].back(); }

void show_only_top() {
  for (int t = 0; t < 3; t++)
    for (auto p : s_stack[t]) lv_obj_add_flag(p->root, LV_OBJ_FLAG_HIDDEN);
  if (Page *p = top_page()) {
    lv_obj_remove_flag(p->root, LV_OBJ_FLAG_HIDDEN);
    p->refresh(RF_ALL);
    p->on_show();
  }
}

void update_nav() {
  for (int i = 0; i < 3; i++) {
    lv_color_t c = i == s_tab ? C_TEXT : C_SUB;
    lv_obj_set_style_text_color(lv_obj_get_child(s_nav_items[i], 0), c, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(s_nav_items[i], 1), c, 0);
  }
}

void nav_cb(lv_event_t *e) {
  Tab t = (Tab)(intptr_t)lv_event_get_user_data(e);
  if (ui_nowplaying_visible()) ui_nowplaying_close();
  if (t == s_tab) {
    // seconda pressione: torna alla radice della scheda
    while (s_stack[t].size() > 1) {
      delete s_stack[t].back();
      s_stack[t].pop_back();
    }
    if (Page *p = top_page()) lv_obj_scroll_to_y(p->root, 0, LV_ANIM_ON);
    show_only_top();
    return;
  }
  ui_switch_tab(t);
}

// ----------------------------------------------------------- mini player ----
void mini_click_cb(lv_event_t *) { ui_nowplaying_open(); }
void mini_play_cb(lv_event_t *e) {
  lv_event_stop_bubbling(e);
  player_toggle();
}
void mini_dev_cb(lv_event_t *e) {
  lv_event_stop_bubbling(e);
  ui_device_sheet();
}
void mini_gesture_cb(lv_event_t *) {
  lv_dir_t d = lv_indev_get_gesture_dir(lv_indev_active());
  if (d == LV_DIR_LEFT) player_next();
  else if (d == LV_DIR_RIGHT) player_prev();
  else if (d == LV_DIR_TOP) ui_nowplaying_open();
}

void build_mini() {
  s_mini = ui_box(s_scr);
  lv_obj_set_size(s_mini, SCR_W - 12, MINI_H);
  lv_obj_set_pos(s_mini, 6, SCR_H - NAV_H - MINI_H - 4);
  lv_obj_set_style_radius(s_mini, 6, 0);
  lv_obj_set_style_bg_opa(s_mini, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(s_mini, lv_color_hex(0x3A3A3A), 0);
  lv_obj_set_style_clip_corner(s_mini, true, 0);
  lv_obj_add_flag(s_mini, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(s_mini, LV_OBJ_FLAG_GESTURE_BUBBLE);
  lv_obj_add_event_cb(s_mini, mini_click_cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_add_event_cb(s_mini, mini_gesture_cb, LV_EVENT_GESTURE, nullptr);

  s_mini_cover = ui_cover(s_mini, 36, 4);
  lv_obj_align(s_mini_cover, LV_ALIGN_LEFT_MID, 7, -1);
  s_mini_title = ui_label(s_mini, &font_b12, C_TEXT, "");
  ui_dot(s_mini_title);
  lv_obj_set_width(s_mini_title, SCR_W - 12 - 52 - 72);
  lv_obj_set_pos(s_mini_title, 52, 8);
  s_mini_artist = ui_label(s_mini, &font_r12, lv_color_hex(0xDDDDDD), "");
  ui_dot(s_mini_artist);
  lv_obj_set_width(s_mini_artist, SCR_W - 12 - 52 - 72);
  lv_obj_set_pos(s_mini_artist, 52, 25);

  s_mini_dev = ui_icon_btn(s_mini, &icons_18, IC_SPEAKER, C_TEXT, mini_dev_cb);
  lv_obj_align(s_mini_dev, LV_ALIGN_RIGHT_MID, -46, -1);
  s_mini_play = ui_icon_btn(s_mini, &icons_22, IC_PLAY, C_TEXT, mini_play_cb);
  lv_obj_align(s_mini_play, LV_ALIGN_RIGHT_MID, -12, -1);

  lv_obj_t *track = ui_box(s_mini);
  lv_obj_set_size(track, SCR_W - 12 - 14, 2);
  lv_obj_align(track, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_set_style_bg_color(track, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_bg_opa(track, LV_OPA_20, 0);
  s_mini_bar = ui_box(track);
  lv_obj_set_size(s_mini_bar, 0, 2);
  lv_obj_set_style_bg_color(s_mini_bar, C_TEXT, 0);
  lv_obj_set_style_bg_opa(s_mini_bar, LV_OPA_COVER, 0);
  lv_obj_add_flag(s_mini, LV_OBJ_FLAG_HIDDEN);
}

void update_mini_track() {
  auto &T = lib_tracks();
  bool has = s_ps.track >= 0 && s_ps.track < (int)T.size();
  if (!has || !s_chrome) {
    lv_obj_add_flag(s_mini, LV_OBJ_FLAG_HIDDEN);
    return;
  }
  lv_obj_remove_flag(s_mini, LV_OBJ_FLAG_HIDDEN);
  const Track &t = T[s_ps.track];
  lv_label_set_text(s_mini_title, t.title);
  lv_label_set_text(s_mini_artist, t.artist);
  ui_cover_set(s_mini_cover, COVER_ALBUM, t.album);
  lv_obj_set_style_bg_color(s_mini, col_darken(album_color(t.album), 120), 0);
}

void update_mini_state() {
  lv_label_set_text(s_mini_play, s_ps.playing ? IC_PAUSE : IC_PLAY);
  bool hp = audio_out_active() == DEV_HEADPHONES;
  lv_label_set_text(s_mini_dev, hp ? IC_HEADPHONES : IC_SPEAKER);
  lv_obj_set_style_text_color(s_mini_dev, hp ? C_GREEN : C_TEXT, 0);
}

void update_mini_progress() {
  int w = SCR_W - 12 - 14;
  int x = s_ps.dur_ms ? (int)((uint64_t)w * s_ps.pos_ms / s_ps.dur_ms) : 0;
  lv_obj_set_width(s_mini_bar, std::min(x, w));
}

// ----------------------------------------------------------- barra nav ----
void build_nav() {
  s_nav = ui_box(s_scr);
  lv_obj_set_size(s_nav, SCR_W, NAV_H);
  lv_obj_set_pos(s_nav, 0, SCR_H - NAV_H);
  lv_obj_set_style_bg_color(s_nav, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(s_nav, LV_OPA_90, 0);
  static const char *icons[3] = {IC_HOME, IC_SEARCH, IC_LIBRARY};
  static const char *names[3] = {"Home", "Cerca", "La tua libreria"};
  for (int i = 0; i < 3; i++) {
    lv_obj_t *it = ui_box(s_nav);
    lv_obj_set_size(it, SCR_W / 3, NAV_H);
    lv_obj_set_pos(it, i * SCR_W / 3, 0);
    lv_obj_add_flag(it, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(it, nav_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    lv_obj_t *ic = ui_icon(it, &icons_18, icons[i], C_SUB);
    lv_obj_align(ic, LV_ALIGN_TOP_MID, 0, 8);
    lv_obj_t *l = ui_label(it, &font_r10, C_SUB, names[i]);
    lv_obj_align(l, LV_ALIGN_TOP_MID, 0, 31);
    lv_obj_set_style_opa(it, LV_OPA_60, LV_STATE_PRESSED);
    s_nav_items[i] = it;
  }
}

// ------------------------------------------------- avvio e scansione ----
void boot_retry_cb(lv_event_t *) {
  if (s_rescan_fn) s_rescan_fn(false);
}

void build_boot() {
  s_boot = ui_box(s_scr);
  lv_obj_set_size(s_boot, SCR_W, SCR_H);
  lv_obj_set_style_bg_color(s_boot, C_BG, 0);
  lv_obj_set_style_bg_opa(s_boot, LV_OPA_COVER, 0);
  lv_obj_t *logo = ui_box(s_boot);
  lv_obj_set_size(logo, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_align(logo, LV_ALIGN_CENTER, 0, -40);
  lv_obj_t *w = ui_label(logo, &font_b26, C_TEXT, APP_NAME);
  lv_obj_t *dot = ui_box(logo);
  lv_obj_set_size(dot, 9, 9);
  lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(dot, C_GREEN, 0);
  lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
  lv_obj_align_to(dot, w, LV_ALIGN_OUT_RIGHT_BOTTOM, 2, -5);
  lv_obj_t *sub = ui_label(s_boot, &font_r12, C_SUB, "lettore FLAC");
  lv_obj_align(sub, LV_ALIGN_CENTER, 0, -12);

  lv_obj_t *track = ui_box(s_boot);
  lv_obj_set_size(track, 160, 4);
  lv_obj_align(track, LV_ALIGN_CENTER, 0, 30);
  lv_obj_set_style_radius(track, 2, 0);
  lv_obj_set_style_bg_color(track, lv_color_hex(0x4D4D4D), 0);
  lv_obj_set_style_bg_opa(track, LV_OPA_COVER, 0);
  s_boot_bar = ui_box(track);
  lv_obj_set_size(s_boot_bar, 0, 4);
  lv_obj_set_style_radius(s_boot_bar, 2, 0);
  lv_obj_set_style_bg_color(s_boot_bar, C_GREEN, 0);
  lv_obj_set_style_bg_opa(s_boot_bar, LV_OPA_COVER, 0);
  s_boot_lbl = ui_label(s_boot, &font_r12, C_SUB, "Avvio...");
  lv_obj_set_width(s_boot_lbl, 200);
  lv_obj_set_style_text_align(s_boot_lbl, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(s_boot_lbl, LV_ALIGN_CENTER, 0, 54);

  s_boot_btn = ui_box(s_boot);
  lv_obj_set_size(s_boot_btn, 120, 40);
  lv_obj_set_style_radius(s_boot_btn, 20, 0);
  lv_obj_set_style_bg_color(s_boot_btn, C_TEXT, 0);
  lv_obj_set_style_bg_opa(s_boot_btn, LV_OPA_COVER, 0);
  lv_obj_align(s_boot_btn, LV_ALIGN_CENTER, 0, 100);
  lv_obj_add_flag(s_boot_btn, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_transform_scale(s_boot_btn, 240, LV_STATE_PRESSED);
  lv_obj_add_event_cb(s_boot_btn, boot_retry_cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *bl = ui_label(s_boot_btn, &font_b14, C_BLACK, "Riprova");
  lv_obj_center(bl);
  lv_obj_add_flag(s_boot_btn, LV_OBJ_FLAG_HIDDEN);
}

void update_boot(const LibProgress &p) {
  static const char *phases[] = {"Ricerca dei file FLAC", "Lettura dei tag", "Preparazione copertine",
                                 "Salvataggio indice"};
  char buf[96];
  lv_obj_add_flag(s_boot_btn, LV_OBJ_FLAG_HIDDEN);
  if (p.state == LIB_NO_SD) {
    lv_label_set_text(s_boot_lbl, "Inserisci una scheda microSD\ncon i tuoi file .flac");
    lv_obj_set_width(s_boot_bar, 0);
    lv_obj_remove_flag(s_boot_btn, LV_OBJ_FLAG_HIDDEN);
    return;
  }
  if (p.total) {
    snprintf(buf, sizeof buf, "%s\n%u / %u", phases[p.phase & 3], (unsigned)p.done, (unsigned)p.total);
    lv_obj_set_width(s_boot_bar, 160 * p.done / p.total);
  } else {
    snprintf(buf, sizeof buf, "%s\n%u trovati", phases[p.phase & 3], (unsigned)p.done);
    lv_obj_set_width(s_boot_bar, (plat_millis() / 8) % 160);
  }
  lv_label_set_text(s_boot_lbl, buf);
}

void destroy_pages() {
  for (int t = 0; t < 3; t++) {
    for (auto p : s_stack[t]) delete p;
    s_stack[t].clear();
  }
}

void build_main() {
  plat_mark(0, "interfaccia: costruzione pagine");
  destroy_pages();
  s_stack[TAB_HOME].push_back(page_home_create());
  s_stack[TAB_SEARCH].push_back(page_search_create());
  s_stack[TAB_LIBRARY].push_back(page_library_create());
  s_main_built = true;
  s_tab = TAB_HOME;
  update_nav();
  show_only_top();
  lv_obj_add_flag(s_boot, LV_OBJ_FLAG_HIDDEN);
}

// -------------------------------------------------------------- toast ----
void toast_hide_cb(lv_anim_t *a) { lv_obj_add_flag((lv_obj_t *)a->var, LV_OBJ_FLAG_HIDDEN); }
void toast_opa_cb(void *o, int32_t v) { lv_obj_set_style_opa((lv_obj_t *)o, v, 0); }

void refresh_visible(uint32_t what) {
  if (Page *p = top_page()) p->refresh(what);
  ui_nowplaying_refresh(what);
}

}  // namespace

// ================================================================= API ==

lv_obj_t *ui_page_root() { return s_pages; }

lv_obj_t *ui_page_scroll(Page *p) {
  lv_obj_t *r = ui_box(s_pages);
  lv_obj_set_size(r, SCR_W, PAGE_H);
  lv_obj_set_style_bg_color(r, C_BG, 0);
  lv_obj_set_style_bg_opa(r, LV_OPA_COVER, 0);
  lv_obj_add_flag(r, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(r, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(r, LV_SCROLLBAR_MODE_OFF);
  lv_obj_add_flag(r, LV_OBJ_FLAG_SCROLL_MOMENTUM);
  p->root = r;
  return r;
}

void ui_push(Page *p) {
  s_stack[s_tab].push_back(p);
  show_only_top();
}

void ui_pop() {
  if (s_stack[s_tab].size() <= 1) return;
  delete s_stack[s_tab].back();
  s_stack[s_tab].pop_back();
  show_only_top();
}

void ui_switch_tab(Tab t) {
  s_tab = t;
  update_nav();
  show_only_top();
}

void ui_set_chrome_visible(bool vis) {
  s_chrome = vis;
  if (vis) lv_obj_remove_flag(s_nav, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(s_nav, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_height(s_pages, vis ? PAGE_H : SCR_H);
  update_mini_track();
}

void ui_toast(const char *msg) {
  lv_label_set_text(lv_obj_get_child(s_toast, 0), msg);
  lv_obj_remove_flag(s_toast, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_to_index(s_toast, -1);
  lv_anim_delete(s_toast, toast_opa_cb);
  lv_obj_set_style_opa(s_toast, LV_OPA_COVER, 0);
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, s_toast);
  lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_TRANSP);
  lv_anim_set_delay(&a, 1800);
  lv_anim_set_duration(&a, 300);
  lv_anim_set_exec_cb(&a, toast_opa_cb);
  lv_anim_set_ready_cb(&a, toast_hide_cb);
  lv_anim_start(&a);
}

bool ui_is_playing() { return s_ps.playing; }
int ui_current_track() { return s_ps.track; }

bool ui_context_is_current(const PlayContext &c) {
  PlayContext cur = player_context();
  return s_ps.track >= 0 && cur.type == c.type && cur.key == c.key;
}

void ui_play_context(const PlayContext &ctx, int start, bool force_shuffle) {
  std::vector<uint16_t> tr;
  lib_context_tracks(ctx, tr);
  if (tr.empty()) {
    ui_toast("Nessun brano da riprodurre");
    return;
  }
  if (force_shuffle) {
    player_set_shuffle(true);
    start = rand() % tr.size();
  }
  if (start < 0 || start >= (int)tr.size()) start = 0;
  player_play_list(tr, start, ctx);
  lib_recent_push(ctx);
}

void ui_toggle_context(const PlayContext &ctx) {
  if (ui_context_is_current(ctx)) {
    player_toggle();
    return;
  }
  bool sh = player_status().shuffle;
  ui_play_context(ctx, 0, sh);
}

void ui_open_album(int a) {
  if (a < 0) return;
  if (ui_nowplaying_visible()) ui_nowplaying_close();
  ui_push(page_collection_create(lib_ctx_album(a)));
}

void ui_open_artist(int a) {
  if (a < 0) return;
  if (ui_nowplaying_visible()) ui_nowplaying_close();
  ui_push(page_artist_create(a));
}

void ui_open_context(const PlayContext &c) {
  int i;
  switch (c.type) {
    case CTX_ARTIST:
      if ((i = lib_artist_by_key(c.key)) >= 0) ui_open_artist(i);
      break;
    case CTX_NONE:
    case CTX_SEARCH: break;
    default:
      if (lib_ctx_valid(c)) ui_push(page_collection_create(c));
  }
}

const char *ctx_type_label(CtxType t) {
  switch (t) {
    case CTX_ALBUM: return "ALBUM";
    case CTX_ARTIST: return "ARTISTA";
    case CTX_PLAYLIST: return "PLAYLIST";
    case CTX_LIKED: return "PLAYLIST";
    case CTX_GENRE: return "GENERE";
    case CTX_ALL: return "LA TUA LIBRERIA";
    case CTX_SEARCH: return "RICERCA";
    default: return "";
  }
}

void ui_set_brightness(uint8_t pct) {
  plat_kv_set("bright", pct);
  if (s_bright_fn) s_bright_fn(pct);
}

void ui_request_rescan() {
  if (!s_rescan_fn) return;
  player_stop();
  s_main_built = false;
  ui_nowplaying_close();
  destroy_pages();
  art_flush_unused();
  lv_obj_remove_flag(s_boot, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_to_index(s_boot, -1);
  s_rescan_fn(true);
}

void ui_init(RescanFn rescan, BrightnessFn bright) {
  plat_mark(0, "interfaccia: init");
  s_rescan_fn = rescan;
  s_bright_fn = bright;
  lv_display_t *disp = lv_display_get_default();
  lv_theme_t *th = lv_theme_default_init(disp, C_GREEN, lv_color_hex(0x535353), true, &font_r14);
  lv_display_set_theme(disp, th);

  s_scr = lv_screen_active();
  lv_obj_remove_style_all(s_scr);
  lv_obj_set_style_bg_color(s_scr, C_BG, 0);
  lv_obj_set_style_bg_opa(s_scr, LV_OPA_COVER, 0);
  lv_obj_remove_flag(s_scr, LV_OBJ_FLAG_SCROLLABLE);

  s_pages = ui_box(s_scr);
  lv_obj_set_size(s_pages, SCR_W, PAGE_H);
  build_mini();
  build_nav();
  ui_nowplaying_build(s_scr);
  build_boot();

  s_toast = ui_box(lv_layer_top());
  lv_obj_set_size(s_toast, SCR_W - 24, LV_SIZE_CONTENT);
  lv_obj_set_style_pad_all(s_toast, 10, 0);
  lv_obj_set_style_radius(s_toast, 6, 0);
  lv_obj_set_style_bg_color(s_toast, C_TEXT, 0);
  lv_obj_set_style_bg_opa(s_toast, LV_OPA_COVER, 0);
  lv_obj_align(s_toast, LV_ALIGN_BOTTOM_MID, 0, -(NAV_H + MINI_H + 12));
  lv_obj_t *tl = ui_label(s_toast, &font_b12, C_BLACK, "");
  lv_obj_set_width(tl, lv_pct(100));
  lv_obj_add_flag(s_toast, LV_OBJ_FLAG_HIDDEN);

  ui_set_brightness(plat_kv_get("bright", 80));
  lv_timer_create([](lv_timer_t *) { ui_tick(); }, 100, nullptr);
}

void ui_tick() {
  // ---- stato libreria ----
  LibProgress lp = lib_progress();
  if (lp.state != LIB_READY) {
    if (s_main_built && lp.state == LIB_SCANNING) {
      s_main_built = false;
      destroy_pages();
      lv_obj_remove_flag(s_boot, LV_OBJ_FLAG_HIDDEN);
      lv_obj_move_to_index(s_boot, -1);
    }
    update_boot(lp);
    s_last_lib = lp.state;
    return;
  }
  if (!s_main_built) {
    build_main();
    if (s_last_track == -2) restore_session();
    s_last_track = -1;
  }
  s_last_lib = lp.state;

  // ---- eventi del player ----
  uint32_t pe = player_take_events();
  s_ps = player_status();
  uint32_t what = 0;
  if (pe & PLAYER_EV_TRACK) {
    if (s_ps.track != s_last_track) {
      s_last_track = s_ps.track;
      if (s_ps.track >= 0 && s_ps.playing) lib_note_played(s_ps.track);
      update_mini_track();
      save_session(false);
    }
    what |= RF_PLAYER;
  }
  if (pe & PLAYER_EV_ERROR) ui_toast("Impossibile riprodurre questo file");
  if ((pe & PLAYER_EV_STATE) || s_ps.playing != s_last_playing) {
    s_last_playing = s_ps.playing;
    what |= RF_PLAYER;
    if (!s_ps.playing) save_session(true);
  }
  if (pe & PLAYER_EV_QUEUE) what |= RF_PLAYER;

  // ---- uscita audio / cuffie ----
  uint32_t ae = audio_out_take_events();
  if ((ae & AOUT_EV_HP_UNPLUGGED) && audio_out_muted()) {
    player_pause();
    ui_toast("Cuffie scollegate: riproduzione in pausa");
  } else if (ae & AOUT_EV_DEVICE_CHANGED) {
    ui_toast(audio_out_active() == DEV_HEADPHONES ? "Riproduzione sulle cuffie" : "Riproduzione sull'altoparlante");
  }
  if (ae) what |= RF_PLAYER;

  if (what) {
    update_mini_state();
    refresh_visible(what);
  }
  update_mini_progress();
  ui_nowplaying_tick();

  // ---- salvataggi periodici ----
  // Durante la riproduzione niente scritture (né SD né flash): si salva in pausa
  // e al cambio brano. Le scritture in flash fermano entrambi i core per qualche
  // ms e sono tra i sospettati dei blocchi.
  uint32_t now = plat_millis();
  if (!s_ps.playing && now - s_last_save > 30000) {
    s_last_save = now;
    plat_event("sd salva libreria");
    lib_save_dirty();
    plat_event("sd ok");
  }
  (void)s_last_pos_save;
}
