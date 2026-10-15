// Motore di riproduzione: task FreeRTOS dedicato (core 0) che decodifica FLAC
// con dr_flac e scrive campioni a 32 bit sull'uscita I2S.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <vector>
#include "../types.h"

struct PlayerStatus {
  int track = -1;          // indice brano in libreria, -1 = nessuno
  bool playing = false;
  uint32_t pos_ms = 0;
  uint32_t dur_ms = 0;
  uint32_t sample_rate = 0;
  uint8_t bits = 0;
  uint8_t channels = 0;
  bool shuffle = false;
  RepeatMode repeat = REPEAT_OFF;
  uint32_t queue_version = 0;
  bool error = false;
};

enum : uint32_t {
  PLAYER_EV_TRACK = 1 << 0,   // brano cambiato
  PLAYER_EV_STATE = 1 << 1,   // play/pausa
  PLAYER_EV_QUEUE = 1 << 2,   // coda/shuffle/repeat cambiati
  PLAYER_EV_ERROR = 1 << 3,   // impossibile aprire/decodificare
  PLAYER_EV_ENDED = 1 << 4,   // fine coda
};

typedef const char *(*TrackPathFn)(int track);

void player_begin(TrackPathFn resolver);
void player_play_list(const std::vector<uint16_t> &tracks, int start, const PlayContext &ctx,
                      bool start_paused = false, uint32_t start_ms = 0);
void player_beep(int count);   // avviso sonoro (sopra la musica, o da solo in pausa)
void player_toggle();
void player_pause();
void player_resume();
void player_next();
void player_prev();
void player_seek_ms(uint32_t ms);
void player_set_shuffle(bool on);
void player_set_repeat(RepeatMode r);
void player_enqueue(uint16_t track);       // "Aggiungi in coda"
void player_jump(int upcoming_index);      // salta a un elemento di player_upcoming()
void player_stop();
// Ferma il task di uscita (true) finché serve liberare/riconfigurare l'I2S; false = riprende
void player_hold_output(bool hold);

PlayerStatus player_status();
PlayContext player_context();
uint32_t player_take_events();
const char *player_perf_text();   // misure di decodifica/SD/buchi audio (diagnostica)
// Brani successivi: prima quelli aggiunti manualmente, poi il resto del contesto.
void player_upcoming(std::vector<uint16_t> &out, size_t max_items, size_t *manual_count);
