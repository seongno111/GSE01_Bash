"""Independent binary GLB checks; run with the bundled Python (numpy)."""
import json,struct,hashlib
from pathlib import Path
import numpy as np
P=Path(__file__).resolve().parents[1];BASE=P.parent
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

old=GLB(BASE/'Hero_v1/hero_rigged.glb');lib=GLB(P/cat['library_file']);checks={};details={}
checks['source_files_unchanged']=all(hashlib.sha256((BASE/f).read_bytes()).hexdigest()==h for f,h in cat['hero_source_hashes'].items())
def skeleton(g):
 skin=g.d['skins'][0];names=[g.d['nodes'][i]['name'] for i in skin['joints']]
 parent={c:p for p,n in enumerate(g.d['nodes']) for c in n.get('children',[])}
 hierarchy={n:(g.d['nodes'][parent[i]]['name'] if i in parent else None) for n,i in zip(names,skin['joints'])}
 return names,hierarchy,g.acc(skin['inverseBindMatrices'])
on,oh,ob=skeleton(old);ln,lh,lb=skeleton(lib)
checks['original_33_bone_names_and_hierarchy_preserved']=on==ln and oh==lh and len(ln)==33
details['inverse_bind_max_error']=float(np.max(np.abs(ob-lb)))
checks['inverse_bind_matrices_preserved']=details['inverse_bind_max_error']<2e-6
geoerror=0;geook=True
for name,node in old.nodes.items():
 if 'mesh' not in node: continue
 newnode=lib.nodes.get(name)
 if not newnode or 'mesh' not in newnode:geook=False;continue
 pa=old.d['meshes'][node['mesh']]['primitives'];pb=lib.d['meshes'][newnode['mesh']]['primitives']
 if len(pa)!=len(pb):geook=False;continue
 for a,b in zip(pa,pb):
  for key in ['POSITION','NORMAL','JOINTS_0','WEIGHTS_0']:
   av=old.acc(a['attributes'][key]);bv=lib.acc(b['attributes'][key])
   if av.shape!=bv.shape: geook=False;continue
   geoerror=max(geoerror,float(np.max(np.abs(av.astype(float)-bv))))
checks['original_hero_meshes_and_weights_preserved']=geook and geoerror<2e-6
details['hero_attribute_max_error']=geoerror
oldclips=old.clips();newclips=lib.clips()
checks['exactly_11_original_plus_30_new_clips']=set(newclips)==set(oldclips)|{s['name'] for s in cat['new_clips']} and len(newclips)==41
checks['single_shared_walk_and_run']=sum('Walk' in n for n in newclips)==1 and sum('Run' in n for n in newclips)==1 and not any(('Screen' in n or 'Turn' in n) for n in newclips)
commonerr=0;commonok=True
for n,a in oldclips.items():
 x=old.channels(a);y=lib.channels(newclips[n]);duration=max(t[0][-1]-t[0][0] for t in x.values());ts=np.linspace(0,duration,37)
 for key in x:
  av=sample(x[key],ts);bv=sample(y[key],ts)
  if key[1]=='rotation':
   av/=np.linalg.norm(av,axis=1)[:,None];bv/=np.linalg.norm(bv,axis=1)[:,None];err=np.max(1-np.abs(np.sum(av*bv,axis=1)))
  else:err=np.max(np.abs(av-bv))
  commonerr=max(commonerr,float(err))
checks['original_11_animation_poses_preserved']=commonerr<2e-5;details['common_clip_sample_error']=commonerr
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
  events &=all(0<=e['time_seconds']<=duration+1e-5 and abs(e['time_seconds']-(e['frame']-1)/60)<1e-5 for e in s['events'])
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
checks.update(eight_glbs_readable=True,equipped_glbs_match_original_skin_and_expected_parts=bool(equipped),all_30_clips_have_full_33_bone_channels=bool(channelcoverage),all_30_clip_durations_match_catalog=bool(durations),all_30_clip_values_finite=bool(finite),root_translation_and_yaw_static=bool(rootstatic),loop_endpoints_match=bool(loops),event_times_valid=bool(events),bow_string_limb_and_arrow_morph_channels=bool(bowchannels))
re=json.loads((P/'reimport_validation.json').read_text('utf-8'));stats=[s for group in re.values() for s in group.values()]
checks['all_30_actual_export_clips_reimported']=len(stats)==30
details['minimum_boot_z_m']=min(s['minimum_boot_z_m'] for s in stats)
details['maximum_foot_anchor_deviation_m']=max(s['max_foot_anchor_deviation_m'] for s in stats)
details['maximum_support_grip_gap_m']=max(s['maximum_support_grip_gap_m'] for s in stats)
details['maximum_bow_draw_hand_to_nock_m']=max(s['maximum_bow_draw_hand_to_nock_m'] for s in stats)
details['maximum_bow_hand_step_per_60fps_frame_m']=max(s['maximum_hand_step_per_60fps_frame_m'] for s in re['bow'].values())
checks['boot_floor_and_foot_lock_within_1mm']=details['minimum_boot_z_m']>-.001 and details['maximum_foot_anchor_deviation_m']<.001
checks['two_handed_grips_within_1mm']=details['maximum_support_grip_gap_m']<.001
checks['bow_draw_hand_matches_animated_nock_within_1mm']=details['maximum_bow_draw_hand_to_nock_m']<.001
checks['bow_transitions_no_single_frame_hand_jump_over_5cm']=details['maximum_bow_hand_step_per_60fps_frame_m']<.05
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
 return error
for s in cat['new_clips']:
 if s['weapon_kind']=='bow':continue
 if s['start_pose']=='Ready':
  prefix='Hammer' if s['weapon_kind']=='warhammer' else s['weapon_kind'].title()
  transition_error=max(transition_error,endpoint_error(s['name'],0,'HERO_'+prefix+'_Ready',0),endpoint_error(s['name'],-1,'HERO_'+prefix+'_Ready',0))
for a,which,b,other in [('HERO_Bow_Nock',0,'HERO_Bow_Ready',0),('HERO_Bow_Nock',-1,'HERO_Bow_Draw',0),('HERO_Bow_Draw',-1,'HERO_Bow_Aim',0),('HERO_Bow_Release',0,'HERO_Bow_Aim',0),('HERO_Bow_Release',-1,'HERO_Bow_Ready',0),('HERO_Bow_Lower',0,'HERO_Bow_Aim',0),('HERO_Bow_Lower',-1,'HERO_Bow_Ready',0)]:transition_error=max(transition_error,endpoint_error(a,which,b,other))
details['attack_and_bow_transition_pose_error']=transition_error
checks['attack_ready_and_bow_chain_endpoints_match']=transition_error<1e-5
revision=P/'revisions/20261105_before_hammer_grip'
if revision.exists():
 previous=GLB(revision/'hero_weapon_animation_library.glb');previous_clips=previous.clips();unchanged_error=0
 for name,a in previous_clips.items():
  if name.startswith('HERO_Hammer_'):continue
  ac=previous.channels(a);bc=lib.channels(newclips[name]);duration=max(v[0][-1]-v[0][0] for v in ac.values());ts=np.linspace(0,duration,43)
  for key in ac:
   x=sample(ac[key],ts);y=sample(bc[key],ts)
   if key[1]=='rotation':
    x/=np.linalg.norm(x,axis=1)[:,None];y/=np.linalg.norm(y,axis=1)[:,None];err=np.max(1-np.abs(np.sum(x*y,axis=1)))
   else:err=np.max(np.abs(x-y))
   unchanged_error=max(unchanged_error,float(err))
 details['unchanged_37_clip_sample_error']=unchanged_error
 checks['other_37_library_clips_preserved']=unchanged_error<2e-5
 old_files={f['file']:f['sha256'] for f in json.loads((revision/'delivery_files.json').read_text('utf-8'))['files']}
 checks['other_six_equipped_glbs_byte_identical']=all(hashlib.sha256((P/e['file']).read_bytes()).hexdigest()==old_files[e['file']] for e in cat['equipment'] if e['weapon_kind']!='warhammer')
result=dict(date='2026-11-05',all_checks_passed=all(checks.values()),checks=checks,metrics=details,clips=newdetails,scope='Exported GLB binary structure, original-source hashes, bind/geometry comparison, motion channel sampling, full 60fps reimported bone/boot/hand checks. No game execution or performance check.')
(P/'validation.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(dict(checks=checks,metrics=details),indent=2));assert result['all_checks_passed'], 'See validation.json'
