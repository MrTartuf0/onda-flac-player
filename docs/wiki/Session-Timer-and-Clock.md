## Session timer

A stopwatch with a limit, made for not staying too long on a highline, but useful for any activity.

It is the bottom row of the control center:

- the elapsed time, with *running* or *over* underneath;
- the limit: tap it to cycle through 10, 15, 20, 30, 45 min, 1 h, 1 h 30, 2 h or off;
- the round button starts and stops it.

When the limit is reached you hear **three beeps over the music** (or on their own if it is paused) and a notice appears. After that, two beeps every 5 minutes until you stop it.

With the player in your pocket, **3 clicks on the BOOT button** start and stop the timer: 1 beep means started, 2 beeps mean stopped.

While the timer is running the player never powers off by itself.

## Clock

The board has no backup battery for the clock, so the player gets the time from somewhere else:

- **from your phone or computer**, every time you open the [file transfer page](Web-Interface);
- **from the internet** (NTP), when the player is connected to your home Wi-Fi.

After that the chip keeps the time with the screen off, when powered off with the button, and across restarts. It is lost only when the battery is disconnected. While powered off it can drift by a few minutes a day (there is no clock crystal); the next transfer sets it right again.

The time is in the top-right corner of **Settings**, and the date is in the *Date and time* section together with where the time came from.

### Time zone

The default is Italy (CET/CEST, with automatic daylight saving time). To change it, set `TIME_ZONE` in `src/config.h` to a [POSIX time zone string](https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html), for example:

| Place | `TIME_ZONE` |
|---|---|
| Italy, Central Europe | `CET-1CEST,M3.5.0,M10.5.0/3` |
| United Kingdom | `GMT0BST,M3.5.0/1,M10.5.0` |
| US Eastern | `EST5EDT,M3.2.0,M11.1.0` |
| Japan | `JST-9` |
