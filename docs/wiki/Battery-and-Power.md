## The BOOT button

The board's **BOOT** button (IO0) works as the power button, so no extra pin is needed:

| BOOT button | What it does |
|---|---|
| 1 click | Screen off / on. The music keeps playing |
| 2 clicks | Play / pause, also with the screen off (for example in your pocket) |
| 3 clicks | [Timer](Session-Timer-and-Clock): start with the last time set (1 beep) / stop (2 beeps) |
| Hold for about 1 s | Power off (deep sleep). Press it again to turn the player back on |

## Screen timeout

The screen turns off by itself after 30 seconds without a touch. Change it in **Settings → Screen → Turn off the screen**: 15 s, 30 s, 1 min, 2 min, 5 min or Never.

- If the screen went off by itself, a tap wakes it up (you can turn this off with **Tap to wake**).
- If you turned it off with the button, touch stays locked and only the button wakes it, so nothing happens by accident in your pocket.
- The tap that wakes the screen is not passed on to the interface.
- During file transfer the screen stays on.

With the screen off the display sleeps with the backlight off, LVGL stops drawing and the CPU drops to 80 MHz (160 MHz for files above 48 kHz; see `SCREEN_OFF_CPU_MHZ` in `src/config.h`). Playback is not affected.

## Auto power-off

When the music is paused, the player is on battery and nobody touches it for 10 minutes, it saves where you were and powers off. Change the delay in **Settings → BOOT button → Auto power off**: 5, 10, 30, 60 minutes or Never. It never powers off while the USB cable is connected or while the [session timer](Session-Timer-and-Clock) is running. **Power off now** is in the same section.

When you turn it back on it continues from the same song and position, without the boot wait.

## Battery

<img src="https://raw.githubusercontent.com/MrTartuf0/onda-flac-player/main/docs/images/player-control.png" alt="Battery in the control center and in Settings, and the battery-low screen">

With a 3.7 V LiPo on the **BAT** connector (tested with 1100 mAh), the level and charging status are in the control center. **Settings → Battery** also shows the voltage, the time to full and the estimated current draw.

### Charging

The board's charger (TP4054, about 300 mA) fills an 1100 mAh cell in about 4 hours. The last part, from about 90%, is slower.

The board doesn't connect the charge LED or the USB power line to the chip, so charging is inferred from the voltage: it jumps up when you plug the cable in and drops when you unplug it. On a computer this is detected immediately; with a wall charger it can take a few seconds, and a few minutes if you turn the player on with the cable already connected. *Fully charged* appears when the voltage settles at 4.2 V.

### Low battery

- Warnings at 10% and 5%.
- Below 3.3 V the player saves where you were, shows *Battery low* and goes to standby. After plugging in the charger, wake it by tapping the screen, with the BOOT button or with RESET. If the battery is still too low, the warning comes back.

### Runtime estimate

The current draw is estimated from what is on: brightness, speaker or headphones and their volume, playback, Wi-Fi. That is why the estimate changes as soon as you raise the volume or the brightness.

The starting values are typical for this board. While on battery the player compares the estimated draw with how fast the battery actually drops and corrects itself; after about twenty minutes on battery the estimate is more reliable, and the correction is saved.

The percentage comes from a typical LiPo discharge curve, so it can move by a few points during playback. If the voltage in Settings differs from your multimeter, set `BAT_OFFSET_MV` in `src/config.h`.

### Storage

Powered off (deep sleep) the board still draws a little current (regulator, display, touch, external DAC). If you won't use the player for a long time, disconnect the battery.
