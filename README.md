<p align="center">
  <img width="560" height="144" alt="DevilutionX" src="https://github.com/user-attachments/assets/7ac73801-ef7b-4cc1-8442-a191a2a0a1ce" />
</p>

---

# DevilutionX-Hellgate With D1Hellforge Save Editor

**DevilutionX-Hellgate** is a DevilutionX fork intended as a practical platform
for developing, testing, and validating Diablo and Hellfire mods. It includes
the **D1Hellforge Save Editor** under [`tools/D1Hellforge`](tools/D1Hellforge),
where it can reuse the same item, archive, and content logic as the game.

This project is useful for mod authors who need to:

- Create, inspect, edit, import, export, and reforge test items.
- Move items through equipment, inventory, belt, and shared-stash interfaces.
- Test Diablo, Hellfire, and developing mod-aware content profiles.
- Build repeatable test saves without manually finding every item in game.
- Verify save changes using backup, reopen, and validation checks.
- Test custom item display names in the matching Hellgate game build.

Hellgate is a build identity and mod-testing layer, not a third game mode.
Diablo and Hellfire remain the supported gameplay and save-format modes.

> **Development status:** D1Hellforge v0.3.0 is the first stable development
> release. Keep independent backups of valuable saves. Original GOG/retail
> Diablo and Hellfire save writing remains experimental.

## D1Hellforge Save Editor

D1Hellforge is built as part of this source tree and is located at
[`tools/D1Hellforge`](tools/D1Hellforge). Its current features include:

- Diablo and Hellfire single-player save discovery and character-stat editing.
- Graphical equipment, backpack, belt, and shared-stash interfaces.
- Shared item details, graphics, actions, movement, footprints, and collisions.
- Item Workshop creation, deterministic Reforge, and **I'm Feeling Lucky**
  advanced editing.
- Fast and Compatible deterministic Reforge engines.
- `.dxitem`, raw ItemPack, Diablo ITM01, and Hellfire HIF item adapters.
- Diablo-themed and Native UI presentation modes.

### Custom item names in game

D1Hellforge can store a custom display name and set the persisted
`CF_CUSTOM_NAME` item flag. The DevilutionX-Hellgate game build recognizes that
flag and displays the stored name. Items without the flag continue to use normal
Diablo/Hellfire item-name generation and localization. Stock DevilutionX is not
changed and does not use this Hellgate-only display behavior.

See the [D1Hellforge changelog](tools/D1Hellforge/CHANGELOG.txt) and
[Hellgate compatibility notes](docs/d1hellforge/DEVILUTIONX_HELLGATE.md).

## Building and testing

The general engine requirements and platform instructions remain those of
[upstream DevilutionX](docs/building.md). Enable the Hellgate build identity with
`DEVILUTIONX_HELLGATE_BUILD=ON` to produce `devilutionx-hellgate.exe`.

D1Hellforge is a Windows CMake target:

```powershell
cmake --build <build-directory> --target D1Hellforge
```

The editor executable is produced under `tools/D1Hellforge` in the selected
build directory. Configure runtime game-data locations using `D1Hellforge.ini`
beside the executable. Personal paths and game data are not committed.

Run the focused editor tests with:

```powershell
ctest --test-dir <build-directory> -R "D1HellforgeItemTest\.|ItemFile\." --output-on-failure
```

## What is DevilutionX?

[DevilutionX](https://github.com/diasurgical/DevilutionX) is a port of Diablo
and Hellfire that makes the games easier to run on modern operating systems
while providing engine improvements, bug fixes, and optional quality-of-life
features.

You must supply data from a legally owned copy of Diablo. No Blizzard game data
or MPQ files are included in this repository. See the upstream
[installation instructions](docs/installing.md) and
[manual](https://github.com/diasurgical/DevilutionX/wiki).

## Project documentation

- [Current D1Hellforge implementation status](docs/d1hellforge/STATUS.md)
- [D1Hellforge architecture](docs/d1hellforge/ARCHITECTURE.md)
- [Save-format notes](docs/d1hellforge/SAVE_FORMAT.md)
- [Supported item formats](docs/d1hellforge/ITEM_FORMATS.md)
- [Testing notes](docs/d1hellforge/TESTING.md)
- [GitHub Desktop publishing guide](GITHUB_DESKTOP_PUBLISHING.md)
- [Upstream DevilutionX changelog](docs/CHANGELOG.md)

## Contributing and mod testing

Bug reports should identify whether the problem occurs in D1Hellforge,
DevilutionX-Hellgate, or unmodified upstream DevilutionX. When reporting an
editor problem, include the content mode, operation performed, expected result,
and verification result. Do not attach copyrighted MPQs or private save files to
public reports.

Changes inherited from DevilutionX should continue to follow the upstream
[contribution guidance](docs/CONTRIBUTING.md).

## Credits

- The original [Devilution](https://github.com/diasurgical/devilution) project.
- [DevilutionX](https://github.com/diasurgical/DevilutionX) and all of its
  contributors.
- Everyone testing DevilutionX-Hellgate and D1Hellforge with real Diablo,
  Hellfire, and mod-development workflows.

## License and legal notice

This fork retains the upstream project history and is governed by
[`LICENSE.md`](LICENSE.md). The source code is made available for non-commercial
use under that license.

Diablo and Blizzard Entertainment are trademarks or registered trademarks of
Blizzard Entertainment. This project is not affiliated with or endorsed by
Blizzard Entertainment.
