# Logic audit — 0.4.23

The authoritative reference is this checkout's Randomizer, rather than a generic vanilla walkthrough. The wiki's List of Randomizer Checks explicitly says it is not a definitive logic-based list: https://wiki.tprandomizer.com/index.php?title=List_of_Randomizer_Checks .

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
