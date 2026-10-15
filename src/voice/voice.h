// Ricerca vocale (prova): dici il nome di un artista, album, playlist, genere o
// brano della tua libreria e il lettore lo apre.
//
// Gira tutto sul chip, senza internet, con ESP-SR di Espressif (MultiNet,
// inglese): non è una dettatura libera ma il riconoscimento di frasi da un
// elenco, e l'elenco sono i nomi della libreria (fino a kVoiceMaxPhrases, prima
// quelli che ascolti di più). I modelli stanno in una partizione "model" della
// flash: serve lo schema "ESP SR 16M" (vedi wiki). Senza quella partizione
// voice_supported() è falso e il microfono non compare.
//
// Tutte le funzioni vanno chiamate dal thread dell'interfaccia: il lavoro
// pesante (modello, microfono) lo fa un task a parte sul core 1.
#pragma once
#include <stdint.h>
#include <stddef.h>

enum VoiceState : uint8_t {
  VOICE_IDLE,
  VOICE_LOADING,     // prima volta: carica il modello e prepara i nomi (qualche secondo)
  VOICE_LISTENING,   // microfono aperto
  VOICE_DONE,        // finito: risultati in VoiceStatus (anche nessuno)
  VOICE_ERROR,
};

enum VoiceKind : uint8_t { VK_ARTIST, VK_ALBUM, VK_PLAYLIST, VK_GENRE, VK_TRACK };

struct VoiceMatch {
  VoiceKind kind;
  uint32_t key;        // chiave stabile (album/artista/playlist/genere) o path_hash del brano
  int index;           // indice attuale in libreria, -1 se non c'è più
  float prob;          // 0..1
};

struct VoiceStatus {
  VoiceState state = VOICE_IDLE;
  int level = 0;               // livello del microfono 0..100 (per l'interfaccia)
  int n = 0;                   // risultati validi in m[]
  VoiceMatch m[3] = {};
  const char *error = "";      // testo per l'utente (VOICE_ERROR)
  int phrases = 0;             // nomi riconoscibili
};

constexpr int kVoiceMaxPhrases = 300;

bool voice_supported();        // compilata con ESP-SR e modelli presenti in flash
void voice_listen();           // avvia l'ascolto; la prima volta carica il modello
void voice_stop();             // smette di ascoltare e cerca il risultato con quello che ha
void voice_cancel();           // annulla senza risultato
VoiceStatus voice_status();
void voice_library_changed();  // i nomi vanno ricostruiti al prossimo ascolto
void voice_release();          // libera la memoria del modello (es. prima del Wi-Fi)

// Testo che il riconoscitore si aspetta per un nome ("Mareggiata (Live) - 2019" →
// "mareggiata"): minuscolo, solo lettere, numeri in parole inglesi.
// Restituisce false se non resta niente di pronunciabile.
bool voice_phrase_text(const char *name, bool trim_extras, char *out, size_t cap);
