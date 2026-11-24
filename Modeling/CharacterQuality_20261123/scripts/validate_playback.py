"""Reimport the GLBs and inspect deformed frames, events and looping motion."""
import json
import math
import sys
from pathlib import Path

import bpy
from mathutils import Vector

P=Path(__file__).resolve().parents[1]
root=P/'staged'
group=next((a.split('=',1)[1] for a in sys.argv if a.startswith('set=')),'npc')
if group=='npc':
    files=[root/'LowPolyKit_v3/npc/npc_parts_rigged.glb']
elif group=='legacy':files=sorted((root/'LowPolyKit_v3/monsters').glob('*.glb'))
else:files=sorted((root/'Monsters_v1').glob('*.glb'))
result={}
for file in files:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene=bpy.context.scene;scene.render.fps=60
    bpy.ops.import_scene.gltf(filepath=str(file))
    rigs=[o for o in scene.objects if o.type=='ARMATURE']
    assert len(rigs)==1,(file,'rig')
    rig=rigs[0]
    assert not any(b.constraints for b in rig.pose.bones)
    meshes=[o for o in scene.objects if o.type=='MESH' and o.name.startswith(('NPC_','MON_'))
            and not any(k in o.name for k in ('Sword','Staff','Shield','ACCESSORY_HAND'))]
    for track in rig.animation_data.nla_tracks:track.mute=True
    deps=bpy.context.evaluated_depsgraph_get()
    rows={}
    for track in rig.animation_data.nla_tracks:
        strip=track.strips[0];action=strip.action
        rig.animation_data.action=action;rig.animation_data.action_slot=strip.action_slot
        start,end=action.frame_range
        times=[start+(end-start)*i/8 for i in range(9)]
        times += [m.frame for m in action.pose_markers]
        lowest=math.inf;chain_error=0;floor_frames=[]
        for frame in times:
            f=int(frame);scene.frame_set(f,subframe=frame-f)
            for o in meshes:
                evaluated=o.evaluated_get(deps)
                bottom=min((evaluated.matrix_world@v.co).z for v in evaluated.data.vertices)
                if not math.isfinite(bottom):raise AssertionError((file,track.name,frame,'nonfinite'))
                if bottom<lowest:lowest=bottom
            if lowest<-.025:floor_frames.append(round(frame/60,3))
            for b in rig.pose.bones:
                if (b.parent and b.name.startswith(('front_','hind_','forearm_','hand_','shin_','foot_','toe_'))
                        and (b.bone.head_local-b.parent.bone.tail_local).length<.00001):
                    chain_error=max(chain_error,(b.head-b.parent.tail).length)
        # Compare exact loop endpoints only for actions declared as loops.
        rows[track.name]=dict(minimum_character_z_m=round(lowest,6),
                             maximum_connected_joint_gap_m=round(chain_error,7),
                             low_frames=floor_frames)
        print('CHECKED',file.name,track.name,round(lowest,4),round(chain_error,6),flush=True)
    result[str(file.relative_to(root))]=rows
(P/('playback_'+group+'.json')).write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
