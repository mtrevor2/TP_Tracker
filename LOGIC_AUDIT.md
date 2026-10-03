# Logic audit — 0.4.23

The authoritative reference is this checkout's Randomizer, rather than a generic vanilla walkthrough. The wiki's List of Randomizer Checks explicitly says it is not a definitive logic-based list: https://wiki.tprandomizer.com/index.php?title=List_of_Randomizer_Checks .

## 0.4.40 follow-up: hidden hint and controller inspection

The user reported controller inspection failing on both map types with Map: Show Check Info hint disabled. The previous flag was referenced only by hint drawing, and no direct input-disable assignment was found. A separate confirmed input limitation accepted only a south-button press after the trigger was already down. The handler now detects a rising combined RT/south state on either button or trigger events, so either press order works. Both halves must come from the same controller; repeated held events or moving a held chord onto an icon cannot activate it.

The hint's optional occlusion rectangle is now separate from rendered check targets, and hiding it explicitly clears only that rectangle. Both native map renderers share this selection state. Automated input tests run with the hint hidden, shown, then hidden again, checking both press orders, mouse clicks, empty map space, cross-controller rejection, repeat suppression and one-step B/Esc dismissal. Preference tests verify hiding the hint leaves map/minimap availability enabled and independent. These tests verify input/selection behavior; the user's exact native gameplay failure has not been independently replayed, so the press-order issue is not claimed as a proven cause of the toggle-specific report. Version remains 0.4.40.

## 0.4.40 follow-up: desert and Palace warp access

Randomizer randomizer_context.cpp initializes mMapBits to 0x20 when Unlock Map Regions is On; tools.cpp::setRegionBit applies this to the native saved field map. Desert is region 5 (1 << 5). Nevertheless world/Root.yaml requires the generated Desert Province Map Sector event for both Gerudo Desert and Mirror Chamber portals, unlike the other portals' setting alternatives. This makes these warps depend on first reaching the desert by a different route. The tracker exporter now accepts Unlock Map Regions, actual saved region-5 discovery, or the existing reachable-sector event. Owning the respective portal and satisfying Can Use Warp Portals remain required. The new Desert Map Unlocked inventory fact reads dComIfGs_isRegionBit(5) on each scan; it never writes the save.

A regression reproduces the starting-portal/Shadow-Crystal failure without Auru's Memo, then verifies the two portal routes, missing portals, missing Crystal, region unlock Off, saved discovery and discovery reset. All 20 Palace of Twilight catalogue entries are OPEN with full required inventory and Memo absent. Entrance tests cover Open, four Mirror Shards, three Fused Shadows, and Vanilla/City completion; negative cases preserve small-key doors, the big key, Light Sword and Clawshot, with Keysy alternatives. These are model/runtime-adapter tests rather than replay of a supplied affected save. The user requested that this correction remain version 0.4.40.

## 0.4.40 event queries, City forms and arena approaches

The native `dSv_event_c::isEventBit` hook in Randomizer `hooks.cpp` returns true for `ZORA_ESCORT_CLEARED` (0x0810) in Castle Town and Renado's room to avoid unwanted dialogue/cutscenes. It returns false for `GORON_MINES_CLEARED` (0x0701) in the main dungeon/Death Mountain interiors, and for `HOWLED_AT_SNOWPEAK_STONE` (0x3A08) in Snowpeak. Those are compatibility answers, not stored collection state. Tracker event reads now use the documented native byte/mask representation read-only, including reward flags and Gate Keys; this preserves the distinction between obtaining Rutela's reward (0x0804) and unlocking access to it (0x0810).

`randomizer_item_func_ANCIENT_DOCUMENT2` sets `SKY_CANNON_REPAIRED` (0x3B08) and the private sky-character counter rather than installing the completed document in the native slot. The repaired-cannon bit is now a valid alternative to the full-book requirement; the seed bypass and Clawshot remain explicit alternatives/requirements. Exact partial Sky Book counts remain unavailable. The supplied settings fixture has the Sky Book bypass On; all 29 City entries pass with the required inventory. This does not reproduce the reported all-locked live snapshot, for which no save/inventory snapshot was supplied.

City Garden Island Poe formerly required Double Clawshots (human) and Senses (wolf) in one form. A local reach-island event separates the steps. The central tower rope area is reachable only by using a tightrope (wolf); requiring Tightrope again together with Climb Vines (human) made both ledge chests impossible. Their local rule now requires Climb Vines after reaching that area. Regressions verify missing Clawshot, missing wolf form, bypass Off, repaired cannon, full-book and Gate Keys/Keysy cases.

Arena positions previously preferred a stage-level restart spawn. Lakebed Deku Toad's stage restart points to room 9 spawn 3 near (-46 Y, -306 Z), away from the actual submerged approach (room 9 spawn 1 at -1325.58 Y, -4280 Z) and upper door (spawn 2). Export now prioritizes reciprocal room-return spawns and retains both genuine approaches. Dangoro's check is physically in parent stage D_MN04, so stage-suffix discovery missed it; it now explicitly belongs to the D_MN04B entrance group while retaining its true chest stage/flag. No randomized reward is invented for minibosses without one.

Six WdStone actors are selected by native event-index parameters 0x1D8..0x1DD. Their actual room coordinates replace the wolf encounter positions; completion flags remain the reward's original 0x3C/0x3D bits. `mapHidden` is read from the six 0x3A howl bits each scan, is cleared between saves, and only affects marker rendering. The introductory Faron wolf is the seventh encounter and has no stone.

## 0.4.35 native ownership, mixed forms, hint encoding and marker audit

`randomizer/src/item.cpp::check_item_get` intentionally returns false for Pumpkin/Cheese inside Snowpeak, swaps Ball and Chain ownership for the Darkhammer pickup switch, and masks shields/Hawkeye at shops. Those queries support game dialogue and are unsuitable as inventory truth. Tracker overrides now read the first-item flags (Pumpkin 0xF4, Cheese 0xF5, Ordon Shield 0x2A, Hylian Shield 0x2C, Hawkeye 0x3E) or Ball and Chain SLOT_6/0x42. Sword ownership uses first-item flags set by `exec_item_get`, including starting inventory (`session.cpp`), instead of collect/equip bits which `dComIfGs_setSelectEquipSword` can set independently. This hardens the reported new-save symptom; no affected seed was supplied to reproduce that symptom.

The full Goron big key grants only the third shard flag, so summing distinct shard flags can return 1 instead of 3. The adapter now recognizes the highest shard or boss-key flag, and registers the item as countable. Regressions cover 0..3 shards, full keys, Own Dungeon/Keysanity/Keysy, and required Fyrus combat items.

Poe routes such as `Can_Break_Webs and Can_Use_Senses` are impossible in a single form. Seven explicit local-obstacle events now separate clearing webs/ice/armor, moving statues or purchasing Flight by Fowl as human from subsequently collecting the Poe as wolf. The solver still has to reach the area in each form and satisfy the original obstacle/trick setting. Lake cave's existing equivalent remains unchanged. Snowpeak keys, Pumpkin/Cheese routes and boss access retain the upstream requirements.

The nine unconditional Ball and Chain substitutions for ranged bugs were removed. The current randomizer's per-location rules are authoritative. The supplied lunarsoap5 tracker was read for logic comparison only; none of its coordinates were used. The wiki's Ball and Chain Pickup trick establishes pickup collision, not reachability of every raised bug: https://wiki.tprandomizer.com/index.php?title=Glitches_and_Tricks#Ball_and_Chain_Pickup . Ground-level bug routes remain available normally.

`d_msg_class.cpp` defines US male/female glyph bytes as B2/B3 (Japanese 8189/818A). These are not UTF-8 and previously reached nlohmann JSON dump unconverted. Notebook conversion now translates the glyphs, preserves valid UTF-8, replaces unsupported bytes, bounds the render-buffer view and rolls back on serialization failure. Partial pages, raw native glyphs and JSON round-trips are tested. This establishes a concrete tracker-side failure path consistent with the Ordon hint-sign report, but does not prove the origin of that player's crash without their log/save.

Shad/Charlo/roasted boar are mapped through NPC_SHAD, NPC_PRAYER and OBJ_RW respectively. Camp chest IDs are changed to 31/30 by `object_patches.yaml`; applying chest/item/key patches before matching restores those markers and the guard key. The same correction resolves an added Flight by Fowl reward and separates its chest positions. Shad's basement uses its actual external sanctuary return spawn. No web-tracker coordinates, invented positions or item-placement spoilers are used.

Independent map/minimap filters share one tested OPEN-only predicate, applied to native/static positions, interior groups and dungeon arena counts; empty temple groups are hidden too. Existing seed/category filtering remains in force. Native game icons are outside this overlay's control. All modifications read game inventory/flags; only tracker notes/preferences/journal are mutable. No claim is made that every seed or shuffled entrance route is now solved; existing Sky Book private-count and entrance-shuffle limitations remain.

## 0.4.34 missing river minigame markers

Plumm is a dynamic `item_check_commit("plumm_minigame_reward", ...)` in `d_a_npc_myna2.cpp`, missed by the static DUSK_ITEM_CHECK actor scan. Lake Hylia has ambiguous stage membership, so the catalogue now explicitly maps this check to F_SP115. Its myna2 actor in room 0 supplies the real position.

Fishing Hole's placed heart piece is `htPiece`, not `item`. Both use an eight-bit save flag at parameter bits 8..15 (`daObjLife_c::getSaveBitNo`). Supporting this actor adds the actual F_SP127 flag-0x80 point and four other correctly flagged outdoor heart-piece markers. The bottle is a scripted catch without a placed actor: its point is parsed from `d_a_mg_rod.cpp`'s `cXyz bin_pos(6800.0f, 30.0f, -270.0f)` catch region. Hena's R_SP127 cabin is another Current Area for the heart-piece check because the world logic provides its canoe-rental route there.

Iza's two atlas entries already had valid F_SP126 entrance spawns, but the map loader took its F_SP112 stage prefix as evidence of an outdoor point and ignored the explicit exterior anchor. It then rejected room -1 placeholder coordinates. The shared worldMapAnchor selector now prioritizes explicit entrances, preserves true outdoor coordinates and rejects non-local placeholders. Both Iza event flags remain distinct (0x0B01 vs 0x5908); native reward aliases complete each one independently. The single remaining check at an entrance is named in the hover label instead of an opaque interior label/count.

Regression tests exercise the actual map-loader selector, affected Current Area memberships, all five scripted grant aliases, distinct Iza rewards/flags, fishing/clawshot heart-piece deduplication, NPC seed filtering and exact actor/catch-region coordinates. No game state is written. Live map layout still needs gameplay confirmation.

## 0.4.33 Memo inventory and Forest Temple key door

Randomizer `src/item.cpp::randomizer_item_func_RAFRELS_MEMO` places item 0x90 in SLOT_7. Vanilla `src/d/d_item.cpp::item_getcheck_func_RAFRELS_MEMO` queries SLOT_19, explaining the missing memo despite its item-wheel presence. TPTracker now reads SLOT_7 directly and retains the item when event 0x2680 records delivery to Fyer, matching the Randomizer's `src/tools.cpp` inventory reconstruction. This is read-only and does not infer ownership from the memo reward check or spoiler placements.

The generator's Forest Temple East Water Room <-> Second Monkey Outside Room exits require four total small keys or Keysy. Native D_MN05/STG_00 door parameters 0x6c102201 identify front room 1, back room 2 and a front key lock; angle.z 0xff0b supplies switch 0x0B in stage-save 0x10. The reported under-bridge chest is in room 2. The tracker adds a human-form route through this specific door when its unlock switch is set or at least one unspent Forest Temple key remains. The original four-key/Keysy alternatives remain. Other door requirements and access to the parent area are unchanged. The native stage-switch accessor selects live flags in the current dungeon and stored flags elsewhere.

The unspent-key reading is captured before adding consumed keys to the existing total-key inventory count. Both runtime facts are refreshed each scan and cleared with the rest of inventory on save changes; worker snapshots carry the same facts as inventory. Tests verify held memo, delivered memo, missing memo, sketch discrimination, save reset, a locked door with zero keys, one usable key, a spent key with an open door, a key spent elsewhere, other-gate isolation, Keysy/all-key fallback and parent-route enforcement.

## 0.4.32 Lake Lantern Cave correction

The generator requires Lantern for every location in Lake Hylia Long Cave. TPTracker now treats darkness as optional for the 13 ordinary chests, three Poes and hint sign, retaining Can_Smash and Can_Use_Senses where present. This correction is applied in export_catalogue.py before producing both per-check access and area Locations, so regeneration and the UI evaluator use the same rules.

Two chests genuinely require torch lighting. The user's extracted D_SB03/R00_00.arc TRES records identify flag 14 (Seventh Chest in the installed Randomizer locations.yaml) as tboxB1, parameters 0xff151381, waiting on switch 0x51. AND_SW2 parameters 0x2e510002 combine the two candlL2 switches 0x2e/0x2f into 0x51. End Lantern Chest, flag 3, has parameters 0xff12d0c1, waiting on 0x2d; AND_SW2 0x2b2d0002 combines torch switches 0x2b/0x2c. These two keep Can_Smash and Lantern. Ordinary Sixth Chest is flag 8, tboxA0, parameters 0xff0ff200, with no spawn switch. The Randomizer's D_SB03 object patches only add its hint sign and do not remove the torch puzzles.

The wiki confirms two torch rewards but uses older Sixth/Seventh naming: https://wiki.tprandomizer.com/index.php?title=Long_Lantern_Cave and https://wiki.tprandomizer.com/index.php?title=List_of_Randomizer_Checks . Runtime names and actor flags take precedence. Collection-guide overrides reflect this distinction. No raw disc records are packaged.

Poe routes use a cave-local boulder-cleared event before Can_Use_Senses. This permits smashing as human followed by collecting as wolf; requiring Can_Smash and Can_Use_Senses in the same form would leave the Poes permanently locked.

Regression scenarios verify ordinary chests without Lantern using either Bombs or Ball and Chain, both torch chests locked without Lantern and open with it, boulder-blocked routes still locked, Poes requiring Senses, and Check Details matching the corrected local requirement.

## 0.4.27 follow-up

The async solver previously transferred only check statuses back to the UI model. Its area/form reachability and event maps were discarded, so details incorrectly displayed UNKNOWN even for solved areas. These maps now transfer atomically with the status result, retaining the existing generation guard and leaving live inventory, skips and completion data untouched.

Snowpeak regression fixtures verify that local combat equipment does not bypass the route into Chapel. Own Dungeon small keys plus the cheese gate permit the equipped route, and Keysy supplies the supported alternative. Missing route requirements remain locked; this fix does not mark an entire dungeon reachable just because Link entered it.

The 51 Freestanding and 37 Hidden Rupee checks require their seed option to be On. Both kinds have separate map/minimap filters. Coordinates come from item, stone/stoneB and carry actor flags, including the randomizer's object-patch reassignment of shared vanilla flags. All 88 have verified stage/room actor positions; raw stage records are not distributed. Asset validation covers 541 mapped checks and all 14 separate boss/miniboss entrance groups.

## Confirmed fixes

`randomizer/src/item.cpp` grants Coro's key by setting stage-save 2, switch 0x0C. The vanilla check function reads the current area's key count, so it cannot identify this randomized key. TPTracker now reads the persistent switch. North Faron's key similarly uses stage-save 2, switch 0x14; escort Gate Keys use event 0x0810.

The supplied SkullKid Borville Borville log uses Open Faron Woods, skipped prologue, cleared twilight and Own Dungeon small keys. `generator/data/world/overworld/Faron Province.yaml` permits Coro's key, wolf digging, Keysy or twilight through the gate. The mist chest still needs Lantern and completed/skipped prologue. Tests demonstrate Coro key + Lantern opens it, removing Lantern locks it, and Shadow Crystal/Keysy provide alternate gate access.

`generator/logic/requirement.cpp` compares ordered setting options. Previously the tracker accepted only numbers for >= and <=. The two current named-option comparisons are Ilia Memory Quest >= Statue (Doctor's Office) and >= Charm (Hidden Village). Option order is now exported from settings_list.yaml and every pair is regression-tested.

## Audit scope

- Re-exported 658 check definitions, 564 areas, macros and setting options from the generator. The export retains the existing documented pickup/shop adjustments.
- Compared AND/OR grouping, macros/events, human/wolf/day/night/twilight, counted items, setting-based thresholds, hearts, bugs and dungeon completion against the generator requirement evaluator.
- Reviewed runtime inventory adapters against Randomizer item grants, including gate keys, Dominion Rod, bottles, tears, shards and dungeon keys.
- Ran portable model/controller regressions and validation of 455 map positions, 78 inventory entries and their packaged textures.
- Check details use the same evaluator and seed settings as accessibility. They distinguish local requirements from reaching the area and preserve alternative routes.

## Limits

This is not an exhaustive in-game verification of every inventory, setting and entrance combination. Shuffled entrance connections remain intentionally unevaluated; the details window warns when the seed enables them. Unknown inventory/requirements stay UNKNOWN, not OPEN. Progressive Sky Book's separate Randomizer character counter has no tracker adapter and remains a known limitation; do not infer its count from spoiler placements. Existing Agitha delivery and Kakariko shop convenience adjustments differ from strict generator search, as documented in the code.

Both seed logs contain the same Settings section; changing to the spoiler log does not repair an inventory adapter. The tracker reads settings without using spoiler item placements as proof of ownership. No game inventory, key counters or Randomizer save blobs are written by these fixes.

The popup and platform binaries compile, but visual/controller gameplay validation must be performed in Dusklight. Native iOS loading still requires a compatible signed/bundled host, as described in MOBILE.md.
