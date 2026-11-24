"""Check the actual imported bow morph values at the original animation times."""
import bpy
import json
import struct
from pathlib import Path

P=Path(__file__).resolve().parents[1]
catalog=json.loads((P/'corruption_catalog.json').read_text('utf-8'))
module=next(m for m in catalog['modules'] if m['weapon_kind']=='bow')
raw=(P/module['assembled_file']).read_bytes()
length=struct.unpack_from('<I',raw,12)[0]
document=json.loads(raw[20:20+length])
bpy.ops.wm.read_factory_settings(use_empty=True)
scene=bpy.context.scene;scene.render.fps=60
bpy.ops.import_scene.gltf(filepath=str(P/module['assembled_file']))
def shaped(name,key):
    obj=bpy.data.objects[name]
    node=next(n for n in document['nodes'] if n['name']==name)
    mesh_name=document['meshes'][node['mesh']]['name']
    return next(o for o in scene.objects
                if o.type=='MESH' and o.data.name==mesh_name and o.data.shape_keys
                and key in o.data.shape_keys.key_blocks)

body=shaped(module['source_node_map']['WPN_Bow_00_ThornRecurve__Body'],'Draw')
string=shaped(module['source_node_map']['WPN_Bow_00_ThornRecurve__String'],'Draw')
arrow=shaped(module['source_node_map']['AMMO_Arrow_00_Iron__Body'],'Hide')


def sample(name,time):
    for obj in scene.objects:
        blocks=[obj]+([obj.data.shape_keys] if obj.type=='MESH' and obj.data.shape_keys else [])
        for block in blocks:
            ad=block.animation_data
            if not ad:continue
            ad.action=None
            for track in ad.nla_tracks:
                track.mute=True
                if track.name==name:
                    strip=track.strips[0];ad.action=strip.action;ad.action_slot=strip.action_slot
    frame=time*60;scene.frame_set(int(frame),subframe=frame-int(frame))
    return dict(body_draw=body.data.shape_keys.key_blocks['Draw'].value,
        string_draw=string.data.shape_keys.key_blocks['Draw'].value,
        arrow_hide=arrow.data.shape_keys.key_blocks['Hide'].value)


cases=[('HERO_Bow_Ready',0,0,1),('HERO_Bow_Draw',.8,1,0),('HERO_Bow_Aim',0,1,0),
       ('HERO_Bow_Release',.05,1,0),('HERO_Bow_Release',.16,None,1),
       ('HERO_Bow_SkillArrowFan',.55,0,1)]
result=[]
for name,time,draw,hidden in cases:
    values=sample(name,time)
    assert abs(values['body_draw']-values['string_draw'])<.00001
    if draw is not None:assert abs(values['body_draw']-draw)<.0001,(name,values)
    assert abs(values['arrow_hide']-hidden)<.0001,(name,values)
    result.append(dict(clip=name,time_seconds=time,**values))
(P/'bow_validation.json').write_text(json.dumps(dict(all_checks_passed=True,samples=result),indent=2),encoding='utf-8')
print('Verified bow Draw and Hide values through load, draw, aim and release')
