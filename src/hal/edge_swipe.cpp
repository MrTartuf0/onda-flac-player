#include "edge_swipe.h"
#include <stdlib.h>

namespace {

EdgeSwipeHooks s_hooks;

enum State : uint8_t {
  S_IDLE,     // nessun dito
  S_PASS,     // tocco normale: tutto passa a LVGL
  S_EDGE,     // partito dal bordo, non ancora deciso
  S_SWIPE,    // gesto indietro in corso (LVGL non vede nulla)
  S_TAP1,     // tocco breve sul bordo: lo "riproduciamo" a LVGL (pressione...)
  S_TAP2,     // ...e rilascio alla lettura successiva
};

State s_st = S_IDLE;
int s_x0, s_y0;         // punto di partenza sul bordo
int s_lx, s_ly;         // ultimo punto passato a LVGL
uint32_t s_t0;

constexpr int kDecide = 10;        // px di movimento per decidere gesto/scroll
constexpr uint32_t kHoldMs = 350;  // fermo sul bordo così a lungo = pressione lunga normale

bool pass(int x, int y, int *ox, int *oy) {
  s_lx = x;
  s_ly = y;
  *ox = x;
  *oy = y;
  return true;
}

bool released(int *ox, int *oy) {
  *ox = s_lx;
  *oy = s_ly;
  return false;
}

}  // namespace

void edge_swipe_set_hooks(const EdgeSwipeHooks &h) { s_hooks = h; }

bool edge_swipe_filter(bool down, int x, int y, int *ox, int *oy, uint32_t now) {
  switch (s_st) {
    case S_TAP1:
      s_st = S_TAP2;
      return pass(s_x0, s_y0, ox, oy);
    case S_TAP2:
      s_st = S_IDLE;
      return released(ox, oy);
    default:
      break;
  }

  if (!down) {
    if (s_st == S_SWIPE) {
      bool commit = s_lx - s_x0 >= EDGE_SWIPE_COMMIT;
      s_st = S_IDLE;
      if (s_hooks.end) s_hooks.end(commit);
      return released(ox, oy);
    }
    if (s_st == S_EDGE) {
      // tocco breve sul bordo (es. la freccia "indietro" in alto a sinistra)
      s_st = S_TAP1;
      s_lx = s_x0;
      s_ly = s_y0;
      return released(ox, oy);
    }
    s_st = S_IDLE;
    return released(ox, oy);
  }

  switch (s_st) {
    case S_IDLE:
      if (x < EDGE_SWIPE_ZONE && !(s_hooks.excluded && s_hooks.excluded(x, y))) {
        s_st = S_EDGE;
        s_x0 = x;
        s_y0 = y;
        s_t0 = now;
        return released(ox, oy);
      }
      s_st = S_PASS;
      return pass(x, y, ox, oy);

    case S_EDGE: {
      int dx = x - s_x0, dy = y - s_y0;
      if (dx >= kDecide && dx > abs(dy)) {
        s_st = S_SWIPE;
        s_lx = x;
        if (s_hooks.update) s_hooks.update(dx, y);
        return released(ox, oy);
      }
      if (abs(dy) >= kDecide || now - s_t0 >= kHoldMs) {
        // scroll verticale o pressione lunga: è un tocco normale
        s_st = S_PASS;
        return pass(x, y, ox, oy);
      }
      return released(ox, oy);
    }

    case S_SWIPE:
      s_lx = x;
      if (s_hooks.update) s_hooks.update(x - s_x0, y);
      return released(ox, oy);

    case S_PASS:
    default:
      return pass(x, y, ox, oy);
  }
}
