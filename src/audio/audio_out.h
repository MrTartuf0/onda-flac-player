// Uscita audio: un solo bus I2S (32 bit, stereo) che pilota l'ES8311 (speaker)
// e, duplicato via GPIO matrix, il DAC esterno PCM5102 (cuffie).
#pragma once
#include <stdint.h>
#include <stddef.h>

enum OutputMode : uint8_t { OUT_AUTO = 0, OUT_SPEAKER = 1, OUT_HEADPHONES = 2 };
enum OutputDev : uint8_t { DEV_SPEAKER = 0, DEV_HEADPHONES = 1 };

enum : uint32_t {
  AOUT_EV_DEVICE_CHANGED = 1 << 0,
  AOUT_EV_HP_PLUGGED = 1 << 1,
  AOUT_EV_HP_UNPLUGGED = 1 << 2,
};

bool audio_out_begin();
bool audio_out_set_rate(uint32_t sample_rate);   // riconfigura I2S + ES8311 se cambia
uint32_t audio_out_rate();
uint32_t audio_out_max_rate();                     // massimo fs gestito senza decimare
size_t audio_out_write(const int32_t *frames, size_t n_frames, uint32_t timeout_ms);

void audio_out_poll();                // da chiamare spesso (loop): rilevamento cuffie
uint32_t audio_out_take_events();     // eventi accumulati (AOUT_EV_*)

OutputDev audio_out_active();
bool audio_out_hp_present();
bool audio_out_hp_detect_available();
void audio_out_set_mode(OutputMode m);
OutputMode audio_out_mode();
void audio_out_set_detect_inverted(bool inv);
bool audio_out_detect_inverted();
void audio_out_set_pause_on_unplug(bool en);
bool audio_out_pause_on_unplug();

// Volume 0..100 per ciascuna uscita; il guadagno usato è quello dell'uscita attiva.
void audio_out_set_volume(OutputDev d, uint8_t v);
uint8_t audio_out_volume(OutputDev d);
int32_t audio_out_gain_q16();         // guadagno lineare Q16 (0..65536)
bool audio_out_muted();               // mute "di sicurezza" dopo lo scollegamento cuffie
void audio_out_clear_mute();
