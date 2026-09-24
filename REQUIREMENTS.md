# Acceptance status

Implemented in 0.3.0: native F1 toggle, two tabs, vertical rows, status filters, area headings, text search, completion hiding, per-slot journals, selected saved-seed lookup, vanilla connection disclaimer, twilight state, restored rod and expanded inventory/count adapters, exact grant aliases, native province/area map markers for matching TRES records and cursor status labels.

Build and portable logic tests pass. Required live verification remains:

- Check heading spacing and status filtering at normal and increased UI scale.
- Load two different slots and confirm each displays its own seed and completion.
- Create a seed, save it, and confirm automatic binding on the next update.
- Collect an owl character and a twilight insect; verify DONE regardless of accessibility.
- With Golden Bugs shuffled, verify collecting a bug item elsewhere never completes its original location.
- Open the map, zoom/pan, target markers and verify tooltip placement and DONE colors; close/reopen and disable/reload the mod with the map open.
- Validate widescreen, mirror mode, clipping, native texture availability and frame cost in-game.
- Verify no journal or worker result survives loading a different save.

Outstanding functionality: complete coordinates for non-chest checks, kingdom overview markers, dungeon-map integration, live private Sky Book count and any remaining generated logic, passive draggable tracker window, and native multi-monitor window support. The existing UiService focus-stack window cannot fulfill the last two requirements without a host API extension.

The mod must never modify game progression or seed placements. Unsupported state must remain visibly unknown. Full-map or full-logic parity must not be claimed until the outstanding coverage and live checks are complete.
