// Gesti dai bordi, intercettati tra il touch e LVGL:
//  - "indietro" stile Android: dal bordo sinistro verso l'interno;
//  - centro di controllo stile iPhone: dal bordo in basso verso l'alto.
// I tocchi che partono da un bordo vengono trattenuti finché non si capisce se
// sono un gesto (non arrivano a LVGL) oppure un tocco normale (vengono passati
// a LVGL, anche in ritardo).
#pragma once
#include <stdint.h>

struct EdgeSwipeHooks {
  // bordo sinistro
  void (*update)(int dx, int y) = nullptr;     // dito in movimento durante il gesto
  void (*end)(bool commit) = nullptr;          // dito sollevato: commit = torna indietro
  bool (*excluded)(int x, int y) = nullptr;    // zone dove il bordo non va intercettato
  // bordo in basso
  bool (*bottom_allowed)() = nullptr;          // false = gesto disattivato (es. già aperto)
  void (*bottom_update)(int dist) = nullptr;   // distanza percorsa verso l'alto
  void (*bottom_end)(bool commit, int speed) = nullptr;   // speed: px negli ultimi ~100 ms
};

constexpr int EDGE_SWIPE_ZONE = 18;      // px dal bordo sinistro in cui parte il gesto
constexpr int EDGE_SWIPE_COMMIT = 48;    // px di trascinamento per confermare
constexpr int EDGE_BOTTOM_ZONE = 18;     // px dal bordo in basso
constexpr int EDGE_BOTTOM_COMMIT = 70;   // px verso l'alto per aprire (o una spinta veloce)

void edge_swipe_set_hooks(const EdgeSwipeHooks &h);

// Da chiamare a ogni lettura del touch. In: dito giù + coordinate (valide solo se
// down). Out: stato e coordinate da passare a LVGL. Ritorna true = premuto.
bool edge_swipe_filter(bool down, int x, int y, int *out_x, int *out_y, uint32_t now_ms);
