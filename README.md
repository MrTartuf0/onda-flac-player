# onda · lettore FLAC per Freenove ESP32-S3 Display (FNK0104B)

Lettore di file **FLAC** dalla microSD integrata, con uscita sullo **speaker integrato**
oppure sulle **cuffie** tramite un DAC **PCM5102** (I2S). Quando inserisci il jack delle cuffie
l'uscita passa da sola alle cuffie; quando lo togli torna allo speaker (e la musica va in pausa,
come sul telefono). Interfaccia touch in stile Spotify, scritta con LVGL 9.5 (inclusa nel progetto).

![Schermate](docs/screenshots.png)

## Funzioni

- **Home**: scorciatoie recenti, "I più ascoltati", "Riscopri i tuoi album", artisti, generi
- **Cerca**: ricerca istantanea (senza accenti: "bjork" trova "Björk") + "Sfoglia tutto" per genere
- **La tua libreria**: filtri Playlist / Album / Artisti / Generi, ordinamento Recenti / Alfabetico
- Pagine **album**, **artista** (Popolari + Discografia), **playlist** `.m3u`, **Brani che ti piacciono**
- **In riproduzione**: copertina, gradiente col colore dell'album, seek, casuale, uscita audio con volume,
  swipe sinistra/destra per cambiare brano, giù per chiudere, su per la coda, formato (es. *FLAC 24 bit • 96 kHz*)
- **Coda** con Casuale e Ripeti (tutto / brano), "Aggiungi in coda", menu del brano (Mi piace, Vai all'album/artista)
- **Gesto indietro** come su Android: trascina dal bordo sinistro verso l'interno
- **Selettore dispositivo** (Automatico / Altoparlante / Cuffie) con volume separato per ciascuna uscita
- Mini player sempre visibile, riprende brano e posizione (salvata in pausa) all'accensione
- FLAC 8–32 bit, 8–96 kHz; i file a 176.4/192 kHz vengono decimati a 88.2/96 kHz
- Copertine JPEG incorporate nel FLAC o `cover.jpg` / `folder.jpg` / `front.jpg` nella cartella

## Hardware

| Funzione | Pin ESP32-S3 | Note |
|---|---|---|
| LCD ILI9341 (SPI) | SCK 12, MOSI 11, MISO 13, CS 10, DC 46, BL 45 | già sulla scheda |
| Touch FT6336U | I2C SDA 16, SCL 15, RST 18, INT 17 | già sulla scheda |
| microSD (SDMMC 4 bit) | CLK 38, CMD 40, D0 39, D1 41, D2 48, D3 47 | già sulla scheda |
| Codec ES8311 → ampli SC8002B | MCLK 4, BCLK 5, WS 7, DOUT 8; ampli EN = IO1 (attivo basso) | già sulla scheda |
| **PCM5102 BCK** | **IO14** | header P3 "Expanded IO" |
| **PCM5102 LCK** | **IO21** | header P3 |
| **PCM5102 DIN** | **IO3** | header P3 |
| **Rilevamento cuffie** | **IO2** | header P3 |

### Collegare il PCM5102

```
 Freenove P3 (Expanded IO)          Modulo PCM5102 (GY-PCM5102)
   IO14  ─────────────────────────── BCK
   IO21  ─────────────────────────── LCK
   IO3   ─────────────────────────── DIN
 Freenove P4 (I2C)  3V3 ──────────── VIN
                    GND ──────────── GND, SCK (SCK a massa = PLL interno)
```

Sul retro del modulo i ponticelli devono essere: **FLT = L, DEMP = L, XSMT = H, FMT = L**
(è la configurazione di fabbrica della maggior parte dei moduli; se non senti nulla controlla XSMT).

Il segnale I2S è lo stesso che va all'ES8311: il firmware lo **duplica** sui pin 14/21/3 tramite la
GPIO matrix dell'ESP32-S3, quindi il cambio di uscita è istantaneo, senza riconfigurare nulla. Quando
sono attive le cuffie l'ampli dello speaker viene spento e il DAC dell'ES8311 messo in muto.

### Rilevamento automatico delle cuffie

Il jack del modulo PCM5102 "viola" **non ha un contatto di rilevamento**, quindi serve un jack da
3.5 mm con contatto *switch/detect* (es. PJ-307, PJ-320D a 5 pin, o una breakout "audio jack with
switch"):

- uscite L / R / GND del PCM5102 → L / R / GND del nuovo jack
- contatto **detect** del jack → **IO2**, l'altro capo dell'interruttore → **GND**

Con le impostazioni predefinite "cuffie inserite" = IO2 a GND. Se il tuo jack funziona al contrario
attiva **Impostazioni → Inverti rilevamento cuffie** (oppure cambia `HP_DETECT_PLUGGED` in
`src/config.h`). Senza contatto di rilevamento metti `HP_DETECT_PIN -1`: sceglierai l'uscita a mano
dal selettore dispositivo (icona altoparlante/cuffie nel mini player).

## Compilazione (Arduino IDE)

1. Core **esp32 di Espressif 3.x** (testato con **3.2.0**, quello indicato nel tutorial Freenove).
2. Nessuna libreria da installare: **LVGL 9.5** è dentro lo sketch (`src/lvgl`, configurazione
   in `src/lv_conf.h`). Se hai già una LVGL 8 installata nell'IDE non dà fastidio, non viene usata.
   TFT_eSPI e FT6336U **non** servono: display e touch sono gestiti direttamente dallo sketch.
3. Apri `FlacPlayer/FlacPlayer.ino` e imposta in *Strumenti*:
   - Scheda: **ESP32S3 Dev Module**
   - PSRAM: **OPI PSRAM**
   - Partition Scheme: **Huge APP (3MB No OTA/1MB SPIFFS)**
   - USB CDC On Boot: **Enabled**
4. Carica. Il firmware occupa circa 1 MB.

La prima compilazione dura qualche minuto in più (compila anche LVGL); le successive sono veloci.
La versione precedente con LVGL 8.4 esterna è nel branch `lvgl8`.

## La scheda microSD

- FAT32 (o exFAT), cartelle libere, ad esempio `Musica/Artista/Album/01 - Titolo.flac`
- Tag Vorbis letti: TITLE, ARTIST, ALBUM, ALBUMARTIST, TRACKNUMBER, DISCNUMBER, DATE, GENRE
- Playlist: file `.m3u` / `.m3u8` ovunque sulla scheda (percorsi relativi o assoluti)
- Alla prima accensione la libreria viene indicizzata (barra di avanzamento); le volte successive
  parte in un attimo. Se aggiungi brani, all'avvio vengono letti solo i file nuovi; puoi anche
  usare **Impostazioni → Aggiorna libreria**.
- Cache, preferiti e statistiche stanno in `/.onda` (puoi cancellarla: verrà ricreata).

## Struttura del progetto

```
FlacPlayer/
  FlacPlayer.ino          setup/loop, montaggio SD, task di scansione
  src/config.h            pin e opzioni
  src/hal/display.*       ILI9341 via esp_lcd (SPI+DMA), touch FT6336U, driver LVGL
  src/audio/es8311.*      codec dello speaker
  src/audio/audio_out.*   I2S 32 bit, duplicazione sul PCM5102, rilevamento cuffie, volumi
  src/audio/player.*      task di decodifica (dr_flac), coda, shuffle/repeat, rampe anti-click
  src/lib/                scansione SD, tag FLAC, indice, ricerca, preferiti, copertine (TJpgDec)
  src/ui/                 pagine LVGL (Home, Cerca, Libreria, Album, Artista, In riproduzione...)
  src/fonts/              font generati (Figtree + icone Font Awesome)
  src/lvgl/               LVGL 9.5.0 (solo i file necessari, licenza MIT)
  src/lv_conf.h           configurazione LVGL
tools/gen_fonts.sh        rigenera i font con lv_font_conv
```

## Limiti noti

- Copertine PNG o JPEG progressive non vengono mostrate (compare il segnaposto).
- Caratteri non latini (cirillico, CJK...) nei tag non hanno glifi nel font.
- Tra brani con frequenze di campionamento diverse c'è una brevissima pausa (riconfigurazione I2S).

## Licenze di terze parti

dr_flac (pubblico dominio / MIT-0), TJpgDec (© ChaN, licenza libera), font Figtree (SIL OFL 1.1),
icone Font Awesome Free 6 (SIL OFL 1.1 / CC BY 4.0). L'interfaccia è *ispirata* a Spotify per uso
personale: nomi, logo e marchi Spotify non sono inclusi.
