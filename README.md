# MonTea

Short for Monitor Tea.

> PL: Małe urządzenie, które pilnuje herbaty i piszczy, gdy wystygnie do
> wybranej temperatury. Czujnik podczerwieni mierzy dno kubka, więc nic nie
> dotyka herbaty.

**Status:** working prototype, built and used on my desk. Firmware and logic
tests are in this repo, the enclosure files are not (see [Enclosure](#enclosure)).

A small device that watches your tea and beeps when it has cooled down enough
to drink. An IR sensor in the case reads the bottom of the cup, so nothing
touches the tea.

Put a hot cup on top and walk away. There is nothing to press.

<table>
  <tr>
    <td><img src="docs/photo.jpg" width="400" alt="A glass of tea on the device, screen showing Watching 50"></td>
    <td><img src="docs/top.jpg" width="400" alt="The device from above with the sensor window, screen showing Waiting for tea"></td>
  </tr>
  <tr>
    <td align="center">Watching a cup</td>
    <td align="center">Waiting, with the sensor in the middle</td>
  </tr>
</table>

## Hardware

- Waveshare RP2040-Zero
- MLX90614 IR temperature sensor
- SSD1306 OLED, 128x32, I2C
- Active-low buzzer (pin low = sound)
- 2 buttons that read LOW at rest and HIGH when pressed. For buttons wired
  from the pin to GND instead, set `BUTTON_PRESSED_LEVEL` in `src/main.cpp` to
  `LOW`
- LED with a resistor for the error signal (optional, the screen shows faults
  too)

### Wiring

| Part            | RP2040-Zero pin |
| --------------- | --------------- |
| OLED SDA        | GP0             |
| OLED SCL        | GP1             |
| MLX90614 SDA    | GP0             |
| MLX90614 SCL    | GP1             |
| Buzzer          | GP14            |
| Error LED       | GP5             |
| Left button     | GP26            |
| Right button    | GP27            |

### Optional: give the sensor its own bus

This isn't required, the device works just fine with the wiring above.

The MLX90614 is limited to 100 kHz, so on a shared bus the display has to slow
down too, and the sensor still sees every frame. If you ever get sensor errors
or odd readings, move the sensor to its own bus:

1. Connect MLX90614 SDA to GP2 and SCL to GP3.
2. Set `MLX_ON_OWN_BUS` to `1` at the top of `src/main.cpp`.

## Enclosure

The case in the photos is 3D printed in PLA. I would not recommend that:
the bottom of a hot cup can reach 60-70 °C, and PLA starts to soften around
60 °C. Use PETG, or better ASA.

The STL files are not in this repo, on purpose. My design was not good
enough to pass on: some dimensions were assumed instead of measured, so after
printing it I had to fix the fit with hot glue. If you build your own case,
measure your modules first. The parts that matter most are the sensor can
height, the button bodies, and the position of the sensor hole, which has to
sit in the middle of where the cup stands.

## How it behaves

| Screen            | What is happening                                   |
| ----------------- | --------------------------------------------------- |
| `Waiting for tea` | Idle, stays quiet at any temperature                |
| `Watching 50`     | Hot cup arrived, 50 is the target                   |
| `Cup lifted`      | You took the cup, watching continues when it's back |
| `Tea is ready!`   | At the target, beeping                              |
| `Sensor error`    | No good reading for 2 seconds, keeps retrying       |
| `Set target`      | Editing a setting                                   |

- Watching starts by itself when the reading shoots up by 3 degrees within a
  few seconds and stays up for 3 seconds. That is what a cup arriving looks
  like, so it works at any target. A cup that was already standing there, or
  slow warming, is not picked up.
- For 10 minutes after an alarm, only a cup above the target starts watching,
  so putting your cup back after a sip doesn't set it off again.
- After about a minute of watching, the top line adds an estimate like `~7 min`.
- Tea at or below the target for 3 seconds sets off the alarm, once the reading
  has dropped half a degree from its peak. A glass warms up slowly, so a cup
  that arrives below the target only beeps when it starts cooling.
- The alarm stops when you press any button, pick up the cup, or after a minute.
- The screen turns off after 5 idle minutes. A button press or a hot cup wakes
  it up.
- The green error LED blinks once for half a second at power-up as a self-test.
  After that it is on while the `Sensor error` screen shows and goes off when
  readings come back. If it stays on from boot with a blank screen, the display
  was not found.

## Buttons

- Any press opens the target setting. This first press changes nothing.
- While editing, left lowers the target and right raises it, 1 degree at a
  time. Hold to repeat.
- Holding both for a second switches to the calibration offset.
- During the alarm, any press stops it.

Settings close and save by themselves 5 seconds after the last press.

## Calibration

You can usually skip this and leave the offset at 0.

> **The offset corrects the sensor, not the tea.** The device measures the cup,
> and a cup cools differently from the liquid in it, so don't match the reading
> to a thermometer in the tea.

If the sensor itself reads wrong, hold both buttons for a second and adjust the
offset until the reading matches a room thermometer, with an empty cup that has
been sitting at room temperature.

For the cup, change the target instead. The default of 50 suits a glass cup, so
taste a few cups in anything else and set what you like.

## Placing the cup

- Stand the cup in the middle of the sensor hole. Off to the side, the sensor
  also sees the case around it and reads low.
- The sensor sees the bottom of the cup, not the tea. IR doesn't pass through
  glass, so even with a glass cup it reads the glass.
- A cup with a thick or hollow base reads lower and reacts later than the tea
  inside. A flat, thin bottom works best.
- Shiny metal reads badly in IR, since the sensor is calibrated for dull
  surfaces. Glass and ceramic are fine, the bottom of a steel mug is not.
- Keep the sensor hole clean. A drop of tea on the sensor throws the reading
  off.

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

### RGB LED

The RGB LED on the board (GP16) shows the state too, but it's hidden inside the
case. If you build it with the board visible, it works as is: amber while
watching or with the cup lifted, green when the tea is ready, red on a sensor
error, blue while editing a setting, and off while waiting.

The LED colour order is `NEO_RGB`. If red and green look swapped on your board,
change it to `NEO_GRB` in `src/main.cpp`.

## What I learned

I designed the enclosure from datasheet and shop dimensions. Several were off
by a millimetre or two, which is enough to stop a button or the sensor from
fitting. Next time I would measure every module with calipers first and print
a small test piece before the full case.

The sensor reads the cup, not the tea, so calibrating against a thermometer in
the tea made no sense. The offset only corrects the sensor, and the target
temperature is a matter of taste.

## License

MIT, see [LICENSE](LICENSE).
