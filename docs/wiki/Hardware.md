## The board

The **Freenove ESP32-S3 Display FNK0104B** already has almost everything onda needs:

- ESP32-S3 with OPI PSRAM
- 2.8" ILI9341 display, 240×320, with FT6336U capacitive touch
- microSD slot wired in 4-bit SDMMC mode
- ES8311 codec with an SC8002B amplifier and a small speaker
- LiPo charger (TP4054) and a battery connector, with the battery voltage readable on IO9
- BOOT button on IO0, which onda uses as its power button

For headphones you add a PCM5102 DAC module and, if you want automatic switching, a 3.5 mm jack with a detect contact. A 3.7 V LiPo is optional.

## Pins

| Function | ESP32-S3 pins | Notes |
|---|---|---|
| LCD ILI9341 (SPI) | SCK 12, MOSI 11, MISO 13, CS 10, DC 46, BL 45 | on the board |
| Touch FT6336U | I2C SDA 16, SCL 15, RST 18, INT 17 | on the board |
| microSD (SDMMC 4-bit) | CLK 38, CMD 40, D0 39, D1 41, D2 48, D3 47 | on the board |
| ES8311 codec → SC8002B amp | MCLK 4, BCLK 5, WS 7, DOUT 8; amp enable IO1 (active low) | on the board |
| **PCM5102 BCK** | **IO21** | header P3 "Expanded IO" |
| **PCM5102 DIN** | **IO14** | header P3 |
| **PCM5102 LCK** | **IO3** | header P3 |
| **Headphone detect** | **IO2** | header P3 |
| Battery (200k/200k divider) | IO9 | on the board, LiPo on the BAT connector |
| BOOT button | IO0 | on the board |

All of these are defined in `src/config.h`.

## Wiring the PCM5102

```
 Freenove P3 (Expanded IO)          PCM5102 module (GY-PCM5102)
   IO21  ─────────────────────────── BCK
   IO14  ─────────────────────────── DIN
   IO3   ─────────────────────────── LCK
 Freenove P4 (I2C)  3V3 ──────────── VIN
                    GND ──────────── GND, SCK (SCK to ground = internal PLL)
```

On the back of the module the solder jumpers must be **FLT = L, DEMP = L, XSMT = H, FMT = L**. That is the factory setting on most modules; if you hear nothing, check XSMT first.

The PCM5102 drives headphones directly from its line output, so no headphone amplifier is needed for normal headphones and IEMs.

### How the two outputs share one I2S bus

The ESP32-S3 sends a single I2S stream to the ES8311. The firmware copies that same stream onto IO21/IO14/IO3 through the GPIO matrix, so switching between speaker and headphones is instant and needs no reconfiguration:

- **Headphones**: the copy to the PCM5102 is on, the speaker amplifier is switched off and the ES8311 DAC is muted.
- **Speaker**: the PCM5102 pins are disconnected from I2S and held low, so the module goes into standby and stays silent.

## Headphone detection

The jack on the common purple PCM5102 module **has no detect contact**, so for automatic switching you need a 3.5 mm jack with a *switch/detect* pin (for example PJ-307, a 5-pin PJ-320D, or an "audio jack with switch" breakout):

- PCM5102 outputs L / R / GND → L / R / GND of the new jack
- the jack's **detect** contact → **IO2**, the other side of the switch → **GND**

By default "headphones plugged in" means IO2 reads low. If your jack works the other way round, turn on **Settings → Invert jack detection** (or change `HP_DETECT_PLUGGED` in `src/config.h`).

Without a detect contact, set `HP_DETECT_PIN` to `-1`. You then choose the output by hand from the control center or from the device selector (the speaker/headphone icon in the mini player).

## Battery

A single-cell 3.7 V LiPo goes on the **BAT** connector. The board charges it at about 300 mA from USB (a 1100 mAh cell takes about 4 hours). See [Battery and Power](Battery-and-Power) for how charging is detected and how the runtime is estimated.

If the voltage shown in **Settings → Battery** differs from what your multimeter reads, correct it with `BAT_OFFSET_MV` in `src/config.h`. `BAT_CAPACITY_MAH` and `BAT_CHARGE_MA` are only used for the time-to-full estimate.
