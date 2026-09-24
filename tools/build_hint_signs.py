from dependency_paths import DUSKLIGHT, RANDOMIZER
"""Export exact hint sign identities from randomizer actor patches (no hint text)."""
import json
from pathlib import Path
import yaml

root = Path(__file__).resolve().parents[1]
patches = yaml.safe_load((RANDOMIZER / 'generator/data/object_patches.yaml').read_text())
signs = []
for stage, rooms in patches.items():
    for room, actors in rooms.items():
        for actor in actors:
            name = actor.get('flow', '')
            if actor.get('name') == 'Obj_kn2' and name.endswith('Hint Sign'):
                signs.append(dict(name=name, stage=stage, room=room,
                                  pos=[actor['position'][axis] for axis in ('x','y','z')]))
assert len({s['name'] for s in signs}) == 34
(root / 'res/hint_signs.json').write_text(json.dumps(signs, indent=2)+'\n', encoding='utf-8')
print(f'Exported {len(signs)} hint sign placements')
