#include "library.h"
#include <dirent.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <algorithm>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include "flac_meta.h"
#include "artwork.h"
#include "../platform.h"
#include "../config.h"

namespace {

constexpr uint32_t kDbMagic = 0x42444E4F;  // "ONDB"
constexpr uint32_t kDbVersion = 3;

// ------------------------------------------------------------- arena ----
// Tutte le stringhe della libreria vivono qui (in PSRAM sull'ESP32).
struct Arena {
  std::vector<char *> blocks;
  char *cur = nullptr;
  size_t left = 0;
  const char *dup(const char *s, size_t n) {
    if (n + 1 > left) {
      size_t bs = std::max<size_t>(64 * 1024, n + 1);
      cur = (char *)plat_alloc_big(bs);
      blocks.push_back(cur);
      left = bs;
    }
    memcpy(cur, s, n);
    cur[n] = 0;
    const char *r = cur;
    cur += n + 1;
    left -= n + 1;
    return r;
  }
  const char *dup(const char *s) { return dup(s, strlen(s)); }
  void clear() {
    for (auto b : blocks) plat_free(b);
    blocks.clear();
    cur = nullptr;
    left = 0;
  }
};

struct Raw {   // record così come salvato nel database
  const char *path, *title, *artist, *album, *albumartist, *genre;
  uint32_t size, dur_ms, pic_off, pic_size, sample_rate;
  uint16_t trackno, disc, year;
  uint8_t pic_kind, bits;
};

struct Stat {
  uint32_t plays;
  uint32_t last;
};

Arena g_arena;
std::vector<Track> g_tracks;
std::vector<Album> g_albums;
std::vector<Artist> g_artists;
std::vector<Genre> g_genres;
std::vector<Playlist> g_playlists;
std::vector<uint16_t> g_all;          // tutti i brani in ordine artista/album/traccia
std::vector<uint16_t> g_liked;
std::vector<PlayContext> g_recents;
std::unordered_map<uint32_t, Stat> g_stats;
std::unordered_map<uint32_t, int> g_path_ci;   // hash path (case-insensitive) -> brano
uint32_t g_seq = 0;
bool g_dirty_liked = false, g_dirty_stats = false, g_dirty_recents = false;

volatile LibProgress g_prog = {LIB_IDLE, 0, 0, 0};

void set_prog(LibState s, uint32_t done, uint32_t total, uint8_t phase) {
  g_prog.state = s;
  g_prog.done = done;
  g_prog.total = total;
  g_prog.phase = phase;
}

// ------------------------------------------------------- normalizzazione ----
const char kExtA[] =
    "aaaaaaccccccccddddeeeeeeeeeegggggggghhhhiiiiiiiiiiiijjkkklllllll"
    "lllnnnnnnnnnoooooooorrrrrrssssssssttttttuuuuuuuuuuuuwwyyyzzzzzzs";
const char kLat1[] = "aaaaaaaceeeeiiiidnooooo*ouuuuytsaaaaaaaceeeeiiiidnooooo/ouuuuyty";

int cmp_fold(const char *a, const char *b);

bool ieq_ext(const char *name, const char *ext) {
  size_t n = strlen(name), e = strlen(ext);
  return n > e && strcasecmp(name + n - e, ext) == 0;
}

const char *base_name(const char *p) {
  const char *s = strrchr(p, '/');
  return s ? s + 1 : p;
}

std::string dir_of(const char *p) {
  const char *s = strrchr(p, '/');
  return s ? std::string(p, s - p) : std::string();
}

// "03 - Titolo.flac" -> "Titolo"
std::string title_from_file(const char *path) {
  std::string n = base_name(path);
  size_t dot = n.rfind('.');
  if (dot != std::string::npos) n.resize(dot);
  size_t i = 0;
  while (i < n.size() && isdigit((uint8_t)n[i])) i++;
  if (i > 0 && i < 4) {
    size_t j = i;
    while (j < n.size() && (n[j] == ' ' || n[j] == '-' || n[j] == '.' || n[j] == '_')) j++;
    if (j > i && j < n.size()) n = n.substr(j);
  }
  return n;
}

// ------------------------------------------------------------ scansione ----
struct FileEnt {
  const char *path;
  uint32_t size;
};

struct CoverCand {
  const char *path;
  int prio;
};

bool skip_dir(const char *name, const char *full) {
  if (name[0] == '.') return true;
  if (!strcasecmp(name, "System Volume Information")) return true;
  if (!strcmp(full, APP_DIR)) return true;
  return false;
}

int cover_prio(const char *name) {
  static const char *names[] = {"cover", "folder", "front", "album", "albumart"};
  if (!(ieq_ext(name, ".jpg") || ieq_ext(name, ".jpeg"))) return -1;
  for (int i = 0; i < 5; i++) {
    size_t l = strlen(names[i]);
    if (!strncasecmp(name, names[i], l) && name[l] == '.') return 10 - i;
  }
  return 1;   // un jpg qualsiasi nella cartella: ultima scelta
}

void walk(const char *dir, int depth, std::vector<FileEnt> &flacs, std::vector<const char *> &m3us,
          std::unordered_map<std::string, CoverCand> &covers, Arena &tmp) {
  if (depth > 10) return;
  DIR *d = opendir(dir);
  if (!d) return;
  std::vector<std::string> subdirs;
  struct dirent *e;
  char full[512];
  unsigned n_ent = 0;
  while ((e = readdir(d)) != nullptr) {
    if ((++n_ent & 63) == 0) plat_yield();
    const char *n = e->d_name;
    if (!strcmp(n, ".") || !strcmp(n, "..")) continue;
    snprintf(full, sizeof full, "%s/%s", dir, n);
    bool is_dir = e->d_type == DT_DIR;
    struct stat st;
    bool have_st = false;
    if (e->d_type == DT_UNKNOWN) {
      if (stat(full, &st) != 0) continue;
      have_st = true;
      is_dir = S_ISDIR(st.st_mode);
    }
    if (is_dir) {
      if (!skip_dir(n, full)) subdirs.push_back(full);
      continue;
    }
    if (n[0] == '.') continue;   // file nascosti (es. "._" di macOS)
    if (ieq_ext(n, ".flac")) {
      if (!have_st && stat(full, &st) != 0) continue;
      flacs.push_back({tmp.dup(full), (uint32_t)st.st_size});
      if ((flacs.size() & 31) == 0) {
        set_prog(LIB_SCANNING, flacs.size(), 0, 0);
        plat_yield();
      }
    } else if (ieq_ext(n, ".m3u") || ieq_ext(n, ".m3u8")) {
      m3us.push_back(tmp.dup(full));
    } else {
      int p = cover_prio(n);
      if (p > 0) {
        auto &c = covers[dir];
        if (p > c.prio) c = {tmp.dup(full), p};
      }
    }
  }
  closedir(d);
  std::sort(subdirs.begin(), subdirs.end());
  for (auto &s : subdirs) walk(s.c_str(), depth + 1, flacs, m3us, covers, tmp);
}

// --------------------------------------------------------- database file ----
void w32(FILE *f, uint32_t v) { fwrite(&v, 4, 1, f); }
void w16(FILE *f, uint16_t v) { fwrite(&v, 2, 1, f); }
void w8(FILE *f, uint8_t v) { fwrite(&v, 1, 1, f); }
void wstr(FILE *f, const char *s) {
  uint16_t n = s ? strlen(s) : 0;
  w16(f, n);
  if (n) fwrite(s, 1, n, f);
}
bool r32(FILE *f, uint32_t &v) { return fread(&v, 4, 1, f) == 1; }
bool r16(FILE *f, uint16_t &v) { return fread(&v, 2, 1, f) == 1; }
bool r8(FILE *f, uint8_t &v) { return fread(&v, 1, 1, f) == 1; }
bool rstr(FILE *f, Arena &a, const char *&out) {
  uint16_t n;
  if (!r16(f, n)) return false;
  char buf[600];
  if (n >= sizeof buf) return false;
  if (n && fread(buf, 1, n, f) != n) return false;
  out = a.dup(buf, n);
  return true;
}

std::string db_path() { return std::string(APP_DIR) + "/library.db"; }

struct DbExtra {
  std::unordered_map<uint32_t, std::pair<uint16_t, uint8_t>> colors;  // album key -> colore, stato
};

bool load_db(std::vector<Raw> &out, uint32_t &sig, DbExtra &ex, Arena &a) {
  FILE *f = fopen(db_path().c_str(), "rb");
  if (!f) return false;
  setvbuf(f, nullptr, _IOFBF, 8192);
  uint32_t magic, ver, n;
  bool ok = r32(f, magic) && r32(f, ver) && r32(f, sig) && r32(f, n) && magic == kDbMagic && ver == kDbVersion;
  for (uint32_t i = 0; ok && i < n; i++) {
    Raw r{};
    ok = rstr(f, a, r.path) && rstr(f, a, r.title) && rstr(f, a, r.artist) && rstr(f, a, r.album) &&
         rstr(f, a, r.albumartist) && rstr(f, a, r.genre) && r32(f, r.size) && r32(f, r.dur_ms) &&
         r32(f, r.pic_off) && r32(f, r.pic_size) && r32(f, r.sample_rate) && r16(f, r.trackno) &&
         r16(f, r.disc) && r16(f, r.year) && r8(f, r.pic_kind) && r8(f, r.bits);
    if (ok) out.push_back(r);
  }
  uint32_t nc = 0;
  if (ok && r32(f, nc)) {
    for (uint32_t i = 0; i < nc; i++) {
      uint32_t k;
      uint16_t c;
      uint8_t s;
      if (!(r32(f, k) && r16(f, c) && r8(f, s))) break;
      ex.colors[k] = {c, s};
    }
  }
  fclose(f);
  if (!ok) out.clear();
  return ok;
}

void save_db(const std::vector<Raw> &raws, uint32_t sig) {
  mkdir(APP_DIR, 0775);
  std::string tmp = db_path() + ".tmp";
  FILE *f = fopen(tmp.c_str(), "wb");
  if (!f) return;
  setvbuf(f, nullptr, _IOFBF, 8192);
  w32(f, kDbMagic);
  w32(f, kDbVersion);
  w32(f, sig);
  w32(f, raws.size());
  for (auto &r : raws) {
    wstr(f, r.path);
    wstr(f, r.title);
    wstr(f, r.artist);
    wstr(f, r.album);
    wstr(f, r.albumartist);
    wstr(f, r.genre);
    w32(f, r.size);
    w32(f, r.dur_ms);
    w32(f, r.pic_off);
    w32(f, r.pic_size);
    w32(f, r.sample_rate);
    w16(f, r.trackno);
    w16(f, r.disc);
    w16(f, r.year);
    w8(f, r.pic_kind);
    w8(f, r.bits);
  }
  w32(f, g_albums.size());
  for (auto &al : g_albums) {
    w32(f, al.key);
    w16(f, al.color);
    w8(f, al.art_state);
  }
  fclose(f);
  remove(db_path().c_str());
  rename(tmp.c_str(), db_path().c_str());
}

// ------------------------------------------------------- costruzione ----
std::mutex s_stale_mtx;
std::unordered_set<uint32_t> s_stale;   // path_hash dei file modificati dalla pagina web

std::unordered_set<uint32_t> s_art_stale;   // album con una copertina nuova (pagina web)

bool take_stale(uint32_t h) {
  std::lock_guard<std::mutex> l(s_stale_mtx);
  return s_stale.erase(h) > 0;
}
bool any_stale() {
  std::lock_guard<std::mutex> l(s_stale_mtx);
  return !s_stale.empty();
}
bool take_art_stale(uint32_t key) {
  std::lock_guard<std::mutex> l(s_stale_mtx);
  return s_art_stale.erase(key) > 0;
}

const char *kUnknownArtist = "Artista sconosciuto";
const char *kVarious = "Artisti vari";

void build(const std::vector<Raw> &raws, const std::unordered_map<std::string, CoverCand> &covers,
           const DbExtra &ex) {
  g_tracks.clear();
  g_albums.clear();
  g_artists.clear();
  g_genres.clear();
  g_all.clear();
  g_path_ci.clear();
  g_tracks.reserve(raws.size());

  std::unordered_map<uint32_t, int> album_map, genre_map;
  char fold[400];
  for (auto &r : raws) {
    if (g_tracks.size() >= 0xFFF0) break;
    Track t{};
    t.path = g_arena.dup(r.path);
    t.miss = (r.title[0] ? 0 : LIB_MISS_TITLE) | (r.artist[0] ? 0 : LIB_MISS_ARTIST) |
             (r.album[0] ? 0 : LIB_MISS_ALBUM) | (r.genre[0] ? 0 : LIB_MISS_GENRE) | (r.year ? 0 : LIB_MISS_YEAR) |
             (r.trackno ? 0 : LIB_MISS_TRACKNO);
    t.title = r.title[0] ? g_arena.dup(r.title) : g_arena.dup(title_from_file(r.path).c_str());
    t.artist = r.artist[0] ? g_arena.dup(r.artist) : kUnknownArtist;
    std::string dir = dir_of(r.path);
    const char *album_name = r.album;
    std::string folder_name;
    if (!album_name[0]) {
      folder_name = base_name(dir.c_str());
      album_name = (dir == MUSIC_ROOT || folder_name.empty()) ? "Brani singoli" : folder_name.c_str();
    }
    t.album_tag = g_arena.dup(album_name);
    t.albumartist_tag = r.albumartist[0] ? g_arena.dup(r.albumartist) : nullptr;
    t.genre_tag = r.genre[0] ? g_arena.dup(r.genre) : nullptr;
    t.path_hash = fnv1a(r.path);
    t.size = r.size;
    t.dur_ms = r.dur_ms;
    t.pic_off = r.pic_off;
    t.pic_size = r.pic_size;
    t.pic_kind = r.pic_kind;
    t.sample_rate = r.sample_rate;
    t.bits = r.bits;
    t.trackno = r.trackno;
    t.disc = r.disc;
    t.year = r.year;
    snprintf(fold, sizeof fold, "%s %s %s", t.title, t.artist, t.album_tag);
    char folded[400];
    lib_fold(fold, folded, sizeof folded);
    t.search = g_arena.dup(folded);

    // raggruppamento album: con ALBUMARTIST, oppure per cartella
    uint32_t key = t.albumartist_tag ? fnv1a_ci(t.album_tag, fnv1a_ci("\x1f", fnv1a_ci(t.albumartist_tag)))
                                     : fnv1a_ci(t.album_tag, fnv1a_ci("\x1e", fnv1a(dir.c_str())));
    auto it = album_map.find(key);
    int ai;
    if (it == album_map.end()) {
      Album a{};
      a.title = t.album_tag;
      a.artist = t.albumartist_tag;
      a.dir = g_arena.dup(dir.c_str());
      a.key = key;
      a.art_track = -1;
      auto c = covers.find(dir);
      a.art_file = c != covers.end() ? g_arena.dup(c->second.path) : nullptr;
      auto ce = ex.colors.find(key);
      if (ce != ex.colors.end()) {
        a.color = ce->second.first;
        a.art_state = ce->second.second;
      }
      ai = g_albums.size();
      album_map[key] = ai;
      g_albums.push_back(a);
    } else {
      ai = it->second;
    }
    t.album = ai;
    Album &a = g_albums[ai];
    if (t.year && t.year > a.year) a.year = t.year;
    if (a.art_track < 0 && t.pic_kind == PIC_JPEG) a.art_track = g_tracks.size();
    a.total_ms += t.dur_ms;
    a.tracks.push_back(g_tracks.size());
    g_tracks.push_back(t);
  }

  // artista dell'album quando manca ALBUMARTIST
  for (auto &a : g_albums) {
    if (a.artist) continue;
    const char *first = g_tracks[a.tracks[0]].artist;
    bool same = true;
    for (auto ti : a.tracks)
      if (strcmp(g_tracks[ti].artist, first) != 0) same = false;
    a.artist = same ? first : kVarious;
  }

  // ordina i brani dentro l'album
  for (auto &a : g_albums) {
    std::sort(a.tracks.begin(), a.tracks.end(), [](uint16_t x, uint16_t y) {
      const Track &p = g_tracks[x], &q = g_tracks[y];
      if (p.disc != q.disc) return p.disc < q.disc;
      if (p.trackno != q.trackno) return p.trackno < q.trackno;
      return strcmp(p.path, q.path) < 0;
    });
  }

  // ordina gli album: artista, anno, titolo
  std::vector<int> perm(g_albums.size());
  for (size_t i = 0; i < perm.size(); i++) perm[i] = i;
  std::sort(perm.begin(), perm.end(), [](int x, int y) {
    const Album &a = g_albums[x], &b = g_albums[y];
    int c = cmp_fold(a.artist, b.artist);
    if (c) return c < 0;
    if (a.year != b.year) return a.year < b.year;
    return cmp_fold(a.title, b.title) < 0;
  });
  std::vector<Album> sorted;
  sorted.reserve(g_albums.size());
  std::vector<int> inv(g_albums.size());
  for (size_t i = 0; i < perm.size(); i++) {
    inv[perm[i]] = i;
    sorted.push_back(std::move(g_albums[perm[i]]));
  }
  g_albums.swap(sorted);
  for (auto &t : g_tracks) t.album = inv[t.album];
  for (auto &a : g_albums)
    for (auto ti : a.tracks) g_all.push_back(ti);

  // artisti (dagli artisti degli album)
  std::unordered_map<uint32_t, int> artist_map;
  for (size_t ai = 0; ai < g_albums.size(); ai++) {
    Album &a = g_albums[ai];
    uint32_t k = fnv1a_ci(a.artist);
    auto it = artist_map.find(k);
    int ri;
    if (it == artist_map.end()) {
      ri = g_artists.size();
      artist_map[k] = ri;
      Artist ar{};
      ar.name = a.artist;
      ar.key = k;
      g_artists.push_back(ar);
    } else {
      ri = it->second;
    }
    a.artist_idx = ri;
    g_artists[ri].albums.push_back(ai);
    for (auto ti : a.tracks) g_artists[ri].tracks.push_back(ti);
  }
  {
    std::vector<int> p(g_artists.size());
    for (size_t i = 0; i < p.size(); i++) p[i] = i;
    std::sort(p.begin(), p.end(), [](int x, int y) { return cmp_fold(g_artists[x].name, g_artists[y].name) < 0; });
    std::vector<Artist> s;
    std::vector<int> iv(p.size());
    for (size_t i = 0; i < p.size(); i++) {
      iv[p[i]] = i;
      s.push_back(std::move(g_artists[p[i]]));
    }
    g_artists.swap(s);
    for (auto &a : g_albums) a.artist_idx = iv[a.artist_idx];
  }

  // generi
  for (auto ti : g_all) {
    Track &t = g_tracks[ti];
    t.genre = 0xFFFF;
    if (!t.genre_tag) continue;
    uint32_t k = fnv1a_ci(t.genre_tag);
    auto it = genre_map.find(k);
    int gi;
    if (it == genre_map.end()) {
      gi = g_genres.size();
      genre_map[k] = gi;
      Genre g{};
      g.name = t.genre_tag;
      g.key = k;
      g_genres.push_back(g);
    } else {
      gi = it->second;
    }
    g_genres[gi].tracks.push_back(ti);
  }
  std::sort(g_genres.begin(), g_genres.end(), [](const Genre &a, const Genre &b) { return cmp_fold(a.name, b.name) < 0; });
  for (size_t gi = 0; gi < g_genres.size(); gi++)
    for (auto ti : g_genres[gi].tracks) g_tracks[ti].genre = gi;

  for (size_t i = 0; i < g_tracks.size(); i++) g_path_ci[fnv1a_ci(g_tracks[i].path)] = i;
}

// ------------------------------------------------------------- playlist ----
std::string normalize_path(const std::string &p) {
  std::vector<std::string> parts;
  size_t i = 0;
  while (i <= p.size()) {
    size_t j = p.find('/', i);
    if (j == std::string::npos) j = p.size();
    std::string s = p.substr(i, j - i);
    if (s == "..") {
      if (!parts.empty()) parts.pop_back();
    } else if (!s.empty() && s != ".") {
      parts.push_back(s);
    }
    i = j + 1;
  }
  std::string out;
  for (auto &s : parts) out += "/" + s;
  return out;
}

void load_playlists(const std::vector<const char *> &m3us) {
  g_playlists.clear();
  char line[512];
  for (auto path : m3us) {
    FILE *f = fopen(path, "r");
    if (!f) continue;
    Playlist pl{};
    std::string n = base_name(path);
    n.resize(n.rfind('.'));
    pl.name = g_arena.dup(n.c_str());
    pl.path = g_arena.dup(path);
    pl.key = fnv1a(path);
    std::string dir = dir_of(path);
    while (fgets(line, sizeof line, f)) {
      size_t l = strlen(line);
      while (l && (line[l - 1] == '\n' || line[l - 1] == '\r' || line[l - 1] == ' ')) line[--l] = 0;
      char *s = line;
      if (!strncmp(s, "\xEF\xBB\xBF", 3)) s += 3;   // BOM
      while (*s == ' ') s++;
      if (!*s || *s == '#') continue;
      if (strstr(s, "://")) continue;                // URL: non supportati
      for (char *c = s; *c; c++)
        if (*c == '\\') *c = '/';
      if (isalpha((uint8_t)s[0]) && s[1] == ':') s += 2;   // "C:/..." -> "/..."
      std::string full = s[0] == '/' ? std::string(SD_MOUNT) + s : dir + "/" + s;
      full = normalize_path(full);
      auto it = g_path_ci.find(fnv1a_ci(full.c_str()));
      if (it != g_path_ci.end()) pl.tracks.push_back(it->second);
    }
    fclose(f);
    g_playlists.push_back(std::move(pl));
  }
  std::sort(g_playlists.begin(), g_playlists.end(),
            [](const Playlist &a, const Playlist &b) { return cmp_fold(a.name, b.name) < 0; });
}

// ------------------------------------------------ preferiti / statistiche ----
std::string app_file(const char *n) { return std::string(APP_DIR) + "/" + n; }

void load_user_data() {
  g_liked.clear();
  FILE *f = fopen(app_file("liked.txt").c_str(), "r");
  if (f) {
    char line[512];
    while (fgets(line, sizeof line, f)) {
      size_t l = strlen(line);
      while (l && (line[l - 1] == '\n' || line[l - 1] == '\r')) line[--l] = 0;
      auto it = g_path_ci.find(fnv1a_ci(line));
      if (it != g_path_ci.end() &&
          std::find(g_liked.begin(), g_liked.end(), it->second) == g_liked.end())
        g_liked.push_back(it->second);
    }
    fclose(f);
  }
  g_stats.clear();
  g_seq = 0;
  f = fopen(app_file("stats.bin").c_str(), "rb");
  if (f) {
    uint32_t rec[3];
    while (fread(rec, 4, 3, f) == 3) {
      g_stats[rec[0]] = {rec[1], rec[2]};
      g_seq = std::max(g_seq, rec[2]);
    }
    fclose(f);
  }
  g_recents.clear();
  f = fopen(app_file("recents.bin").c_str(), "rb");
  if (f) {
    PlayContext c;
    while (fread(&c, sizeof c, 1, f) == 1) {
      c.name[sizeof c.name - 1] = 0;
      if (lib_ctx_valid(c)) g_recents.push_back(c);
    }
    fclose(f);
  }
}

}  // namespace

// ======================================================================= API ==

int cmp_fold_impl(const char *a, const char *b) {
  char fa[160], fb[160];
  lib_fold(a, fa, sizeof fa);
  lib_fold(b, fb, sizeof fb);
  // "the " iniziale ignorato come fanno i lettori musicali
  const char *pa = strncmp(fa, "the ", 4) ? fa : fa + 4;
  const char *pb = strncmp(fb, "the ", 4) ? fb : fb + 4;
  return strcmp(pa, pb);
}
namespace {
int cmp_fold(const char *a, const char *b) { return cmp_fold_impl(a, b); }
}  // namespace

void lib_fold(const char *in, char *out, size_t cap) {
  size_t o = 0;
  const uint8_t *s = (const uint8_t *)in;
  while (*s && o + 1 < cap) {
    uint32_t cp;
    int n;
    if (*s < 0x80) {
      cp = *s;
      n = 1;
    } else if ((*s & 0xE0) == 0xC0 && s[1]) {
      cp = ((s[0] & 0x1F) << 6) | (s[1] & 0x3F);
      n = 2;
    } else if ((*s & 0xF0) == 0xE0 && s[1] && s[2]) {
      cp = ((s[0] & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
      n = 3;
    } else {
      // altri script: copiamo i byte così come sono
      out[o++] = *s++;
      continue;
    }
    char c = 0;
    if (cp < 0x80) c = tolower(cp);
    else if (cp >= 0xC0 && cp <= 0xFF) c = kLat1[cp - 0xC0];
    else if (cp >= 0x100 && cp <= 0x17F) c = kExtA[cp - 0x100];
    else if (cp == 0x2019 || cp == 0x2018) c = '\'';
    else if (cp == 0x2013 || cp == 0x2014) c = '-';
    if (c) {
      out[o++] = c;
    } else {
      for (int k = 0; k < n && o + 1 < cap; k++) out[o++] = s[k];
    }
    s += n;
  }
  out[o] = 0;
}

bool lib_scan(bool force) {
  set_prog(LIB_SCANNING, 0, 0, 0);
  Arena tmp;
  std::vector<FileEnt> files;
  std::vector<const char *> m3us;
  std::unordered_map<std::string, CoverCand> covers;
  DIR *probe = opendir(MUSIC_ROOT);
  if (!probe) {
    set_prog(LIB_NO_SD, 0, 0, 0);
    return false;
  }
  closedir(probe);
  mkdir(APP_DIR, 0775);
  std::string artdir = std::string(APP_DIR) + "/art";
  mkdir(artdir.c_str(), 0775);

  uint32_t t0 = plat_millis();
  plat_mark(1, "scansione: cartelle");
  walk(MUSIC_ROOT, 0, files, m3us, covers, tmp);
  uint32_t sig = 2166136261u;
  for (auto &f : files) {
    sig = fnv1a(f.path, sig);
    sig ^= f.size;
    sig *= 16777619u;
  }
  plat_log("[lib] %u file FLAC, %u playlist (%u ms)", (unsigned)files.size(), (unsigned)m3us.size(),
           (unsigned)(plat_millis() - t0));

  std::vector<Raw> old;
  uint32_t old_sig = 0;
  DbExtra ex;
  Arena dbarena;
  bool have_db = load_db(old, old_sig, ex, dbarena);

  std::vector<Raw> raws;
  Arena rawarena;
  bool changed = force || !have_db || old_sig != sig || any_stale();
  if (!changed) {
    raws = old;
  } else {
    std::unordered_map<uint32_t, const Raw *> prev;
    if (!force)
      for (auto &r : old) prev[fnv1a(r.path)] = &r;
    FlacMeta *m = new FlacMeta;
    for (size_t i = 0; i < files.size(); i++) {
      set_prog(LIB_SCANNING, i, files.size(), 1);
      plat_mark(1, "tag", files[i].path);
      auto it = prev.find(fnv1a(files[i].path));
      if (it != prev.end() && it->second->size == files[i].size && !take_stale(fnv1a(files[i].path))) {
        raws.push_back(*it->second);
        continue;
      }
      Raw r{};
      r.path = files[i].path;
      r.size = files[i].size;
      if (flac_read_meta(files[i].path, *m)) {
        r.title = rawarena.dup(m->title);
        r.artist = rawarena.dup(m->artist);
        r.album = rawarena.dup(m->album);
        r.albumartist = rawarena.dup(m->albumartist);
        r.genre = rawarena.dup(m->genre);
        r.dur_ms = m->duration_ms;
        r.pic_off = m->pic_offset;
        r.pic_size = m->pic_size;
        r.pic_kind = m->pic_kind;
        r.sample_rate = m->sample_rate;
        r.bits = m->bits;
        r.trackno = m->track;
        r.disc = m->disc;
        r.year = m->year;
      } else {
        r.title = r.artist = r.album = r.albumartist = r.genre = "";
      }
      raws.push_back(r);
      if ((i & 7) == 0) plat_yield();
    }
    delete m;
  }

  // ricostruzione completa delle strutture in memoria
  g_arena.clear();
  build(raws, covers, ex);
  load_playlists(m3us);

  // copertine mancanti
  bool art_changed = false;
  for (size_t i = 0; i < g_albums.size(); i++) {
    Album &a = g_albums[i];
    set_prog(LIB_SCANNING, i, g_albums.size(), 2);
    plat_mark(1, "copertina", a.title);
    bool fresh = take_art_stale(a.key);
    if (!fresh && a.art_state == 1 && art_has_cache(a.key)) continue;
    if (!fresh && a.art_state == 2 && !changed) continue;
    uint16_t col = 0;
    bool ok = art_build(a, &col);
    a.art_state = ok ? 1 : 2;
    a.color = ok ? col : 0;
    art_changed = true;
    plat_yield();
  }
  if (changed || art_changed) {
    set_prog(LIB_SCANNING, 0, 1, 3);
    plat_mark(1, "scansione: salvataggio indice");
    save_db(raws, sig);
  }
  plat_mark(1, "scansione: preferiti e statistiche");
  load_user_data();
  tmp.clear();
  rawarena.clear();
  dbarena.clear();
  plat_log("[lib] pronto: %u brani, %u album, %u artisti, %u generi, %u playlist (%u ms)",
           (unsigned)g_tracks.size(), (unsigned)g_albums.size(), (unsigned)g_artists.size(),
           (unsigned)g_genres.size(), (unsigned)g_playlists.size(), (unsigned)(plat_millis() - t0));
  plat_mark(1, "scansione completata");
  set_prog(LIB_READY, g_tracks.size(), g_tracks.size(), 3);
  return true;
}

LibProgress lib_progress() {
  LibProgress p;
  p.state = g_prog.state;
  p.done = g_prog.done;
  p.total = g_prog.total;
  p.phase = g_prog.phase;
  return p;
}
LibState lib_state() { return g_prog.state; }

const std::vector<Track> &lib_tracks() { return g_tracks; }
const std::vector<Album> &lib_albums() { return g_albums; }
const std::vector<Artist> &lib_artists() { return g_artists; }
const std::vector<Genre> &lib_genres() { return g_genres; }

void lib_mark_stale(const char *path) {
  std::lock_guard<std::mutex> l(s_stale_mtx);
  s_stale.insert(fnv1a(path));
}

void lib_mark_art_stale(uint32_t album_key) {
  std::lock_guard<std::mutex> l(s_stale_mtx);
  s_art_stale.insert(album_key);
}
const std::vector<Playlist> &lib_playlists() { return g_playlists; }
const char *lib_track_path(int t) { return (t >= 0 && t < (int)g_tracks.size()) ? g_tracks[t].path : nullptr; }

int lib_album_by_key(uint32_t k) {
  for (size_t i = 0; i < g_albums.size(); i++)
    if (g_albums[i].key == k) return i;
  return -1;
}
int lib_artist_by_key(uint32_t k) {
  for (size_t i = 0; i < g_artists.size(); i++)
    if (g_artists[i].key == k) return i;
  return -1;
}
int lib_genre_by_key(uint32_t k) {
  for (size_t i = 0; i < g_genres.size(); i++)
    if (g_genres[i].key == k) return i;
  return -1;
}
int lib_playlist_by_key(uint32_t k) {
  for (size_t i = 0; i < g_playlists.size(); i++)
    if (g_playlists[i].key == k) return i;
  return -1;
}

static PlayContext mkctx(CtxType t, uint32_t key, const char *name) {
  PlayContext c;
  c.type = t;
  c.key = key;
  utf8_copy(c.name, sizeof c.name, name, strlen(name));
  return c;
}
PlayContext lib_ctx_album(int a) { return mkctx(CTX_ALBUM, g_albums[a].key, g_albums[a].title); }
PlayContext lib_ctx_artist(int a) { return mkctx(CTX_ARTIST, g_artists[a].key, g_artists[a].name); }
PlayContext lib_ctx_playlist(int p) { return mkctx(CTX_PLAYLIST, g_playlists[p].key, g_playlists[p].name); }
PlayContext lib_ctx_genre(int g) { return mkctx(CTX_GENRE, g_genres[g].key, g_genres[g].name); }
PlayContext lib_ctx_liked() { return mkctx(CTX_LIKED, 1, "Brani che ti piacciono"); }
PlayContext lib_ctx_all() { return mkctx(CTX_ALL, 2, "Tutti i brani"); }

bool lib_ctx_valid(const PlayContext &c) {
  switch (c.type) {
    case CTX_ALBUM: return lib_album_by_key(c.key) >= 0;
    case CTX_ARTIST: return lib_artist_by_key(c.key) >= 0;
    case CTX_PLAYLIST: return lib_playlist_by_key(c.key) >= 0;
    case CTX_GENRE: return lib_genre_by_key(c.key) >= 0;
    case CTX_LIKED:
    case CTX_ALL: return true;
    default: return false;
  }
}

void lib_context_tracks(const PlayContext &c, std::vector<uint16_t> &out) {
  out.clear();
  int i;
  switch (c.type) {
    case CTX_ALBUM:
      if ((i = lib_album_by_key(c.key)) >= 0) out = g_albums[i].tracks;
      break;
    case CTX_ARTIST:
      if ((i = lib_artist_by_key(c.key)) >= 0) out = g_artists[i].tracks;
      break;
    case CTX_PLAYLIST:
      if ((i = lib_playlist_by_key(c.key)) >= 0) out = g_playlists[i].tracks;
      break;
    case CTX_GENRE:
      if ((i = lib_genre_by_key(c.key)) >= 0) out = g_genres[i].tracks;
      break;
    case CTX_LIKED: out = g_liked; break;
    case CTX_ALL: out = g_all; break;
    default: break;
  }
}

bool lib_is_liked(int t) { return std::find(g_liked.begin(), g_liked.end(), (uint16_t)t) != g_liked.end(); }

void lib_set_liked(int t, bool liked) {
  auto it = std::find(g_liked.begin(), g_liked.end(), (uint16_t)t);
  if (liked && it == g_liked.end()) g_liked.insert(g_liked.begin(), t);
  else if (!liked && it != g_liked.end()) g_liked.erase(it);
  else return;
  g_dirty_liked = true;
}
const std::vector<uint16_t> &lib_liked() { return g_liked; }

void lib_note_played(int t) {
  if (t < 0 || t >= (int)g_tracks.size()) return;
  auto &s = g_stats[g_tracks[t].path_hash];
  s.plays++;
  s.last = ++g_seq;
  g_dirty_stats = true;
}

uint32_t lib_play_count(int t) {
  auto it = g_stats.find(g_tracks[t].path_hash);
  return it == g_stats.end() ? 0 : it->second.plays;
}

uint32_t lib_album_plays(int a) {
  uint32_t n = 0;
  for (auto t : g_albums[a].tracks) n += lib_play_count(t);
  return n;
}

void lib_recent_push(const PlayContext &c) {
  if (c.type == CTX_NONE || c.type == CTX_SEARCH) return;
  for (auto it = g_recents.begin(); it != g_recents.end(); ++it)
    if (it->type == c.type && it->key == c.key) {
      if (it == g_recents.begin()) return;
      g_recents.erase(it);
      break;
    }
  g_recents.insert(g_recents.begin(), c);
  if (g_recents.size() > 12) g_recents.resize(12);
  g_dirty_recents = true;
}
const std::vector<PlayContext> &lib_recents() { return g_recents; }

void lib_save_dirty() {
  if (g_dirty_liked) {
    g_dirty_liked = false;
    FILE *f = fopen(app_file("liked.txt").c_str(), "w");
    if (f) {
      for (auto t : g_liked) fprintf(f, "%s\n", g_tracks[t].path);
      fclose(f);
    }
  }
  if (g_dirty_stats) {
    g_dirty_stats = false;
    FILE *f = fopen(app_file("stats.bin").c_str(), "wb");
    if (f) {
      for (auto &kv : g_stats) {
        uint32_t rec[3] = {kv.first, kv.second.plays, kv.second.last};
        fwrite(rec, 4, 3, f);
      }
      fclose(f);
    }
  }
  if (g_dirty_recents) {
    g_dirty_recents = false;
    FILE *f = fopen(app_file("recents.bin").c_str(), "wb");
    if (f) {
      for (auto &c : g_recents) fwrite(&c, sizeof c, 1, f);
      fclose(f);
    }
  }
}

void lib_search(const char *query, SearchResult &r, size_t max_tracks) {
  r.tracks.clear();
  r.albums.clear();
  r.artists.clear();
  r.playlists.clear();
  char q[128];
  lib_fold(query, q, sizeof q);
  // parole separate da spazi: devono comparire tutte
  std::vector<std::string> words;
  char *save = nullptr;
  for (char *tok = strtok_r(q, " ", &save); tok; tok = strtok_r(nullptr, " ", &save)) words.push_back(tok);
  if (words.empty()) return;
  auto match = [&](const char *s) {
    for (auto &w : words)
      if (!strstr(s, w.c_str())) return false;
    return true;
  };
  char f[200];
  for (size_t i = 0; i < g_artists.size() && r.artists.size() < 4; i++) {
    lib_fold(g_artists[i].name, f, sizeof f);
    if (match(f)) r.artists.push_back(i);
  }
  for (size_t i = 0; i < g_albums.size() && r.albums.size() < 6; i++) {
    char tmp[300];
    snprintf(tmp, sizeof tmp, "%s %s", g_albums[i].title, g_albums[i].artist);
    lib_fold(tmp, f, sizeof f);
    if (match(f)) r.albums.push_back(i);
  }
  for (size_t i = 0; i < g_playlists.size() && r.playlists.size() < 4; i++) {
    lib_fold(g_playlists[i].name, f, sizeof f);
    if (match(f)) r.playlists.push_back(i);
  }
  for (auto ti : g_all) {
    if (r.tracks.size() >= max_tracks) break;
    if (match(g_tracks[ti].search)) r.tracks.push_back(ti);
  }
}

void fmt_duration(uint32_t ms, char *out, size_t cap) {
  uint32_t s = ms / 1000;
  if (s >= 3600) snprintf(out, cap, "%u:%02u:%02u", (unsigned)(s / 3600), (unsigned)(s / 60 % 60), (unsigned)(s % 60));
  else snprintf(out, cap, "%u:%02u", (unsigned)(s / 60), (unsigned)(s % 60));
}

void fmt_long_duration(uint32_t ms, char *out, size_t cap) {
  uint32_t m = (ms + 30000) / 60000;
  if (ms < 60000) snprintf(out, cap, "%u s", (unsigned)(ms / 1000));
  else if (m >= 60) snprintf(out, cap, "%u h %u min", (unsigned)(m / 60), (unsigned)(m % 60));
  else snprintf(out, cap, "%u min", (unsigned)m);
}
