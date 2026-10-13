#include "artwork.h"
#include "../lvgl/src/misc/cache/instance/lv_image_cache.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <algorithm>
#include <vector>
#include "library.h"
#include "flac_meta.h"
#include "onda_tjpgd.h"
#include "../platform.h"
#include "../config.h"

namespace {

constexpr size_t kBudget = 3 * 1024 * 1024;   // cache copertine in PSRAM
constexpr size_t kWork = 12 * 1024;           // workspace TJpgDec

struct JCtx {
  FILE *f;
  uint32_t left;
  uint16_t *out;
  int ow, oh;
};

size_t jin(JDEC *jd, uint8_t *buf, size_t n) {
  JCtx *c = (JCtx *)jd->device;
  if (n > c->left) n = c->left;
  if (buf) n = fread(buf, 1, n, c->f);
  else if (fseek(c->f, n, SEEK_CUR) != 0) return 0;
  c->left -= n;
  return n;
}

int jout(JDEC *jd, void *bmp, JRECT *r) {
  JCtx *c = (JCtx *)jd->device;
  const uint16_t *src = (const uint16_t *)bmp;
  int w = r->right - r->left + 1;
  for (int y = r->top; y <= r->bottom; y++) {
    if (y >= c->oh) break;
    const uint16_t *row = src + (y - r->top) * w;
    int x1 = std::min<int>(r->right, c->ow - 1);
    if (x1 >= r->left) memcpy(c->out + y * c->ow + r->left, row, (x1 - r->left + 1) * 2);
  }
  return 1;
}

// Decodifica un JPEG (baseline) da file/offset. Scala 1/1..1/8 scelta per avere
// almeno min_side pixel sul lato corto. Ritorna buffer RGB565 da liberare con plat_free.
uint16_t *decode_jpeg(FILE *f, uint32_t off, uint32_t len, int min_side, int *ow, int *oh) {
  if (fseek(f, off, SEEK_SET) != 0) return nullptr;
  void *work = plat_alloc_fast(kWork);
  if (!work) return nullptr;
  JDEC jd;
  memset(&jd, 0, sizeof jd);
  JCtx c{f, len, nullptr, 0, 0};
  uint16_t *out = nullptr;
  if (ojd_prepare(&jd, jin, work, kWork, &c) == JDR_OK) {
    int s = 0;
    int shortest = std::min(jd.width, jd.height);
    while (s < 3 && (shortest >> (s + 1)) >= min_side) s++;
    c.ow = (jd.width + (1 << s) - 1) >> s;
    c.oh = (jd.height + (1 << s) - 1) >> s;
    c.out = (uint16_t *)plat_alloc_big((size_t)c.ow * c.oh * 2);
    if (c.out) {
      memset(c.out, 0, (size_t)c.ow * c.oh * 2);
      if (ojd_decomp(&jd, jout, s) == JDR_OK) {
        out = c.out;
        *ow = c.ow;
        *oh = c.oh;
      } else {
        plat_free(c.out);
      }
    }
  }
  plat_free(work);
  return out;
}

void write_cache(uint32_t key, int size, const uint16_t *px, uint16_t color) {
  char p[96];
  art_cache_path(key, size, p, sizeof p);
  FILE *f = fopen(p, "wb");
  if (!f) return;
  uint16_t hdr[4] = {0x414F, (uint16_t)size, color, 0};
  fwrite(hdr, 2, 4, f);
  fwrite(px, 2, size * size, f);
  fclose(f);
}

uint16_t *read_cache(uint32_t key, int size) {
  char p[96];
  art_cache_path(key, size, p, sizeof p);
  FILE *f = fopen(p, "rb");
  if (!f) return nullptr;
  uint16_t hdr[4];
  uint16_t *px = nullptr;
  if (fread(hdr, 2, 4, f) == 4 && hdr[0] == 0x414F && hdr[1] == size) {
    px = (uint16_t *)plat_alloc_big(size * size * 2);
    if (px && fread(px, 2, size * size, f) != (size_t)(size * size)) {
      plat_free(px);
      px = nullptr;
    }
  }
  fclose(f);
  return px;
}

// ------------------------------------------------------- cache in RAM ----
struct Entry {
  uint32_t key;     // chiave album (stabile anche dopo una nuova scansione)
  int size;
  lv_image_dsc_t dsc;
  uint16_t *px;
  int refs;
  uint32_t last;
};
std::vector<Entry *> g_cache;
size_t g_bytes = 0;
uint32_t g_tick = 0;

void evict() {
  while (g_bytes > kBudget) {
    Entry *victim = nullptr;
    for (auto e : g_cache)
      if (e->refs == 0 && (!victim || e->last < victim->last)) victim = e;
    if (!victim) return;
    lv_image_cache_drop(&victim->dsc);
    g_bytes -= victim->size * victim->size * 2;
    plat_free(victim->px);
    g_cache.erase(std::find(g_cache.begin(), g_cache.end(), victim));
    delete victim;
  }
}

Entry *add_entry(uint32_t key, int size, uint16_t *px) {
  Entry *e = new Entry();
  e->key = key;
  e->size = size;
  e->px = px;
  e->refs = 0;
  e->last = ++g_tick;
  memset(&e->dsc, 0, sizeof e->dsc);
  e->dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
  e->dsc.header.cf = LV_COLOR_FORMAT_RGB565;
  e->dsc.header.w = size;
  e->dsc.header.h = size;
  e->dsc.header.stride = size * 2;
  e->dsc.data_size = size * size * 2;
  e->dsc.data = (const uint8_t *)px;
  g_cache.push_back(e);
  g_bytes += size * size * 2;
  return e;
}

Entry *find_entry(uint32_t key, int size) {
  for (auto e : g_cache)
    if (e->key == key && e->size == size) return e;
  return nullptr;
}

Entry *get_master(int album, int msize) {
  auto &albums = lib_albums();
  if (album < 0 || album >= (int)albums.size()) return nullptr;
  Album &a = const_cast<Album &>(albums[album]);
  Entry *e = find_entry(a.key, msize);
  if (e) return e;
  if (a.art_state == 2) return nullptr;
  uint16_t *px = read_cache(a.key, msize);
  if (!px) {
    // cache cancellata o non ancora generata: la ricreiamo al volo
    uint16_t col;
    if (!art_build(a, &col)) {
      a.art_state = 2;
      return nullptr;
    }
    a.art_state = 1;
    a.color = col;
    px = read_cache(a.key, msize);
    if (!px) return nullptr;
  }
  return add_entry(a.key, msize, px);
}

}  // namespace

void art_cache_path(uint32_t key, int size, char *out, size_t cap) {
  snprintf(out, cap, "%s/art/%08lx_%d.bin", APP_DIR, (unsigned long)key, size);
}

bool art_has_cache(uint32_t key) {
  char p[96];
  struct stat st;
  art_cache_path(key, ART_SMALL, p, sizeof p);
  if (stat(p, &st) != 0) return false;
  art_cache_path(key, ART_LARGE, p, sizeof p);
  return stat(p, &st) == 0;
}

void art_resample(const uint16_t *src, int sw, int sh, uint16_t *dst, int dw, int dh) {
  // media d'area con passo a virgola fissa (16.16)
  for (int y = 0; y < dh; y++) {
    int y0 = y * sh / dh, y1 = std::max(y0 + 1, (y + 1) * sh / dh);
    for (int x = 0; x < dw; x++) {
      int x0 = x * sw / dw, x1 = std::max(x0 + 1, (x + 1) * sw / dw);
      uint32_t r = 0, g = 0, b = 0, n = 0;
      for (int yy = y0; yy < y1; yy++) {
        const uint16_t *row = src + yy * sw;
        for (int xx = x0; xx < x1; xx++) {
          uint16_t p = row[xx];
          r += p >> 11;
          g += (p >> 5) & 0x3F;
          b += p & 0x1F;
          n++;
        }
      }
      dst[y * dw + x] = ((r / n) << 11) | ((g / n) << 5) | (b / n);
    }
  }
}

uint16_t art_dominant_color(const uint16_t *px, int n) {
  // media pesata per saturazione: privilegia i colori "vivi" come fa Spotify
  float sr = 0, sg = 0, sb = 0, sw = 0;
  for (int i = 0; i < n; i++) {
    float r = (px[i] >> 11) / 31.0f, g = ((px[i] >> 5) & 0x3F) / 63.0f, b = (px[i] & 0x1F) / 31.0f;
    float mx = std::max(r, std::max(g, b)), mn = std::min(r, std::min(g, b));
    float sat = mx > 0 ? (mx - mn) / mx : 0;
    float w = 0.05f + sat * sat * (0.3f + mx);
    sr += r * w;
    sg += g * w;
    sb += b * w;
    sw += w;
  }
  float r = sr / sw, g = sg / sw, b = sb / sw;
  // normalizza la luminosità: colore pieno ma non abbagliante
  float mx = std::max(r, std::max(g, b));
  if (mx < 0.01f) return 0x4208;
  float k = 0.62f / mx;
  r = std::min(1.0f, r * k);
  g = std::min(1.0f, g * k);
  b = std::min(1.0f, b * k);
  return ((uint16_t)(r * 31) << 11) | ((uint16_t)(g * 63) << 5) | (uint16_t)(b * 31);
}

bool art_build(const Album &a, uint16_t *color_out) {
  auto &tracks = lib_tracks();
  struct Src {
    const char *path;
    uint32_t off, len;
  } srcs[2];
  int ns = 0;
  if (a.art_track >= 0) {
    const Track &t = tracks[a.art_track];
    srcs[ns++] = {t.path, t.pic_off, t.pic_size};
  }
  if (a.art_file) srcs[ns++] = {a.art_file, 0, 0x7FFFFFFF};

  for (int i = 0; i < ns; i++) {
    FILE *f = fopen(srcs[i].path, "rb");
    if (!f) continue;
    setvbuf(f, nullptr, _IOFBF, 4096);
    int w = 0, h = 0;
    uint16_t *img = decode_jpeg(f, srcs[i].off, srcs[i].len, ART_LARGE, &w, &h);
    fclose(f);
    if (!img) continue;
    // ritaglio quadrato centrale
    int side = std::min(w, h);
    uint16_t *sq = img;
    if (w != h) {
      sq = (uint16_t *)plat_alloc_big(side * side * 2);
      int ox = (w - side) / 2, oy = (h - side) / 2;
      for (int y = 0; y < side; y++) memcpy(sq + y * side, img + (y + oy) * w + ox, side * 2);
    }
    uint16_t *large = (uint16_t *)plat_alloc_big(ART_LARGE * ART_LARGE * 2);
    uint16_t *small = (uint16_t *)plat_alloc_big(ART_SMALL * ART_SMALL * 2);
    art_resample(sq, side, side, large, ART_LARGE, ART_LARGE);
    art_resample(sq, side, side, small, ART_SMALL, ART_SMALL);
    uint16_t col = art_dominant_color(small, ART_SMALL * ART_SMALL);
    write_cache(a.key, ART_LARGE, large, col);
    write_cache(a.key, ART_SMALL, small, col);
    if (sq != img) plat_free(sq);
    plat_free(img);
    plat_free(large);
    plat_free(small);
    if (color_out) *color_out = col;
    return true;
  }
  return false;
}

const lv_image_dsc_t *art_acquire(int album, int size) {
  auto &albums = lib_albums();
  if (size <= 0 || album < 0 || album >= (int)albums.size()) return nullptr;
  uint32_t key = albums[album].key;
  Entry *e = find_entry(key, size);
  if (!e) {
    int msize = size <= ART_SMALL ? ART_SMALL : ART_LARGE;
    Entry *m = get_master(album, msize);
    if (!m) return nullptr;
    if (size == msize) {
      e = m;
    } else {
      uint16_t *px = (uint16_t *)plat_alloc_big(size * size * 2);
      if (!px) return nullptr;
      art_resample(m->px, msize, msize, px, size, size);
      e = add_entry(key, size, px);
    }
  }
  e->refs++;
  e->last = ++g_tick;
  evict();
  return &e->dsc;
}

void art_release(const lv_image_dsc_t *img) {
  if (!img) return;
  for (auto e : g_cache)
    if (&e->dsc == img) {
      if (e->refs > 0) e->refs--;
      return;
    }
}

void art_flush_unused() {
  for (size_t i = 0; i < g_cache.size();) {
    Entry *e = g_cache[i];
    if (e->refs == 0) {
      lv_image_cache_drop(&e->dsc);
      g_bytes -= e->size * e->size * 2;
      plat_free(e->px);
      g_cache.erase(g_cache.begin() + i);
      delete e;
    } else {
      i++;
    }
  }
}
