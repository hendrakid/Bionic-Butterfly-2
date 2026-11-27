
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

ARM button uses `INPUT_PULLUP` and is active-low (pressed to GND).

All joystick inputs must use ADC1.

## Receiver: ESP32-C3 SuperMini

| Component | GPIO |
|---|---:|
| Left servo signal | 4 |
| Right servo signal | 5 |

Servo GPIO assignments and all measured pin behavior remain subject to verification on the actual boards. Confirm GPIO32-GPIO35 are ADC1-capable inputs on the selected controller board. Measure joystick ADC endpoints and center; do not use nominal ADC assumptions.

## Power

- Do not power SG90 servos directly from the MCU 3.3V rail.
- Use an appropriately rated regulated 5V supply.
- Connect servo and MCU grounds.
- Use appropriate decoupling.
- Verify servo stall current and supply capacity.
- Use a driver for the 5V active buzzer.
