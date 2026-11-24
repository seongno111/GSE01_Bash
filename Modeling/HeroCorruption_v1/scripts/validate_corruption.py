"""Verify modular composition, the common skin and unchanged animation channels."""
import json
import sys
from pathlib import Path
import numpy as np

P=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(P.parents[1]/'doc/tools'))
from inspect_modeling_assets import load_glb,accessor

catalog=json.loads((P/'corruption_catalog.json').read_text('utf-8'))
source,sb,_=load_glb(P/catalog['source_library'])
library,lb,_=load_glb(P/catalog['part_library_file'])
bone_names=lambda doc:[doc['nodes'][i]['name'] for i in doc['skins'][0]['joints']]
reference=bone_names(source)
assert len(reference)==33
checks={};details={}
names={p['mesh'] for p in catalog['parts']}
assert len(names)==catalog['unique_mesh_count']==40
assert {n['name'] for n in library['nodes'] if 'mesh' in n}==names
assert len(catalog['modules'])==8 and len({m['weapon_key'] for m in catalog['modules']})==8


def channels(doc,binary,action):
    return {(doc['nodes'][c['target']['node']]['name'],c['target']['path']):
            (accessor(doc,binary,action['samplers'][c['sampler']]['input']),
             accessor(doc,binary,action['samplers'][c['sampler']]['output']),
             action['samplers'][c['sampler']].get('interpolation','LINEAR'))
            for c in action['channels']}


source_clips={a['name']:channels(source,sb,a) for a in source['animations']}
library_nodes={n['name']:n for n in library['nodes'] if 'mesh' in n}
bind={b['name']:np.array([b['head_blender_z_up'][0],b['head_blender_z_up'][2],-b['head_blender_z_up'][1]]) for b in catalog['bone_schema']}


def forearm_radius(name,side):
    node=library_nodes[name]
    primitives=[p for p in library['meshes'][node['mesh']]['primitives']
                if any(key in library['materials'][p['material']]['name'] for key in ('CharredDermis','AshVioletMuscle'))]
    points=np.concatenate([accessor(library,lb,p['attributes']['POSITION']) for p in primitives])
    elbow=bind['forearm_'+side];axis=bind['hand_'+side]-elbow
    t=((points-elbow)@axis)/(axis@axis)
    cross=points-elbow-t[:,None]*axis
    return float(np.max(np.linalg.norm(cross[(t>.15)&(t<.75)],axis=1)))
files=[P/catalog['part_library_file'],P/catalog['shared_body_file']]
files += [P/m[key] for m in catalog['modules'] for key in ('module_file','assembled_file')]
for file in files:
    doc,binary,_=load_glb(file)
    assert bone_names(doc)==reference,(file,'bone names')
    error=float(np.max(abs(accessor(doc,binary,doc['skins'][0]['inverseBindMatrices'])-
                           accessor(source,sb,source['skins'][0]['inverseBindMatrices']))))
    assert error<.000002,(file,'bind')
    triangles=0
    mesh_names=[]
    for node in doc['nodes']:
        if 'mesh' not in node:continue
        name=node['name'];mesh_names.append(name)
        assert name in names and node.get('skin')==0
        assert node['extras']['part_index']==next(p['part_index'] for p in catalog['parts'] if p['mesh']==name)
        primitives=doc['meshes'][node['mesh']]['primitives']
        original=library['meshes'][library_nodes[name]['mesh']]['primitives']
        assert len(primitives)==len(original)
        for primitive,lib_part in zip(primitives,original):
            triangles+=doc['accessors'][primitive['indices']]['count']//3
            for attribute in ('POSITION','NORMAL','JOINTS_0','WEIGHTS_0'):
                value=accessor(doc,binary,primitive['attributes'][attribute])
                assert np.isfinite(value).all()
                assert np.array_equal(value,accessor(library,lb,lib_part['attributes'][attribute])),(file,name,'shared geometry')
                if attribute=='WEIGHTS_0':assert np.max(abs(value.sum(axis=1)-1))<.0002
                if attribute=='JOINTS_0':assert value.max()<33
    for action in doc.get('animations',[]):
        current=channels(doc,binary,action)
        bone_channels={k:v for k,v in current.items() if k[0] in reference}
        expected={k:v for k,v in source_clips[action['name']].items() if k[0] in reference}
        assert bone_channels.keys()==expected.keys()
        for key,(times,values,interpolation) in bone_channels.items():
            old=expected[key]
            assert np.array_equal(times,old[0]) and np.array_equal(values,old[1]) and interpolation==old[2]
    details[file.relative_to(P).as_posix()]=dict(meshes=len(mesh_names),triangles=triangles,
        bones=33,actions=len(doc.get('animations',[])),maximum_inverse_bind_difference=error)

assert len(library['animations'])==49
common=set(catalog['shared_body'])
for module in catalog['modules']:
    doc,binary,_=load_glb(P/module['assembled_file'])
    expected=common|set(module['parts'])|{module['free_arm']}
    if module['weapon_kind'] not in ('warhammer','spear','bow'):expected|=set(catalog['optional_lamp'])
    assert {n['name'] for n in doc['nodes'] if 'mesh' in n}==expected
    assert {a['name'] for a in doc['animations']}==set(module['clip_names'])
    assert module['side']==('l' if module['weapon_kind']=='bow' else 'r')
    opposite='r' if module['side']=='l' else 'l'
    fused_radius=forearm_radius(module['parts'][0],module['side'])
    free_radius=forearm_radius(module['free_arm'],opposite)
    ratio=fused_radius/free_radius
    assert ratio>1.5,(module['weapon_kind'],'arm asymmetry',ratio)
    details[module['assembled_file']]['fused_forearm_radius_m']=fused_radius
    details[module['assembled_file']]['free_forearm_radius_m']=free_radius
    details[module['assembled_file']]['forearm_flesh_radius_ratio']=ratio
    if module['weapon_kind']=='bow':
        old,ob,_=load_glb(P/module['source_equipped_file'])
        old_actions={a['name']:a for a in old['animations']}
        for action in doc['animations']:
            if action['name'] not in old_actions:continue
            actual=channels(doc,binary,action)
            for key,(times,values,interpolation) in channels(old,ob,old_actions[action['name']]).items():
                if key[1]!='weights':continue
                mapped=(module['source_node_map'][key[0]],'weights')
                current=actual[mapped]
                assert np.array_equal(times,current[0]) and np.array_equal(values,current[1]) and interpolation==current[2]

checks.update(eight_equipment_keys_have_unique_modules=True,forty_shared_meshes_have_stable_indices=True,
    all_18_glbs_share_the_33_bone_hero_bind=True,all_weights_normalized=True,
    selected_arms_and_optional_lamp_compose_without_duplicates=True,
    all_49_bone_animations_copied_without_resampling=True,
    bow_draw_string_and_arrow_visibility_channels_match_source=True,
    shared_geometry_matches_library_and_modules=True)
checks['weapon_forearm_flesh_radius_exceeds_the_other_arm_by_1_5_times']=True
result=dict(date='2026-11-24',all_checks_passed=all(checks.values()),checks=checks,files=details,
    scope='Binary geometry, skin, assembly and animation verification. Deformed pose observations are in playback_validation.json.')
(P/'validation.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(result,ensure_ascii=False,indent=2))
