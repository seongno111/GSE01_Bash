"""Reimport each assembled skin and inspect fusion sockets throughout its clips."""
import bpy
import json
import math
from pathlib import Path
from mathutils import Vector

P=Path(__file__).resolve().parents[1]
catalog=json.loads((P/'corruption_catalog.json').read_text('utf-8'))
results={}
for module in catalog['modules']:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene=bpy.context.scene;scene.render.fps=60
    bpy.ops.import_scene.gltf(filepath=str(P/module['assembled_file']))
    rig=next(o for o in scene.objects if o.type=='ARMATURE')
    for track in rig.animation_data.nla_tracks:track.mute=True
    meshes=[o for o in scene.objects if o.type=='MESH' and o.name.startswith('CORR_')]
    body=[o for o in meshes if o.name.startswith('CORR_BODY_')]
    wrist='hand_'+module['side']
    anchor=rig.data.bones[module['fused_bone']].head_local.copy()
    hand_rest=rig.data.bones[wrist].matrix_local.inverted()@anchor
    deps=bpy.context.evaluated_depsgraph_get()
    rows={}
    for track in rig.animation_data.nla_tracks:
        strip=track.strips[0]
        rig.animation_data.action=strip.action;rig.animation_data.action_slot=strip.action_slot
        for obj in meshes:
            keys=obj.data.shape_keys
            if keys and keys.animation_data:
                keys.animation_data.action=None
                for morph_track in keys.animation_data.nla_tracks:
                    morph_track.mute=True
                    if morph_track.name==track.name:
                        morph=morph_track.strips[0]
                        keys.animation_data.action=morph.action;keys.animation_data.action_slot=morph.action_slot
        start,end=strip.action.frame_range
        # Dense samples cover the very fast hammer release, wrist roll and sword turn.
        count=max(12,math.ceil((end-start)/3))
        times=[start+(end-start)*i/count for i in range(count+1)]
        max_gap=0;max_joint=0;lowest_body=1
        for frame in times:
            f=int(frame);scene.frame_set(f,subframe=frame-f)
            bone=rig.pose.bones[module['fused_bone']]
            hand=rig.pose.bones[wrist]
            expected=hand.matrix@hand_rest
            max_gap=max(max_gap,(bone.head-expected).length)
            for side in 'lr':
                for parent,child in [('upper_arm_','forearm_'),('forearm_','hand_'),('thigh_','shin_'),('shin_','foot_')]:
                    max_joint=max(max_joint,(rig.pose.bones[parent+side].tail-rig.pose.bones[child+side].head).length)
            for obj in body:
                data=obj.evaluated_get(deps).data
                low=min((obj.matrix_world@v.co).z for v in data.vertices)
                assert math.isfinite(low)
                lowest_body=min(lowest_body,low)
            for obj in meshes:
                if obj.data.shape_keys:
                    assert all(math.isfinite(key.value) for key in obj.data.shape_keys.key_blocks)
        assert max_gap<.006,(module['weapon_kind'],track.name,'fusion socket',max_gap)
        assert max_joint<.001,(module['weapon_kind'],track.name,'joint gap',max_joint)
        rows[track.name]=dict(samples=len(times),maximum_fusion_socket_gap_m=max_gap,
            maximum_limb_joint_gap_m=max_joint,minimum_corrupted_body_z_m=lowest_body)
    results[module['weapon_kind']]=rows
    print('CHECKED',module['weapon_kind'],len(rows),'clips',flush=True)
report=dict(date='2026-11-24',all_checks_passed=True,variants=results,
    scope='Reimported assembled skins. Sampled fusion sockets, limb joints and finite deformed body bounds. Terrain collision and arbitrary crossfades are integration work.')
(P/'playback_validation.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print('Completed fusion playback verification')
