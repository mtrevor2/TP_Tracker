# Logic and completion research

The bundled Dusklight sources are the implementation authority for this build. The upstream project is https://github.com/TwilitRealm/dusklight.

| Finding | Source in this checkout | Implementation |
| --- | --- | --- |
| Faron Woods Owl Statue Sky Character requires Restored_Dominion_Rod | mods/randomizer/generator/data/world/overworld/Faron Province.yaml; macros.yaml | Two rod tiers plus human form, and world reachability |
| The powered rod item query is unsupported; the randomizer sets event 0x2580 | src/d/d_item.cpp::item_getcheck_func_COPY_ROD_2; mods/randomizer/src/item.cpp::randomizer_item_func_COPY_ROD_2 | Read the rod slot and restoration event |
| Twilight is distinct from human/wolf day/night; entry to uncleared regions can convert to twilight | mods/randomizer/generator/logic/requirement.cpp::EvaluateExitRequirement | Five-state fixed-point search |
| Insects use chest-bit metadata, not the item name received | mods/randomizer/generator/data/locations.yaml (Twilit Insect entries) | Stage/save-table and chest flag completion |
| Male Faron Field Beetle has Nothing as its local requirement; female requires Clawshot or Gale Boomerang | Faron Province.yaml | Preserve individual requirements and area access, never apply one rule to all bugs |
| Sky characters use sky:stage:room grant names | sdk/include/mods/items.h; locations.yaml | Export exact aliases and persist actual grants |
| Small-key search counts remaining keys plus opened key doors | mods/randomizer/src/tools.cpp::getTempleKeysFound | Same saved-door lists, current-stage or saved-stage key count |
| Randomizer seed identity is saved in its own per-slot seed_hash blob | mods/randomizer/src/session.cpp; src/dusk/mods/svc/save.cpp | Read-only lookup of exact selected CARD sidecar, decode hash, validate matching log |
| seed.dat format 3 is serialized YAML; human-readable settings are in the anti-spoiler log | User-provided Bulblin Misha GreatSpin seed files | Validate format and identity, load log settings without consuming spoilers |
| Native TRES records can survive chest collection; vanilla visibility suppresses completed records | src/d/d_map_path_fmap.cpp; src/d/d_map.cpp | Match raw native records, bypass visibility suppression, preserve game progression |

A grant's received item is not evidence of the original location in a randomized seed. Completion and accessibility are separate states. Missing data stays UNKNOWN; it is never automatically treated as free or complete.

Map coordinates are only asserted for matching native records. Full non-chest location coverage requires additional verified actor/room coordinate metadata and live map tests. No screen-percentage coordinates have been invented from the reference images.
# 0.4.1 follow-up evidence

- `Model::enabled` previously treated Golden Bugs Off and Poe Souls Vanilla as absent checks; the world solver already evaluates those locations. Visibility now retains both vanilla collectibles.
- Disc `I_Kag` actors plus `src/d/actor/d_a_obj_kag.cpp::create` establish the dayfly's two-bit location field and value-3-to-zero normalization. Male/female names are matched to all 24 entries in the bundled catalogue. Species/location cross-check: [Golden Bug locations](https://zeldawiki.wiki/wiki/Golden_Bug) (20 September 2026); exact coordinates come from the user's disc, not the web map.
- `entrance_shuffle_data.yaml` distinguishes Gerudo Desert Skulltula Grotto's return to F_SP124 room 0 spawn 8 from Lanayru's return to F_SP121 room 10 spawn 6, despite both interiors using D_SB08 room 3. Exterior positions use corresponding PLYR records and Field0 FILI transforms. Temple/dungeon categories are excluded from the world renderer and atlas projection.
- `src/dusk/ui/pane.cpp` treats Right on a controlled pane as selecting a row; ordinary buttons do not consume that direction. `number_button.cpp::handle_nav_command` consumes Left/Right before pane navigation. Check rows now bind that native control to page number only. Hidden rows also have disabled predicates to prevent navigation into unused slots. `elem_focus` returns page changes to the first check.
