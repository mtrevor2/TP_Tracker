from dependency_paths import DUSKLIGHT, RANDOMIZER
"""Derive actor coordinates and interior exit anchors; never invent exact positions."""
from pathlib import Path
import json,struct,re,math,yaml
root=Path(__file__).resolve().parents[1]
records=json.loads((root/'build/stage-records.json').read_text(encoding='utf-8'));cat=json.loads((root/'res/catalogue.json').read_text(encoding='utf-8'))
transforms=json.loads((root/'build/room-transforms.json').read_text())
def actor(h):
 b=bytes.fromhex(h)
 return {'actor':b[:8].split(b'\0')[0].decode(errors='replace'),'param':int.from_bytes(b[8:12],'big'),'pos':list(struct.unpack_from('>fff',b,12)),'spawn':int.from_bytes(b[28:30],'big'),'angle_x':int.from_bytes(b[24:26],'big')}
rooms={};actors={};spawns={};exits={}
for path,chunks in records.items():
 stage=path.split('/')[-2];m=re.search(r'/R(\d+)_',path);room=int(m[1]) if m else -1
 key=(stage,room);rooms[key]=chunks
 for tag,values in chunks.items():
  if tag=='SCLS':
   exits[key]=[(bytes.fromhex(h)[:8].split(b'\0')[0].decode(),bytes.fromhex(h)[9],bytes.fromhex(h)[8]) for h in values]
  elif tag=='PLYR':spawns[key]=[actor(h) for h in values]
  elif tag!='FILI' and not (room==-1 and tag.startswith('TRE')):actors.setdefault(stage,[]).extend(dict(actor(h),room=room) for h in values)
def corrected(stage,room,pos):
 fili=transforms.get(f'{stage}/room{room}.dzs'.lower())
 if not fili:return pos
 b=bytes.fromhex(fili);dx,dz=struct.unpack_from('>ff',b,20);angle=struct.unpack_from('>h',b,28)[0]*math.pi/32768
 x,y,z=pos;s,c=math.sin(angle),math.cos(angle)
 return [c*x+s*z+dx,y,c*z-s*x+dz]
area_stages={}
for c in cat['checks']:
 for a in c['access']:area_stages.setdefault(a['area'],set()).update(c['stages'])
bugs=dict(zip(['kab_o','I_Cho','I_Kuw','I_Nan','I_Dan','I_Kam','I_Ten','I_Ari','I_Kag','I_Tom','I_Bat','I_Kat'],['Beetle','Butterfly','Stag Beetle','Phasmid','Pill Bug','Mantis','Ladybug','Ant','Dayfly','Dragonfly','Grasshopper','Snail']))
def exit_anchor(stage,room,seen=()):
 if (stage,room) in seen:return None
 for dst,r,spawn in exits.get((stage,room),[])+exits.get((stage,-1),[]):
  if dst.startswith('F_'):
   found=[a for a in spawns.get((dst,r),[]) if a['spawn']==spawn]
   if found:return {'stage':dst,'room':r,'pos':corrected(dst,r,found[0]['pos']),'anchor':'interior entrance'}
  else:
   found=exit_anchor(dst,r,seen+((stage,room),))
   if found:return found
 return None
repo=DUSKLIGHT
stage_names=re.findall(r'"([A-Z0-9_]+)"',(RANDOMIZER/'src/stages.cpp').read_text().split('};')[0])
entrances=yaml.safe_load((RANDOMIZER/'generator/data/entrance_shuffle_data.yaml').read_text())
def exterior_entrance(check,stage=None,room=None):
 areas={a['area'] for a in check['access']};exact=[];fallback=[]
 for entry in entrances:
  if entry['Type'] not in ('Interior','Cave','Grotto'):continue
  f=entry.get('Forward',{});r=entry.get('Return',{})
  if not r or not stage_names[r['Stage']].startswith('F_'):continue
  names={f.get('Connection','').split(' -> ')[-1],f.get('Alias','').split(' -> ')[-1],entry.get('Entrance Couple Tag','')}
  if areas&names:exact.append(entry)
  elif stage is not None and stage_names[f['Stage']]==stage and f['Room']==room:fallback.append(entry)
 choices=exact or fallback
 # Shared grotto stages must be resolved by the logical area, never first-exit order.
 if not choices:return None
 # Multiple real doors to the same known house/cave use the first exterior
 # entrance. Shared grotto rooms still require an exact logical-area match.
 if not exact and len(choices)>1 and any(e['Type']!='Interior' for e in choices):return None
 entry=choices[0];r=entry['Return'];dst=stage_names[r['Stage']];room=r['Room']
 spawn=[a for a in spawns.get((dst,room),[]) if a['spawn']==r['Spawn']]
 if not spawn:return None
 label=entry.get('Entrance Couple Tag') or entry['Forward']['Connection'].split(' -> ')[-1]
 if check['name']=='Renados Letter':label="Renado's Sanctuary: Renado's Letter"
 return {'stage':dst,'room':room,'pos':corrected(dst,room,spawn[0]['pos']),
         'anchor':'exterior entrance','label':label,
         'grotto':entry['Type'] in ('Cave','Grotto')}
item_macros=dict(re.findall(r'#define\s+(ITEM_CHECK_\w+)\s+"([^"]+)"',(repo/'sdk/include/mods/items.h').read_text(encoding='utf-8')))
lookup={name:item_macros.get(code,code.strip('"')) for name,code in re.findall(r'\{"([^"]+)",\s*(ITEM_CHECK_\w+|"[^"]+")\}',(RANDOMIZER/'src/tools.cpp').read_text(encoding='utf-8'))}
object_names={}
for name,profile in re.findall(r'OBJNAME\("([^"]+)"\s*,\s*fpcNm_(\w+)_e',(repo/'src/d/d_stage.cpp').read_text(encoding='utf-8')):object_names.setdefault(profile,set()).add(name)
npc_actors={}
for file in (repo/'src/d/actor').glob('*.cpp'):
 text=file.read_text(encoding='utf-8',errors='replace')
 profiles=re.findall(r'DUSK_PROFILE[^\n]*g_profile_(\w+)\s*=',text)
 tags=set(re.findall(r'DUSK_(?:ITEM_CHECK|GIVE_TAG)\("([^"]+)"',text))
 for check,tag in lookup.items():
  if tag in tags:
   for profile in profiles:npc_actors.setdefault(check,set()).update(object_names.get(profile,set()))
# Coro commits ITEM_CHECK_CORO_* dynamically in d_a_npc_kkri.cpp,
# so the literal DUSK_ITEM_CHECK tag scan above cannot discover his rewards.
for check in ('Coro Bottle','Coro Gate Key','Coro Lantern'):
 npc_actors.setdefault(check,set()).update(object_names['NPC_KKRI'])
# Plumm commits his reward dynamically, outside the static tag scan.
npc_actors.setdefault('Plumm Fruit Balloon Minigame',set()).update(object_names['MYNA2'])
for check in ('Sacred Grove Pedestal Master Sword','Sacred Grove Pedestal Shadow Crystal'):
 npc_actors.setdefault(check,set()).update(object_names['Obj_MasterSword'])
# Item flags may be reassigned by the randomizer (notably Ordon and the
# Coro boulder). Match the original actor before using its patched flag,
# otherwise several independent pickups incorrectly collapse onto one point.
patches=yaml.safe_load((RANDOMIZER/'generator/data/object_patches.yaml').read_text())
for stage,room_patches in patches.items():
 for room,changes in room_patches.items():
  room=-1 if room=='Stage' else int(room)
  for change in changes:
   if change['name']!='item':continue
   pos=[change['position'][axis] for axis in ('x','y','z')]
   if change['action']=='add':
    actors.setdefault(stage,[]).append(dict(actor='item',param=change['parameters'],pos=pos,room=room))
    continue
   for a in actors.get(stage,[]):
    if a['actor']!='item' or a['room']!=room or a['param']!=change['parameters']:continue
    if any(abs(x-y)>0.05 for x,y in zip(a['pos'],pos)):continue
    if change['action']=='delete':a['actor']='deleted'
    else:
     patch=change['patch']
     a['param']=patch.get('parameters',a['param'])
     if 'position' in patch:a['pos']=[patch['position'].get(axis,a['pos'][i]) for i,axis in enumerate(('x','y','z'))]

# The bottle is a scripted catch, not a placed item. Use the center of the
# native catch region from d_a_mg_rod.cpp, rather than a guessed NPC/door point.
rod_source=(repo/'src/d/actor/d_a_mg_rod.cpp').read_text(encoding='utf-8')
bottle_match=re.search(r'cXyz bin_pos\(\s*([-\d.]+)f,\s*([-\d.]+)f,\s*([-\d.]+)f\)',rod_source)
if not bottle_match:raise RuntimeError('Fishing bottle catch-region coordinate missing')
bottle_pos=[float(v) for v in bottle_match.groups()]

out={};counts={}
for c in cat['checks']:
 candidates=[];stages=set(c['stages'])
 if not stages:
  for a in c['access']:
   ss=area_stages.get(a['area'],set())
   if len(ss)==1:stages.update(ss)
 for stage in stages:
  for a in actors.get(stage,[]):
   name=a['actor'];p=a['param'];match=False
   for flag in c['flags']:
    if flag['kind']=='item' and name.startswith('carry') and 'Freestanding Item' in c['categories']:match|=(a['angle_x']>>8)==flag['flag']
    if flag['kind']=='item' and name in ('stone','stoneB') and 'Freestanding Item' in c['categories']:match|=((p>>16)&255)==flag['flag']
    if flag['kind']=='item' and name in ('item','htPiece') and 'Freestanding Item' in c['categories']:match|=((p>>8)&255)==flag['flag']
    if flag['kind']=='chest' and name.startswith('tbox') and name!='tbox_sw':match|=(((p>>16)&255) if name.startswith('tboxEL') else ((p>>6)&63))==flag['flag']
    if flag['kind']=='switch' and name=='E_hp' and 'Poe' in c['categories']:match|=((p>>8)&255)==flag['flag']
   if name in bugs and 'Golden Bug' in c['categories']:
    location=p&3 if name=='I_Kag' else p&15
    if name=='I_Kag' and location==3:location=0
    match = c['original_item']==('Female ' if (p>>4)&1 else 'Male ')+bugs[name] and location==0
   if name=='GWolf' and 'Golden Wolf' in c['categories']:
    mode=((p>>8)&255)-1;flags=[0x3c10,0x3c08,0x3c04,0x3c02,0x3c01,0x3d80,0x3d40]
    match=(p&255) not in (1,2) and 0<=mode<len(flags) and f'golden_wolf:{flags[mode]}' in c['aliases']
   if name=='CstaF' and 'Sky Character' in c['categories']:match=f'sky:{stage}:{a["room"]}' in c['aliases']
   if name in npc_actors.get(c['name'],set()):match=True
   for alias in c['aliases']:
    parts=alias.split(':')
    if len(parts)==4 and parts[0]=='shop' and stage==parts[1] and a['room']==int(parts[2]) and name in ('ShopItm','TGSPITM') and (p&255)==int(parts[3]):match=True
   if match:candidates.append(dict(stage=stage,room=a['room'],pos=a['pos'],anchor='actor'))
 if c['name']=='Fishing Hole Bottle':
  candidates.append(dict(stage='F_SP127',room=0,pos=bottle_pos,anchor='native fishing catch region'))
 # Deduplicate actors repeated in time/event layers.
 unique=[]
 for a in candidates:
  if not any(a==b for b in unique):unique.append(a)
 if not unique:
  exterior=exterior_entrance(c)
  if exterior and 'Dungeon' not in c['categories']:
   out[c['name']]=[{'stage':next(iter(stages),'interior'),'room':-1,'pos':[0,0,0],'local':False,'overworld':exterior}]
  continue
 # Interior markers represent a reachable external exit, not the indoor coordinate.
 for a in unique:
  if a['stage'].startswith('F_'):
   a['world_pos']=corrected(a['stage'],a['room'],a['pos'])
  elif 'Dungeon' not in c['categories']:
   exterior=exterior_entrance(c,a['stage'],a['room'])
   if exterior:a['overworld']=exterior
 out[c['name']]=unique
 for a in unique:counts[a['anchor']]=counts.get(a['anchor'],0)+1
# Link's basement shares the house entrance; room 7 has no external SCLS.
if 'Wooden Sword Chest' in out and 'Links Basement Chest' in out:
 for point in out['Links Basement Chest']:
  point['overworld']=out['Wooden Sword Chest'][0].get('overworld')
(root/'res/positions.json').write_text(json.dumps(out,indent=2)+'\n')
# One overworld anchor per dungeon, derived from its return entrance spawn.
# Goron Mines exits through the sumo hall, so project that interior to its
# real exterior exit using the same room transform as other interior markers.
dungeon_names=('Forest Temple','Goron Mines','Lakebed Temple','Arbiters Grounds',
               'Snowpeak Ruins','Temple of Time','City in the Sky','Palace of Twilight','Hyrule Castle')
temples={}
for entry in entrances:
 if entry['Type']!='Dungeon':continue
 destination=entry['Forward'].get('Alias',entry['Forward']['Connection']).split(' -> ')[-1]
 name=next((n for n in dungeon_names if destination.startswith(n)),None)
 if not name or name in temples:continue
 r=entry['Return'];stage=stage_names[r['Stage']];room=r['Room']
 if stage.startswith('F_'):
  spawn=next((a for a in spawns.get((stage,room),[]) if a['spawn']==r['Spawn']),None)
  if not spawn:raise RuntimeError(f'Missing dungeon return spawn: {name} {stage}/{room}/{r["Spawn"]}')
  point={'stage':stage,'room':room,'pos':corrected(stage,room,spawn['pos'])}
 else:
  point=exit_anchor(stage,room)
  if not point:raise RuntimeError(f'Missing exterior dungeon anchor: {name}')
 temples[name]=point
assert set(temples)==set(dungeon_names)
(root/'res/temples.json').write_text(json.dumps(temples,indent=2)+'\n')
print('Checks with actor coordinates:',len(out),'Interior projected:',sum(any('overworld'in a for a in v) for v in out.values()))
print('Link house:',{k:v for k,v in out.items() if k in ['Wooden Sword Chest','Links Basement Chest']})
