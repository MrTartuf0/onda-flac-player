## Preparing the card

- Format it as **FAT32** (exFAT works too).
- Organise folders however you like, for example `Music/Artist/Album/01 - Title.flac`. The whole card is scanned.
- Only `.flac` files are played. Playlists (`.m3u` / `.m3u8`) can be anywhere on the card.

You can copy files with a card reader, with the USB-C cable as if the player were a flash drive (see [USB Disk](USB-Disk)), or over Wi-Fi from a browser (see [Web Interface](Web-Interface)).

## Indexing

On the first start the player reads every file and shows a progress bar (looking for files, reading tags, preparing covers, saving the index). After that it starts in a moment: only new or changed files are read again. A file counts as changed when its size or its modification date is different, so tags rewritten on a computer are picked up too, and albums with changed songs get their cover rebuilt.

If you add music with a card reader, the new files are picked up at the next start, or right away with **Settings → Update library**, which reads every file and every cover again. After a Wi-Fi transfer the library is updated when you leave the transfer screen, after the USB disk when the player restarts.

The player remembers in which scan each file first appeared: that is the order of **Recently added** on the Home screen. Files that were already on the card before this was introduced are ordered by their modification date. Renaming or moving a file makes it count as new.

The player keeps its index, cover thumbnails, likes and play counts in the hidden folder `/.onda` on the card. You can delete it at any time; it is rebuilt (but likes and play counts are lost).

## Tags

onda reads standard Vorbis comments, the same tags every FLAC tagger writes:

| Tag | Used for |
|---|---|
| `TITLE` | Song title (if missing, the file name is used) |
| `ARTIST` | Song artist (if missing: *Unknown artist*) |
| `ALBUM` | Album (if missing, the folder name; files in the root of the card go to *Single tracks*) |
| `ALBUMARTIST` | Groups an album across folders and names its artist |
| `TRACKNUMBER`, `DISCNUMBER` | Song order inside an album |
| `DATE` | Year shown on the album page |
| `GENRE` | Genre browsing (one genre per song, the first `GENRE` tag) |

How albums are grouped:

- with `ALBUMARTIST`: same album title + same album artist, even in different folders;
- without it: same album title in the same folder. If the songs have different artists, the album artist becomes *Various artists*.

Missing tags are easy to fill in from the web page, by hand or automatically from MusicBrainz. Its **Libreria** (Library) tab has a *Dati mancanti* (missing data) filter.

## Covers

For each album the player uses, in this order:

1. a `cover.jpg` in the album folder;
2. the picture embedded in the FLAC files;
3. another image in the folder: `folder.jpg`, `front.jpg`, `album.jpg`, `albumart.jpg`, then any `.jpg`.

Covers must be **baseline JPEG**. PNG and progressive JPEG are not decoded on the device and show a placeholder instead. The web page fixes this in one go: **Libreria → Sistema copertine** (fix covers) converts unreadable embedded covers to a 600 px `cover.jpg` in each album folder.

The album colour used for the now-playing gradient is computed once from the cover and cached.

## Playlists

Any `.m3u` or `.m3u8` file on the card becomes a playlist. Paths can be relative to the playlist file or absolute from the root of the card. Playlists made in the web page are saved in `/Playlist`. They appear under **Search → Playlist** after the library is updated.

**Liked songs** and **All songs** are built-in playlists, always the first two tiles on the Home screen.
