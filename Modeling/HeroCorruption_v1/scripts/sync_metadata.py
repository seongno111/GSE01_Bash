"""Store stable part indices and editable bow morph tracks in every delivery."""
import bpy
import json
import struct
from pathlib import Path

P=Path(__file__).resolve().parents[1]
catalog=json.loads((P/'corruption_catalog.json').read_text('utf-8'))
indices={part['mesh']:i for i,part in enumerate(catalog['parts'])}
for part in catalog['parts']:part['part_index']=indices[part['mesh']]
catalog['shared_body_indices']=[indices[n] for n in catalog['shared_body']]
catalog['free_arm_indices']={side:indices[name] for side,name in catalog['free_arms'].items()}
catalog['optional_lamp_indices']=[indices[n] for n in catalog['optional_lamp']]
for module in catalog['modules']:
    module['part_indices']=[indices[n] for n in module['parts']]
    module['free_arm_index']=indices[module['free_arm']]
for file in P.rglob('*.glb'):
    raw=file.read_bytes();length=struct.unpack_from('<I',raw,12)[0]
    doc=json.loads(raw[20:20+length]);binary=raw[28+length:]
    for node in doc['nodes']:
        name=node.get('name')
        if name in indices:node.setdefault('extras',{}).update(part_index=indices[name],asset_id=catalog['asset_id'])
        if name=='CORR_AMMO_02_BoneArrow':
            node['weights']=[1.0]
            doc['meshes'][node['mesh']]['weights']=[1.0]
        if name in ('CORR_WEAPON_02_Bow_Body','CORR_WEAPON_02_Bow_String'):
            node['weights']=[0.0]
            doc['meshes'][node['mesh']]['weights']=[0.0]
    header=json.dumps(doc,ensure_ascii=False,separators=(',',':')).encode()
    header+=b' '*(-len(header)%4)
    file.write_bytes(struct.pack('<4sIII4s',b'glTF',2,28+len(header)+len(binary),len(header),b'JSON')+
                     header+struct.pack('<I4s',len(binary),b'BIN\0')+binary)
(P/'corruption_catalog.json').write_text(json.dumps(catalog,ensure_ascii=False,indent=2),encoding='utf-8')

bpy.ops.wm.open_mainfile(filepath=str(P/'hero_corruption_v1.blend'))
for name,index in indices.items():bpy.data.objects[name]['part_index']=index
source=P.parent/'HeroWeaponAnimations_v1/hero_weapon_animations_v1.blend'
with bpy.data.libraries.load(str(source),link=False) as (src,dst):
    dst.actions=[n for n in src.actions if n.startswith('HERO_Bow_') and '__' in n]
new_actions={action.name:action for action in dst.actions}
bow=next(m for m in catalog['modules'] if m['weapon_kind']=='bow')
for original,name in bow['source_node_map'].items():
    obj=bpy.data.objects[name]
    keys=obj.data.shape_keys
    if keys is None:continue
    keys.animation_data_clear();keys.animation_data_create()
    for clip in [s for s in catalog['weapon_clips'] if s['weapon_kind']=='bow']:
        prefix=clip['name']+'__'
        action=next((a for n,a in new_actions.items() if n.startswith(prefix)
                     and (original in n or original.startswith('AMMO_') and 'ArrowVisibility' in n)),None)
        if action:
            track=keys.animation_data.nla_tracks.new();track.name=clip['name'];track.mute=True
            strip=track.strips.new(clip['name'],1,action)
            strip.action_slot=action.slots[0]
            strip.extrapolation='NOTHING'
    for key in keys.key_blocks:key.value=1 if key.name=='Hide' else 0
bpy.ops.wm.save_as_mainfile(filepath=str(P/'hero_corruption_v1.blend'))
print('Synchronized',len(indices),'part indices and native bow morph tracks')
