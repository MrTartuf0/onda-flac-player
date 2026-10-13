// Cerca: "Sfoglia tutto" (tile colorate per genere) + ricerca con tastiera.
#include "ui_internal.h"
#include <stdio.h>
#include <string.h>
#include "../lib/library.h"
#include "../lib/artwork.h"
#include "../audio/player.h"

namespace {

enum RKind : uint8_t { R_ARTIST, R_ALBUM, R_PLAYLIST, R_TRACK };
struct RItem {
  RKind kind;
  int index;
};

void img_release_cb(lv_event_t *e) { art_release((const lv_img_dsc_t *)lv_event_get_user_data(e)); }

class SearchPage : public Page {
 public:
  lv_obj_t *browse, *view, *ta, *kb, *results, *cancel, *hint;
  VList list;
  SearchResult res;
  std::vector<RItem> items;
  std::vector<PlayContext> tiles;
  lv_timer_t *debounce = nullptr;
  bool kb_visible = false;

  SearchPage() { build(); }
  ~SearchPage() override {
    if (debounce) lv_timer_del(debounce);
    ui_set_chrome_visible(true);
  }

  // ------------------------------------------------------- sfoglia ----
  static void box_cb(lv_event_t *e) { ((SearchPage *)lv_event_get_user_data(e))->enter(); }
  static void tile_cb(lv_event_t *e) {
    SearchPage *p = (SearchPage *)lv_event_get_user_data(e);
    int i = (int)(intptr_t)lv_obj_get_user_data(lv_event_get_current_target(e));
    ui_open_context(p->tiles[i]);
  }

  void tile(lv_obj_t *grid, const PlayContext &c, lv_color_t col, int album) {
    lv_obj_t *t = ui_box(grid);
    lv_obj_set_size(t, (SCR_W - 24 - 8) / 2, 62);
    lv_obj_set_style_radius(t, 6, 0);
    lv_obj_set_style_clip_corner(t, true, 0);
    lv_obj_set_style_bg_color(t, col, 0);
    lv_obj_set_style_bg_opa(t, LV_OPA_COVER, 0);
    lv_obj_add_flag(t, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_transform_zoom(t, 245, LV_STATE_PRESSED);
    lv_obj_set_user_data(t, (void *)(intptr_t)tiles.size());
    tiles.push_back(c);
    lv_obj_add_event_cb(t, tile_cb, LV_EVENT_CLICKED, this);
    if (album >= 0) {
      const lv_img_dsc_t *d = art_acquire(album, 48);
      if (d) {
        lv_obj_t *img = lv_img_create(t);
        lv_img_set_src(img, d);
        lv_img_set_pivot(img, 24, 24);
        lv_img_set_angle(img, 250);
        lv_obj_set_pos(img, (SCR_W - 32) / 2 - 36, 26);
        lv_obj_add_event_cb(img, img_release_cb, LV_EVENT_DELETE, (void *)d);
      }
    }
    lv_obj_t *l = ui_label(t, &font_b14, C_TEXT, c.name);
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(l, (SCR_W - 32) / 2 - 14);
    lv_obj_set_pos(l, 8, 6);
  }

  void build_browse() {
    browse = ui_box(root);
    lv_obj_set_size(browse, SCR_W, lv_pct(100));
    lv_obj_add_flag(browse, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(browse, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(browse, LV_SCROLLBAR_MODE_OFF);
    lv_obj_t *t = ui_label(browse, &font_b20, C_TEXT, "Cerca");
    lv_obj_set_pos(t, 12, 14);

    lv_obj_t *box = ui_box(browse);
    lv_obj_set_size(box, SCR_W - 24, 40);
    lv_obj_set_pos(box, 12, 50);
    lv_obj_set_style_radius(box, 6, 0);
    lv_obj_set_style_bg_color(box, C_TEXT, 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    lv_obj_add_flag(box, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(box, box_cb, LV_EVENT_CLICKED, this);
    lv_obj_t *ic = ui_icon(box, &icons_18, IC_SEARCH, lv_color_hex(0x121212));
    lv_obj_align(ic, LV_ALIGN_LEFT_MID, 10, 0);
    lv_obj_t *ph = ui_label(box, &font_b14, lv_color_hex(0x535353), "Cosa vuoi ascoltare?");
    lv_obj_align(ph, LV_ALIGN_LEFT_MID, 38, 0);

    lv_obj_t *h = ui_label(browse, &font_b16, C_TEXT, "Sfoglia tutto");
    lv_obj_set_pos(h, 12, 106);

    lv_obj_t *g = ui_box(browse);
    lv_obj_set_size(g, SCR_W, LV_SIZE_CONTENT);
    lv_obj_set_pos(g, 0, 136);
    lv_obj_set_style_pad_hor(g, 12, 0);
    lv_obj_set_style_pad_gap(g, 8, 0);
    lv_obj_set_style_pad_bottom(g, BOTTOM_PAD, 0);
    lv_obj_set_flex_flow(g, LV_FLEX_FLOW_ROW_WRAP);
    tiles.clear();
    tile(g, lib_ctx_all(), lv_color_hex(0x27856A), -1);
    tile(g, lib_ctx_liked(), lv_color_hex(0x5038A0), -1);
    auto &G = lib_genres();
    for (size_t i = 0; i < G.size(); i++)
      tile(g, lib_ctx_genre(i), genre_color(i), G[i].tracks.empty() ? -1 : lib_tracks()[G[i].tracks[0]].album);
    for (size_t i = 0; i < lib_playlists().size(); i++) {
      auto &pl = lib_playlists()[i];
      tile(g, lib_ctx_playlist(i), genre_color(i + 5), pl.tracks.empty() ? -1 : lib_tracks()[pl.tracks[0]].album);
    }
  }

  // ------------------------------------------------------- ricerca ----
  static void cancel_cb(lv_event_t *e) { ((SearchPage *)lv_event_get_user_data(e))->leave(); }
  static void ta_cb(lv_event_t *e) {
    SearchPage *p = (SearchPage *)lv_event_get_user_data(e);
    lv_event_code_t c = lv_event_get_code(e);
    if (c == LV_EVENT_VALUE_CHANGED) {
      if (p->debounce) lv_timer_reset(p->debounce);
      else {
        p->debounce = lv_timer_create(debounce_cb, 250, p);
        lv_timer_set_repeat_count(p->debounce, 1);
      }
    } else if (c == LV_EVENT_CLICKED || c == LV_EVENT_FOCUSED) {
      p->show_kb(true);
    } else if (c == LV_EVENT_READY || c == LV_EVENT_CANCEL) {
      p->show_kb(false);
    }
  }
  static void debounce_cb(lv_timer_t *t) {
    SearchPage *p = (SearchPage *)t->user_data;
    p->debounce = nullptr;   // timer a ripetizione singola: viene eliminato da LVGL
    p->run();
  }
  static void results_scroll_begin_cb(lv_event_t *e) {
    SearchPage *p = (SearchPage *)lv_event_get_user_data(e);
    if (p->kb_visible) p->show_kb(false);
  }
  static void row_cb(lv_event_t *e) {
    SearchPage *p = (SearchPage *)lv_event_get_user_data(e);
    int i = ui_row_index(lv_event_get_current_target(e));
    if (i < 0 || i >= (int)p->items.size()) return;
    const RItem &it = p->items[i];
    p->show_kb(false);
    switch (it.kind) {
      case R_ARTIST: ui_open_artist(it.index); break;
      case R_ALBUM: ui_open_album(it.index); break;
      case R_PLAYLIST: ui_open_context(lib_ctx_playlist(it.index)); break;
      case R_TRACK: {
        PlayContext c;
        c.type = CTX_SEARCH;
        c.key = 3;
        snprintf(c.name, sizeof c.name, "\"%s\"", lv_textarea_get_text(p->ta));
        int start = 0;
        for (size_t k = 0; k < p->res.tracks.size(); k++)
          if (p->res.tracks[k] == it.index) start = k;
        player_play_list(p->res.tracks, start, c);
        break;
      }
    }
  }
  static void more_cb(lv_event_t *e) {
    SearchPage *p = (SearchPage *)lv_event_get_user_data(e);
    lv_event_stop_bubbling(e);
    int i = ui_row_index(lv_obj_get_parent(lv_event_get_target(e)));
    if (i >= 0 && i < (int)p->items.size() && p->items[i].kind == R_TRACK) ui_track_menu(p->items[i].index);
  }

  void build_view() {
    view = ui_box(root);
    lv_obj_set_size(view, SCR_W, lv_pct(100));
    lv_obj_set_style_bg_color(view, C_BG, 0);
    lv_obj_set_style_bg_opa(view, LV_OPA_COVER, 0);
    lv_obj_add_flag(view, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *top = ui_box(view);
    lv_obj_set_size(top, SCR_W, 48);
    lv_obj_set_style_bg_color(top, lv_color_hex(0x242424), 0);
    lv_obj_set_style_bg_opa(top, LV_OPA_COVER, 0);

    ta = lv_textarea_create(top);
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_placeholder_text(ta, "Cosa vuoi ascoltare?");
    lv_obj_set_size(ta, SCR_W - 90, 34);
    lv_obj_align(ta, LV_ALIGN_LEFT_MID, 8, 0);
    lv_obj_set_style_text_font(ta, &font_r14, 0);
    lv_obj_set_style_text_color(ta, C_TEXT, 0);
    lv_obj_set_style_bg_color(ta, lv_color_hex(0x3E3E3E), 0);
    lv_obj_set_style_border_width(ta, 0, 0);
    lv_obj_set_style_radius(ta, 4, 0);
    lv_obj_set_style_pad_ver(ta, 7, 0);
    lv_obj_set_style_pad_hor(ta, 10, 0);
    lv_obj_set_style_text_color(ta, C_SUB, LV_PART_TEXTAREA_PLACEHOLDER);
    lv_obj_set_style_bg_color(ta, C_GREEN, LV_PART_CURSOR | LV_STATE_FOCUSED);
    lv_obj_set_style_border_color(ta, C_GREEN, LV_PART_CURSOR | LV_STATE_FOCUSED);
    lv_obj_add_event_cb(ta, ta_cb, LV_EVENT_ALL, this);

    cancel = ui_label(top, &font_r14, C_TEXT, "Annulla");
    lv_obj_add_flag(cancel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(cancel, 10);
    lv_obj_set_style_opa(cancel, LV_OPA_60, LV_STATE_PRESSED);
    lv_obj_align(cancel, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_obj_add_event_cb(cancel, cancel_cb, LV_EVENT_CLICKED, this);

    results = ui_box(view);
    lv_obj_set_size(results, SCR_W, 100);
    lv_obj_set_pos(results, 0, 48);
    lv_obj_add_flag(results, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(results, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(results, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_event_cb(results, results_scroll_begin_cb, LV_EVENT_SCROLL_BEGIN, this);

    hint = ui_label(results, &font_r14, C_SUB, "Cerca brani, album e artisti\nsulla tua scheda SD.");
    lv_obj_set_width(hint, SCR_W - 40);
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(hint, LV_ALIGN_TOP_MID, 0, 30);

    list.create = [this](lv_obj_t *parent) {
      lv_obj_t *row = ui_row(parent, 56, 44, false, true);
      lv_obj_add_event_cb(row, row_cb, LV_EVENT_CLICKED, this);
      lv_obj_add_event_cb(ui_row_parts(row).more, more_cb, LV_EVENT_CLICKED, this);
      return row;
    };
    list.bind = [this](lv_obj_t *row, int i) { bind_row(row, i); };
    list.init(results, 4, 56);

    kb = lv_keyboard_create(view);
    lv_obj_set_size(kb, SCR_W, 140);
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(kb, lv_color_hex(0x1A1A1A), 0);
    lv_obj_set_style_pad_all(kb, 3, 0);
    lv_obj_set_style_pad_gap(kb, 3, 0);
    lv_obj_set_style_bg_color(kb, lv_color_hex(0x3A3A3A), LV_PART_ITEMS);
    lv_obj_set_style_text_color(kb, C_TEXT, LV_PART_ITEMS);
    lv_obj_set_style_text_font(kb, &lv_font_montserrat_14, LV_PART_ITEMS);
    lv_obj_set_style_radius(kb, 4, LV_PART_ITEMS);
    lv_obj_set_style_border_width(kb, 0, LV_PART_ITEMS);
    lv_obj_set_style_shadow_width(kb, 0, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(kb, lv_color_hex(0x5A5A5A), LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(kb, lv_color_hex(0x262626), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_keyboard_set_textarea(kb, ta);
  }

  void bind_row(lv_obj_t *row, int i) {
    const RItem &it = items[i];
    RowParts p = ui_row_parts(row);
    char sub[160];
    lv_obj_set_style_radius(p.cover, it.kind == R_ARTIST ? LV_RADIUS_CIRCLE : 3, 0);
    lv_obj_set_style_text_color(p.title, C_TEXT, 0);
    switch (it.kind) {
      case R_ARTIST:
        ui_cover_set(p.cover, COVER_ARTIST, it.index);
        lv_label_set_text(p.title, lib_artists()[it.index].name);
        lv_label_set_text(p.sub, "Artista");
        lv_obj_add_flag(p.more, LV_OBJ_FLAG_HIDDEN);
        break;
      case R_ALBUM: {
        const Album &a = lib_albums()[it.index];
        ui_cover_set(p.cover, COVER_ALBUM, it.index);
        lv_label_set_text(p.title, a.title);
        snprintf(sub, sizeof sub, "Album \xE2\x80\xA2 %s", a.artist);
        lv_label_set_text(p.sub, sub);
        lv_obj_add_flag(p.more, LV_OBJ_FLAG_HIDDEN);
        break;
      }
      case R_PLAYLIST: {
        const Playlist &pl = lib_playlists()[it.index];
        ui_cover_set(p.cover, pl.tracks.empty() ? COVER_PLAYLIST : COVER_ALBUM,
                     pl.tracks.empty() ? -1 : lib_tracks()[pl.tracks[0]].album);
        lv_label_set_text(p.title, pl.name);
        lv_label_set_text(p.sub, "Playlist");
        lv_obj_add_flag(p.more, LV_OBJ_FLAG_HIDDEN);
        break;
      }
      case R_TRACK: {
        const Track &t = lib_tracks()[it.index];
        ui_cover_set(p.cover, COVER_ALBUM, t.album);
        lv_label_set_text(p.title, t.title);
        snprintf(sub, sizeof sub, "Brano \xE2\x80\xA2 %s", t.artist);
        lv_label_set_text(p.sub, sub);
        lv_obj_clear_flag(p.more, LV_OBJ_FLAG_HIDDEN);
        if (ui_current_track() == it.index) lv_obj_set_style_text_color(p.title, C_GREEN, 0);
        break;
      }
    }
  }

  void run() {
    const char *q = lv_textarea_get_text(ta);
    lib_search(q, res);
    items.clear();
    for (auto a : res.artists) items.push_back({R_ARTIST, a});
    for (auto a : res.albums) items.push_back({R_ALBUM, a});
    for (auto a : res.playlists) items.push_back({R_PLAYLIST, a});
    for (auto t : res.tracks) items.push_back({R_TRACK, t});
    if (q[0] && items.empty()) {
      lv_label_set_text_fmt(hint, "Nessun risultato per\n\"%s\"", q);
      lv_obj_clear_flag(hint, LV_OBJ_FLAG_HIDDEN);
    } else if (!q[0]) {
      lv_label_set_text(hint, "Cerca brani, album e artisti\nsulla tua scheda SD.");
      lv_obj_clear_flag(hint, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(hint, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_scroll_to_y(results, 0, LV_ANIM_OFF);
    list.set_count(items.size());
  }

  void layout() {
    int h = kb_visible ? SCR_H : PAGE_H;
    lv_obj_set_height(root, h);
    lv_obj_set_height(results, (kb_visible ? SCR_H - 140 : PAGE_H) - 48);
    list.refresh(true);
  }

  void show_kb(bool on) {
    if (on == kb_visible && !lv_obj_has_flag(view, LV_OBJ_FLAG_HIDDEN)) return;
    kb_visible = on;
    if (on) {
      lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_state(ta, LV_STATE_FOCUSED);
    } else {
      lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_state(ta, LV_STATE_FOCUSED);
    }
    ui_set_chrome_visible(!on);
    layout();
  }

  void enter() {
    lv_obj_add_flag(browse, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(view, LV_OBJ_FLAG_HIDDEN);
    show_kb(true);
  }

  void leave() {
    lv_textarea_set_text(ta, "");
    items.clear();
    list.set_count(0);
    show_kb(false);
    lv_obj_add_flag(view, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(browse, LV_OBJ_FLAG_HIDDEN);
    run();
  }

  void build() {
    root = ui_box(ui_page_root());
    lv_obj_set_size(root, SCR_W, PAGE_H);
    lv_obj_set_style_bg_color(root, C_BG, 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    build_browse();
    build_view();
    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
  }

  void on_show() override {
    // tornando a questa scheda la tastiera resta chiusa
    if (kb_visible) show_kb(false);
  }

  void refresh(uint32_t what) override {
    if (what & RF_PLAYER) list.rebind_all();
  }
};

}  // namespace

Page *page_search_create() { return new SearchPage(); }
