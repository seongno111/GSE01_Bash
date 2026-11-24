"""Render the assembled corruption forms and their animated fusion seams."""
import bpy
import json
import math
import sys
from pathlib import Path
from mathutils import Vector

P=Path(__file__).resolve().parents[1]
V=P/'previews';V.mkdir(exist_ok=True)
catalog=json.loads((P/'corruption_catalog.json').read_text('utf-8'))
wanted=next((a.split('=',1)[1] for a in sys.argv if a.startswith('kind=')),None)
animate='animate' in sys.argv


def activate(rig,name):
    for obj in bpy.context.scene.objects:
        blocks=[obj]+([obj.data.shape_keys] if obj.type=='MESH' and obj.data.shape_keys else [])
        for block in blocks:
            ad=block.animation_data
            if not ad:continue
            ad.action=None
            for track in ad.nla_tracks:
                track.mute=True
                if track.name==name:
                    strip=track.strips[0]
                    ad.action=strip.action;ad.action_slot=strip.action_slot


for module in catalog['modules']:
    kind=module['weapon_kind']
    if wanted and kind not in wanted.split(','):continue
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene=bpy.context.scene;scene.render.fps=60
    bpy.ops.import_scene.gltf(filepath=str(P/module['assembled_file']))
    rig=next(o for o in scene.objects if o.type=='ARMATURE')
    prefix={'warhammer':'Hammer','longsword':'Sword'}.get(kind,kind.title())
    ready='HERO_Idle' if kind=='longsword' else 'HERO_'+prefix+'_Ready'
    activate(rig,ready);scene.frame_set(0)
    scene.render.engine='CYCLES';scene.cycles.samples=12;scene.cycles.use_denoising=True
    scene.render.resolution_x=520;scene.render.resolution_y=580;scene.render.resolution_percentage=100
    scene.render.image_settings.file_format='PNG'
    scene.world=bpy.data.worlds.new('AshenVault');scene.world.use_nodes=True
    scene.world.node_tree.nodes['Background'].inputs['Color'].default_value=(.085,.098,.135,1)
    scene.world.node_tree.nodes['Background'].inputs['Strength'].default_value=.55
    scene.view_settings.view_transform='AgX';scene.view_settings.look='AgX - Medium High Contrast';scene.view_settings.exposure=.85
    target=Vector((0,-.16,1.1))
    def area(name,pos,energy,color,size):
        d=bpy.data.lights.new(name,'AREA');o=bpy.data.objects.new(name,d);scene.collection.objects.link(o)
        o.location=pos;d.energy=energy;d.color=color;d.shape='DISK';d.size=size
        o.rotation_euler=(target-o.location).to_track_quat('-Z','Y').to_euler()
    area('Key',(-3,-5,5.5),800,(1,.77,.62),4)
    area('Fill',(3,-2,3),650,(.57,.67,1),4)
    area('Rim',(0,3,4),1000,(.60,.29,.42),3)
    mat=bpy.data.materials.new('AshFloor');mat.diffuse_color=(.032,.037,.048,1)
    mat.use_nodes=True;mat.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value=mat.diffuse_color
    bpy.ops.mesh.primitive_plane_add(size=200,location=(0,0,-.012));bpy.context.object.data.materials.append(mat)
    data=bpy.data.cameras.new('CorruptionPreview');cam=bpy.data.objects.new('CorruptionPreview',data)
    scene.collection.objects.link(cam);scene.camera=cam
    cam.location=(-4,-6,2.75) if module['side']=='r' else (4,-6,2.75)
    cam.rotation_euler=(target-cam.location).to_track_quat('-Z','Y').to_euler();data.type='ORTHO';data.ortho_scale=2.75 if kind!='staff' else 3.20
    scene.render.filepath=str(V/(kind+'.png'));bpy.ops.render.render(write_still=True)
    grip=rig.matrix_world@rig.pose.bones[module['fused_bone']].head
    fore=rig.matrix_world@rig.pose.bones['forearm_'+module['side']].head
    close=grip.lerp(fore,.4)
    original=cam.matrix_world.copy();original_scale=data.ortho_scale
    cam.location=close+Vector((-3,-5,1.45)) if module['side']=='r' else close+Vector((3,-5,1.45))
    cam.rotation_euler=(close-cam.location).to_track_quat('-Z','Y').to_euler()
    data.ortho_scale=1.05
    scene.render.resolution_x=640;scene.render.resolution_y=520
    scene.render.filepath=str(V/(kind+'_fusion.png'));bpy.ops.render.render(write_still=True)
    cam.matrix_world=original;data.ortho_scale=original_scale
    if animate and kind in ('longsword','warhammer','bow'):
        clip={'longsword':'HERO_AttackLongsword','warhammer':'HERO_Hammer_SkillGroundSlam','bow':'HERO_Bow_Draw'}[kind]
        activate(rig,clip)
        duration=next(s['duration_seconds'] for s in catalog['common_clips']+catalog['weapon_clips'] if s['name']==clip)
        folder=V/'_frames'/kind;folder.mkdir(parents=True,exist_ok=True)
        scene.render.resolution_x=340;scene.render.resolution_y=380;scene.cycles.samples=5
        for j in range(25):
            f=duration*60*j/24;scene.frame_set(int(f),subframe=f-int(f))
            scene.render.filepath=str(folder/f'{j:03d}.png');bpy.ops.render.render(write_still=True)
    print('RENDERED',kind,flush=True)
