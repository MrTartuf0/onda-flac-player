Connect the player to a computer with a USB-C cable and the microSD card shows up as a drive, like a flash drive. Copy music, rename or delete folders with your usual file manager, without taking the card out.

## How to use it

1. Open **Settings → Add music → USB disk**. When you plug the cable into a computer, a notice also appears for a few seconds: tap it to go straight there.
2. The music stops. As soon as the computer opens the card, the screen says **On your computer** and the card appears in Finder, Explorer or your file manager.
3. Copy your files. A ring pulses around the icon while the computer reads or writes, and below it you see how much has been copied.
4. When you are done, **eject the drive on the computer** (Finder: *Eject*; Windows: *Safely remove*; Linux: *Eject* or `umount`). The player sees it, restarts and updates the library with the new files. Then it is back on the song you were listening to, paused.

<p align="center"><img src="https://raw.githubusercontent.com/MrTartuf0/onda-flac-player/main/docs/images/usb-disk.png" width="480" alt="The USB disk screen: waiting for the computer, open on the computer while copying, and the confirmation when you leave without ejecting"></p>

Good to know:

- **Use a cable that carries data.** With a charge-only cable the screen stays on *Connect a computer*. In **Settings → Add music**, the line under *USB disk* turns green when the player sees a computer.
- **Done** leaves without ejecting. If the computer has written something, the player asks once more (*Leave anyway*): the last file copied could be incomplete, as with any flash drive pulled out too early.
- If the computer goes to sleep or the cable comes out, the screen says *Disconnected*: plug it back in, or tap **Back to the music**.
- The speed is that of USB full speed, about 1 MB/s, like the [Wi-Fi transfer](Web-Interface). For a whole library a card reader is faster.
- The screen stays on and the player does not power off while the computer has the card open.

## What the library picks up

At the restart the player reads again every file that is new or changed. It compares size and modification date, so tags edited on the computer are picked up even when the file size doesn't change. Albums with changed songs get their cover rebuilt.

If you only replaced a `cover.jpg`, use **Settings → Update library**: it reads every file again and rebuilds all the covers.

## How it works

The ESP32-S3 has one USB port and two USB controllers: **USB Serial/JTAG**, which carries the serial monitor and firmware uploads, and **USB-OTG**. The port normally belongs to USB Serial/JTAG. The USB disk hands it over to USB-OTG, running TinyUSB's Mass Storage class, and the computer reads and writes the card's sectors directly. In the meantime the player keeps away from the card: no playback, no index or play-count saves.

When you leave, the player gives the port back to USB Serial/JTAG and restarts, so serial output and uploads work as before.

- Nothing to change in the Arduino IDE: keep **USB Mode** on *Hardware CDC and JTAG* (the default) and **USB CDC On Boot** *Enabled*.
- While the disk is open the serial monitor is disconnected and uploads don't work. Leave the disk screen first, or press RESET.

The code is in `src/hal/usb_disk.*` (USB and sector access) and `src/ui/usb_disk_ui.cpp` (the screen).
