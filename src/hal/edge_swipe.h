// Gesto "indietro" stile Android: trascinare il dito dal bordo sinistro verso
// l'interno. Il filtro sta tra il touch e LVGL: i tocchi che partono dal bordo
// vengono trattenuti finché non si capisce se sono un gesto (non arrivano a LVGL)
// oppure un tocco normale (vengono passati a LVGL, anche in ritardo).
#pragma once
#include <stdint.h>

struct EdgeSwipeHooks {
  void (*update)(int dx, int y) = nullptr;     // dito in movimento durante il gesto
  void (*end)(bool commit) = nullptr;          // dito sollevato: commit = torna indietro
  bool (*excluded)(int x, int y) = nullptr;    // zone dove il bordo non va intercettato
};

constexpr int EDGE_SWIPE_ZONE = 18;      // px dal bordo sinistro in cui parte il gesto
constexpr int EDGE_SWIPE_COMMIT = 48;    // px di trascinamento per confermare

void edge_swipe_set_hooks(const EdgeSwipeHooks &h);

// Da chiamare a ogni lettura del touch. In: dito giù + coordinate (valide solo se
// down). Out: stato e coordinate da passare a LVGL. Ritorna true = premuto.
bool edge_swipe_filter(bool down, int x, int y, int *out_x, int *out_y, uint32_t now_ms);
