A map of the firmware for anyone who wants to change it. Code comments are in Italian; names and structure should be enough to find your way.

## Source tree

```
FlacPlayer.ino            setup/loop, SD mount, scan task, crash report, safe mode
src/config.h              pins and options
src/platform.*            settings (NVS), event log, crash and freeze detection, resume after reset
src/power.*               BOOT button, screen off, auto power-off, deep sleep, CPU clock
src/clock.*               clock (time from the browser or NTP, time zone, date format)
src/i18n.*, i18n_en.h     translations (see Language and Translations)
src/session.h             timer API: countdown, pause, +1 min, alarms (implemented in src/ui/session.cpp)
src/voice/voice.*         voice search: names from the library, ESP-SR MultiNet in its own task (VOICE_SEARCH, off)
src/hal/display.*         ILI9341 via esp_lcd (SPI + DMA), FT6336U touch, LVGL display/input drivers
src/hal/battery.*         battery voltage, percentage, charge detection, runtime estimate
src/hal/edge_swipe.*      edge gestures: back (left edge) and control center (bottom edge)
src/hal/usb_disk.*        USB disk: hands the USB port to USB-OTG (TinyUSB Mass Storage) and serves the card's sectors
src/audio/es8311.*        codec setup over I2C: speaker DAC, and the ADC for the microphone
src/audio/audio_out.*     I2S output, mirroring to the PCM5102, headphone detection, volumes, microphone input
src/audio/player.*        decoder and output tasks, queue, shuffle/repeat, anti-click ramps, beeps
src/lib/flac_meta.*       fast tag and picture reading during the scan
src/lib/flac_tags.*       full metadata reading and in-place tag writing (web page)
src/lib/library.*         scan, index, albums/artists/genres/playlists, search, likes, play counts
src/lib/artwork.*         cover decoding (TJpgDec), thumbnail cache on SD and in RAM, album colour
src/ui/                   LVGL pages: Home, Search, Settings, album/artist/collection pages,
                          Now playing and queue, control center, sheets, transfer, battery screens,
                          timer screen, voice search screen, USB disk screen
src/net/xfer.*            Wi-Fi (home network or hotspot), mDNS, HTTP server and JSON API
src/net/web_index.h       the web page, generated from tools/web/index.html
src/fonts/                generated fonts (Figtree + Font Awesome icons)
src/lvgl/, src/lv_conf.h  LVGL 9.5.0 (only the files that are needed) and its configuration
tools/gen_fonts.sh        regenerates the fonts with lv_font_conv
tools/web/                web page source and embed.py
tools/i18n/               translation table and generator
```

## Tasks

| Task | Core | Role |
|---|---|---|
| `loopTask` (Arduino loop) | 1 | LVGL, the whole interface, power management |
| `flac` | any | Decodes FLAC with dr_flac into a PCM ring buffer in PSRAM |
| `i2s` | 0, high priority | Takes PCM from the ring, applies volume, ramps and beeps, writes to I2S |
| `scan` | 0 | Library scan, tag reading, cover thumbnails; also writes the crash log |
| `xfer` | 0 | Wi-Fi and HTTP server, only during file transfer |
| `usbd` | any, highest | TinyUSB device task, only while the USB disk is open: reads and writes the card's sectors for the computer |
| `voice` | 1, priority 1 | Voice search: loads the model, reads the microphone and runs MultiNet. Created the first time the microphone is used, deleted before file transfer |
| `monitor` | 1, high priority | Detects stalls of the other tasks |

## Audio path

```
SD card ──► [flac] dr_flac decode ──► 1 MB ring buffer in PSRAM ──► [i2s] volume + I2S ──► ES8311 ──► speaker amp
                                      (~3 s at 44.1 kHz)                     │
                                                                             └─ GPIO matrix copy ──► PCM5102 ──► headphones
```

- The ring buffer absorbs pauses from the SD card, the interface and flash writes, so the output task always has data ready.
- Samples are 32-bit stereo on I2S. Mono files are duplicated to both channels, and files above 96 kHz are decimated (176.4/192 kHz → 88.2/96 kHz) in the decoder.
- There is a single I2S port. For the headphones the BCK, LRCK and DOUT signals are copied to IO21/IO3/IO14 through the GPIO matrix; for the speaker those pins are detached and held low and the PCM5102 goes into standby. Switching output is instant.
- Volume, fade in/out ramps (no clicks on pause, seek or track change) and timer beeps are applied in the output task.

## Library

On the first start the scan task walks the card, reads tags and picture offsets from every `.flac` (`flac_meta`), and stores them in `/.onda/library.db`. Later starts reload that index and read again only new or changed files, so startup is fast.

In memory the library is a set of compact arrays (tracks, albums, artists, genres, playlists) with strings in a single arena. Search runs on a pre-folded lower-case, accent-free string per track.

Covers are decoded once with TJpgDec into 160 px and 64 px thumbnails cached in `/.onda/art`; the album's colour, used for the gradients, is computed at the same time and kept in the index.

The web page can change tags (`flac_tags`). It marks the file as stale so the next library update reads it again, and marks the album when its cover changes.

## Interface

The UI is LVGL 9.5 with a Spotify-like layout: a page stack (`ui_push`) under a bottom tab bar, the mini player above it, and overlays for Now playing, the queue, the control center and bottom sheets.

Touch goes through `edge_swipe`, which sits between the touch driver and LVGL and recognises gestures that start at the left or bottom edge before LVGL sees them.

## Settings, crashes and resume

- Settings live in NVS (`plat_kv_get` / `plat_kv_set`).
- A small block of RTC memory that survives a reset (`RTC_NOINIT`) holds the crash details, the last events of a "black box" log, what each task was doing, and the current song and position.
- A FreeRTOS tick hook checks that the system is still alive. If nothing has made progress for 3 seconds it records the state of both cores and restarts the chip.
- On boot, `plat_boot_check` reports why the chip restarted. After a crash the report is printed and appended to `/.onda/crash.log`, and playback resumes from the saved song and position. A second crash within 20 seconds stops on the safe-mode screen.

## Building for a PC

`src/platform.*`, `src/clock.*` and the library and UI code have host fallbacks (`#ifndef ARDUINO`), so the interface can be compiled for a desktop LVGL simulator to try out layouts and take screenshots without flashing the board. The simulator itself is not part of the repository.
