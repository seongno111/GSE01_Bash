import bpy,json,math,sys
from pathlib import Path
from mathutils import Vector
P=Path(__file__).resolve().parents[1]
cat=json.loads((P/'animation_catalog.json').read_text('utf-8'))
quick='quick' in sys.argv
frames='frames' in sys.argv
poses='poses' in sys.argv
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
    s=bpy.context.scene;s.render.engine='CYCLES';s.cycles.samples=12;s.cycles.use_denoising=True
    s.render.resolution_x=640 if quick else 480;s.render.resolution_y=s.render.resolution_x;s.render.resolution_percentage=100
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

for eq in cat['equipment']:
    kind=eq['weapon_kind'];bpy.ops.wm.read_factory_settings(use_empty=True)
    if kind_filter and kind!=kind_filter:continue
    bpy.context.scene.render.fps=60
    bpy.ops.import_scene.gltf(filepath=str(P/eq['file']))
    rig=next(o for o in bpy.data.objects if o.type=='ARMATURE')
    meshes=[o for o in bpy.data.objects if o.type=='MESH' and any(m.type=='ARMATURE' and m.object==rig for m in o.modifiers)]
    print('IMPORTED',kind,rig.name,[(t.name,t.strips[0].action.name,list(t.strips[0].action.frame_range)) for t in rig.animation_data.nla_tracks],flush=True)
    if not quick and not frames and not poses:
        stats={}
        for spec in [s for s in cat['new_clips'] if s['weapon_kind']==kind]:
            activate(spec['name']);minimum=1e9;footmove=0;supporterr=0;arrowerr=0;rootmove=0;first=None;last=None;last_grips=None;max_grip_step=0;bow_weight_error=0
            for f in range(0,spec['end_frame']):
                bpy.context.scene.frame_set(f);bpy.context.view_layer.update()
                root=rig.matrix_world@rig.pose.bones['root'].matrix
                rootmove=max(rootmove,root.translation.length)
                grips=[rig.pose.bones['weapon_'+side].head.copy() for side in ['l','r']]
                if last_grips:max_grip_step=max(max_grip_step,max((a-b).length for a,b in zip(grips,last_grips)))
                last_grips=grips
                for side in ['l','r']:
                    pb=rig.pose.bones['foot_'+side];p=rig.matrix_world@pb.head
                    footmove=max(footmove,(p-Vector((.14 if side=='l' else -.14,0,.13))).length)
                for o in meshes:
                    if not o.name.startswith('HERO_FOOT_'):continue
                    evaluated=o.evaluated_get(bpy.context.evaluated_depsgraph_get());me=evaluated.to_mesh()
                    minimum=min(minimum,min((evaluated.matrix_world@v.co).z for v in me.vertices));evaluated.to_mesh_clear()
                if kind in ('spear','warhammer'):
                    r=rig.pose.bones['weapon_r'];l=rig.pose.bones['weapon_l']
                    # Derive current weapon frame from the exact rest-bone rotation.
                    R=r.matrix.to_3x3() @ rig.data.bones['weapon_r'].matrix_local.to_3x3().inverted()
                    expected=r.head+R@Vector((0,0,.32 if kind=='spear' else eq.get('support_offset_weapon_local_z_m',-.22)))
                    supporterr=max(supporterr,(l.head-expected).length)
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
            stats[spec['name']]=dict(minimum_boot_z_m=minimum,max_foot_anchor_deviation_m=footmove,max_root_translation_m=rootmove,maximum_support_grip_gap_m=supporterr,maximum_bow_draw_hand_to_nock_m=arrowerr,maximum_hand_step_per_60fps_frame_m=max_grip_step,loop_matrix_error=looperr)
        output[kind]=stats
    if quick or frames or poses:
        stage(kind)
        name=clip_override or targets[kind];activate(name)
        spec=next(s for s in cat['new_clips'] if s['name']==name)
        frame=round((.8 if kind=='bow' else spec['events'][0]['time_seconds'])*60)
        if poses:
            for sp in [s for s in cat['new_clips'] if s['weapon_kind']==kind]:
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
        print('RENDERED',kind,flush=True)

if output:(P/'reimport_validation.json').write_text(json.dumps(output,indent=2),encoding='utf-8')
