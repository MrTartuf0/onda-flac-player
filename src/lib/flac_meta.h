// Lettura veloce dei metadati FLAC (STREAMINFO, VORBIS_COMMENT, PICTURE)
// senza decodificare l'audio. Codice C++ "puro": testabile anche su PC.
#pragma once
#include <stdint.h>
#include <stdio.h>

enum PicKind : uint8_t { PIC_NONE = 0, PIC_JPEG = 1, PIC_PNG = 2, PIC_OTHER = 3 };

struct FlacMeta {
  char title[128];
  char artist[128];
  char album[128];
  char albumartist[128];
  char genre[64];
  uint16_t track;
  uint16_t disc;
  uint16_t year;
  uint32_t sample_rate;
  uint8_t bits;
  uint8_t channels;
  uint64_t total_samples;
  uint32_t duration_ms;
  uint32_t pic_offset;     // offset dei dati immagine nel file
  uint32_t pic_size;
  uint8_t pic_kind;
};

bool flac_read_meta(const char *path, FlacMeta &m);
bool flac_read_meta_fp(FILE *f, FlacMeta &m);

// Utilità condivise
void utf8_copy(char *dst, size_t cap, const char *src, size_t len);
uint32_t fnv1a(const char *s, uint32_t h = 2166136261u);
uint32_t fnv1a_ci(const char *s, uint32_t h = 2166136261u);   // case-insensitive (ASCII)
