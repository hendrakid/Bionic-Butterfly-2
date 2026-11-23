---
name: Shared Radio Protocol Rules
description: Rules for the shared controller-receiver protocol.
applyTo: "shared/libraries/RadioProtocol/**/*"
---

# Protocol Rules

This library is the single source of truth for both firmware projects.

- Maintain identical protocol definitions.
- Use fixed-width integer types.
- Avoid pointers and dynamic strings.
- Avoid platform-dependent serialized objects.
- Define explicit packet types.
- Define protocol version.
- Define channel count and ranges centrally.
- Validate all received fields.
- Handle sequence rollover.
- Keep packet structures compact.
- Update docs/RADIO_PROTOCOL.md when packet layouts change.

Any breaking protocol change must be explicitly versioned and reviewed.
