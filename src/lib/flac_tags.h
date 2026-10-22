// Lettura completa e modifica dei metadati di un file FLAC (per la pagina web).
//
// flac_info legge tutto quello che serve a mostrare un file: formato (STREAMINFO),
// tutti i commenti Vorbis (i tag), le immagini incorporate e la struttura dei
// blocchi. flac_write_comments riscrive i tag: se nel file c'è spazio di riserva
// (il blocco PADDING che quasi tutti gli encoder lasciano) cambia solo l'intestazione,
// in un attimo; altrimenti ricopia il file con 8 KB di riserva per le volte dopo.
// flac_replace_picture fa lo stesso con un'immagine incorporata.
// L'audio non viene mai toccato (e dopo ogni scrittura si ricontrolla).
#pragma once
#include <stdint.h>
#include <string>
#include <vector>

struct FlacInfo {
  struct Block {
    uint8_t type;   // 0 STREAMINFO, 1 PADDING, 2 APPLICATION, 3 SEEKTABLE, 4 VORBIS_COMMENT, 5 CUESHEET, 6 PICTURE
    bool last;
    long off;       // posizione dell'intestazione del blocco (4 byte)
    uint32_t len;   // lunghezza dei dati (senza intestazione)
  };
  struct Picture {
    uint32_t type = 0;   // 3 = copertina frontale
    std::string mime, desc;
    uint32_t w = 0, h = 0, depth = 0;
    long data_off = 0;
    uint32_t data_len = 0;
    int block = -1;      // indice in blocks
  };
  long base = 0;          // dove inizia "fLaC" (dopo un eventuale tag ID3v2 non standard)
  long audio_off = 0;     // primo frame audio
  long file_size = 0;
  std::vector<Block> blocks;
  uint32_t sample_rate = 0;
  uint16_t min_block = 0, max_block = 0;
  uint8_t channels = 0, bits = 0;
  uint64_t total_samples = 0;
  uint8_t md5[16] = {0};
  std::string vendor;                  // encoder che ha scritto i tag
  std::vector<std::string> comments;   // "CHIAVE=valore", nell'ordine del file
  uint32_t skipped = 0;                // campi troppo grandi non letti (restano nel file)
  std::vector<Picture> pictures;
  uint32_t padding = 0;                // byte di riserva
};

// false se non è un FLAC leggibile. I campi più lunghi di max_field (es. testi
// enormi) non vengono letti ma vengono conservati da flac_write_comments.
bool flac_info(const char *path, FlacInfo &fi, uint32_t max_field = 32768);

// Sostituisce tutti i tag con `comments` ("CHIAVE=valore"). err: messaggio per
// l'utente. rewrote = il file è stato ricopiato (non c'era spazio di riserva).
bool flac_write_comments(const char *path, const std::vector<std::string> &comments, std::string &err,
                         bool *rewrote = nullptr, uint32_t max_field = 32768);

// L'immagine che il lettore usa come copertina (come flac_read_meta): la frontale
// (tipo 3), poi "altro" (0), poi la prima; -1 se non ce n'è.
int flac_cover_index(const FlacInfo &fi);

// Dimensioni di un JPEG che il lettore sa decodificare (baseline, 8 bit); false
// se non lo è (progressivo, PNG, dati rovinati...).
bool jpeg_baseline_size(const uint8_t *d, size_t n, uint32_t *w, uint32_t *h);

// Sostituisce l'immagine fi.pictures[index] con un JPEG baseline, tenendone tipo e
// descrizione (es. una copertina PNG che il lettore non legge, convertita dal browser).
bool flac_replace_picture(const char *path, int index, const uint8_t *jpeg, size_t len, std::string &err,
                          bool *rewrote = nullptr);
