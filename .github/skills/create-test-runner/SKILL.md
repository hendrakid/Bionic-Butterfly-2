---
name: create-test-runner
description: "Propose or use user-provided Bionic Butterfly test scenarios and create an automated test runner only after explicit user approval of creation and scenario scope."
---
# Create Test Runner

Create an automated test runner for the scenarios requested by the user. Loading this skill, proposing tests, or approving a feature does not authorize creating a runner.

## Mandatory Approval Gate

Before creating or changing runner files, test harnesses, dependencies, or configuration:

1. Inspect relevant source, tests, configuration, and specifications without modifying files.
2. Use scenarios provided by the user. If none are provided, suggest a small set based on the feature and current implementation.
3. Explain the proposed scenarios, runner location, affected files, test method, limitations, and hardware requirements.
4. Use `ask_user` to obtain both the decision to create the runner and the approved scenario scope. Include a create/do-not-create choice and scenario selection; allow the user to provide or revise scenarios.
5. Proceed only when the user explicitly chooses creation and approves the scenarios. Do not treat a declined or unanswered form as consent. If the user declines creation, provide the proposed test approach without modifying files. If cancelled, stop.

Do not add unapproved scenarios. If approval is already recorded for this exact runner and scenario scope in the current conversation, do not ask again. Material changes to scope or hardware behavior require renewed approval.

## Context

Read relevant portions of:
- `.github/copilot-instructions.md` and applicable `.github/instructions/`.
- `docs/PROJECT_SPEC.md`, `docs/SYSTEM_ARCHITECTURE.md`, `docs/RADIO_PROTOCOL.md`, and `docs/FAILSAFE.md`.
- `docs/CHANNEL_MAPPING.md` and `docs/HARDWARE_MAPPING.md` when applicable.
- Active controller and receiver sources, shared libraries, PlatformIO configurations, and existing tests.

Distinguish active source from old `.pio` build artifacts. Read timing and protocol values from current specifications and configuration; report mismatches instead of silently changing firmware. Do not assume a transport or harness already exists.

## Scenario Definition

For each approved scenario define:
- Preconditions and required hardware state.
- Actions and expected events.
- Observable evidence and measurable PASS/FAIL criteria.
- Per-step deadlines and timing tolerances.
- Cleanup and the expected final receiver state.

Possible scenarios, offered only when relevant:
- Valid control packets and matching application ACKs.
- DISARM to ARM to DISARM through the actual radio and safety path.
- Link loss and receiver failsafe.
- Reconnection without automatic ARM.
- Invalid, duplicate, stale, or unsupported-version packet rejection.

Identify unresolved behavior, such as ACK deadlines, link-health criteria, restart sequence handling, or recognizing a new ARM request. Ask for essential decisions rather than inventing production requirements.

## Implementation After Approval

- Reuse existing test tooling before introducing a new runner or dependency.
- For physical two-MCU tests, prefer a PC runner controlling minimal firmware harnesses through separate serial ports when both devices must be observed.
- Keep transport, protocol validation, application safety, and runner responsibilities separate.
- The runner must not force receiver state or bypass validation to make a test pass. Hardware integration tests must use the real control packet and radio path.
- Isolate test-only harness behavior from normal firmware using explicit test configurations or build flags.
- Use structured events with sequence numbers, receiver state, timestamps, and rejection reasons where needed. Keep callbacks short; emit logs in the main context without blocking radio scheduling.
- Correlate ACKs with transmitted sequences and the current test phase. Do not accept old ACKs or radio send success as application confirmation.
- Use bounded waits, monotonic deadlines, and event-based assertions. Report failures with useful evidence.
- Do not compare unsynchronized MCU clocks. Measure RTT on the controller and failsafe elapsed time on the receiver.
- Observe receiver state independently through serial when control transmission stops; missing ACKs alone do not prove failsafe.
- Detect unexpected restarts and serial disconnections; report inconclusive or failed tests honestly.
- Make serial ports and peer MACs explicit inputs. Never invent GPIOs, MAC addresses, or mechanical safe positions.
- On exit, request and verify DISARM where possible. If unavailable, stop control transmission and report that the safe state was not confirmed; do not assume cleanup succeeded.
- Save a concise test summary and diagnostic logs as runner outputs. Document usage and prerequisites in directly related documentation.
- Do not modify the wire layout, protocol version, or unrelated features.

## Hardware Execution Gate

Approval to create a runner is not permission to upload firmware, operate physical hardware, or execute ARM scenarios.

Before hardware execution, use `ask_user` to confirm the target devices, ports, and permission to run the selected scenarios. For initial ARM tests require confirmation that servos are disconnected. Tests with actuators connected need separately confirmed power, mechanical limits, safe positions, and operating conditions. Do not execute when these prerequisites are unresolved.

## Validation and Report

- Run focused existing tests and new tests for introduced runner behavior, such as event correlation, deadlines, and failure cleanup, using simulated inputs where hardware is unavailable.
- Build each affected PlatformIO environment and inspect warnings if firmware harnesses are changed.
- Documentation-only changes do not require firmware builds.
- Separate software validation from physical MCU results; compilation is not hardware validation.
- Report approved scenarios, changed files, commands executed, results, and unverified hardware assumptions.
- If hardware execution is not authorized or devices are unavailable, deliver the approved runner with reproducible instructions and explicitly state that physical validation remains pending.
