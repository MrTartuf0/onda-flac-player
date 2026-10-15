// Ricerca vocale: elenco dei nomi dalla libreria + riconoscimento con ESP-SR.
// Vedi voice.h. Sul PC (simulatore) c'è una finta implementazione a tempo.
#include "voice.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <vector>
#include <algorithm>
#include "../lib/library.h"
#include "../lib/flac_meta.h"
#include "../platform.h"
#include "../i18n.h"

#ifdef ARDUINO
#include <Arduino.h>
#include <esp_partition.h>
#include <esp_heap_caps.h>
// La libreria ESP_SR di Arduino non la usiamo (vuole il suo I2S e la wake word),
// ma includerla fa copiare srmodels.bin nella cartella di build: con lo schema
// "ESP SR 16M" l'IDE lo scrive da solo nella partizione "model".
#include <ESP_SR.h>
extern "C" {
#include "model_path.h"
#include "esp_mn_iface.h"
#include "esp_mn_models.h"
#include "esp_mn_speech_commands.h"
#include "flite_g2p.h"
}
#include "../audio/audio_out.h"
#include "../audio/player.h"
#endif

namespace {

struct Phrase {
  VoiceKind kind;
  uint32_t key;
  char text[64];
};

// ------------------------------------------------- testo dei nomi ----
const char *const kOnes[] = {"zero", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine",
                             "ten", "eleven", "twelve", "thirteen", "fourteen", "fifteen", "sixteen",
                             "seventeen", "eighteen", "nineteen"};
const char *const kTens[] = {"", "", "twenty", "thirty", "forty", "fifty", "sixty", "seventy", "eighty", "ninety"};

struct Out {
  char *p;
  size_t cap, n = 0;
  void word(const char *w) {
    if (n && p[n - 1] != ' ' && n + 1 < cap) p[n++] = ' ';
    while (*w && n + 1 < cap) p[n++] = *w++;
    p[n] = 0;
  }
};

void say_below_100(int v, Out &o) {
  if (v < 20) {
    o.word(kOnes[v]);
    return;
  }
  o.word(kTens[v / 10]);
  if (v % 10) o.word(kOnes[v % 10]);
}

void say_below_10000(int v, Out &o) {
  if (v >= 1000) {
    say_below_100(v / 1000, o);
    o.word("thousand");
    v %= 1000;
    if (!v) return;
  }
  if (v >= 100) {
    say_below_100(v / 100, o);
    o.word("hundred");
    v %= 100;
    if (!v) return;
  }
  say_below_100(v, o);
}

// "1984" → "nineteen eighty four", "2008" → "two thousand eight", "42" → "forty two",
// numeri lunghi cifra per cifra
void say_number(const char *d, int len, Out &o) {
  if (len > 4) {
    for (int i = 0; i < len; i++) o.word(kOnes[d[i] - '0']);
    return;
  }
  int v = 0;
  for (int i = 0; i < len; i++) v = v * 10 + (d[i] - '0');
  if (len == 4 && v >= 1100 && v < 2000 && v % 100) {   // anni: 19 84
    say_below_100(v / 100, o);
    if (v % 100 < 10) o.word("oh");
    say_below_100(v % 100, o);
  } else if (len == 4 && v >= 2010 && v < 2100) {      // 20 24
    say_below_100(v / 100, o);
    say_below_100(v % 100, o);
  } else {
    say_below_10000(v, o);
  }
}

// ------------------------------------------------ elenco dei nomi ----
struct Cand {
  VoiceKind kind;
  uint32_t key;
  const char *name;
  uint32_t score;
};

bool skip_name(const char *n) {
  return !n || !*n || !strcmp(n, TR("Artista sconosciuto")) || !strcmp(n, TR("Artisti vari")) ||
         !strcmp(n, TR("Brani singoli"));
}

// Chi entra se i nomi sono troppi: playlist e generi, poi artisti e album più
// ascoltati, poi i brani ascoltati o tra i preferiti.
std::vector<Phrase> build_table(uint32_t *sig) {
  std::vector<Cand> c;
  const auto &pl = lib_playlists();
  for (size_t i = 0; i < pl.size(); i++) c.push_back({VK_PLAYLIST, pl[i].key, pl[i].name, 3000000});
  const auto &ge = lib_genres();
  for (size_t i = 0; i < ge.size(); i++)
    c.push_back({VK_GENRE, ge[i].key, ge[i].name, 2000000 + (uint32_t)ge[i].tracks.size()});
  const auto &al = lib_albums();
  std::vector<uint32_t> aplays(al.size());
  for (size_t i = 0; i < al.size(); i++) aplays[i] = lib_album_plays(i);
  const auto &ar = lib_artists();
  for (size_t i = 0; i < ar.size(); i++) {
    uint32_t p = 0;
    for (auto a : ar[i].albums) p += aplays[a];
    c.push_back({VK_ARTIST, ar[i].key, ar[i].name, 1000000 + p * 4});
  }
  for (size_t i = 0; i < al.size(); i++) c.push_back({VK_ALBUM, al[i].key, al[i].title, 900000 + aplays[i] * 4});
  const auto &tr = lib_tracks();
  for (size_t i = 0; i < tr.size(); i++) {
    uint32_t p = lib_play_count(i);
    bool liked = lib_is_liked(i);
    if (!p && !liked) continue;
    c.push_back({VK_TRACK, tr[i].path_hash, tr[i].title, 100000 + p * 20 + (liked ? 200 : 0)});
  }
  std::stable_sort(c.begin(), c.end(), [](const Cand &a, const Cand &b) { return a.score > b.score; });

  std::vector<Phrase> out;
  std::vector<uint32_t> seen;
  uint32_t h = 2166136261u;
  for (auto &x : c) {
    if ((int)out.size() >= kVoiceMaxPhrases) break;
    if (skip_name(x.name)) continue;
    Phrase p;
    p.kind = x.kind;
    p.key = x.key;
    if (!voice_phrase_text(x.name, x.kind == VK_TRACK || x.kind == VK_ALBUM, p.text, sizeof p.text)) continue;
    uint32_t th = fnv1a(p.text);
    if (std::find(seen.begin(), seen.end(), th) != seen.end()) continue;   // stesso nome: vince il primo
    seen.push_back(th);
    out.push_back(p);
    h = fnv1a(p.text, h ^ (x.kind * 31 + x.key));
  }
  if (sig) *sig = h | 1;
  return out;
}

int resolve(VoiceKind k, uint32_t key) {
  switch (k) {
    case VK_ARTIST: return lib_artist_by_key(key);
    case VK_ALBUM: return lib_album_by_key(key);
    case VK_PLAYLIST: return lib_playlist_by_key(key);
    case VK_GENRE: return lib_genre_by_key(key);
    case VK_TRACK: {
      const auto &t = lib_tracks();
      for (size_t i = 0; i < t.size(); i++)
        if (t[i].path_hash == key) return i;
      return -1;
    }
  }
  return -1;
}

const char *kind_name(VoiceKind k) {
  static const char *const n[] = {"artista", "album", "playlist", "genere", "brano"};
  return n[k];
}

bool s_dirty = true;        // la libreria è cambiata: ricostruire i nomi
uint32_t s_sent_sig = 0;    // firma dell'ultimo elenco mandato al riconoscitore
uint32_t s_built_ms = 0;
int s_phrases = 0;

// I brani ascoltati entrano nell'elenco: ogni tanto si ricalcola anche senza
// riscansione (se non cambia niente il modello non si ricarica).
bool need_rebuild() { return s_dirty || !s_sent_sig || plat_millis() - s_built_ms > 15 * 60 * 1000; }

void resolve_all(VoiceStatus &s) {
  for (int i = 0; i < s.n; i++) s.m[i].index = resolve(s.m[i].kind, s.m[i].key);
  // tolti quelli che non esistono più (libreria cambiata nel frattempo)
  int w = 0;
  for (int i = 0; i < s.n; i++)
    if (s.m[i].index >= 0) s.m[w++] = s.m[i];
  s.n = w;
  s.phrases = s_phrases;
}

}  // namespace

bool voice_phrase_text(const char *name, bool trim_extras, char *out, size_t cap) {
  char f[160];
  lib_fold(name, f, sizeof f);
  if (trim_extras) {
    // "Titolo (Live)", "Titolo [Remastered]", "Titolo - 2011 Remaster", "Titolo feat. X"
    static const char *const cuts[] = {" (", "(", " [", "[", " - ", " feat", " ft.", " featuring "};
    for (const char *k : cuts) {
      char *p = strstr(f, k);
      if (p && p - f >= 2) *p = 0;
    }
  }
  Out o{out, cap};
  out[0] = 0;
  const char *s = f;
  while (*s) {
    unsigned char ch = *s;
    if (ch >= 'a' && ch <= 'z') {
      char w[64];
      int n = 0;
      while (((*s >= 'a' && *s <= 'z') || *s == '\'') && n < 63) {
        if (*s != '\'') w[n++] = *s;
        s++;
      }
      w[n] = 0;
      o.word(w);
    } else if (ch >= '0' && ch <= '9') {
      const char *d = s;
      while (*s >= '0' && *s <= '9') s++;
      say_number(d, s - d, o);
    } else {
      if (ch == '&') o.word("and");
      else if (ch == '+') o.word("plus");
      s++;
    }
  }
  // massimo 63 caratteri, tagliando su una parola intera
  size_t n = strlen(out);
  if (n > 63) {
    out[63] = 0;
    char *sp = strrchr(out, ' ');
    if (sp) *sp = 0;
  }
  int letters = 0;
  for (const char *p = out; *p; p++) letters += (*p >= 'a' && *p <= 'z');
  return letters >= 2;
}

void voice_library_changed() { s_dirty = true; }

#ifdef ARDUINO
// =========================================================== ESP32-S3 ====
namespace {

portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
VoiceStatus s_st;                           // protetto da s_mux
std::vector<Phrase> *s_pending = nullptr;   // nuovo elenco per il task (s_mux)
TaskHandle_t s_task = nullptr;
volatile bool s_busy = false;               // il task sta lavorando a una richiesta
volatile bool s_cancel = false, s_stop = false, s_quit = false;
volatile bool s_reload = false;            // il task ha liberato il modello: rimandare i nomi
int s_supported = -1;

// --- solo nel task ---
srmodel_list_t *s_models = nullptr;
const esp_mn_iface_t *s_mn = nullptr;
model_iface_data_t *s_md = nullptr;
bool s_mn5 = false;
int s_chunk = 0;
std::vector<Phrase> s_table;
int16_t s_buf[1024];

void set_state(VoiceState st, const char *err = nullptr) {
  portENTER_CRITICAL(&s_mux);
  s_st.state = st;
  if (err) s_st.error = err;
  portEXIT_CRITICAL(&s_mux);
}

void set_level(int lv) {
  portENTER_CRITICAL(&s_mux);
  s_st.level = lv;
  portEXIT_CRITICAL(&s_mux);
}

unsigned kb_internal() { return heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024; }
unsigned kb_psram() { return heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024; }

void unload() {
  if (s_md) {
    esp_mn_commands_free();
    s_mn->destroy(s_md);
    s_md = nullptr;
  }
  if (s_models) {
    esp_srmodel_deinit(s_models);
    s_models = nullptr;
  }
  s_table.clear();
  s_table.shrink_to_fit();
}

bool load() {
  uint32_t t0 = millis();
  unsigned in0 = kb_internal(), ps0 = kb_psram();
  s_models = esp_srmodel_init("model");
  if (!s_models || s_models->num <= 0) {
    plat_log("[voice] nessun modello nella partizione \"model\"");
    set_state(VOICE_ERROR, TR("Modello vocale mancante: carica il firmware con lo schema \"ESP SR 16M\""));
    if (s_models) esp_srmodel_deinit(s_models);
    s_models = nullptr;
    return false;
  }
  char *name = esp_srmodel_filter(s_models, ESP_MN_PREFIX, ESP_MN_ENGLISH);
  if (!name) {
    plat_log("[voice] nessun modello MultiNet inglese tra %d modelli", s_models->num);
    set_state(VOICE_ERROR, TR("Modello vocale inglese non trovato"));
    unload();
    return false;
  }
  s_mn = esp_mn_handle_from_name(name);
  s_md = s_mn ? s_mn->create(name, 6000) : nullptr;
  if (!s_md) {
    plat_log("[voice] %s: creazione fallita (RAM interna %u KB, PSRAM %u KB)", name, kb_internal(), kb_psram());
    set_state(VOICE_ERROR, TR("Memoria insufficiente per la ricerca vocale"));
    unload();
    return false;
  }
  s_chunk = s_mn->get_samp_chunksize(s_md);
  int rate = s_mn->get_samp_rate(s_md);
  if (s_chunk <= 0 || s_chunk > (int)(sizeof s_buf / 2) || rate != 16000) {
    plat_log("[voice] %s: blocco %d campioni a %d Hz non gestito", name, s_chunk, rate);
    set_state(VOICE_ERROR, TR("Modello vocale non compatibile"));
    unload();
    return false;
  }
  esp_mn_commands_alloc((esp_mn_iface_t *)s_mn, s_md);
  s_mn5 = !strncmp(name, "mn5", 3);
  plat_log("[voice] modello %s pronto in %u ms (blocco %d). RAM interna %u -> %u KB, PSRAM %u -> %u KB", name,
           (unsigned)(millis() - t0), s_chunk, in0, kb_internal(), ps0, kb_psram());
  return true;
}

void add_commands(std::vector<Phrase> &tab) {
  uint32_t t0 = millis();
  esp_mn_commands_clear();
  int added = 0, nog2p = 0;
  for (size_t i = 0; i < tab.size(); i++) {
    char *ph = flite_g2p(tab[i].text, 1);
    if (!ph) {
      nog2p++;
      continue;
    }
    // MultiNet 5 (core 3.2) vuole i fonemi come testo del comando; dal 6 in poi
    // testo e fonemi separati
    esp_err_t e = s_mn5 ? esp_mn_commands_add(i, ph) : esp_mn_commands_phoneme_add(i, tab[i].text, ph);
    free(ph);
    if (e == ESP_OK) added++;
    if ((i & 15) == 15) vTaskDelay(1);
  }
  esp_mn_error_t *err = esp_mn_commands_update();
  int rej = err ? err->num : 0;
  plat_log("[voice] %d nomi pronti in %u ms (senza fonemi %d, rifiutati %d)", added - rej,
           (unsigned)(millis() - t0), nog2p, rej);
  for (int i = 0; err && i < err->num && i < 5; i++)
    if (err->phrases[i] && err->phrases[i]->string) plat_log("[voice]   rifiutato: %s", err->phrases[i]->string);
  portENTER_CRITICAL(&s_mux);
  s_st.phrases = added - rej;
  portEXIT_CRITICAL(&s_mux);
}

int level_of(const int16_t *b, int n, int *peak_out) {
  int peak = 0;
  for (int i = 0; i < n; i++) {
    int v = b[i] < 0 ? -b[i] : b[i];
    if (v > peak) peak = v;
  }
  *peak_out = peak;
  if (peak < 33) return 0;
  float db = 20.f * log10f(peak / 32768.f);   // -60..0 dB → 0..100
  int lv = (int)((db + 60.f) * 100.f / 60.f);
  return lv < 0 ? 0 : (lv > 100 ? 100 : lv);
}

void listen() {
  s_mn->clean(s_md);
  // i primi ~100 ms dopo l'accensione del microfono sono un "tump": via
  for (int i = 0; i < 3; i++) audio_out_mic_read(s_buf, s_chunk, 200);
  set_state(VOICE_LISTENING);
  uint32_t t0 = millis();
  esp_mn_state_t r = ESP_MN_STATE_DETECTING;
  int extra = 0, peak_all = 0, short_reads = 0;
  while (!s_cancel) {
    if (s_stop) {
      memset(s_buf, 0, s_chunk * 2);   // "fine frase": silenzio finché decide
    } else {
      size_t n = audio_out_mic_read(s_buf, s_chunk, 200);
      if ((int)n < s_chunk) {
        memset(s_buf + n, 0, (s_chunk - n) * 2);
        if (++short_reads > 20) {
          plat_log("[voice] il microfono non manda dati");
          set_state(VOICE_ERROR, TR("Il microfono non risponde"));
          return;
        }
      }
      int peak;
      set_level(level_of(s_buf, s_chunk, &peak));
      if (peak > peak_all) peak_all = peak;
    }
    r = s_mn->detect(s_md, s_buf);
    if (r != ESP_MN_STATE_DETECTING) break;
    if (s_stop && ++extra > 40) break;
    if (millis() - t0 > 8000) break;
  }
  set_level(0);
  if (s_cancel) {
    plat_log("[voice] annullato");
    set_state(VOICE_IDLE);
    return;
  }
  esp_mn_results_t *res = s_mn->get_results(s_md);
  VoiceStatus st;
  int num = res ? res->num : 0;
  plat_log("[voice] %s dopo %u ms, picco microfono %d/32767, %d candidati",
           r == ESP_MN_STATE_DETECTED ? "riconosciuto" : (r == ESP_MN_STATE_TIMEOUT ? "tempo scaduto" : "fermato"),
           (unsigned)(millis() - t0), peak_all, num);
  for (int i = 0; i < num && st.n < 3; i++) {
    int id = res->command_id[i];
    if (id < 0 || id >= (int)s_table.size()) continue;
    bool dup = false;
    for (int k = 0; k < st.n; k++) dup |= st.m[k].key == s_table[id].key && st.m[k].kind == s_table[id].kind;
    plat_log("[voice]   %d. \"%s\" (%s) %.2f%s", i + 1, s_table[id].text, kind_name(s_table[id].kind), res->prob[i],
             dup ? " (doppio)" : "");
    if (dup) continue;
    st.m[st.n++] = {s_table[id].kind, s_table[id].key, -1, res->prob[i]};
  }
  portENTER_CRITICAL(&s_mux);
  st.phrases = s_st.phrases;
  s_st = st;
  s_st.state = VOICE_DONE;
  portEXIT_CRITICAL(&s_mux);
}

void session() {
  std::vector<Phrase> *nt;
  portENTER_CRITICAL(&s_mux);
  nt = s_pending;
  s_pending = nullptr;
  portEXIT_CRITICAL(&s_mux);
  if (!s_md || nt) set_state(VOICE_LOADING);
  if (!s_md && !load()) {
    s_reload = true;   // i nomi non sono entrati: al prossimo tentativo si rimandano
    delete nt;
    return;
  }
  if (nt) {
    s_table.swap(*nt);
    delete nt;
    add_commands(s_table);
  }
  if (s_cancel) {
    set_state(VOICE_IDLE);
    return;
  }
  // la musica l'ha già messa in pausa l'interfaccia: aspetta la dissolvenza
  for (int i = 0; i < 50 && player_status().playing; i++) vTaskDelay(pdMS_TO_TICKS(10));
  player_hold_output(true);
  if (!audio_out_mic_begin(16000)) {
    plat_log("[voice] I2S in ricezione non disponibile");
    audio_out_resume();
    player_hold_output(false);
    set_state(VOICE_ERROR, TR("Il microfono non risponde"));
    return;
  }
  listen();
  audio_out_mic_end();
  if (!audio_out_resume()) {
    // l'uscita non riparte (RAM interna?): via il modello e riprova
    plat_log("[voice] uscita audio non ripartita (RAM interna %u KB): libero il modello", kb_internal());
    unload();
    s_reload = true;
    portENTER_CRITICAL(&s_mux);
    s_st.phrases = 0;
    portEXIT_CRITICAL(&s_mux);
    audio_out_resume();
  }
  player_hold_output(false);
}

void task(void *) {
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    if (s_quit) break;
    session();
    plat_log("[voice] RAM interna libera %u KB, stack libero %u B", kb_internal(),
             (unsigned)uxTaskGetStackHighWaterMark(nullptr));
    s_busy = false;
  }
  unload();
  plat_log("[voice] modello liberato: RAM interna %u KB, PSRAM %u KB", kb_internal(), kb_psram());
  s_busy = false;
  s_task = nullptr;
  vTaskDelete(nullptr);
}

}  // namespace

bool voice_supported() {
  if (s_supported < 0) {
    const esp_partition_t *p = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "model");
    s_supported = p && psramFound();
    plat_log("[voice] partizione \"model\" %s", p ? "presente: ricerca vocale disponibile" : "assente: niente ricerca vocale");
  }
  return s_supported == 1;
}

void voice_listen() {
  if (!voice_supported()) return;
  if (s_busy) {
    portENTER_CRITICAL(&s_mux);
    VoiceState st = s_st.state;
    portEXIT_CRITICAL(&s_mux);
    if ((st == VOICE_LOADING || st == VOICE_LISTENING) && !s_cancel) return;   // sta già ascoltando
    for (int i = 0; i < 100 && s_busy; i++) vTaskDelay(pdMS_TO_TICKS(10));
    if (s_busy) return;
  }
  if (s_reload) {   // modello ricaricato da zero: servono di nuovo i nomi
    s_reload = false;
    s_sent_sig = 0;
  }
  if (need_rebuild()) {
    uint32_t t0 = millis(), sig = 0;
    std::vector<Phrase> tab = build_table(&sig);
    s_dirty = false;
    s_built_ms = plat_millis();
    s_phrases = tab.size();
    plat_log("[voice] %d nomi dalla libreria in %u ms%s", (int)tab.size(), (unsigned)(millis() - t0),
             sig == s_sent_sig ? " (invariati)" : "");
    if (sig != s_sent_sig) {
      auto *nt = new std::vector<Phrase>();
      nt->swap(tab);
      portENTER_CRITICAL(&s_mux);
      delete s_pending;   // mai usato: sostituito
      s_pending = nt;
      portEXIT_CRITICAL(&s_mux);
      s_sent_sig = sig;
    }
  }
  s_cancel = s_stop = s_quit = false;
  portENTER_CRITICAL(&s_mux);
  int ph = s_st.phrases;
  s_st = VoiceStatus();
  s_st.state = VOICE_LOADING;
  s_st.phrases = ph;
  portEXIT_CRITICAL(&s_mux);
  if (!s_task && xTaskCreatePinnedToCore(task, "voice", 12 * 1024, nullptr, 1, &s_task, 1) != pdPASS) {
    s_task = nullptr;
    set_state(VOICE_ERROR, TR("Memoria insufficiente per la ricerca vocale"));
    return;
  }
  s_busy = true;
  xTaskNotifyGive(s_task);
}

void voice_stop() { s_stop = true; }

void voice_cancel() {
  if (s_busy) s_cancel = true;
  else set_state(VOICE_IDLE);
}

VoiceStatus voice_status() {
  portENTER_CRITICAL(&s_mux);
  VoiceStatus s = s_st;
  portEXIT_CRITICAL(&s_mux);
  resolve_all(s);
  return s;
}

void voice_release() {
  if (!s_task) return;
  s_cancel = true;
  s_quit = true;
  for (int i = 0; i < 300 && s_busy; i++) vTaskDelay(pdMS_TO_TICKS(10));
  xTaskNotifyGive(s_task);
  for (int i = 0; i < 300 && s_task; i++) vTaskDelay(pdMS_TO_TICKS(10));
  s_sent_sig = 0;   // al prossimo ascolto i nomi vanno ricaricati nel modello
  portENTER_CRITICAL(&s_mux);
  delete s_pending;
  s_pending = nullptr;
  s_st = VoiceStatus();
  portEXIT_CRITICAL(&s_mux);
}

#else
// ============================================== simulatore (PC) ====
// SIM_VOICE: "none" = non capisce, "err" = errore, "sure" = sicuro (apre da
// solo), altrimenti tre candidati incerti.
namespace {
uint32_t s_t0 = 0;
bool s_active = false, s_loaded = false, s_cancelled = false;
std::vector<Phrase> s_table;
}  // namespace

bool voice_supported() { return true; }

void voice_listen() {
  if (need_rebuild()) {
    s_table = build_table(&s_sent_sig);
    s_phrases = s_table.size();
    s_dirty = false;
    s_built_ms = plat_millis();
    plat_log("[voice] %d nomi:", (int)s_table.size());
    for (size_t i = 0; i < s_table.size() && i < 12; i++)
      plat_log("[voice]   %s: \"%s\"", kind_name(s_table[i].kind), s_table[i].text);
  }
  s_t0 = plat_millis();
  s_active = true;
  s_cancelled = false;
}

void voice_stop() {
  if (s_active) s_t0 = plat_millis() - 100000;
}

void voice_cancel() {
  s_active = false;
  s_cancelled = true;
}

VoiceStatus voice_status() {
  VoiceStatus s;
  s.phrases = s_phrases;
  if (!s_active) return s;
  uint32_t load = s_loaded ? 150 : 1200;
  uint32_t el = plat_millis() - s_t0;
  const char *mode = getenv("SIM_VOICE");
  mode = mode ? mode : "";
  if (el < load) {
    s.state = VOICE_LOADING;
    return s;
  }
  s_loaded = true;
  if (el < load + 2600) {
    s.state = VOICE_LISTENING;
    float t = (el - load) / 1000.f;
    s.level = t < 0.5f ? 8 : (int)(55 + 35 * sinf(t * 9.f) * sinf(t * 2.3f));
    if (s.level < 5) s.level = 5;
    return s;
  }
  if (!strcmp(mode, "err")) {
    s.state = VOICE_ERROR;
    s.error = TR("Il microfono non risponde");
    return s;
  }
  s.state = VOICE_DONE;
  if (strcmp(mode, "none")) {
    static const VoiceKind want[3] = {VK_ALBUM, VK_ARTIST, VK_TRACK};
    bool sure = !strcmp(mode, "sure");
    float pr[3] = {sure ? 0.93f : 0.48f, sure ? 0.03f : 0.31f, 0.09f};
    for (int k = 0; k < 3; k++)
      for (auto &p : s_table)
        if (p.kind == want[k]) {
          s.m[s.n++] = {p.kind, p.key, -1, pr[k]};
          break;
        }
  }
  resolve_all(s);
  return s;
}

void voice_release() {}
#endif
