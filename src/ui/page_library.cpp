// "La tua libreria": filtri a chip (Playlist / Album / Artisti / Generi),
// ordinamento (recenti / alfabetico), lista virtualizzata.
#include "ui_internal.h"
#include <stdio.h>
#include <algorithm>
#include "../lib/library.h"
#include "../lib/flac_meta.h"

namespace {

enum Filter { F_NONE = -1, F_PLAYLIST = 0, F_ALBUM, F_ARTIST, F_GENRE };

struct Entry {
  CtxType type;
  int index;
};

class LibraryPage : public Page {
 public:
  Filter filter = F_NONE;
  bool alpha = false;
  std::vector<Entry> items;
  VList list;
  lv_obj_t *chips[4];
  lv_obj_t *sort_lbl;

  LibraryPage() { build(); }

  static void settings_cb(lv_event_t *) { ui_push(page_settings_create()); }
  static void search_cb(lv_event_t *) { ui_switch_tab(TAB_SEARCH); }

  static void chip_cb(lv_event_t *e) {
    LibraryPage *p = (LibraryPage *)lv_event_get_user_data(e);
    lv_obj_t *c = lv_event_get_current_target(e);
    for (int i = 0; i < 4; i++)
      if (p->chips[i] == c) p->filter = p->filter == i ? F_NONE : (Filter)i;
    for (int i = 0; i < 4; i++) ui_chip_set(p->chips[i], p->filter == i);
    p->reload();
    lv_obj_scroll_to_y(p->root, 0, LV_ANIM_OFF);
  }

  static void sort_cb(lv_event_t *e) {
    LibraryPage *p = (LibraryPage *)lv_event_get_user_data(e);
    p->alpha = !p->alpha;
    lv_label_set_text(p->sort_lbl, p->alpha ? "Alfabetico" : "Recenti");
    p->reload();
  }

  static void row_cb(lv_event_t *e) {
    LibraryPage *p = (LibraryPage *)lv_event_get_user_data(e);
    int i = ui_row_index(lv_event_get_current_target(e));
    if (i < 0 || i >= (int)p->items.size()) return;
    ui_open_context(p->ctx_of(p->items[i]));
  }

  PlayContext ctx_of(const Entry &e) {
    switch (e.type) {
      case CTX_ALBUM: return lib_ctx_album(e.index);
      case CTX_ARTIST: return lib_ctx_artist(e.index);
      case CTX_PLAYLIST: return lib_ctx_playlist(e.index);
      case CTX_GENRE: return lib_ctx_genre(e.index);
      case CTX_LIKED: return lib_ctx_liked();
      default: return lib_ctx_all();
    }
  }

  const char *name_of(const Entry &e) {
    switch (e.type) {
      case CTX_ALBUM: return lib_albums()[e.index].title;
      case CTX_ARTIST: return lib_artists()[e.index].name;
      case CTX_PLAYLIST: return lib_playlists()[e.index].name;
      case CTX_GENRE: return lib_genres()[e.index].name;
      case CTX_LIKED: return "Brani che ti piacciono";
      default: return "Tutti i brani";
    }
  }

  void reload() {
    items.clear();
    bool all = filter == F_NONE;
    if (all || filter == F_PLAYLIST) {
      items.push_back({CTX_LIKED, 0});
      for (size_t i = 0; i < lib_playlists().size(); i++) items.push_back({CTX_PLAYLIST, (int)i});
    }
    if (all) items.push_back({CTX_ALL, 0});
    if (all || filter == F_ALBUM)
      for (size_t i = 0; i < lib_albums().size(); i++) items.push_back({CTX_ALBUM, (int)i});
    if (all || filter == F_ARTIST)
      for (size_t i = 0; i < lib_artists().size(); i++) items.push_back({CTX_ARTIST, (int)i});
    if (filter == F_GENRE)
      for (size_t i = 0; i < lib_genres().size(); i++) items.push_back({CTX_GENRE, (int)i});

    // "Brani che ti piacciono" resta sempre in cima
    auto pinned = [](const Entry &e) { return e.type == CTX_LIKED; };
    if (alpha) {
      std::stable_sort(items.begin(), items.end(), [&](const Entry &a, const Entry &b) {
        if (pinned(a) != pinned(b)) return pinned(a);
        char fa[96], fb[96];
        lib_fold(name_of(a), fa, sizeof fa);
        lib_fold(name_of(b), fb, sizeof fb);
        return strcmp(fa, fb) < 0;
      });
    } else {
      auto &rec = lib_recents();
      auto rank = [&](const Entry &e) {
        if (pinned(e)) return -1;
        PlayContext c = ctx_of(e);
        for (size_t i = 0; i < rec.size(); i++)
          if (rec[i].type == c.type && rec[i].key == c.key) return (int)i;
        return 1000;
      };
      std::stable_sort(items.begin(), items.end(), [&](const Entry &a, const Entry &b) { return rank(a) < rank(b); });
    }
    list.set_count(items.size());
  }

  void build() {
    lv_obj_t *r = ui_page_scroll(this);

    lv_obj_t *h = ui_box(r);
    lv_obj_set_size(h, SCR_W, 50);
    lv_obj_t *av = ui_box(h);
    lv_obj_set_size(av, 30, 30);
    lv_obj_set_style_radius(av, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(av, C_GREEN, 0);
    lv_obj_set_style_bg_opa(av, LV_OPA_COVER, 0);
    lv_obj_align(av, LV_ALIGN_LEFT_MID, 12, 2);
    lv_obj_center(ui_label(av, &font_b14, C_BLACK, "O"));
    lv_obj_t *t = ui_label(h, &font_b20, C_TEXT, "La tua libreria");
    lv_obj_align(t, LV_ALIGN_LEFT_MID, 52, 2);
    lv_obj_t *s = ui_icon_btn(h, &icons_18, IC_SEARCH, C_TEXT, search_cb);
    lv_obj_align(s, LV_ALIGN_RIGHT_MID, -14, 2);

    lv_obj_t *cr = ui_box(r);
    lv_obj_set_size(cr, SCR_W, 36);
    lv_obj_set_y(cr, 50);
    lv_obj_add_flag(cr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(cr, LV_DIR_HOR);
    lv_obj_set_scrollbar_mode(cr, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_flex_flow(cr, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cr, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(cr, 12, 0);
    lv_obj_set_style_pad_gap(cr, 8, 0);
    static const char *names[4] = {"Playlist", "Album", "Artisti", "Generi"};
    for (int i = 0; i < 4; i++) chips[i] = ui_chip(cr, names[i], chip_cb, this);

    lv_obj_t *so = ui_box(r);
    lv_obj_set_size(so, LV_SIZE_CONTENT, 26);
    lv_obj_set_pos(so, 12, 92);
    lv_obj_add_flag(so, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_opa(so, LV_OPA_60, LV_STATE_PRESSED);
    lv_obj_add_event_cb(so, sort_cb, LV_EVENT_CLICKED, this);
    lv_obj_t *si = ui_icon(so, &icons_14, IC_SORT, C_TEXT);
    lv_obj_align(si, LV_ALIGN_LEFT_MID, 0, 0);
    sort_lbl = ui_label(so, &font_b12, C_TEXT, "Recenti");
    lv_obj_align(sort_lbl, LV_ALIGN_LEFT_MID, 22, 0);

    list.create = [this](lv_obj_t *parent) {
      lv_obj_t *row = ui_row(parent, 62, 50, false, false);
      lv_obj_add_event_cb(row, row_cb, LV_EVENT_CLICKED, this);
      return row;
    };
    list.bind = [this](lv_obj_t *row, int i) { bind_row(row, i); };
    list.init(r, 122, 62);
    reload();
  }

  void bind_row(lv_obj_t *row, int i) {
    const Entry &e = items[i];
    RowParts p = ui_row_parts(row);
    char sub[160];
    PlayContext c = ctx_of(e);
    bool cur = ui_context_is_current(c);
    lv_obj_set_style_radius(p.cover, e.type == CTX_ARTIST ? LV_RADIUS_CIRCLE : 3, 0);
    switch (e.type) {
      case CTX_ALBUM: {
        const Album &a = lib_albums()[e.index];
        ui_cover_set(p.cover, COVER_ALBUM, e.index);
        snprintf(sub, sizeof sub, "Album \xE2\x80\xA2 %s", a.artist);
        break;
      }
      case CTX_ARTIST:
        ui_cover_set(p.cover, COVER_ARTIST, e.index);
        snprintf(sub, sizeof sub, "Artista");
        break;
      case CTX_PLAYLIST: {
        const Playlist &pl = lib_playlists()[e.index];
        ui_cover_set(p.cover, pl.tracks.empty() ? COVER_PLAYLIST : COVER_ALBUM,
                     pl.tracks.empty() ? -1 : lib_tracks()[pl.tracks[0]].album);
        snprintf(sub, sizeof sub, "Playlist \xE2\x80\xA2 %u brani", (unsigned)pl.tracks.size());
        break;
      }
      case CTX_GENRE:
        ui_cover_set(p.cover, COVER_GENRE, e.index);
        snprintf(sub, sizeof sub, "Genere \xE2\x80\xA2 %u brani", (unsigned)lib_genres()[e.index].tracks.size());
        break;
      case CTX_LIKED:
        ui_cover_set(p.cover, COVER_LIKED, 0);
        snprintf(sub, sizeof sub, "Playlist \xE2\x80\xA2 %u bran%s", (unsigned)lib_liked().size(), lib_liked().size() == 1 ? "o" : "i");
        break;
      default:
        ui_cover_set(p.cover, COVER_ALL, 0);
        snprintf(sub, sizeof sub, "%u brani", (unsigned)lib_tracks().size());
        break;
    }
    lv_label_set_text(p.title, name_of(e));
    lv_obj_set_style_text_color(p.title, cur ? C_GREEN : C_TEXT, 0);
    lv_label_set_text(p.sub, sub);
  }

  void refresh(uint32_t what) override {
    if (what & (RF_LIKED | RF_LIBRARY)) reload();
    else list.rebind_all();
  }
};

}  // namespace

Page *page_library_create() { return new LibraryPage(); }
