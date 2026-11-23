"""Sample the supplied FBX for reference, without copying its rig into the hero."""
import json
import sys
from pathlib import Path

import bpy
from mathutils import Vector

source, output = sys.argv[sys.argv.index('--')+1:]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.context.scene.render.fps=60
bpy.ops.import_scene.fbx(filepath=source)
rig=next(o for o in bpy.context.scene.objects if o.type=='ARMATURE')
scene=bpy.context.scene
action=rig.animation_data.action
start,end=action.frame_range
fps=scene.render.fps/scene.render.fps_base
names={b.name.split(':')[-1]: b for b in rig.pose.bones}
chosen=('Hips','Spine','Spine1','Spine2','Head','RightHand','LeftHand','RightFoot','LeftFoot','RightToeBase','LeftToeBase')
print('ARMATURE',rig.name,rig.scale[:],action.name,list(action.frame_range), scene.render.fps)
print('BONES',list(names))
chosen=[name for name in chosen if name in names]

def xyz(v):
    return [round(x,6) for x in v]

samples=[]
for f in range(round(start),round(end)+1):
    scene.frame_set(f)
    positions={name:rig.matrix_world@names[name].head for name in chosen}
    chest=rig.matrix_world@names['Spine2'].matrix
    samples.append(dict(frame=f,time=(f-start)/fps,points={n:xyz(v) for n,v in positions.items()},
                        chest_y=xyz((chest.to_3x3()@Vector((0,1,0))).normalized()),
                        chest_z=xyz((chest.to_3x3()@Vector((0,0,1))).normalized())))
Path(output).write_text(json.dumps({'action':action.name,'fps':fps,'samples':samples}),encoding='utf-8')
for row in samples[::6]:
    points=row['points'];h=Vector(points['Hips'])
    vectors={name:xyz(Vector(points[name])-h) for name in ('RightHand','LeftHand','Spine2','RightFoot','LeftFoot')}
    print(round(row['time'],2),xyz(h),vectors)
