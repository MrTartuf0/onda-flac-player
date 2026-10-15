// Pagina artista: testata, "Popolari", discografia.
#include "ui_internal.h"
#include "../i18n.h"
#include <stdio.h>
#include <algorithm>
#include "../lib/library.h"
#include "../audio/player.h"

namespace {

class ArtistPage : public Page {
 public:
  int artist;
  PlayContext ctx;
  std::vector<uint16_t> popular;
  std::vector<lv_obj_t *> pop_rows;
  lv_obj_t *play_btn, *shuffle_btn, *topbar, *topbar_title;

  uint64_t ident() const override { return (uint64_t)ctx.type << 32 | ctx.key; }

  explicit ArtistPage(int a) : artist(a) {
    ctx = lib_ctx_artist(a);
    build();
  }

  static void back_cb(lv_event_t *) { ui_pop(); }
  static void play_cb(lv_event_t *e) { ui_toggle_context(((ArtistPage *)lv_event_get_user_data(e))->ctx); }
  static void shuffle_cb(lv_event_t *e) {
    ArtistPage *p = (ArtistPage *)lv_event_get_user_data(e);
    ui_play_context(p->ctx, 0, true);
  }
  static void pop_cb(lv_event_t *e) {
    ArtistPage *p = (ArtistPage *)lv_event_get_user_data(e);
    int i = ui_row_index(lv_event_get_current_target_obj(e));
    // riproduce l'intera discografia partendo dal brano scelto
    std::vector<uint16_t> all;
    lib_context_tracks(p->ctx, all);
    auto it = std::find(all.begin(), all.end(), p->popular[i]);
    ui_play_context(p->ctx, it == all.end() ? 0 : it - all.begin());
  }
  static void pop_more_cb(lv_event_t *e) {
    ArtistPage *p = (ArtistPage *)lv_event_get_user_data(e);
    lv_event_stop_bubbling(e);
    int i = ui_row_index(lv_obj_get_parent(lv_event_get_target_obj(e)));
    ui_track_menu(p->popular[i]);
  }
  static void album_cb(lv_event_t *e) { ui_open_album(ui_row_index(lv_event_get_current_target_obj(e))); }
  static void all_cb(lv_event_t *e) {
    ArtistPage *p = (ArtistPage *)lv_event_get_user_data(e);
    ui_push(page_collection_create(p->ctx));
  }
  static void scroll_cb(lv_event_t *e) {
    ArtistPage *p = (ArtistPage *)lv_event_get_user_data(e);
    int y = lv_obj_get_scroll_y(p->root);
    int opa = y <= 120 ? 0 : (y >= 190 ? 255 : (y - 120) * 255 / 70);
    lv_obj_set_style_bg_opa(p->topbar, opa, 0);
    lv_obj_set_style_text_opa(p->topbar_title, opa, 0);
  }

  void build() {
    const Artist &A = lib_artists()[artist];
    auto &T = lib_tracks();
    lv_obj_t *r = ui_page_scroll(this);
    lv_obj_add_event_cb(r, scroll_cb, LV_EVENT_SCROLL, this);
    lv_color_t col = A.albums.empty() ? lv_color_hex(0x535353) : album_color(A.albums[0]);

    lv_obj_t *bg = ui_box(r);
    lv_obj_set_size(bg, SCR_W, 260);
    lv_obj_set_style_bg_color(bg, col_darken(col, 210), 0);
    lv_obj_set_style_bg_grad_color(bg, C_BG, 0);
    lv_obj_set_style_bg_grad_dir(bg, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(bg, LV_OPA_COVER, 0);

    lv_obj_t *cv = ui_cover(r, 120, LV_RADIUS_CIRCLE);
    ui_cover_set(cv, COVER_ARTIST, artist);
    lv_obj_set_pos(cv, (SCR_W - 120) / 2, 36);

    lv_obj_t *name = ui_label(r, &font_b26, C_TEXT, A.name);
    lv_label_set_long_mode(name, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_width(name, SCR_W - 24);
    lv_obj_set_pos(name, 12, 166);
    lv_obj_update_layout(name);
    int y = 166 + lv_obj_get_height(name) + 2;

    char na[24], nt[24], buf[64];
    unsigned albums = A.albums.size(), tracks = A.tracks.size();
    snprintf(na, sizeof na, albums == 1 ? TR("1 album") : TR("%u album"), albums);
    snprintf(nt, sizeof nt, tracks == 1 ? TR("1 brano") : TR("%u brani"), tracks);
    snprintf(buf, sizeof buf, "%s \xE2\x80\xA2 %s", na, nt);
    lv_obj_t *meta = ui_label(r, &font_r12, C_SUB, buf);
    lv_obj_set_pos(meta, 12, y);
    y += 22;

    shuffle_btn = ui_icon_btn(r, &icons_18, IC_SHUFFLE, C_SUB, shuffle_cb, this);
    lv_obj_set_pos(shuffle_btn, SCR_W - 12 - 48 - 34, y + 15);
    play_btn = ui_play_btn(r, 48, play_cb, this);
    lv_obj_set_pos(play_btn, SCR_W - 12 - 48, y);
    y += 60;

    // Popolari: i brani più ascoltati (o i primi della discografia)
    popular = A.tracks;
    std::stable_sort(popular.begin(), popular.end(),
                     [](uint16_t a, uint16_t b) { return lib_play_count(a) > lib_play_count(b); });
    if (popular.size() > 5) popular.resize(5);
    lv_obj_t *h = ui_label(r, &font_b16, C_TEXT, TR("Popolari"));
    lv_obj_set_pos(h, 12, y);
    y += 30;
    for (size_t i = 0; i < popular.size(); i++) {
      const Track &t = T[popular[i]];
      lv_obj_t *row = ui_row(r, 54, 40, false, true);
      lv_obj_set_y(row, y);
      ui_row_set_index(row, i);
      RowParts p = ui_row_parts(row);
      // numero in classifica a sinistra
      lv_obj_align(p.cover, LV_ALIGN_LEFT_MID, 30, 0);
      lv_obj_t *num = ui_label(row, &font_r14, C_SUB, "");
      lv_label_set_text_fmt(num, "%u", (unsigned)(i + 1));
      lv_obj_align(num, LV_ALIGN_LEFT_MID, 12, 0);
      lv_obj_set_x(p.title, 30 + 40 + 10);
      lv_obj_set_width(p.title, SCR_W - 80 - 36);
      lv_obj_set_x(p.sub, 30 + 40 + 10);
      lv_obj_set_width(p.sub, SCR_W - 80 - 36);
      ui_cover_set(p.cover, COVER_ALBUM, t.album);
      lv_label_set_text(p.title, t.title);
      uint32_t pc = lib_play_count(popular[i]);
      if (pc == 1) lv_label_set_text(p.sub, TR("1 ascolto"));
      else if (pc) lv_label_set_text_fmt(p.sub, TR("%u ascolti"), (unsigned)pc);
      else lv_label_set_text(p.sub, t.artist);
      lv_obj_add_event_cb(row, pop_cb, LV_EVENT_CLICKED, this);
      lv_obj_add_event_cb(p.more, pop_more_cb, LV_EVENT_CLICKED, this);
      pop_rows.push_back(row);
      y += 54;
    }

    y += 12;
    lv_obj_t *dh = ui_label(r, &font_b16, C_TEXT, TR("Discografia"));
    lv_obj_set_pos(dh, 12, y);
    y += 30;
    for (auto ai : A.albums) {
      const Album &al = lib_albums()[ai];
      lv_obj_t *row = ui_row(r, 70, 56, false, false);
      lv_obj_set_y(row, y);
      ui_row_set_index(row, ai);
      RowParts p = ui_row_parts(row);
      ui_cover_set(p.cover, COVER_ALBUM, ai);
      lv_label_set_text(p.title, al.title);
      if (al.year) lv_label_set_text_fmt(p.sub, TR("%u \xE2\x80\xA2 Album"), al.year);
      else lv_label_set_text(p.sub, TR("Album"));
      lv_obj_add_event_cb(row, album_cb, LV_EVENT_CLICKED, nullptr);
      y += 70;
    }

    lv_obj_t *all = ui_box(r);
    lv_obj_set_size(all, LV_SIZE_CONTENT, 32);
    lv_obj_set_style_pad_hor(all, 14, 0);
    lv_obj_set_style_radius(all, 16, 0);
    lv_obj_set_style_border_color(all, lv_color_hex(0x727272), 0);
    lv_obj_set_style_border_width(all, 1, 0);
    lv_obj_add_flag(all, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_border_color(all, C_TEXT, LV_STATE_PRESSED);
    lv_obj_add_event_cb(all, all_cb, LV_EVENT_CLICKED, this);
    lv_obj_t *al = ui_label(all, &font_b12, C_TEXT, TR("Vedi tutti i brani"));
    lv_obj_align(al, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_pos(all, 12, y + 10);
    lv_obj_t *sp = ui_box(r);
    lv_obj_set_size(sp, 1, 1);
    lv_obj_set_pos(sp, 0, y + 50 + BOTTOM_PAD);

    topbar = ui_box(r);
    lv_obj_set_size(topbar, SCR_W, 40);
    lv_obj_add_flag(topbar, LV_OBJ_FLAG_FLOATING);
    lv_obj_set_style_bg_color(topbar, col_darken(col, 150), 0);
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
    lv_obj_center(ui_icon(back, &icons_18, IC_BACK, C_TEXT));
    topbar_title = ui_label(topbar, &font_b14, C_TEXT, A.name);
    lv_obj_align(topbar_title, LV_ALIGN_LEFT_MID, 46, 0);
    lv_obj_set_style_text_opa(topbar_title, 0, 0);
  }

  void refresh(uint32_t) override {
    bool cur = ui_context_is_current(ctx);
    ui_play_btn_set(play_btn, cur && ui_is_playing());
    for (size_t i = 0; i < pop_rows.size(); i++) {
      bool c = ui_current_track() == popular[i];
      lv_obj_set_style_text_color(ui_row_parts(pop_rows[i]).title, c ? C_GREEN : C_TEXT, 0);
    }
  }
};

}  // namespace

Page *page_artist_create(int a) { return new ArtistPage(a); }
