#pragma GCC optimize("O2")
#include <Arduino.h>
#include "player.h"
#include <deque>
#include <algorithm>
#include <esp_heap_caps.h>
#include <esp_random.h>
#include "audio_out.h"
#include "dr_flac.h"
#include "../platform.h"
#include <esp_timer.h>
#include <string.h>

namespace {

constexpr int kFrames = 512;          // frame per blocco di decodifica
constexpr int kMaxCh = 2;          // file multicanale: letti a blocchi più piccoli

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

// ---- stato del decoder (solo task) ----
FILE *s_fp = nullptr;
char *s_fbuf = nullptr;
drflac *s_fl = nullptr;
uint32_t s_src_rate = 0;
int s_decim = 1;
int32_t s_gain = 0;               // guadagno corrente Q16 (rampa)
bool s_fade_to_pause = false;
int32_t *s_tmp = nullptr;         // campioni decodificati (interleaved)
int32_t *s_out = nullptr;         // stereo per l'I2S

// Il mutex viene creato al primo uso: l'interfaccia può interrogare il player
// anche prima di player_begin() senza crash.
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

// ----------------------------------------------------- callback dr_flac ----
size_t cb_read(void *u, void *out, size_t n) { return fread(out, 1, n, (FILE *)u); }
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

bool open_track(int track) {
  close_track();
  const char *path = s_resolve ? s_resolve(track) : nullptr;
  plat_mark(2, "player: apertura", path ? path : "?");
  bool ok = false;
  if (path) {
    s_fp = fopen(path, "rb");
    if (s_fp) {
      setvbuf(s_fp, s_fbuf, _IOFBF, 16 * 1024);
      s_fl = drflac_open(cb_read, cb_seek, cb_tell, s_fp, nullptr);
    }
    if (s_fl && s_fl->channels >= 1 && s_fl->channels <= 8) {
      s_src_rate = s_fl->sampleRate;
      s_decim = 1;
      while (s_src_rate / s_decim > audio_out_max_rate() && s_decim < 8) s_decim *= 2;
      ok = audio_out_set_rate(s_src_rate / s_decim);
      if (!ok) Serial.printf("[player] sample rate non supportato: %u Hz\n", (unsigned)s_src_rate);
    } else {
      Serial.printf("[player] impossibile aprire %s\n", path);
    }
  }
  {
    Lock l;
    s_st.track = track;
    s_st.pos_ms = 0;
    s_st.error = !ok;
    if (ok) {
      s_st.dur_ms = (uint32_t)(s_fl->totalPCMFrameCount * 1000ULL / s_src_rate);
      s_st.sample_rate = s_src_rate;
      s_st.bits = s_fl->bitsPerSample;
      s_st.channels = s_fl->channels;
    } else {
      s_st.dur_ms = 0;
    }
  }
  if (!ok) close_track();
  s_gain = 0;  // ogni brano parte con una micro-rampa: niente click
  post_event(PLAYER_EV_TRACK | (ok ? 0 : PLAYER_EV_ERROR));
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
bool open_at_pos(bool keep_playing) {
  for (int tries = 0; tries < 8; tries++) {
    int t;
    {
      Lock l;
      if (s_pos < 0 || s_pos >= (int)s_order.size()) return false;
      t = s_base[s_order[s_pos]];
    }
    if (open_track(t)) {
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

void go_next(bool user) {
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
        int cur = s_base.empty() ? 0 : s_order[std::max(s_pos, 0)];
        build_order_locked(cur);
        // dopo un giro in shuffle si riparte con un nuovo ordine casuale
        for (size_t i = s_order.size() - 1; i > 0; i--) std::swap(s_order[i], s_order[esp_random() % (i + 1)]);
      }
      s_pos = s_order.empty() ? -1 : 0;
      s_qver++;
    }
  }
  post_event(PLAYER_EV_QUEUE);
  bool was = is_playing() || !user;
  if (manual >= 0) {
    if (open_track(manual)) set_playing(was);
    return;
  }
  if (end) {
    open_at_pos(false);       // come Spotify: torna al primo brano, in pausa
    post_event(PLAYER_EV_ENDED);
    return;
  }
  open_at_pos(was || wrap);
}

void go_prev() {
  bool restart;
  {
    Lock l;
    restart = s_st.pos_ms > 3000 || s_order.empty() || (s_pos <= 0 && s_repeat != REPEAT_ALL);
    if (!restart) {
      s_pos = s_pos > 0 ? s_pos - 1 : (int)s_order.size() - 1;
      s_qver++;
    }
  }
  if (restart) {
    if (s_fl) drflac_seek_to_pcm_frame(s_fl, 0);
    s_gain = 0;
    Lock l;
    s_st.pos_ms = 0;
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

void handle(const Cmd &c) {
  switch (c.t) {
    case C_PLAY: {
      bool paused = c.a & 1;
      open_at_pos(!paused);
      if (c.b && s_fl) {
        uint64_t f = (uint64_t)c.b * s_src_rate / 1000;
        if (f < s_fl->totalPCMFrameCount) drflac_seek_to_pcm_frame(s_fl, f);
        Lock l;
        s_st.pos_ms = c.b;
      }
      break;
    }
    case C_TOGGLE:
      if (is_playing()) s_fade_to_pause = true;
      else if (s_fl) set_playing(true);
      else open_at_pos(true);
      break;
    case C_PAUSE:
      if (is_playing()) s_fade_to_pause = true;
      break;
    case C_RESUME:
      if (!is_playing()) {
        if (s_fl) set_playing(true);
        else open_at_pos(true);
      }
      break;
    case C_NEXT: go_next(true); break;
    case C_PREV: go_prev(); break;
    case C_SEEK:
      if (s_fl) {
        uint64_t f = (uint64_t)c.b * s_src_rate / 1000;
        if (f >= s_fl->totalPCMFrameCount) f = s_fl->totalPCMFrameCount ? s_fl->totalPCMFrameCount - 1 : 0;
        drflac_seek_to_pcm_frame(s_fl, f);
        s_gain = 0;
        Lock l;
        s_st.pos_ms = c.b;
      }
      break;
    case C_SHUFFLE: set_shuffle(c.a); break;
    case C_REPEAT: {
      Lock l;
      s_repeat = (RepeatMode)c.a;
      s_qver++;
      post_event(PLAYER_EV_QUEUE);
      break;
    }
    case C_JUMP: {
      int target = -1;
      {
        Lock l;
        int i = c.a;
        if (i < (int)s_manual.size()) {
          target = s_manual[i];
          s_manual.erase(s_manual.begin(), s_manual.begin() + i + 1);
        } else {
          int k = s_pos + 1 + (i - (int)s_manual.size());
          if (k >= 0 && k < (int)s_order.size()) s_pos = k;
          else k = -1;
          if (k < 0) break;
        }
        s_qver++;
      }
      post_event(PLAYER_EV_QUEUE);
      if (target >= 0) {
        if (open_track(target)) set_playing(true);
      } else {
        open_at_pos(true);
      }
      break;
    }
    case C_STOP:
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

// Statistiche: tempo di decodifica rispetto alla durata audio prodotta.
uint64_t s_cpu_us = 0, s_audio_us = 0;
uint32_t s_cpu_last_log = 0;
void note_cpu(uint32_t dec_us, uint32_t frames) {
  s_cpu_us += dec_us;
  s_audio_us += (uint64_t)frames * 1000000ULL / (s_src_rate ? s_src_rate : 44100);
  uint32_t now = millis();
  if (now - s_cpu_last_log >= 3000 && s_audio_us > 0) {
    s_cpu_last_log = now;
    unsigned pct = (unsigned)(s_cpu_us * 100 / s_audio_us);
    char d[48];
    snprintf(d, sizeof d, "CPU %u%% (%u Hz, %u bit, %u ch)", pct, (unsigned)s_src_rate,
             (unsigned)s_fl->bitsPerSample, (unsigned)s_fl->channels);
    plat_mark(2, "player: decodifica", d);
    Serial.printf("[player] decodifica: %s\n", d);
    s_cpu_us = s_audio_us = 0;
  }
}

// Decodifica e invia un blocco. Ritorna false a fine brano.
bool decode_block() {
  const int ch = s_fl->channels;
  const int want = ch <= 2 ? kFrames : (kFrames * 2) / ch;   // il buffer contiene kFrames*2 campioni
  const uint32_t t_start = (uint32_t)esp_timer_get_time();
  drflac_uint64 got = drflac_read_pcm_frames_s32(s_fl, want, (drflac_int32 *)s_tmp);
  if (got == 0) return false;

  // Down-mix canali -> stereo e decimazione (per file > 96 kHz)
  int outn = 0;
  if (ch == 2 && s_decim == 1) {
    memcpy(s_out, s_tmp, got * 8);
    outn = got;
  } else
  for (int i = 0; i + s_decim <= (int)got; i += s_decim) {
    int64_t l = 0, r = 0;
    for (int k = 0; k < s_decim; k++) {
      const int32_t *f = s_tmp + (i + k) * ch;
      l += f[0];
      r += ch > 1 ? f[1] : f[0];
    }
    if (s_decim > 1) {
      l /= s_decim;
      r /= s_decim;
    }
    s_out[outn * 2] = (int32_t)l;
    s_out[outn * 2 + 1] = (int32_t)r;
    outn++;
  }

  // Volume con rampa (evita click su play/pausa/seek/cambio uscita)
  int32_t target = s_fade_to_pause ? 0 : audio_out_gain_q16();
  int32_t step = s_fade_to_pause ? (s_gain / outn + 1) : 96;
  for (int i = 0; i < outn; i++) {
    if (s_gain < target) s_gain = std::min(target, s_gain + step);
    else if (s_gain > target) s_gain = std::max(target, s_gain - step);
    int64_t l = (int64_t)s_out[i * 2] * s_gain;
    int64_t r = (int64_t)s_out[i * 2 + 1] * s_gain;
    s_out[i * 2] = (int32_t)(l >> 16);
    s_out[i * 2 + 1] = (int32_t)(r >> 16);
  }
  const uint32_t t_dec = (uint32_t)esp_timer_get_time() - t_start;
  audio_out_write(s_out, outn, 1000);
  note_cpu(t_dec, got);

  {
    Lock l;
    s_st.pos_ms = (uint32_t)(s_fl->currentPCMFrame * 1000ULL / s_src_rate);
  }
  if (s_fade_to_pause) {
    s_fade_to_pause = false;
    s_gain = 0;
    set_playing(false);
  }
  return true;
}

void task(void *) {
  for (;;) {
    Cmd c;
    bool active = is_playing() && s_fl;
    TickType_t wait = active ? 0 : portMAX_DELAY;
    while (xQueueReceive(s_q, &c, wait) == pdTRUE) {
      handle(c);
      wait = 0;
    }
    if (!(is_playing() && s_fl)) continue;
    // Il task ha priorità alta sul core 0: se non si blocca mai (I2S sempre
    // pronto o decodifica lenta) lascia comunque 1 ms agli altri compiti,
    // altrimenti scatta il watchdog di sistema.
    static TickType_t last_yield = 0;
    TickType_t now = xTaskGetTickCount();
    if (now - last_yield >= pdMS_TO_TICKS(40)) {
      vTaskDelay(1);
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
        post_event(PLAYER_EV_TRACK);
      } else {
        go_next(false);
      }
    }
  }
}

}  // namespace

void player_begin(TrackPathFn resolver) {
  s_resolve = resolver;
  mtx();
  s_q = xQueueCreate(16, sizeof(Cmd));
  // Buffer di lettura in RAM interna (più veloce per la SD), il resto in PSRAM.
  s_fbuf = (char *)heap_caps_malloc(16 * 1024, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  // buffer di decodifica in RAM interna: vengono letti/scritti per ogni campione
  s_tmp = (int32_t *)heap_caps_malloc(kFrames * kMaxCh * 4, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (!s_tmp) s_tmp = (int32_t *)heap_caps_malloc(kFrames * kMaxCh * 4, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  s_out = (int32_t *)heap_caps_malloc(kFrames * 2 * 4, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  xTaskCreatePinnedToCore(task, "player", 16 * 1024, nullptr, configMAX_PRIORITIES - 4, nullptr, 0);
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
  for (auto t : s_manual) {
    if (out.size() >= max_items) break;
    out.push_back(t);
  }
  if (manual_count) *manual_count = out.size();
  for (int i = s_pos + 1; i < (int)s_order.size() && out.size() < max_items; i++) out.push_back(s_base[s_order[i]]);
}
