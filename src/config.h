// =============================================================================
//  config.h  -  Pinout e opzioni del lettore FLAC
//  Scheda: Freenove ESP32-S3 Display FNK0104B (2.8" ILI9341 + touch FT6336U)
// =============================================================================
#pragma once

// ---------------------------------------------------------------- Display ----
#define LCD_W            240
#define LCD_H            320
#define LCD_PIN_SCLK     12
#define LCD_PIN_MOSI     11
#define LCD_PIN_MISO     13
#define LCD_PIN_CS       10
#define LCD_PIN_DC       46
#define LCD_PIN_BL       45
#define LCD_SPI_HZ       (40 * 1000 * 1000)   // prova 80 MHz se il tuo pannello regge
#define LCD_BUF_LINES    30                   // righe per buffer LVGL (x2, in DMA RAM)

// ------------------------------------------------------------------ Touch ----
#define TP_I2C_ADDR      0x38                 // FT6336U
#define TP_PIN_RST       18
#define TP_PIN_INT       17

// --------------------------------------------------------- I2C (condiviso) ----
#define I2C_PIN_SDA      16
#define I2C_PIN_SCL      15
#define I2C_HZ           400000

// --------------------------------------------------------------- SD card ----
#define SD_PIN_CLK       38
#define SD_PIN_CMD       40
#define SD_PIN_D0        39
#define SD_PIN_D1        41
#define SD_PIN_D2        48
#define SD_PIN_D3        47
#ifndef SD_MOUNT
#define SD_MOUNT         "/sdcard"
#endif
#ifndef MUSIC_ROOT
#define MUSIC_ROOT       "/sdcard"            // scansiona tutta la scheda
#endif
#ifndef APP_DIR
#define APP_DIR          "/sdcard/.onda"      // cache libreria/copertine/preferiti
#endif

// ----------------------------------------------- Codec ES8311 + speaker ----
#define I2S_PIN_MCLK     4
#define I2S_PIN_BCLK     5
#define I2S_PIN_WS       7
#define I2S_PIN_DOUT     8
#define I2S_PIN_DIN      6                    // dati dal codec: microfono (ricerca vocale)
#define ES8311_ADDR      0x18
#define AMP_PIN_EN       1                    // SC8002B shutdown: LOW = acceso
#define AMP_ON_LEVEL     0

// -------------------------------- DAC esterno PCM5102 (header P3 "Expanded IO") --
//  Il segnale I2S viene duplicato su questi pin tramite la GPIO matrix.
#define DAC_PIN_BCK      21
#define DAC_PIN_LRCK     3
#define DAC_PIN_DIN      14

// --------------------------------------------------- Rilevamento cuffie ----
//  Collega il contatto "detect/switch" di un jack 3.5mm a questo pin.
//  Metti -1 se non hai il contatto: resterà la selezione manuale dell'uscita.
#define HP_DETECT_PIN        2
#define HP_DETECT_PLUGGED    0     // livello letto quando le cuffie sono inserite (0 = verso GND)
#define HP_DETECT_DEBOUNCE   120   // ms

// --------------------------------------------------------------- Batteria ----
//  LiPo 3,7 V sul connettore BAT: tensione letta da GPIO9 tramite partitore
//  200k/200k, caricatore TP4054 (~300 mA). Metti BAT_ADC_PIN -1 per disattivare.
#ifndef BAT_ADC_PIN
#define BAT_ADC_PIN          9
#endif
#define BAT_DIVIDER_X100     200   // tensione batteria = tensione letta * 2
#define BAT_OFFSET_MV        0     // correzione se la tensione mostrata differisce dal tester
#define BAT_CAPACITY_MAH     1100  // solo per stimare il tempo di ricarica
#define BAT_CHARGE_MA        300   // corrente di carica (R_PROG 3,3k del TP4054)
#define BAT_SHUTDOWN_MV      3300  // sotto (a batteria, per 30 s): salva e va in standby
#define BAT_BOOT_MIN_MV      3400  // all'accensione: sotto questa tensione non parte

// ---------------------------------------------------------------- Energia ----
//  A schermo spento il processore rallenta (la musica continua): 80 MHz bastano
//  per i FLAC fino a 48 kHz, sopra si usa 160. Metti 0 per lasciarlo a 240 MHz.
#define SCREEN_OFF_CPU_MHZ   80

// ------------------------------------------------------------- Orologio ----
//  Fuso orario in formato POSIX (Italia: CET/CEST con ora legale automatica).
//  L'ora arriva dal telefono/computer (pagina del trasferimento file) o da internet.
#define TIME_ZONE "CET-1CEST,M3.5.0,M10.5.0/3"

// --------------------------------------------------------------- Lingua ----
//  Lingua al primo avvio: 0 = italiano, 1 = English (poi si cambia in
//  Impostazioni → Lingua · Language). Chi aggiorna da una versione precedente
//  resta in italiano.
#define UI_LANG_DEFAULT 1

// ------------------------------------------------------------------ Varie ----
#define LED_RGB_PIN      42
#define BTN_BOOT_PIN     0
#define PAUSE_ON_UNPLUG_DEFAULT  true   // come sul telefono: stacchi le cuffie -> pausa
#define APP_NAME         "onda"         // nome mostrato nell'interfaccia
