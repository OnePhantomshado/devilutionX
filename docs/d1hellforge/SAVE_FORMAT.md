# Save Format Notes

Only confirmed findings should be kept here. Investigation history belongs elsewhere.

## DevilutionX
- Use current DevilutionX save/archive APIs and serialization code instead of inventing an independent format description when practical.
- Relevant source areas include `Source/loadsave.cpp`, `Source/pfile.cpp`, MPQ/archive code, and codec code.
- Player serialization contains paired read/write operations for character identity, class, base/current attributes, stat points, spells, inventory/equipment, and additional state.
- Do not assume a single `hero` record is authoritative for every save mode without confirming the current save path.

## Codec
- Reuse the DevilutionX/Diablo codec implementation.
- The codec uses the Diablo-specific hash/SHA behavior; do not replace it with a generic SHA implementation.

## Format detection
Primary target: DevilutionX MPQ-backed save format.
Classic/original support is later and must be fixture/source verified before using field offsets.

## Safe writing requirement
Every write path must:
1. create a timestamped backup,
2. write to a temporary file,
3. reopen and validate the temporary file,
4. replace the original only after validation.

Suggested backup naming:
`Backups/<filename>_YYYY-MM-DD_HH-MM-SS.<ext>`
