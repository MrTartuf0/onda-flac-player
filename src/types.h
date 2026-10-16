// Tipi condivisi tra player, libreria e interfaccia (senza dipendenze Arduino)
#pragma once
#include <stdint.h>

enum RepeatMode : uint8_t { REPEAT_OFF = 0, REPEAT_ALL = 1, REPEAT_ONE = 2 };

enum CtxType : uint8_t {
  CTX_NONE = 0, CTX_ALBUM, CTX_ARTIST, CTX_PLAYLIST, CTX_LIKED, CTX_GENRE, CTX_ALL, CTX_SEARCH,
  CTX_NEW,        // "Aggiunti di recente": gli ultimi album arrivati sulla scheda
  CTX_UNPLAYED,   // "Mai ascoltati": i brani che non sono mai partiti
};

struct PlayContext {
  CtxType type = CTX_NONE;
  uint32_t key = 0;        // hash che identifica album/artista/playlist/genere
  char name[64] = {0};     // nome mostrato ("In riproduzione da ...")
};

