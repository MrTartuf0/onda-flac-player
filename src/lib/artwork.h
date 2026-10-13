// Copertine: decodifica JPEG (incorporato nel FLAC o cover.jpg nella cartella),
// cache su SD in due misure (160 e 64 px) e cache in RAM per l'interfaccia.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "lvgl.h"

constexpr int ART_LARGE = 160;
constexpr int ART_SMALL = 64;

struct Album;
struct Track;

// Genera i file in cache per un album (chiamato durante la scansione).
// Ritorna false se l'album non ha copertine utilizzabili.
bool art_build(const Album &a, uint16_t *color_out);
bool art_has_cache(uint32_t album_key);
void art_cache_path(uint32_t key, int size, char *out, size_t cap);

// Immagine dell'album alla misura richiesta (quadrata). nullptr = nessuna copertina.
// Ogni acquire va bilanciato da un release (gestito dai widget dell'interfaccia).
const lv_img_dsc_t *art_acquire(int album, int size);
void art_release(const lv_img_dsc_t *img);
void art_flush_unused();

// Ridimensionamento a media d'area (RGB565)
void art_resample(const uint16_t *src, int sw, int sh, uint16_t *dst, int dw, int dh);
uint16_t art_dominant_color(const uint16_t *px, int n);
