#include "flac_meta.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

uint32_t fnv1a(const char *s, uint32_t h) {
  while (*s) {
    h ^= (uint8_t)*s++;
    h *= 16777619u;
  }
  return h;
}

uint32_t fnv1a_ci(const char *s, uint32_t h) {
  while (*s) {
    h ^= (uint8_t)tolower((uint8_t)*s++);
    h *= 16777619u;
  }
  return h;
}

// Copia al massimo cap-1 byte senza spezzare una sequenza UTF-8.
void utf8_copy(char *dst, size_t cap, const char *src, size_t len) {
  if (cap == 0) return;
  if (len > cap - 1) {
    len = cap - 1;
    while (len > 0 && ((uint8_t)src[len] & 0xC0) == 0x80) len--;
  }
  memcpy(dst, src, len);
  dst[len] = 0;
  for (size_t i = 0; i < len; i++)
    if ((uint8_t)dst[i] < 0x20) dst[i] = ' ';
  // spazi finali
  while (len > 0 && dst[len - 1] == ' ') dst[--len] = 0;
}

namespace {

uint32_t be24(const uint8_t *p) { return ((uint32_t)p[0] << 16) | (p[1] << 8) | p[2]; }
uint32_t be32(const uint8_t *p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | (p[2] << 8) | p[3]; }
uint32_t le32(const uint8_t *p) { return p[0] | (p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }

bool key_is(const char *c, size_t klen, const char *key) {
  size_t n = strlen(key);
  if (klen != n) return false;
  for (size_t i = 0; i < n; i++)
    if (toupper((uint8_t)c[i]) != key[i]) return false;
  return true;
}

int parse_int(const char *v, size_t vl) {
  int n = 0;
  size_t i = 0;
  while (i < vl && v[i] == ' ') i++;
  bool any = false;
  for (; i < vl && v[i] >= '0' && v[i] <= '9'; i++) {
    n = n * 10 + (v[i] - '0');
    any = true;
    if (n > 100000) break;
  }
  return any ? n : 0;
}

void append_field(char *dst, size_t cap, const char *v, size_t vlen) {
  size_t cur = strlen(dst);
  if (cur == 0) {
    utf8_copy(dst, cap, v, vlen);
    return;
  }
  // più valori per lo stesso tag (es. due ARTIST): "A, B"
  if (cur + 4 >= cap) return;
  dst[cur] = ',';
  dst[cur + 1] = ' ';
  utf8_copy(dst + cur + 2, cap - cur - 2, v, vlen);
}

void parse_comment(FlacMeta &m, const char *c, size_t len) {
  const char *eq = (const char *)memchr(c, '=', len);
  if (!eq) return;
  size_t kl = eq - c;
  const char *v = eq + 1;
  size_t vl = len - kl - 1;
  if (vl == 0) return;
  if (key_is(c, kl, "TITLE")) {
    if (!m.title[0]) utf8_copy(m.title, sizeof m.title, v, vl);
  } else if (key_is(c, kl, "ARTIST")) {
    append_field(m.artist, sizeof m.artist, v, vl);
  } else if (key_is(c, kl, "ALBUM")) {
    if (!m.album[0]) utf8_copy(m.album, sizeof m.album, v, vl);
  } else if (key_is(c, kl, "ALBUMARTIST") || key_is(c, kl, "ALBUM ARTIST") || key_is(c, kl, "ALBUM_ARTIST")) {
    if (!m.albumartist[0]) utf8_copy(m.albumartist, sizeof m.albumartist, v, vl);
  } else if (key_is(c, kl, "GENRE")) {
    if (!m.genre[0]) utf8_copy(m.genre, sizeof m.genre, v, vl);
  } else if (key_is(c, kl, "TRACKNUMBER")) {
    m.track = (uint16_t)parse_int(v, vl);
  } else if (key_is(c, kl, "DISCNUMBER")) {
    m.disc = (uint16_t)parse_int(v, vl);
  } else if (key_is(c, kl, "DATE") || key_is(c, kl, "YEAR") || key_is(c, kl, "ORIGINALDATE")) {
    if (!m.year) {
      int y = parse_int(v, vl);
      if (y > 1000 && y < 3000) m.year = y;
    }
  }
}

bool read_exact(FILE *f, void *buf, size_t n) { return fread(buf, 1, n, f) == n; }

}  // namespace

bool flac_read_meta_fp(FILE *f, FlacMeta &m) {
  memset(&m, 0, sizeof m);
  uint8_t hdr[10];
  if (!read_exact(f, hdr, 4)) return false;
  long base = 0;
  if (memcmp(hdr, "ID3", 3) == 0) {
    // tag ID3v2 davanti al file: lo saltiamo
    if (!read_exact(f, hdr + 4, 6)) return false;
    uint32_t sz = ((hdr[6] & 0x7F) << 21) | ((hdr[7] & 0x7F) << 14) | ((hdr[8] & 0x7F) << 7) | (hdr[9] & 0x7F);
    base = 10 + sz + ((hdr[5] & 0x10) ? 10 : 0);
    if (fseek(f, base, SEEK_SET) != 0 || !read_exact(f, hdr, 4)) return false;
  }
  if (memcmp(hdr, "fLaC", 4) != 0) return false;

  int best_pic_type = 999;
  char cbuf[512];
  bool last = false;
  while (!last) {
    uint8_t bh[4];
    if (!read_exact(f, bh, 4)) break;
    last = bh[0] & 0x80;
    uint8_t type = bh[0] & 0x7F;
    uint32_t blen = be24(bh + 1);
    long start = ftell(f);
    if (type == 0 && blen >= 34) {  // STREAMINFO
      uint8_t si[34];
      if (!read_exact(f, si, 34)) return false;
      m.sample_rate = ((uint32_t)si[10] << 12) | (si[11] << 4) | (si[12] >> 4);
      m.channels = ((si[12] >> 1) & 0x07) + 1;
      m.bits = (((si[12] & 1) << 4) | (si[13] >> 4)) + 1;
      m.total_samples = ((uint64_t)(si[13] & 0x0F) << 32) | be32(si + 14);
      if (m.sample_rate) m.duration_ms = (uint32_t)(m.total_samples * 1000ULL / m.sample_rate);
    } else if (type == 4) {  // VORBIS_COMMENT (little endian)
      uint8_t b4[4];
      if (!read_exact(f, b4, 4)) break;
      uint32_t vendor = le32(b4);
      if (vendor > blen) break;
      fseek(f, vendor, SEEK_CUR);
      if (!read_exact(f, b4, 4)) break;
      uint32_t n = le32(b4);
      for (uint32_t i = 0; i < n && i < 1000; i++) {
        if (!read_exact(f, b4, 4)) break;
        uint32_t cl = le32(b4);
        if ((long)(ftell(f) + cl) > start + (long)blen) break;
        if (cl < sizeof cbuf) {
          if (!read_exact(f, cbuf, cl)) break;
          parse_comment(m, cbuf, cl);
        } else {
          // campo enorme (es. LYRICS): leggiamo solo il nome per sicurezza e saltiamo
          fseek(f, cl, SEEK_CUR);
        }
      }
    } else if (type == 6 && blen > 32) {  // PICTURE (big endian)
      uint8_t b4[4];
      if (!read_exact(f, b4, 4)) break;
      uint32_t ptype = be32(b4);
      if (!read_exact(f, b4, 4)) break;
      uint32_t mlen = be32(b4);
      char mime[32] = {0};
      if (mlen >= blen) break;
      if (mlen < sizeof mime) read_exact(f, mime, mlen);
      else fseek(f, mlen, SEEK_CUR);
      if (!read_exact(f, b4, 4)) break;
      uint32_t dlen = be32(b4);
      if (dlen >= blen) break;
      fseek(f, dlen + 16, SEEK_CUR);   // descrizione + w,h,depth,colors
      if (!read_exact(f, b4, 4)) break;
      uint32_t size = be32(b4);
      uint32_t off = (uint32_t)ftell(f);
      // preferenza: copertina frontale (3), poi la prima trovata
      int rank = ptype == 3 ? 0 : (ptype == 0 ? 1 : 2);
      if (rank < best_pic_type && size > 0) {
        uint8_t magic[4] = {0};
        read_exact(f, magic, 4);
        uint8_t kind = PIC_OTHER;
        if (magic[0] == 0xFF && magic[1] == 0xD8) kind = PIC_JPEG;
        else if (magic[0] == 0x89 && magic[1] == 'P') kind = PIC_PNG;
        best_pic_type = rank;
        m.pic_offset = off;
        m.pic_size = size;
        m.pic_kind = kind;
      }
    }
    if (fseek(f, start + blen, SEEK_SET) != 0) break;
  }
  return m.sample_rate != 0;
}

bool flac_read_meta(const char *path, FlacMeta &m) {
  FILE *f = fopen(path, "rb");
  if (!f) return false;
  bool ok = flac_read_meta_fp(f, m);
  fclose(f);
  return ok;
}
