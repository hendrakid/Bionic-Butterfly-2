---
name: Debug Firmware
description: Systematically investigate and diagnose firmware bugs using evidence.
---

# Debug Firmware

Act as a senior ESP32 firmware debugging engineer.

Investigate the following problem:

${input:problem:Describe the observed problem}

## Debugging Context

Target firmware:
${input:target:Controller / Receiver / Shared Protocol}

Observed behavior:
${input:behavior:Describe expected versus actual behavior}

Error logs:
${input:logs:Paste compiler output, serial logs, or error messages}

## Debugging Methodology

### Step 1: Understand
- Read relevant repository instructions.
- Inspect affected source files.
- Understand the expected behavior.
- Identify recent relevant changes.

### Step 2: Classify

Determine the likely category:

- Compilation error.
- Dependency problem.
- Hardware configuration.
- GPIO conflict.
- ADC/PWM issue.
- ESP-NOW initialization.
- Packet transmission/reception.
- Protocol mismatch.
- Timing issue.
- Race condition.
- ARM/DISARM logic.
- Failsafe behavior.
- Power supply problem.

### Step 3: Investigate

- Trace the execution flow.
- Inspect relevant API usage.
- Verify framework and library compatibility.
- Check packet sizes and field values.
- Check scheduling and callback behavior.
- Compare actual logs against expected behavior.

Do not assume the first suspected cause is correct.

### Step 4: Root Cause

Identify:
- Confirmed root cause.
- Supporting evidence.
- Other possible contributing factors.
- Missing diagnostic information.

### Step 5: Propose Correction

Provide:
- Minimal correction.
- Affected files.
- Potential side effects.
- Required regression tests.

### Step 6: Verify

After an approved correction:
- Build affected firmware.
- Run relevant tests.
- Inspect compiler warnings.
- Verify related functionality.
- Report any remaining hardware validation.

## Safety Restrictions

- Never bypass receiver failsafe to hide communication problems.
- Never disable packet validation as a debugging shortcut.
- Never automatically ARM during debugging.
- Never remove safety checks to make tests pass.
- Never assume successful compilation means hardware functionality.

## Constraints

- Prefer root-cause fixes over workarounds.
- Avoid unrelated refactoring.
- Do not invent serial logs or test results.
- Do not claim hardware tests without actual hardware.
- If evidence is insufficient, request the specific missing information.

## Expected Output

1. Problem summary.
2. Error classification.
3. Investigation findings.
4. Root cause.
5. Proposed correction.
6. Files affected.
7. Verification steps.
8. Remaining uncertainties.
