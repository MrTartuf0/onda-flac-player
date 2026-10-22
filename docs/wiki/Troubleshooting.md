## The player froze or restarted by itself

onda watches itself. If it crashes, or if nothing responds for 3 seconds, it restarts on its own and continues from the same song and position, playing if it was playing. A notice tells you it happened.

Each time, a technical report is:

- printed on the serial port (115200 baud), between `=== RIAVVIO DOPO UN BLOCCO ===` and `=== fine resoconto ===`;
- appended to **`/.onda/crash.log`** on the microSD card.

The report contains the reason for the restart, the firmware build date, the crash details, the last events, and what each CPU core and each part of the firmware (interface, SD scan, player) was doing. **This is what to attach when you open an issue.** You can download the file from the [web page](Web-Interface) (turn on *File nascosti* to see `/.onda`) or read it with a card reader.

If the player crashes again within 20 seconds of restarting, it stops on the **Startup interrupted** screen with the report, instead of looping. Take a photo of it and press **RESET** to try again.

## No sound from the headphones

- Check the PCM5102 jumpers: **FLT = L, DEMP = L, XSMT = H, FMT = L**. XSMT low means muted.
- **SCK** of the module must go to GND (internal PLL).
- Check the three signal wires (IO21 → BCK, IO14 → DIN, IO3 → LCK) and that the module has 3.3 V.
- Make sure the output is set to headphones: control center, or **Settings → Audio → Audio output**.

## Speaker and headphones play at the same time, or the wrong one plays

- In **AUTO** the output follows the jack's detect contact. If it works the wrong way round (speaker with headphones in, headphones with nothing in) turn on **Settings → Invert jack detection**.
- **Settings → Audio → Headphone jack** shows what the player reads. If it never changes, check the detect wire on IO2. With a jack that has no detect contact, set `HP_DETECT_PIN -1` and choose the output by hand.

## "Insert a microSD card"

- The card must be FAT32 or exFAT. Try formatting it with the official SD Card Formatter.
- If it still isn't found, try another card.
- Tap **Retry** after reinserting it.

## Songs or covers are missing

- Only `.flac` files are played.
- After copying files with a card reader, use **Settings → Update library** (or restart). With the [USB disk](USB-Disk) the library updates by itself when you eject the drive.
- Covers in PNG or progressive JPEG are not shown on the device: use **Sistema copertine** in the web page's Library tab (it converts them inside the files), or put a baseline `cover.jpg` in the album folder. See [SD Card and Library](SD-Card-and-Library).
- Albums split in two usually have songs with different `ALBUM` or `ALBUMARTIST` values, or sit in different folders without `ALBUMARTIST`. Fix them with *Modifica album* in the web page.

## Battery reading looks wrong

- If the voltage in **Settings → Battery** differs from a multimeter, set `BAT_OFFSET_MV` in `src/config.h`.
- The percentage can move by a few points during playback; that is normal.
- The runtime estimate improves after about twenty minutes on battery. See [Battery and Power](Battery-and-Power).
- Without a battery, *No battery detected* is shown. To disable battery support completely, set `BAT_ADC_PIN -1`.

## The clock shows "Time not set"

Open the file transfer page once from a phone or computer, or connect the player to your home Wi-Fi. See [Session Timer and Clock](Session-Timer-and-Clock).

## The USB disk doesn't show up on the computer

- Use a cable that carries data. In **Settings** the line under *USB disk* turns green (*Computer connected*) when the player sees a computer; if it stays grey with the cable plugged in, the cable only charges.
- Plug the player directly into the computer rather than into a hub or a monitor.
- *No microSD card*: the card isn't mounted; reinsert it and restart the player.
- After the USB disk the serial monitor comes back when the player restarts. If uploads fail, press RESET once. See [USB Disk](USB-Disk).

## Wi-Fi transfer is slow or won't connect

- Typical speed is 0.5–1 MB/s; large libraries are faster to copy with a card reader.
- If the home network can't be reached, the player falls back to its own hotspot `onda-XXXX`: scan the QR code on the screen.
- `onda.local` may not work on some Android phones; use the IP address shown on the player.

## Voice search: no microphone, or it doesn't understand

Voice search is off by default. The microphone appears only when the firmware is built with `VOICE_SEARCH 1` and flashed with the *ESP SR 16M* partition scheme. Recognition is in English and limited to the names in your library. See [Voice Search](Voice-Search) for setup and for the `[voice]` lines in the serial log.

## Known limits

- PNG and progressive JPEG covers are not decoded on the device (the web page can convert them).
- Non-Latin characters (Cyrillic, CJK…) in tags have no glyphs in the font.
- There is a very short gap between songs with different sample rates (I2S is reconfigured).
- 176.4 and 192 kHz files are decimated to 88.2 and 96 kHz.
- The web page is in Italian only.
