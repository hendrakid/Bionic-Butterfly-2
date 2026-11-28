
# Channel Mapping

| Channel | Input | Function |
|---|---|---|
| CH1 | Left Y | Throttle / Flapping frequency (0 to receiver-configured maximum, never above 5 Hz) |
| CH2 | Right X | Aileron / Steering |
| CH3 | Left X | Reserved; transmit 1500 until assigned |
| CH4 | Right Y | Reserved; transmit 1500 until assigned |
| CH5 | Momentary button | ARM / DISARM |
| CH6-CH12 | Reserved | Expansion |

## Analog Channels

Minimum: 1000
Center: 1500
Maximum: 2000

## CH5

1000 = DISARM
2000 = ARM

An active-low button short press is recognized on release after 20 ms debounce and before 500 ms; a held button at boot must first be released. A short press toggles the controller ARM request. Ignore ARM requests without confirmed application link, confirmed DISARM ACK, ready input samples, and measured valid calibration; the user must press again after recovery. A confirmed application ACK can report DISARMED, ARMED, or FAILSAFE: link confirmation and receiver safety state are maintained separately. OLED/buzzer display remains deferred; serial diagnostics are available. Reconnection never automatically arms.

CH1 calibration maps the full Left Y travel to bottom=1000, center≈1500, top=2000, with 5% deadzone near the bottom. The receiver maps these values to 0, half, and full configured maximum frequency: default 5 Hz, supplied override 4 Hz, hard limit 5 Hz. CH2 maps Right X left/center/right to 1000/1500/2000 with 5% deadzone on each side of center. CH3-CH4 and CH6-CH12 transmit center 1500 until assigned. Clamp calibrated values to 1000-2000 and use short non-blocking ADC averaging.

## Supplied Controller ADC Calibration

Firmware uses 12-bit ADC resolution and 11 dB attenuation on all four joystick inputs, matching the supplied measurement sketch.

| Calibration field | ADC reading | Physical position |
|---|---:|---|
| `ch1Bottom` | 4095 | Left Y bottom |
| `ch1Top` | 0 | Left Y top |
| `ch2Left` | 4095 | Right X left |
| `ch2Center` | 1890 | Right X released center, based on the final capture block |
| `ch2Right` | 0 | Right X right |

Defaults and overrides use these numbers. Defaults remain `measured=false`; the supplied override is `measured=true` based on the user's capture and direction confirmation. Missing or invalid overrides remain uncalibrated and inhibit ARM. The measured ADC direction decreases toward CH1 top and CH2 right; channel outputs still increase toward those positions. ADC readings of 0/4095 can indicate saturation and do not establish voltage accuracy or full-travel linearity. Verify CH1 bottom/top=1000/2000 and CH2 left/center/right=1000/1500/2000 on firmware after flashing. Released Left Y center is not zero throttle.

Right Y was reported as top=0 and bottom=4095, but CH4 remains reserved at 1500; no RY calibration is applied yet.

Receiver must independently validate ARM requests.
