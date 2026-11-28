
# Hardware Mapping

## Controller: ESP32 DevKit V1

| Component | GPIO |
|---|---:|
| Left joystick X | 32 |
| Left joystick Y | 33 |
| Right joystick X | 34 |
| Right joystick Y | 35 |
| OLED SDA | 21 |
| OLED SCL | 22 |
| ARM button | 13 |
| Active buzzer driver | 27 |

ARM button uses `INPUT_PULLUP` and is active-low (pressed to GND). Firmware now targets this table; the older source mapping had the four joystick GPIOs swapped in pairs, ARM on GPIO27, and buzzer on GPIO25. That source mismatch has been corrected, but wiring is not thereby validated.

All joystick inputs must use ADC1. At commissioning, trace each installed signal and record raw bottom/center/top or left/center/right values and direction before creating `calibration_overrides.h`. Until a valid measured calibration is compiled, controller ARM is inhibited and safe channel defaults are transmitted.

## Receiver: ESP32-C3 SuperMini

| Component | GPIO |
|---|---:|
| Left servo signal | 4 |
| Right servo signal | 5 |

Servo GPIO assignments and all measured pin behavior remain subject to verification on the actual boards. Confirm GPIO32-GPIO35 are ADC1-capable inputs on the selected controller board. Measure joystick ADC endpoints and center; do not use nominal ADC assumptions.

The receiver board definition remains `esp32-c3-devkitm-1`; successful compilation does not prove compatibility with the exact SuperMini revision. Verify GPIO4/GPIO5, USB mode, actual STA MACs, and LEDC waveforms on the installed boards. Servo PWM uses two Arduino core 3 LEDC outputs at 50 Hz/14-bit resolution only after measured configuration and commissioning gates pass. The 50 Hz carrier is distinct from the 0–5 Hz commanded flapping rate; validate both physically. With absent/invalid overrides no servo pin is attached to PWM. OLED and buzzer are intentionally not driven by this MVP.

## Power

- Do not power SG90 servos directly from the MCU 3.3V rail.
- Use an appropriately rated regulated 5V supply.
- Connect servo and MCU grounds.
- Use appropriate decoupling.
- Verify servo stall current and supply capacity.
- Use a driver for the 5V active buzzer.
