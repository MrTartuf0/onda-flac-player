#include "edge_swipe.h"
#include <stdlib.h>
#include "../config.h"

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
bool s_bottom = false;  // il gesto in corso parte dal bordo in basso
int s_hy[4];            // ultime posizioni verticali (velocità della spinta)
uint32_t s_ht[4];
int s_hn = 0;
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

static void hist_add(int y, uint32_t now) {
  if (s_hn == 4) {
    for (int i = 1; i < 4; i++) {
      s_hy[i - 1] = s_hy[i];
      s_ht[i - 1] = s_ht[i];
    }
    s_hn = 3;
  }
  s_hy[s_hn] = y;
  s_ht[s_hn] = now;
  s_hn++;
}

// px percorsi verso l'alto negli ultimi ~100 ms
static int hist_speed(uint32_t now) {
  for (int i = 0; i < s_hn; i++)
    if (now - s_ht[i] <= 120) return s_hy[i] - s_hy[s_hn - 1];
  return 0;
}

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
      s_st = S_IDLE;
      if (s_bottom) {
        int dist = s_y0 - s_ly, v = hist_speed(now);
        if (s_hooks.bottom_end) s_hooks.bottom_end(dist >= EDGE_BOTTOM_COMMIT || (dist > 20 && v > 25), v);
      } else {
        bool commit = s_lx - s_x0 >= EDGE_SWIPE_COMMIT;
        if (s_hooks.end) s_hooks.end(commit);
      }
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
      s_bottom = false;
      if (x < EDGE_SWIPE_ZONE && !(s_hooks.excluded && s_hooks.excluded(x, y))) {
        s_st = S_EDGE;
        s_x0 = x;
        s_y0 = y;
        s_t0 = now;
        return released(ox, oy);
      }
      if (y >= LCD_H - EDGE_BOTTOM_ZONE && s_hooks.bottom_allowed && s_hooks.bottom_allowed()) {
        s_st = S_EDGE;
        s_bottom = true;
        s_x0 = x;
        s_y0 = y;
        s_t0 = now;
        s_hn = 0;
        hist_add(y, now);
        return released(ox, oy);
      }
      s_st = S_PASS;
      return pass(x, y, ox, oy);

    case S_EDGE: {
      int dx = x - s_x0, dy = y - s_y0;
      if (s_bottom) {
        hist_add(y, now);
        if (-dy >= kDecide && -dy > abs(dx)) {
          s_st = S_SWIPE;
          s_ly = y;
          if (s_hooks.bottom_update) s_hooks.bottom_update(-dy);
          return released(ox, oy);
        }
        if (abs(dx) >= kDecide || dy >= kDecide || now - s_t0 >= kHoldMs) {
          s_st = S_PASS;   // trascinamento orizzontale o pressione lunga: tocco normale
          return pass(x, y, ox, oy);
        }
        return released(ox, oy);
      }
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
      if (s_bottom) {
        hist_add(y, now);
        s_ly = y;
        if (s_hooks.bottom_update) s_hooks.bottom_update(s_y0 - y);
        return released(ox, oy);
      }
      s_lx = x;
      if (s_hooks.update) s_hooks.update(x - s_x0, y);
      return released(ox, oy);

    case S_PASS:
    default:
      return pass(x, y, ox, oy);
  }
}
