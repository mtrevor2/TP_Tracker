# Logic audit — 0.4.23

The authoritative reference is this checkout's Randomizer, rather than a generic vanilla walkthrough. The wiki's List of Randomizer Checks explicitly says it is not a definitive logic-based list: https://wiki.tprandomizer.com/index.php?title=List_of_Randomizer_Checks .

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
