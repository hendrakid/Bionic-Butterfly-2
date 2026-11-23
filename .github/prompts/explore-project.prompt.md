---
name: Explore Project
description: Analyze project architecture, firmware, dependencies, and potential issues without modifying files.
---

# Explore Project

Act as a senior embedded firmware architect.

Analyze the entire Bionic Butterfly repository.

## Mandatory Context

Read:
- .github/copilot-instructions.md
- All applicable .github/instructions/
- docs/PROJECT_SPEC.md
- docs/SYSTEM_ARCHITECTURE.md
- docs/RADIO_PROTOCOL.md
- docs/CHANNEL_MAPPING.md
- docs/HARDWARE_MAPPING.md
- docs/FAILSAFE.md

## Tasks

1. Understand the repository structure.
2. Analyze controller firmware architecture.
3. Analyze receiver firmware architecture.
4. Inspect shared RadioProtocol.
5. Inspect PlatformIO configurations.
6. Review library dependencies and compatibility.
7. Identify architectural inconsistencies.
8. Identify communication reliability risks.
9. Identify timing, memory, and concurrency risks.
10. Identify hardware and safety concerns.

## Important Considerations

- Controller must remain receiver-agnostic.
- Receiver owns device-specific behavior.
- Both firmware projects must use the shared protocol.
- ESP-NOW is the MVP transport.
- Wi-Fi channel is fixed at 6.
- Support 12 generic channels.
- CH5 controls ARM/DISARM.
- Receiver must independently implement failsafe.

## Constraints

- DO NOT modify any files.
- DO NOT implement features.
- DO NOT invent missing specifications.
- DO NOT assume hardware behavior without evidence.
- Clearly distinguish confirmed issues from potential risks.

## Expected Output

Provide:

1. Project overview.
2. Architecture assessment.
3. Controller assessment.
4. Receiver assessment.
5. Communication assessment.
6. PlatformIO and dependency assessment.
7. Safety assessment.
8. Missing specifications.
9. Prioritized recommendations.

Classify findings as:
- Critical
- High
- Medium
- Low

Keep recommendations practical and avoid unnecessary abstractions.
