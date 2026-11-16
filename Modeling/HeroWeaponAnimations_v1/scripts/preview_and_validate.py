import bpy,json,math,sys
from pathlib import Path
from mathutils import Vector
PACKAGE=Path(__file__).resolve().parents[1]
P=next((Path(a.split('=',1)[1]).resolve() for a in sys.argv if a.startswith('out=')),PACKAGE)
sys.path.insert(0,str(PACKAGE/'scripts'))
from skill_motion import skill_time
cat=json.loads((P/'animation_catalog.json').read_text('utf-8'))
quick='quick' in sys.argv
frames='frames' in sys.argv
poses='poses' in sys.argv
skill_frames='skill-frames' in sys.argv
model_views='model-views' in sys.argv
kind_filter=next((a.split('=',1)[1] for a in sys.argv if a.startswith('kind=')),None)
clip_override=next((a.split('=',1)[1] for a in sys.argv if a.startswith('clip=')),None)
output={}
targets={'staff':'HERO_Staff_CastBolt','mace':'HERO_Mace_Strike','axe':'HERO_Axe_Chop','warhammer':'HERO_Hammer_Smash','spear':'HERO_Spear_Thrust','dagger':'HERO_Dagger_Stab','bow':'HERO_Bow_Draw'}

def activate(name):
    for obj in list(bpy.data.objects):
        blocks=[obj]+([obj.data.shape_keys] if obj.type=='MESH' and obj.data.shape_keys else [])
        for block in blocks:
            ad=block.animation_data
            if not ad:continue
            for tr in ad.nla_tracks:
                tr.mute=True
                if tr.name==name:
                    strip=tr.strips[0];ad.action=strip.action;ad.action_slot=strip.action_slot

def stage(kind):
    s=bpy.context.scene;s.render.engine='CYCLES';s.cycles.samples=6 if frames or skill_frames else 12;s.cycles.use_denoising=True
    s.render.resolution_x=640 if quick or model_views else 300 if frames or skill_frames else 480
    s.render.resolution_y=720 if model_views else s.render.resolution_x;s.render.resolution_percentage=100
    s.render.image_settings.file_format='PNG';s.render.film_transparent=False
    s.world=bpy.data.worlds.new('StudioWorld');s.world.use_nodes=True
    s.world.node_tree.nodes['Background'].inputs['Color'].default_value=(.19,.22,.26,1)
    s.world.node_tree.nodes['Background'].inputs['Strength'].default_value=.55
    s.view_settings.view_transform='AgX';s.view_settings.look='AgX - Medium High Contrast';s.view_settings.exposure=.65
    def area(name,pos,power,color,size):
        d=bpy.data.lights.new(name,'AREA');o=bpy.data.objects.new(name,d);s.collection.objects.link(o);o.location=pos;d.energy=power;d.color=color;d.shape='DISK';d.size=size;o.rotation_euler=(Vector((0,-.1,1))-o.location).to_track_quat('-Z','Y').to_euler()
    area('Warm key',(-3,-4,6),650,(1,.80,.58),4)
    area('Cool fill',(4,-1,3),500,(.55,.7,1),4)
    area('Rim',(1,4,5),900,(1,.57,.28),3)
    mat=bpy.data.materials.new('StudioSlate');mat.diffuse_color=(.055,.065,.08,1);mat.use_nodes=True;mat.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value=(.055,.065,.08,1)
    bpy.ops.mesh.primitive_plane_add(size=200,location=(0,0,-.012));bpy.context.object.data.materials.append(mat)
    d=bpy.data.cameras.new('PreviewCamera');c=bpy.data.objects.new('PreviewCamera',d);s.collection.objects.link(c);s.camera=c
    c.location=(-4,-6,3.0);target=Vector((0,-.18,1.22));c.rotation_euler=(target-c.location).to_track_quat('-Z','Y').to_euler();d.type='ORTHO';d.ortho_scale=3.75 if kind in ('staff','spear') else 3.15

if model_views:
    for name,file,position in [
        ('hero_before',PACKAGE/'revisions/20261110_before_five_head/hero_rigged.glb',(-2.4,-6,2.35)),
        ('hero_front',P/'hero_rigged.glb',(-2.4,-6,2.35)),
        ('hero_back',P/'hero_rigged.glb',(2.9,5.6,2.55)),
        ('hero_side',P/'hero_rigged.glb',(-6,-1.3,2.25)),
    ]:
        bpy.ops.wm.read_factory_settings(use_empty=True)
        bpy.context.scene.render.fps=60
        bpy.ops.import_scene.gltf(filepath=str(file))
        activate('HERO_Idle')
        stage('common')
        s=bpy.context.scene
        s.frame_set(0)
        s.camera.location=position
        s.camera.rotation_euler=(Vector((0,-.065,1.00))-s.camera.location).to_track_quat('-Z','Y').to_euler()
        s.camera.data.ortho_scale=2.38
        s.render.filepath=str(P/'previews'/(name+'.png'))
        bpy.ops.render.render(write_still=True)
    sys.exit(0)

for eq in [{'weapon_kind':'common','file':'hero_rigged.glb'}]+cat['equipment'] if P!=PACKAGE else cat['equipment']:
    kind=eq['weapon_kind'];bpy.ops.wm.read_factory_settings(use_empty=True)
    if kind_filter and kind not in kind_filter.split(','):continue
    bpy.context.scene.render.fps=60
    bpy.ops.import_scene.gltf(filepath=str(P/eq['file']))
    rig=next(o for o in bpy.data.objects if o.type=='ARMATURE')
    meshes=[o for o in bpy.data.objects if o.type=='MESH' and any(m.type=='ARMATURE' and m.object==rig for m in o.modifiers)]
    print('IMPORTED',kind,rig.name,[(t.name,t.strips[0].action.name,list(t.strips[0].action.frame_range)) for t in rig.animation_data.nla_tracks],flush=True)
    if not quick and not frames and not poses and not skill_frames:
        stats={}
        clips=cat['common_clips'] if kind=='common' else [s for s in cat['new_clips'] if s['weapon_kind']==kind]
        for spec in clips:
            activate(spec['name']);minimum=1e9;footmove=0;supporterr=0;arrowerr=0;rootmove=0;first=None;last=None;last_grips=None;max_grip_step=0
            min_mesh_z=1e9; limb_gap=0; wrist_bend=0; max_boot_lift=0
            elbow_speed=0; last_elbows=None
            previous_sample=None
            contact_speed=0
            settle_speed=0
            sample_frames=set(range(0,spec['end_frame']))
            authored_frames=set(sample_frames)
            if 'skill_label' in spec:
                end=spec['duration_seconds']*60
                sample_frames={f for f in sample_frames if f<=end}
                sample_frames.update([end]+[e['time_seconds']*60 for e in spec['events']])
                divisions=spec.get('sample_divisions',1)
                authored_frames={skill_time(spec,1+j/divisions)*60 for j in range(1+(spec['end_frame']-1)*divisions)}
                sample_frames.update(authored_frames)
                if divisions>1:
                    authored=sorted(authored_frames)
                    sample_frames.update((a+b)/2 for a,b in zip(authored,authored[1:]))
            contact_floor={}
            baked_grip_error=0
            for sample_frame in sorted(sample_frames):
                f=int(sample_frame)
                bpy.context.scene.frame_set(f,subframe=sample_frame-f);bpy.context.view_layer.update()
                root=rig.matrix_world@rig.pose.bones['root'].matrix
                rootmove=max(rootmove,root.translation.length)
                grips=[rig.pose.bones['weapon_'+side].head.copy() for side in ['l','r']]
                if last_grips:
                    step=max((a-b).length for a,b in zip(grips,last_grips))
                    max_grip_step=max(max_grip_step,step)
                    elapsed=(sample_frame-previous_sample)/60
                    if elapsed>.001:
                        time=sample_frame/60
                        if any(abs(time-e['time_seconds'])<.11 for e in spec['events']):
                            contact_speed=max(contact_speed,step/elapsed)
                        if time>spec['duration_seconds']-.09:
                            settle_speed=max(settle_speed,step/elapsed)
                previous_sample=sample_frame
                last_grips=grips
                elbows=[rig.pose.bones['forearm_'+side].head.copy() for side in ['l','r']]
                if last_elbows:elbow_speed=max(elbow_speed,max((a-b).length for a,b in zip(elbows,last_elbows)))
                last_elbows=elbows
                for side in ['l','r']:
                    pb=rig.pose.bones['foot_'+side];p=rig.matrix_world@pb.head
                    if 'ready_foot_anchors_blender_xy' in eq and not (side=='l' and spec['name'].endswith(('_Thrust','_Stab','SkillLongThrust'))):
                        x,y=eq['ready_foot_anchors_blender_xy'][0 if side=='l' else 1]
                        footmove=max(footmove,math.hypot(p.x-x,p.y-y))
                    if kind=='warhammer':
                        fore=rig.pose.bones['forearm_'+side];hand=rig.pose.bones['hand_'+side]
                        wrist_bend=max(wrist_bend,math.degrees((fore.tail-fore.head).angle(hand.tail-hand.head)))
                    for parent,child in [('upper_arm_','forearm_'),('forearm_','hand_'),('thigh_','shin_'),('shin_','foot_')]:
                        limb_gap=max(limb_gap,(rig.pose.bones[parent+side].tail-rig.pose.bones[child+side].head).length)
                for o in meshes:
                    evaluated=o.evaluated_get(bpy.context.evaluated_depsgraph_get());me=evaluated.to_mesh()
                    low=min((evaluated.matrix_world@v.co).z for v in me.vertices)
                    min_mesh_z=min(min_mesh_z,low)
                    if 'skill_label' in spec and kind in ('staff','warhammer') and o.name.startswith('WPN_'):
                        for event in spec['events']:
                            if abs(sample_frame/60-event['time_seconds'])<1e-6:
                                contact_floor[str(event['time_seconds'])]=low
                    if o.name.startswith('HERO_FOOT_'):
                        minimum=min(minimum,low);max_boot_lift=max(max_boot_lift,low)
                    evaluated.to_mesh_clear()
                if kind in ('spear','warhammer'):
                    r=rig.pose.bones['weapon_r'];l=rig.pose.bones['weapon_l']
                    # Derive current weapon frame from the exact rest-bone rotation.
                    R=r.matrix.to_3x3() @ rig.data.bones['weapon_r'].matrix_local.to_3x3().inverted()
                    expected=r.head+R@Vector((0,0,.32 if kind=='spear' else eq.get('support_offset_weapon_local_z_m',-.22)))
                    gap=(l.head-expected).length
                    supporterr=max(supporterr,gap)
                    if sample_frame in authored_frames:baked_grip_error=max(baked_grip_error,gap)
                if kind=='bow' and spec['name'] in ['HERO_Bow_Draw','HERO_Bow_Aim']:
                    left=rig.pose.bones['weapon_l'];right=rig.pose.bones['weapon_r']
                    body=next(o for o in meshes if o.name.startswith('WPN_Bow_') and '__Body' in o.name)
                    draw=body.data.shape_keys.key_blocks['Draw'].value
                    R=left.matrix.to_3x3()@rig.data.bones['weapon_l'].matrix_local.to_3x3().inverted()
                    expected=left.head+R@Vector((0,.2+.29*draw,0))
                    arrowerr=max(arrowerr,(right.head-expected).length)
                matrices={p.name:p.matrix.copy() for p in rig.pose.bones}
                if first is None:first=matrices
                last=matrices
            looperr=max(abs(first[n][a][b]-last[n][a][b]) for n in first for a in range(4) for b in range(4)) if spec['loop'] else None
            stats[spec['name']]=dict(minimum_boot_z_m=minimum,minimum_visible_mesh_z_m=min_mesh_z,maximum_boot_lift_m=max_boot_lift,max_foot_anchor_deviation_m=footmove,max_root_translation_m=rootmove,maximum_support_grip_gap_m=supporterr,maximum_bow_draw_hand_to_nock_m=arrowerr,maximum_hand_step_per_60fps_frame_m=max_grip_step,maximum_elbow_step_per_60fps_frame_m=elbow_speed,maximum_chain_gap_m=limb_gap,maximum_hammer_wrist_bend_degrees=wrist_bend,loop_matrix_error=looperr)
            if contact_floor:stats[spec['name']]['weapon_floor_at_contacts_m']=contact_floor
            stats[spec['name']]['baked_support_grip_gap_m']=baked_grip_error
            stats[spec['name']]['peak_contact_grip_speed_m_per_s']=contact_speed
            stats[spec['name']]['peak_final_settle_grip_speed_m_per_s']=settle_speed
        output[kind]=stats
    if quick or frames or poses or skill_frames:
        stage(kind)
        if skill_frames:
            kind_key='longsword' if kind=='common' else kind
            for sp in [s for s in cat['skills'] if s['weapon_kind']==kind_key]:
                activate(sp['name'])
                duration=sp['duration_seconds']
                count=max(9,round(duration*12)+1)
                times=[duration*j/(count-1) for j in range(count)]
                for event in sp['events']:
                    time=event['time_seconds']
                    index=min(range(count),key=lambda i:abs(times[i]-time))
                    times[index]=time
                folder=P/'previews'/'_frames'/sp['name'];folder.mkdir(parents=True,exist_ok=True)
                for j,time in enumerate(times):
                    frame=time*60;f=int(frame)
                    bpy.context.scene.frame_set(f,subframe=frame-f)
                    path=folder/f'{j:03d}.png'
                    bpy.context.scene.render.filepath=str(path)
                    bpy.ops.render.render(write_still=True)
                    if abs(time-sp['events'][0]['time_seconds'])<1e-6:
                        (P/'previews'/(sp['name']+'.png')).write_bytes(path.read_bytes())
            continue
        if kind=='common':
            if poses:
                for sp in [s for s in cat['common_clips'] if not clip_override or s['name']==clip_override]:
                    activate(sp['name'])
                    if 'skill_label' in sp and (P/'previews'/(sp['name']+'.png')).exists():continue
                    marker={'HERO_AttackLongsword':24,'HERO_Death':71,'HERO_GetUp':46,'HERO_Hit':5,'HERO_LampInspect':55,'HERO_Walk':42,'HERO_Run':12}.get(sp['name'],(sp['end_frame']-1)//2)
                    bpy.context.scene.frame_set(marker)
                    bpy.context.scene.render.filepath=str(P/'previews'/(sp['name']+'.png'))
                    bpy.ops.render.render(write_still=True)
                continue
            motion=([clip_override] if clip_override else
                    ['HERO_Walk','HERO_Run','HERO_AttackLongsword','HERO_Hit','HERO_Death','HERO_GetUp','HERO_LampInspect'])
            for name in motion:
                activate(name)
                sp=next(s for s in cat['common_clips'] if s['name']==name)
                fd=P/'previews'/'_frames'/name;fd.mkdir(parents=True,exist_ok=True)
                count=max(6,round(sp['duration_seconds']*12))
                for j in range(count):
                    u=j/count if sp['loop'] else j/(count-1)
                    bpy.context.scene.frame_set(round(u*(sp['end_frame']-1)))
                    bpy.context.scene.render.filepath=str(fd/f'{j:03d}.png')
                    bpy.ops.render.render(write_still=True)
            continue
        name=clip_override or targets[kind];activate(name)
        spec=next(s for s in cat['new_clips'] if s['name']==name)
        frame=round((.8 if kind=='bow' else spec['events'][0]['time_seconds'])*60)
        if poses:
            for sp in [s for s in cat['new_clips'] if s['weapon_kind']==kind]:
                if 'skill_label' in sp and (P/'previews'/(sp['name']+'.png')).exists():continue
                activate(sp['name'])
                f=round(sp['events'][0]['time_seconds']*60) if sp['events'] and sp['name']!='HERO_Bow_Nock' else sp['end_frame']-1
                bpy.context.scene.frame_set(f);bpy.context.scene.render.filepath=str(P/'previews'/(sp['name']+'.png'));bpy.ops.render.render(write_still=True)
        elif quick:
            bpy.context.scene.frame_set(frame);bpy.context.scene.render.filepath=str(P/'previews'/('pose_'+kind+'.png'));bpy.ops.render.render(write_still=True)
        else:
            fd=P/'previews'/'_frames'/(kind if not clip_override else kind+'_uppercut');fd.mkdir(parents=True,exist_ok=True)
            # Bow shows complete sequence, other weapons show their primary attack.
            if kind=='bow':
                sequence=[('HERO_Bow_Nock',.75),('HERO_Bow_Draw',.8),('HERO_Bow_Aim',.35),('HERO_Bow_Release',.55)]
            else:sequence=[(name,spec['duration_seconds'])]
            idx=0
            for clip,duration in sequence:
                activate(clip)
                count=max(3,round(duration*12))
                for j in range(count):
                    bpy.context.scene.frame_set(round(j/count*duration*60));bpy.context.scene.render.filepath=str(fd/(str(idx).zfill(3)+'.png'));bpy.ops.render.render(write_still=True);idx+=1
            if kind=='warhammer' and not clip_override:
                name='HERO_Hammer_Uppercut';activate(name)
                fd=P/'previews'/'_frames'/'warhammer_uppercut';fd.mkdir(parents=True,exist_ok=True)
                for j in range(20):
                    bpy.context.scene.frame_set(round(j/19*99))
                    bpy.context.scene.render.filepath=str(fd/f'{j:03d}.png')
                    bpy.ops.render.render(write_still=True)
        print('RENDERED',kind,flush=True)

if output:
    report=P/'reimport_validation.json'
    if kind_filter and report.exists():
        output={**json.loads(report.read_text('utf-8')),**output}
    report.write_text(json.dumps(output,indent=2),encoding='utf-8')
