## What you need

- **Arduino IDE 2** or **arduino-cli**
- Espressif's **esp32** core 3.x. The firmware builds with **3.2.0** (the version in the Freenove tutorial) and with **3.3.x**.
- No libraries to install. **LVGL 9.5** is bundled in `src/lvgl` (configured in `src/lv_conf.h`), and the display and touch drivers are part of the sketch, so TFT_eSPI and FT6336U libraries are not used. An LVGL 8 already installed in the IDE does no harm; it is simply ignored.

## Get the code

Arduino wants the sketch folder to have the same name as the `.ino` file, so clone the repository into a folder called `FlacPlayer`:

```sh
git clone https://github.com/MrTartuf0/onda-flac-player.git FlacPlayer
```

## Arduino IDE

1. Open `FlacPlayer/FlacPlayer.ino`.
2. In **Tools** set:
   - Board: **ESP32S3 Dev Module**
   - PSRAM: **OPI PSRAM**
   - Partition Scheme: **Huge APP (3MB No OTA/1MB SPIFFS)**
   - USB CDC On Boot: **Enabled**
   - USB Mode: **Hardware CDC and JTAG** (the default; the [USB disk](USB-Disk) switches the port by itself when you use it)
3. Upload. The firmware is about 1.9 MB.

### With voice search

[Voice search](Voice-Search) is experimental and off by default. To try it, set `VOICE_SEARCH` to `1` in `src/config.h` (the firmware grows to about 2.7 MB) and change two settings, because the speech models need a flash partition of their own:

- Flash Size: **16MB (128Mb)**
- Partition Scheme: **ESP SR 16M (3MB APP/6MB SPIFFS/3.9MB MODEL)** (core 3.2.0: *ESP SR 16M (3MB APP/7MB SPIFFS/2.9MB MODEL)*)

The IDE then also writes the models (`srmodels.bin`, about 3.3 MB) on every upload. With Huge APP everything else works the same and the microphone button is simply not shown.

The first build takes a few extra minutes because LVGL is compiled too; later builds are quick.

## arduino-cli

```sh
arduino-cli core install esp32:esp32
arduino-cli compile --upload -p /dev/cu.usbmodem1101 \
  --fqbn "esp32:esp32:esp32s3:PSRAM=opi,FlashSize=4M,PartitionScheme=huge_app,CDCOnBoot=cdc" \
  FlacPlayer
```

For voice search use `FlashSize=16M,PartitionScheme=esp_sr_16` instead.

Replace the port with yours (`arduino-cli board list` shows it). Serial output is at 115200 baud; it is where crash reports are printed (see [Troubleshooting](Troubleshooting)).

## Configuration

Everything you might want to change is in `src/config.h`:

| Option | Default | What it does |
|---|---|---|
| `DAC_PIN_BCK` / `DAC_PIN_DIN` / `DAC_PIN_LRCK` | 21 / 14 / 3 | PCM5102 pins |
| `HP_DETECT_PIN` | 2 | Headphone detect pin, `-1` if your jack has no detect contact |
| `HP_DETECT_PLUGGED` | 0 | Level read when headphones are plugged in |
| `PAUSE_ON_UNPLUG_DEFAULT` | true | Pause when headphones are unplugged (also in Settings) |
| `I2S_PIN_DIN` | 6 | Data from the codec: the microphone, for voice search |
| `VOICE_SEARCH` | 0 | Voice search (experimental): `1` to build it in; needs the *ESP SR 16M* scheme |
| `BAT_ADC_PIN` | 9 | Battery voltage pin, `-1` to disable battery support |
| `BAT_OFFSET_MV` | 0 | Correction if the displayed voltage differs from a multimeter |
| `BAT_CAPACITY_MAH`, `BAT_CHARGE_MA` | 1100, 300 | Used for the time-to-full estimate |
| `BAT_SHUTDOWN_MV` | 3300 | Below this, on battery for 30 s, the player saves and goes to standby |
| `SCREEN_OFF_CPU_MHZ` | 80 | CPU clock when you are not using the screen, on or off (160 is used above 48 kHz); `0` keeps 240 MHz |
| `SCREEN_DIM_S` | 10 | Seconds without a touch before the screen dims to a quarter; `0` never dims |
| `TIME_ZONE` | Italy (`CET-1CEST,M3.5.0,M10.5.0/3`) | POSIX time zone string for the clock |
| `UI_LANG_DEFAULT` | 1 | Language on first start: 0 Italian, 1 English |
| `APP_NAME` | `onda` | Name shown in the interface, used for `onda.local` and the `onda-XXXX` hotspot |
| `LCD_SPI_HZ` | 40 MHz | Display SPI clock; some panels handle 80 MHz |

## Changing the web page or the fonts

- The web page lives in `tools/web/index.html`. After editing it, regenerate `src/net/web_index.h` with `python3 tools/web/embed.py`.
- Fonts are generated with `lv_font_conv` by `tools/gen_fonts.sh` (Figtree for text, Font Awesome for icons). Run it again after adding an icon or a character range.
- Interface strings are translated through `tools/i18n`; see [Language and Translations](Language-and-Translations).

## Older version

The earlier version that used an external LVGL 8.4 is kept in the `lvgl8` branch.
