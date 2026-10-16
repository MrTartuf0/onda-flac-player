The interface has three tabs at the bottom (**Home**, **Search**, **Settings**), a mini player that is always visible while something is playing, and a full-screen **Now playing** view.

<p align="center"><img src="https://raw.githubusercontent.com/MrTartuf0/onda-flac-player/main/docs/images/tour.gif" alt="A tour of the player" width="300"></p>

## Gestures

| Gesture | Where | What it does |
|---|---|---|
| Drag from the **left edge** towards the middle | anywhere | Back, like on Android |
| Swipe up from the **bottom edge** | anywhere, also in Now playing | Opens the control center |
| Swipe left / right | Now playing | Next / previous song |
| Swipe down | Now playing | Closes it |
| Swipe up | Now playing | Opens the queue |
| Long press | Liked songs or All songs tile | Starts it straight away in shuffle |
| Long press | cover or title in Now playing | Song menu |

## Home

<img src="https://raw.githubusercontent.com/MrTartuf0/onda-flac-player/main/docs/images/player-browse.png" alt="Home, album page, artist page and the library in Search">

- **Liked songs** and **All songs** are always the first two tiles. Tap to open them; long press to start them in shuffle right away.
- The other tiles are what you played recently: albums, artists, playlists, genres.
- Below: **Most played**, **Rediscover your albums**, **Your artists** and genres.

## Search

Type in the search box for instant results across songs, albums and artists. Accents don't matter: "bjork" finds "Björk".

Without a query the page shows your library, with filters (**Playlist**, **Album**, **Artists**, **Genres**) and a sort button (recently played first, or alphabetical).

With [voice search](Voice-Search) turned on (experimental, off by default), a **microphone** in the search box lets you say the name of an artist, album, playlist or song.

## Album, artist and playlist pages

- **Album**: cover, artist, year and length, the big play button, shuffle, then the songs in disc/track order.
- **Artist**: **Popular** (the songs you play most) and **Discography**.
- **Playlist**, **Genre**, **Liked songs**, **All songs**: same layout as an album.

The **⋯** button on a song opens its menu: Play, Add to / Remove from Liked songs, Add to queue, Go to album, Go to artist, and the file format.

## Now playing

<img src="https://raw.githubusercontent.com/MrTartuf0/onda-flac-player/main/docs/images/player-playing.png" alt="Now playing, the queue, the song menu and the device selector">

Tap the mini player to open it. It shows the cover on a gradient in the album's colour, the position bar (drag to seek), the format (for example *FLAC 24-bit • 96 kHz*), shuffle, previous, play/pause, next and the queue button.

- Tap the **cover** or the **title** to go to the album; tap the **artist** for the artist page.
- Tap **Playing from …** at the top to open the album or playlist the music is coming from.
- Tap the **heart** to add the song to Liked songs.

## Queue

The queue shows the song now playing, the songs you added with *Add to queue*, and then the rest of the album or playlist. **Shuffle** and **Repeat** (all / one) are at the bottom. To close it, tap the header, swipe down on it, or tap the song now playing.

## Control center

<img src="https://raw.githubusercontent.com/MrTartuf0/onda-flac-player/main/docs/images/player-control.png" alt="Control center, the Battery section of Settings, and the battery-low screen">

Swipe up from the bottom edge. The panel follows your finger; close it by dragging the handle down, swiping down, tapping an empty area or with the back gesture.

- **Wi-Fi**: opens file transfer (blue while it is on).
- **Speaker / Headphones / AUTO**: the audio output. AUTO uses the headphones when they are plugged in.
- **Battery**: percentage, estimated runtime on battery or time to full while charging.
- **Volume** and **Brightness**: tap a point to jump to that level (in 5% steps), or put your finger anywhere and drag up or down for fine control. Volume is the one of the output in use; each output remembers its own.

The timer has its own screen, from **Settings → Timer**: see [Session Timer and Clock](Session-Timer-and-Clock).

> **Headphone safety**: with headphones, a tap never sets the volume above 75%. To go higher you have to drag.

## Audio output

The output can also be chosen from the device selector (the speaker/headphone icon in the mini player): **Automatic**, **Built-in speaker** or **Headphones (PCM5102)**.

When you unplug the headphones the music pauses, like on a phone (**Settings → Pause on unplug**). The speaker takes over when you press play again.

## Settings

| Section | What's there |
|---|---|
| Timer | Opens the timer screen; underneath, the time left (see [Session Timer and Clock](Session-Timer-and-Clock)) |
| Battery | Level and voltage, charging status, estimated current draw |
| Date and time | Date and where the time came from |
| Audio | Output, headphone jack status, pause on unplug, invert jack detection |
| Language | English / Italian |
| Screen | Brightness, screen timeout, tap to wake |
| BOOT button | What each click does, auto power-off, power off now |
| Wi-Fi | File transfer |
| Library | Update library, cache folder |
| About | Version and supported formats |

The clock is in the top-right corner of Settings.

## Resuming

The player remembers the song, the position, the album or playlist it was playing from, and the shuffle and repeat settings. After power-off, a reset or a crash it starts again from the same point. Songs you added with *Add to queue* are not kept.
