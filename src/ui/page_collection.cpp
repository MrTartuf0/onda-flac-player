// Pagina album / playlist / preferiti / genere: testata con gradiente dal colore
// della copertina, pulsanti shuffle + play e lista brani virtualizzata.
#include "ui_internal.h"
#include <stdio.h>
#include <algorithm>
#include "../lib/library.h"
#include "../audio/player.h"

namespace {

constexpr int COVER = 112;

class CollectionPage : public Page {
 public:
  PlayContext ctx;
  std::vector<uint16_t> tracks;
  VList list;
  int album = -1;          // se è un album
  bool show_covers = true;
  lv_obj_t *topbar, *topbar_title, *play_btn, *shuffle_btn, *heart = nullptr;
  lv_color_t color;
  int header_h = 0;

  explicit CollectionPage(const PlayContext &c) : ctx(c) {
    lib_context_tracks(ctx, tracks);
    if (ctx.type == CTX_ALBUM) album = lib_album_by_key(ctx.key);
    show_covers = ctx.type != CTX_ALBUM;
    color = lv_color_hex(0x535353);
    if (album >= 0) color = album_color(album);
    else if (ctx.type == CTX_LIKED) color = lv_color_hex(0x5038A0);
    else if (ctx.type == CTX_GENRE) color = genre_color(lib_genre_by_key(ctx.key));
    else if (ctx.type == CTX_PLAYLIST && !tracks.empty()) color = album_color(lib_tracks()[tracks[0]].album);
    else if (ctx.type == CTX_ALL) color = lv_color_hex(0x1E3264);
    build();
  }

  static void back_cb(lv_event_t *) { ui_pop(); }
  static void play_cb(lv_event_t *e) {
    CollectionPage *p = (CollectionPage *)lv_event_get_user_data(e);
    ui_toggle_context(p->ctx);
  }
  static void shuffle_cb(lv_event_t *e) {
    CollectionPage *p = (CollectionPage *)lv_event_get_user_data(e);
    bool on = !player_status().shuffle;
    player_set_shuffle(on);
    ui_toast(on ? "Riproduzione casuale attivata" : "Riproduzione casuale disattivata");
    lv_obj_set_style_text_color(p->shuffle_btn, on ? C_GREEN : C_SUB, 0);
  }
  static void artist_cb(lv_event_t *e) {
    CollectionPage *p = (CollectionPage *)lv_event_get_user_data(e);
    if (p->album >= 0) ui_open_artist(lib_albums()[p->album].artist_idx);
  }
  static void row_cb(lv_event_t *e) {
    CollectionPage *p = (CollectionPage *)lv_event_get_user_data(e);
    int i = ui_row_index(lv_event_get_current_target(e));
    if (i < 0 || i >= (int)p->tracks.size()) return;
    if (ui_context_is_current(p->ctx) && ui_current_track() == p->tracks[i]) {
      player_toggle();
      return;
    }
    ui_play_context(p->ctx, i);
  }
  static void more_cb(lv_event_t *e) {
    CollectionPage *p = (CollectionPage *)lv_event_get_user_data(e);
    lv_event_stop_bubbling(e);
    int i = ui_row_index(lv_obj_get_parent(lv_event_get_target(e)));
    if (i >= 0 && i < (int)p->tracks.size()) ui_track_menu(p->tracks[i]);
  }
  static void long_cb(lv_event_t *e) {
    CollectionPage *p = (CollectionPage *)lv_event_get_user_data(e);
    int i = ui_row_index(lv_event_get_current_target(e));
    if (i >= 0 && i < (int)p->tracks.size()) ui_track_menu(p->tracks[i]);
  }
  static void scroll_cb(lv_event_t *e) {
    CollectionPage *p = (CollectionPage *)lv_event_get_user_data(e);
    int y = lv_obj_get_scroll_y(p->root);
    int start = COVER, end = COVER + 70;
    int opa = y <= start ? 0 : (y >= end ? 255 : (y - start) * 255 / (end - start));
    lv_obj_set_style_bg_opa(p->topbar, opa, 0);
    lv_obj_set_style_text_opa(p->topbar_title, opa, 0);
  }

  void build() {
    lv_obj_t *r = ui_page_scroll(this);
    lv_obj_add_event_cb(r, scroll_cb, LV_EVENT_SCROLL, this);
    auto &T = lib_tracks();

    // gradiente di sfondo (scorre con il contenuto)
    lv_obj_t *bg = ui_box(r);
    lv_obj_set_size(bg, SCR_W, 300);
    lv_obj_set_style_bg_color(bg, col_darken(color, 200), 0);
    lv_obj_set_style_bg_grad_color(bg, C_BG, 0);
    lv_obj_set_style_bg_grad_dir(bg, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(bg, LV_OPA_COVER, 0);

    int y = 36;
    lv_obj_t *cv = ui_cover(r, COVER, 2);
    lv_obj_set_pos(cv, (SCR_W - COVER) / 2, y);
    int sidx;
    CoverKind k = COVER_ALBUM;
    sidx = album;
    if (ctx.type == CTX_LIKED) k = COVER_LIKED;
    else if (ctx.type == CTX_GENRE) {
      k = COVER_GENRE;
      sidx = lib_genre_by_key(ctx.key);
    } else if (ctx.type == CTX_ALL) k = COVER_ALL;
    else if (ctx.type == CTX_PLAYLIST) {
      k = tracks.empty() ? COVER_PLAYLIST : COVER_ALBUM;
      sidx = tracks.empty() ? -1 : T[tracks[0]].album;
    } else if (ctx.type == CTX_ARTIST) {
      k = COVER_ARTIST;
      sidx = lib_artist_by_key(ctx.key);
      lv_obj_set_style_radius(cv, LV_RADIUS_CIRCLE, 0);
    }
    ui_cover_set(cv, k, sidx);
    y += COVER + 12;

    lv_obj_t *title = ui_label(r, &font_b20, C_TEXT, ctx.name);
    lv_label_set_long_mode(title, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(title, SCR_W - 24);
    lv_obj_set_pos(title, 12, y);
    lv_obj_update_layout(title);
    y += lv_obj_get_height(title) + 4;
    const int ytxt = y;
    const int txt_w = SCR_W - 12 - 48 - 34 - 16;

    char buf[96], d[24];
    uint32_t tot = 0;
    for (auto t : tracks) tot += T[t].dur_ms;
    fmt_long_duration(tot, d, sizeof d);
    if (album >= 0) {
      const Album &a = lib_albums()[album];
      lv_obj_t *ar = ui_box(r);
      lv_obj_set_size(ar, LV_SIZE_CONTENT, 22);
      lv_obj_set_pos(ar, 12, y);
      lv_obj_add_flag(ar, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_set_style_opa(ar, LV_OPA_60, LV_STATE_PRESSED);
      lv_obj_add_event_cb(ar, artist_cb, LV_EVENT_CLICKED, this);
      lv_obj_t *mini = ui_cover(ar, 20, LV_RADIUS_CIRCLE);
      ui_cover_set(mini, COVER_ARTIST, a.artist_idx);
      lv_obj_align(mini, LV_ALIGN_LEFT_MID, 0, 0);
      lv_obj_t *an = ui_label(ar, &font_b12, C_TEXT, a.artist);
      ui_dot(an);
      lv_obj_set_width(an, txt_w - 27);
      lv_obj_align(an, LV_ALIGN_LEFT_MID, 27, 0);
      y += 26;
      if (a.year) snprintf(buf, sizeof buf, "Album \xE2\x80\xA2 %u \xE2\x80\xA2 %s", a.year, d);
      else snprintf(buf, sizeof buf, "Album \xE2\x80\xA2 %s", d);
    } else {
      snprintf(buf, sizeof buf, "%u bran%s \xE2\x80\xA2 %s", (unsigned)tracks.size(), tracks.size() == 1 ? "o" : "i", d);
    }
    lv_obj_t *meta = ui_label(r, &font_r12, C_SUB, buf);
    ui_dot(meta);
    lv_obj_set_width(meta, txt_w);
    lv_obj_set_pos(meta, 12, y);
    y += 20;

    // pulsanti shuffle + play a destra, allineati alle righe di testo
    int by = std::max(ytxt, y - 48);
    shuffle_btn = ui_icon_btn(r, &icons_18, IC_SHUFFLE, player_status().shuffle ? C_GREEN : C_SUB, shuffle_cb, this);
    lv_obj_set_pos(shuffle_btn, SCR_W - 12 - 48 - 32, by + 15);
    play_btn = ui_play_btn(r, 48, play_cb, this);
    lv_obj_set_pos(play_btn, SCR_W - 12 - 48, by);
    y = std::max(y, by + 48) + 8;
    header_h = y;

    if (tracks.empty()) {
      lv_obj_t *e = ui_label(r, &font_r14, C_SUB,
                             ctx.type == CTX_LIKED ? "I brani a cui metti \"Mi piace\" compariranno qui."
                                                   : "Nessun brano.");
      lv_obj_set_width(e, SCR_W - 40);
      lv_obj_set_pos(e, 20, y + 10);
    }

    list.create = [this](lv_obj_t *parent) {
      lv_obj_t *row = ui_row(parent, show_covers ? 56 : 52, show_covers ? 44 : 0, false, true);
      lv_obj_add_event_cb(row, row_cb, LV_EVENT_CLICKED, this);
      lv_obj_add_event_cb(row, long_cb, LV_EVENT_LONG_PRESSED, this);
      lv_obj_add_event_cb(ui_row_parts(row).more, more_cb, LV_EVENT_CLICKED, this);
      return row;
    };
    list.bind = [this](lv_obj_t *row, int i) { bind_row(row, i); };
    list.init(r, y, show_covers ? 56 : 52);
    list.set_count(tracks.size());

    // barra superiore fissa (compare scorrendo)
    topbar = ui_box(r);
    lv_obj_set_size(topbar, SCR_W, 40);
    lv_obj_add_flag(topbar, LV_OBJ_FLAG_FLOATING);
    lv_obj_set_style_bg_color(topbar, col_darken(color, 150), 0);
    lv_obj_set_style_bg_opa(topbar, 0, 0);
    lv_obj_t *back = ui_box(topbar);
    lv_obj_set_size(back, 30, 30);
    lv_obj_set_style_radius(back, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(back, C_BLACK, 0);
    lv_obj_set_style_bg_opa(back, LV_OPA_40, 0);
    lv_obj_align(back, LV_ALIGN_LEFT_MID, 6, 0);
    lv_obj_add_flag(back, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(back, 8);
    lv_obj_add_event_cb(back, back_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *bi = ui_icon(back, &icons_18, IC_BACK, C_TEXT);
    lv_obj_center(bi);
    topbar_title = ui_label(topbar, &font_b14, C_TEXT, ctx.name);
    ui_dot(topbar_title);
    lv_obj_set_width(topbar_title, SCR_W - 100);
    lv_obj_align(topbar_title, LV_ALIGN_LEFT_MID, 46, 0);
    lv_obj_set_style_text_opa(topbar_title, 0, 0);
  }

  void bind_row(lv_obj_t *row, int i) {
    const Track &t = lib_tracks()[tracks[i]];
    RowParts p = ui_row_parts(row);
    bool cur = ui_context_is_current(ctx) && ui_current_track() == tracks[i];
    if (show_covers) ui_cover_set(p.cover, COVER_ALBUM, t.album);
    lv_label_set_text(p.title, t.title);
    lv_obj_set_style_text_color(p.title, cur ? C_GREEN : C_TEXT, 0);
    int tx = show_covers ? 12 + 44 + 10 : 12;
    if (cur) {
      lv_obj_clear_flag(p.eq, LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_x(p.title, tx + 20);
      lv_obj_set_width(p.title, SCR_W - tx - 20 - 36);
    } else {
      lv_obj_add_flag(p.eq, LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_x(p.title, tx);
      lv_obj_set_width(p.title, SCR_W - tx - 36);
    }
    lv_label_set_text(p.sub, t.artist);
  }

  void refresh(uint32_t what) override {
    if (what & RF_LIKED && ctx.type == CTX_LIKED) {
      lib_context_tracks(ctx, tracks);
      list.set_count(tracks.size());
      lv_obj_move_foreground(topbar);
    }
    bool cur = ui_context_is_current(ctx);
    ui_play_btn_set(play_btn, cur && ui_is_playing());
    lv_obj_set_style_text_color(shuffle_btn, player_status().shuffle ? C_GREEN : C_SUB, 0);
    list.rebind_all();
  }
};

}  // namespace

Page *page_collection_create(const PlayContext &ctx) { return new CollectionPage(ctx); }
