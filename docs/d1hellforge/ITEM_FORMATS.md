# Item Format Notes

## DevilutionX ItemPack
Status: VERIFIED / TESTED in prior Codex work.

- Compact serialized representation is 20 bytes in the audited fork.
- Reuse `PackItem` / `UnPackItem` and reconstruction logic where practical.
- Do not serialize the full runtime `Item` memory image.

## DXITEM
Status: IMPLEMENTED / TESTED in prior work.

Purpose: versioned D1Hellforge/DevilutionX portable item container with metadata and integrity checking.

## Raw ItemPack
Status: TESTED in prior work.

## ITM01
Status: DETECTION IMPLEMENTED.
Full compatibility decode/export: TBD and fixture-backed only.

## Hellfire HIF
Status: FIXTURES AVAILABLE.
- Large collection exists in the user's Google Drive.
- Observed files are 400 bytes.
- Actual field mapping is not yet considered confirmed.

## Legacy collections
User has several Diablo item ZIP collections and older editor/reference material in Google Drive.

## Adapter rule
All legacy formats should convert through a compact portable model rather than directly mutating a full runtime `Item`.

Conceptual model:
- seed
- createInfo
- item/base index
- value
- buff/flags needed for reconstruction
- source-format metadata/unknown bytes when needed for lossless round-trip

## Research rule
Never infer layout from extension/name alone. Compare multiple fixtures and existing editor/source documentation before marking offsets confirmed.
