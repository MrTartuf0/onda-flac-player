// Traduzioni dell'interfaccia (italiano / inglese).
//
// Nei sorgenti i testi restano in italiano, racchiusi in TR("..."): l'hash del
// testo si calcola durante la compilazione e a runtime si cerca la traduzione
// nella tabella della lingua scelta (src/i18n_en.h, generata da
// tools/i18n/en.tsv con tools/i18n/i18n.py). Senza traduzione resta l'italiano.
#pragma once
#include <stdint.h>
#include <type_traits>

enum Lang : uint8_t { LANG_IT = 0, LANG_EN = 1 };

void i18n_set(Lang l);
Lang i18n_lang();
const char *i18n_tr(uint32_t hash, const char *it);
const char *i18n_tr_dyn(const char *it);   // testo non letterale (es. da una tabella)

constexpr uint32_t i18n_hash(const char *s, uint32_t h = 2166136261u) {
  return *s ? i18n_hash(s + 1, (h ^ (uint8_t)*s) * 16777619u) : h;
}

#define TR(s) i18n_tr(std::integral_constant<uint32_t, i18n_hash(s)>::value, s)
