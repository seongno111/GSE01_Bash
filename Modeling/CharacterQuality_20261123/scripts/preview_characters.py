"""Render the exported revised characters in a consistent inspection studio."""
import bpy
import json
import math
import sys
from pathlib import Path
from mathutils import Vector

P=Path(__file__).resolve().parents[1]
STAGE=P/'staged'
OUT=P/'previews'
OUT.mkdir(exist_ok=True)
variant=next((a.split('=',1)[1] for a in sys.argv if a.startswith('set=')),'npc')


def activate(rig,label):
    for t in rig.animation_data.nla_tracks:
        t.mute=True
    track=next((t for t in rig.animation_data.nla_tracks if t.name.endswith('_'+label)),None)
    if track:
        strip=track.strips[0]
        rig.animation_data.action=strip.action;rig.animation_data.action_slot=strip.action_slot
        return strip.action


def setup(file):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    s=bpy.context.scene
    s.render.fps=60
    bpy.ops.import_scene.gltf(filepath=str(file))
    rig=next(o for o in s.objects if o.type=='ARMATURE')
    activate(rig,'Idle');s.frame_set(0)
    meshes=[o for o in s.objects if o.type=='MESH' and o.name.startswith(('NPC_','MON_','HERO_'))]
    deps=bpy.context.evaluated_depsgraph_get()
    pts=[o.matrix_world@v.co for o in meshes for v in o.evaluated_get(deps).data.vertices]
    low=Vector([min(v[i] for v in pts) for i in range(3)])
    high=Vector([max(v[i] for v in pts) for i in range(3)])
    target=(low+high)/2
    s.render.engine='CYCLES';s.cycles.samples=12;s.cycles.use_denoising=True
    s.render.resolution_x=440;s.render.resolution_y=520;s.render.resolution_percentage=100
    s.render.image_settings.file_format='PNG'
    s.world=bpy.data.worlds.new('AshenStudio');s.world.use_nodes=True
    s.world.node_tree.nodes['Background'].inputs['Color'].default_value=(.11,.14,.18,1)
    s.world.node_tree.nodes['Background'].inputs['Strength'].default_value=.55
    s.view_settings.view_transform='AgX';s.view_settings.look='AgX - Medium High Contrast';s.view_settings.exposure=.65
    def light(pos,power,color,size):
        d=bpy.data.lights.new('Studio','AREA');o=bpy.data.objects.new('Studio',d);s.collection.objects.link(o)
        o.location=pos;d.energy=power;d.color=color;d.shape='DISK';d.size=size
        o.rotation_euler=(target-o.location).to_track_quat('-Z','Y').to_euler()
    light((-3,-4,6),700,(1,.82,.63),4)
    light((4,-1,4),500,(.57,.72,1),4)
    light((1,3,5),950,(.97,.59,.29),3)
    m=bpy.data.materials.new('Slate');m.diffuse_color=(.035,.043,.053,1)
    m.use_nodes=True;m.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value=m.diffuse_color
    bpy.ops.mesh.primitive_plane_add(size=200,location=(0,0,-.012))
    bpy.context.object.data.materials.append(m)
    cam=bpy.data.cameras.new('Inspection');o=bpy.data.objects.new('Inspection',cam);s.collection.objects.link(o);s.camera=o
    o.location=target+Vector((-4,-6,1.7))
    o.rotation_euler=(target-o.location).to_track_quat('-Z','Y').to_euler()
    cam.type='ORTHO'
    def fit_camera():
        right=o.rotation_euler.to_quaternion()@Vector((1,0,0))
        up=o.rotation_euler.to_quaternion()@Vector((0,1,0))
        horizontal=[p.dot(right) for p in pts]
        vertical=[p.dot(up) for p in pts]
        cam.ortho_scale=max((max(vertical)-min(vertical))*1.18,
                           (max(horizontal)-min(horizontal))*1.18/(s.render.resolution_x/s.render.resolution_y))
    fit_camera()
    return s,rig,fit_camera


items=[]
if variant=='hero':
    items=[('hero',P.parent/'Hero_v1/hero_rigged.glb')]
elif variant=='npc':
    for role in ('priest','knight','adventurer','raider','merchant'):
        items.append((role,STAGE/'LowPolyKit_v3/npc'/f'{role}_rigged.glb'))
elif variant=='legacy':
    folder=STAGE/'LowPolyKit_v3/monsters'
    items=[(f.stem.replace('_rigged',''),f) for f in sorted(folder.glob('*.glb'))]
else:
    folder=STAGE/'Monsters_v1'
    items=[(f.stem,f) for f in sorted(folder.glob('*.glb'))]

for name,file in items:
    wanted=next((a.split('=',1)[1] for a in sys.argv if a.startswith('model=')),None)
    if wanted and name not in wanted.split(','):continue
    s,rig,fit_camera=setup(file)
    s.render.filepath=str(OUT/(name+'.png'));bpy.ops.render.render(write_still=True)
    # Side view shows body proportions, depth, headgear and joint silhouettes.
    target=Vector((0,0,s.camera.location.z-1.7))
    s.camera.location=target+Vector((-6,-.6,1.0))
    s.camera.rotation_euler=(target-s.camera.location).to_track_quat('-Z','Y').to_euler()
    fit_camera()
    s.render.filepath=str(OUT/(name+'_side.png'));bpy.ops.render.render(write_still=True)
    if name in ('knight','AshWolf','FallenPaladin','HighDemonAzar'):
        action=activate(rig,'Walk')
        if action:
            folder=OUT/'_frames'/name;folder.mkdir(parents=True,exist_ok=True)
            s.render.resolution_x=300;s.render.resolution_y=340;s.cycles.samples=5
            for i in range(20):
                s.frame_set(round(i/19*action.frame_range[1]))
                s.render.filepath=str(folder/f'{i:03d}.png');bpy.ops.render.render(write_still=True)
    print('PREVIEW',name,flush=True)
