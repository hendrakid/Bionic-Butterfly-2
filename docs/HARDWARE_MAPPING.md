
# Hardware Mapping

## Controller: ESP32 DevKit V1

| Component | GPIO |
|---|---:|
| Left joystick X | 34 |
| Left joystick Y | 35 |
| Right joystick X | 32 |
| Right joystick Y | 33 |
| OLED SDA | 21 |
| OLED SCL | 22 |
| ARM button | 27 |
| Active buzzer driver | 25 |

All joystick inputs must use ADC1.

## Receiver: ESP32-C3 SuperMini

| Component | GPIO |
|---|---:|
| Left servo signal | 4 |
| Right servo signal | 5 |

Receiver GPIOs are provisional.

Verify actual board pinout before implementation.

## Power

- Do not power SG90 servos directly from the MCU 3.3V rail.
- Use an appropriately rated regulated 5V supply.
- Connect servo and MCU grounds.
- Use appropriate decoupling.
- Verify servo stall current and supply capacity.
- Use a driver for the 5V active buzzer.
