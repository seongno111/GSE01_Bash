"""Refresh measurements, editing displays and canonical exports from revised sources."""
import bpy
import json
import sys
import struct
import numpy as np
from pathlib import Path

P=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(P/'scripts'))
from mesh_quality import humanoid_feet
pack=next(a.split('=',1)[1] for a in sys.argv if a.startswith('pack='))
folder=P/'staged'/pack
kit=pack=='LowPolyKit_v3'
floor_only='floor-only' in sys.argv
source=folder/('lowpoly_kit_v3.blend' if kit else 'monsters_v1.blend')
bpy.ops.wm.open_mainfile(filepath=str(source))
scene=bpy.context.scene
cat_path=folder/('asset_catalog.json' if kit else 'monster_catalog.json')
cat=json.loads(cat_path.read_text('utf-8'))
for action in list(bpy.data.actions):
    if action.name.endswith('__BeforeQuality'):bpy.data.actions.remove(action)
for rig in [o for o in bpy.data.objects if o.type=='ARMATURE' and not o.name.startswith('DISPLAY')]:
    if rig.animation_data:
        rig.animation_data.action=None
        for track in rig.animation_data.nla_tracks:track.mute=True
    for b in rig.pose.bones:b.matrix_basis.identity()
bpy.context.view_layer.update()
if kit and not floor_only:
    for m in cat['monsters']:
        obj=bpy.data.objects[m['mesh']]
        obj.data=obj.data.copy()
    for m in cat['monsters']:
        humanoid_feet(bpy.data.objects[m['mesh']],bpy.data.objects[m['rig']])
    a=bpy.data.objects['MON_03_MutatedHuman']
    b=bpy.data.objects['MON_04_DeathEcho']
    if len(a.data.vertices)==len(b.data.vertices) and all((v.co-w.co).length<.000001 for v,w in zip(a.data.vertices,b.data.vertices)):
        b.data=a.data
elif not kit and not floor_only:
    for m in cat['monsters']:
        if m['rig_family']!='quadruped':
            body=next(bpy.data.objects[n] for n in m['meshes'] if n.endswith('_Body'))
            humanoid_feet(body,bpy.data.objects[m['rig']])
    for m in cat['monsters']:
        meshes=[bpy.data.objects[n] for n in m['meshes']]
        points=[v.co for o in meshes for v in o.data.vertices]
        for o in meshes:o.data.calc_loop_triangles()
        m['triangle_count']=sum(len(o.data.loop_triangles) for o in meshes)
        m['dimensions_m']=[max(v[i] for v in points)-min(v[i] for v in points) for i in range(3)]
        m['bind_revision']='2026-11-23'
        rig=bpy.data.objects[m['rig']]
        m['bone_bind_schema']=[dict(name=b.name,parent=b.parent.name if b.parent else None,
                                   head_blender_z_up=list(b.head_local),tail_blender_z_up=list(b.tail_local)) for b in rig.data.bones]
def floor_contacts(rig,objects):
    meshes=[o for o in objects if not any(k in o.name for k in ('Sword','Staff','Shield'))]
    ad=rig.animation_data
    for track in ad.nla_tracks:track.mute=True
    deps=bpy.context.evaluated_depsgraph_get()
    for track in ad.nla_tracks:
        strip=track.strips[0]
        ad.action=strip.action;ad.action_slot=strip.action_slot
        for frame in range(round(strip.action.frame_range[0]),round(strip.action.frame_range[1])+1):
            scene.frame_set(frame)
            lowest=1
            for o in meshes:
                data=o.evaluated_get(deps).data
                values=np.empty(len(data.vertices)*3,dtype=np.float32)
                data.vertices.foreach_get('co',values)
                lowest=min(lowest,float(values.reshape(-1,3)[:,2].min()))
            if lowest<.0005:
                hip=rig.pose.bones['hips']
                matrix=hip.matrix.copy();matrix.translation.z+=.001-lowest
                hip.matrix=matrix
                hip.keyframe_insert('location',frame=frame,group='hips')
        print('FLOOR',track.name,flush=True)
    ad.action=None
    for bone in rig.pose.bones:bone.matrix_basis.identity()
    bpy.context.view_layer.update()

def export(path,rig,objects):
    bpy.ops.object.select_all(action='DESELECT')
    for o in [rig,*objects]:
        o.hide_render=False;o.hide_viewport=False;o.hide_set(False);o.select_set(True)
    bpy.context.view_layer.objects.active=rig
    bpy.ops.export_scene.gltf(filepath=str(path),export_format='GLB',use_selection=True,
        export_animations=True,export_animation_mode='NLA_TRACKS',export_force_sampling=True,
        export_frame_range=False,export_anim_slide_to_zero=True,export_skins=True,export_extras=True,
        export_yup=True,export_lights=False)
    for original in objects:
        for display in list(bpy.data.objects):
            if display.type=='MESH' and display.name.startswith('DISPLAY') and display.name.endswith(original.name):
                matrix=display.matrix_world.copy();parent=display.parent;name=display.name
                collections=list(display.users_collection)
                bpy.data.objects.remove(display,do_unlink=True)
                duplicate=original.copy();duplicate.name=name;duplicate.parent=parent
                for collection in collections:collection.objects.link(duplicate)
                duplicate.matrix_world=matrix
                for mod in duplicate.modifiers:
                    if mod.type=='ARMATURE':mod.object=parent
if kit and not floor_only:
    for role in cat['roles']:
        file=role['name'].lower()+'_rigged.glb'
        raw=(P.parent/pack/'npc'/file).read_bytes()
        length=struct.unpack_from('<I',raw,12)[0]
        old=json.loads(raw[20:20+length])
        objects=[bpy.data.objects[n['name']] for n in old['nodes'] if 'mesh' in n]
        export(folder/'npc'/file,bpy.data.objects['RIG_NPC_Shared'],objects)
for m in cat['monsters']:
    if floor_only and not kit and m['rig_family']=='quadruped':continue
    if kit:
        objects=[bpy.data.objects[m['mesh']]];file=folder/'monsters'/(m['name']+'_rigged.glb')
    else:
        objects=[bpy.data.objects[n] for n in m['meshes']];file=folder/m['file']
    if kit or m['rig_family']!='quadruped':
        floor_contacts(bpy.data.objects[m['rig']],objects)
    export(file,bpy.data.objects[m['rig']],objects)
# Show only the linked presentation collections when the editing file is opened.
for collection in bpy.data.collections:
    if collection.name in ('NPC_ROLE_PREVIEWS','MONSTER_PREVIEWS','PRESENTATION','PRESENTATION.001'):
        for o in collection.objects:o.hide_set(False)
    elif collection.name in ('NPC_PART_LIBRARY','MONSTER_LIBRARY','RIG_LIBRARY','NEW_MONSTER_LIBRARY','SOURCE_RIG'):
        for o in collection.objects:o.hide_set(True)
cat_path.write_text(json.dumps(cat,ensure_ascii=False,indent=2),encoding='utf-8')
bpy.ops.wm.save_as_mainfile(filepath=str(source))
print('FINALIZED',pack,len(bpy.data.actions),flush=True)
