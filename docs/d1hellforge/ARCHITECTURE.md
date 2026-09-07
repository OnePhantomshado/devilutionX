# D1Hellforge Architecture

## Goal
A standalone Windows GUI editor for Diablo 1 / Hellfire that supports DevilutionX first, then original/classic saves, while also importing/exporting legacy item files.

## Recommended boundaries

### GUI
Responsible for:
- file picker/open/save UX
- character/stat controls
- equipment/inventory/belt views
- item import/export actions
- warnings/validation dialogs
- auto-backup toggle

GUI should not contain binary parsing logic.

### Save core
Suggested responsibilities:
- format detection
- archive/save open
- player extraction
- editable document/model
- validation
- safe write + backup + verification

Suggested files/modules under `tools/D1Hellforge/`:
- `save_document.*`
- `save_reader.*`
- `save_writer.*`
- `backup.*`
- `validation.*`

### Item compatibility layer
Suggested responsibilities:
- DevilutionX ItemPack adapter
- portable D1Hellforge item model
- `.dxitem`
- raw ItemPack
- legacy `.itm`
- Hellfire `.HIF`

Suggested files/modules:
- `item_model.*`
- `item_pack_adapter.*`
- `item_file.*`
- `legacy_itm.*`
- `hellfire_hif.*`

## Data flow

Save file -> format adapter -> editable document -> GUI -> validation -> writer -> temp save -> verify -> replace

Legacy item file -> format detector -> portable item model -> validation -> DevilutionX item reconstruction / save insertion

## Design principle
Do not use the full runtime `Item` as the persistent editor model. Runtime items carry rendering/gameplay state that legacy formats do not need. Keep a compact portable model and convert at the boundary using existing DevilutionX packing/recreation functions.
