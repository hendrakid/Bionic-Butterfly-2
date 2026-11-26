
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

## Build Controller

cd firmware/controller
pio run

## Build Receiver

cd firmware/receiver
pio run

## Upload Controller

Port serial controller: `COM4`.

```shell
cd firmware/controller
pio run -t upload --upload-port COM4
```

## Upload Receiver

Port serial receiver: `COM10`.

```shell
cd firmware/receiver
pio run -t upload --upload-port COM10
```

## Serial Monitor

Pilih port sesuai board yang ingin dipantau:

```shell
# Controller
cd firmware/controller
pio device monitor -p COM4 -b 115200

# Receiver
cd firmware/receiver
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
