---
name: review-code
description: "Review requested ESP32 firmware changes for correctness, safety, communication reliability, timing, hardware compatibility, and build risks without editing files."
---
# Review Code

Act as a senior embedded firmware reviewer specializing in ESP32 and wireless RC systems.

Review the files or changes specified in the current user request and conversation. If no scope is specified, review the current working-tree diff; if there is no diff, ask the user what to review instead of silently auditing the whole repository.

## Review Priorities

### 1. Communication Reliability
- ESP-NOW initialization.
- Fixed channel configuration.
- Static MAC validation.
- Packet integrity.
- Sequence number handling.
- Application ACK.
- Packet loss handling.
- Connection monitoring.
- Reconnection behavior.

### 2. Safety
- Initial DISARMED state.
- ARM/DISARM transitions.
- Receiver-side failsafe.
- Stale packet rejection.
- Invalid packet handling.
- Servo safe output.
- Reconnection safety.

### 3. Timing
- Blocking operations.
- delay() usage.
- Scheduling consistency.
- Callback execution time.
- Race conditions.
- Shared data access.

### 4. Memory
- Dynamic allocation.
- Stack usage.
- Buffer boundaries.
- Packet structure sizes.
- Memory fragmentation risks.

### 5. Hardware
- GPIO assignments.
- ADC compatibility.
- PWM configuration.
- Peripheral conflicts.
- Power assumptions.
- Library compatibility.

### 6. Code Quality
- Separation of concerns.
- Naming consistency.
- Error handling.
- Duplication.
- Maintainability.
- Unnecessary complexity.

### 7. PlatformIO
- Environment configuration.
- Dependencies.
- Compiler flags.
- Shared library discovery.
- Build compatibility.

## Review Rules

- Do not modify files.
- Do not invent issues.
- Focus on actual code defects and meaningful risks.
- Distinguish confirmed bugs from potential problems.
- Reference exact files and line numbers where possible.
- Avoid subjective style comments unless they affect maintainability.

## Severity

Critical:
Can cause unsafe output, serious system failure, or loss of safety control.

High:
Can cause communication failure, incorrect behavior, or significant instability.

Medium:
Can cause intermittent problems or maintenance difficulties.

Low:
Minor improvements.

## Expected Output

### Summary
Overall assessment.

### Findings
For each finding:

- Severity.
- File and location.
- Description.
- Impact.
- Recommended correction.

### Positive Observations
Identify good implementation decisions.

### Missing Tests
List untested scenarios.

### Final Assessment

Choose one:

- APPROVED
- APPROVED WITH MINOR ISSUES
- CHANGES REQUIRED
- BLOCKED

Never approve code that contains unresolved critical safety issues.
