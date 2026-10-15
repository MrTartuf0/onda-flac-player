#include "ui_internal.h"
#include "../lib/library.h"
#include "../lib/artwork.h"

lv_color_t col565(uint16_t c) { return lv_color_make((c >> 8) & 0xF8, (c >> 3) & 0xFC, (c << 3) & 0xF8); }

lv_color_t col_darken(lv_color_t c, uint8_t keep) { return lv_color_mix(c, C_BLACK, keep); }

lv_color_t album_color(int album) {
  auto &al = lib_albums();
  if (album < 0 || album >= (int)al.size() || al[album].art_state != 1) return lv_color_hex(0x535353);
  return col565(al[album].color);
}

lv_obj_t *ui_box(lv_obj_t *parent) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  return o;
}

lv_obj_t *ui_label(lv_obj_t *parent, const lv_font_t *f, lv_color_t c, const char *txt) {
  lv_obj_t *l = lv_label_create(parent);
  lv_obj_set_style_text_font(l, f, 0);
  lv_obj_set_style_text_color(l, c, 0);
  lv_label_set_text(l, txt);
  return l;
}

lv_obj_t *ui_icon(lv_obj_t *parent, const lv_font_t *f, const char *ic, lv_color_t c) {
  return ui_label(parent, f, c, ic);
}

void ui_dot(lv_obj_t *l) {
  lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_DOTS);
  const lv_font_t *f = lv_obj_get_style_text_font(l, LV_PART_MAIN);
  // una riga: l'altezza deve contare anche il padding, altrimenti il testo viene tagliato
  lv_obj_set_height(l, lv_font_get_line_height(f) + lv_obj_get_style_pad_top(l, LV_PART_MAIN) +
                           lv_obj_get_style_pad_bottom(l, LV_PART_MAIN));
}

void ui_pressable(lv_obj_t *o) {
  lv_obj_add_flag(o, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_color(o, C_PRESS, LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, LV_STATE_PRESSED);
}

lv_obj_t *ui_icon_btn(lv_obj_t *parent, const lv_font_t *f, const char *ic, lv_color_t c, lv_event_cb_t cb,
                      void *ud) {
  lv_obj_t *l = ui_icon(parent, f, ic, c);
  lv_obj_add_flag(l, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_ext_click_area(l, 10);
  lv_obj_set_style_opa(l, LV_OPA_60, LV_STATE_PRESSED);
  lv_obj_set_style_transform_scale(l, 230, LV_STATE_PRESSED);
  if (cb) lv_obj_add_event_cb(l, cb, LV_EVENT_CLICKED, ud);
  return l;
}

lv_obj_t *ui_play_btn(lv_obj_t *parent, int size, lv_event_cb_t cb, void *ud) {
  lv_obj_t *b = ui_box(parent);
  lv_obj_set_size(b, size, size);
  lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(b, C_GREEN, 0);
  lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
  lv_obj_set_style_transform_scale(b, 240, LV_STATE_PRESSED);
  lv_obj_set_style_transform_pivot_x(b, size / 2, 0);
  lv_obj_set_style_transform_pivot_y(b, size / 2, 0);
  lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_ext_click_area(b, 6);
  lv_obj_t *ic = ui_icon(b, size >= 44 ? &icons_22 : &icons_18, IC_PLAY, C_BLACK);
  lv_obj_center(ic);
  lv_obj_set_style_translate_x(ic, 2, 0);   // il triangolo "play" va otticamente centrato
  if (cb) lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, ud);
  return b;
}

void ui_play_btn_set(lv_obj_t *btn, bool playing) {
  lv_obj_t *ic = lv_obj_get_child(btn, 0);
  lv_label_set_text(ic, playing ? IC_PAUSE : IC_PLAY);
  lv_obj_set_style_translate_x(ic, playing ? 0 : 2, 0);
}

lv_obj_t *ui_chip(lv_obj_t *parent, const char *txt, lv_event_cb_t cb, void *ud) {
  lv_obj_t *c = ui_box(parent);
  lv_obj_set_size(c, LV_SIZE_CONTENT, 28);
  lv_obj_set_style_pad_hor(c, 12, 0);
  lv_obj_set_style_radius(c, 14, 0);
  lv_obj_set_style_bg_color(c, C_TILE, 0);
  lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(c, lv_color_hex(0x3E3E3E), 0);
  lv_obj_set_style_border_width(c, 0, 0);
  lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_t *l = ui_label(c, &font_r12, C_TEXT, txt);
  lv_obj_align(l, LV_ALIGN_LEFT_MID, 0, 0);
  if (cb) lv_obj_add_event_cb(c, cb, LV_EVENT_CLICKED, ud);
  return c;
}

void ui_chip_set(lv_obj_t *chip, bool on) {
  lv_obj_set_style_bg_color(chip, on ? C_GREEN : C_TILE, 0);
  lv_obj_set_style_text_color(lv_obj_get_child(chip, 0), on ? C_BLACK : C_TEXT, 0);
}

// ------------------------------------------------------------- copertine ----
namespace {
struct CoverData {
  int size;
  const lv_image_dsc_t *dsc;
  int8_t kind;
  int index;
};

void cover_del_cb(lv_event_t *e) {
  CoverData *d = (CoverData *)lv_event_get_user_data(e);
  art_release(d->dsc);
  delete d;
}

const lv_color_t kGenreColors[] = {
    lv_color_hex(0xE13300), lv_color_hex(0x1E3264), lv_color_hex(0x8400E7), lv_color_hex(0x27856A),
    lv_color_hex(0xE8115B), lv_color_hex(0x148A08), lv_color_hex(0xBC5900), lv_color_hex(0x503750),
    lv_color_hex(0x0D73EC), lv_color_hex(0x8D67AB), lv_color_hex(0xAF2896), lv_color_hex(0x477D95),
};
}  // namespace

lv_color_t genre_color(int g) { return kGenreColors[(g < 0 ? 0 : g) % 12]; }

lv_obj_t *ui_cover(lv_obj_t *parent, int size, int radius) {
  lv_obj_t *c = ui_box(parent);
  // un oggetto LVGL nasce cliccabile: la copertina "mangerebbe" il tocco della riga,
  // della scheda o del mini player che la contiene (chi la vuole cliccabile lo dice)
  lv_obj_remove_flag(c, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(c, size, size);
  lv_obj_set_style_radius(c, radius, 0);
  lv_obj_set_style_clip_corner(c, radius > 0, 0);
  lv_obj_set_style_bg_color(c, C_ELEV, 0);
  lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
  lv_obj_t *img = lv_image_create(c);
  lv_obj_add_flag(img, LV_OBJ_FLAG_HIDDEN);
  lv_obj_t *ic = ui_icon(c, size >= 100 ? &icons_28 : (size >= 44 ? &icons_18 : &icons_14), IC_MUSIC, C_DIM);
  lv_obj_center(ic);
  CoverData *d = new CoverData{size, nullptr, -1, -2};
  lv_obj_set_user_data(c, d);
  lv_obj_add_event_cb(c, cover_del_cb, LV_EVENT_DELETE, d);
  return c;
}

void ui_cover_set(lv_obj_t *c, CoverKind kind, int index) {
  CoverData *d = (CoverData *)lv_obj_get_user_data(c);
  if (d->kind == kind && d->index == index) return;
  d->kind = kind;
  d->index = index;
  lv_obj_t *img = lv_obj_get_child(c, 0);
  lv_obj_t *ic = lv_obj_get_child(c, 1);
  const lv_image_dsc_t *nd = nullptr;
  int album = -1;
  if (kind == COVER_ALBUM) album = index;
  else if (kind == COVER_ARTIST && index >= 0 && index < (int)lib_artists().size() && !lib_artists()[index].albums.empty())
    album = lib_artists()[index].albums[0];
  if (album >= 0) nd = art_acquire(album, d->size);
  art_release(d->dsc);
  d->dsc = nd;

  lv_obj_set_style_bg_grad_dir(c, LV_GRAD_DIR_NONE, 0);
  lv_obj_set_style_bg_color(c, C_ELEV, 0);
  const lv_font_t *f = d->size >= 100 ? &icons_28 : (d->size >= 44 ? &icons_18 : &icons_14);
  lv_obj_set_style_text_font(ic, f, 0);
  lv_obj_set_style_text_color(ic, C_DIM, 0);
  if (nd) {
    lv_image_set_src(img, nd);
    lv_obj_remove_flag(img, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ic, LV_OBJ_FLAG_HIDDEN);
    return;
  }
  lv_obj_add_flag(img, LV_OBJ_FLAG_HIDDEN);
  lv_obj_remove_flag(ic, LV_OBJ_FLAG_HIDDEN);
  switch (kind) {
    case COVER_LIKED:
      lv_obj_set_style_bg_color(c, lv_color_hex(0x4A1BE0), 0);
      lv_obj_set_style_bg_grad_color(c, lv_color_hex(0xB7B0F0), 0);
      lv_obj_set_style_bg_grad_dir(c, LV_GRAD_DIR_VER, 0);
      lv_label_set_text(ic, IC_HEART);
      lv_obj_set_style_text_color(ic, C_TEXT, 0);
      break;
    case COVER_ARTIST: lv_label_set_text(ic, IC_USER); break;
    case COVER_PLAYLIST: lv_label_set_text(ic, IC_MUSIC); break;
    case COVER_GENRE:
      lv_obj_set_style_bg_color(c, genre_color(index), 0);
      lv_label_set_text(ic, IC_MUSIC);
      lv_obj_set_style_text_color(ic, C_TEXT, 0);
      break;
    case COVER_ALL:
      lv_obj_set_style_bg_color(c, lv_color_hex(0x1E3264), 0);
      lv_obj_set_style_bg_grad_color(c, lv_color_hex(0x1ED760), 0);
      lv_obj_set_style_bg_grad_dir(c, LV_GRAD_DIR_VER, 0);
      lv_label_set_text(ic, IC_SHUFFLE);
      lv_obj_set_style_text_color(ic, C_TEXT, 0);
      break;
    default: lv_label_set_text(ic, IC_MUSIC); break;
  }
  lv_obj_center(ic);
}

// --------------------------------------------------------------- righe ----
lv_obj_t *ui_row(lv_obj_t *parent, int h, int cover_size, bool cover_round, bool with_more) {
  lv_obj_t *r = ui_box(parent);
  lv_obj_set_size(r, SCR_W, h);
  ui_pressable(r);
  int x = 12;
  lv_obj_t *cv = nullptr;
  if (cover_size > 0) {
    cv = ui_cover(r, cover_size, cover_round ? LV_RADIUS_CIRCLE : 3);
    lv_obj_align(cv, LV_ALIGN_LEFT_MID, 12, 0);
    x = 12 + cover_size + 10;
  } else {
    // segnaposto per l'ordine dei figli
    cv = ui_box(r);
    lv_obj_add_flag(cv, LV_OBJ_FLAG_HIDDEN);
  }
  int right = with_more ? 36 : 12;
  lv_obj_t *eq = ui_icon(r, &icons_14, IC_BARS, C_GREEN);
  lv_obj_add_flag(eq, LV_OBJ_FLAG_HIDDEN);
  lv_obj_t *t = ui_label(r, &font_r14, C_TEXT, "");
  ui_dot(t);
  lv_obj_set_width(t, SCR_W - x - right);
  lv_obj_set_pos(t, x, h / 2 - 18);
  lv_obj_t *s = ui_label(r, &font_r12, C_SUB, "");
  ui_dot(s);
  lv_obj_set_width(s, SCR_W - x - right);
  lv_obj_set_pos(s, x, h / 2 + 2);
  lv_obj_set_pos(eq, x, h / 2 - 16);
  lv_obj_t *m = nullptr;
  if (with_more) {
    m = ui_icon(r, &icons_14, IC_MORE, C_SUB);
    lv_obj_align(m, LV_ALIGN_RIGHT_MID, -12, 0);
    lv_obj_add_flag(m, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(m, 14);
  } else {
    m = ui_box(r);
    lv_obj_add_flag(m, LV_OBJ_FLAG_HIDDEN);
  }
  return r;
}

RowParts ui_row_parts(lv_obj_t *row) {
  RowParts p;
  p.cover = lv_obj_get_child(row, 0);
  p.eq = lv_obj_get_child(row, 1);
  p.title = lv_obj_get_child(row, 2);
  p.sub = lv_obj_get_child(row, 3);
  p.more = lv_obj_get_child(row, 4);
  return p;
}

void ui_row_set_index(lv_obj_t *row, int idx) { lv_obj_set_user_data(row, (void *)(intptr_t)idx); }
int ui_row_index(lv_obj_t *row) { return (int)(intptr_t)lv_obj_get_user_data(row); }

// ------------------------------------------------------- lista virtuale ----
static void vlist_scroll_cb(lv_event_t *e) {
  VList *v = (VList *)lv_event_get_user_data(e);
  v->refresh();
}

void VList::init(lv_obj_t *s, int start_y, int rh) {
  scroll = s;
  y0 = start_y;
  row_h = rh;
  spacer = ui_box(scroll);
  lv_obj_set_size(spacer, 1, 1);
  lv_obj_add_event_cb(scroll, vlist_scroll_cb, LV_EVENT_SCROLL, this);
  lv_obj_add_event_cb(scroll, vlist_scroll_cb, LV_EVENT_SIZE_CHANGED, this);
}

void VList::set_y0(int y) {
  if (y == y0) return;
  y0 = y;
  refresh(true);
}

void VList::set_count(int n) {
  count = n;
  lv_obj_set_y(spacer, y0 + count * row_h + BOTTOM_PAD);
  refresh(true);
}

void VList::refresh(bool force) {
  if (!scroll) return;
  lv_obj_set_y(spacer, y0 + count * row_h + BOTTOM_PAD);
  int view_h = lv_obj_get_height(scroll);
  if (view_h <= 0) view_h = PAGE_H;
  int need = view_h / row_h + 3;
  while ((int)pool.size() < need && (int)pool.size() < count) {
    lv_obj_t *r = create(scroll);
    lv_obj_add_flag(r, LV_OBJ_FLAG_HIDDEN);
    pool.push_back(r);
    bound.push_back(-1);
  }
  int sy = lv_obj_get_scroll_y(scroll);
  int first = (sy - y0) / row_h - 1;
  if (first < 0) first = 0;
  int last = std::min(count - 1, first + (int)pool.size() - 1);
  // righe ancora nell'intervallo: restano dove sono
  std::vector<bool> used(pool.size(), false);
  std::vector<bool> shown(std::max(0, last - first + 1), false);
  for (size_t i = 0; i < pool.size(); i++) {
    int b = bound[i];
    if (!force && b >= first && b <= last) {
      used[i] = true;
      shown[b - first] = true;
    }
  }
  size_t free_i = 0;
  for (int idx = first; idx <= last; idx++) {
    if (shown[idx - first]) continue;
    while (free_i < pool.size() && used[free_i]) free_i++;
    if (free_i >= pool.size()) break;
    lv_obj_t *r = pool[free_i];
    used[free_i] = true;
    bound[free_i] = idx;
    lv_obj_set_y(r, y0 + idx * row_h);
    lv_obj_remove_flag(r, LV_OBJ_FLAG_HIDDEN);
    ui_row_set_index(r, idx);
    bind(r, idx);
  }
  for (size_t i = 0; i < pool.size(); i++) {
    if (!used[i]) {
      bound[i] = -1;
      lv_obj_add_flag(pool[i], LV_OBJ_FLAG_HIDDEN);
    }
  }
}
