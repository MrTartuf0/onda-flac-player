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
#define ES8311_ADDR      0x18
#define AMP_PIN_EN       1                    // SC8002B shutdown: LOW = acceso
#define AMP_ON_LEVEL     0

// -------------------------------- DAC esterno PCM5102 (header P3 "Expanded IO") --
//  Il segnale I2S viene duplicato su questi pin tramite la GPIO matrix.
#define DAC_PIN_BCK      14
#define DAC_PIN_LRCK     21
#define DAC_PIN_DIN      3

// --------------------------------------------------- Rilevamento cuffie ----
//  Collega il contatto "detect/switch" di un jack 3.5mm a questo pin.
//  Metti -1 se non hai il contatto: resterà la selezione manuale dell'uscita.
#define HP_DETECT_PIN        2
#define HP_DETECT_PLUGGED    0     // livello letto quando le cuffie sono inserite (0 = verso GND)
#define HP_DETECT_DEBOUNCE   120   // ms

// ------------------------------------------------------------------ Varie ----
#define LED_RGB_PIN      42
#define BTN_BOOT_PIN     0
#define PAUSE_ON_UNPLUG_DEFAULT  true   // come sul telefono: stacchi le cuffie -> pausa
#define APP_NAME         "onda"         // nome mostrato nell'interfaccia
