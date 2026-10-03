"""Derive boss/miniboss door anchors from extracted SCLS exits and PLYR spawns.
Run export_positions.py on your own disc first. No disc data is downloaded.
"""
from pathlib import Path
import json, re, struct, sys
root = Path(__file__).resolve().parents[1]
records = json.loads(Path(sys.argv[1] if len(sys.argv)>1 else root/'build/stage-records.json').read_text())
checks = json.loads((root/'res/catalogue.json').read_text())['checks']
rooms = {}
for path, chunks in records.items():
    stage = path.split('/')[-2]
    match = re.search(r'/R(\d+)_',path)
    room = int(match[1]) if match else -1
    rooms[(stage,room)] = chunks

def exits(chunks):
    for value in chunks.get('SCLS',[]):
        b=bytes.fromhex(value)
        yield b[:8].split(b'\0')[0].decode(), b[9], b[8]

# Dangoro's reward chest is beyond his arena in the parent dungeon.
rewards={arena:[c['name'] for c in checks if arena in c['stages']]
         for arena in {s for c in checks for s in c['stages'] if re.fullmatch(r'D_MN\d+[A-Z]',s)}}
rewards['D_MN04B']=['Goron Mines Dangoro Chest']
arenas=sorted(rewards)
result=[]
for arena in arenas:
    parent=arena[:-1]
    # Room exits are the actual door/tunnel approaches. A stage restart can
    # be in another part of the room (Deku Toad used the high restart ledge).
    # Keep every distinct approach, including Lakebed's underwater tunnel.
    room_candidates=[]
    for (stage,room),chunks in rooms.items():
        if stage==arena and room>=0: room_candidates.extend(exits(chunks))
    anchors=[]
    for candidates in (room_candidates, list(exits(rooms.get((arena,-1),{})))):
        for stage,room,spawn in candidates:
            if stage!=parent or room>=64: continue
            chunks=rooms.get((stage,room),{})
            if not any(dst==arena for dst,_,_ in exits(chunks)): continue
            for value in chunks.get('PLYR',[]):
                b=bytes.fromhex(value)
                if int.from_bytes(b[28:30],'big')!=spawn: continue
                entry={'stage':stage,'room':room,'pos':list(struct.unpack_from('>fff',b,12)),
                       'arena':arena,'checks':rewards[arena],
                       'source':'reciprocal room SCLS and return PLYR spawn'}
                if not any(e['room']==room and e['pos']==entry['pos'] for e in anchors):anchors.append(entry)
                break
        if anchors: break
    anchor=anchors[0] if anchors else None
    if not anchor:
        # Zant's multi-stage fight returns elsewhere. Use the actual boss door
        # actor in the room with the forward SCLS, never the outdoor warp.
        for (stage,room),chunks in rooms.items():
            if stage!=parent or room<0 or not any(dst==arena for dst,_,_ in exits(chunks)): continue
            doors=[bytes.fromhex(h) for h in chunks.get('Door',[]) if b'Bdoor' in bytes.fromhex(h)[:8]]
            if len(doors)==1:
                anchor={'stage':stage,'room':room,'pos':list(struct.unpack_from('>fff',doors[0],12)),
                        'arena':arena,'checks':rewards[arena],
                        'source':'boss Door actor in forward SCLS room'}
                break
    if not anchor: raise RuntimeError(f'Missing reciprocal entrance for {arena}')
    result.extend(anchors or [anchor])
(root/'res/arena_entrances.json').write_text(json.dumps(result,indent=2)+'\n')
print(f'Exported {len(result)} boss/miniboss entrances.')
