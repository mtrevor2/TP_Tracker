from dependency_paths import DUSKLIGHT, RANDOMIZER
"""Developer-only export. Requires Python and PyYAML; never used at runtime."""
import json
import re
from pathlib import Path
import yaml

root = Path(__file__).resolve().parents[1]
rando = RANDOMIZER
data = rando / 'generator/data'
stages = re.findall(r'"([A-Z0-9_]+)"', (rando / 'src/stages.cpp').read_text().split('};')[0])
save_ids = {}
stage_labels = {}
pending = []
source = (rando / 'src/tools.cpp').read_text().split('int getStageSaveId(int id)')[1].split('int getStageSaveId(const char*')[0]
for line in source.splitlines():
    match = re.search(r'case (\d+):', line)
    if match:
        pending.append(int(match[1]))
    label = re.search(r'case\s+\d+:\s*//\s*([A-Z0-9_]+)\s*\((.*)\)', line)
    if label:
        stage_labels[label[1]] = label[2]
    match = re.search(r'return (0x[0-9A-Fa-f]+);', line)
    if match:
        for index in pending:
            save_ids[index] = int(match[1], 16)
        pending = []
world = []
for path in sorted((data / 'world').rglob('*.yaml')):
    world.extend(yaml.safe_load(path.read_text(encoding='utf-8')) or [])
access = {}
area_stages = {}
grotto_scenes = {}
# Entrance destinations provide authoritative stage metadata for event rewards
# that have no actor/chest stage in locations.yaml (for example the goat reward).
known_areas = {area['Name'] for area in world}
for entrance in yaml.safe_load((data / 'entrance_shuffle_data.yaml').read_text(encoding='utf-8')):
    for direction in ('Forward', 'Return'):
        endpoint = entrance.get(direction) or {}
        index = endpoint.get('Stage')
        if not isinstance(index, int) or not 0 <= index < len(stages):
            continue
        for field in ('Connection', 'Alias'):
            destination = endpoint.get(field, '').split(' -> ')[-1]
            if destination in known_areas:
                area_stages.setdefault(destination, set()).add(stages[index])
                if entrance.get('Type') == 'Grotto' and direction == 'Forward':
                    scene = {'stage': stages[index], 'room': endpoint['Room'], 'layer': endpoint['State']}
                    if scene not in grotto_scenes.setdefault(destination, []):
                        grotto_scenes[destination].append(scene)
for area in world:
    # The generator's four-key worst-case route is too strict for a live save.
    # This one door needs one unspent key, or its persistent unlocked flag.
    # Retain the generator route for Keysy/all-keys and every unrelated gate.
    forest_door = {
        'Forest Temple East Water Room': 'Forest Temple Second Monkey Outside Room',
        'Forest Temple Second Monkey Outside Room': 'Forest Temple East Water Room',
    }
    if area['Name'] in forest_door:
        target = forest_door[area['Name']]
        old_requirement = area['Exits'][target]
        area['Exits'][target] = (f"({old_requirement}) or (Human_Link and "
            "(Forest_Temple_Second_Monkey_Door_Unlocked or Forest_Temple_Small_Keys_Available))")
    # These events are synthesized by the generator, not stored in its YAML.
    events = area.setdefault('Events', {}) or {}
    area['Events'] = events
    if area.get('Can Warp'):
        events['Can Warp'] = 'Nothing'
    if area.get('Map Sector'):
        events[area['Map Sector'] + ' Map Sector'] = 'Nothing'
    # Darkness alone does not prevent collecting Lake cave checks. Keep the
    # boulder and Senses requirements, and Lantern only for torch-spawned chests.
    # D_SB03 chest flags 14 (Seventh) and 3 (End) use switches 0x51 and 0x2D;
    # the wiki calls the first puzzle Sixth, but Dusklight maps Sixth to flag 8.
    # Apply before building access so the world and per-check rules agree.
    if area['Name'] == 'Lake Hylia Long Cave':
        torch_chests = {'Lake Lantern Cave Seventh Chest', 'Lake Lantern Cave End Lantern Chest'}
        for name, requirement in area['Locations'].items():
            if name not in torch_chests:
                area['Locations'][name] = str(requirement).replace(' and Lantern', '')
        # Smashing is done as human, then Poes are collected as wolf. An area
        # event preserves the obstacle without requiring both forms at once.
        events['Lake Cave Boulders Cleared'] = 'Can_Smash'
        for name in area['Locations']:
            if name.endswith(' Poe'):
                area['Locations'][name] = "'Lake_Cave_Boulders_Cleared' and Can_Use_Senses"
    for name, requirement in (area.get('Locations') or {}).items():
        access.setdefault(name, []).append({'area': area['Name'], 'requirement': str(requirement)})
checks = []
for location in yaml.safe_load((data / 'locations.yaml').read_text(encoding='utf-8')):
    meta = location.get('Metadata') or {}
    if not isinstance(meta, dict):
        meta = {}
    categories = location.get('Categories', []) + list(meta)
    # These shops are opened by the randomizer's Kakariko start flags. A
    # renewable rupee source (the generator uses Lake Hylia) is not an item
    # prerequisite for visiting/purchasing their checks. Keep Hawkeye's genuine
    # sharpshooting unlock and the existing door, form and twilight conditions.
    if location['Name'].startswith('Kakariko Village Malo Mart ') or location['Name'] == 'Barnes Bomb Bag':
        for route in access.get(location['Name'], []):
            route['requirement'] = route['requirement'].replace("'Can_Farm_Lots_of_Rupees'", 'Nothing')
    # Randomized bug rewards are freestanding pickups. The Ball and Chain can
    # collect these directly; preserve area access and additional prerequisites.
    # https://wiki.tprandomizer.com/index.php?title=Glitches_and_Tricks#Ball_and_Chain_Pickup
    if 'Golden Bug' in categories:
        for route in access.get(location['Name'], []):
            route['requirement'] = route['requirement'].replace(
                'Clawshot or Gale_Boomerang', 'Clawshot or Gale_Boomerang or Ball_and_Chain')
    if 'Warp Portal' in categories:
        continue
    flags, stage_names = [], set()
    aliases = list(meta.get('Name Lookup', []))
    for kind, entries in meta.items():
        if kind == 'Event Flag':
            flags.append({'kind': 'event', 'flag': entries})
            continue
        for entry in entries if isinstance(entries, list) else [entries]:
            if not isinstance(entry, dict):
                continue
            index = entry.get('Stage')
            if index is not None and index < len(stages):
                stage_names.add(stages[index])
            flag = entry.get('Tbox Id', entry.get('Flag'))
            if index is not None and index < len(stages):
                prefix = {'Chest': 'chest', 'Freestanding Item': 'freestanding', 'Poe': 'poe'}.get(kind)
                if prefix and flag is not None:
                    aliases.append(f'{prefix}:{stages[index]}:{flag}')
                if kind == 'Sky Character':
                    aliases.append(f'sky:{stages[index]}:{entry["Room"]}')
                if kind == 'Shop':
                    aliases.append(f'shop:{stages[index]}:{entry["Room"]}:{entry["Item"]}')
            if kind == 'Golden Wolf' and flag is not None:
                aliases.append(f'golden_wolf:{flag}')
            if kind == 'Bug Reward':
                aliases.append(f'bug:{entry["Item Id"]}')
            flag_kind = {'Chest': 'chest', 'Poe': 'switch', 'Freestanding Item': 'item',
                         'Golden Wolf': 'event', 'Switch Flag': 'switch', 'Item Flag': 'item',
                         'Twilit Insect': 'chest'}.get(kind)
            if location['Name'] == 'Forest Temple Big Baba Key':
                flag_kind = 'chest'
            if flag_kind and flag is not None:
                save = save_ids.get(index, -1)
                if flag_kind == 'event' or 0 <= save < 32:
                    flags.append({'kind': flag_kind, 'flag': flag, 'save': save})
    if not stage_names:
        for route in access.get(location['Name'], []):
            mapped = area_stages.get(route['area'], set())
            if len(mapped) == 1:
                stage_names.update(mapped)
    # Scripted rewards have no placed item actor in locations.yaml. Plumm's
    # Lake Hylia area spans multiple stages, so it cannot use ambiguous inference.
    if location['Name'] == 'Plumm Fruit Balloon Minigame':
        stage_names.add('F_SP115')
    # Renting Hena's canoe is an alternate route to this same heart-piece check.
    if location['Name'] == 'Fishing Hole Heart Piece':
        stage_names.add('R_SP127')
    native_rewards = {
        'Plumm Fruit Balloon Minigame': 'plumm_minigame_reward',
        'Iza Helping Hand': 'iza_reward_1',
        'Iza Raging Rapids Minigame': 'iza_reward_2',
        'Fishing Hole Bottle': 'fishing_bottle',
        'Fishing Hole Heart Piece': 'fishing_heart_piece',
    }
    if location['Name'] in native_rewards:
        aliases.append(native_rewards[location['Name']])
    checks.append({'name': location['Name'], 'categories': categories, 'stages': sorted(stage_names),
                   'grotto_scenes': [scene for route in access.get(location['Name'], [])
                                     for scene in grotto_scenes.get(route['area'], [])],
                   'flags': flags, 'access': access.get(location['Name'], []),
                   'original_item': location.get('Original Item') or '',
                   'group': next((c for c in location.get('Categories', []) if isinstance(c, str) and c not in
                       {'Overworld', 'Dungeon', 'ARC', 'DZX', 'Npc', 'Golden Bug', 'Poe', 'Shop', 'Hint'}), 'Other'),
                   'aliases': aliases})
items = [x for x in yaml.safe_load((data / 'items.yaml').read_text(encoding='utf-8'))
         if x.get('Importance') == 'Major' and isinstance(x.get('Id'), int) and x['Id'] < 256]
result = {'version': 1, 'source': 'mods/randomizer/generator/data', 'checks': checks,
          'items': items, 'areas': world, 'stage_labels': stage_labels,
          'macros': yaml.safe_load((data / 'macros.yaml').read_text(encoding='utf-8'))}
result['setting_options'] = {
    setting['Name']: [str(next(iter(option))) if isinstance(option, dict) else str(option)
                      for option in setting.get('Options', [])]
    for setting in yaml.safe_load((data / 'settings_list.yaml').read_text(encoding='utf-8'))
    if isinstance(setting, dict) and 'Name' in setting and isinstance(setting.get('Options'), list)
}
(root / 'res/catalogue.json').write_text(json.dumps(result, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')
print(f'Exported {len(checks)} checks, {len(items)} inventory entries, {len(world)} areas')
