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
- Exception: renumbering `MUTATOR_*` IDs in `common/const.h` and `INSTANT_*` values in `dlls/gamerules.cpp` to keep them alphabetical is intended (see item 6).

4. C++ correctness and safety
- Prioritize null safety, bounds checks, lifetime ownership, initialization order, and error handling.
- Watch for undefined behavior, implicit narrowing, sign issues, and unsafe casts.

5. Build system and project files
- Review updates in project/build config files to ensure both DLL targets are still included and configured.
- Flag missing source additions/removals that could cause one target to silently diverge.
- Project files of record: `projects/vs2019/hldll.vcxproj` + `.filters`, `projects/vs2019/hl_cdll.vcxproj` + `.filters`, `linux/Makefile.hldll`, `linux/Makefile.hl_cdll`. A new `.cpp` must appear in the matching `.vcxproj`, `.vcxproj.filters` and Linux makefile.
- `vs2019` is the only Visual Studio project set (`projects/vs2010` was removed). Legacy MSVC 6 `.dsp`/`.dsw` files are not maintained; do not request updates to them.

6. Mutator IDs and mode filters
- Mutator IDs are renumbered on purpose. `MUTATOR_*` IDs follow alphabetical order, so a new mutator is inserted at its alphabetical place and every later ID shifts by one. Do not flag the shift or ask to restore old values and append the new mutator at the end. Numeric IDs (`sv_addmutator <number>`, `gmsgAddMutator`, vote numbers, the `startspot` mirror, grave_bot) only have to agree within one release: client, server and bot ship together, and configs should use mutator names.
- Instead, check that the lists in this tree moved in lockstep: `common/const.h`, `g_szMutators[]` (`dlls/gamerules.cpp`), `sMutators[]` (`cl_dll/vgui_TeamFortressViewport.cpp`) and `MAX_MUTATORS` / `MAX_MUTATORS_CL`. The bot's `const.h`, `settings.scr` and docs live in the parent `workspace/` repo.
- A new mutator (full `MUTATOR_*` or instant `INSTANT_*`) must come with a review of every game mode's filter: `CHalfLifeMultiplay::MutatorAllowed` (base), `CHalfLifeRules::MutatorAllowed` (single player) and each `*_gamerules.cpp` override. Modes without an override inherit the base.
- Blocks follow `if (strstr(mutator, g_szMutators[MUTATOR_X - 1]) || atoi(mutator) == MUTATOR_X) return FALSE;` placed before the base-class chain. `strstr` also matches longer names that contain the blocked one.
- Instants have no `MutatorAllowed` path; mode restrictions must be gates inside the instant itself.
- Instant indices (`instant_mutator_e` in `dlls/gamerules.cpp`, used by `sv_addinstantmutator <index>`) are also renumbered on purpose: they follow the alphabetical (ASCII) order of the name in the trailing comment on each enum line, so a new instant is inserted at its alphabetical place and every later value shifts by one. Do not ask to append it at the end; flag an entry that is out of order or missing its name comment, or a `case` in the `AddInstantMutator` switch that is not in enum order. The index lists in `server_commands.txt`, `settings.scr`, `readme.txt` and the docs live in the parent `workspace/` repo.
- Flag a new mutator with no recorded review ("Mode filters" line in `workspace/ai/mutator_system.md`).

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
