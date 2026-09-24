# Logic audit — 0.4.23

The authoritative reference is this checkout's Randomizer, rather than a generic vanilla walkthrough. The wiki's List of Randomizer Checks explicitly says it is not a definitive logic-based list: https://wiki.tprandomizer.com/index.php?title=List_of_Randomizer_Checks .

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
