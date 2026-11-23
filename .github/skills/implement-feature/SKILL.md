---
name: implement-feature
description: "Implement user-requested ESP32 Arduino firmware features in this repository while preserving its architecture, shared protocol, and receiver safety requirements."
---
# Implement Feature

Act as a senior embedded firmware developer.

Implement the feature described in the current user request and conversation. Treat the request as authorization for that scope; ask for clarification only when an essential behavior, hardware, protocol, or safety decision is unresolved.

## Mandatory Preparation

1. Read repository instructions.
2. Read relevant project documentation.
3. Inspect existing implementation.
4. Use any agreed implementation plan. If none exists and the change is complex, state the approach, affected files, and safety/compatibility implications before editing.
5. Identify the exact files that need modification.

## Implementation Rules

### Code Quality
- Write clean, readable C++.
- Follow existing naming conventions.
- Keep functions small and focused.
- Avoid unnecessary abstractions.
- Avoid unnecessary dynamic memory allocation.
- Avoid magic numbers.
- Use constexpr for configuration values.
- Handle errors explicitly.

### Firmware Architecture
- Preserve controller/receiver separation.
- Keep transport independent from application logic.
- Reuse shared protocol definitions.
- Keep hardware configuration centralized.
- Avoid unnecessary changes to unrelated modules.

### Runtime
- Avoid blocking delay() in control paths.
- Use non-blocking scheduling.
- Keep wireless callbacks short.
- Handle concurrent data access safely.
- Avoid expensive operations inside timing-sensitive callbacks.

### Safety
- Preserve receiver-side failsafe.
- Validate all received packets.
- Never use stale or invalid channel data.
- Never automatically ARM after reconnection.
- DISARM must take priority.
- Never assume servo mechanical endpoints.

## Implementation Workflow

1. Implement the smallest functional change.
2. Integrate with existing modules.
3. Handle initialization and runtime errors.
4. Update relevant documentation.
5. Build affected firmware.
6. Run relevant unit tests.
7. Review compiler warnings.
8. Inspect the complete diff.

## Testing

For controller:
- Verify input mapping.
- Verify channel values.
- Verify radio scheduling where relevant.

For receiver:
- Verify packet validation.
- Verify safety state transitions.
- Verify failsafe behavior.
- Verify output constraints.

For shared protocol:
- Verify packet layout.
- Verify serialization and validation.
- Verify compatibility between firmware.

Only run tests that are supported by the available environment.

## Constraints

- Do not silently modify protocol definitions.
- Do not introduce unapproved dependencies.
- Do not modify unrelated firmware.
- Do not claim physical hardware testing without actual hardware verification.
- Stop and report if the implementation plan conflicts with existing specifications.

## Expected Final Report

1. Feature implementation summary.
2. Files created.
3. Files modified.
4. Design decisions.
5. Build results.
6. Unit test results.
7. Remaining hardware tests.
8. Known limitations.
