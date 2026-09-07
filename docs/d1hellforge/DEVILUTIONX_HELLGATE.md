# DevilutionX-Hellgate

DevilutionX-Hellgate is the identifying name for this repository's locally modified
DevilutionX executable. It is not a third content mode: Diablo and Hellfire remain
the save-format and gameplay modes.

## Compatibility targets

- **DevilutionX-Hellgate**: D1Hellforge direct single-player statistics and explicitly
  marked custom item display names are supported.
- **Stock DevilutionX**: D1Hellforge's verified Diablo/Hellfire save and item work is
  the primary compatibility target. Arbitrary ordinary-item display names are stored,
  but stock DevilutionX reconstructs the visible name. Ears use their native compact
  owner-name format.
- **GOG/retail Diablo and Hellfire**: experimental until separate original-game archive
  fixtures and end-to-end tests are complete. Never overwrite the only copy of a save.

## Source changes from the stock repository baseline

- Adds the standalone D1Hellforge save editor and its focused tests.
- Adds ContentProfile plumbing for detected Diablo/Hellfire packed, built-in, and loose
  content without hardcoding individual third-party mods.
- Adds item-file codecs and adapters for `.dxitem`, raw ItemPack, Diablo ITM01, and
  Hellfire HIF transfers.
- Adds verified character-stat and inventory editing with backups, temporary archive
  verification, compact `hero`, active `game`, and DevilutionX `heroitems` handling.
- Adds the Item Workshop, deterministic native generation, import/export, movement,
  and advanced direct single-player item editing.
- Adds a persisted `CF_CUSTOM_NAME` marker. Only the Hellgate build uses that marker
  to show stored names for ordinary items; stock builds retain standard reconstruction
  and localization behavior.
- Contains pre-existing local source/build adjustments visible in the working tree;
  `git diff` remains the authoritative detailed audit and no uncommitted work is discarded.

## Executable identity

With `DEVILUTIONX_HELLGATE_BUILD=ON`, the window/product name is
`DevilutionX-Hellgate` and the Windows executable is `devilutionx-hellgate.exe`.
Configuring the option `OFF` retains the stock `DevilutionX` label and
`devilutionx.exe` output name.
