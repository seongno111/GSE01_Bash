"""Independent binary GLB checks; run with the bundled Python (numpy)."""
import json,struct,hashlib,sys
from pathlib import Path
import numpy as np
PACKAGE=Path(__file__).resolve().parents[1];BASE=PACKAGE.parent
P=next((Path(a.split('=',1)[1]).resolve() for a in sys.argv if a.startswith('out=')),PACKAGE)
BASELINE=PACKAGE/'revisions/20261110_before_five_head'
cat=json.loads((P/'animation_catalog.json').read_text('utf-8'))
class GLB:
 def __init__(self,p):
  b=p.read_bytes();assert b[:4]==b'glTF';n=struct.unpack_from('<I',b,12)[0];self.d=json.loads(b[20:20+n]);self.b=b[28+n:];self.nodes={v['name']:v for v in self.d['nodes'] if 'name' in v}
 def acc(self,i):
  a=self.d['accessors'][i];v=self.d['bufferViews'][a['bufferView']];dt={5120:'i1',5121:'u1',5122:'<i2',5123:'<u2',5125:'<u4',5126:'<f4'}[a['componentType']];n={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4,'MAT4':16}[a['type']];dtype=np.dtype(dt);stride=v.get('byteStride',n*dtype.itemsize)
  return np.ndarray((a['count'],n),dtype=dtype,buffer=self.b,offset=v.get('byteOffset',0)+a.get('byteOffset',0),strides=(stride,dtype.itemsize)).copy()
 def clips(self): return {a['name']:a for a in self.d.get('animations',[])}
 def channels(self,a):
  out={}
  for c in a['channels']:
   s=a['samplers'][c['sampler']];path=c['target']['path'];name=self.d['nodes'][c['target']['node']]['name'];t=self.acc(s['input']).flatten();values=self.acc(s['output']);out[name,path]=(t,values,s.get('interpolation','LINEAR'))
  return out

def sample(channel,t):
 ts,v,interp=channel;t=t+ts[0]
 if interp=='STEP': return v[np.searchsorted(ts,t,side='right').clip(1,len(ts))-1]
 return np.stack([np.interp(t,ts,v[:,i]) for i in range(v.shape[1])],axis=1)

hero_path=P/'hero_rigged.glb' if P!=PACKAGE else BASE/'Hero_v1/hero_rigged.glb'
old=GLB(BASELINE/'hero_rigged.glb');active=GLB(hero_path);lib=GLB(P/cat['library_file']);checks={};details={}
def primitives(g,node):
 return g.d['meshes'][node['mesh']]['primitives']


def triangles(g):
 return sum(g.d['accessors'][p['indices']]['count']//3
            for name,node in g.nodes.items() if name.startswith('HERO_') and 'mesh' in node
            for p in primitives(g,node))


def part_points(g,prefix):
 return np.concatenate([g.acc(p['attributes']['POSITION']) for name,node in g.nodes.items()
                        if name.startswith(prefix) and 'mesh' in node for p in primitives(g,node)])


mesh_names=lambda g:{n for n,v in g.nodes.items() if n.startswith('HERO_') and 'mesh' in v}
checks['25_original_hero_part_names_retained']=mesh_names(old)==mesh_names(active)==mesh_names(lib) and len(mesh_names(active))==25
checks['common_and_library_keep_lantern_light']=all('KHR_lights_punctual' in g.d.get('extensions',{}) for g in (active,lib))
head=part_points(active,'HERO_HEAD_')
hood=part_points(active,'HERO_HEADGEAR_')
feet=part_points(active,'HERO_FOOT_')
head_height=float(np.ptp(head[:,1]))
height=float(np.max(head[:,1])-np.min(feet[:,1]))
details.update(head_height_m=head_height,height_without_hood_m=height,body_to_head_ratio=height/head_height,
               hood_height_m=float(np.max(hood[:,1])),triangles_before=triangles(old),triangles_after=triangles(active))
checks['five_head_body_ratio']=4.85<=details['body_to_head_ratio']<=5.20
checks['moderate_polygon_budget']=details['triangles_before']<details['triangles_after']<8000
def skeleton(g):
 skin=g.d['skins'][0];names=[g.d['nodes'][i]['name'] for i in skin['joints']]
 parent={c:p for p,n in enumerate(g.d['nodes']) for c in n.get('children',[])}
 hierarchy={n:(g.d['nodes'][parent[i]]['name'] if i in parent else None) for n,i in zip(names,skin['joints'])}
 return names,hierarchy,g.acc(skin['inverseBindMatrices'])
on,oh,ob=skeleton(old);ln,lh,lb=skeleton(lib);an,ah,ab=skeleton(active)
checks['original_33_bone_names_and_hierarchy_preserved']=on==ln and oh==lh and len(ln)==33
details['inverse_bind_max_error']=float(np.max(np.abs(ab-lb)))
checks['common_and_library_share_revised_bind']=details['inverse_bind_max_error']<2e-6 and an==ln and ah==lh
checks['bind_updated_for_reproportioned_limbs']=np.max(np.abs(ob-lb))>.05
socket_indices=[ln.index('weapon_l'),ln.index('weapon_r')]
socket_delta=ob.reshape(-1,4,4)[socket_indices,:3,:3]-lb.reshape(-1,4,4)[socket_indices,:3,:3]
details['weapon_socket_rest_axis_max_change']=float(np.max(np.abs(socket_delta)))
checks['weapon_socket_rest_axes_match_previous_mounts']=details['weapon_socket_rest_axis_max_change']<.0002
geoerror=0;geook=True;weights_ok=True
for name,node in active.nodes.items():
 if 'mesh' not in node: continue
 newnode=lib.nodes.get(name)
 if not newnode or 'mesh' not in newnode:geook=False;continue
 pa=primitives(active,node);pb=primitives(lib,newnode)
 if len(pa)!=len(pb):geook=False;continue
 for a,b in zip(pa,pb):
  for key in ['POSITION','NORMAL','JOINTS_0','WEIGHTS_0']:
   av=active.acc(a['attributes'][key]);bv=lib.acc(b['attributes'][key])
   if av.shape!=bv.shape: geook=False;continue
   geoerror=max(geoerror,float(np.max(np.abs(av.astype(float)-bv))))
  weights=active.acc(a['attributes']['WEIGHTS_0'])
  weights_ok &= np.all(np.isfinite(weights)) and np.max(np.abs(weights.sum(axis=1)-1))<.0002
checks['common_and_library_use_same_tailored_meshes']=geook and geoerror<2e-6
checks['tailored_vertices_have_normalized_weights']=weights_ok
details['hero_attribute_max_error']=geoerror
oldclips=old.clips();newclips=lib.clips()
checks['exactly_41_revised_actions_and_8_skills']=set(newclips)=={s['name'] for s in cat['common_clips']+cat['new_clips']} and len(newclips)==49
checks['single_shared_walk_and_run']=sum('Walk' in n for n in newclips)==1 and sum('Run' in n for n in newclips)==1 and not any(('Screen' in n or 'Turn' in n) for n in newclips)
commonerr=0;commonok=True
for s in cat['common_clips']:
 n=s['name'];x=active.channels(active.clips()[n]);y=lib.channels(newclips[n]);duration=max(t[0][-1]-t[0][0] for t in x.values())
 commonok &= abs(duration-s['duration_seconds'])<1e-5 and len(x)==99 and x.keys()==y.keys()
 for key in x:
  av=x[key][1];bv=y[key][1];err=np.max(np.abs(av-bv));commonerr=max(commonerr,float(err))
  commonok &= np.array_equal(x[key][0],y[key][0]) and np.all(np.isfinite(av))
  if key==('root','rotation') and s['root_yaw_degrees']==0:commonok &= np.max(np.abs(av[:,:3]))<1e-6
checks['common_and_sword_actions_match_library']=bool(commonok and commonerr<1e-6)
details['common_clip_sample_error']=commonerr
finite=True;durations=True;rootstatic=True;loops=True;equipped=True;events=True;channelcoverage=True;bowchannels=True
newdetails={}
for eq in cat['equipment']:
 g=GLB(P/eq['file']);cs=g.clips();equipped &=set(cs)==set(eq['clip_names'])
 sn,sh,sb=skeleton(g);equipped &=sn==ln and sh==lh and np.max(np.abs(sb-lb))<2e-6
 mesh_nodes={n for n,v in g.nodes.items() if 'mesh' in v}
 equipped &=all(n not in mesh_nodes for n in eq['hide_original_meshes']) and all(n in mesh_nodes for n in eq['weapon_parts'])
 for s in [s for s in cat['new_clips'] if s['weapon_kind']==eq['weapon_kind']]:
  ch=g.channels(cs[s['name']]);duration=max(v[0][-1]-v[0][0] for v in ch.values());durations &=abs(duration-s['duration_seconds'])<1e-5
  for (name,path),(t,v,interp) in ch.items():
   finite &=bool(np.all(np.isfinite(v)) and np.all(np.isfinite(t)) and np.all(np.diff(t)>0))
   if name=='root' and path=='translation':rootstatic &=bool(np.max(np.abs(v))<1e-6)
   if name=='root' and path=='rotation':rootstatic &=bool(np.max(np.abs(v[:,:3]))<1e-6)
   if s['loop']:loops &=bool(np.max(np.abs(v[0]-v[-1]))<1e-5)
  channelcoverage &= all((n,p) in ch for n in ln for p in ['translation','rotation','scale'])
  events &=all(0<=e['time_seconds']<=duration+1e-5 and abs(e['time_seconds']-(e['frame']-1)/60)<=1/120+1e-6 for e in s['events'])
  if eq['weapon_kind']=='bow':
   morphs={name:values for (name,path),values in ch.items() if path=='weights'}
   details.setdefault('bow_morph_ranges',{})[s['name']]={n:dict(first=float(v[1][0,0]),last=float(v[1][-1,0]),minimum=float(v[1].min()),maximum=float(v[1].max())) for n,v in morphs.items()}
   bowchannels &=len(morphs)==3
   arrow=morphs.get('AMMO_Arrow_00_Iron__Body')
   if arrow:
    val=arrow[1]
    if s['name'] in ['HERO_Bow_Aim','HERO_Bow_Draw']:bowchannels &=float(val.max())==0
    if s['name']=='HERO_Bow_Ready':bowchannels &=float(val.min())==1
    if s['name']=='HERO_Bow_Release':bowchannels &=float(val[0,0])==0 and float(val[-1,0])==1
   else:bowchannels=False
  newdetails[s['name']]=dict(duration_seconds=float(duration),channels=len(ch),events=s['events'])
checks.update(nine_glbs_readable=True,equipped_glbs_match_original_skin_and_expected_parts=bool(equipped),equipped_clips_have_full_33_bone_channels=bool(channelcoverage),equipped_clip_durations_match_catalog=bool(durations),equipped_clip_values_finite=bool(finite),equipped_root_translation_and_yaw_static=bool(rootstatic),loop_endpoints_match=bool(loops),event_times_valid=bool(events),bow_string_limb_and_arrow_morph_channels=bool(bowchannels))
re=json.loads((P/'reimport_validation.json').read_text('utf-8'));stats=[s for group in re.values() for s in group.values()]
checks['all_49_actual_export_clips_reimported']=len(stats)==49
details['minimum_boot_z_m']=min(s['minimum_boot_z_m'] for s in stats)
details['minimum_visible_mesh_z_m']=min(s['minimum_visible_mesh_z_m'] for s in stats)
details['maximum_chain_gap_m']=max(s['maximum_chain_gap_m'] for s in stats)
details['maximum_hammer_wrist_bend_degrees']=max(s['maximum_hammer_wrist_bend_degrees'] for s in stats)
details['maximum_elbow_step_m']=max(s['maximum_elbow_step_per_60fps_frame_m'] for s in stats)
details['maximum_hammer_elbow_step_m']=max(s['maximum_elbow_step_per_60fps_frame_m'] for s in re['warhammer'].values())
details['maximum_foot_anchor_deviation_m']=max(s['max_foot_anchor_deviation_m'] for s in stats)
details['maximum_support_grip_gap_m']=max(s['maximum_support_grip_gap_m'] for s in stats)
details['maximum_bow_draw_hand_to_nock_m']=max(s['maximum_bow_draw_hand_to_nock_m'] for s in stats)
details['maximum_bow_hand_step_per_60fps_frame_m']=max(s['maximum_hand_step_per_60fps_frame_m'] for s in re['bow'].values())
checks['boot_floor_and_foot_lock_within_1mm']=details['minimum_boot_z_m']>-.001 and details['maximum_foot_anchor_deviation_m']<.001
checks['visible_meshes_clear_floor_within_2mm']=details['minimum_visible_mesh_z_m']>-.002
checks['limb_connections_within_1mm']=details['maximum_chain_gap_m']<.001
checks['hammer_neutral_wrist_and_continuous_elbows']=details['maximum_hammer_wrist_bend_degrees']<5 and details['maximum_hammer_elbow_step_m']<.17
checks['all_clips_keep_root_in_place']=max(s['max_root_translation_m'] for s in stats)<1e-6
details['maximum_baked_support_grip_gap_m']=max(s['baked_support_grip_gap_m'] for s in stats)
checks['two_handed_grips_within_1mm_on_keys_and_5mm_between_keys']=(
    details['maximum_baked_support_grip_gap_m']<.001 and details['maximum_support_grip_gap_m']<.005
)
checks['bow_draw_hand_matches_animated_nock_within_1mm']=details['maximum_bow_draw_hand_to_nock_m']<.001
checks['bow_transitions_no_single_frame_hand_jump_over_6cm']=details['maximum_bow_hand_step_per_60fps_frame_m']<.06
checks['reimported_loops_match']=all(s['loop_matrix_error'] is None or s['loop_matrix_error']<1e-5 for s in stats)
transition_error=0
def endpoint_error(a,which,b,other):
 ac=lib.channels(newclips[a]);bc=lib.channels(newclips[b]);error=0
 for key in ac:
  if key not in bc:continue
  x=ac[key][1][which];y=bc[key][1][other]
  if key[1]=='rotation':e=1-abs(np.dot(x,y)/(np.linalg.norm(x)*np.linalg.norm(y)))
  else:e=np.max(np.abs(x-y))
  error=max(error,float(e))
 if error>1e-5:
  print('Endpoint difference',a,which,b,other,error)
 return error
for s in cat['new_clips']:
 if s['weapon_kind']=='bow':continue
 if s['start_pose']=='Ready':
  prefix='Hammer' if s['weapon_kind']=='warhammer' else s['weapon_kind'].title()
  transition_error=max(transition_error,endpoint_error(s['name'],0,'HERO_'+prefix+'_Ready',0),endpoint_error(s['name'],-1,'HERO_'+prefix+'_Ready',0))
for a,which,b,other in [('HERO_Bow_Nock',0,'HERO_Bow_Ready',0),('HERO_Bow_Nock',-1,'HERO_Bow_Draw',0),('HERO_Bow_Draw',-1,'HERO_Bow_Aim',0),('HERO_Bow_Release',0,'HERO_Bow_Aim',0),('HERO_Bow_Release',-1,'HERO_Bow_Ready',0),('HERO_Bow_Lower',0,'HERO_Bow_Aim',0),('HERO_Bow_Lower',-1,'HERO_Bow_Ready',0)]:transition_error=max(transition_error,endpoint_error(a,which,b,other))
for s in cat['skills']:
 if s['weapon_kind']=='longsword':
  ready='HERO_Idle'
 else:
  ready=s['name'].split('_Skill')[0]+'_Ready'
 transition_error=max(transition_error,endpoint_error(s['name'],0,ready,0),endpoint_error(s['name'],-1,ready,0))
transition_error=max(transition_error,endpoint_error('HERO_Death',-1,'HERO_GetUp',0),endpoint_error('HERO_GetUp',-1,'HERO_Idle',0))
details['attack_and_bow_transition_pose_error']=transition_error
checks['attack_ready_and_bow_chain_endpoints_match']=transition_error<1e-5
previous=GLB(BASELINE/'hero_weapon_animation_library.glb')
changes={}
for name,a in previous.clips().items():
 ac=previous.channels(a);bc=lib.channels(newclips[name])
 duration=max(v[0][-1]-v[0][0] for v in ac.values());ts=np.linspace(0,duration,43)
 change=0
 for key in ac:
  x=sample(ac[key],ts);y=sample(bc[key],ts)
  if key[1]=='rotation':
   x/=np.linalg.norm(x,axis=1)[:,None];y/=np.linalg.norm(y,axis=1)[:,None];err=np.max(1-np.abs(np.sum(x*y,axis=1)))
  else:err=np.max(np.abs(x-y))
  change=max(change,float(err))
 changes[name]=change
details['per_clip_pose_change']=changes
checks['all_49_actions_retargeted_from_previous_delivery']=len(changes)==49 and min(changes.values())>.0001
def timing(specs):
 return {s['name']:(s['duration_seconds'],s['loop'],s['events']) for s in specs if 'skill_label' not in s}
checks['original_clip_timing_loop_and_events_preserved']=(
 timing(cat['common_clips'])==timing(json.loads((BASELINE/'hero_catalog.json').read_text('utf-8'))['clips'])
 and timing(cat['new_clips'])==timing(json.loads((BASELINE/'animation_catalog.json').read_text('utf-8'))['new_clips'])
)
expected={
 'HERO_Staff_SkillAshField':(1.40,[.45,.80,1.15]),
 'HERO_Bow_SkillArrowFan':(.95,[.55]),
 'HERO_Mace_SkillGuardBreak':(.82,[.38]),
 'HERO_Axe_SkillExecution':(1.05,[.55]),
 'HERO_Hammer_SkillGroundSlam':(1.35,[.72]),
 'HERO_Spear_SkillLongThrust':(.86,[.40]),
 'HERO_Dagger_SkillFlurry':(.67,[.14,.27,.40]),
 'HERO_Sword_SkillWhirlwind':(1.12,[.35,.75]),
}
checks['skill_metadata_matches_specification']={
    s['name']:(s['duration_seconds'],[e['time_seconds'] for e in s['events']]) for s in cat['skills']
}==expected and all(not s['loop'] and all(e['name']=='skill_contact' for e in s['events']) for s in cat['skills'])
checks['skill_contact_samples_exact_in_export']=all(
    all(abs(t[-1]-duration)<1e-6 and all(np.min(np.abs(t-hit))<1e-6 for hit in hits)
        for t,_,_ in lib.channels(newclips[name]).values())
    for name,(duration,hits) in expected.items()
)
sword_channels=active.channels(active.clips()['HERO_Sword_SkillWhirlwind'])
rotation=sword_channels['root','rotation'][1]
angle=np.unwrap(2*np.arctan2(rotation[:,1],rotation[:,3]))
turns=float((angle[-1]-angle[0])/(2*np.pi))
details['whirlwind_turns']=turns
checks['sword_two_turns_about_vertical_at_fixed_origin']=abs(abs(turns)-2)<.0001 and np.max(np.abs(rotation[:,[0,2]]))<1e-6
bow=GLB(P/'hero_bow_actions.glb')
fan=bow.channels(bow.clips()['HERO_Bow_SkillArrowFan'])
weights={name:values for (name,path),values in fan.items() if path=='weights'}
before_hit=np.array([32/60+1e-6])
at_hit=np.array([.55+1e-6])
checks['bow_skill_string_and_arrow_release_at_055']=len(weights)==3 and all(
    abs(float(sample(c,before_hit)[0,0])-(0 if name.startswith('AMMO_') else 1))<.0001
    and abs(float(sample(c,at_hit)[0,0])-(1 if name.startswith('AMMO_') else 0))<.0001
    for name,c in weights.items()
)
ground={
    name:re[kind][name].get('weapon_floor_at_contacts_m',{})
    for kind,name in [('staff','HERO_Staff_SkillAshField'),('warhammer','HERO_Hammer_SkillGroundSlam')]
}
details['weapon_floor_at_skill_contacts_m']=ground
checks['staff_and_hammer_ground_contacts_within_8mm']=all(
    len(samples)==len(expected[name][1]) and all(abs(z)<.008 for z in samples.values())
    for name,samples in ground.items()
)
speed_examples=[
    ('common','HERO_AttackLongsword'),('mace','HERO_Mace_SkillGuardBreak'),
    ('axe','HERO_Axe_SkillExecution'),('warhammer','HERO_Hammer_SkillGroundSlam'),
    ('spear','HERO_Spear_SkillLongThrust'),('dagger','HERO_Dagger_SkillFlurry'),
]
speed_profile={}
for group,name in speed_examples:
    row=re[group][name]
    speed_profile[name]={
        'contact_m_per_s':row['peak_contact_grip_speed_m_per_s'],
        'settle_m_per_s':row['peak_final_settle_grip_speed_m_per_s'],
    }
details['grip_speed_profile']=speed_profile
checks['strike_motion_faster_than_final_settle']=all(
    row['contact_m_per_s']>1.5*max(.25,row['settle_m_per_s']) for row in speed_profile.values()
)
hammer_contact_path=P/'hammer_contact_validation.json'
if hammer_contact_path.exists() and cat.get('hammer_animation_revision'):
    faces=json.loads(hammer_contact_path.read_text('utf-8'))
    checks['hammer_steel_striking_caps_lead_at_all_three_contacts']=len(faces)==3 and all(
        row['normal_vs_incoming_motion']>.76 and row['side_vs_incoming_motion']<.53
        for row in faces.values()
    )
    checks['all_hammer_striking_caps_reach_floor']=all(
        row['face_angle_from_floor_normal_degrees']<33
        and abs(row['positive_steel_face_lowest_point_m']-.001)<.003 for row in faces.values()
    )
    checks['hammer_heads_follow_one_vertical_semicircle']=all(
        abs(row['arc_span_degrees']-180)<.1 and row['maximum_reverse_degrees']<.02
        and row['maximum_radius_error_m']<.003 and row['maximum_plane_error_m']<.001
        and row['maximum_grip_socket_error_m']<.003 for row in faces.values()
    )
    checks['hammer_mesh_clears_head_hood_torso_and_floor']=all(
        row['minimum_hammer_mesh_z_m']>-.001 and not row['mesh_intersection_times']
        for row in faces.values()
    )
    slam=faces['HERO_Hammer_SkillGroundSlam']
    checks['ground_slam_overhead_extension_and_torso_fold']=(
        max(slam['overhead_grip_degrees_from_vertical'].values())<35
        and max(slam['overhead_elbow_bend_degrees'].values())<45
        and slam['impact_torso_lean_degrees']>30 and slam['swing_radius_m']>.93
    )
    details['hammer_striking_faces']=faces
checks={name:bool(passed) for name,passed in checks.items()}
result=dict(date=cat.get('hammer_animation_revision',{}).get('date',cat['date']),all_checks_passed=all(checks.values()),checks=checks,metrics=details,clips=newdetails,scope='Five-head hero mesh, shared revised skin, and frame-by-frame checks for 49 retargeted animations.')
(P/'validation.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(dict(checks=checks,metrics=details),indent=2));assert result['all_checks_passed'], 'See validation.json'
