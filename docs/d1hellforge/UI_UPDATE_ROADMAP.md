# D1Hellforge UI Update Roadmap

Status: planned
Companion specification: `UI_THEME_MAPPING.md`

## Goal

Add a complete optional Hellforge presentation to D1Hellforge while retaining the current Native presentation. Theme selection must never change save parsing, item generation, Workshop/Reforge behavior, movement validation, or verified writes.

Every milestone below must produce a runnable build that can be tested before the next milestone begins.

## Guardrails

- Keep `Native` and `Hellforge` as distinct presentation modes over the same application behavior.
- Allow per-element Native fallback when a custom asset is missing or invalid.
- Keep authentic Diablo/Hellfire/active-content inventory, stash, and item graphics unchanged.
- Reuse one themed rendering path for equivalent controls across Character, Inventory, Stash, Workshop, Reforge, and Advanced Editor.
- Keep labels and dynamic text in code; do not bake them into decorative images.
- Preserve keyboard navigation, focus indication, scrolling, high-DPI scaling, and readable disabled states.
- Do not replace native context menus or message boxes until accessible custom equivalents exist.

## Milestone 0 — Baseline and theme boundary

Deliverables:

- Record screenshots and dimensions for every current screen in Native mode.
- Add the appearance configuration boundary:

```ini
[Appearance]
Theme=Native
AllowNativeFallback=true
```

- Define shared theme roles for colors, fonts, spacing, borders, and control states.
- Add a runtime theme selector that can switch between Native and Hellforge, with restart allowed initially.
- Ensure an absent or damaged Hellforge asset returns that element to Native presentation.

Acceptance:

- Native mode looks and behaves exactly like the current build.
- Switching themes does not change loaded data or pending edits.
- Bad/missing assets do not prevent the editor from opening.

## Milestone 1 — Asset pipeline and application icon

Deliverables:

- Preserve the twelve source reference images unchanged.
- Create an asset manifest recording source crop, output role, state, safe insets, scaling rule, DPI target, fallback, and checksum.
- Prepare true 16, 24, 32, 48, 64, 128, and 256 pixel icon masters and one multi-resolution Windows `.ico`.
- Add alpha-edge and scale-preview contact sheets for review.

Acceptance:

- The icon is crisp in the executable, title bar, taskbar, Alt-Tab, and Explorer.
- Small icons remain recognizable and do not use a blindly scaled large illustration.

## Milestone 2 — Shared panel and semantic display skin

Deliverables:

- Prepare the content-panel frame as a nine-slice asset.
- Theme the shared semantic item-detail host used by Inventory, Stash, Workshop, Reforge, and Advanced Editor.
- Retain black background, gold body text, orange warnings, and red skull-marked negative effects.
- Theme shared status/preview notices without replacing selected-item details.

Acceptance:

- The same item produces the same lines and semantic colors in every host.
- Long details scroll correctly and negative coloring never spills into adjacent lines.
- Resizing does not distort corners or ornaments.

## Milestone 3 — Shared buttons and navigation

Deliverables:

- Prepare scalable horizontal button states: normal, hover, pressed, selected, disabled, and keyboard focus.
- Apply the shared renderer to main navigation, Workshop/Reforge commands, Advanced Editor commands, save actions, and stash navigation where appropriate.
- Prepare compact square-button states for icon commands and page navigation.

Acceptance:

- Mouse, keyboard, disabled, and active-view states are visually distinct.
- Button text remains code-driven and fits at supported DPI and configured window sizes.
- Native fallback works per button family.

## Milestone 4 — Shared form controls

Deliverables:

- Prepare checkbox and radio states, including hover, pressed, disabled, and focus overlays.
- Add restrained themed edit fields, numeric inputs, combo boxes, and list frames while retaining native input behavior.
- Reuse the same controls across Character, Workshop/Reforge, and Advanced Editor.

Acceptance:

- Tab order, caret, selection, copy/paste, dropdowns, validation, and screen-reader names remain functional.
- Checked/selected/disabled states are unambiguous.

## Milestone 5 — Inventory and stash integration

Deliverables:

- Apply themed surrounding panels, headings, navigation, and details to both Inventory and Stash.
- Keep the existing authentic game canvases, item sprites, footprint logic, collision checks, and drag overlays.
- Preserve shared selection behavior and shared item actions.

Acceptance:

- Inventory and Stash use the same selection/details/action vocabulary.
- Valid and invalid drag destinations remain immediately recognizable.
- Theme switching does not change item coordinates, footprints, or saved bytes.

## Milestone 6 — Workshop, Reforge, and Advanced Editor

Deliverables:

- Apply shared headers, panels, controls, sprite frames, semantic details, and command buttons.
- Preserve the unified Workshop controller and both Fast and Compatible Reforge engines.
- Keep all Top-3 results visible with complete selected-candidate statistics.
- Confirm configured window sizing works in both themes.

Acceptance:

- Create, Edit, Reforge, Attempt Fix, and I'm Feeling Lucky retain identical results between themes.
- Fast/Compatible selection and Reforge progress/cancellation remain usable.
- A displayed candidate exactly matches the applied candidate.

## Milestone 7 — Headers, dividers, and layout polish

Deliverables:

- Prepare full-width and half-width section headers with stretch-safe centers.
- Prepare ornamental dividers, corners, and compact status plaques.
- Normalize spacing, alignment, minimum sizes, and DPI behavior across all screens.
- Add visual regression screenshots at representative window sizes and DPI settings.

Acceptance:

- No clipped labels, controls, results, or scrollbars at supported minimum sizes.
- Decorative elements never cover interactive content.

## Milestone 8 — Optional outer shell

Deliverables:

- Prepare the main window frame last, after real content insets are known.
- Implement it as a scalable outer shell with a neutral center workspace.
- Restrict it to Hellforge mode and suitable window sizes.

Acceptance:

- The frame scales without stretching skulls, spikes, or corners.
- Maximize, restore, resizing, and multi-monitor DPI transitions remain reliable.
- The shell can be disabled independently if it causes a platform-specific problem.

## Milestone 9 — Accessibility, resilience, and release

Deliverables:

- Complete keyboard-only navigation and visible focus review.
- Verify contrast, high-DPI layouts, scrolling, reduced available space, and missing-asset recovery.
- Compare Native and Hellforge behavior using the same Diablo, Hellfire, DevilutionX, and supported mod-profile saves.
- Update user documentation, configuration comments, screenshots, changelog, and packaged assets.
- Create local and Google Drive release backups after final verification.

Acceptance:

- All focused tests pass and the release build succeeds.
- Native mode remains a dependable compatibility path.
- Hellforge mode never blocks access to an editor function.
- Existing save/item deterministic results remain unchanged.

## Proposed test rhythm

For each milestone:

1. Prepare only that milestone's assets and code.
2. Build the editor and run focused automated tests.
3. Launch both themes against the same saves.
4. Capture before/after screenshots at the configured window sizes.
5. Let the test build be exercised before continuing.
6. Record accepted behavior and known issues in `STATUS.md`.

## Decisions to lock before asset cutting

- Use the strongest fire glow for `Selected/Active`; derive subtler Hover and Pressed states.
- Treat purple as decorative/magic identity, never as a functional error or selection signal.
- Use the 256 artwork for About/splash only if it remains optional and does not delay startup.
- Keep native context menus and system dialogs for the first themed release.
- Show the ornate outer frame only in Hellforge mode and only where sufficient space exists.

## Immediate next testable chunk

Start Milestones 0 and 1 together:

- add the configuration/theme boundary with Native as the safe default;
- prepare and integrate the multi-resolution application icon;
- add the asset manifest and validation/fallback loader;
- produce one runnable build whose application content remains Native while the new theme infrastructure and icon can be tested safely.
