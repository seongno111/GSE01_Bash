"""Inspect the steel striking caps on the exported hammer at contact."""
import json
import math
import sys
from pathlib import Path

import bpy
from mathutils import Vector
from mathutils.bvhtree import BVHTree

sys.path.insert(0,str(Path(__file__).resolve().parent))
from hammer_motion import GROUND_SLAM, HEAD, SWINGS

path=Path(sys.argv[sys.argv.index('--')+1])
bpy.ops.wm.read_factory_settings(use_empty=True)
scene=bpy.context.scene
scene.render.fps=60
bpy.ops.import_scene.gltf(filepath=str(path))
rig=next(o for o in scene.objects if o.type=='ARMATURE')
ad=rig.animation_data
for track in ad.nla_tracks:
    track.mute=True
socket=rig.pose.bones['weapon_r']
rest_inverse=socket.bone.matrix_local.to_quaternion().inverted()
face_axis=Vector((1,0,0))
grip_spacing=.30
weapon=next(o for o in scene.objects if o.type=='MESH' and o.name.startswith('WPN_Hammer_'))
body_parts=[o for o in scene.objects if o.type=='MESH' and o.name.startswith(('HERO_HEAD','HERO_TORSO'))]
depsgraph=bpy.context.evaluated_depsgraph_get()
faces={o.name:[tuple(poly.vertices) for poly in o.data.polygons] for o in [weapon,*body_parts]}


def world_vertices(obj):
    evaluated=obj.evaluated_get(depsgraph)
    return [evaluated.matrix_world@v.co for v in evaluated.data.vertices]


def sample(time):
    frame=time*60
    scene.frame_set(int(frame),subframe=frame-int(frame))
    pose=rig.matrix_world@socket.matrix
    orient=pose.to_quaternion()@rest_inverse
    return pose.translation+orient@HEAD,orient


def bone_direction(start,end):
    bones=rig.pose.bones
    return rig.matrix_world.to_3x3()@(bones[end].head-bones[start].head)


catalog=json.loads((path.parent/'animation_catalog.json').read_text('utf-8'))
clips=[s for s in catalog['new_clips'] if s['name'] in SWINGS]
result={}
for clip in clips:
    name=clip['name'];time=clip['events'][0]['time_seconds']
    strip=ad.nla_tracks[name].strips[0]
    ad.action=strip.action
    ad.action_slot=strip.action_slot
    windup,hold,radius,*_=SWINGS[name]
    before,_=sample(time-.006)
    center,orientation=sample(time)
    direction=(center-before).normalized()
    face=orientation@face_axis
    side=orientation@Vector((0,1,0))
    downward=Vector((0,0,-1))
    result[name]={
        'contact_seconds':time,
        'normal_vs_incoming_motion':abs(face.dot(direction)),
        'side_vs_incoming_motion':abs(side.dot(direction)),
        'positive_cap_normal_world':list(face),
        'incoming_head_direction_world':list(direction),
    }
    points=[sample(windup+(time-windup)*i/160)[0] for i in range(161)]
    pivot=(points[0]+points[-1])/2
    angles=[math.atan2((p-pivot).z,-(p-pivot).y) for p in points]
    left=rig.pose.bones['weapon_l']
    gaps=[]
    for i in range(201):
        p,q=sample(windup+(hold-windup)*i/200)
        support=p+q@(-HEAD+Vector((0,0,-grip_spacing)))
        gaps.append((rig.matrix_world@left.head-support).length)
    clearance_hits={}
    lowest=1
    duration=clip['duration_seconds']
    count=round(duration*240)
    for t in [i*duration/count for i in range(count+1)]+[time]:
        sample(t)
        vertices=world_vertices(weapon)
        lowest=min(lowest,min(v.z for v in vertices))
        hammer=BVHTree.FromPolygons(vertices,faces[weapon.name])
        for part in body_parts:
            body=BVHTree.FromPolygons(world_vertices(part),faces[part.name])
            if hammer.overlap(body):
                clearance_hits.setdefault(part.name,[]).append(round(t,5))
    result[name].update(
        face_angle_from_floor_normal_degrees=math.degrees(face.angle(downward)),
        positive_steel_face_lowest_point_m=min(
            (center+orientation@Vector((.2525,y,z))).z for y in (-.11,.11) for z in (-.135,.135)),
        arc_span_degrees=math.degrees(angles[0]-angles[-1]),
        maximum_reverse_degrees=max(0,math.degrees(max(b-a for a,b in zip(angles,angles[1:])))),
        maximum_radius_error_m=max(abs((p-pivot).length-radius) for p in points),
        maximum_plane_error_m=max(abs(p.x-pivot.x) for p in points),
        maximum_grip_socket_error_m=max(gaps),
        minimum_hammer_mesh_z_m=lowest,
        mesh_intersection_times=clearance_hits,
    )
    if name==GROUND_SLAM:
        up=Vector((0,0,1))
        sample(windup)
        result[name]['overhead_grip_degrees_from_vertical']={
            side:math.degrees(bone_direction('upper_arm_'+side,'weapon_'+side).angle(up))
            for side in ('l','r')
        }
        result[name]['overhead_elbow_bend_degrees']={
            side:math.degrees(bone_direction('upper_arm_'+side,'forearm_'+side).angle(
                bone_direction('forearm_'+side,'hand_'+side)))
            for side in ('l','r')
        }
        result[name]['swing_radius_m']=radius
        sample(time)
        result[name]['impact_torso_lean_degrees']=math.degrees(bone_direction('hips','neck').angle(up))
print(json.dumps(result,indent=2))
(path.parent/'hammer_contact_validation.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
