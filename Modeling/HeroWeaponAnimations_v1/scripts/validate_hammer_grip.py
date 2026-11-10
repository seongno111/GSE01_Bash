"""Measure the corrected grip from real GLBs, including the previous revision."""
import bpy,math,json
from pathlib import Path
from mathutils import Vector
P=Path(__file__).resolve().parents[1]
results={}
for version,path in [('before',P/'revisions/20261105_before_hammer_grip/hero_warhammer_actions.glb'),('after',P/'hero_warhammer_actions.glb')]:
 bpy.ops.wm.read_factory_settings(use_empty=True);bpy.context.scene.render.fps=60
 bpy.ops.import_scene.gltf(filepath=str(path));rig=next(o for o in bpy.data.objects if o.type=='ARMATURE')
 data={};ad=rig.animation_data
 for tr in ad.nla_tracks:tr.mute=True
 for track in ad.nla_tracks:
  strip=track.strips[0];ad.action=strip.action;ad.action_slot=strip.action_slot
  angles=[];cross=[];bind=[]
  for frame in range(round(strip.action.frame_range[0]),round(strip.action.frame_range[1])+1):
   bpy.context.scene.frame_set(frame);bpy.context.view_layer.update()
   w=rig.pose.bones['weapon_r'];q=w.matrix.to_quaternion()@rig.data.bones['weapon_r'].matrix_local.to_quaternion().inverted();shaft=q@Vector((0,0,1))
   for side in ['l','r']:
    hand=rig.pose.bones['hand_'+side];forearm=rig.pose.bones['forearm_'+side];weapon=rig.pose.bones['weapon_'+side]
    h=(hand.tail-hand.head).normalized();f=(forearm.tail-forearm.head).normalized()
    angles.append(math.degrees(h.angle(f)));cross.append(abs(h.dot(shaft)))
    center=hand.matrix @ (rig.data.bones['hand_'+side].matrix_local.inverted() @ rig.data.bones['weapon_'+side].head_local)
    bind.append((center-weapon.head).length)
  data[track.name]=dict(mean_wrist_bend_degrees=sum(angles)/len(angles),maximum_wrist_bend_degrees=max(angles),maximum_hand_axis_dot_shaft=max(cross),maximum_palm_to_grip_distance_m=max(bind))
 results[version]=data
after=results['after'];checks=dict(hand_length_perpendicular_to_shaft=all(s['maximum_hand_axis_dot_shaft']<1e-4 for s in after.values()),handle_center_stays_in_palm=all(s['maximum_palm_to_grip_distance_m']<.001 for s in after.values()),mean_wrist_bend_reduced_in_all_four_clips=all(after[n]['mean_wrist_bend_degrees']<results['before'][n]['mean_wrist_bend_degrees'] for n in after))
checks['wrist_bend_under_5_degrees_in_every_keyframe']=all(s['maximum_wrist_bend_degrees']<5 for s in after.values())
result=dict(date='2026-11-05',checks=checks,measurements=results)
(P/'hammer_grip_validation.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
print(json.dumps(result,indent=2));assert all(checks.values())
