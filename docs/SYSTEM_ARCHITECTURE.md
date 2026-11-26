## Transport Headers

- Include `IRadioTransport.h` for the transport contract, configuration, frames, statistics, and constants without ESP32 hardware dependencies.
- Include `EspNowTransport.h` when using the concrete ESP-NOW implementation. It already includes the transport interface.
- Use explicit headers internally. An umbrella header may be introduced in the future if it provides a clear benefit without adding unwanted hardware dependencies.

```mermaid
flowchart TD
    A[Controller Hardware] --> B[Input Manager]
    B --> C[Channel Manager]
    C --> D[Controller State Manager]
    D --> E[Radio Transport]
    E <-->|ESP-NOW Channel 6| F[Receiver Transport]
    F --> G[Packet Validator]
    G --> H[Receiver Safety Manager]
    H --> I[Butterfly Controller]
    I --> J[Servo Left]
    I --> K[Servo Right]
    H --> L[Application ACK]
    L --> E
```