#include "i18n.h"
#include <stddef.h>

namespace {
struct Entry {
  uint32_t h;
  const char *s;
};
#include "i18n_en.h"   // kEn[]: ordinata per hash
Lang s_lang = LANG_IT;
}  // namespace

void i18n_set(Lang l) { s_lang = l; }
Lang i18n_lang() { return s_lang; }

const char *i18n_tr(uint32_t h, const char *it) {
  if (s_lang == LANG_IT) return it;
  size_t lo = 0, hi = sizeof kEn / sizeof kEn[0];
  while (lo < hi) {
    size_t mid = (lo + hi) / 2;
    if (kEn[mid].h < h) lo = mid + 1;
    else hi = mid;
  }
  return lo < sizeof kEn / sizeof kEn[0] && kEn[lo].h == h ? kEn[lo].s : it;
}

const char *i18n_tr_dyn(const char *it) { return it ? i18n_tr(i18n_hash(it), it) : it; }
