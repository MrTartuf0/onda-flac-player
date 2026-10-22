// Libreria musicale: scansione della SD, indice in cache, album/artisti/generi,
// playlist .m3u, preferiti, statistiche di ascolto e ricerca.
#pragma once
#include <stdint.h>
#include <string>
#include <vector>
#include "../types.h"

struct Track {
  const char *path;
  const char *title;
  const char *artist;
  const char *album_tag;
  const char *albumartist_tag;
  const char *genre_tag;
  const char *search;      // testo normalizzato per la ricerca
  uint32_t path_hash;
  uint32_t size;
  uint32_t dur_ms;
  uint32_t pic_off;
  uint32_t pic_size;
  uint32_t sample_rate;
  uint16_t trackno;
  uint16_t disc;
  uint16_t year;
  uint16_t album;          // indice album
  uint16_t genre;          // indice genere o 0xFFFF
  uint8_t bits;
  uint8_t pic_kind;
  uint8_t miss;            // tag mancanti (LIB_MISS_*): per la pagina web
  uint32_t added;          // scansione in cui è comparso (0 = c'era già prima che lo si contasse)
  uint32_t mtime;          // data di modifica del file
};

enum : uint8_t {
  LIB_MISS_TITLE = 1, LIB_MISS_ARTIST = 2, LIB_MISS_ALBUM = 4, LIB_MISS_GENRE = 8,
  LIB_MISS_YEAR = 16, LIB_MISS_TRACKNO = 32,
};

struct Album {
  const char *title;
  const char *artist;
  const char *dir;         // cartella del primo brano (per cover.jpg)
  // Immagine accanto ai brani, oppure nullptr:
  //  - "<nome del brano>.jpg" di uno dei suoi brani (vince su tutto)
  //  - cover.jpg / folder.jpg ... della cartella, solo se nella cartella c'è
  //    soltanto questo album (cover.* vince sull'incorporata, le altre dopo)
  const char *art_file;
  uint32_t key;
  uint32_t art_sig;        // da dove viene la copertina: se cambia, la cache si rifà
  uint16_t artist_idx;
  uint16_t year;
  uint16_t color;          // colore dominante della copertina (RGB565)
  int16_t art_track;       // brano con copertina incorporata JPEG, -1 se nessuno
  uint8_t art_state;       // 0 = da verificare, 1 = ok, 2 = nessuna copertina
  bool art_first;          // art_file prima dell'immagine incorporata
  bool art_own;            // art_file è "<nome del brano>.jpg"
  bool shared_dir;         // nella cartella ci sono brani di più album
  uint32_t total_ms;
  std::vector<uint16_t> tracks;
};

struct Artist {
  const char *name;
  uint32_t key;
  std::vector<uint16_t> albums;
  std::vector<uint16_t> tracks;
};

struct Genre {
  const char *name;
  uint32_t key;
  std::vector<uint16_t> tracks;
};

struct Playlist {
  const char *name;
  const char *path;
  uint32_t key;
  std::vector<uint16_t> tracks;
};

enum LibState : uint8_t { LIB_IDLE, LIB_NO_SD, LIB_SCANNING, LIB_READY };

struct LibProgress {
  LibState state;
  uint32_t done;
  uint32_t total;
  uint8_t phase;           // 0 = file, 1 = metadati, 2 = copertine, 3 = salvataggio
};

// Esegue (o carica dalla cache) la scansione. Bloccante: chiamarla da un task.
bool lib_scan(bool force);
LibProgress lib_progress();
LibState lib_state();
// Tag del file cambiati (pagina web): alla prossima scansione si rileggono
// anche se la dimensione del file è rimasta uguale.
void lib_mark_stale(const char *path);
void lib_mark_art_stale(uint32_t album_key);   // copertina nuova: rigenera la cache
// Dove salvare una copertina nuova per l'album (percorso completo sulla SD):
// cover.jpg nella sua cartella, oppure "<nome del primo brano>.jpg" se la cartella
// è condivisa con altri album (es. brani sparsi nella radice) o se l'album ne ha già una
std::string lib_album_cover_path(int album);

const std::vector<Track> &lib_tracks();
const std::vector<Album> &lib_albums();
const std::vector<Artist> &lib_artists();
const std::vector<Genre> &lib_genres();
const std::vector<Playlist> &lib_playlists();
const char *lib_track_path(int t);

int lib_album_by_key(uint32_t key);
int lib_artist_by_key(uint32_t key);
int lib_genre_by_key(uint32_t key);
int lib_playlist_by_key(uint32_t key);

// Brani di un contesto (album, artista, playlist, preferiti, genere, tutti)
void lib_context_tracks(const PlayContext &c, std::vector<uint16_t> &out);
PlayContext lib_ctx_album(int a);
PlayContext lib_ctx_artist(int a);
PlayContext lib_ctx_playlist(int p);
PlayContext lib_ctx_genre(int g);
PlayContext lib_ctx_liked();
PlayContext lib_ctx_all();
PlayContext lib_ctx_new();        // "Aggiunti di recente" (i brani degli ultimi album arrivati)
PlayContext lib_ctx_unplayed();   // "Mai ascoltati"
// Album dal più recente sulla scheda (ultima scansione in cui sono comparsi, poi data del file)
void lib_albums_by_added(std::vector<int> &out);
size_t lib_unplayed_count();
bool lib_ctx_valid(const PlayContext &c);

// Preferiti ("Brani che ti piacciono")
bool lib_is_liked(int t);
void lib_set_liked(int t, bool liked);
const std::vector<uint16_t> &lib_liked();   // dal più recente

// Statistiche e "Ascoltati di recente"
void lib_note_played(int t);
uint32_t lib_play_count(int t);
uint32_t lib_album_plays(int a);
void lib_recent_push(const PlayContext &c);
const std::vector<PlayContext> &lib_recents();
void lib_save_dirty();                       // salva su SD ciò che è cambiato

// Ricerca
struct SearchResult {
  std::vector<uint16_t> tracks, albums, artists, playlists;
};
void lib_search(const char *query, SearchResult &r, size_t max_tracks = 60);
void lib_fold(const char *in, char *out, size_t cap);   // minuscolo, senza accenti

// Utilità di formattazione
void fmt_duration(uint32_t ms, char *out, size_t cap);     // 3:07
void fmt_long_duration(uint32_t ms, char *out, size_t cap); // 1 h 12 min
