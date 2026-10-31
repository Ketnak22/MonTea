# Mon Tea

A small device that watches your tea and beeps when it has cooled down enough
to drink. An IR sensor reads the cup from a distance, so nothing touches the
tea.

Put a hot cup under the sensor and walk away. There is nothing to press.

## Hardware

- Waveshare RP2040-Zero
- MLX90614 IR temperature sensor
- SSD1306 OLED, 128x32, I2C
- Active-low buzzer (pin low = sound)
- 2 push buttons, each between its pin and GND
- LED with a resistor for the error signal (optional, the screen and the RGB
  LED show faults too)

### Wiring

| Part            | RP2040-Zero pin |
| --------------- | --------------- |
| OLED SDA        | GP0             |
| OLED SCL        | GP1             |
| MLX90614 SDA    | GP0             |
| MLX90614 SCL    | GP1             |
| Buzzer          | GP14            |
| Error LED       | GP5             |
| Left button     | GP27            |
| Right button    | GP26            |

The RGB LED is the one already on the board (GP16).

### Better: give the sensor its own bus

The MLX90614 is limited to 100 kHz, so on a shared bus the display has to slow
down too, and the sensor still sees every frame. If you get sensor errors or
odd readings, move the sensor to its own bus:

1. Connect MLX90614 SDA to GP2 and SCL to GP3.
2. Set `MLX_ON_OWN_BUS` to `1` at the top of `src/main.cpp`.

## How it behaves

| Screen            | RGB LED | What is happening                                 |
| ----------------- | ------- | ------------------------------------------------- |
| `Waiting for tea` | off     | Idle, stays quiet at any temperature              |
| `Watching 55`     | amber   | Hot cup found, 55 is the target                   |
| `Cup lifted`      | amber   | You took the cup, watching continues when it's back |
| `Tea is ready!`   | green   | At the target, beeping                            |
| `Sensor error`    | red     | No good reading for 2 seconds, keeps retrying     |
| `Set target`      | blue    | Editing a setting                                 |

- Watching starts by itself once the reading stays 5 degrees above the target
  for 3 seconds.
- After about a minute of watching, the top line adds an estimate like `~7 min`.
- Tea at or below the target for 3 seconds sets off the alarm.
- The alarm stops when you press any button, pick up the cup, or after a minute.
- The screen turns off after 5 idle minutes. A button press or a hot cup wakes
  it up.

## Buttons

- **Any press:** open the target setting. This first press changes nothing.
- **Left / right while editing:** target down / up by 1 degree. Hold to repeat.
- **Both held for a second:** switch to the calibration offset.
- **During the alarm:** any press stops it.

Settings close and save by themselves 5 seconds after the last press.

## Calibration

The sensor only sees the surface of the tea, which is cooler than the drink, so
the reading will probably be a bit low. To correct it once:

1. Pour a hot cup and stir it.
2. Measure the tea with a kitchen thermometer.
3. Hold both buttons for a second. The top line shows `now` and the live
   reading.
4. Adjust the offset until the reading matches the thermometer.

Then taste a few cups and set the target to what you like. The default of 55 is
only a starting point.

## Placement

- Point the sensor straight at the tea, not at the rim of the mug.
- Keep it close enough that the tea fills most of what it sees. Further away
  it averages in the mug and the table, and reads low.
- Don't mount it directly above the steam, condensation on the window throws
  the reading off. A slight angle from the side helps.
- Shiny metal reads badly in IR, since the sensor is calibrated for dull
  surfaces. Tea and ceramic are fine, the inside wall of a steel mug is not.

## Building

This is a [PlatformIO](https://platformio.org/) project.

```
pio run -t upload       # build and flash
pio test -e native      # run the logic tests on the PC, needs gcc on the PATH
```

The logic that decides when to beep is in `lib/TeaCore` and has no Arduino code
in it. The timings and margins are constants at the top of each class, and they
are starting values, not measured ones.

Uncomment `#define DEBUG` in `src/main.cpp` for serial output. The board then
waits for a USB serial connection at boot.

The LED colour order is `NEO_RGB`. If red and green look swapped on your board,
change it to `NEO_GRB` in `src/main.cpp`.
