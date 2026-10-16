# onda

<img src="docs/images/tour.gif" align="right" width="260" hspace="24" alt="A tour of the player: home, album, now playing, queue, control center, search, settings and the timer">

**A pocket FLAC player for the Freenove ESP32-S3 Display (FNK0104B).**

onda plays FLAC files from the microSD card on the built-in speaker, or on headphones through a PCM5102 DAC. Plug the headphones in and the sound moves to them; unplug them and the music pauses, like on a phone.

The touch interface is inspired by Spotify and written with LVGL 9.5. Over Wi-Fi, a web page lets you upload music, fix tags and covers with MusicBrainz, and build playlists.

**[Wiki](https://github.com/MrTartuf0/onda-flac-player/wiki)** · [Hardware](https://github.com/MrTartuf0/onda-flac-player/wiki/Hardware) · [Build](https://github.com/MrTartuf0/onda-flac-player/wiki/Building-and-Flashing) · [Web interface](https://github.com/MrTartuf0/onda-flac-player/wiki/Web-Interface)

<br clear="right">

## Features

- **Lossless playback**: FLAC 8–32 bit up to 96 kHz (176.4/192 kHz files are decimated to 88.2/96 kHz), gapless within the same sample rate, no clicks on pause or seek
- **Library from your tags**: albums, artists, genres, `.m3u` playlists, Liked songs, play counts, instant accent-insensitive search
- **Now playing** with album-coloured gradient, swipe to change song, queue with shuffle and repeat
- **Control center** (swipe up from the bottom edge): output, volume, brightness, battery, Wi-Fi. A tap never sets the headphone volume above 75%
- **Voice search** offline on the chip, with the board microphone (experimental, off by default)
- **Automatic headphone switching** with a detect jack, separate volume for each output
- **Web interface over Wi-Fi**: upload music, edit tags inside the FLAC files, fix covers, build playlists, fill in missing data from MusicBrainz
- **BOOT button as power button**: screen off while the music keeps playing, play/pause in your pocket, power off
- **Battery**: level, charge detection, self-calibrating runtime estimate, safe shutdown when empty
- **Timer** with its own screen: set it to the second (the pasta, a highline session) or use it as a stopwatch; beeps over the music when time's up
- **Clock** set from your phone or from the internet
- **Resumes** the same song at the same position after power-off, and even after a crash
- **English and Italian** interface

![Screenshots](docs/screenshots.png)

## Web interface

Turn on Wi-Fi from the player and manage the card from any browser: upload music, see what is missing, fix tags and covers, fill albums in from MusicBrainz, build playlists.

<p>
  <img src="docs/images/web-library.png" width="49%" alt="Library: every album with technical details and missing data highlighted">
  <img src="docs/images/web-musicbrainz.png" width="49%" alt="Album editor filled in from MusicBrainz">
</p>
<p>
  <img src="docs/images/web-track.png" width="49%" alt="Song editor with technical details and tags">
  <img src="docs/images/web-playlist.png" width="49%" alt="Playlist editor">
</p>

## What you need

- Freenove ESP32-S3 Display **FNK0104B** (2.8" ILI9341 touchscreen, microSD, speaker)
- A microSD card with your `.flac` files
- Optional: a **PCM5102** DAC module and a 3.5 mm jack with a detect contact for headphones
- Optional: a 3.7 V LiPo battery

## Quick start

1. Clone the repository into a folder called `FlacPlayer` (Arduino wants the folder to match the sketch name):
   ```sh
   git clone https://github.com/MrTartuf0/onda-flac-player.git FlacPlayer
   ```
2. In Arduino IDE install Espressif's **esp32** core 3.x. No other libraries are needed.
3. Open `FlacPlayer.ino` and set **Tools**: Board *ESP32S3 Dev Module*, PSRAM *OPI PSRAM*, Partition Scheme *Huge APP (3MB No OTA/1MB SPIFFS)*, USB CDC On Boot *Enabled*.
4. Upload, insert the card and wait for the first library scan.

Voice search is off by default; to try it see the [Voice Search](https://github.com/MrTartuf0/onda-flac-player/wiki/Voice-Search) page.

Pins and options are in [`src/config.h`](src/config.h).

## Documentation

Everything else is in the **[wiki](https://github.com/MrTartuf0/onda-flac-player/wiki)**:

- [Hardware](https://github.com/MrTartuf0/onda-flac-player/wiki/Hardware): pins, wiring the PCM5102, headphone detection, battery
- [Building and Flashing](https://github.com/MrTartuf0/onda-flac-player/wiki/Building-and-Flashing): IDE and arduino-cli settings, configuration options
- [SD Card and Library](https://github.com/MrTartuf0/onda-flac-player/wiki/SD-Card-and-Library): folders, tags, covers, playlists
- [Using the Player](https://github.com/MrTartuf0/onda-flac-player/wiki/Using-the-Player): screens and gestures
- [Battery and Power](https://github.com/MrTartuf0/onda-flac-player/wiki/Battery-and-Power): BOOT button, screen timeout, charging, runtime
- [Web Interface](https://github.com/MrTartuf0/onda-flac-player/wiki/Web-Interface): file transfer, tag editor, MusicBrainz, playlists
- [Voice Search](https://github.com/MrTartuf0/onda-flac-player/wiki/Voice-Search): experimental, off by default
- [Session Timer and Clock](https://github.com/MrTartuf0/onda-flac-player/wiki/Session-Timer-and-Clock): the timer screen, and where the time comes from
- [Language and Translations](https://github.com/MrTartuf0/onda-flac-player/wiki/Language-and-Translations)
- [Troubleshooting](https://github.com/MrTartuf0/onda-flac-player/wiki/Troubleshooting): crash reports, no sound, known limits
- [Architecture](https://github.com/MrTartuf0/onda-flac-player/wiki/Architecture): how the firmware is organised

The wiki pages live in [`docs/wiki`](docs/wiki) and are published automatically on every push to `main`. Screenshots are taken from a desktop build of the interface with a made-up demo library.

## Third-party code and assets

LVGL 9.5 (MIT), dr_flac (public domain / MIT-0), TJpgDec (© ChaN, permissive licence), Figtree font (SIL OFL 1.1), Font Awesome Free 6 icons (SIL OFL 1.1 / CC BY 4.0). Voice search uses Espressif ESP-SR, which ships with the esp32 Arduino core (Espressif licence: models for Espressif chips only).

The interface is *inspired by* Spotify for personal use; no Spotify names, logos or trademarks are included.
