# D1Hellforge Current Status

Last consolidated: 2026-09-07

## D1Hellforge v0.3.0 — released
- Promoted the user-tested restrained Diablo UI checkpoint from the Step 7 development series to D1Hellforge v0.3.0.
- The executable now carries independent D1Hellforge file/product version metadata (`0.3.0.0`) rather than relying on the parent DevilutionX project version.
- This release baseline includes the Native/Diablo UI switch, application icon, themed navigation and dialog controls, shared inventory/stash actions and display, and both Fast and Compatible Reforge engines.
- All 46 focused item/file tests pass at the release checkpoint.

## Restrained Diablo UI — first test chunk
- Integrated the user-selected D1Hellforge icon as a multi-resolution executable/window icon.
- The five main commands (`Open Save`, `Refresh`, `Character`, `Inventory`, and `Stash`) now use the supplied Hellforge button states in Diablo UI, with the earlier code-drawn treatment retained as an asset-failure fallback.
- Character, Workshop/Reforge, the legacy compact item dialog, and Advanced Editor now share one BUTTON-class-only themed adapter for the supplied Hellforge checkbox/radio images. Native button semantics, labels, keyboard focus, and Native UI fallback remain in control. Restricting attachment to actual BUTTON controls fixes the test-build regression that painted choice art over character fields and item-detail panels.
- Added a live `View > Native UI / Diablo UI` presentation switch, persisted in `D1Hellforge.ini` under `[Appearance]` with Native fallback enabled by default.
- Diablo UI keeps the existing restrained gold-on-dark command/navigation treatment and black/gold semantic information panels; Native UI renders the same owner-drawn command controls with Windows-style presentation.
- Authentic Diablo inventory and DevilutionX stash canvases, item sprites, movement overlays, save behavior, Workshop, and Reforge are unchanged.
- Large ornamental frames, custom context menus, custom message boxes, and full-window reskinning remain deliberately out of this test chunk.
- The focused D1Hellforge item/file suite passes all 46 tests, and the updated GUI target builds successfully.

## UI update roadmap — planned
- Added `UI_UPDATE_ROADMAP.md`, a nine-milestone implementation plan for the optional Hellforge presentation and retained Native presentation.
- Each milestone ends in a runnable, reviewable build; shared behavior and deterministic save/item results remain outside the theme boundary.
- The immediate proposed chunk establishes the theme/fallback configuration and asset manifest, then integrates the multi-resolution application icon without reskinning functional content yet.

## UI theme mapping — review draft complete
- Audited the twelve supplied Drive design sheets and mapped the current main window, Character, Inventory, Stash, Workshop, Reforge, Advanced Editor, and modal controls to specific reference families in `UI_THEME_MAPPING.md`.
- Defined `Native` and `Hellforge` as separate presentation modes with per-element Native fallback; theme selection must not alter save or item behavior.
- Identified required image preparation: source-sheet preservation, cropping, alpha verification, state extraction, content-safe insets, nine-slice/tile rules, DPI handling, and a runtime manifest.
- Recommended preparation order starts with the icon and shared content panel; the highly decorative outer frame comes last after real layout insets are proven.
- No artwork or UI behavior changed in this mapping pass.

## Roadmap Step 6 — Shared stash actions, first test chunk (complete)
- Added memory-only Copy, Delete, Workshop, and Reforge actions to the stash context menu.
- Right-click now selects the stash item under the pointer before enabling item actions.
- Copy retains the exact full Diablo/Hellfire item record, assigns a canonical reference, and finds the first collision-free footprint on the selected page.
- Delete removes the selected record and renumbers all later grid references so stash serialization remains canonical.
- Workshop Edit exposes the existing `I'm Feeling Lucky` Advanced Item Editor and stages the returned full record without writing the stash immediately.
- Stash `Workshop...` and `Reforge...` now enter the same `OpenItemWorkshop` controller, Workshop model, asynchronous Reforge engine, and semantic item-display path used by inventory items. Only the result-storage adapter differs; stash results preserve their placement coordinates and stage the profile-sized full record.
- The stash context-menu names now match their inventory counterparts. `I'm Feeling Lucky` remains inside the shared Workshop Edit window, exactly as it is for inventory.
- Inventory and stash selection panels now share one item-detail formatter. A completed stash move keeps the item selected, restores the normal gold outline, retains its complete semantic details, and appends the unsaved-preview notice instead of replacing the details with a status-only message.
- `Make Item from Catalog...` is now available in stash and enters the same Workshop Create path as inventory. The stash adapter materializes the profile-sized full item record and places it in the first collision-free footprint on the selected page.
- All three actions remain previews until `Save Stash Changes (with Backup)` runs the existing temporary-write, reopen, replace, and final-verification workflow.
- Focused copy/delete coverage runs in both Diablo and Hellfire. All 46 focused item/file tests pass.

## Roadmap Step 5 — Shared Item Display foundation (complete)
- Added `item_display.hpp/.cpp` with location-neutral Compact, GameTooltip, Detailed, and DetailedWithMetadata modes plus structured semantic sections and tones.
- Inventory/equipment/belt summaries, stash selected-item details, Workshop/Create/Edit previews, retained Reforge results, and the Advanced Item Editor now flow through the shared display interpretation (existing `detailLines` remains as a compatibility transport for current Win32 controls).
- The shared layer owns names, quality/identified state, damage/armor, durability, charges/spells, requirements, combat/elemental/attribute/life/mana effects, affix power text, value, common Diablo/Hellfire special flags, and optional generation/content metadata.
- `Damage From Enemies` is classified once: negative stored values are beneficial; positive stored values are detrimental and receive Negative tone plus the skull-marked plain-text fallback.
- Display construction uses an already-resolved `devilution::Item`; it does not generate candidates or add work to rejected Fast Reforge seeds.
- Focused semantic classification coverage was added. All 45 focused item/file tests pass.
- The Win32 display adapter now renders Unicode item details on a black background with gold text, orange warnings, and red `☠` negative lines. Inventory/stash details are scrollable and Workshop, Reforge, and Feeling Lucky use the same presentation control.
- Item summaries now request Detailed depth rather than the former compact host subset.
- `D1Hellforge.ini` now exposes safe-clamped startup dimensions under `[Windows]` for the main window, one shared Item Workshop/Create/Edit/Reforge window, and Advanced Editor, allowing quick layout experiments without recompilation. Older separate Workshop/Reforge keys are migration fallbacks.
- Workshop now uses a taller item catalog and scrolling detail panel with generation controls moved into the free column to the right of the item sprite. Rich-text coloring uses actual control line indexes, preventing a negative line's red style from spilling into the following positive line.

## Roadmap Step 1 — exact-seed Reforge boundary (complete)
- Fast Reforge now uses `GenerateExactSeedNative`: one requested seed performs exactly one native DevilutionX generation, and the returned item seed is verified before scoring.
- Quality, affix-name, and effect-focus locks are evaluated after that exact generation; Fast no longer inherits the catalog generator's 4,096/65,536 search-until-match loop.
- Compatible Reforge remains unchanged as the deterministic/reference path.
- Initial roadmap budgets are active: Novice 512, Competent 4,096, Master 32,768.
- Focused Diablo/Hellfire exact-seed coverage was added. All 40 focused D1Hellforge item/file tests pass.
- The GUI and test executable compile, and all 40 focused D1Hellforge item/file tests pass.

## Roadmap Step 2 — asynchronous Reforge (complete)
- Reforge search now runs on a single background worker; DevilutionX generation is intentionally not parallelized.
- The Workshop remains responsive and reports seeds examined, real native generation calls, elapsed rate, and cancellation state.
- Search controls are disabled while work is active; the Cancel button requests a clean stop and does not apply partial candidates.
- Completed results are transferred back to the Win32 UI thread before list/detail controls are updated.
- Closing the Workshop during a search requests cancellation before releasing dialog state.
- Focused cancellation coverage verifies that the requested/original item bytes remain unchanged. All 41 focused tests pass.

## Roadmap Step 3 — generation-free item catalog (complete)
- Catalog membership, names, and categories are now built directly from the initialized effective DevilutionX item table; ordinary catalog browsing no longer calls item generation.
- Results are cached by the base mode, packed/built-in content identifiers, loose-content state, and initialization identity captured by `ContentProfile`.
- Switching between Diablo and Hellfire invalidates the single effective-content cache rather than retaining stale table entries.
- Existing visible catalog behavior is preserved: runtime drop restrictions were deliberately not added as a new Workshop filter.
- Focused coverage checks deterministic cached results against the effective DevilutionX table in both Diablo and Hellfire. All 42 focused item/file tests pass.

## Roadmap Step 4 — Workshop generation-choice caching (complete)
- Prefix, suffix, and compatible unique choices are cached by the active `ContentProfile`, base item, generation level, and `onlyGood`/curse-filter state.
- A content-profile switch clears the cache before reading the newly effective DevilutionX tables; no permanent duplicate affix database was introduced.
- The existing 1–63 level merge used to recognize loaded-item affixes now benefits from the same cache on subsequent Workshop openings.
- The reported amulet drag regression was fixed alongside this chunk: slot compatibility now uses DevilutionX's native `iLoc` identity rather than the human-readable `equipType` label, and the selection, hit target, and drag-preview outline align with the rendered neck slot (including its visible border).
- Focused tests cover cache stability/profile invalidation and native amulet-to-neck-slot compatibility. All 44 focused item/file tests pass.

## Architecture modernization - PHASES 1-2 DONE / PHASE 3 STARTED / DIRECT ITEM EDITOR TEST BUILD
- Shared Workshop supports Create, Edit, Reforge, and Attempt Fix through one working-copy model.
- Workshop affix recognition now follows DevilutionX compact reconstruction while retaining conflicting legacy expanded names for warning/display.
- Phase 1 moved catalog metadata and construction into `item_catalog.cpp/.hpp`, and generation choices, deterministic creation, and seed regeneration into `item_generator.cpp/.hpp`.
- `save_document` remains responsible for save discovery/decoding/writing, active-game overlays, backups, and legacy expanded-record export.
- Phase 2 added the thin `devx_adapter.cpp/.hpp` boundary. Diablo/Hellfire content initialization, effective item/prefix/suffix/unique table access, safe base lookup, `InitializeItem`, `SetupAllItems`, `PackItem`, and `UnPackItem` now pass through it.
- No direct access to those DevilutionX item integration points remains elsewhere in D1Hellforge.
- Phase 3 now has an editor-owned `ContentProfile` exposed by `devx_adapter`. It records the Diablo/Hellfire base mode separately from active packed/built-in content identifiers, loose-content presence, a truthful display label, and initialization state/error storage without adding named-mod cases or activating third-party mods.
- Item catalog and generation services accept the profile while retaining game-based compatibility overloads, and Workshop models now carry the effective profile incrementally alongside the save-format mode.
- Loaded character sessions now retain the effective profile separately from their save-format mode. The character/inventory information panels display its truthful label, and the GUI passes that captured profile into catalog and Workshop operations.
- Catalog generation and Workshop validation reject uninitialized, stale, or base-mode-mismatched profiles instead of falling through to another content environment.
- Focused profile tests cover vanilla Diablo, both supported Hellfire packaging forms (identified packed/built-in content or active loose content), Diablo/Hellfire switching, Workshop propagation, and invalid-profile rejection.
- Current build succeeds and all 27 focused `D1HellforgeItemTest.*` and `ItemFile.*` tests pass (the original 22 plus five content-profile tests).
- D1Hellforge now reads and preserves the authoritative single-player `heroitems` full-item stream and includes it in backup -> temporary write -> reopen/verify -> replace -> final verification inventory saves.
- The first testable `I'm Feeling Lucky...` modal is available from Workshop Edit. It stages direct full-record edits for physical damage/AC, core bonuses, attributes, resistances, human-readable HP/mana, secondary bonuses, elemental damage, and strength requirement; Apply returns to the Workshop preview and disk writing remains explicit.
- Legacy ITM01/HIF imports retain their complete 368/372-byte item records instead of collapsing custom fields to `ItemPack`. `.dxitem` version 2 can carry the full record with checksum protection while version 1 and raw ItemPack remain readable; expanded exports preserve the record.
- Direct-item tests cover full-record stat edits, fixed-point HP conversion, unknown flag preservation, invalid min/max rejection, and reset/cancel semantics. The focused total is now 30 tests.
- The Advanced Item Editor now begins its player-facing field-layout pass: gameplay fields are ordered ahead of requirements/value, labels use modern tooltip terminology, Indestructible uses DevilutionX's actual durability marker, and a live informational preview shows staged names, quality, damage/armor, durability, charges, bonuses, elemental ranges, attributes, life/mana, spell levels, damage taken, and requirements. Numeric elemental values remain independent from effect flags. The focused total is now 33 tests.
- Reforge now offers INI-persisted Fast and Compatible engines. Compatible preserves the prior full-summary/string-scoring path; Fast scores reconstructed DevilutionX items numerically, retains lightweight Top-3 winners, and materializes summaries only for those winners. The widened Reforge UI shows all three results, full selected-candidate details, and measured attempts/rate. Focused Diablo and Hellfire tests exercise both engines; the total is now 34 tests.
- Verified recovery snapshot: `work/snapshots/D1Hellforge-Phase1-2026-09-03.zip`, SHA-256 `9ED23B630F2000912630522FE3FAB63573F81600B3D2000721D6B6F6C82FC489`.
- NEXT: Finish the remaining Phase 3 base-mode-only service boundaries where a session profile is already available, then prepare the Phase 4 design for explicitly loading and verifying effective mod content. Preserve Diablo/Hellfire save behavior and do not add named-mod cases.

## Environment - DONE
- Git for Windows installed and working.
- Visual Studio C++ toolchain installed.
- Standalone CMake 4.4.3 installed.
- Ninja available.
- vcpkg dependencies configured.
- Full DevilutionX build succeeded.
- GOG Diablo + Hellfire data configured locally for testing.
- DevilutionX development desktop shortcut created and user-tested.

Do not redo environment setup unless a build actually fails.

## DevilutionX fork - WORKING
- Fork is cloned locally and builds.
- A fork-specific monster ally declaration/order issue was fixed so the current engine compiles.
- Obsolete `_mFlags` usage in custom helper code was updated to current `flags` usage.
- Full development game executable launched successfully with the user's GOG data.

## D1Hellforge - WORKING / PARTIAL
Confirmed from previous Codex work:
- Native Windows GUI executable exists/builds.
- Save discovery/open flow was implemented.
- Real DevilutionX test save decoding succeeded.
- Basic character information is readable/displayable, including identity and core stats.
- Test character "Darklord" was created for Hellfire/DevilutionX testing.
- Basic STR, MAG, DEX, VIT, and unspent-stat-point editing is implemented in the GUI.
- Each base stat displays its fixed class-specific safe maximum. A checked-by-default save option automatically caps oversized base-stat entries; unspent points remain explicitly limited to 0-255.
- Stat writes update both the packed hero and active-game records when present.
- Safe writes now use a dedicated `Backups` directory, collision-safe timestamped names, a temporary archive, reopen verification, and final verification before reporting success.
- The active-game stat locator uses the source-confirmed name/class record boundary and rejects ambiguous matches before editing; it does not require the packed hero and active-game stats to already match.
- Focused fixture checks pass for save read, no-change round trip, and an actual Magic-stat edit.
- User validation succeeded with the Hellfire Sorcerer "Darklord": edited base stats and unspent points were reopened in D1Hellforge and loaded correctly in DevilutionX.
- The stat-control layout was widened so `safe max` labels remain on one line at the normal window size.
- An initial read-only equipment/inventory/belt view is implemented from the packed hero record. It reports verified slot/index, seed, durability, charges, identification, and quality data without mutating items or initializing the full game runtime.
- Packed Diablo/Spawn item IDs are now remapped with DevilutionX functions and resolved to human-readable base names from the shipped item catalog, with numeric-ID fallback.
- Owned Diablo/Hellfire MPQs were confirmed in the local GOG installations, so the graphical inventory can load backgrounds and item sprites at runtime without bundling copyrighted assets.
- Automatic MPQ discovery now searches save/executable ancestors and common GOG locations, validates inventory background and item-sprite contents, requires Hellfire data for Hellfire saves, and reports the selected directory or text-fallback state in the UI.
- A read-only MPQ-backed graphical Inventory view is implemented with Character/Inventory navigation. It decodes the class inventory CEL, item cursor CEL, and town palette with DevilutionX code, then renders equipment, the 10x4 inventory grid, and eight belt slots.
- Visual QA against Darklord confirms the Sorcerer background, Short Staff of Mana, gold stack, and two belt potions render in the saved positions. The text/details view remains the fallback when graphics cannot be loaded.
- Loaded saves now start on the Character view. Open Save, Refresh, Character, and Inventory are grouped in a Diablo-inspired owner-drawn bottom toolbar with a highlighted active view.
- The graphical inventory is centered and scales responsively with nearest-neighbor sampling as the editor window changes size, preserving slot placement and the pixel-art appearance.
- Maximize, restore, and drag resizing now force a complete inventory repaint and prevent stale or overlapping scaled bitmap fragments.
- Graphical items now have read-only occupied-slot hit regions covering equipment, every inventory footprint cell, and belt slots. Left-clicking an occupied slot selects it with a gold outline and persistently shows location, base name, durability, charges, and value.
- The inventory bitmap is hosted by a custom-drawn button canvas rather than a transparent STATIC control, so real mouse-button releases reliably reach slot selection and provide local click coordinates for future drag capture.
- In Inventory view the save list contracts after loading, and the recovered left-side space becomes a persistent dark/gold character, save, and selected-item information panel.
- Backpack items can be dragged to preview positions. The canvas shows green valid footprints and red invalid footprints, rejects grid-boundary and occupied-cell collisions, and uses DevilutionX's source-confirmed positive bottom-left/negative remainder `InvGrid` encoding. Refresh/reload discards previews; Save Inventory Changes commits them through the verified writer.
- Memory-only previews now support belt reordering and belt-to-backpack transfers. Backpack-to-belt accepts only 1x1 items whose source item data marks them belt-compatible; tests confirm potions move and gold is rejected.
- Preview movement is unified across backpack, belt, and empty compatible equipment slots. Item metadata includes equip type and base requirements; preview validation covers helm, armor, ring, amulet, one-handed, and two-handed destinations plus hand occupancy. Round trips are verified for potion belt -> backpack -> belt and staff backpack -> hand -> backpack.
- The GUI now has Import Item and Export Item actions. A selected item can be exported as a checksummed, game-tagged `.dxitem` or raw 20-byte `.itm`; supported files can be imported, validated, assigned an unused item record, and auto-placed into the first fitting backpack footprint as a memory-only preview.
- Modern/raw file transfer and legacy detection are separated in `item_transfer.*` and `legacy_item_import.*`. ITM01 and 400-byte HIF files are identified with explicit fixture-decoder status; their layouts are not guessed. Export/import round-trip and all six focused item-file codec tests pass.
- Inventory item actions now use a right-click context menu instead of crowding the bottom navigation. Import is available from empty or occupied space; Export and Edit are tied to the selected occupied item; Make Item from Catalog is exposed as the next staged workflow. The bottom bar is again limited to Open Save, Refresh, Character, and Inventory.
- The inventory context menu now includes Copy Item. It duplicates the exact selected packed item into the first fitting backpack location as a memory-only preview, preserving the original item and save.
- Fixture-backed legacy import is enabled for the original 400-byte Diablo ITM01 and Hellfire HIF formats. The isolated adapter converts the source-confirmed 32-bit `ItemStruct` fields into `ItemPack`, tags the originating game, and keeps normal DevilutionX validation in the import path. All 124 supplied ITM01 fixtures and all 376 genuine HIF fixtures pass decoding and game-specific item validation; one mislabeled `.HIF` containing RIFF/WAV audio is safely rejected.
- The Import Item dialog automatically starts in `build-local/Im-Ex Items` when that folder exists.
- The inventory context menu now supports named, Yes/No-confirmed Delete Item for backpack, belt, and equipment previews.
- Edit Item now opens a focused modal editor for identified state, current/maximum durability, current/maximum charges, and gold value. Inputs are range-checked and current values cannot exceed their maximums; changes remain previews until explicitly saved.
- Save Inventory Changes persists moves, imports, copies, edits, and deletions. It updates both the compact `hero` `PlayerPack` and the full active-game player inventory, locating the latter only through a unique count/grid/item-seed match. Full item records are regenerated through DevilutionX `UnPackItem` rules. The workflow creates a timestamped backup, writes a temporary archive, reopens and byte-verifies both records, replaces the original, and verifies the final compact record.
- Focused copied-save checks pass for no-change inventory round trip, deletion plus reopen, legacy HIF import plus full-record reconstruction, and durability edit plus reopen. Each write check created its expected backup; the item codec suite remains 6/6 passing.
- A human-readable `D1Hellforge.ini` beside the executable remembers independent DevilutionX-save, original/GOG-save, and item-library directories across application restarts. DevilutionX's AppData save directory is the default, and item browsing cannot change the Open Save location.
- Expanded detail decoding guards charged items with no valid spell identifier, fixing the debug `vector subscript out of range` assertion seen with the Short Staff of Mana.
- Selected-item details now use DevilutionX reconstruction to show the in-game name, quality/identification, damage or armor, durability, charges and spell, requirements, value, and decoded prefix/suffix powers in the black information panel.
- Make Item from Catalog is now a category-filtered dialog backed by DevilutionX item definitions. It creates a normal game-native instance through `InitializeItem`/`PackItem`, validates the packed result, and places it as an unsaved backpack preview.
- A named, confirmed Diablo-to-Hellfire import conversion remaps the packed base-item identifier with DevilutionX's mapping and validates the destination item before preview placement.
- Export now supports regenerated 400-byte legacy Diablo ITM01 and Hellfire HIF files in addition to `.dxitem` and raw ItemPack. Cross-game legacy export uses the same checked identifier conversion.
- Automated D1Hellforge item tests now cover deterministic catalog generation, seed regeneration, semantic legacy Diablo ITM01 and Hellfire HIF export/re-import, and Diablo-to-Hellfire conversion. Together with the six dependency-light codecs, 11 focused item tests pass.
- The catalog dialog has begun its Item Workshop expansion: category filtering now works alongside case-insensitive name search, the selected item has an owned-MPQ sprite and decoded preview, and decimal seed controls support direct entry, previous/next exploration, and randomization.
- Catalog generation now sets DevilutionX's random seed before base-item initialization, so the same base item and seed reproduce identical packed output. Existing-item editing also exposes the seed and a Randomize action; changing it regenerates game-derived properties through the packed DevilutionX recreation path before remaining safe edits are applied.
- A shared item-generation options model separates quality, seed, level, and curse-filter intent from the GUI.
- A legacy Diablo-to-Hellfire premium-item regression was reproduced from the supplied Mithril Sword fixture. Town-generated compact items can ignore their stored base index and regenerate from seed under different game tables; D1Hellforge had also loaded base affix tables instead of the Hellfire overlay, producing the impossible `Knight's Staff of wizardry` description on a sword sprite.
- Item reconstruction now loads the correct Diablo or Hellfire data/mod tables, reports the actual regenerated base item and matching sprite, and reloads tables safely when switching games. Legacy conversion prompts show the legacy stored name plus the destination reconstruction and warn when compact generation data cannot preserve the original name/effects.
- The Item Workshop now exposes Normal, Magic, and Unique generation, generation level, and an `Exclude cursed affixes` option. Magic and unique items are generated through DevilutionX `SetupAllItems` rules with deterministic seed searching; bases without a game-defined unique fail clearly instead of creating handcrafted flags.
- Inventory saving now rejects duplicate equipment, backpack, or belt slot records before writing either save representation.
- Save-derived asset selection now validates Hellfire's `objcurs2.cel` in `hellfire.mpq`, loads its separate width table, and selects item frames across the primary Diablo and secondary Hellfire cursor sheets exactly like DevilutionX. This fixes Hellfire runes/oils previewing as unrelated Diablo items. The character panel identifies the active Diablo or Hellfire ruleset and validated MPQ sprite sources.
- Magic/Unique workshop generation now includes an optional rule-backed effect focus: damage, chance to hit, attributes, resistances, life/mana, or attack speed. D1Hellforge searches deterministic game-native results rather than directly writing effect flags.
- Final item identity now overlays the uniquely matched active-game full record onto the compact hero summary. This makes D1Hellforge display the same final name, base item, cursor graphic, footprint, and equipment type that the running DevilutionX game displays.
- Items whose active-game identity differs from their compact hero reconstruction are explicitly flagged in the item panel. Inventory saving is blocked for such split-identity saves until a canonical game-native replacement is chosen, preventing unrelated edits from silently transforming the item.
- A warned split-identity item now offers `Attempt Fix...` in its right-click menu. The action generates a canonical active-game replacement from the selected base, seed, quality, and character level, shows the current and proposed identities for confirmation, and changes only the in-memory preview; the normal verified save flow remains required. Because incompatible legacy compact data cannot always preserve its original affixes, the confirmation explicitly warns that the name and effects may change.
- Item Workshop steps 5-8 now add game/level/base-filtered prefix, suffix, and named unique selectors. Named affixes are found through deterministic game-native seed searching, while named uniques use DevilutionX's compatible unique selection rules; generated magic and unique items are identified so the requested final name is visible in the preview.
- Existing inventory items can now be right-clicked and opened with `Reforge in Item Workshop...`. The current base item is preselected, the full generation controls are available, and the generated replacement preserves the selected inventory slot as an in-memory preview until the verified inventory save is chosen.

## Item support - WORKING / PARTIAL
- The locally modified game executable is now explicitly labeled `DevilutionX-Hellgate`;
  Hellgate is a build-identification tag, not a new save or content mode. The output
  executable is `build-local/devilutionx-hellgate.exe`.
- The Hellgate-only ordinary-item custom-name display behavior is build-gated. A stock
  build keeps DevilutionX's reconstructed/localized names, while both builds retain the
  existing Diablo/Hellfire save-format distinction.
- GOG/retail save support remains experimental. The discovered writable-style GOG install
  is `C:/GOG Games/Diablo` with Hellfire under `hellfire`; no original-game save fixture was
  present there during discovery, and direct original-game archive support is not yet verified.
- Dependency-light item file codec work exists.
- Raw 20-byte DevilutionX `ItemPack` round-trip tested.
- Versioned `.dxitem` container implemented/tested in prior work.
- Diablo/Hellfire metadata preservation tested.
- Corruption/unknown malformed input rejection tested.
- Original 400-byte ITM01 and HIF fixture decoding is implemented in the isolated D1Hellforge adapter and validated against the supplied collections.
- User has legacy Diablo item ZIPs and a large Hellfire `.HIF` fixture collection in Google Drive.
- Hellfire `.HIF` fixtures observed at 400 bytes each.

## Current milestone
Initial DevilutionX shared-stash support is available for user validation.

The main toolbar now includes STASH. After a character save is loaded,
D1Hellforge discovers the matching `stash` archive, uses the character's active
ContentProfile, and exposes shared gold plus Previous/Next browsing across the
100 stash pages. This first slice is read-only: it establishes strict parsing
and identity validation before any operation is allowed to update both the
character and shared-stash archives.

The executable is `build-local/tools/D1Hellforge/D1Hellforge.exe`. The focused
D1Hellforge item/file suite passes 36/36 tests, including Diablo and Hellfire
stash payload coverage.

Ask the user to test these first:
1. Launch and load the Short Staff of Mana save; confirm there is no Visual C++ `vector subscript out of range` assertion.
2. Browse to a DevilutionX save and separately import/export an item, restart the editor, and confirm each dialog remembers its own location.
3. Select weapon, armor, staff, gold, and magical items and check the expanded black-panel details.
4. Open right-click `Make Item from Catalog...`, create a normal item, save inventory changes, and load it in DevilutionX.
5. Import a legacy Diablo `.ITM` into a Hellfire character, accept the named conversion, save, and verify it in DevilutionX.
6. Export/re-import representative `.ITM` and `.HIF` files. Do this on copies and preserve the fixture originals.

Known limitation: the INI has an `OriginalGameSavePath` key for separation and future use, but original Blizzard/GOG save decoding is not implemented yet. Current save editing targets DevilutionX archives.

Required write flow:
1. Make timestamped backup.
2. Write edits to a temporary save/archive.
3. Reopen temporary result.
4. Verify edited fields.
5. Replace original only after verification succeeds.

User-validated character fields include base STR, MAG, DEX, VIT, and unspent stat points. Inventory movement/import/copy/edit/delete and verified saving have also been user-tested. The newest catalog, richer-detail, INI, conversion, and legacy-export changes still need the focused user checks above.

## Next milestones
1. PLANNED ONLY: add original/GOG/retail backward compatibility as an isolated
   `tools/D1Hellforge/legacy_save/` translation layer. Begin with archive detection,
   capability reporting, and read-only fixture adapters; do not enable writes until
   independent Diablo and Hellfire round-trip verification is complete.
2. Fix any regressions found by the six focused user checks in `Current milestone` before adding more features.
3. DONE: Add automated semantic round trips for regenerated legacy ITM01/HIF export and Diablo-to-Hellfire conversion.
4. DONE: Catalog search, save-derived Diablo/Hellfire MPQ sprite selection, shared generation options, deterministic seed controls, rule-backed Magic/Unique generation, effect-focus filtering, explicit compatible prefix/suffix and named unique selectors, and existing-item Workshop reforging are implemented.
5. Validate equipment movement with more fixtures, including dual-wield classes, two-handed conflicts, stat requirements, full belts/backpacks, and collision edges.
6. Refine the INI/settings UI so users can choose DevilutionX save, future original-game save, item-library, and MPQ asset folders without editing the file manually.
7. Continue Diablo-style UI polish and improve missing-MPQ/fallback messaging.
8. Collect representative untouched classic Diablo and Hellfire fixtures before beginning the planned translation layer.

## Do not redo
- Git/Visual Studio/CMake/Ninja installation.
- Initial GOG data discovery.
- Initial DevilutionX build setup.
- Basic ItemPack size/round-trip investigation.
- Broad legacy item web research unless a specific unresolved format requires it.
