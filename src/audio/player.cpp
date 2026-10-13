// Motore di riproduzione.
//
//   [task "flac"]  decodifica FLAC  ──►  buffer PCM in PSRAM (~1.5-3 s)  ──►  [task "i2s"]  volume + I2S
//
// Il buffer assorbe le pause della SD, dell'interfaccia e delle scritture in flash:
// il task di uscita (alta priorità) ha sempre dati pronti. Il decoder lavora "in
// anticipo" su qualunque core sia libero.
#pragma GCC optimize("O2")
#include <Arduino.h>
#include "player.h"
#include <deque>
#include <algorithm>
#include <esp_heap_caps.h>
#include <esp_random.h>
#include <esp_timer.h>
#include <string.h>
#include "audio_out.h"
#include "dr_flac.h"
#include "../platform.h"

namespace {

constexpr int kFrames = 512;               // frame per blocco di decodifica
constexpr int kOutFrames = 256;            // frame per scrittura I2S
constexpr uint32_t kRingFrames = 131072;   // 1 MB: ~3 s a 44.1 kHz, ~1.4 s a 96 kHz

enum CmdType : uint8_t { C_PLAY, C_TOGGLE, C_PAUSE, C_RESUME, C_NEXT, C_PREV, C_SEEK, C_SHUFFLE,
                         C_REPEAT, C_JUMP, C_STOP };
struct Cmd {
  CmdType t;
  int32_t a;
  uint32_t b;
};

QueueHandle_t s_q;
SemaphoreHandle_t s_mtx;
TrackPathFn s_resolve;

// ---- stato condiviso (protetto da s_mtx) ----
std::vector<uint16_t> s_base;     // brani del contesto (ordine originale)
std::vector<uint16_t> s_order;    // indici in s_base nell'ordine di riproduzione
int s_pos = -1;                   // posizione corrente in s_order
std::deque<uint16_t> s_manual;    // coda manuale ("In coda")
PlayContext s_ctx;
bool s_shuffle = false;
RepeatMode s_repeat = REPEAT_OFF;
uint32_t s_qver = 0;
PlayerStatus s_st;
volatile uint32_t s_events = 0;
portMUX_TYPE s_evmux = portMUX_INITIALIZER_UNLOCKED;

// Cambio brano "senza pause": quando il task di uscita raggiunge il frame `at`
// del buffer, il brano mostrato diventa quello del marker.
struct Marker {
  uint32_t at;
  int track;
  uint32_t dur_ms, rate, start_ms;
  uint8_t bits, ch;
  int pos;          // s_pos del brano (posizione nella coda)
  int manual;       // brano preso dalla coda manuale, o -1
};
std::deque<Marker> s_markers;
uint32_t s_base_total = 0;        // frame del buffer in cui inizia la posizione s_base_ms
uint32_t s_base_ms = 0;
uint32_t s_rate_now = 44100;      // frequenza dei campioni in uscita
int s_play_pos = -1;              // s_pos del brano che si sta ascoltando (il decoder può essere avanti)

// ---- buffer PCM (un solo scrittore: decoder; un solo lettore: uscita) ----
int32_t *s_ring = nullptr;
volatile uint32_t s_wr = 0, s_rd = 0;   // contatori assoluti di frame
volatile uint32_t s_gen = 0;            // incrementato a ogni svuotamento
portMUX_TYPE s_ringmux = portMUX_INITIALIZER_UNLOCKED;
volatile bool s_hold = false;           // il decoder chiede all'uscita di fermarsi
volatile bool s_out_idle = true;        // l'uscita è ferma (nessuna scrittura I2S in corso)
volatile bool s_fade_req = false;       // dissolvenza verso la pausa
volatile uint32_t s_gaps = 0;           // buffer vuoto durante la riproduzione

// ---- stato del decoder ----
FILE *s_fp = nullptr;
char *s_fbuf = nullptr;
drflac *s_fl = nullptr;
uint32_t s_src_rate = 0;
int s_decim = 1;
int32_t *s_tmp = nullptr;         // campioni decodificati (interleaved)
int32_t *s_mix = nullptr;         // stereo per il buffer
int32_t *s_out = nullptr;         // blocco per l'I2S (RAM interna)

SemaphoreHandle_t mtx() {
  if (!s_mtx) s_mtx = xSemaphoreCreateMutex();
  return s_mtx;
}

struct Lock {
  Lock() { xSemaphoreTake(mtx(), portMAX_DELAY); }
  ~Lock() { xSemaphoreGive(s_mtx); }
};

void post_event(uint32_t e) {
  portENTER_CRITICAL(&s_evmux);
  s_events |= e;
  portEXIT_CRITICAL(&s_evmux);
}

void send(CmdType t, int32_t a = 0, uint32_t b = 0) {
  if (!s_q) return;   // player non ancora avviato
  Cmd c{t, a, b};
  xQueueSend(s_q, &c, pdMS_TO_TICKS(100));
}

// ------------------------------------------------------------- buffer ----
uint32_t ring_fill() { return s_wr - s_rd; }
uint32_t ring_free() { return kRingFrames - ring_fill(); }

void ring_write(const int32_t *src, uint32_t n) {
  uint32_t w = s_wr % kRingFrames;
  uint32_t first = std::min(n, kRingFrames - w);
  memcpy(s_ring + w * 2, src, first * 8);
  if (n > first) memcpy(s_ring, src + first * 2, (n - first) * 8);
  portENTER_CRITICAL(&s_ringmux);
  s_wr += n;
  portEXIT_CRITICAL(&s_ringmux);
}

// Svuota il buffer (seek, cambio brano manuale, stop): l'audio cambia subito.
void ring_flush() {
  portENTER_CRITICAL(&s_ringmux);
  s_rd = s_wr;
  s_gen++;
  portEXIT_CRITICAL(&s_ringmux);
  Lock l;
  s_markers.clear();
}

// Il decoder può aver già aperto il brano successivo: prima di un comando
// dell'utente (successivo, precedente, salto) si torna a ciò che si sta ascoltando.
void rewind_lookahead() {
  Lock l;
  for (auto it = s_markers.rbegin(); it != s_markers.rend(); ++it)
    if (it->manual >= 0) s_manual.push_front(it->manual);
  if (!s_markers.empty()) s_pos = s_play_pos;
  s_markers.clear();
}

// Ferma il task di uscita (serve per cambiare frequenza di campionamento).
void out_hold(bool h) {
  s_hold = h;
  if (h) {
    for (int i = 0; i < 200 && !s_out_idle; i++) vTaskDelay(1);
  }
}

// Aspetta che l'uscita abbia suonato tutto il buffer (fine coda, cambio frequenza).
void ring_drain(uint32_t max_ms) {
  uint32_t t0 = millis();
  while (ring_fill() > 0 && millis() - t0 < max_ms) {
    bool playing;
    {
      Lock l;
      playing = s_st.playing;
    }
    if (!playing) break;
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// ---------------------------------------------- allocazioni di dr_flac ----
// Lo stato del decoder (cache di lettura + campioni del blocco) è usato a ogni
// campione: in RAM interna se c'è spazio, altrimenti in PSRAM.
void *fl_malloc(size_t sz, void *) {
  void *p = nullptr;
  if (heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL) > sz + 48 * 1024)
    p = heap_caps_malloc(sz, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  return p ? p : heap_caps_malloc(sz, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}
void *fl_realloc(void *p, size_t sz, void *) { return heap_caps_realloc(p, sz, MALLOC_CAP_8BIT); }
void fl_free(void *p, void *) { heap_caps_free(p); }
const drflac_allocation_callbacks kFlAlloc = {nullptr, fl_malloc, fl_realloc, fl_free};

// ----------------------------------------------------- callback dr_flac ----
uint64_t s_io_us = 0;   // tempo passato a leggere dalla SD
size_t cb_read(void *u, void *out, size_t n) {
  int64_t t = esp_timer_get_time();
  size_t r = fread(out, 1, n, (FILE *)u);
  s_io_us += esp_timer_get_time() - t;
  return r;
}
drflac_bool32 cb_seek(void *u, int off, drflac_seek_origin o) {
  int wh = o == DRFLAC_SEEK_SET ? SEEK_SET : (o == DRFLAC_SEEK_CUR ? SEEK_CUR : SEEK_END);
  return fseek((FILE *)u, off, wh) == 0;
}
drflac_bool32 cb_tell(void *u, drflac_int64 *cur) {
  long p = ftell((FILE *)u);
  if (p < 0) return DRFLAC_FALSE;
  *cur = p;
  return DRFLAC_TRUE;
}

void close_track() {
  if (s_fl) drflac_close(s_fl);
  s_fl = nullptr;
  if (s_fp) fclose(s_fp);
  s_fp = nullptr;
}

// Imposta subito il brano mostrato e l'origine della posizione.
void apply_now_locked(int track, bool ok, uint32_t start_ms) {
  s_play_pos = s_pos;
  s_st.track = track;
  s_st.pos_ms = start_ms;
  s_st.error = !ok;
  s_base_total = s_wr;
  s_base_ms = start_ms;
  if (ok) {
    s_st.dur_ms = (uint32_t)(s_fl->totalPCMFrameCount * 1000ULL / s_src_rate);
    s_st.sample_rate = s_src_rate;
    s_st.bits = s_fl->bitsPerSample;
    s_st.channels = s_fl->channels;
  } else {
    s_st.dur_ms = 0;
  }
}

// deferred = passaggio naturale al brano successivo: il brano mostrato cambia
// quando l'audio arriva davvero lì (riproduzione senza pause tra i brani).
bool open_track(int track, bool deferred = false, int manual = -1) {
  close_track();
  const char *path = s_resolve ? s_resolve(track) : nullptr;
  plat_mark(2, "player: apertura", path ? path : "?");
  bool ok = false;
  if (path) {
    s_fp = fopen(path, "rb");
    if (s_fp) {
      setvbuf(s_fp, s_fbuf, _IOFBF, 16 * 1024);
      s_fl = drflac_open(cb_read, cb_seek, cb_tell, s_fp, &kFlAlloc);
    }
    if (s_fl && s_fl->channels >= 1 && s_fl->channels <= 8) {
      s_src_rate = s_fl->sampleRate;
      s_decim = 1;
      while (s_src_rate / s_decim > audio_out_max_rate() && s_decim < 8) s_decim *= 2;
      uint32_t out_rate = s_src_rate / s_decim;
      if (out_rate != s_rate_now) {
        // frequenza diversa: prima si suona tutto ciò che è nel buffer
        if (deferred) ring_drain(4000);
        ring_flush();
        out_hold(true);
        ok = audio_out_set_rate(out_rate);
        out_hold(false);
        if (ok) s_rate_now = out_rate;
        deferred = false;
      } else {
        ok = true;
      }
    }
  }
  if (deferred && ok) {
    Lock l;
    Marker m{s_wr, track, (uint32_t)(s_fl->totalPCMFrameCount * 1000ULL / s_src_rate), s_src_rate, 0,
             (uint8_t)s_fl->bitsPerSample, (uint8_t)s_fl->channels, s_pos, manual};
    s_markers.push_back(m);
  } else {
    {
      Lock l;
      apply_now_locked(track, ok, 0);
    }
    post_event(PLAYER_EV_TRACK | (ok ? 0 : PLAYER_EV_ERROR));
  }
  if (!ok) close_track();
  return ok;
}

void set_playing(bool p) {
  {
    Lock l;
    if (s_st.playing == p) return;
    s_st.playing = p;
  }
  if (p) audio_out_clear_mute();
  post_event(PLAYER_EV_STATE);
}

bool is_playing() {
  Lock l;
  return s_st.playing;
}

void build_order_locked(int start_base_idx) {
  size_t n = s_base.size();
  s_order.resize(n);
  for (size_t i = 0; i < n; i++) s_order[i] = i;
  if (n == 0) {
    s_pos = -1;
    return;
  }
  if (start_base_idx < 0 || start_base_idx >= (int)n) start_base_idx = 0;
  if (s_shuffle) {
    for (size_t i = n - 1; i > 0; i--) {
      size_t j = esp_random() % (i + 1);
      std::swap(s_order[i], s_order[j]);
    }
    auto it = std::find(s_order.begin(), s_order.end(), (uint16_t)start_base_idx);
    std::iter_swap(s_order.begin(), it);
    s_pos = 0;
  } else {
    s_pos = start_base_idx;
  }
  s_qver++;
}

// Apre il brano alla posizione corrente; se fallisce prova i successivi.
bool open_at_pos(bool keep_playing, bool deferred = false) {
  for (int tries = 0; tries < 8; tries++) {
    int t;
    {
      Lock l;
      if (s_pos < 0 || s_pos >= (int)s_order.size()) return false;
      t = s_base[s_order[s_pos]];
    }
    if (open_track(t, deferred)) {
      set_playing(keep_playing);
      return true;
    }
    Lock l;
    if (s_pos + 1 >= (int)s_order.size()) break;
    s_pos++;
  }
  set_playing(false);
  return false;
}

// user = premuto "successivo" (cambio immediato) / false = fine naturale del brano
void go_next(bool user) {
  if (user) rewind_lookahead();
  int manual = -1;
  bool wrap = false, end = false;
  {
    Lock l;
    if (!s_manual.empty()) {
      manual = s_manual.front();
      s_manual.pop_front();
      s_qver++;
    } else if (s_pos + 1 < (int)s_order.size()) {
      s_pos++;
    } else if (s_repeat == REPEAT_ALL || user) {
      wrap = true;
    } else {
      end = true;
    }
    if (wrap || end) {
      if (s_shuffle && !s_base.empty()) {
        int cur = s_order[std::max(s_pos, 0)];
        build_order_locked(cur);
        for (size_t i = s_order.size() - 1; i > 0; i--) std::swap(s_order[i], s_order[esp_random() % (i + 1)]);
      }
      s_pos = s_order.empty() ? -1 : 0;
      s_qver++;
    }
  }
  post_event(PLAYER_EV_QUEUE);
  bool was = is_playing() || !user;
  if (user) ring_flush();
  if (manual >= 0) {
    if (open_track(manual, !user, manual)) set_playing(was);
    return;
  }
  if (end) {
    ring_drain(5000);         // suona fino all'ultimo campione
    ring_flush();
    open_at_pos(false);       // come Spotify: torna al primo brano, in pausa
    post_event(PLAYER_EV_ENDED);
    return;
  }
  open_at_pos(was || wrap, !user);
}

void go_prev() {
  rewind_lookahead();
  bool restart;
  {
    Lock l;
    restart = s_st.pos_ms > 3000 || s_order.empty() || (s_pos <= 0 && s_repeat != REPEAT_ALL);
    if (!restart) {
      s_pos = s_pos > 0 ? s_pos - 1 : (int)s_order.size() - 1;
      s_qver++;
    }
  }
  ring_flush();
  if (restart) {
    if (s_fl) drflac_seek_to_pcm_frame(s_fl, 0);
    Lock l;
    s_st.pos_ms = 0;
    s_base_total = s_wr;
    s_base_ms = 0;
    return;
  }
  post_event(PLAYER_EV_QUEUE);
  open_at_pos(is_playing());
}

void set_shuffle(bool on) {
  {
    Lock l;
    if (on == s_shuffle) return;
    s_shuffle = on;
    if (!s_order.empty() && s_pos >= 0) {
      int cur = s_order[s_pos];
      if (on) {
        build_order_locked(cur);
      } else {
        for (size_t i = 0; i < s_order.size(); i++) s_order[i] = i;
        s_pos = cur;
      }
    }
    s_qver++;
  }
  post_event(PLAYER_EV_QUEUE);
}

void seek_to(uint32_t ms) {
  if (!s_fl) return;
  ring_flush();
  uint64_t f = (uint64_t)ms * s_src_rate / 1000;
  if (f >= s_fl->totalPCMFrameCount) f = s_fl->totalPCMFrameCount ? s_fl->totalPCMFrameCount - 1 : 0;
  drflac_seek_to_pcm_frame(s_fl, f);
  Lock l;
  s_st.pos_ms = ms;
  s_base_total = s_wr;
  s_base_ms = ms;
}

void handle(const Cmd &c) {
  switch (c.t) {
    case C_PLAY: {
      bool paused = c.a & 1;
      ring_flush();
      open_at_pos(!paused);
      if (c.b) seek_to(c.b);
      break;
    }
    case C_TOGGLE:
      if (is_playing()) s_fade_req = true;
      else if (s_fl) set_playing(true);
      else open_at_pos(true);
      break;
    case C_PAUSE:
      if (is_playing()) s_fade_req = true;
      break;
    case C_RESUME:
      if (!is_playing()) {
        if (s_fl) set_playing(true);
        else open_at_pos(true);
      }
      break;
    case C_NEXT: go_next(true); break;
    case C_PREV: go_prev(); break;
    case C_SEEK: seek_to(c.b); break;
    case C_SHUFFLE: set_shuffle(c.a); break;
    case C_REPEAT: {
      Lock l;
      s_repeat = (RepeatMode)c.a;
      s_qver++;
      post_event(PLAYER_EV_QUEUE);
      break;
    }
    case C_JUMP: {
      rewind_lookahead();
      int target = -1;
      {
        Lock l;
        int i = c.a;
        if (i < (int)s_manual.size()) {
          target = s_manual[i];
          s_manual.erase(s_manual.begin(), s_manual.begin() + i + 1);
        } else {
          int k = s_pos + 1 + (i - (int)s_manual.size());
          if (k < 0 || k >= (int)s_order.size()) break;
          s_pos = k;
        }
        s_qver++;
      }
      post_event(PLAYER_EV_QUEUE);
      ring_flush();
      if (target >= 0) {
        if (open_track(target)) set_playing(true);
      } else {
        open_at_pos(true);
      }
      break;
    }
    case C_STOP:
      ring_flush();
      close_track();
      set_playing(false);
      {
        Lock l;
        s_st.track = -1;
        s_st.pos_ms = s_st.dur_ms = 0;
      }
      post_event(PLAYER_EV_TRACK);
      break;
  }
}

// ------------------------------------------------------ statistiche ----
// Il decoder accumula i tempi; il task di uscita prepara il testo ogni 2 s
// (anche quando il decoder dorme perché il buffer è pieno).
volatile uint32_t s_dec_us = 0, s_dec_audio_us = 0, s_sd_us = 0;
uint32_t s_stat_last = 0, s_gaps_last = 0;
char s_perf[96] = "";
void note_stats(uint32_t dec_us, uint32_t frames) {
  s_dec_us += dec_us;
  s_dec_audio_us += (uint32_t)((uint64_t)frames * 1000000ULL / (s_src_rate ? s_src_rate : 44100));
  s_sd_us += (uint32_t)s_io_us;
  s_io_us = 0;
}
void update_stats() {
  uint32_t now = millis();
  if (now - s_stat_last < 2000) return;
  s_stat_last = now;
  uint32_t dec_us = s_dec_us, audio_us = s_dec_audio_us, sd_us = s_sd_us;
  s_dec_us = s_dec_audio_us = s_sd_us = 0;
  if (sd_us > dec_us) sd_us = dec_us;
  unsigned dec = audio_us ? (unsigned)((uint64_t)(dec_us - sd_us) * 100 / audio_us) : 0;
  unsigned sd = audio_us ? (unsigned)((uint64_t)sd_us * 100 / audio_us) : 0;
  unsigned buf_ms = (unsigned)((uint64_t)ring_fill() * 1000 / (s_rate_now ? s_rate_now : 44100));
  uint32_t gaps = s_gaps;
  snprintf(s_perf, sizeof s_perf, "dec %u%% sd %u%% buf %u.%us buchi %u", dec, sd, buf_ms / 1000,
           (buf_ms % 1000) / 100, (unsigned)(gaps - s_gaps_last));
  s_gaps_last = gaps;
  plat_mark(2, "player", s_perf);
}

// Decodifica un blocco nel buffer. Ritorna false a fine brano.
bool decode_block() {
  const int ch = s_fl->channels;
  const int want = ch <= 2 ? kFrames : (kFrames * 2) / ch;   // s_tmp contiene kFrames*2 campioni
  const int64_t t0 = esp_timer_get_time();
  drflac_uint64 got = drflac_read_pcm_frames_s32(s_fl, want, (drflac_int32 *)s_tmp);
  if (got == 0) return false;

  const int32_t *src = s_tmp;
  int outn = got;
  if (!(ch == 2 && s_decim == 1)) {
    // down-mix a stereo e decimazione (file > 96 kHz)
    outn = 0;
    for (int i = 0; i + s_decim <= (int)got; i += s_decim) {
      int64_t l = 0, r = 0;
      for (int k = 0; k < s_decim; k++) {
        const int32_t *f = s_tmp + (i + k) * ch;
        l += f[0];
        r += ch > 1 ? f[1] : f[0];
      }
      s_mix[outn * 2] = (int32_t)(l / s_decim);
      s_mix[outn * 2 + 1] = (int32_t)(r / s_decim);
      outn++;
    }
    src = s_mix;
  }
  ring_write(src, outn);
  note_stats((uint32_t)(esp_timer_get_time() - t0), got);
  return true;
}

void decoder_task(void *) {
  TickType_t last_yield = xTaskGetTickCount();
  for (;;) {
    Cmd c;
    // con il buffer pieno (o niente da decodificare) si dorme sulla coda comandi
    bool can_decode = s_fl && ring_free() >= (uint32_t)kFrames;
    TickType_t wait = can_decode ? 0 : (s_fl ? pdMS_TO_TICKS(10) : portMAX_DELAY);
    while (xQueueReceive(s_q, &c, wait) == pdTRUE) {
      handle(c);
      wait = 0;
    }
    if (!(s_fl && ring_free() >= (uint32_t)kFrames)) continue;
    // dopo una pausa il buffer resta pieno; mentre è in pausa non serve decodificare oltre
    if (!is_playing() && ring_fill() >= kRingFrames / 4) {
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }
    if (xTaskGetTickCount() - last_yield >= pdMS_TO_TICKS(30)) {
      vTaskDelay(1);   // lascia respirare gli altri task (watchdog)
      last_yield = xTaskGetTickCount();
    }
    if (!decode_block()) {
      bool one;
      {
        Lock l;
        one = s_repeat == REPEAT_ONE;
      }
      if (one) {
        drflac_seek_to_pcm_frame(s_fl, 0);
        Lock l;
        Marker m{s_wr, s_st.track, s_st.dur_ms, s_src_rate, 0, s_st.bits, s_st.channels, s_play_pos, -1};
        s_markers.push_back(m);
      } else {
        go_next(false);
      }
    }
  }
}

// ------------------------------------------------------ task di uscita ----
void output_task(void *) {
  int32_t gain = 0;
  uint32_t last_gen = s_gen;
  for (;;) {
    bool playing;
    {
      Lock l;
      playing = s_st.playing;
    }
    if (s_hold || !playing) {
      s_out_idle = true;
      gain = 0;
      vTaskDelay(pdMS_TO_TICKS(5));
      continue;
    }
    s_out_idle = false;
    uint32_t avail = ring_fill();
    if (avail == 0) {
      if (s_fade_req) {          // pausa richiesta con il buffer vuoto
        s_fade_req = false;
        set_playing(false);
        continue;
      }
      if (gain > 0 && s_gen == last_gen) s_gaps++;   // svuotato mentre suonava: buco udibile
      gain = 0;
      vTaskDelay(pdMS_TO_TICKS(2));
      continue;
    }
    uint32_t n = std::min<uint32_t>(avail, kOutFrames);
    uint32_t gen = s_gen;
    uint32_t r = s_rd % kRingFrames;
    uint32_t first = std::min(n, kRingFrames - r);
    memcpy(s_out, s_ring + r * 2, first * 8);
    if (n > first) memcpy(s_out + first * 2, s_ring, (n - first) * 8);
    bool valid;
    portENTER_CRITICAL(&s_ringmux);
    valid = (gen == s_gen);
    if (valid) s_rd += n;
    portEXIT_CRITICAL(&s_ringmux);
    if (!valid) continue;   // il buffer è stato svuotato mentre copiavamo
    if (gen != last_gen) {
      last_gen = gen;
      gain = 0;             // dopo seek/cambio brano: micro-rampa, niente click
    }

    // volume con rampa; dissolvenza rapida verso la pausa
    bool fading = s_fade_req;
    int32_t target = fading ? 0 : audio_out_gain_q16();
    int32_t step = fading ? (gain / (int32_t)n + 1) : 96;
    for (uint32_t i = 0; i < n; i++) {
      if (gain < target) gain = std::min(target, gain + step);
      else if (gain > target) gain = std::max(target, gain - step);
      s_out[i * 2] = (int32_t)(((int64_t)s_out[i * 2] * gain) >> 16);
      s_out[i * 2 + 1] = (int32_t)(((int64_t)s_out[i * 2 + 1] * gain) >> 16);
    }
    audio_out_write(s_out, n, 200);

    // posizione e cambio brano al punto giusto
    uint32_t ev = 0;
    {
      Lock l;
      while (!s_markers.empty() && (int32_t)(s_rd - s_markers.front().at) >= 0) {
        const Marker &m = s_markers.front();
        s_st.track = m.track;
        s_st.dur_ms = m.dur_ms;
        s_st.sample_rate = m.rate;
        s_st.bits = m.bits;
        s_st.channels = m.ch;
        s_st.error = false;
        s_base_total = m.at;
        s_base_ms = m.start_ms;
        s_play_pos = m.pos;
        s_markers.pop_front();
        ev |= PLAYER_EV_TRACK;
      }
      uint32_t rate = s_st.sample_rate ? s_st.sample_rate : s_rate_now;
      uint32_t out_rate = rate;
      while (out_rate > audio_out_max_rate()) out_rate /= 2;
      s_st.pos_ms = s_base_ms + (uint32_t)((uint64_t)(s_rd - s_base_total) * 1000 / out_rate);
      if (fading) {
        s_fade_req = false;
        s_st.playing = false;
        ev |= PLAYER_EV_STATE;
      }
    }
    if (ev) post_event(ev);
    update_stats();
  }
}

}  // namespace

void player_begin(TrackPathFn resolver) {
  s_resolve = resolver;
  mtx();
  s_q = xQueueCreate(16, sizeof(Cmd));
  s_ring = (int32_t *)heap_caps_malloc(kRingFrames * 8, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  s_fbuf = (char *)heap_caps_malloc(16 * 1024, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  s_tmp = (int32_t *)heap_caps_malloc(kFrames * 2 * 4, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  s_mix = (int32_t *)heap_caps_malloc(kFrames * 2 * 4, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  s_out = (int32_t *)heap_caps_malloc(kOutFrames * 2 * 4, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (!s_ring || !s_fbuf || !s_tmp || !s_mix || !s_out) Serial.println("[player] memoria insufficiente");
  // uscita: priorità alta, core 0. Decoder: priorità media, qualunque core libero.
  xTaskCreatePinnedToCore(output_task, "i2s", 4096, nullptr, configMAX_PRIORITIES - 3, nullptr, 0);
  xTaskCreatePinnedToCore(decoder_task, "flac", 16 * 1024, nullptr, 3, nullptr, tskNO_AFFINITY);
}

void player_play_list(const std::vector<uint16_t> &tracks, int start, const PlayContext &ctx,
                      bool start_paused, uint32_t start_ms) {
  {
    Lock l;
    s_base = tracks;
    s_ctx = ctx;
    s_manual.clear();
    build_order_locked(start);
  }
  post_event(PLAYER_EV_QUEUE);
  send(C_PLAY, start_paused ? 1 : 0, start_ms);
}

void player_toggle() { send(C_TOGGLE); }
void player_pause() { send(C_PAUSE); }
void player_resume() { send(C_RESUME); }
void player_next() { send(C_NEXT); }
void player_prev() { send(C_PREV); }
void player_seek_ms(uint32_t ms) { send(C_SEEK, 0, ms); }
void player_set_shuffle(bool on) { send(C_SHUFFLE, on); }
void player_set_repeat(RepeatMode r) { send(C_REPEAT, r); }
void player_jump(int i) { send(C_JUMP, i); }
void player_stop() { send(C_STOP); }

void player_enqueue(uint16_t t) {
  {
    Lock l;
    s_manual.push_back(t);
    s_qver++;
  }
  post_event(PLAYER_EV_QUEUE);
}

PlayerStatus player_status() {
  Lock l;
  PlayerStatus s = s_st;
  s.shuffle = s_shuffle;
  s.repeat = s_repeat;
  s.queue_version = s_qver;
  return s;
}

PlayContext player_context() {
  Lock l;
  return s_ctx;
}

const char *player_perf_text() { return s_perf; }

uint32_t player_take_events() {
  portENTER_CRITICAL(&s_evmux);
  uint32_t e = s_events;
  s_events = 0;
  portEXIT_CRITICAL(&s_evmux);
  return e;
}

void player_upcoming(std::vector<uint16_t> &out, size_t max_items, size_t *manual_count) {
  Lock l;
  out.clear();
  // vista "dell'ascoltatore": il decoder può essere già avanti di un brano
  for (auto &m : s_markers)
    if (m.manual >= 0 && out.size() < max_items) out.push_back(m.manual);
  for (auto t : s_manual) {
    if (out.size() >= max_items) break;
    out.push_back(t);
  }
  if (manual_count) *manual_count = out.size();
  int from = s_markers.empty() ? s_pos : s_play_pos;
  for (int i = from + 1; i < (int)s_order.size() && out.size() < max_items; i++) out.push_back(s_base[s_order[i]]);
}
