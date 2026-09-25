"""Validate packaged coordinate and texture data without starting the game."""
import json
import math
import struct
from pathlib import Path

root = Path(__file__).resolve().parents[1]
catalogue = json.loads((root / 'res/catalogue.json').read_text())
checks = {c['name']: c for c in catalogue['checks']}
positions = json.loads((root / 'res/positions.json').read_text())
temples = json.loads((root / 'res/temples.json').read_text())
assert set(temples)=={'Forest Temple','Goron Mines','Lakebed Temple','Arbiters Grounds','Snowpeak Ruins','Temple of Time','City in the Sky','Palace of Twilight','Hyrule Castle'}
for name, point in temples.items():
    assert point['stage'].startswith('F_') and 0 <= point['room'] < 64, name
    assert len(point['pos'])==3 and all(math.isfinite(v) and abs(v)<1e7 for v in point['pos']), name
    assert any('Dungeon' in c['categories'] and name in c['categories'] for c in checks.values()), name
for name in ('Temple','TempleAvailable'):
    assert (root / f'res/icons/{name}.bti').exists()
for name, points in positions.items():
    assert name in checks and points, name
    for point in points:
        for p in (point, point.get('overworld', point)):
            assert isinstance(p['stage'], str) and -1 <= p['room'] < 64, name
            assert len(p['pos']) == 3 and all(math.isfinite(v) and abs(v) < 1e7 for v in p['pos']), name
        if 'world_pos' in point:
            assert all(math.isfinite(v) and abs(v) < 1e7 for v in point['world_pos']), name
        if 'Sky Character' in checks[name]['categories'] and point.get('local',True):
            assert f"sky:{point['stage']}:{point['room']}" in checks[name]['aliases'], name
        if 'Dungeon' in checks[name]['categories']:
            assert 'overworld' not in point, name
        if 'overworld' in point:
            assert point['overworld']['stage'].startswith('F_') and point['overworld']['label'], name
for name in ('Coro Bottle', 'Coro Gate Key', 'Coro Lantern'):
    assert 'Npc' in checks[name]['categories']
    assert len(positions[name]) == 1
    point = positions[name][0]
    assert point['stage'] == 'F_SP108' and point['room'] == 4
    assert point.get('local', True) and 'world_pos' in point
    assert abs(point['pos'][0] - (-13659.43359375)) < 0.01
    assert abs(point['pos'][2] - (-14367.0830078125)) < 0.01
bugs=[c for c in checks.values() if 'Golden Bug' in c['categories']]
assert 'Renados Sanctuary Front Door' not in checks
assert all(p['overworld']['label']=="Renado's Sanctuary: Renado's Letter" for p in positions['Renados Letter'] if 'overworld' in p)
for name in ('Sacred Grove Pedestal Master Sword', 'Sacred Grove Pedestal Shadow Crystal'):
    assert positions[name] and all(p['stage']=='F_SP117' and p['room']==1 and 'world_pos' in p for p in positions[name])
for check in checks.values():
    if any(s.startswith('D_SB') for s in check['stages']) and any('Grotto' in a['area'] for a in check['access']):
        assert check['grotto_scenes'], check['name']
assert checks['Herding Goats Reward']['stages']==['F_SP00']
assert positions['Herding Goats Reward']
assert all(p['stage']=='F_SP00' and p['room']==0 and 'world_pos' in p for p in positions['Herding Goats Reward'])
assert len(bugs)==24 and all(c['name'] in positions for c in bugs)
assert all(any(p['stage'].startswith('F_') or 'overworld' in p for p in positions[c['name']]) for c in bugs)
for sex in ('Male','Female'):
    for species in ('Ant','Mantis','Butterfly','Phasmid','Dayfly','Stag Beetle','Ladybug','Grasshopper','Beetle','Pill Bug','Snail','Dragonfly'):
        assert any(c['original_item']==f'{sex} {species}' for c in bugs)
assert all(c['name'] in positions for c in checks.values() if 'Poe' in c['categories'] and 'Dungeon' not in c['categories'])
assert positions['Gerudo Desert Skulltula Grotto Chest'][0]['overworld']['stage']=='F_SP124'
assert positions['Lanayru Field Skulltula Grotto Chest'][0]['overworld']['stage']=='F_SP121'
assert positions['Links Basement Chest'][0]['overworld']==positions['Wooden Sword Chest'][0]['overworld']
for points in positions.values():
    for point in points:
        anchor=point.get('overworld',{})
        if 'Cave' in anchor.get('label','') or 'Grotto' in anchor.get('label',''):
            assert anchor.get('grotto'), anchor
for name in ('Wooden Sword Chest', 'Links Basement Chest'):
    assert positions[name][0]['overworld']['stage'] == 'F_SP103'
for category in ('Golden Wolf', 'Golden Bug', 'Poe', 'Sky Character', 'Shop', 'Npc', 'Chest'):
    assert any(category in checks[name]['categories'] for name in positions), category
for base in ('ItemChest', 'Gift', 'Bug', 'GoldenWolf', 'OwlStatue'):
    for state in ('Available', 'Locked'):
        assert (root / f'res/icons/{base}{state}.bti').exists()
for file in (root / 'res/icons').glob('*.bti'):
    data = file.read_bytes()
    w, h = struct.unpack_from('>HH', data, 2)
    offset = struct.unpack_from('>I', data, 28)[0]
    assert data[0] == 6 and 0 < w <= 64 and 0 < h <= 64, file
    assert len(data) >= offset + ((w+3)//4)*((h+3)//4)*64, file
art=json.loads((root/'res/inventory.json').read_text())
inventory={i['Name'] for i in catalogue['items'] if not i['Name'].endswith('Portal') and ('Bottle' not in i['Name'] or i['Name']=='Empty Bottle')}
assert set(art)==inventory
for name,entry in art.items():
    assert entry['section'] in ('Progression','Collectibles','Keys','Quest items')
    image=(root/'res'/entry['icon']).read_bytes()
    assert image[:8]==b'\x89PNG\r\n\x1a\n'
    assert struct.unpack('>II',image[16:24])==(96,96)
big={v['icon'] for k,v in art.items() if 'Big Key' in k}
small={v['icon'] for k,v in art.items() if ('Small Key' in k or 'Camp Key' in k or 'Gate Key' in k or 'Coro Key' in k)}
assert len(big)==len(small)==1 and big!=small
assert all(v['section']=='Keys' for k,v in art.items() if 'Key' in k)
assert all(v['section']=='Collectibles' for k,v in art.items() if k.startswith(('Male ','Female ')) or 'Bottle' in k)
print(f'Validated {len(positions)} check positions, {len(art)} inventory entries and packaged icon textures.')

assert [name for name in art if 'Bottle' in name] == ['Empty Bottle']

expected_icons = {'Progressive Mirror Shard':'mirror_shard_4.bti','Gale Boomerang':'tt_boomerang_05.bti','Bomb Bag':'st_bompoach_lv1.bti','Aurus Memo':'im_kakioki_48.bti','Progressive Sky Book':'o_gd_komonsho.bti','Renados Letter':'st_len_letter.bti','Invoice':'st_bill.bti','Progressive Wallet':'ni_saifu2_48.bti','Ordon Cheese':'im_cheese_48.bti','Ordon Pumpkin':'im_pumpkin_48.bti','Progressive Hidden Skill':'ni_item_icon_makimono.bti','Shadow Crystal':'shadow_crystal.bti','Progressive Fused Shadow':'fused_shadow_3.bti'}
for name, resource in expected_icons.items():
    assert art[name]['resource'] == resource, name
    assert not art[name]['fallback'], name

# Every separate boss/miniboss check stage has one parent-dungeon anchor.
import re, math
arenas=json.loads((root/'res/arena_entrances.json').read_text())
expected={stage for check in checks.values() for stage in check['stages'] if re.fullmatch(r'D_MN\d+[A-Z]',stage)}
assert {a['arena'] for a in arenas}==expected
assert len(arenas)==len(expected)
for arena in arenas:
    assert arena['stage']==arena['arena'][:-1]
    assert 0 <= arena['room'] < 64 and all(math.isfinite(v) for v in arena['pos'])
    assert set(arena['checks'])=={c['name'] for c in checks.values() if arena['arena'] in c['stages']}
darkhammer=next(a for a in arenas if a['arena']=='D_MN11B')
blizzeta=next(a for a in arenas if a['arena']=='D_MN11A')
assert set(darkhammer['checks'])=={'Snowpeak Ruins Ball and Chain','Snowpeak Ruins Chest After Darkhammer'}
assert darkhammer['room']==blizzeta['room']==4
assert darkhammer['pos'][1] < blizzeta['pos'][1]  # Same XZ, different floors.
print(f'Validated {len(arenas)} boss/miniboss entrance groups.')

# All seed-shuffled rupees need a local anchor, including crates and rocks.
rupees=[c for c in checks.values() if any(cat in c['categories'] for cat in ('Rupee - Freestanding','Rupee - Hidden'))]
assert len(rupees)==88
for c in rupees:
    assert c['name'] in positions, c['name']
    assert all(p.get('local',True) and p['stage'] in c['stages'] and p['room']>=0 for p in positions[c['name']]),c['name']
# Coro's shared vanilla flag is split by randomizer object patches.
coro_rupees=[positions[f'Faron Woods Coro Boulder Rupee {i}'] for i in range(1,5)]
assert all(len(points)==1 for points in coro_rupees)
assert len({tuple(points[0]['pos']) for points in coro_rupees})==4
assert positions['Kakariko Village Hot Spring Ledge Box Rupee'][0]['pos'][1]>2700
assert positions['Death Mountain Volcano Pipe Ledge Rock Rupee'][0]['room']==3
for name in ('HiddenRupeeAvailable','HiddenRupeeLocked'):
    assert (root/f'res/icons/{name}.bti').exists()
print('Validated all 88 rupee checks, including patched pickup flags and container sources.')

# Every selectable check has an offline collection guide, including optional rupees.
guides=json.loads((root/'res/check_guides.json').read_text(encoding='utf-8'))
assert guides['version']==1 and set(guides['checks'])==set(checks)
assert len(guides['upstream_revision'])==40
for name, guide in guides['checks'].items():
    assert 1 <= len(guide['steps']) <= 8, name
    assert guide['sources'] and all(s in guides['sources'] for s in guide['sources']), name
    for step in guide['steps']:
        assert isinstance(step,str) and 10 <= len(step) <= 1600, name
        assert not any(token in step for token in ('**','Requires:','*Source:','<div','https://')), name
for source in guides['sources'].values():
    assert source['url'].startswith('https://') and source['label']
assert sum(g.get('kind')=='hint_sign' for g in guides['checks'].values())==34
assert sum(g.get('kind')=='twilit_insect' for g in guides['checks'].values())==48
assert 'Copyright (c) 2026 Travis Gesslein' in (root/'res/CHECK_GUIDE_SOURCES.txt').read_text(encoding='utf-8')
print(f"Validated {len(guides['checks'])} attributed offline collection guides.")

# Regenerated cave rules must match their per-check copies and native puzzle IDs.
lake_area=next(a for a in catalogue['areas'] if a['Name']=='Lake Hylia Long Cave')
torch_names={'Lake Lantern Cave Seventh Chest':14,'Lake Lantern Cave End Lantern Chest':3}
for name,requirement in lake_area['Locations'].items():
    assert checks[name]['access']==[{'area':lake_area['Name'],'requirement':requirement}],name
    assert ('Lantern' in requirement)==(name in torch_names),name
for name,flag in torch_names.items():
    assert any(f['kind']=='chest' and f['flag']==flag for f in checks[name]['flags']),name
    assert 'both torches' in ' '.join(guides['checks'][name]['steps']),name
assert 'not required' in ' '.join(guides['checks']['Lake Lantern Cave Sixth Chest']['steps'])
assert 'not required' in ' '.join(guides['checks']['Lake Lantern Cave Fourteenth Chest']['steps'])
print('Validated Lake cave ordinary chests and both torch-spawn exceptions.')
