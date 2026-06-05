# Copilot Instructions for C++ DLL Build Reviews (workspace/src)

These instructions guide Copilot reviews for C/C++ changes under this source tree.

## Primary Objective
- Ensure C++ changes build and link cleanly for both game binaries:
  - client DLL (`cl_dll`),
  - server/game DLL (`dlls`).
- Prevent behavior regressions caused by client/server drift.
- This source tree is part of the larger `hl-mods` repository, where `workspace/` is one top-level folder alongside the other game and packaging folders.

## Required Review Focus
1. Dual-target build impact
- Treat every shared or gameplay-related C++ change as potentially affecting both targets.
- Verify headers, symbols, macros, and includes are valid for each build target.
- Flag target-specific compile or link risks early.

2. Client/server parity
- Check that logic touching prediction, HUD/events, weapons, player state, networking, and rules remains consistent between `cl_dll` and `dlls`.
- Call out mismatched constants, enums, message formats, or conditional code paths.

3. ABI and interface stability
- Highlight changes to shared structs, serialization/network payloads, exported interfaces, and virtual class layouts.
- Flag any ordering/packing changes that can break compatibility.

4. C++ correctness and safety
- Prioritize null safety, bounds checks, lifetime ownership, initialization order, and error handling.
- Watch for undefined behavior, implicit narrowing, sign issues, and unsafe casts.

5. Build system and project files
- Review updates in project/build config files to ensure both DLL targets are still included and configured.
- Flag missing source additions/removals that could cause one target to silently diverge.

## Validation Expectations
- Require practical validation notes for non-trivial changes:
  - build client DLL,
  - build server DLL,
  - run a basic in-game smoke test that exercises changed behavior.
- If a full build was not verified, explicitly state that as a risk.

## Review Output Format
- Report findings first, ordered by severity: Critical, High, Medium, Low.
- For each finding include:
  - issue summary,
  - why it matters for client/server stability,
  - affected file(s),
  - concrete fix guidance.
- If no major issues are found, say so explicitly and list unverified build/test areas.

## Scope
- Prioritize C/C++ source under `workspace/src`, especially:
  - `cl_dll/`,
  - `dlls/`,
  - shared headers and shared game logic.
- Avoid requesting broad refactors unless they are needed for correctness, build health, or parity.

## Baseline Evolution
As review history grows, add:
- known fragile modules,
- required smoke-test scenarios,
- branch-specific build gates,
- recurring compiler/linker failure patterns.
