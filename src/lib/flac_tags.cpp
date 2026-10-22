#include "flac_tags.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../platform.h"

namespace {

uint32_t be24(const uint8_t *p) { return ((uint32_t)p[0] << 16) | (p[1] << 8) | p[2]; }
uint32_t be32(const uint8_t *p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | (p[2] << 8) | p[3]; }
uint32_t le32(const uint8_t *p) { return p[0] | (p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
void put_le32(std::string &s, uint32_t v) {
  for (int i = 0; i < 4; i++) s += (char)((v >> (8 * i)) & 0xFF);
}
void block_header(uint8_t *h, uint8_t type, bool last, uint32_t len) {
  h[0] = (uint8_t)((last ? 0x80 : 0) | (type & 0x7F));
  h[1] = (uint8_t)(len >> 16);
  h[2] = (uint8_t)(len >> 8);
  h[3] = (uint8_t)len;
}

bool rd(FILE *f, void *b, size_t n) { return fread(b, 1, n, f) == n; }

// Struttura del file: posizione di "fLaC" (saltando un tag ID3v2) e dei blocchi
bool read_blocks(FILE *f, FlacInfo &fi) {
  uint8_t h[10];
  fi.base = 0;
  if (!rd(f, h, 4)) return false;
  if (memcmp(h, "ID3", 3) == 0) {
    if (!rd(f, h + 4, 6)) return false;
    uint32_t sz = ((h[6] & 0x7F) << 21) | ((h[7] & 0x7F) << 14) | ((h[8] & 0x7F) << 7) | (h[9] & 0x7F);
    fi.base = 10 + sz + ((h[5] & 0x10) ? 10 : 0);
    if (fseek(f, fi.base, SEEK_SET) != 0 || !rd(f, h, 4)) return false;
  }
  if (memcmp(h, "fLaC", 4) != 0) return false;
  long pos = fi.base + 4;
  for (int guard = 0; guard < 1024; guard++) {
    uint8_t bh[4];
    if (fseek(f, pos, SEEK_SET) != 0 || !rd(f, bh, 4)) return false;
    FlacInfo::Block b;
    b.type = bh[0] & 0x7F;
    b.last = bh[0] & 0x80;
    b.off = pos;
    b.len = be24(bh + 1);
    if (b.type == 127) return false;   // non valido
    fi.blocks.push_back(b);
    pos += 4 + (long)b.len;
    if (pos > fi.file_size) return false;
    if (b.last) {
      fi.audio_off = pos;
      return !fi.blocks.empty() && fi.blocks[0].type == 0;
    }
  }
  return false;
}

// Commenti Vorbis grezzi di un blocco: vendor + elenco (i campi > max_field non
// vengono letti: li conta `skipped` e, se big != nullptr, li copia lì interi).
bool read_comments(FILE *f, const FlacInfo::Block &b, uint32_t max_field, std::string &vendor,
                   std::vector<std::string> *out, uint32_t *skipped, std::vector<std::string> *big) {
  long end = b.off + 4 + (long)b.len;
  uint8_t b4[4];
  if (fseek(f, b.off + 4, SEEK_SET) != 0 || !rd(f, b4, 4)) return false;
  uint32_t vl = le32(b4);
  if (b.off + 8 + (long)vl > end) return false;
  vendor.assign(vl, '\0');
  if (vl && !rd(f, &vendor[0], vl)) return false;
  if (!rd(f, b4, 4)) return false;
  uint32_t n = le32(b4);
  for (uint32_t i = 0; i < n; i++) {
    if (!rd(f, b4, 4)) return false;
    uint32_t cl = le32(b4);
    long here = ftell(f);
    if (here + (long)cl > end) return false;
    if (cl > max_field) {
      if (skipped) (*skipped)++;
      if (big) {
        std::string s(cl, '\0');
        if (!rd(f, &s[0], cl)) return false;
        big->push_back(std::move(s));
      } else if (fseek(f, cl, SEEK_CUR) != 0) {
        return false;
      }
      continue;
    }
    std::string s(cl, '\0');
    if (cl && !rd(f, &s[0], cl)) return false;
    if (out) out->push_back(std::move(s));
  }
  return true;
}

// Copia `len` byte da in (posizione corrente) a out
bool copy_bytes(FILE *in, FILE *out, uint64_t len, uint8_t *buf, size_t cap) {
  while (len) {
    size_t k = len < cap ? (size_t)len : cap;
    if (fread(buf, 1, k, in) != k) return false;
    if (fwrite(buf, 1, k, out) != k) return false;
    len -= k;
  }
  return true;
}

// Ricontrollo dopo una scrittura: i blocchi si leggono ancora, l'audio ha la stessa
// lunghezza e comincia dove deve (codice di sincronia del primo frame)
bool same_audio(const char *path, const FlacInfo &before, std::string &err) {
  FlacInfo now;
  FILE *f = fopen(path, "rb");
  bool ok = f != nullptr;
  if (f) {
    fseek(f, 0, SEEK_END);
    now.file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t sync[2] = {0, 0};
    ok = read_blocks(f, now) && fseek(f, now.audio_off, SEEK_SET) == 0 && rd(f, sync, 2) && sync[0] == 0xFF &&
         (sync[1] & 0xFE) == 0xF8;
    fclose(f);
  }
  ok = ok && now.file_size - now.audio_off == before.file_size - before.audio_off &&
       now.blocks[0].len == before.blocks[0].len;
  if (!ok) err = "Dopo la scrittura il file non torna: controllalo";
  return ok;
}

// Scrive `data` come blocco di tipo `type` al posto di fi.blocks[idx] (idx < 0: blocco
// nuovo). fi: la struttura del file letta poco prima (read_blocks).
bool put_block(const char *path, const FlacInfo &fi, int idx, uint8_t type, const std::string &data,
               std::string &err, bool *rewrote) {
  if (rewrote) *rewrote = false;
  if (data.size() >= (1u << 24)) {
    err = "Blocco troppo grande";
    return false;
  }
  const uint32_t N = data.size();

  // 1) sul posto: il nuovo blocco (+ eventuale riserva) occupa esattamente lo
  //    spazio del vecchio blocco e del PADDING che lo segue (o di un PADDING)
  long reg_off = -1, reg_len = 0;
  bool reg_last = false;
  if (idx >= 0) {
    reg_off = fi.blocks[idx].off;
    size_t j = idx;
    reg_len = 4 + fi.blocks[j].len;
    reg_last = fi.blocks[j].last;
    while (j + 1 < fi.blocks.size() && fi.blocks[j + 1].type == 1 && fi.blocks[j + 1].off == reg_off + reg_len) {
      j++;
      reg_len += 4 + fi.blocks[j].len;
      reg_last = fi.blocks[j].last;
    }
  }
  long need = 4 + (long)N;
  bool fits = reg_off >= 0 && (reg_len == need || reg_len - need >= 4);
  if (!fits && idx < 0) {   // blocco nuovo: un PADDING abbastanza grande
    for (auto &b : fi.blocks) {
      if (b.type != 1) continue;
      long rl = 4 + (long)b.len;
      if (rl == need || rl - need >= 4) {
        reg_off = b.off;
        reg_len = rl;
        reg_last = b.last;
        fits = true;
        break;
      }
    }
  }
  if (fits) {
    FILE *w = fopen(path, "r+b");
    if (!w) {
      err = "Impossibile scrivere il file";
      return false;
    }
    bool ok = fseek(w, reg_off, SEEK_SET) == 0;
    uint8_t h[4];
    long rest = reg_len - need;
    block_header(h, type, rest == 0 && reg_last, N);
    ok = ok && fwrite(h, 1, 4, w) == 4 && fwrite(data.data(), 1, N, w) == N;
    if (ok && rest) {
      block_header(h, 1, reg_last, (uint32_t)(rest - 4));
      ok = fwrite(h, 1, 4, w) == 4;
      static const uint8_t zero[256] = {0};
      for (long z = rest - 4; ok && z > 0; z -= 256) ok = fwrite(zero, 1, z < 256 ? z : 256, w) == (size_t)(z < 256 ? z : 256);
    }
    ok = (fclose(w) == 0) && ok;
    if (!ok) {
      err = "Errore di scrittura sulla SD";
      return false;
    }
    return same_audio(path, fi, err);
  }

  // 2) non c'è posto: si ricopia il file con 8 KB di riserva (audio identico)
  std::string tmp = std::string(path) + ".tmp~";
  FILE *in = fopen(path, "rb");
  FILE *out = in ? fopen(tmp.c_str(), "wb") : nullptr;
  if (!in || !out) {
    if (in) fclose(in);
    err = "Impossibile creare il file temporaneo";
    return false;
  }
  constexpr size_t CAP = 16 * 1024;
  uint8_t *buf = (uint8_t *)plat_alloc_fast(CAP);
  if (!buf) {
    fclose(in);
    fclose(out);
    remove(tmp.c_str());
    err = "Memoria insufficiente";
    return false;
  }
  constexpr uint32_t PAD = 8192;
  bool ok = fseek(in, 0, SEEK_SET) == 0 && copy_bytes(in, out, fi.base, buf, CAP);   // tag ID3 iniziale (raro)
  ok = ok && fwrite("fLaC", 1, 4, out) == 4;
  for (size_t i = 0; i < fi.blocks.size(); i++) {
    const FlacInfo::Block &b = fi.blocks[i];
    if (!ok) break;
    // via la riserva e il blocco sostituito (per i tag: anche eventuali doppioni)
    if (b.type == 1 || (int)i == idx || (type == 4 && b.type == 4)) continue;
    uint8_t h[4];
    block_header(h, b.type, false, b.len);
    ok = fwrite(h, 1, 4, out) == 4 && fseek(in, b.off + 4, SEEK_SET) == 0 && copy_bytes(in, out, b.len, buf, CAP);
  }
  if (ok) {
    uint8_t h[4];
    block_header(h, type, false, N);
    ok = fwrite(h, 1, 4, out) == 4 && fwrite(data.data(), 1, N, out) == N;
    block_header(h, 1, true, PAD);
    ok = ok && fwrite(h, 1, 4, out) == 4;
    memset(buf, 0, PAD < CAP ? PAD : CAP);
    ok = ok && fwrite(buf, 1, PAD, out) == PAD;
  }
  ok = ok && fseek(in, fi.audio_off, SEEK_SET) == 0 && copy_bytes(in, out, fi.file_size - fi.audio_off, buf, CAP);
  plat_free(buf);
  fclose(in);
  ok = (fclose(out) == 0) && ok;
  if (!ok) {
    remove(tmp.c_str());
    err = "Errore di scrittura sulla SD (scheda piena?)";
    return false;
  }
  // FAT: rename non sovrascrive; il vecchio file si toglie solo a copia riuscita
  if (remove(path) != 0 || rename(tmp.c_str(), path) != 0) {
    err = "Impossibile sostituire il file (la copia è in " + tmp + ")";
    return false;
  }
  if (rewrote) *rewrote = true;
  return same_audio(path, fi, err);
}

}  // namespace

bool flac_info(const char *path, FlacInfo &fi, uint32_t max_field) {
  fi = FlacInfo();
  FILE *f = fopen(path, "rb");
  if (!f) return false;
  fseek(f, 0, SEEK_END);
  fi.file_size = ftell(f);
  fseek(f, 0, SEEK_SET);
  bool ok = read_blocks(f, fi);
  for (size_t i = 0; ok && i < fi.blocks.size(); i++) {
    const FlacInfo::Block &b = fi.blocks[i];
    if (b.type == 0 && b.len >= 34) {
      uint8_t si[34];
      if (fseek(f, b.off + 4, SEEK_SET) != 0 || !rd(f, si, 34)) break;
      fi.min_block = (si[0] << 8) | si[1];
      fi.max_block = (si[2] << 8) | si[3];
      fi.sample_rate = ((uint32_t)si[10] << 12) | (si[11] << 4) | (si[12] >> 4);
      fi.channels = ((si[12] >> 1) & 0x07) + 1;
      fi.bits = (((si[12] & 1) << 4) | (si[13] >> 4)) + 1;
      fi.total_samples = ((uint64_t)(si[13] & 0x0F) << 32) | be32(si + 14);
      memcpy(fi.md5, si + 18, 16);
    } else if (b.type == 1) {
      fi.padding += b.len;
    } else if (b.type == 4 && fi.vendor.empty() && fi.comments.empty()) {
      read_comments(f, b, max_field, fi.vendor, &fi.comments, &fi.skipped, nullptr);
    } else if (b.type == 6 && b.len > 32) {
      uint8_t b4[4];
      FlacInfo::Picture p;
      if (fseek(f, b.off + 4, SEEK_SET) != 0 || !rd(f, b4, 4)) continue;
      p.type = be32(b4);
      if (!rd(f, b4, 4)) continue;
      uint32_t ml = be32(b4);
      if (ml > b.len) continue;
      p.mime.assign(ml, '\0');
      if (ml && !rd(f, &p.mime[0], ml)) continue;
      if (!rd(f, b4, 4)) continue;
      uint32_t dl = be32(b4);
      if (dl > b.len) continue;
      p.desc.assign(dl, '\0');
      if (dl && !rd(f, &p.desc[0], dl)) continue;
      uint8_t d[20];
      if (!rd(f, d, 20)) continue;
      p.w = be32(d);
      p.h = be32(d + 4);
      p.depth = be32(d + 8);
      p.data_len = be32(d + 16);
      p.data_off = ftell(f);
      p.block = (int)i;
      if (p.data_off + (long)p.data_len > b.off + 4 + (long)b.len) continue;
      fi.pictures.push_back(p);
    }
  }
  fclose(f);
  return ok && fi.sample_rate;
}

bool flac_write_comments(const char *path, const std::vector<std::string> &comments, std::string &err, bool *rewrote,
                         uint32_t max_field) {
  if (rewrote) *rewrote = false;
  FlacInfo fi;
  FILE *f = fopen(path, "rb");
  if (!f) {
    err = "File non trovato";
    return false;
  }
  fseek(f, 0, SEEK_END);
  fi.file_size = ftell(f);
  fseek(f, 0, SEEK_SET);
  if (!read_blocks(f, fi)) {
    fclose(f);
    err = "Non è un file FLAC valido";
    return false;
  }
  int vc = -1;
  for (size_t i = 0; i < fi.blocks.size(); i++)
    if (fi.blocks[i].type == 4) {
      vc = (int)i;
      break;
    }
  // vendor originale e campi enormi non modificabili dalla pagina (restano com'erano)
  std::string vendor = "onda";
  std::vector<std::string> big;
  if (vc >= 0) {
    std::string v;
    if (read_comments(f, fi.blocks[vc], max_field, v, nullptr, nullptr, &big) && !v.empty()) vendor = v;
  }
  fclose(f);

  // nuovo blocco VORBIS_COMMENT
  std::string data;
  put_le32(data, vendor.size());
  data += vendor;
  uint32_t n = 0;
  for (auto &c : comments)
    if (c.find('=') != std::string::npos && c.find('=') > 0) n++;
  put_le32(data, n + big.size());
  for (auto &c : comments) {
    size_t eq = c.find('=');
    if (eq == std::string::npos || eq == 0) continue;
    put_le32(data, c.size());
    data += c;
  }
  for (auto &c : big) {
    put_le32(data, c.size());
    data += c;
  }
  if (data.size() >= (1u << 24)) {
    err = "Tag troppo grandi";
    return false;
  }
  return put_block(path, fi, vc, 4, data, err, rewrote);
}

int flac_cover_index(const FlacInfo &fi) {
  int best = -1, best_rank = 3;
  for (size_t i = 0; i < fi.pictures.size(); i++) {
    const auto &p = fi.pictures[i];
    int rank = p.type == 3 ? 0 : (p.type == 0 ? 1 : 2);
    if (rank < best_rank && p.data_len > 0) {
      best = (int)i;
      best_rank = rank;
    }
  }
  return best;
}

bool jpeg_baseline_size(const uint8_t *d, size_t n, uint32_t *w, uint32_t *h) {
  if (n < 4 || d[0] != 0xFF || d[1] != 0xD8) return false;
  while (n > 4 && d[n - 1] == 0) n--;                      // zeri in coda (alcuni encoder)
  if (d[n - 2] != 0xFF || d[n - 1] != 0xD9) return false;  // incompleto
  size_t p = 2;
  while (p + 4 <= n) {
    if (d[p] != 0xFF) return false;
    uint8_t m = d[p + 1];
    if (m == 0xFF) {   // byte di riempimento
      p++;
      continue;
    }
    if (m == 0x01 || (m >= 0xD0 && m <= 0xD7)) {
      p += 2;
      continue;
    }
    if (m == 0xD8 || m == 0xD9 || m == 0xDA) return false;   // dati prima delle dimensioni
    uint32_t L = (d[p + 2] << 8) | d[p + 3];
    if (L < 2 || p + 2 + L > n) return false;
    if (m >= 0xC0 && m <= 0xCF && m != 0xC4 && m != 0xC8 && m != 0xCC) {
      // il decoder del lettore legge solo il baseline (SOF0) a 8 bit
      if (m != 0xC0 || L < 8 || d[p + 4] != 8) return false;
      *h = (d[p + 5] << 8) | d[p + 6];
      *w = (d[p + 7] << 8) | d[p + 8];
      return *w > 0 && *h > 0;
    }
    p += 2 + L;
  }
  return false;
}

bool flac_replace_picture(const char *path, int index, const uint8_t *jpeg, size_t len, std::string &err,
                          bool *rewrote) {
  if (rewrote) *rewrote = false;
  uint32_t w = 0, h = 0;
  if (!jpeg_baseline_size(jpeg, len, &w, &h)) {
    err = "L'immagine non è un JPEG baseline";
    return false;
  }
  FlacInfo fi;
  if (!flac_info(path, fi)) {
    err = "Non è un file FLAC valido";
    return false;
  }
  if (index < 0 || index >= (int)fi.pictures.size()) {
    err = "Immagine non trovata nel file";
    return false;
  }
  const FlacInfo::Picture &old = fi.pictures[index];
  // blocco PICTURE: tipo, mime, descrizione, larghezza, altezza, profondità, colori, dati
  std::string data;
  auto put_be32 = [&](uint32_t v) {
    for (int i = 3; i >= 0; i--) data += (char)((v >> (8 * i)) & 0xFF);
  };
  static const char kMime[] = "image/jpeg";
  data.reserve(len + 64 + old.desc.size());
  put_be32(old.type);
  put_be32(sizeof kMime - 1);
  data += kMime;
  put_be32(old.desc.size());
  data += old.desc;
  put_be32(w);
  put_be32(h);
  put_be32(24);
  put_be32(0);
  put_be32(len);
  data.append((const char *)jpeg, len);
  return put_block(path, fi, old.block, 6, data, err, rewrote);
}
