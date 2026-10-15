// Home: scorciatoie recenti a griglia + caroselli orizzontali.
#include "ui_internal.h"
#include <algorithm>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../lib/library.h"

namespace {

struct Item {
  CoverKind kind;
  int index;        // album / artista / genere / playlist
  PlayContext ctx;
};

const char *ctx_title(const PlayContext &c) { return c.name; }

// true se il testo sta in 2 righe senza spezzare parole
bool fits(const char *txt, const lv_font_t *f, int w) {
  char word[64];
  const char *p = txt;
  while (*p) {
    while (*p == ' ') p++;
    size_t n = 0;
    while (p[n] && p[n] != ' ' && n < sizeof word - 1) n++;
    memcpy(word, p, n);
    word[n] = 0;
    lv_point_t sz;
    lv_text_get_size(&sz, word, f, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    if (sz.x > w) return false;
    p += n;
  }
  lv_point_t all;
  lv_text_get_size(&all, txt, f, 0, -1, w, LV_TEXT_FLAG_NONE);
  return all.y <= 2 * lv_font_get_line_height(f);
}

CoverKind kind_for(const PlayContext &c, int *index) {
  *index = -1;
  switch (c.type) {
    case CTX_ALBUM: *index = lib_album_by_key(c.key); return COVER_ALBUM;
    case CTX_ARTIST: *index = lib_artist_by_key(c.key); return COVER_ARTIST;
    case CTX_PLAYLIST: *index = lib_playlist_by_key(c.key); return COVER_PLAYLIST;
    case CTX_GENRE: *index = lib_genre_by_key(c.key); return COVER_GENRE;
    case CTX_LIKED: return COVER_LIKED;
    default: return COVER_ALL;
  }
}

class HomePage : public Page {
 public:
  std::vector<PlayContext> tiles_ctx;
  std::vector<lv_obj_t *> tiles;
  std::vector<PlayContext> card_ctx;   // contesti delle card (indice in user_data)
  lv_obj_t *col = nullptr;

  HomePage() {
    lv_obj_t *r = ui_page_scroll(this);
    col = ui_box(r);
    lv_obj_set_width(col, SCR_W);
    lv_obj_set_height(col, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_bottom(col, BOTTOM_PAD, 0);
    lv_obj_set_style_pad_top(col, 10, 0);   // niente intestazione: si parte subito dalle scorciatoie
    build();
  }

  static void ctx_click_cb(lv_event_t *e) {
    HomePage *self = (HomePage *)lv_event_get_user_data(e);
    int i = (int)(intptr_t)lv_obj_get_user_data(lv_event_get_current_target_obj(e));
    if (i >= 0 && i < (int)self->card_ctx.size()) ui_open_context(self->card_ctx[i]);
  }

  // pressione lunga su "Brani che ti piacciono" / "Tutti i brani": casuale subito
  static void shuffle_long_cb(lv_event_t *e) {
    HomePage *self = (HomePage *)lv_event_get_user_data(e);
    int i = (int)(intptr_t)lv_obj_get_user_data(lv_event_get_current_target_obj(e));
    if (i < 0 || i >= (int)self->card_ctx.size()) return;
    const PlayContext &c = self->card_ctx[i];
    if (c.type == CTX_LIKED && lib_liked().empty()) {
      ui_toast("Ancora nessun brano: tocca il cuore per aggiungerne");
      return;
    }
    ui_play_context(c, -1, true);
    ui_toast("In ordine casuale");
  }

  int add_ctx(const PlayContext &c) {
    card_ctx.push_back(c);
    return card_ctx.size() - 1;
  }

  void tile(lv_obj_t *grid, const PlayContext &c) {
    int idx;
    CoverKind k = kind_for(c, &idx);
    lv_obj_t *t = ui_box(grid);
    lv_obj_set_size(t, (SCR_W - 16 - 8) / 2, 44);
    lv_obj_set_style_radius(t, 4, 0);
    lv_obj_set_style_clip_corner(t, true, 0);
    lv_obj_set_style_bg_color(t, C_TILE, 0);
    lv_obj_set_style_bg_opa(t, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(t, lv_color_hex(0x3E3E3E), LV_STATE_PRESSED);
    lv_obj_add_flag(t, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t *cv = ui_cover(t, 44, 0);
    ui_cover_set(cv, k, idx);
    // testo su due righe: se una parola non ci sta o servono più righe, font più piccolo
    const int avail = (SCR_W - 24) / 2 - 44 - 10;
    const lv_font_t *f = &font_b12;
    int lines = 2;
    if (!fits(ctx_title(c), &font_b12, avail)) {
      if (fits(ctx_title(c), &font_b10, avail)) f = &font_b10;
      else lines = 1;   // parola troppo lunga: una riga con "..."
    }
    lv_obj_t *l = ui_label(t, f, C_TEXT, ctx_title(c));
    ui_dot(l);
    lv_obj_set_size(l, avail, lines * lv_font_get_line_height(f) - (lines - 1));
    lv_obj_align(l, LV_ALIGN_LEFT_MID, 44 + 7, 0);
    lv_obj_set_style_text_line_space(l, -1, 0);
    lv_obj_set_user_data(t, (void *)(intptr_t)add_ctx(c));
    if (c.type == CTX_LIKED || c.type == CTX_ALL) {
      lv_obj_add_event_cb(t, ctx_click_cb, LV_EVENT_SHORT_CLICKED, this);   // non dopo la pressione lunga
      lv_obj_add_event_cb(t, shuffle_long_cb, LV_EVENT_LONG_PRESSED, this);
    } else {
      lv_obj_add_event_cb(t, ctx_click_cb, LV_EVENT_CLICKED, this);
    }
    // indicatore "in riproduzione"
    lv_obj_t *eq = ui_icon(t, &icons_14, IC_BARS, C_GREEN);
    lv_obj_align(eq, LV_ALIGN_RIGHT_MID, -6, 0);
    lv_obj_add_flag(eq, LV_OBJ_FLAG_HIDDEN);
    tiles.push_back(t);
    tiles_ctx.push_back(c);
  }

  void grid() {
    std::vector<PlayContext> list;
    auto add = [&](const PlayContext &c) {
      for (auto &x : list)
        if (x.type == c.type && x.key == c.key) return;
      if (lib_ctx_valid(c)) list.push_back(c);
    };
    // sempre per prime: Brani che ti piacciono e Tutti i brani (tenendole premute
    // partono subito in ordine casuale)
    list.push_back(lib_ctx_liked());
    list.push_back(lib_ctx_all());
    for (auto &c : lib_recents()) add(c);
    // completa con gli album più ascoltati, poi con i primi della libreria
    std::vector<int> al(lib_albums().size());
    for (size_t i = 0; i < al.size(); i++) al[i] = i;
    std::stable_sort(al.begin(), al.end(), [](int a, int b) { return lib_album_plays(a) > lib_album_plays(b); });
    for (int a : al) {
      if (list.size() >= 6) break;
      add(lib_ctx_album(a));
    }
    if (list.size() > 6) list.resize(6);

    lv_obj_t *g = ui_box(col);
    lv_obj_set_size(g, SCR_W, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_hor(g, 8, 0);
    lv_obj_set_style_pad_gap(g, 8, 0);
    lv_obj_set_flex_flow(g, LV_FLEX_FLOW_ROW_WRAP);
    for (auto &c : list) tile(g, c);
  }

  lv_obj_t *section(const char *title) {
    lv_obj_t *t = ui_label(col, &font_b16, C_TEXT, title);
    lv_obj_set_style_pad_left(t, 12, 0);
    lv_obj_set_style_pad_top(t, 18, 0);
    lv_obj_set_style_pad_bottom(t, 10, 0);
    lv_obj_t *row = ui_box(col);
    lv_obj_set_size(row, SCR_W, LV_SIZE_CONTENT);
    lv_obj_add_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(row, LV_DIR_HOR);
    lv_obj_set_scrollbar_mode(row, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_hor(row, 12, 0);
    lv_obj_set_style_pad_gap(row, 12, 0);
    lv_obj_add_flag(row, LV_OBJ_FLAG_SCROLL_MOMENTUM);
    return row;
  }

  void card(lv_obj_t *row, const PlayContext &c, const char *sub, bool round) {
    int idx;
    CoverKind k = kind_for(c, &idx);
    const int W = 100;
    lv_obj_t *cd = ui_box(row);
    lv_obj_set_size(cd, W, LV_SIZE_CONTENT);
    lv_obj_add_flag(cd, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_opa(cd, LV_OPA_70, LV_STATE_PRESSED);
    lv_obj_set_flex_flow(cd, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cd, round ? LV_FLEX_ALIGN_CENTER : LV_FLEX_ALIGN_START,
                          round ? LV_FLEX_ALIGN_CENTER : LV_FLEX_ALIGN_START,
                          round ? LV_FLEX_ALIGN_CENTER : LV_FLEX_ALIGN_START);
    lv_obj_add_flag(cd, LV_OBJ_FLAG_SCROLL_CHAIN_HOR);
    lv_obj_t *cv = ui_cover(cd, W, round ? LV_RADIUS_CIRCLE : 4);
    ui_cover_set(cv, k, idx);
    lv_obj_t *t = ui_label(cd, &font_b12, C_TEXT, c.name);
    lv_obj_set_style_margin_top(t, 6, 0);   // margine, non padding: l'altezza della riga resta intera
    ui_dot(t);
    lv_obj_set_width(t, W);
    if (round) lv_obj_set_style_text_align(t, LV_TEXT_ALIGN_CENTER, 0);
    if (sub) {
      lv_obj_t *s = ui_label(cd, &font_r12, C_SUB, sub);
      ui_dot(s);
      lv_obj_set_width(s, W);
      if (round) lv_obj_set_style_text_align(s, LV_TEXT_ALIGN_CENTER, 0);
    }
    lv_obj_set_user_data(cd, (void *)(intptr_t)add_ctx(c));
    lv_obj_add_event_cb(cd, ctx_click_cb, LV_EVENT_CLICKED, this);
  }

  void empty_state() {
    lv_obj_t *box = ui_box(col);
    lv_obj_set_size(box, SCR_W, 200);
    lv_obj_t *ic = ui_icon(box, &icons_28, IC_SDCARD, C_SUB);
    lv_obj_align(ic, LV_ALIGN_TOP_MID, 0, 30);
    lv_obj_t *l = ui_label(box, &font_b16, C_TEXT, "Nessun brano trovato");
    lv_obj_align(l, LV_ALIGN_TOP_MID, 0, 76);
    lv_obj_t *s = ui_label(box, &font_r12, C_SUB,
                           "Copia i tuoi file .flac sulla scheda SD,\npoi tocca \"Aggiorna libreria\" nelle impostazioni.");
    lv_obj_set_width(s, 210);
    lv_obj_set_style_text_align(s, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s, LV_ALIGN_TOP_MID, 0, 104);
  }

  void build() {
    lv_obj_clean(col);
    tiles.clear();
    tiles_ctx.clear();
    card_ctx.clear();
    auto &albums = lib_albums();
    if (albums.empty()) {
      empty_state();
      return;
    }
    grid();

    // ascoltati di recente: album ordinati per ultimo ascolto
    std::vector<int> played;
    for (size_t a = 0; a < albums.size(); a++)
      if (lib_album_plays(a) > 0) played.push_back(a);
    std::sort(played.begin(), played.end(), [](int a, int b) { return lib_album_plays(a) > lib_album_plays(b); });
    if (!played.empty()) {
      lv_obj_t *row = section("I più ascoltati");
      for (size_t i = 0; i < played.size() && i < 10; i++)
        card(row, lib_ctx_album(played[i]), albums[played[i]].artist, false);
    }

    // "Riscopri": album in ordine casuale
    std::vector<int> rnd(albums.size());
    for (size_t i = 0; i < rnd.size(); i++) rnd[i] = i;
    for (size_t i = rnd.size(); i > 1; i--) std::swap(rnd[i - 1], rnd[rand() % i]);
    {
      lv_obj_t *row = section("Riscopri i tuoi album");
      for (size_t i = 0; i < rnd.size() && i < 10; i++) card(row, lib_ctx_album(rnd[i]), albums[rnd[i]].artist, false);
    }

    auto &artists = lib_artists();
    if (artists.size() > 1) {
      lv_obj_t *row = section("I tuoi artisti");
      std::vector<int> ar(artists.size());
      for (size_t i = 0; i < ar.size(); i++) ar[i] = i;
      for (size_t i = ar.size(); i > 1; i--) std::swap(ar[i - 1], ar[rand() % i]);
      for (size_t i = 0; i < ar.size() && i < 10; i++) card(row, lib_ctx_artist(ar[i]), "Artista", true);
    }

    auto &genres = lib_genres();
    if (!genres.empty()) {
      lv_obj_t *row = section("Generi");
      for (size_t g = 0; g < genres.size() && g < 10; g++) {
        char sub[32];
        snprintf(sub, sizeof sub, "%u brani", (unsigned)genres[g].tracks.size());
        card(row, lib_ctx_genre(g), sub, false);
      }
    }
  }

  void refresh(uint32_t what) override {
    if (what & RF_LIBRARY) build();
    for (size_t i = 0; i < tiles.size(); i++) {
      lv_obj_t *eq = lv_obj_get_child(tiles[i], 2);
      lv_obj_t *lbl = lv_obj_get_child(tiles[i], 1);
      bool cur = ui_context_is_current(tiles_ctx[i]);
      if (cur) lv_obj_remove_flag(eq, LV_OBJ_FLAG_HIDDEN);
      else lv_obj_add_flag(eq, LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_style_text_color(lbl, cur ? C_GREEN : C_TEXT, 0);
    }
  }
};

}  // namespace

Page *page_home_create() { return new HomePage(); }
