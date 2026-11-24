"""Rebuild modular NPCs and monster detail, then retarget the existing actions."""
import json
import math
import sys
import struct
from pathlib import Path

import bpy
from mathutils import Matrix, Quaternion, Vector

HERE=Path(__file__).resolve().parents[1]
BASE=HERE.parent
sys.path.insert(0,str(HERE/'scripts'))
from mesh_quality import Mesh, bevel, npc_parts, palette

PACK=next((a.split('=',1)[1] for a in sys.argv if a.startswith('pack=')),'LowPolyKit_v3')
OUT=HERE/'staged'/PACK
OUT.mkdir(parents=True,exist_ok=True)
for sub in ('npc','monsters','previews'):(OUT/sub).mkdir(exist_ok=True)
SOURCE_DIR=HERE/'backups'/PACK
if not SOURCE_DIR.exists():SOURCE_DIR=BASE/PACK
SOURCE=SOURCE_DIR/('lowpoly_kit_v3.blend' if PACK=='LowPolyKit_v3' else 'monsters_v1.blend')
bpy.ops.wm.open_mainfile(filepath=str(SOURCE))
scene=bpy.context.scene
scene.render.fps=60
pal=palette()
catalog=json.loads((SOURCE_DIR/('asset_catalog.json' if PACK=='LowPolyKit_v3' else 'monster_catalog.json')).read_text('utf-8'))
animation=json.loads((SOURCE_DIR/'animation_catalog.json').read_text('utf-8')) if PACK=='LowPolyKit_v3' else None
report={'date':'2026-11-23','characters':{},'source':str(SOURCE.relative_to(BASE))}
for obj in bpy.data.objects:
    if obj.name.startswith('DISPLAY'):
        obj.hide_set(True)


def reset(rig):
    rig.animation_data_create()
    rig.animation_data.action=None
    for track in rig.animation_data.nla_tracks:track.mute=True
    for bone in rig.pose.bones:
        bone.rotation_mode='QUATERNION'
        bone.matrix_basis=Matrix.Identity(4)
    bpy.context.view_layer.update()


def world(name,pos,rot,rig,scale=None):
    m=rot.to_matrix().to_4x4()
    if scale:
        for j in range(3):m.col[j]*=scale[j]
    m.translation=pos
    rig.pose.bones[name].matrix=m


def skeleton(rig):
    return {b.name:b.matrix_local.copy() for b in rig.data.bones}


def tailor_bind(rig):
    old=skeleton(rig)
    ends={b.name:b.tail_local.copy() for b in rig.data.bones}
    height=rig.data.bones['head'].tail_local.z
    scale=height/1.8
    h={
        'root':(0,0,0),'hips':(0,.018,.934),'spine':(0,.015,1.035),
        'chest':(0,.009,1.226),'neck':(0,-.014,1.437),'head':(0,-.019,1.524),
        'jaw':(0,-.092,1.615),'skirt_front':(0,-.113,.948),
        'skirt_back':(0,.154,.975),'skirt_l':(.154,.046,.958),
        'skirt_r':(-.154,.046,.958),'tail':(0,.151,.945),'tail_tip':(0,.426,.77),
    }
    for side,sign in [('l',1),('r',-1)]:
        h.update({
            'clavicle_'+side:(0,-.004,1.416),'upper_arm_'+side:(sign*.224,0,1.405),
            'forearm_'+side:(sign*.300,.016,1.133),'hand_'+side:(sign*.335,.002,.878),
            'thigh_'+side:(sign*.101,.013,.879),'shin_'+side:(sign*.111,-.011,.486),
            'foot_'+side:(sign*.118,0,.113),'toe_'+side:(sign*.118,-.156,.047),
        })
        h['weapon_'+side]=Vector(h['hand_'+side])+(old['weapon_'+side].translation-old['hand_'+side].translation)*(.78/scale)
    h={n:Vector(p)*scale for n,p in h.items()}
    for b in rig.data.bones:
        if b.name not in h:
            # Extra wings and cast sockets follow their original parent attachment.
            p=b.parent.name
            ratio=.80 if b.name.startswith('wing') else .78
            h[b.name]=h[p]+(b.head_local-old[p].translation)*ratio
    child={'hips':'spine','spine':'chest','chest':'neck','neck':'head','tail':'tail_tip'}
    for side in 'lr':
        for a,b in [('clavicle','upper_arm'),('upper_arm','forearm'),('forearm','hand'),
                    ('thigh','shin'),('shin','foot'),('foot','toe')]:
            child[a+'_'+side]=b+'_'+side
    bpy.context.view_layer.objects.active=rig
    rig.hide_set(False);rig.select_set(True)
    bpy.ops.object.mode_set(mode='EDIT')
    for b in rig.data.edit_bones:b.use_connect=False
    for b in rig.data.edit_bones:
        n=b.name
        b.head=h[n]
        if n in child:b.tail=h[child[n]]
        elif n=='head':b.tail=Vector((0,-.019,1.84))*scale
        else:
            factor=.78 if n.startswith(('hand','weapon','jaw')) else .85
            b.tail=h[n]+(ends[n]-old[n].translation)*factor
        b.align_roll(old[n].to_3x3().col[2])
    bpy.ops.object.mode_set(mode='OBJECT')
    bpy.context.view_layer.update()
    return old,skeleton(rig),scale


def skin_remap(objects,old,new):
    for o in objects:
        # Keep linked display meshes on the same revised data after tailoring.
        group={g.index:g.name for g in o.vertex_groups}
        transforms={}
        for name in old:
            width=.74
            length=bpy.data.objects[o.parent.name].data.bones[name].length if o.parent and o.parent.type=='ARMATURE' else 1
            if name=='head' or name=='jaw':
                length_scale=.608333
                width=.59
            else:
                old_length=original_lengths[name]
                length_scale=length/old_length
                if name.startswith(('hand','weapon')):length_scale=width=.78
                if name.startswith('foot'):length_scale=width=.79
                if name.startswith('wing'):width=.86
            stretch=Matrix.Diagonal((width,length_scale,width,1))
            transforms[name]=new[name]@stretch@old[name].inverted()
        for v in o.data.vertices:
            p=v.co.copy()
            weights=[(group[g.group],g.weight) for g in v.groups if g.weight>0 and group[g.group] in transforms]
            if weights:
                v.co=sum((transforms[n]@p*w for n,w in weights),Vector())/sum(w for _,w in weights)
        o.data.update()


def add_humanoid_detail(rig,objects,scale):
    def b(n):return rig.data.bones[n].head_local.copy()
    body=next(o for o in objects if o.name.endswith('_Body') or o.name.startswith('MON_0'))
    name=body.name
    mesh=Mesh(pal)
    undead='Hollow' in name
    demon='Azar' in name or 'Mutated' in name or 'Echo' in name
    plate='Paladin' in name or 'Warden' in name or 'Raider' in name
    for side,sign in [('l',1),('r',-1)]:
        for bone in ('upper_arm','forearm','thigh','shin'):
            n=bone+'_'+side
            a=rig.data.bones[n].head_local.copy()
            end=rig.data.bones[n].tail_local.copy()
            radius={'upper_arm':.077,'forearm':.057,'thigh':.085,'shin':.051}[bone]*scale
            color='iron' if plate else 'flesh' if demon else 'bone'
            mesh.limb(a,end,[(.02,radius*.97,radius*.91,0),(.23,radius*1.1,radius,0),
                           (.53,radius*.88,radius*.89,0),(.88,radius*.69,radius*.72,0)],
                      [n]*4,[color,color,'dark'],12)
            if plate:
                for t in (.18,.43):
                    c=a.lerp(end,t)
                    mesh.limb(c,c+(end-a)*.08,[(0,radius*1.18,radius*1.12,0),(1,radius*1.12,radius*1.07,0)],
                              [n]*2,['edge','iron'],12)
        shoulder=b('upper_arm_'+side)
        if plate or demon:
            mesh.ellipsoid(shoulder+Vector((sign*.016,0,.008))*scale,
                           (.113*scale,.114*scale,.080*scale),'upper_arm_'+side,'iron' if plate else 'flesh',12,5)
            for j in range(3):
                a=shoulder+Vector((sign*(.028+j*.026),.011,.047-j*.012))*scale
                mesh.limb(a,a+Vector((sign*.065,.012,.044))*scale,
                          [(0,.024*scale,.022*scale,0),(1,.003*scale,.003*scale,0)],
                          ['upper_arm_'+side]*2,['horn' if demon else 'edge'],7)
        hand=b('hand_'+side)
        for j in range(4):
            a=hand+Vector(((j-1.5)*.016,-.024,-.071))*scale
            mesh.limb(a,a+Vector((sign*.004,-.008,-.080 if demon else -.046))*scale,
                      [(0,.009*scale,.010*scale,0),(.55,.008*scale,.009*scale,0),(1,.002*scale,.003*scale,0)],
                      ['hand_'+side]*3,['bone' if undead or demon else 'iron'],6)
    if undead:
        for j in range(5):
            z=(1.16+j*.039)*scale
            width=(.116-j*.011)*scale
            mesh.cord([(-width,-.105*scale,z),(0,-.143*scale,z-.014*scale),(width,-.105*scale,z)],
                      .010*scale,'chest','bone')
        mesh.cord([(0,-.142*scale,1.12*scale),(0,-.148*scale,1.36*scale)],.012*scale,'chest','bone')
    elif plate:
        for z,w in [(1.32,.16),(1.19,.145),(1.08,.144)]:
            mesh.plate([(-w*scale,-.132*scale,(z+.10)*scale),(0,-.162*scale,(z+.12)*scale),
                        (w*scale,-.132*scale,(z+.10)*scale),(w*.80*scale,-.148*scale,z*scale),
                        (0,-.166*scale,(z-.02)*scale),(-w*.80*scale,-.148*scale,z*scale)],
                       .008*scale,'chest' if z>1.20 else 'spine','iron','edge')
    elif demon:
        for j in range(5):
            width=(.19-j*.018)*scale
            z=(1.37-j*.055)*scale
            for sign in (-1,1):
                mesh.cord([(sign*.018*scale,-.151*scale,z),(sign*width,-.101*scale,z+.025*scale)],
                          .010*scale,'chest','horn')
    else:
        for sign in (-1,1):
            mesh.cord([(sign*.11*scale,-.10*scale,1.40*scale),(sign*.082*scale,-.131*scale,1.20*scale),
                       (sign*.074*scale,-.128*scale,.99*scale)],.006*scale,{'chest':.5,'spine':.5},'brass')
    mesh.append(body)
    for o in objects:
        bevel(o,.0023*scale)


def add_animal_detail(rig,objects,key):
    body=next(o for o in objects if o.name.endswith('_Body'))
    mesh=Mesh(pal)
    bones=rig.data.bones
    head=bones['head'].head_local
    neck=bones['neck'].head_local
    bear=key=='BlightBear';stag=key=='ThornStag'
    fur='rot' if bear else 'bone' if stag else 'panel'
    # Contoured mane or shoulder ruff made from tapered overlapping locks.
    for side,sign in [('l',1),('r',-1)]:
        for j in range(7 if not stag else 4):
            t=j/6
            c=head.lerp(neck,min(1,t*.85))
            x=sign*(.10+.075*t)*(1.5 if bear else 1)
            a=c+Vector((x,.03,.025))
            end=a+Vector((sign*(.025+.025*t),.075+.06*t,-.09-.04*t))
            r=(.055 if bear else .037)*(1-.3*t)
            mesh.limb(a,end,[(0,r,r*.63,0),(.44,r*1.08,r*.6,0),(1,.002,.003,0)],
                      ['head' if j<2 else 'neck']*3,[fur,'dark',fur],7)
        for region in ('front','hind'):
            n=region+'_'+side+'_paw';p=bones[n].head_local
            width=.055 if bear else .031
            # Raised keratin hoof ridge or divided toes reads clearly at ground level.
            for j in range(2 if stag else 4):
                x=p.x+(j-(.5 if stag else 1.5))*width
                mesh.ellipsoid((x,p.y-.062,p.z-.025),(width*.47,.073 if bear else .05,.033),n,'horn',8,4)
    if bear:
        cyst=next(o for o in objects if 'BackCysts' in o.name)
        detail=Mesh(pal)
        for j in range(7):
            y=-.34+j*.13
            x=.21*math.sin(j*2.3)
            z=1.35+.06*math.cos(j*1.9)
            detail.ellipsoid((x,y,z),(.052,.049,.068),'cysts','rot',10,5)
            for k in range(3):
                angle=k*math.tau/3
                a=Vector((x+.028*math.cos(angle),y+.028*math.sin(angle),z+.03))
                detail.limb(a,a+Vector((.018*math.cos(angle),.018*math.sin(angle),.06)),
                            [(0,.011,.009,0),(1,.002,.002,0)],['cysts']*2,['bone'],6)
        detail.append(cyst)
    if stag:
        crown=next(o for o in objects if 'ThornCrown' in o.name)
        detail=Mesh(pal)
        for sign in (-1,1):
            points=[(sign*.04,-.63,1.91),(sign*.13,-.61,2.04),(sign*.24,-.59,2.16),
                    (sign*.37,-.56,2.24)]
            detail.cord(points,.016,'antlers','horn')
            for j,p in enumerate(points[1:]):
                p=Vector(p)
                detail.limb(p,p+Vector((sign*.025,-.085,.11-j*.016)),
                            [(0,.015,.013,0),(1,.002,.002,0)],['antlers']*2,['bone'],7)
        detail.append(crown)
    # Add jaw planes and small nasal folds around the species-specific face.
    for sign in (-1,1):
        mesh.cord([head+Vector((sign*.065,-.08,.03)),head+Vector((sign*.11,-.19,-.035)),
                   head+Vector((sign*.078,-.27,-.053))],.011,'head','dark')
    mesh.append(body)
    for o in objects:bevel(o,.0024 if bear else .0017)


def chain(rig,upper,lower,target,pole):
    a=rig.pose.bones[upper].head.copy()
    l1=rig.data.bones[upper].length;l2=rig.data.bones[lower].length
    d=target-a;axis=d.normalized()
    distance=max(abs(l1-l2)+.001,min(d.length,l1+l2-.002))
    end=a+axis*distance
    bend=Vector(pole)-a; bend-=axis*bend.dot(axis)
    if bend.length<.001:bend=Vector((0,-1,.01))
    bend.normalize()
    along=(l1*l1-l2*l2+distance*distance)/(2*distance)
    elbow=a+axis*along+bend*math.sqrt(max(0,l1*l1-along*along))
    for n,start,finish in [(upper,a,elbow),(lower,elbow,end)]:
        rest=rig.data.bones[n]
        q=(rest.tail_local-rest.head_local).normalized().rotation_difference((finish-start).normalized())@rest.matrix_local.to_quaternion()
        world(n,start,q,rig)
        bpy.context.view_layer.update()
    return end


def retarget(rig,old,new,samples,objects,scale):
    actions={}
    modifiers=[mod for obj in objects for mod in obj.modifiers if mod.type=='ARMATURE']
    boots={s:[v.co.copy() for o in objects for v in o.data.vertices
              if any(o.vertex_groups[g.group].name=='foot_'+s and g.weight>.8 for g in v.groups)] for s in 'lr'}
    for old_action,frames in samples:
        old_name=old_action.name
        old_action.name=old_name+'__BeforeQuality'
        act=bpy.data.actions.new(old_name);act.use_fake_user=True
        rig.animation_data.action=act
        last={}
        for f,pose in frames:
            for mod in modifiers:mod.show_viewport=False
            scene.frame_set(f)
            for b in rig.pose.bones:b.matrix_basis=Matrix.Identity(4)
            bpy.context.view_layer.update()
            for bone in rig.data.bones:
                n=bone.name;old_pose=pose[n]
                old_rot=old_pose.to_quaternion()
                rotation=old_rot@old[n].to_quaternion().inverted()@new[n].to_quaternion()
                if bone.parent:
                    parent=bone.parent.name
                    relative=new[parent].inverted()@new[n].translation
                    pos=rig.pose.bones[parent].matrix@relative
                    # Preserve animated cloth offsets, translated into the revised rest axes.
                    old_relative=pose[parent].inverted()@old_pose.translation
                    old_rest=old[parent].inverted()@old[n].translation
                    delta=old_relative-old_rest
                    if delta.length>.00001 and n.startswith('skirt'):
                        pos+=rig.pose.bones[parent].matrix.to_3x3()@(delta*.90)
                else:pos=new[n].translation.copy()
                if n=='hips':
                    parent_old=pose['root']
                    delta=parent_old.inverted()@old_pose.translation-old['hips'].translation
                    displacement=Vector((delta.x*.85,delta.y*1.20,delta.z*1.30))
                    pos=rig.pose.bones['root'].matrix@ (new['hips'].translation+displacement)
                world(n,pos,rotation,rig,old_pose.to_scale())
                bpy.context.view_layer.update()
            death=any(word in old_name for word in ('Death','DeadHold','Spawn','Transform','GetUp'))
            if not death:
                # Retarget foot contacts independently so the longer legs do not skate or sink.
                for side in 'lr':
                    n='foot_'+side
                    old_pos=pose[n].translation
                    q=rig.pose.bones[n].matrix.to_quaternion()
                    foot_rest=new[n].to_quaternion()
                    deformed=[q@foot_rest.inverted()@(v-new[n].translation) for v in boots[side]]
                    minz=min((v.z for v in deformed),default=-new[n].translation.z)
                    target=Vector((old_pos.x*.84,old_pos.y*1.17,max(.001-minz,old_pos.z-new[n].translation.z)))
                    thigh=rig.pose.bones['thigh_'+side].head
                    reach=rig.data.bones['thigh_'+side].length+rig.data.bones['shin_'+side].length-.002
                    horizontal=(target.x-thigh.x)**2+(target.y-thigh.y)**2
                    if horizontal<reach*reach:
                        lower=thigh.z-target.z-math.sqrt(reach*reach-horizontal)
                        if lower>0:
                            hip=rig.pose.bones['hips'].matrix.copy();hip.translation.z-=lower+.001
                            rig.pose.bones['hips'].matrix=hip
                            bpy.context.view_layer.update()
                    end=chain(rig,'thigh_'+side,'shin_'+side,target,(target.x,-.8*scale,.35*scale))
                    world(n,end,q,rig)
                    bpy.context.view_layer.update()
            # Clamp genuine floor penetration of the deformed character during the revised pose.
            for mod in modifiers:mod.show_viewport=True
            bpy.context.view_layer.update()
            deps=bpy.context.evaluated_depsgraph_get()
            lowest=min((v.co.z for o in objects if not any(k in o.name for k in ('Sword','Staff','Shield'))
                        for v in o.evaluated_get(deps).data.vertices),default=0)
            if lowest<-.0001:
                hip=rig.pose.bones['hips'].matrix.copy();hip.translation.z+=.001-lowest
                rig.pose.bones['hips'].matrix=hip
                bpy.context.view_layer.update()
            for b in rig.pose.bones:
                q=b.rotation_quaternion.copy()
                if b.name in last and last[b.name].dot(q)<0:b.rotation_quaternion=-q
                last[b.name]=b.rotation_quaternion.copy()
                for channel in ('location','rotation_quaternion','scale'):
                    b.keyframe_insert(channel,frame=f,group=b.name)
        for layer in act.layers:
            for strip in layer.strips:
                for bag in strip.channelbags:
                    for fc in bag.fcurves:
                        for k in fc.keyframe_points:k.interpolation='LINEAR'
        for marker in old_action.pose_markers:act.pose_markers.new(marker.name).frame=marker.frame
        actions[old_name]=act
        for track in rig.animation_data.nla_tracks:
            for strip in track.strips:
                if strip.action==old_action:
                    strip.action=act;strip.action_slot=act.slots[0]
        print('RETARGET',rig.name,old_name,flush=True)
    rig.animation_data.action=None
    return actions


def export(path,objects,rig):
    reset(rig)
    bpy.ops.object.select_all(action='DESELECT')
    for o in [rig,*objects]:
        o.hide_render=False;o.hide_viewport=False;o.hide_set(False);o.select_set(True)
    bpy.context.view_layer.objects.active=rig
    bpy.ops.export_scene.gltf(filepath=str(path),export_format='GLB',use_selection=True,
        export_animations=True,export_animation_mode='NLA_TRACKS',export_force_sampling=True,
        export_frame_range=False,export_anim_slide_to_zero=True,export_skins=True,
        export_morph=True,export_extras=True,export_yup=True,export_lights=False)


if PACK=='LowPolyKit_v3':
    targets=[(bpy.data.objects['RIG_NPC_Shared'],[bpy.data.objects[p['mesh']] for p in catalog['parts']],'NPC')]
    targets += [(bpy.data.objects[m['rig']],[bpy.data.objects[m['mesh']]],m['name']) for m in catalog['monsters']]
else:
    targets=[(bpy.data.objects[m['rig']],[bpy.data.objects[n] for n in m['meshes']],m['modelKey']) for m in catalog['monsters']]

for rig,objects,key in targets:
    reset(rig)
    original_lengths={b.name:b.length for b in rig.data.bones}
    before=sum(len(o.data.loop_triangles) for o in objects)
    # Store only the matrices needed for keyframe retargeting before changing the bind.
    animal=key in ('AshWolf','BlightBear','ThornStag')
    samples=[]
    if not animal:
        for track in rig.animation_data.nla_tracks:
            st=track.strips[0];act=st.action
            rig.animation_data.action=act;rig.animation_data.action_slot=st.action_slot
            frames=[]
            for frame in range(round(act.frame_range[0]),round(act.frame_range[1])+1):
                scene.frame_set(frame)
                frames.append((frame,{b.name:b.matrix.copy() for b in rig.pose.bones}))
            samples.append((act,frames))
        reset(rig)
        # Detach shared geometry once, then refresh the linked presentation meshes below.
        for o in objects:o.data=o.data.copy()
        old,new,scale=tailor_bind(rig)
        skin_remap(objects,old,new)
        if key=='NPC':npc_parts(rig,objects,pal)
        else:add_humanoid_detail(rig,objects,scale)
        retarget(rig,old,new,samples,objects,scale)
    else:
        for o in objects:o.data=o.data.copy()
        add_animal_detail(rig,objects,key)
        scale=1
    for o in objects:o.data.calc_loop_triangles()
    after=sum(len(o.data.loop_triangles) for o in objects)
    report['characters'][key]=dict(rig=rig.name,bones=len(rig.data.bones),
        meshes=[o.name for o in objects],triangles_before=before,triangles_after=after,
        actions=len(rig.animation_data.nla_tracks),height_scale=scale)
    # Presentation rigs and meshes use the revised bind and revised actions.
    for display in [o for o in bpy.data.objects if o.type=='ARMATURE' and o.data!=rig.data and
                    o.name.startswith('DISPLAY') and
                    any(m.type=='ARMATURE' and m.object==o and any(child.name.endswith(v.name) for v in objects)
                        for child in bpy.data.objects if child.type=='MESH' for m in child.modifiers)]:
        display.data=rig.data
        display.animation_data_clear()
        display.animation_data_create()
        if rig.animation_data.nla_tracks:
            st=rig.animation_data.nla_tracks[0].strips[0]
            display.animation_data.action=st.action;display.animation_data.action_slot=st.action_slot
    for o in objects:
        for display in list(bpy.data.objects):
            if display.type=='MESH' and display.name.startswith('DISPLAY') and display.name.endswith(o.name):
                name=display.name;matrix=display.matrix_world.copy();parent=display.parent
                collections=list(display.users_collection)
                bpy.data.objects.remove(display,do_unlink=True)
                display=o.copy();display.name=name;display.parent=parent
                for collection in collections:collection.objects.link(display)
                display.matrix_world=matrix
                for modifier in display.modifiers:
                    if modifier.type=='ARMATURE':modifier.object=parent
    if key=='NPC':
        export(OUT/'npc/npc_parts_rigged.glb',objects,rig)
        for role in catalog['roles']:
            role_name=role['name']
            raw=(SOURCE_DIR/'npc'/f'{role_name.lower()}_rigged.glb').read_bytes()
            length=struct.unpack_from('<I',raw,12)[0]
            document=json.loads(raw[20:20+length])
            selected=[bpy.data.objects[n['name']] for n in document['nodes'] if 'mesh' in n]
            export(OUT/'npc'/f'{role_name.lower()}_rigged.glb',selected,rig)
        for p in catalog['parts']:
            o=bpy.data.objects[p['mesh']]
            p.update(vertices=len(o.data.vertices),triangles=len(o.data.loop_triangles))
            bone=p['slot'] if p['slot'] in rig.data.bones else 'head' if p['slot'] in ('hair','headgear') else 'hips'
            pivot=rig.data.bones[bone].head_local
            p['joint_pivot_blender_z_up']=list(pivot)
            p['joint_pivot_gltf_y_up']=[pivot.x,pivot.z,-pivot.y]
        catalog['bone_schema']=[dict(name=b.name,parent=b.parent.name if b.parent else None,
            head_blender_z_up=list(b.head_local),tail_blender_z_up=list(b.tail_local)) for b in rig.data.bones]
        catalog.update(height_m=1.84,hair_max_height_m=1.865)
    elif PACK=='LowPolyKit_v3':export(OUT/'monsters'/f'{key}_rigged.glb',objects,rig)
    else:
        export(OUT/f'{key}.glb',objects,rig)
        m=next(m for m in catalog['monsters'] if m['modelKey']==key)
        m['bones']=[dict(name=b.name,parent=b.parent.name if b.parent else None) for b in rig.data.bones]
    print('MODELED',key,before,after,flush=True)

catalog['character_quality_revision']='2026-11-23'
catalog['quality_reference']='Hero_v1 five-head body and tailored dark fantasy geometry'
for m in catalog['monsters']:m['character_quality_revision']='2026-11-23'
(OUT/('asset_catalog.json' if PACK=='LowPolyKit_v3' else 'monster_catalog.json')).write_text(json.dumps(catalog,ensure_ascii=False,indent=2),encoding='utf-8')
if animation:
    animation['character_quality_revision']='2026-11-23'
    (OUT/'animation_catalog.json').write_text(json.dumps(animation,ensure_ascii=False,indent=2),encoding='utf-8')
reset(targets[0][0])
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/SOURCE.name))
(HERE/(PACK+'_authoring.json')).write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print('FINISHED',PACK,flush=True)
