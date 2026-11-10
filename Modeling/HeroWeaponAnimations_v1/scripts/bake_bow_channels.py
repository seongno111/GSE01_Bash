"""Write exact 60fps authored morph keys into GLB after NLA export.

Blender 5.2's sampled NLA exporter flattened Key-datablock tracks in this scene.
Bone/skin buffers stay byte-identical; only the three bow morph samplers change.
"""
import json,struct
from pathlib import Path
P=Path(__file__).resolve().parents[1]
path=P/'hero_bow_actions.glb'
b=path.read_bytes();jlen=struct.unpack_from('<I',b,12)[0]
d=json.loads(b[20:20+jlen]);binlen=struct.unpack_from('<I',b,20+jlen)[0]
buf=bytearray(b[28+jlen:28+jlen+binlen])
specs={s['name']:s for s in json.loads((P/'authoring_keyposes.json').read_text('utf-8'))}
def accessor(values):
 while len(buf)%4:buf.append(0)
 offset=len(buf);buf.extend(struct.pack('<'+'f'*len(values),*values))
 vi=len(d['bufferViews']);d['bufferViews'].append(dict(buffer=0,byteOffset=offset,byteLength=len(values)*4))
 ai=len(d['accessors']);d['accessors'].append(dict(bufferView=vi,componentType=5126,count=len(values),type='SCALAR',min=[min(values)],max=[max(values)]))
 return ai
def draw(s,t):
 for i in range(len(s['keyposes'])-1):
  ta,a=s['keyposes'][i];tb,z=s['keyposes'][i+1]
  if t<=tb+1e-6:
   u=max(0,min(1,(t-ta)/(tb-ta)));u=u*u*(3-2*u);return a['draw']+(z['draw']-a['draw'])*u
 return s['keyposes'][-1][1]['draw']
for anim in d['animations']:
 n=anim['name'];s=specs[n];ts=[i/60 for i in range(s['end_frame'])]
 for channel in anim['channels']:
  if channel['target']['path']!='weights':continue
  name=d['nodes'][channel['target']['node']]['name']
  if name.startswith('AMMO_'):
   values=[]
   for t in ts:
    visible=(n in ('HERO_Bow_Draw','HERO_Bow_Aim') or (n=='HERO_Bow_Nock' and t>=.25) or (n=='HERO_Bow_Release' and t<.1) or (n=='HERO_Bow_Lower' and t<.5))
    values.append(0 if visible else 1)
   interpolation='STEP'
  else:values=[draw(s,t) for t in ts];interpolation='LINEAR'
  sampler=anim['samplers'][channel['sampler']];sampler.update(input=accessor(ts),output=accessor(values),interpolation=interpolation)
d['buffers'][0]['byteLength']=len(buf)
d.setdefault('extras',{})['bow_morph_bake']='Exact authored 60fps Draw and Hide keys; synchronized to original hero action times'
js=json.dumps(d,separators=(',',':'),ensure_ascii=False).encode('utf-8');js+=b' '*((-len(js))%4)
buf.extend(b'\0'*((-len(buf))%4))
out=struct.pack('<4sII',b'glTF',2,12+8+len(js)+8+len(buf))+struct.pack('<I4s',len(js),b'JSON')+js+struct.pack('<I4s',len(buf),b'BIN\0')+buf
tmp=path.with_suffix('.glb.tmp');tmp.write_bytes(out);tmp.replace(path)
print('Baked bow Draw / string / arrow channels')
