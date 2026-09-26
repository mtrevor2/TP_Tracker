# TPTracker 0.4.34

## 0.4.34 river minigames and Fishing Hole markers

Plumm's Fruit Balloon Minigame now has an explicit Lake Hylia area mapping and an actor-derived map/minimap marker. Fishing Hole Bottle uses the game's fishing catch-region coordinates, and its Heart Piece uses the actual collectible position. The heart-piece check also appears in Hena's cabin, where the canoe route starts. Special heart-piece actor support restores four additional missing outdoor markers.

Both Iza rewards now use the boat-rental entrance on the Upper Zora's River map/minimap, instead of invalid placeholder coordinates from the boat-course stage. They remain separate checks and completion flags. When one reward remains at an entrance, the hover label names that check, making Raging Rapids visible after Helping Hand is completed. NPC seed filters and skipped/completed marker hiding still apply. Native reward aliases also recognize these scripted grants immediately.

## 0.4.33 Forest Temple door and Auru's Memo

Auru's Memo is read from its randomizer inventory slot and remains recognized after it is handed to Fyer, restoring the memo-dependent Gerudo Desert route. The Forest Temple second-monkey door now recognizes its saved unlock flag or one unspent Forest Temple Small Key instead of always waiting for the generator's four-key threshold. Existing Keysy/all-keys routes and other dungeon gates keep their requirements. The under-bridge chest's collection guide now describes the correct approach.

The GameBanana upload bundle `TP_Randomizer_Tracker_v6.5.zip` contains the 0.4.33 multiplatform `.dusk`, this README and REQUIREMENTS.md.

## 0.4.32 Lake Lantern Cave requirements

Ordinary Lake Lantern Cave chests, the three Poes and the hint sign no longer require Lantern simply to navigate the darkness. Boulder access still requires Bombs or Ball and Chain, and Poes still require Senses. Lantern remains required for the two torch-spawned chests: Seventh Chest and End Lantern Chest. Dusklight's Seventh Chest is the mid-cave torch puzzle that older guides call Sixth. Collection directions and regenerated logic now agree, with regressions covering both boulder tools, the torch exceptions and blocked routes.

## 0.4.31 invisible controller scroll targets

Removed the yellow focus bars shown while scrolling Check Details and the Progression/inventory panel. The same invisible scroll targets in Notes and the collection-guide sidebar stay transparent in focused, selected, hovered and pressed states. Controller scrolling and ordinary check/button highlights are preserved.


## 0.4.30 collection guides

Check Details now has a **How to obtain** sidebar instead of controller instructions. All 658 catalogue checks have bundled collection directions, including all 88 optional rupees, 34 hint signs, and 48 Twilit insects. The wider sidebar scrolls through individual paragraphs and retains source credits. Guides work offline and do not import randomized reward placements.

Most directions are adapted from [Gleed's Tracker](https://github.com/gleedgleed/gleeds-tracker/tree/f0b121a35b8b296f62fd35e7ff0517a65d42ce88/explanations), with concise missing-check additions researched from the [TPR Wiki](https://wiki.tprandomizer.com/index.php?title=Hints) and Zelda Dungeon. `res/CHECK_GUIDE_SOURCES.txt` contains license notices; `res/check_guides.json` retains each guide's sources. Rebuild with `tools/build_check_guides.py` and the pinned archive documented there. Correct or extend entries in `tools/check_guide_overrides.json`.

Directions describe the normal collection method in the unmirrored GameCube layout. They are separate from the seed-aware requirements and availability shown on the left; they do not enumerate every trick or shuffled entrance route. Numbered pickups at one site may share instructions. Missing or malformed guide resources fall back to a message without disabling the tracker. Guide coverage, escaping and malformed-resource handling are tested; controller presentation still needs live gameplay validation.


## 0.4.29 rupee artwork at close zoom

Hidden and Freestanding Rupees show their existing Available/Locked rupee artwork at the closest overworld and temple map zoom. Zoomed-out maps and minimaps retain blue dots for obtainable rupees and gray dots for inaccessible rupees. A single remaining rupee at an interior entrance also uses the rupee artwork at close zoom; multi-check entrance groups keep their count markers. Seed settings and visibility filters still apply.


## 0.4.28 blue rupee markers

Obtainable Hidden Rupees and Freestanding Rupees use blue dots on overworld maps, temple maps and minimaps. Inaccessible rupees stay gray. Interior groups containing only remaining rupee checks also use blue when accessible; mixed groups retain their normal color. Seed gating, category toggles and completion/skip behavior are unchanged.


## 0.4.27 navigation, rupees and check details

- Freestanding Rupees and Hidden Rupees have separate Map and Minimap visibility toggles in F1 > Mods > TPTracker. They appear in the checklists and maps only when their respective seed shuffle setting is On. All 88 rupee locations have actor-derived coordinates, including randomizer-reassigned item flags.
- Left stick/D-pad navigation can move between inventory and check/notes panels and scroll through inventory, Notes and Check Details. Hidden note controls no longer take controller focus. Right-stick checklist page changes retain the row; native page controls retain their focus.
- Skipped check details show **Skipped**, with **Undo skipped check?** on hover/focus. Selecting it restores the check.
- Zero-accessibility temple and boss entrance markers show a gray dot without a number. Temple totals now follow the map category toggles.
- The background solver now returns area reachability and events together with check status. This fixes misleading UNKNOWN area details caused by discarding those results; real unknown requirements and route restrictions remain intact.
- Regression tests cover the async results, Snowpeak Chapel routes with keys/cheese and Keysy, rupee seed gating and category-filtered dungeon counts. All seven platform binaries share these fixes. Controller layout and map placement still need live gameplay validation.


## 0.4.26 temple maps and Notes navigation

- Temple pause maps now display tracker check markers using the game's floor, zoom and pan transforms, with the existing map category filters.
- Separate boss/miniboss rooms have entrance dots on temple maps and minimaps. Yellow means at least one accessible check; gray means none. The number above shows accessible checks. Completed and skipped checks are excluded, and empty groups disappear.
- Entrance positions cover all 14 separate boss/miniboss check stages and are derived from game exits and return spawns, with Zant's actual boss-door actor used for his one-way entrance.
- Right-stick up/down navigation now scrolls through personal notes and recorded hints.

## 0.4.25 controller details scrolling

Move the controller right stick up/down to scroll through Check Details requirements and reach the Skip/Undo button. Navigation stays in the details popup and resets when it closes.

## 0.4.24 readable check details

The details popup now explicitly renders every text row as a block, with spacing between the title, status, requirements and helper definitions. It no longer lists every incoming world connection, which could bury a simple hint-sign requirement in unrelated door and boss routes. Requirements of Nothing read as "No additional items required." Area access is still evaluated separately; this is a presentation-only change.

## 0.4.23 gate logic and check details

- Coro's key, North Faron's gate key and escort gate keys now read the persistent unlock flags used by the Randomizer. Coro's key no longer depends on the current area's small-key counter.
- Select a check in either checklist to open its requirements window. It shows area access, local requirements, named logic helpers and relevant inventory/settings. Skip and Undo are buttons in that window; opening a check does not skip it.
- Ordered seed settings now follow the generator's option order, fixing Ilia Memory Quest comparisons for the Doctor's Office and Hidden Village routes.
- Regression coverage includes Coro key plus Lantern, missing Lantern, Shadow Crystal and Keysy, all exported setting-option ordering pairs, details/skip behavior, and existing warp, bugs, shops, grottos, notes and map assets.

See `LOGIC_AUDIT.md` for source references and remaining limits. Platform binaries are built from the same source; live gameplay and mobile-device validation remain necessary.

## 0.4.22 Mirror Shard and mobile builds

Progressive Mirror Shard now uses the randomizer's mirror_shard_4.bti artwork instead of the Sky Book sprite. Android ARM64 and iOS ARM64 cross-builds are included alongside desktop targets. Mobile binaries require device validation; iOS native loading also requires compatible app signing/bundling.

## 0.4.21 corrected inventory icons and bottles

Corrected the twelve reported item icons using the native item resource identities. Matching Henriko textures are used for Boomerang, Bomb Bag, Auru's Memo and Sky Book; the other corrected icons use their actual game/randomizer artwork where no exact pack replacement exists. The sidebar shows only Empty Bottle (four slots, including filled bottles). Bottle reward checks and their map markers are unchanged.

## 0.4.20 personal note cards and inventory artwork

Type a new note and press Enter to add a separate card. The field clears after submission; select it again to write another. Each card has a Delete button. Up to 64 cards (1,000 characters each, 16,000 total) are stored per save. Existing 0.4.19 personal text becomes the first card. The host text control also commits when leaving the input; Escape cancels.

The sidebar now uses Henriko Magnifico's inventory artwork with full labels, counts, and dimmed unowned items. All 81 tracked non-portal entries are grouped into Progression, Collectibles, Keys, and Quest items. The catalogue comes from the generator's Major item classification; type-specific sections can also contain progression items. Golden Bugs and bottles are Collectibles. All big keys share the big-key texture, and all small/camp/gate/Coro keys share the small-key texture. The installed pack did not provide identifiable unique sidebar sprites for Bedroom Key, key shards, Shadow Crystal, or cheese, so these use labelled shared artwork. The pack's generic bug icon is shared by the 24 Golden Bugs.

Artwork mapping is in `res/inventory.json`; `tools/build_inventory_icons.py` recreates thumbnails from the installed texture pack. See `res/ARTWORK.txt` for credit. Classification references: generator `data/items.yaml` and https://wiki.tprandomizer.com/index.php?title=Dungeon_Items .

## 0.4.19 notes and skipped checks

- Notes tab records the displayed pages of the 34 randomizer hint signs when read and marks the corresponding checks DONE. Sign identity comes from the randomizer's exact actor placements; unread hints and spoiler-log placements are never imported.
- Personal notes can be typed and edited with the keyboard (4,000 characters). The keyboard tracker shortcut is suspended on Notes to avoid closing the window while typing.
- Select an unfinished checklist row to toggle SKIPPED. Skipped rows are purple and crossed out; both checklist tabs have a SKIPPED status filter for restoring them. Actual item collection takes precedence over skipping.
- Skipped checks disappear from both maps and availability totals, including temple counts. Skipping never grants inventory or records item completion.
- Notes and skipped checks belong to the selected save slot and are retained when you save the game. Existing journals remain compatible. Hint text is recorded page by page, so advance through a hint to retain every page.

## 0.4.18 temple indicators

The province and close-up overworld maps now show entrance icons for all nine main dungeons. TempleAvailable is used when at least one unfinished check is OPEN, with a count such as `3x` above it. A gray dot without a number is used at zero (updated in 0.4.27). Hovering shows the dungeon name and count. Counts exclude completed, locked, unknown, seed-disabled checks and hint signs; they follow the map category filters (updated in 0.4.27). Coordinates come from entrance return spawns and room transforms, with Goron Mines projected through the sumo hall's exterior exit. Existing province totals continue to count overworld checks separately.

## 0.4.17 fixes

- Kakariko Malo Mart purchases and Barnes Bomb Bag no longer require the solver's Lake Hylia rupee-farming route. OPEN indicates access to the purchase; the player still pays its price. Shop entry, twilight and Hawkeye's sharpshooting unlock remain required.
- Rename the misleading Renados Sanctuary Front Door marker to Renado's Sanctuary: Renado's Letter. It represents the real letter reward, not opening a door; its quest requirement remains intact.

## 0.4.16 fixes

- Restore the generator's implicit Can Warp and Map Sector events. Shadow Crystal now opens reachable, unlocked warp destinations according to seed settings while individual check requirements still apply.
- Recognize Ball and Chain pickup for the nine Golden Bug routes previously requiring Clawshot or Gale Boomerang: Faron female beetle, both Sacred Grove snails, both Eldin phasmids, both Lanayru stag beetles, and both Lake Hylia Bridge mantises. This is an additional pickup route, not an override of area access. See the [randomizer wiki's pickup documentation](https://wiki.tprandomizer.com/index.php?title=Glitches_and_Tricks#Ball_and_Chain_Pickup).
- Identify reused grotto rooms by their entrance scene layer in Current Area and minimap markers.
- Add actor-derived Sacred Grove Master Sword and Shadow Crystal pedestal coordinates to both maps.
- Fall back to the matching spoiler log when the anti-spoiler log is missing. Only settings are read; item placements are not used to infer completion or inventory.

## Platform builds

The **TPTracker platforms** GitHub Actions workflow builds native Windows x64,
Linux x64 (including Steam Deck), Linux ARM64, and Intel/Apple Silicon macOS
packages. Download its **TPTracker-multiplatform** artifact and extract the
single `.dusk` file. Install it using the matching native Dusklight application.
Steam Deck running the Windows game through Proton uses the Windows binary;
native SteamOS uses the Linux binary.

Keyboard assignment, controller assignment, and right-stick checklist navigation
use the host's SDL input events on every platform. Bindings start unassigned.
Hardware/gameplay testing on each target is still needed in addition to CI tests.

Native C++ tracker for Dusklight, using RmlUi/UiService, HookService, ItemService and SaveService. No external tracker, browser bridge, network connection or runtime Python is required.

## Use

- Open F1 > Mods > TPTracker and enable **TPTracker**.
- Both tabs have an **All statuses / OPEN / LOCKED / DONE / UNKNOWN** dropdown. Checks appear on separate rows. All Checks has area headings.
- F1 also offers text search, Hide completed checks, and Native map check markers.
- A saved randomizer slot automatically selects its seed settings. New seeds can bind after their first save. The manual anti-spoiler log / JSON picker remains available.
- Open the game's map and zoom into a province or area. Move the game's cursor over a marker to see the check name/status. Green means accessible, red locked, amber unknown, and gray/struck through completed.

**Entrance connections always use vanilla routing**, including shuffled seeds. This does not promise that an accessible check can be reached through the seed's shuffled entrances.

## Changes from 0.2.0

- Explicit block layout fixes concatenated headings, subtitles and check rows.
- Status dropdowns, area grouping, and empty-filter feedback.
- Twilight is now its own logic state. Twilight-only insects become locked after a province is cleared, rather than remaining UNKNOWN merely because Twilight was unsupported.
- Restored Dominion Rod uses event 0x2580; its engine item query returns -1 and cannot establish restoration. Owl statue logic still requires the rod and a reachable area.
- Added wallet, hidden skill, tear, Poe, relic, bottle and dungeon-key adapters, plus heart/dungeon count predicates.
- Generated chest, freestanding, sky-character, shop, Poe, bug-reward and golden-wolf grant IDs are recognized. Completion is independent of reachability. Grants invalidate the UI even when the item grid does not change.
- Selected-save lookup reads the active CARD backing path, filename and slot, then the randomizer seed_hash sidecar. It validates seed.dat format 3 and the matching anti-spoiler log hash. It never chooses the most recent seed or reads placements as completion.
- Native map rendering hooks use native TRES positions and the game's map transform/cursor. Status colors, chest/tear art, heart art when available, and completion labels are drawn inside the game map.

## Remaining limits

The native map/minimap features are experimental and have not been playtested in this build. Live chest/key/tear records are supplemented with an actor-coordinate atlas. Some scripted rewards still lack verified coordinates; all 88 shuffled rupee checks have actor-derived positions. Interiors have one exterior entrance marker with available/total counts. Individual dungeon checks are summarized at their entrance icons rather than drawn on the Hyrule map. Province totals exclude dungeon checks; temple pause maps have a separate floor-aware overlay. Kingdom counts cover checks associated with loaded region stages. All catalogue checks remain available through the checklist subject to seed/category filters.

The catalogue contains 658 non-portal checks from the bundled randomizer. UNKNOWN remains valid for unsupported inventory adapters (including the randomizer's private live Sky Book count), missing settings or unsupported/generated requirements. Historical grants without persistent flags cannot always be reconstructed. Bug inventory is only correlated with original bug locations when Golden Bugs is Off; shuffled bug inventory cannot establish which location was collected.

Automatic seed lookup needs an already saved randomizer sidecar. A new unsaved seed can use the manual picker. Sidecars are read only on load/enable or after a save, never per frame. The selected seed settings and exact grant journal are stored per slot.

The SDK tracker window still captures game input and cannot detach to another monitor. The separate native map feature uses the game's map controls.

## Standalone repository

This repository contains TPTracker source, resources, tests, tools and documentation only. Dusklight is an external, pinned SDK dependency downloaded into the ignored `.deps/` directory, not part of this repository's source/history. `dependencies.json` records the exact tested revision and submodule revisions.

For an existing Dusklight checkout, skip bootstrap and configure with `-DDUSKLIGHT_SOURCE_DIR=/path/to/dusklight`. GitHub Actions builds all seven platforms and uploads `TPTracker-multiplatform`; download that artifact for distribution. Local downloads live in ignored `dist/`.

Regenerating catalogues is optional: all generated resources required to build are checked in. Run `python tools/bootstrap.py --with-randomizer` first; developer scripts use `.deps/dusklight` by default, or environment variables `DUSKLIGHT_SOURCE_DIR` / `RANDOMIZER_SOURCE_DIR`. Icon regeneration additionally needs the original texture pack/disc; these inputs are not included. Install PyYAML/Pillow/xxhash as required by the chosen script.

## Build and validation

```powershell
python tools/bootstrap.py
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build --config Release --target TP_Randomizer_Tracker_package tptracker_tests
ctest --test-dir build -C Release --output-on-failure
```

Package: `build/mods/TP_Randomizer_Tracker.dusk`.
Model-only builds support `-DTPTRACKER_TESTS_ONLY=ON`. Catalogue export uses `tools/export_catalogue.py` and PyYAML at development time only.

Tests cover catalogue integrity, grants/deduplication, save reset, stage isolation, parsing/escaping, transactional settings import, form separation, twilight access/clearance, rod tiers, count predicates and vanilla routing despite supplied shuffled entrances. Compilation and model tests do not validate native map rendering or live save binding.

See RESEARCH.md for implementation evidence and REQUIREMENTS.md for the remaining acceptance checks.

## 0.3.1 startup hotfix

Removed the hook on the empty dMenu_Fmap_c::_delete function, which the host rejected with Too short instructions. The draw callback now uses the game-owned active-map pointer and releases temporary heart graphics before returning. Map-hook failure is nonfatal, is logged with the target name, and is shown in F1; the checklist remains usable.


## 0.3.2 crash and usability fixes

- Reset GX descriptors before every solid rectangle and after tracker drawing. This addresses a command-stream corruption path when a completed tooltip draws a strike-through after textured font rendering. The reported crash ended with `indexed XF load from unmapped array 24`; live reproduction remains required to confirm no other crash paths.
- Read the actual map process instead of the debug-dependent `getProcess()` helper. Skip map transitions. Kingdom view shows open/total counts for mapped region checks; province view uses small status dots and hides completed checks; close zoom uses transparent chest artwork supplied by the user, an open chest for DONE, and native heart art. No black icon backgrounds.
- Normalize null group/item fields on import, including older catalogues. Skip a province's Twilit Insects when its seed twilight-cleared setting is On.
- Check rows are native UiService controls. Xbox stick/D-pad up/down focuses and scrolls through them using the host's existing navigation. Pressing a row does not change completion.
- Copy the selected GCI and its mod sidecars into this mod's data directory under `card-backups` on initialization/load and after completed saves. Copies are immutable and never restored automatically. Raw memory-card images are not covered. Backups reduce loss risk; the mod cannot guarantee atomic writes performed by the host.

Validation: Release DLL/package build and portable regression tests, including null catalogue fields and skipped twilight filtering. Native map visuals, controller navigation, and crash-free repeated map opening still require in-game playtesting. Save recovery is separately staged and is not automatically installed.

## 0.4.0 map and navigation update

- HUD minimap dots use the native map picture's actual draw rectangle, current room/floor visibility, mirror setting and opacity. Green is OPEN, light red is LOCKED, amber is UNKNOWN; completed checks are hidden.
- Full-map close zoom uses the supplied TPIcons chest, gift, bug, Poe, golden-wolf, owl-statue and grotto artwork. Province dots and kingdom counts respect type filters. Locked labels use a lighter red with no black text gradient.
- F1 > Mods > TPTracker contains separate map/minimap toggles for all eight check types. These display preferences last for the current mod session.
- Interior actor checks use exterior entrance anchors on the overworld map, including Link's house and basement. Generated positions use the disc's native room rotation/offset data. Unmapped checks remain in the checklist.
- Controller Down/Confirm from a tab targets the checklist controls. The inventory remains visually on the left. Forty reusable check rows and Previous/Next page controls replace building hundreds of controls on every tab switch. Live opening latency has not yet been measured.
- `tools/export_positions.py <GameCube ISO>` and `tools/build_positions.py` regenerate the atlas from a locally supplied disc; the game never reads the ISO through these scripts. `python tests/asset_tests.py` validates position references, room matching, category coverage and BTI dimensions/payloads.

Validation is limited to the Release build, model regressions and asset validation. Native hook availability, controller focus, marker alignment and repeated map opening require an in-game playtest.

## 0.4.1 playtest fixes

- Remove dungeon checks from every Hyrule-map zoom level and overview counts. Group interior checks into a single chest or grotto icon and available/total counter at both province and close zoom. Hover shows completed count too. Counters respect the category filters.
- Resolve exterior entrances from vanilla `entrance_shuffle_data.yaml`, including the correct region for shared grotto rooms. Do not infer an entrance by selecting the first exit in a shared room. Multiple doors to the same known interior use one verified exterior door. No indoor-coordinate markers or entrance fan-out grids are drawn on the Hyrule map.
- Owl-statue chests use chest artwork/category; only Sky Characters use statue artwork/category.
- Golden Bugs Off and Poe Souls Vanilla now mean unshuffled, not hidden. Display filters remain independent. All 24 golden bugs have local coordinates and an overworld position or exterior entrance. Dayflies need a different actor-parameter mask from the other bugs. All non-dungeon Poe checks have atlas entries.
- Ten checks per page, an explicit page-number control, disabled unused rows and boundary buttons. Left/right while a check is focused changes page using native NumberButton navigation. Up/down moves through checks; a page change focuses the first check. A/Confirm on a check can enter a page number; it never marks that check complete. Page buttons remain available for mouse and controller confirmation. The count label distinguishes matching checks from globally completed checks.
- Inventory displays current/maximum; upgrades use tiers, unique items use one, bottles use four, bomb bags use three, and counted/key maxima come from the bundled catalogue. Small-key counts include consumed keys, consistent with logic. Unavailable adapters retain `?` instead of inventing a quantity.

Validated with Release compilation, model regressions for pagination/collectible visibility/icon categories/maxima, and asset regressions for every golden-bug species/sex, non-dungeon Poes, dungeon exclusion, shared grotto destinations and Link's house grouping. Controller behavior and visual alignment still require a live playtest.

## 0.4.2 hover and checklist fixes

- Interior markers show available/total and completed counts only in the hovered marker's tooltip. Kingdom overview counters are unchanged.
- HUD minimap dots are 4 pixels across instead of 6 in both native-record and atlas rendering. Province-map dots are unchanged.
- Check rows are informational buttons again: clicking or confirming a row cannot open a page-number editor. Use the dedicated Page control with left/right, or Previous/Next page buttons, to change pages. Left/right on a check row no longer changes the page.

Validation: Release package build, model regressions and asset validation. Native visuals and controller interaction still require in-game verification.

## 0.4.3 map cleanup and Xbox navigation

- Hide completed individual markers at both province and close zoom; hide an interior group when all its enabled, displayed checks are complete. Partly completed interiors retain their full totals in the hover tooltip.
- Use Grotto artwork for cave entrances as well as grottos. Remove colored status underlines from world-map icons. HUD minimap dots retain their availability colors and smaller size.
- On Windows, read the first connected XInput controller's right stick while the tracker is the top window. Up/down focuses and scrolls check rows; left/right changes pages regardless of which checklist control had focus. A held stick repeats after 350 ms, then every 150 ms; diagonal movement chooses one dominant direction. Focus is reapplied during the gesture so generic host pane selection cannot override it. Mouse/Confirm on checks remains non-editing. Other controllers/platforms retain the native controls and page buttons.

Validated: Release package, model and stick dead-zone/direction/repeat regressions, and cave/grotto asset coverage. Live controller timing and rendering still need an in-game check.

## 0.4.4 event reward locations

- Fill missing check stages from unambiguous vanilla entrance destinations. Herding Goats Reward now belongs to Ordon Ranch (F_SP00) in Current Area and has actual NPC placements on the map/minimap. Regeneration increases map coverage from 445 to 450 checks.
- Opening either checklist tab resets its status filter to All statuses. Empty results identify the stage and explain the remaining filters instead of appearing unexplained.
- Regression checks cover the goat reward's stage and coordinates, plus Lake Hylia Current Area pagination. Release build and model/asset checks pass. A completely blank panel at another location has not been reproduced; in-game verification is still needed.

## 0.4.5 minimap reward visibility

- Outdoor NPC rewards, including Herding Goats, use room visibility without rejecting their static spawn height against the rendered floor. Indoor markers retain floor filtering. Completion, category filters and map bounds still apply.
- Accessible minimap dots use darker green RGB (35, 170, 75) in both native and atlas paths.

Release build, model tests and 450-position asset validation pass. The reported ranch minimap disappearance needs in-game confirmation; the floor-filter cause has not been observed in a live debugger.

## 0.4.6 minimap color

Accessible minimap dots now use bright yellow RGB (255, 235, 30) in both native and atlas rendering paths for better contrast against the green map.

## 0.4.7 interior remaining counts

Interior hover counters show accessible / remaining checks, with completed checks counted separately. A two-check house with one completed and one locked now reads `0 / 1 available (1 completed)`. Fully completed interiors stay hidden. A single remaining locked check uses ItemChestLocked, including cave/grotto groups; other cave/grotto groups retain Grotto artwork.

## 0.4.8 entrance dots and area coverage

- Kingdom overview denominators count uncompleted checks; cleared regions show 0 / 0. Province dots are yellow when accessible and grey otherwise.
- Province interior markers are dots. Grey groups with multiple remaining checks show a small remaining-count label, such as 3x. Close zoom also uses yellow/grey dots for interiors with multiple remaining checks; single-check interiors keep their icon. Full group counts remain hover-only.
- HUD minimaps include one yellow/grey dot at each visible exterior entrance, yellow when any remaining displayed check is accessible. Completed groups disappear. Exterior atlas coordinates are already transformed and must not be transformed twice.
- Current Area includes non-dungeon checks reached through exterior entrances in the current stage, independent of map display toggles. Ordon Ranch includes both Herding Goats and its grotto chest. Existing indoor listings remain available.

Model regressions cover every mapped exterior interior and the two Ordon Ranch checks. Rendering and controller behavior still require in-game verification.

## 0.4.9 menu, sorting and bindings

All inaccessible minimap markers now use grey (including unknown logic); accessible markers remain yellow.
Current Area has Sort By: A-Z or Accessibility (OPEN, LOCKED, DONE, then UNKNOWN, alphabetical within each status).
Portal items are hidden from the inventory display without changing logic.
TPTracker is available directly in the Dusklight menu bar. Under Mods > TPTracker, choose
Toggle TPTracker: keyboard key or controller button. Both default to Unassigned and persist in
Dusklight config. Windows keyboard and XInput buttons/triggers are supported; shortcuts only run
while Dusklight is foreground and toggle once per press. Choosing Unassigned clears a binding.

## 0.4.10 reward counts and press-to-assign shortcuts

Province totals exclude unmapped non-marker entries such as hint signs and continue counting each
unfinished reward separately, including individual accessible rewards within grouped interiors.
Agitha uses owned bugs minus delivered rewards, independently of the solver's Castle Town route.
Both checklist tabs support alphabetical or accessibility sorting, defaulting to accessibility.
Shortcut assignment now opens a press-to-assign prompt, waits for the initiating press to be released,
and saves the next keyboard key or XInput controller button/trigger. Clear buttons remove bindings.
Existing bindings are retained. Escape or Cancel dismisses the prompt; controller B is captured even when the host treats it as Back.

## 0.4.14 native Twilit Bug markers

Twilit Insect checks are excluded from tracker map/minimap markers and map totals,
including interior groups. The game's own markers and the tracker checklist are unchanged.
