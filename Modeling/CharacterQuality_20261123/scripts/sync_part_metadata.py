"""Update modular part pivots in the source catalog and GLB node extras."""
import json
import struct
from pathlib import Path

import bpy

P=Path(__file__).resolve().parents[1]
folder=P/'staged/LowPolyKit_v3'
path=folder/'lowpoly_kit_v3.blend'
bpy.ops.wm.open_mainfile(filepath=str(path))
catalog=json.loads((folder/'asset_catalog.json').read_text('utf-8'))
rig=bpy.data.objects['RIG_NPC_Shared']
by_name={}
for part in catalog['parts']:
    obj=bpy.data.objects[part['mesh']]
    slot=part['slot']
    if slot=='accessory_hand':
        bone='weapon_l' if 'HunterBow' in obj.name else 'weapon_r'
    elif slot=='accessory_back':bone='chest'
    elif slot in ('accessory_waist','pelvis','skirt','torso'):bone='hips'
    elif slot in ('hair','headgear'):bone='head'
    else:bone=slot
    point=rig.data.bones[bone].head_local
    part['joint_pivot_blender_z_up']=list(point)
    part['joint_pivot_gltf_y_up']=[point.x,point.z,-point.y]
    obj['joint_pivot']=list(point)
    obj['bind_revision']='2026-11-23'
    by_name[obj.name]=dict(joint_pivot=list(point),bind_revision='2026-11-23')
catalog['bind_revision']='2026-11-23'
catalog['body_to_head_ratio']=1.84/.365
catalog['rigging_status']='Five-head revised bind with normalized skin and baked compatible keyframe actions.'
(folder/'asset_catalog.json').write_text(json.dumps(catalog,ensure_ascii=False,indent=2),encoding='utf-8')
for file in (folder/'npc').glob('*.glb'):
    raw=file.read_bytes();length=struct.unpack_from('<I',raw,12)[0]
    doc=json.loads(raw[20:20+length]);binary=raw[28+length:]
    for node in doc['nodes']:
        if node.get('name') in by_name:node.setdefault('extras',{}).update(by_name[node['name']])
    header=json.dumps(doc,ensure_ascii=False,separators=(',',':')).encode()
    header+=b' '*(-len(header)%4)
    file.write_bytes(struct.pack('<4sIII4s',b'glTF',2,28+len(header)+len(binary),len(header),b'JSON')+
                     header+struct.pack('<I4s',len(binary),b'BIN\0')+binary)
bpy.ops.wm.save_as_mainfile(filepath=str(path))
print('Synchronized modular part pivots and bind revisions')
