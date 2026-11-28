
# Bionic Butterfly

Wireless RC-controlled Bionic Butterfly project.

## Architecture

- Controller: ESP32 DevKit V1
- Receiver: ESP32-C3 SuperMini
- Framework: Arduino
- Build system: PlatformIO
- MVP transport: ESP-NOW
- Shared protocol: RadioProtocol

## Project Structure

firmware/controller
firmware/receiver
shared/libraries/RadioProtocol
tools/link_monitor
docs

## Requirements

- Visual Studio Code
- PlatformIO IDE extension
- Git
- ESP32 USB drivers

## MVP Firmware Status

- Controller: non-blocking four-sample ADC input, measured calibration gate, debounced ARM button, universal 12-channel mapping, periodic control, immediate persistent DISARM, and application ACK/link state.
- Receiver: strict packet/age/sequence validation, DISARM-priority safety state, independent 500 ms failsafe, stateful ACK, local butterfly waveform, and two calibrated LEDC servo outputs.
- Safety gate: built-in defaults remain uncommissioned and inhibit servo PWM. The checked-in controller/servo overrides contain measured/commissioned settings for the current hardware; the user reports safely testing throttle, steering, DISARM, board restarts, link-loss timeout, and recovery without automatic ARM on 2026-11-28. Do not reuse these hardware-specific values on another setup without measurement. OLED, buzzer, NVS, automatic pairing, and nRF24L01 remain deferred.
- PlatformIO is pinned to pioarduino ESP32 platform 53.03.13 / Arduino core 3.1.3, the verified baseline for the current ESP-NOW and LEDC APIs.

## Build Controller

cd firmware/controller
pio run

## Build Receiver

cd firmware/receiver
pio run

## Host Unit and Integration Tests

Ten host executables were validated with GCC C++17 and `-Wall -Wextra -Werror`: protocol, transport, controller link, receiver link, controller–receiver cycle, input mapping/button, controller state, receiver safety, butterfly motion, and servo output. These use assertions and fake clocks/transports/servo drivers; they do not require connected boards. `pio test` is not configured as the host runner.

Run from the repository root in PowerShell with `g++` on PATH. For example, the radio integration command used for validation is:

```powershell
$common = @('-std=c++17', '-Wall', '-Wextra', '-Werror',
    '-Ishared\libraries\RadioProtocol\src', '-Ishared\libraries\RadioTransport\src',
    '-Ifirmware\tests', '-Ifirmware\tests\support',
    '-Ifirmware\controller\include', '-Ifirmware\controller\src\transport',
    '-Ifirmware\receiver\include', '-Ifirmware\receiver\src',
    '-Ifirmware\receiver\src\transport')
$exe = Join-Path $env:TEMP ('bionic-cycle-' + [guid]::NewGuid().ToString('N') + '.exe')
try {
    g++ @common firmware\tests\test_controller_receiver_cycle.cpp `
        firmware\controller\src\transport\ControllerLink.cpp `
        firmware\receiver\src\transport\ReceiverLink.cpp `
        firmware\receiver\src\safety\ReceiverSafety.cpp -o $exe
    if ($LASTEXITCODE -ne 0) { throw 'Host compilation failed' }
    & $exe
    if ($LASTEXITCODE -ne 0) { throw 'Host test failed' }
} finally {
    if (Test-Path -LiteralPath $exe) { Remove-Item -LiteralPath $exe }
}
```

The same compiler flags and temporary-executable cleanup apply to the focused tests. Compile each test with its corresponding implementation, not firmware `main.cpp`:

| Test | Implementations and additional include directories |
| --- | --- |
| `firmware/controller/test/test_controller_link.cpp` | Controller `src/transport/ControllerLink.cpp`; shared protocol/transport, controller `include/`, `src/transport/`, and firmware `tests/` plus `tests/support/` |
| `firmware/receiver/test/test_receiver_link.cpp` | Receiver `src/transport/ReceiverLink.cpp`, `src/safety/ReceiverSafety.cpp`; shared protocol/transport, receiver `include/`, `src/`, `src/transport/`, and firmware test support |
| `firmware/controller/test/test_input_mapping.cpp` | Controller `src/input/ArmButton.cpp`, `InputManager.cpp`, `src/control/ChannelManager.cpp`; shared protocol/transport, controller `include/` and `src/` |
| `firmware/controller/test/test_controller_state.cpp` | Controller `src/control/ControllerState.cpp`; shared protocol and controller `src/` |
| `firmware/receiver/test/test_receiver_safety.cpp` | Receiver `src/safety/ReceiverSafety.cpp`; shared protocol and receiver `src/` |
| `firmware/receiver/test/test_butterfly_control.cpp` | Receiver `src/control/ButterflyController.cpp`, `src/output/ServoConfig.cpp`; shared protocol/transport, receiver `include/`, `src/control/`, and `src/output/` |
| `firmware/receiver/test/test_servo_output.cpp` | Receiver `src/output/ServoOutput.cpp`, `ServoConfig.cpp`; shared protocol/transport, receiver `include/` and `src/output/`; do not define `ARDUINO` for the fake-driver test |
| `shared/libraries/RadioProtocol/test/test_radio_protocol.cpp` | Header-only; no additional implementation |
| `shared/libraries/RadioTransport/test/test_radio_transport.cpp` | Transport `src/EspNowTransport.cpp`, `test/support/esp_mocks.cpp`; transport `src/` and `test/support/`; use transport mocks, not firmware Arduino support |

## Commissioning Before Upload or Motion

1. Verify actual board revision/pinout, installed wiring, ADC1 inputs, button polarity, regulated servo supply/common ground, and actual STA MACs against [hardware mapping](docs/HARDWARE_MAPPING.md). The corrected source pins are targets, not physical verification.
2. Controller [calibration overrides](firmware/controller/include/calibration_overrides.h) use the supplied hardware capture: CH1 bottom/top=4095/0 and CH2 left/center/right=4095/1890/0, with `measured=true`. Firmware explicitly uses the measurement settings: 12-bit ADC and 11 dB attenuation. `ChannelManager::Calibration` defaults contain the same numeric values but keep `measured=false`; missing/invalid overrides therefore keep CH1/CH5=1000 and other channels=1500. Re-measure after changing joystick hardware or ADC settings and verify mapped channel directions before operating the receiver.
3. Receiver [servo overrides](firmware/receiver/include/servo_overrides.h) currently configure maximum 4 Hz, amplitude -60/+60 degrees, differential 10 degrees, angle endpoints -90/+90 degrees, pulse endpoints 500/2500 us, zero trim, gain 1, Sine, 1000 degrees/second slew, and 20 ms output interval. The commissioning flags are enabled for the current tested hardware. These values must not be reused on another mechanism without measurement. The calculated center is the pulse midpoint plus the per-servo degree-trim converted into pulse offset; DISARM/failsafe target that trimmed center. Steering may clip against endpoints. Built-in defaults remain uncommissioned and keep PWM disabled.
4. The user reports safe physical tests on 2026-11-28 covering measured servo operation, ARM/throttle/steering/DISARM, Controller/Receiver restarts, link loss through timeout, and recovery without automatic ARM. For repeatable evidence, capture serial logs and record board IDs, actual MACs, supply measurements, pulse endpoints, and measured timeout. SG90 has no shaft feedback; safe transition tracks commanded, not measured, position.

The serial ports below are examples from earlier setup; detect the current ports before upload. No automatic upload is performed by the build.

## Controller

Port serial controller: `COM4`.

```shell
cd firmware/controller
pio run -t upload --upload-port COM4
pio device monitor -p COM4 -b 115200
```

## Receiver

Port serial receiver: `COM10`.

```shell
cd firmware/receiver
pio run -t upload --upload-port COM10
pio device monitor -p COM10 -b 115200
```

## Link Monitor

Aplikasi Windows lokal untuk memantau log serial Controller dan Receiver secara bersamaan tersedia di [tools/link_monitor](tools/link_monitor/). Monitor tidak mengubah firmware atau memakai radio tambahan. Baca [docs/LINK_MONITOR.md](docs/LINK_MONITOR.md) untuk arti metrik, batas ACK estimasi, replay, dan validasi hardware.

## Development Rules

Read these documents before implementation:

- docs/PROJECT_SPEC.md
- docs/SYSTEM_ARCHITECTURE.md
- docs/RADIO_PROTOCOL.md
- docs/CHANNEL_MAPPING.md
- docs/HARDWARE_MAPPING.md
- docs/FAILSAFE.md

Follow .github/copilot-instructions.md.
