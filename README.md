# DevilutionX-Hellgate + D1Hellforge

This repository contains **DevilutionX-Hellgate**, a modified DevilutionX build,
and **D1Hellforge**, a Windows save and item editor built on DevilutionX's item,
archive, and content logic.

Hellgate is a build identity, not a third game mode. Diablo and Hellfire remain
the supported content modes. The project requires data from a legally owned copy
of Diablo; no Blizzard game data or MPQ files are included.

## D1Hellforge v0.3.0

D1Hellforge currently provides:

- DevilutionX Diablo and Hellfire single-player save discovery and editing.
- Character-stat editing with class-aware limits.
- Graphical equipment, backpack, belt, and shared-stash interfaces.
- Verified writes using backup, temporary write, reopen, and final verification.
- Item movement, copy, delete, import, export, direct editing, and catalog creation.
- A shared Item Workshop with deterministic generation, Reforge, and
  **I'm Feeling Lucky** advanced editing.
- Fast and Compatible deterministic Reforge engines.
- `.dxitem`, raw ItemPack, Diablo ITM01, and Hellfire HIF item adapters.
- Diablo and Native UI presentation modes.

Classic GOG/retail Diablo and Hellfire save writing remains experimental. Always
retain an independent backup of valuable saves.

## DevilutionX-Hellgate

The Hellgate build preserves the normal Diablo/Hellfire save modes and adds an
explicit marker used to display editor-created custom ordinary-item names. Stock
DevilutionX continues to reconstruct and localize ordinary item names normally.

Configure with `DEVILUTIONX_HELLGATE_BUILD=ON` to identify the modified game as
`DevilutionX-Hellgate` and produce `devilutionx-hellgate.exe`. Configure it `OFF`
for the stock identity and naming behavior.

See [Hellgate compatibility and source changes](docs/d1hellforge/DEVILUTIONX_HELLGATE.md).

## Building

The general engine requirements and platform instructions remain those of
[upstream DevilutionX](docs/building.md). D1Hellforge is a Windows CMake target:

```powershell
cmake --build <build-directory> --target D1Hellforge
```

The executable is produced under `tools/D1Hellforge` inside the selected build
directory. Runtime Diablo/Hellfire asset locations are configured in
`D1Hellforge.ini` beside the executable; personal paths are not committed.

Focused editor tests can be run with:

```powershell
ctest --test-dir <build-directory> -R "D1HellforgeItemTest\.|ItemFile\." --output-on-failure
```

## Documentation

- [D1Hellforge changelog](tools/D1Hellforge/CHANGELOG.txt)
- [Current implementation status](docs/d1hellforge/STATUS.md)
- [Architecture](docs/d1hellforge/ARCHITECTURE.md)
- [Save format notes](docs/d1hellforge/SAVE_FORMAT.md)
- [Item formats](docs/d1hellforge/ITEM_FORMATS.md)
- [Testing](docs/d1hellforge/TESTING.md)

## Upstream and license

DevilutionX-Hellgate is derived from
[diasurgical/DevilutionX](https://github.com/diasurgical/devilutionX). The
upstream project, its contributors, and its existing history are retained and
credited. This repository remains governed by [LICENSE.md](LICENSE.md).

Diablo and Blizzard Entertainment are trademarks or registered trademarks of
Blizzard Entertainment. This project is not affiliated with or endorsed by
Blizzard Entertainment.
