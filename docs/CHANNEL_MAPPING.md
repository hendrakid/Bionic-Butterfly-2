
# Channel Mapping

| Channel | Input | Function |
|---|---|---|
| CH1 | Left Y | Throttle / Flapping frequency (0-5 Hz) |
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

Short press toggles the controller ARM request. Debounce the button and detect short presses. Ignore ARM requests while the application link is unconfirmed; the user must press again after recovery. A confirmed application ACK can report DISARMED, ARMED, or FAILSAFE: link confirmation and receiver safety state are displayed separately, and FAILSAFE is never shown as ARMED. Reconnection never automatically arms.

CH1 calibration maps the full Left Y travel (bottom=1000, center≈1500, top=2000) to 0-5 Hz, with 5% deadzone near the bottom. CH2 maps Right X left/center/right to 1000/1500/2000 with 5% deadzone on each side of center. CH3-CH4 and CH6-CH12 transmit center 1500 until assigned. Clamp calibrated values to 1000-2000 and use short non-blocking ADC averaging.

Receiver must independently validate ARM requests.
