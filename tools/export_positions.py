"""Read-only developer extraction of map coordinates from the user's GameCube disc."""
from pathlib import Path
import struct,json,sys
ROOT=Path(__file__).resolve().parents[1]
def u32(b,o):return struct.unpack_from('>I',b,o)[0]
def yaz(b):
 if b[:4]!=b'Yaz0':return b
 size=u32(b,4);out=bytearray();i=16
 while len(out)<size:
  code=b[i];i+=1
  for bit in range(7,-1,-1):
   if len(out)>=size:break
   if code&(1<<bit):out.append(b[i]);i+=1
   else:
    a,c=b[i:i+2];i+=2;dist=((a&15)<<8|c)+1;n=a>>4
    if not n:n=b[i]+18;i+=1
    else:n+=2
    for _ in range(n):out.append(out[-dist])
 return bytes(out)
def archive(b,full_paths=False):
 b=yaz(b)
 if b[:4]!=b'RARC':return {}
 base=u32(b,8);data=base+u32(b,12);entries=base+u32(b,base+12);strings=base+u32(b,base+20);out={}
 prefixes={}
 if full_paths:
  nodes=base+u32(b,base+4)
  def walk(node,prefix):
   q=nodes+node*16;first=u32(b,q+12);count=struct.unpack_from('>H',b,q+10)[0]
   for i in range(first,first+count):
    p=entries+i*20;flags=u32(b,p+4);no=strings+(flags&0xffffff);name=b[no:b.find(b'\0',no)].decode()
    if name in ('.','..'):continue
    if flags>>24&2:walk(u32(b,p+8),prefix+name+'/')
    else:prefixes[i]=prefix
  walk(0,'')
 for i in range(u32(b,base+8)):
  p=entries+i*20;flags=u32(b,p+4);no=strings+(flags&0xffffff);name=b[no:b.find(b'\0',no)].decode(errors='replace')
  if flags>>24&2:continue
  off=data+u32(b,p+8);out[prefixes.get(i,'')+name]=b[off:off+u32(b,p+12)]
 return out
def disc_files(f):
 f.seek(0x424);offset,size=struct.unpack('>II',f.read(8));f.seek(offset);b=f.read(size)
 count=u32(b,8);strings=count*12;stack=[('',count)];paths={}
 for i in range(1,count):
  while i>=stack[-1][1]:stack.pop()
  p=i*12;word=u32(b,p);start=strings+(word&0xffffff)
  name=b[start:b.find(b'\0',start)].decode();path=stack[-1][0]+'/'+name
  if word>>24:stack.append((path,u32(b,p+8)))
  else:paths[path]=(u32(b,p+4),u32(b,p+8))
 return paths

if __name__=='__main__':
 iso=Path(sys.argv[1])
 records={};transforms={}
 with iso.open('rb') as f:
  paths=disc_files(f)
  for path,(off,size) in paths.items():
   if path.endswith('/FieldMap/Field0.arc'):
    f.seek(off)
    for name,b in archive(f.read(size),True).items():
     if not name.endswith('.dzs'):continue
     b=yaz(b)
     for i in range(u32(b,0)):
      p=4+i*12
      if b[p:p+4]==b'FILI':transforms[name]=b[u32(b,p+8):u32(b,p+8)+32].hex()
   if not ('/Stage/' in path or path.endswith('/fmapres.arc')):continue
   f.seek(off);files=archive(f.read(size))
   if path.endswith('/fmapres.arc'):
    print('map files',[(k,len(v)) for k,v in files.items() if k.endswith('.dat')]);continue
   for name,b in files.items():
    if not name.endswith(('.dzs','.dzr')):continue
    chunks={}
    for i in range(u32(b,0)):
     p=4+12*i;tag=b[p:p+4].decode();n=u32(b,p+4);o=u32(b,p+8)
     if tag in ('TRES','ACTR','SCOB','PLYR','SCLS','FILI') or tag.startswith(('ACT','TRE','SCO')):
      step=13 if tag=='SCLS' else 36 if tag.startswith('SCO') else 32
      chunks[tag]=[b[o+j*step:o+(j+1)*step].hex() for j in range(n)]
    records[path]=chunks
 (ROOT/'build/stage-records.json').write_text(json.dumps(records))
 (ROOT/'build/room-transforms.json').write_text(json.dumps(transforms))
 print('stage archives',len(records),'map room transforms',len(transforms))
