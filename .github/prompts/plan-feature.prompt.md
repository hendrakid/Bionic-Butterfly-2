---
name: Plan Feature
description: Create a detailed implementation plan for a new firmware feature without modifying files.
---

# Plan Feature

Act as a senior embedded systems engineer.

Create an implementation plan for the following feature:

${input:feature:Describe the feature to implement}

## Context

Read the relevant project documentation and source files before planning.

Follow:
- Repository-wide Copilot instructions.
- Applicable path-specific instructions.
- Existing architecture.
- Existing communication protocol.

## Planning Tasks

1. Understand the feature requirements.
2. Inspect existing implementation.
3. Identify affected firmware.
4. Identify affected modules and files.
5. Identify hardware dependencies.
6. Identify protocol changes, if any.
7. Evaluate timing and memory implications.
8. Identify safety implications.
9. Define error handling.
10. Define testing requirements.

## Architecture Requirements

- Keep modules loosely coupled.
- Avoid unnecessary abstractions.
- Preserve controller and receiver separation.
- Reuse shared protocol definitions.
- Avoid blocking runtime operations.
- Keep hardware configuration centralized.
- Preserve receiver-side failsafe.

## Expected Output

### 1. Feature Objective
Explain what the feature should accomplish.

### 2. Current Implementation
Describe relevant existing components.

### 3. Proposed Design
Explain how the feature should work.

### 4. Files to Create or Modify
List each file and its responsibility.

### 5. Implementation Steps
Provide an ordered implementation sequence.

### 6. Dependencies
Identify required libraries or APIs.

### 7. Risks
Identify potential compatibility, timing, communication, and safety issues.

### 8. Testing Strategy
Define unit, integration, and hardware tests.

### 9. Acceptance Criteria
Provide measurable conditions for completion.

## Constraints

- DO NOT modify files.
- DO NOT implement the feature.
- DO NOT silently change existing architecture.
- Ask for clarification if an essential requirement is missing.

Wait for approval before implementation.
