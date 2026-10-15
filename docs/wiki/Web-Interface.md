The player has a small web server for managing the microSD card from a browser on your computer or phone: upload music, fix tags and covers, and make playlists.

<img src="https://raw.githubusercontent.com/MrTartuf0/onda-flac-player/main/docs/images/web-library.png" alt="The Library tab">

> The web page is in Italian for now. Below, each button is given as it appears on the page, with its meaning in brackets.

## Connecting

<p align="center"><img src="https://raw.githubusercontent.com/MrTartuf0/onda-flac-player/main/docs/images/player-transfer.png" alt="File transfer on the player: on the home network, and as a hotspot with a QR code" width="480"></p>

Open **Settings → File transfer** (or tap Wi-Fi in the control center).

- **Home network**: pick your network on the player and type the password with the on-screen keyboard. The page is then at `http://onda.local` or at the IP address shown, with a QR code.
- **Hotspot**: with no saved network, or if it can't be reached, the player creates its own network `onda-XXXX`. Scan the QR code with your phone to join it, then open `http://192.168.4.1`.

Music is paused during the transfer: Wi-Fi and the audio output don't fit in the chip's internal RAM together. Typical speed is 0.5–1 MB/s.

When you are done, tap **Aggiorna libreria** (update library) on the page or go **back** on the player: Wi-Fi turns off, new and changed files are read, and playback resumes where it was.

Opening the page also sets the player's clock from your phone or computer (see [Session Timer and Clock](Session-Timer-and-Clock)).

## File tab

<img src="https://raw.githubusercontent.com/MrTartuf0/onda-flac-player/main/docs/images/web-files.png" alt="The File tab">

- Upload files or whole folders (**Carica file**, **Carica cartella**, or drag them onto the page), with progress and speed.
- Create folders (**Nuova cartella**), rename, move, delete, download.
- Hidden files such as `/.onda` are shown only on request (**File nascosti**).

## Library tab (Libreria)

All songs grouped by album, with technical details (bit depth, sample rate, length, size) and what is missing highlighted. Filters: *Dati mancanti* (missing data), *Senza copertina* (no cover), and instant search.

### Editing a song

<img src="https://raw.githubusercontent.com/MrTartuf0/onda-flac-player/main/docs/images/web-track.png" alt="Song details and tag editor">

Tap a row to see everything about the file: format, samples, average bitrate, encoder, audio MD5, metadata blocks and embedded pictures. You can edit title, artist, album, album artist, track and disc number, year and genre, plus any other tag (one per line, `KEY=value`).

### Editing an album (Modifica album)

Change album, album artist, artist, year and genre for all songs at once, plus number and title of each song. **Numera i brani in ordine** numbers the songs 1, 2, 3… in the current order. Fields you leave unchanged are not written.

### Covers (Copertina)

<img src="https://raw.githubusercontent.com/MrTartuf0/onda-flac-player/main/docs/images/web-cover.png" alt="Cover dialog">

- Upload an image (PNG works too, also from a phone gallery).
- **Usa quella incorporata**: use the picture embedded in the file.
- **Cerca su internet**: search MusicBrainz / Cover Art Archive (the player must be on your home Wi-Fi).

The browser converts the image to a 600 px JPEG and the player saves it as `cover.jpg` in the album folder, where it takes priority over embedded pictures.

**Sistema copertine** (fix covers) does this in one go for every embedded cover the player can't read (PNG, progressive JPEG).

### MusicBrainz

<img src="https://raw.githubusercontent.com/MrTartuf0/onda-flac-player/main/docs/images/web-musicbrainz.png" alt="Album editor filled in from MusicBrainz: pick the release and the fields are filled in">

[MusicBrainz](https://musicbrainz.org) is a free, open music database. The page can use it in two ways:

- **Compila da MusicBrainz** (fill from MusicBrainz), inside *Modifica album*: searches for the album and lists the releases found, with their covers. Releases with the same number of songs are highlighted. Pick one and the form is filled with titles, numbers, artists (including "feat."), year and genre; songs whose length doesn't match are flagged, and the cover can be saved too. You check and save.
- **Completa con MusicBrainz** (complete with MusicBrainz), at the top of the Library tab: does it automatically for every album with missing data or no cover, but **only when the match is certain** (same number of songs and matching lengths), and it **only adds what is missing**, never overwriting existing tags. Albums it isn't sure about are listed so you can fix them with *Modifica album*.

MusicBrainz and Cover Art Archive are queried by the browser, so the phone or computer needs internet access (player on your home Wi-Fi, not in hotspot mode). The `MUSICBRAINZ_*` IDs are written as well, so Picard and other tools recognise the files.

### How tags are written

Tags are written **inside the FLAC file** as standard Vorbis comments, so every other player sees them. The audio is never touched:

- if the file has spare room (a PADDING block, which almost every encoder leaves) only the header is rewritten, instantly;
- otherwise the file is copied once (a few seconds) with 8 KB of spare room for next time.

Very large fields (for example huge embedded lyrics) are preserved even though the page doesn't show them. The player shows the new data after **Aggiorna libreria**.

## Playlist tab

<img src="https://raw.githubusercontent.com/MrTartuf0/onda-flac-player/main/docs/images/web-playlist.png" alt="Playlist editor">

Create, edit and delete playlists: search for songs, add them with **+**, reorder them with the arrows. They are saved as `.m3u8` files in `/Playlist` (existing playlists stay where they are) and appear on the player under **Search → Playlist** after the library is updated.

## On a phone

The page works on a phone too, so you can fix a tag or add a cover from the sofa.

<p align="center"><img src="https://raw.githubusercontent.com/MrTartuf0/onda-flac-player/main/docs/images/web-phone.png" alt="The library and the song editor on a phone" width="520"></p>

## API

The page talks to the player through a small JSON API, handy for scripts:

| Endpoint | Method | Purpose |
|---|---|---|
| `/api/info` | GET | Player name and card space |
| `/api/ls`, `/api/dl` | GET | List a folder, download a file |
| `/api/up`, `/api/mkdir`, `/api/mv`, `/api/rm` | POST | Upload, create folder, rename/move, delete |
| `/api/done` | POST | Leave transfer mode and update the library |
| `/api/lib` | GET | Library: tracks, albums, playlists, device language |
| `/api/meta?p=` | GET | Full metadata of one file |
| `/api/tags?p=` | POST | Replace the tags of one file (URL-encoded `KEY=value` lines) |
| `/api/pic?p=&i=` | GET | An embedded picture |
| `/api/thumb?k=` | GET | Album thumbnail (`k` = album key from `/api/lib`) |
| `/api/cover?k=` | POST | Save `cover.jpg` in an album folder |
| `/api/time` | POST | Set the clock |

The handlers are in `src/net/xfer.cpp`.

## Editing the page

The page is a single file, `tools/web/index.html`. After changing it run `python3 tools/web/embed.py` to regenerate `src/net/web_index.h`, then rebuild the firmware.
