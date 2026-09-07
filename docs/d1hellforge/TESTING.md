# D1Hellforge Testing

## Known local development setup
The previous Codex environment successfully used:
- Visual Studio MSVC
- CMake
- Ninja
- vcpkg
- local GOG Diablo/Hellfire data

Exact machine paths can change; do not hard-code them into source unless they are test-only configurable paths.

## Required tests

### Save read
- valid Diablo DevilutionX save
- valid Hellfire DevilutionX save
- truncated/corrupt file
- unsupported format

### Save round-trip
- open + write without user changes
- reopen result
- compare relevant logical state
- verify DevilutionX can load the result

### Stat edit
- edit one field at a time
- min/max validation
- backup creation
- temp-file verification
- failure leaves original untouched

### Item files
- raw ItemPack round-trip
- `.dxitem` round-trip
- corruption rejection
- malformed/unknown rejection
- `.ITM` fixture detection/decode when implemented
- `.HIF` fixture detection/decode when implemented

### Inventory
- equipment slots
- inventory dimensions/occupancy
- belt restrictions
- invalid placement rejection

## Milestone completion checklist
- focused build passes
- focused automated tests pass
- `git diff --check` clean
- inspect `git status --short`
- launch D1Hellforge if UI changed
- update STATUS.md
