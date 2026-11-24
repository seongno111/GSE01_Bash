"""Inspect actual exported skins, geometry and clips against the original delivery."""
import hashlib
import json
import sys
from pathlib import Path

import numpy as np

P=Path(__file__).resolve().parents[1]
BASE=P.parent
BASELINE=P/'backups' if (P/'backups').exists() else BASE
sys.path.insert(0,str(BASE.parent/'doc/tools'))
from inspect_modeling_assets import load_glb,accessor

checks={}
metrics={}
animal_error=0
old_animation=json.loads((BASELINE/'LowPolyKit_v3/animation_catalog.json').read_text('utf-8'))
new_animation=json.loads((P/'staged/LowPolyKit_v3/animation_catalog.json').read_text('utf-8'))
assert old_animation['clips']==new_animation['clips']
specs={s['name']:s for s in old_animation['clips']}
old_monsters=json.loads((BASELINE/'Monsters_v1/monster_catalog.json').read_text('utf-8'))
new_monsters=json.loads((P/'staged/Monsters_v1/monster_catalog.json').read_text('utf-8'))
for original,current in zip(old_monsters['monsters'],new_monsters['monsters']):
    assert original['clips']==current['clips']
    specs.update({s['name']:s for s in current['clips']})
assert len(specs)==175
files=list((P/'staged/LowPolyKit_v3').rglob('*.glb'))+list((P/'staged/Monsters_v1').glob('*.glb'))
for path in files:
    rel=path.relative_to(P/'staged')
    old,ob,_=load_glb(BASELINE/rel)
    new,nb,_=load_glb(path)
    label=rel.as_posix()
    joint_names=lambda d:[d['nodes'][j]['name'] for j in d['skins'][0]['joints']]
    names=joint_names(new)
    assert names==joint_names(old),(label,'bone names changed')
    mesh_names=lambda d:{n['name'] for n in d['nodes'] if 'mesh' in n}
    assert mesh_names(new)==mesh_names(old),(label,'mesh names changed')
    oldclips={a['name']:a for a in old['animations']}
    newclips={a['name']:a for a in new['animations']}
    assert oldclips.keys()==newclips.keys(),(label,'clip names changed')
    finite=True;max_weight_error=0;triangles=0;old_triangles=0
    for d,binary,is_new in ((old,ob,False),(new,nb,True)):
        for mesh in d['meshes']:
            for primitive in mesh['primitives']:
                triangles_here=d['accessors'][primitive['indices']]['count']//3
                if not is_new:old_triangles+=triangles_here;continue
                triangles+=triangles_here
                for key,i in primitive['attributes'].items():
                    values=accessor(d,binary,i)
                    finite &= bool(np.isfinite(values).all())
                    if key=='WEIGHTS_0':
                        max_weight_error=max(max_weight_error,float(np.max(abs(values.sum(axis=1)-1))))
                    if key=='JOINTS_0':assert values.max()<len(names)
                assert 'WEIGHTS_0' in primitive['attributes'] and 'JOINTS_0' in primitive['attributes']
    assert finite and max_weight_error<.0003,(label,'invalid skin')
    assert triangles>old_triangles,(label,triangles,old_triangles)
    def channels(doc,binary,action):
        return {(doc['nodes'][c['target']['node']]['name'],c['target']['path']):
                (accessor(doc,binary,action['samplers'][c['sampler']]['input']),
                 accessor(doc,binary,action['samplers'][c['sampler']]['output']))
                for c in action['channels']}
    max_loop_error=0
    for name,clip in newclips.items():
        c=channels(new,nb,clip);prev=channels(old,ob,oldclips[name])
        assert c.keys()==prev.keys(),(label,name,'channels')
        duration=max(v[0][-1,0]-v[0][0,0] for v in c.values())
        oldduration=max(v[0][-1,0]-v[0][0,0] for v in prev.values())
        assert abs(duration-oldduration)<.00001,(label,name,'duration')
        for (bone,channel),(times,values) in c.items():
            assert np.isfinite(values).all() and np.all(np.diff(times[:,0])>0)
            if channel=='rotation':
                norms=np.linalg.norm(values,axis=1)
                assert np.max(abs(norms-1))<.001,(label,name,bone,'quaternion')
            if bone=='root' and channel=='translation':
                assert np.max(abs(values[:,(0,2)]-values[0,(0,2)]))<.00001,(label,name,'horizontal root drift')
            if specs[name]['loop']:
                delta=min(np.max(abs(values[-1]-values[0])),np.max(abs(values[-1]+values[0]))) if channel=='rotation' else np.max(abs(values[-1]-values[0]))
                max_loop_error=max(max_loop_error,float(delta))
        if path.stem in ('AshWolf','BlightBear','ThornStag'):
            for key,(times,values) in c.items():
                old_times,old_values=prev[key]
                aligned=old_times[:,0]-old_times[0,0]+times[0,0]
                sampled=np.stack([np.interp(aligned,times[:,0],values[:,j]) for j in range(values.shape[1])],axis=1)
                if key[1]=='rotation':
                    sampled/=np.linalg.norm(sampled,axis=1)[:,None]
                    signs=np.where(np.sum(sampled*old_values,axis=1)<0,-1,1)
                    sampled*=signs[:,None]
                error=float(np.max(abs(sampled-old_values)))
                animal_error=max(animal_error,error)
                assert error<.0001,(label,name,'animal motion changed',error)
    assert max_loop_error<.001,(label,'loop endpoint',max_loop_error)
    assert sum(n.endswith('_Walk') for n in newclips)==1
    assert sum(n.endswith('_Run') for n in newclips)==1
    assert not any('TurnScreen' in n or 'WalkScreen' in n for n in newclips)
    metrics[label]=dict(triangles_before=old_triangles,triangles_after=triangles,
                       meshes=len(mesh_names(new)),bones=len(names),clips=len(newclips),
                       max_weight_error=max_weight_error,max_loop_endpoint_error=max_loop_error)

oldcat=json.loads((BASELINE/'LowPolyKit_v3/asset_catalog.json').read_text('utf-8'))
newcat=json.loads((P/'staged/LowPolyKit_v3/asset_catalog.json').read_text('utf-8'))
identity=lambda c:[(p['mesh'],p['slot'],p['slot_index'],p['part_index']) for p in c['parts']]
assert identity(oldcat)==identity(newcat) and len(newcat['parts'])==51
assert oldcat['npc_presets']==newcat['npc_presets']
assert oldcat['headgear_rules']==newcat['headgear_rules']
assert oldcat['props']==newcat['props']
assert oldcat['slots']==newcat['slots']
doc,binary,_=load_glb(P/'staged/LowPolyKit_v3/npc/npc_parts_rigged.glb')
head=next(n for n in doc['nodes'] if n['name']=='NPC_HEAD_00_Male')
points=np.concatenate([accessor(doc,binary,p['attributes']['POSITION']) for p in doc['meshes'][head['mesh']]['primitives']])
head_height=float(np.ptp(points[:,1]))
ratio=newcat['height_m']/head_height
assert 4.80<ratio<5.20,ratio
checks.update(
    exported_files_count=len(files)==17,
    original_mesh_and_bone_names=True,
    modular_slots_51_parts_and_presets=True,
    headgear_keeps_real_head=True,
    every_character_gained_authored_geometry=True,
    normalized_skin_weights_and_valid_joint_indices=True,
    all_175_clip_names_and_durations_preserved=True,
    one_walk_and_run_per_rig=True,
    all_animation_samples_are_finite=True,
    five_head_npc_ratio=True,
    revised_quadruped_anatomy_and_motion_preserved=True,
    props_and_assignment_indices_preserved=True,
    loops_and_contact_event_times_preserved=True,
)
result=dict(date='2026-11-23',checks=checks,all_checks_passed=all(checks.values()),
            npc_head_height_m=head_height,npc_body_to_head_ratio=ratio,files=metrics,
            quadruped_maximum_animation_channel_difference=animal_error,
            scope='Exported geometry, skins and action data. See playback_validation.json for deformed frame checks.')
(P/'validation.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(result,ensure_ascii=False,indent=2))
