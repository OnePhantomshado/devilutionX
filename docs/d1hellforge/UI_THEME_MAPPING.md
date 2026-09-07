# D1Hellforge UI Element-to-Reference Mapping

Status: review draft. This document maps the current working Win32 UI to the supplied design sheets. It does not authorize image preparation or UI replacement yet.

## Theme boundary

D1Hellforge will retain two complete presentation modes:

- `Native`: the current working Win32 presentation and fallback.
- `Hellforge`: custom imagery and owner-drawn presentation.

Theme selection changes presentation only. Save parsing, item identity, Workshop, Reforge, movement, validation, and verified writes remain shared. Missing or invalid Hellforge assets fall back to Native controls without disabling editor behavior.

Proposed configuration:

```ini
[Appearance]
Theme=Hellforge
AllowNativeFallback=true
```

## Reference inventory

| Reference file | Contents | Primary role |
|---|---|---|
| `d1hellforge_ui_main_window_frame.png` | One ornate full-window frame | Optional outer shell for the main window |
| `d1hellforge_ui_content_panel_frame.png` | One large framed stone panel | Shared content/detail/modal panel |
| `d1hellforge_ui_section_header_frames_set.png` | One full-width and two half-width headers | Window and section headings |
| `d1hellforge_ui_horizontal_panels_set.png` | Three long panel states | Navigation/action bars and long buttons |
| `d1hellforge_ui_controls_states_set.png` | Checkbox off/on, radio off/on, toggle off/on | Interactive control states |
| `d1hellforge_ui_square_frames_set.png` | Normal, active, chained, and disabled frames | Item previews and blocked/disabled panels |
| `d1hellforge_ui_square_button_frames_set.png` | Four square button states | Icon commands and compact navigation |
| `d1hellforge_ui_ornamental_dividers_set.png` | Long divider, two corners, small plaque | Section separation and compact status plaque |
| `d1hellforge_app_icon_32_simplified.png` | Simplified hammer/skull/D1 mark | Best source for 16/24/32-pixel icon masters |
| `d1hellforge_app_icon_64_compact.png` | Compact detailed icon | Best source for 48/64-pixel icon masters |
| `d1hellforge_app_icon_128_wordmark.png` | Detailed icon with Hellforge wordmark | 128-pixel application/about artwork |
| `d1hellforge_app_icon_256_full.png` | Full Save Editor illustration | 256-pixel icon, splash, or About artwork |

## Shared state vocabulary

| UI state | Visual rule | Reference source |
|---|---|---|
| Normal | Dark stone, low red edge light | First/non-glowing variants |
| Hover | Moderate red/orange edge light | Second variants or derived intermediate |
| Pressed | Strong inner glow with content shifted 1–2 px | Third/glowing variants, adjusted during preparation |
| Selected/active | Strong red/orange glow; text remains readable | Active horizontal/square variants |
| Disabled | Desaturated gray, reduced contrast, no glow | Chained/gray square frame; derived gray controls |
| Keyboard focus | Thin gold inner keyline | Derived overlay; not supplied directly |
| Warning | Orange text/accent, normal frame | Existing semantic Warning tone |
| Error/negative | Red text and skull marker, normal frame | Existing semantic Negative tone |

## Main window mapping

| Current element | Current implementation | Hellforge reference | Preparation/use |
|---|---|---|---|
| Application icon/title icon | Default executable/window icon | Simplified 32 + Compact 64 + Wordmark 128 + Full 256 | Build one multi-resolution `.ico`; simplify rather than merely shrink at small sizes |
| Main client background | Native button-face background | Main window frame | Extract into nine-slice outer border plus separate dark center fill; decoration must not consume the workspace |
| Save list frame | `LISTBOX` with client edge | Content panel frame or half-width section frame | Use compact nine-slice panel; keep native list scrolling and selection behavior |
| Left details/status panel | Shared rich edit on black | Content panel frame | Frame the existing semantic display host; do not bake text into the image |
| Main workspace | Character form or game inventory/stash canvas | Main frame center | Use neutral dark fill around content; preserve owned-MPQ inventory/stash art unchanged |
| Bottom command/navigation row | Five owner-drawn Diablo-style buttons | Horizontal panel set | Prepare scalable normal/hover/active button backgrounds; preserve text labels and active-view state |
| Character/Inventory/Stash active state | Darker native fill + gold outline | Active horizontal-panel variant | Active only; do not use full glow on every button |

## Character view mapping

| Current element | Reference match | Notes |
|---|---|---|
| Character heading and identity | Full-width section header | Text-safe center must stretch without distorting skull/end ornaments |
| Attribute group | Content panel frame | One shared resizable panel behind labels and edits |
| Numeric edit fields | No exact supplied frame | Derive a restrained dark inset field from content-panel stone; retain caret, selection, and accessibility |
| Read-only labels/values | Content panel stone texture | Use gold/ivory typography; do not imitate editable-field borders |
| Auto-cap checkbox | Checkbox off/on sheet | Extract both supplied states plus hover, pressed, disabled, and focus overlays |
| Save Stats button | Horizontal panel states | Active glow reserved for default/pressed state |
| Save/content warnings | Small plaque or content panel | Preserve orange/red semantic text |

## Inventory and stash mapping

| Current element | Reference match | Notes |
|---|---|---|
| Diablo inventory canvas | No replacement | Continue rendering authentic MPQ assets and item sprites |
| DevilutionX stash canvas | No replacement | Continue rendering `stash.clx` and profile-correct item sprites |
| Item selection | Derived gold overlay | Reference sheets do not supply the exact grid outline; retain current geometry |
| Valid/invalid drag preview | Derived green/red overlay | Functional overlay remains independent of theme artwork |
| Selected-item details | Content panel frame | Same semantic display host for inventory and stash |
| Stash page heading | Half-width section header | Use text-safe center with page count |
| Previous/Next page buttons | Square-button frames or compact horizontal panels | Create normal, hover, pressed, and disabled states; arrows remain code/text glyphs unless dedicated glyphs are prepared |
| Context menus | Native initially | Keep native menus in the first theme release; custom menus require separate keyboard/accessibility work |
| Dirty/preview notice | Small plaque or detail-panel footer | Append below item details; never replace them |

## Item Workshop and Reforge mapping

| Current element | Reference match | Notes |
|---|---|---|
| Window/section title | Full-width section header | Same header for Create, Edit, Reforge, and Attempt Fix with dynamic text |
| Category/search/catalog area | Content panel frame | Tall resizable frame; native list and scrollbar remain usable |
| Item sprite preview | Normal square frame | Transparent item sprite centered inside; selected/generated state may use active frame |
| Seed controls | Half-width header/panel + derived fields | Previous/Next/Randomize use compact shared button style |
| Quality radio controls | Radio off/on sheet | Shared radio renderer across Workshop and Advanced Editor |
| Identified/rule/lock checkboxes | Checkbox sheet | Shared checkbox renderer |
| Prefix/suffix/unique dropdowns | No exact supplied control | Derived dark inset field plus native dropdown behavior and arrow |
| Item details preview | Content panel frame | Existing black/gold semantic display remains source of truth |
| Reforge group frame | Content panel or ornamental divider | Avoid the native `GROUPBOX`; use a section header/divider plus dark content area |
| Top-3 results | Content panel frame | Preserve all three rows, scrolling, and selected-candidate state |
| Search/Search Again/Apply Selected | Horizontal panel states | Shared command-button renderer |
| Search progress/status | Small plaque | Must clearly show running, cancelling, complete, and error states |

## Advanced Item Editor mapping

| Current element | Reference match | Notes |
|---|---|---|
| Window header | Full-width section header | Dynamic `I'm Feeling Lucky — Advanced Item Editor` title |
| Field groups | Content panel plus ornamental dividers | Group combat, elemental, attributes, resources, requirements/value |
| Text/numeric inputs | Derived dark inset field | Preserve native edit semantics, validation, and keyboard navigation |
| Quality selector | Radio/control sheet or themed dropdown | Must match Workshop behavior |
| Identified/Indestructible | Checkbox sheet | Same shared renderer as Workshop/Character |
| Live semantic preview | Content panel frame | Keep large, black/gold, scrollable, and independent from field decoration |
| Reset/Cancel/Apply | Horizontal panel states | Same shared command renderer as Workshop |

## Modal and auxiliary mapping

| Element | Reference match | Notes |
|---|---|---|
| Basic item edit modal | Compact content panel | Eventually retire or visually align; Workshop remains preferred shared editor |
| Confirmation/error dialogs | Native initially | Preserve trusted Windows message boxes until a fully keyboard-accessible custom modal exists |
| Disabled/unavailable operation | Chained square frame only for large panels | Ordinary disabled controls should use derived desaturation, not chains everywhere |
| About/splash artwork | Full 256 icon | Optional; should not delay functional theming |

## Image-preparation specifications

Every extracted runtime asset needs a manifest entry with:

- source Drive file and source pixel rectangle;
- output filename and logical role;
- transparent or opaque background expectation;
- content-safe insets;
- fixed, tiled, proportional, or nine-slice scaling rule;
- DPI variants or maximum intended render size;
- state name and fallback state;
- checksum.

Preparation rules:

1. Preserve original source sheets unchanged.
2. Crop transparent margins before measuring slice insets.
3. Verify alpha edges against black, gray, and white test backgrounds.
4. Use nine-slice scaling for frames; never stretch skulls, spikes, medallions, or corners.
5. Tile or minimally stretch center stone textures.
6. Derive missing hover, disabled, focus, and pressed states consistently.
7. Optimize runtime PNGs without color-indexing away smooth alpha.
8. Keep text out of bitmap assets, except the icon wordmark used at large sizes.
9. Keep Native controls available per element if an asset fails validation.

## Preparation order

1. Application icon family and `.ico`.
2. Content panel nine-slice (most reusable element).
3. Horizontal command/navigation button states.
4. Checkbox and radio state sheets.
5. Square item-preview and compact-button frames.
6. Section headers and ornamental dividers.
7. Main outer frame last, after real content insets are known.

## Review decisions needed before preparation

- Confirm whether the outer frame should be always visible or Hellforge-theme-only at larger window sizes.
- Confirm whether purple accents represent magic/content identity or are purely decorative.
- Confirm whether the strongest fire-glow variant means Hover, Pressed, or Active/Selected.
- Confirm whether the 256 icon should also serve as an About/splash image.
- Confirm whether custom context menus are in scope for the first theme release; recommendation is Native menus initially.
