"""Author a shared corrupted hero body and eight interchangeable weapon-arm forms."""
import bpy
import json
import math
import sys
from pathlib import Path
from mathutils import Matrix,Vector

P=Path(__file__).resolve().parents[1]
BASE=P.parent
sys.path.insert(0,str(BASE/'CharacterQuality_20261123/scripts'))
from mesh_quality import Mesh

for folder in ('modules','assembled','previews'):(P/folder).mkdir(exist_ok=True)
source=BASE/'HeroWeaponAnimations_v1/hero_weapon_animations_v1.blend'
bpy.ops.wm.open_mainfile(filepath=str(source))
scene=bpy.context.scene;scene.render.fps=60
rig=bpy.data.objects['RIG_HERO_HoodedLantern']
animation=json.loads((BASE/'HeroWeaponAnimations_v1/animation_catalog.json').read_text('utf-8'))
hero_catalog=json.loads((BASE/'Hero_v1/hero_catalog.json').read_text('utf-8'))
rest={b.name:b.matrix_local.copy() for b in rig.data.bones}
source_objects={o.name:o for o in bpy.data.objects if o.type=='MESH' and o.name.startswith(('HERO_','WPN_','AMMO_'))}
rig.animation_data.action=None
for track in rig.animation_data.nla_tracks:track.mute=True
for bone in rig.pose.bones:bone.matrix_basis=Matrix.Identity(4)
for obj in source_objects.values():
    if obj.data.shape_keys:
        obj.data.shape_keys.animation_data_clear()
        for key in obj.data.shape_keys.key_blocks:key.value=0
for col in bpy.data.collections:col.hide_render=False;col.hide_viewport=False
bpy.context.view_layer.update()


def mat(name,color,metal=0,rough=.82,emission=0):
    m=bpy.data.materials.new('MAT_CORR_'+name)
    m.diffuse_color=(*color,1);m.use_nodes=True
    shader=m.node_tree.nodes['Principled BSDF']
    shader.inputs['Base Color'].default_value=(*color,1)
    shader.inputs['Metallic'].default_value=metal;shader.inputs['Roughness'].default_value=rough
    shader.inputs['Emission Color'].default_value=(*color,1)
    shader.inputs['Emission Strength'].default_value=emission
    return m


palette={
    'hide':mat('CharredDermis',(.068,.040,.068)),
    'muscle':mat('AshVioletMuscle',(.145,.068,.100)),
    'scar':mat('OldScars',(.251,.134,.153)),
    'bone':mat('SmokedIvory',(.287,.254,.200)),
    'horn':mat('BlackKeratin',(.037,.029,.041),.08,.61),
    'vein':mat('CursedEmberVein',(.255,.035,.063),.12,.66,.24),
    'eye':mat('EmberEye',(.66,.071,.118),.05,.38,1.6),
    'iron':mat('InfectedIron',(.115,.124,.145),.68,.52),
    'edge':mat('ExposedSteelFace',(.250,.278,.304),.76,.39),
    'cloth':mat('SootHoodCloth',(.042,.047,.060)),
    'cloak':mat('ScorchedCloak',(.053,.039,.066)),
    'leather':mat('OldLeather',(.082,.045,.033)),
    'brass':mat('BlackenedBrass',(.172,.112,.045),.60,.65),
}
body_collection=bpy.data.collections.new('CORRUPTED_SHARED_BODY')
scene.collection.children.link(body_collection)
collections={}
new_objects=[]


def clone(source,name,collection):
    obj=source.copy();obj.data=source.data.copy();obj.name=name
    collection.objects.link(obj)
    obj.hide_render=False;obj.hide_viewport=False;obj.hide_set(False)
    new_objects.append(obj)
    return obj


def create(mesh,name,collection):
    data=bpy.data.meshes.new(name+'_GEO')
    obj=bpy.data.objects.new(name,data);collection.objects.link(obj)
    obj.parent=rig
    mod=obj.modifiers.new('CorruptionSkin','ARMATURE');mod.object=rig
    mesh.fit(obj);new_objects.append(obj)
    return obj


def tint(obj,kind='body'):
    for i,m in enumerate(list(obj.data.materials)):
        name=m.name.lower()
        if kind=='weapon':
            key='edge' if any(s in name for s in ('edge','steel','blade')) else 'brass' if any(s in name for s in ('brass','gold')) else 'horn' if any(s in name for s in ('wood','leather','grip','cord')) else 'iron'
        else:
            key='hide' if 'skin' in name else 'iron' if any(s in name for s in ('iron','edge','metal')) else 'brass' if 'brass' in name else 'leather' if any(s in name for s in ('leather','boot','sole')) else 'cloak' if 'cape' in obj.name.lower() else 'cloth'
        obj.data.materials[i]=palette[key]


def spike(mesh,points,radii,weight,color):
    # Small offset prevents an exactly axial cord from degenerating at its tip.
    for a,b,r1,r2 in zip(points,points[1:],radii,radii[1:]):
        a,b=Vector(a),Vector(b)
        if (b-a).length>.0001:
            mesh.limb(a,b,[(0,r1,r1*.80,0),(1,max(.001,r2),max(.001,r2*.80),0)],[weight]*2,[color],8)


common=[]
excluded=('HERO_WEAPON_','HERO_FOREARM_','HERO_HAND_')
for old_name,original in source_objects.items():
    if not old_name.startswith('HERO_') or old_name.startswith(excluded):continue
    name=old_name.replace('HERO_','CORR_BODY_',1)
    obj=clone(original,name,body_collection);tint(obj);common.append(obj)
    detail=Mesh(palette)
    if 'HEAD_00' in old_name:
        for sign in (-1,1):
            detail.cord([(sign*.092,-.111,1.748),(sign*.115,-.101,1.706),
                         (sign*.098,-.123,1.661),(sign*.064,-.130,1.628)],.0045,'head','vein')
        detail.ellipsoid((-.063,-.153,1.731),(.012,.005,.008),'head','eye',8,4)
        detail.append(obj)
    elif 'HEADGEAR_' in old_name:
        for sign in (-1,1):
            points=[(sign*.115,.029,1.841),(sign*.165,.038,1.895),
                    (sign*.192,.024,1.978),(sign*.176,-.007,2.015)]
            spike(detail,points,[.034,.027,.014,.001],'head','horn')
        detail.cord([(-.125,-.127,1.865),(-.061,-.177,1.900),(0,-.186,1.915)],.005,'head','scar')
        detail.append(obj)
    elif 'TORSO_' in old_name:
        for sign in (-1,1):
            detail.cord([(sign*.104,-.109,1.381),(sign*.068,-.141,1.321),
                         (sign*.035,-.139,1.242),(sign*.070,-.120,1.167)],.006,{'chest':.8,'spine':.2},'vein')
        detail.plate([(-.042,-.132,1.365),(.013,-.146,1.390),(.038,-.145,1.300),
                      (-.011,-.149,1.244),(-.046,-.133,1.283)],.007,'chest','horn','scar')
        detail.append(obj)
    elif 'PAULDRON_' in old_name:
        side='l' if '_L_' in old_name else 'r';sign=1 if side=='l' else -1
        shoulder=rest['upper_arm_'+side].translation
        for j in range(3):
            start=shoulder+Vector((sign*(.017+j*.036),.014,.035-j*.018))
            spike(detail,[start,start+Vector((sign*.036,.018,.041)),start+Vector((sign*.049,.028,.075))],
                  [.018,.011,.001],'upper_arm_'+side,'horn')
        detail.append(obj)
    elif 'UPPER_ARM_' in old_name:
        side='l' if '_L_' in old_name else 'r';sign=1 if side=='l' else -1
        a=rest['upper_arm_'+side].translation;b=rest['forearm_'+side].translation
        for j in range(2):
            detail.cord([a.lerp(b,.22)+Vector((sign*.040,-.037,j*.006)),
                         a.lerp(b,.55)+Vector((sign*.049,-.033,0)),
                         a.lerp(b,.97)+Vector((sign*.034,-.027,0))],.008,'upper_arm_'+side,'muscle')
        detail.append(obj)


def arm_mesh(side,variant=None):
    m=Mesh(palette)
    sign=1 if side=='l' else -1
    fore='forearm_'+side;hand='hand_'+side;weapon='weapon_'+side
    elbow=rest[fore].translation;wrist=rest[hand].translation;grip=rest[weapon].translation
    bulky=variant in ('warhammer','mace')
    radius=(.132 if bulky else .114) if variant else .061
    if variant:
        upper='upper_arm_'+side
        shoulder=rest[upper].translation
        m.limb(shoulder,elbow,[(-.02,.082,.078,0),(.18,.111,.097,.010),
                              (.43,.123,.105,.014),(.70,.110,.092,.012),(1.02,.085,.077,.005)],
               [upper,upper,upper,{upper:.88,fore:.12},{upper:.55,fore:.45}],
               ['hide','muscle','hide','muscle','hide'],14)
        for j in range(3):
            root=shoulder.lerp(elbow,.25+j*.17)+Vector((sign*.100,.020,0))
            spike(m,[root,root+Vector((sign*.032,.022,.027)),root+Vector((sign*.059,.045,.060))],
                  [.025,.018,.001],upper,'horn')
    rings=[(-.06,.054,.049,0),(.14,radius,.057,0),(.37,radius*1.10,.063,.004),
           (.65,radius*.92,.054,.006),(.87,.047,.043,.002),(1.04,.044,.040,0)]
    if variant:
        rings=[(-.11,.088,.075,0),(.13,radius,.094,.010),(.38,radius*1.18,.109,.018),
               (.64,radius*1.05,.099,.021),(.86,.085,.070,.012),(1.07,.069,.063,.004)]
    m.limb(elbow,wrist,rings,[{fore:.85,'upper_arm_'+side:.15},fore,fore,fore,{fore:.72,hand:.28},hand],
           ['hide','muscle','hide','muscle','hide'],12)
    for j in range(3):
        a=elbow.lerp(wrist,.18+j*.17)+Vector((sign*(radius*.91),.007,0))
        spike(m,[a,a+Vector((sign*.034,.036,.041)),a+Vector((sign*.061,.055,.077))],
              [.024 if variant else .016,.016 if variant else .012,.001],fore,'horn')
    for j in range(3):
        off=Vector(((j-1)*.037,-.089 if variant else -.043,0))
        points=[elbow.lerp(wrist,.18)+off,wrist+off*.75,
                grip+Vector(((j-1)*.019,-.030,-.018))]
        for step,(a,b) in enumerate(zip(points,points[1:])):
            weights=[fore,hand] if step==0 else [hand,weapon] if variant else [hand,hand]
            m.limb(a,b,[(0,.0055,.0055,0),(1,.0055,.0055,0)],weights,
                   ['vein' if j==1 else 'scar'],6)
    if variant is None:
        end=wrist+Vector((sign*.005,-.010,-.099))
        m.limb(wrist,end,[(0,.043,.039,0),(.30,.047,.038,0),(.78,.039,.033,.006),(1,.028,.025,.006)],
               [hand]*4,['hide','muscle','hide'],10)
        for j in range(4):
            a=end+Vector(((j-1.5)*.016,-.005,.008))
            spike(m,[a,a+Vector((0,-.024,-.028)),a+Vector((0,-.034,-.050))],
                  [.009,.007,.001],hand,'bone')
        return m
    # The last cuff is a bulb of fused tissue around the original weapon socket.
    m.ellipsoid(grip,(.105 if bulky else .090,.088,.104),{hand:.48,weapon:.52},'muscle',14,7)
    m.ellipsoid(grip+Vector((0,-.030,.006)),(.075,.061,.083),weapon,'hide',12,6)
    for j in range(4):
        angle=math.tau*j/4
        offset=Vector((.036*math.cos(angle),.032*math.sin(angle),0))
        a=wrist+offset
        b=grip+offset*1.25+Vector((0,0,-.018))
        m.cord([a,(a+b)/2+Vector((sign*.008,0,0)),b],.011,hand,'scar')
    return m


free={}
for side in 'lr':
    obj=create(arm_mesh(side),'CORR_ARM_FREE_'+side.upper(),body_collection)
    obj['slot']='free_forearm_'+side;free[side]=obj

weapons={e['weapon_kind']:e for e in animation['equipment']}
keys={k:e['weapon_key'] for k,e in weapons.items()}
keys['longsword']='HERO_Longsword'
kinds=['longsword','staff','bow','mace','axe','warhammer','spear','dagger']
labels=['장검','지팡이','활','철퇴','도끼','전투 망치','창','단검']
modules=[]

for index,(kind,label) in enumerate(zip(kinds,labels)):
    side='l' if kind=='bow' else 'r';weapon='weapon_'+side
    tag=f'{index:02d}_'+kind.title()
    collection=bpy.data.collections.new('CORRUPTED_FORM_'+tag)
    scene.collection.children.link(collection);collections[kind]=collection
    arm=create(arm_mesh(side,kind),'CORR_FUSION_'+tag+'_Arm',collection)
    arm['module_index']=index;arm['slot']='fused_forearm_'+side
    names=['HERO_WEAPON_R_00_LongswordBlade','HERO_WEAPON_R_01_LongswordHilt'] if kind=='longsword' else weapons[kind]['weapon_parts']
    parts=[arm];mapping={}
    for j,name in enumerate(names):
        original=source_objects[name]
        obj=clone(original,'CORR_WEAPON_'+tag+'_'+('Blade' if j==0 and kind=='longsword' else 'Hilt' if kind=='longsword' else 'String' if 'String' in name else 'Body'),collection)
        tint(obj,'weapon')
        parts.append(obj);mapping[name]=obj.name
        if 'String' in name:
            obj.data.materials[0]=palette['scar']
            continue
        detail=Mesh(palette);grip=rest[weapon].translation
        if kind=='longsword' and j==0:
            # The metal spine remains visible while tissue encloses the former grip and blade heel.
            points=sorted([v.co.copy() for v in obj.data.vertices],key=lambda v:(v-grip).length)
            axis=(points[-1]-grip).normalized()
            for sign in (-1,1):
                detail.cord([grip+Vector((sign*.040,-.024,-.018)),
                             grip+axis*.20+Vector((sign*.025,0,0)),
                             grip+axis*.43+Vector((sign*.013,0,0))],.012,weapon,'scar')
        elif kind=='longsword':
            detail.ellipsoid(grip+Vector((0,0,.025)),(.085,.052,.104),weapon,'hide',12,6)
        else:
            # Roots grow out of the arm onto the shaft, leaving the grip used by the other hand clear.
            length={'staff':.46,'bow':.30,'mace':.39,'axe':.34,'warhammer':.27,'spear':.25,'dagger':.26}[kind]
            for strand in range(4):
                points=[]
                for step in range(8):
                    u=step/7
                    angle=strand*math.pi/2+u*2.5
                    radius=(.043 if kind!='bow' else .036)*(1-.55*u)
                    points.append(grip+Vector((radius*math.cos(angle),radius*math.sin(angle),-.035+length*u)))
                detail.cord(points,.012 if kind in ('warhammer','mace') else .009,weapon,'muscle' if strand%2 else 'scar')
            for z in (.11,.19):
                a=grip+Vector((-.035,0,z))
                spike(detail,[a,a+Vector((-.027,.01,.027)),a+Vector((-.045,.016,.065))],
                      [.015,.010,.001],weapon,'horn')
            if kind=='mace':
                detail.ellipsoid(grip+Vector((0,0,.43)),(.060,.060,.083),weapon,'muscle',10,6)
            if kind=='staff':
                for sign in (-1,1):
                    detail.cord([grip+Vector((sign*.031,0,.38)),grip+Vector((sign*.046,0,.66)),
                                 grip+Vector((sign*.038,0,.93))],.010,weapon,'vein')
            if kind=='axe':
                detail.cord([grip+Vector((-.038,0,.15)),grip+Vector((-.050,0,.41)),
                             grip+Vector((-.130,0,.55))],.013,weapon,'scar')
        morphs=None
        if obj.data.shape_keys:
            morphs={key.name:[v.co.copy() for v in key.data] for key in obj.data.shape_keys.key_blocks}
            old_count=len(obj.data.vertices)
        detail.append(obj)
        if morphs:
            obj.shape_key_add(name='Basis');draw=obj.shape_key_add(name='Draw')
            for i,v in enumerate(obj.data.vertices):
                if i<old_count:
                    draw.data[i].co=morphs['Draw'][i]
                else:
                    local=v.co-rest[weapon].translation
                    fraction=min(1,abs(local.z)/.66)
                    draw.data[i].co.y+=.055*fraction*fraction
                    draw.data[i].co.z-=math.copysign(.018*fraction*fraction,local.z)
    if kind=='bow':
        arrow=clone(source_objects['AMMO_Arrow_00_Iron__Body'],'CORR_AMMO_02_BoneArrow',collection)
        tint(arrow,'weapon');parts.append(arrow)
        mapping['AMMO_Arrow_00_Iron__Body']=arrow.name
    else:arrow=None
    original_hidden=[f'HERO_FOREARM_{side.upper()}_00_LeatherBracer',f'HERO_HAND_{side.upper()}_00_Glove',
                     'HERO_WEAPON_R_00_LongswordBlade','HERO_WEAPON_R_01_LongswordHilt']
    module=dict(module_index=index,weapon_key=keys[kind],weapon_kind=kind,name_ko=label,
                side=side,fused_bone=weapon,parts=[o.name for o in parts],
                free_arm=free['r' if side=='l' else 'l'].name,
                module_file='modules/corrupted_'+kind+'_module.glb',
                assembled_file='assembled/hero_corrupted_'+kind+'.glb',
                source_equipped_file='../Hero_v1/hero_rigged.glb' if kind=='longsword' else '../HeroWeaponAnimations_v1/'+weapons[kind]['file'],
                source_node_map=mapping,hide_normal_parts=original_hidden,
                support_hand='left' if kind in ('warhammer','spear') else 'right_draw' if kind=='bow' else None)
    modules.append(module)

# A dim caged lamp remains available on the free left hand for the one-handed forms.
lamp=[]
for j,name in enumerate(hero_catalog['left_hand']):
    obj=clone(source_objects[name],'CORR_LAMP_'+('Frame' if j==0 else 'Ember'),body_collection)
    if j==0:tint(obj,'weapon')
    else:obj.data.materials[0]=palette['vein']
    lamp.append(obj)

# Keep only the authored corruption parts and the shared hero skeleton in the editing scene.
keep=set([rig,*new_objects])
for obj in list(bpy.data.objects):
    if obj not in keep:bpy.data.objects.remove(obj,do_unlink=True)
for obj in new_objects:
    obj['corruption_variant']='weapon_arm_fusion'
    obj['bind_revision']='five_head_hero_20261110'
    obj.data.calc_loop_triangles()
    if obj.data.shape_keys and 'Hide' in obj.data.shape_keys.key_blocks:
        obj.data.shape_keys.key_blocks['Hide'].value=1
bone_schema=[dict(name=b.name,parent=b.parent.name if b.parent else None,
                 head_blender_z_up=list(b.head_local),tail_blender_z_up=list(b.tail_local)) for b in rig.data.bones]
catalog=dict(schema_version=1,date='2026-11-24',asset_id='HERO_CorruptedModular_v1',
    units='meter',front_blender='-Y',up_blender='Z',up_gltf='Y',front_gltf='+Z',
    rig=rig.name,bone_schema=bone_schema,part_library_file='hero_corrupted_parts.glb',
    shared_body_file='hero_corrupted_base.glb',shared_body=[o.name for o in common],
    free_arms={side:obj.name for side,obj in free.items()},optional_lamp=[o.name for o in lamp],
    modules=modules,selection_order=['shared_body','module.parts','module.free_arm','optional_lamp_when_one_handed'],
    corruption_state='Existing game corruption >= MutationThreshold. State selection is applied by the integration task.',
    source_reference='../HeroWeaponAnimations_v1/animation_catalog.json',
    source_common='../Hero_v1/hero_rigged.glb',source_library='../HeroWeaponAnimations_v1/hero_weapon_animation_library.glb',
    common_clips=animation['common_clips'],weapon_clips=animation['new_clips'],
    parts=[dict(mesh=o.name,vertices=len(o.data.vertices),triangles=len(o.data.loop_triangles)) for o in new_objects])


def export(file,objects):
    bpy.ops.object.select_all(action='DESELECT')
    for obj in [rig,*objects]:
        obj.hide_set(False);obj.hide_render=False;obj.hide_viewport=False;obj.select_set(True)
    bpy.context.view_layer.objects.active=rig
    bpy.ops.export_scene.gltf(filepath=str(P/file),export_format='GLB',use_selection=True,
        export_animations=False,export_skins=True,export_morph=True,export_extras=True,
        export_yup=True,export_lights=False)


export(catalog['part_library_file'],new_objects)
export(catalog['shared_body_file'],[*common,*free.values(),*lamp])
for module in modules:
    parts=[bpy.data.objects[n] for n in module['parts']]
    export(module['module_file'],parts)
    selected=[*common,bpy.data.objects[module['free_arm']],*parts]
    if module['weapon_kind'] not in ('warhammer','spear','bow'):selected+=lamp
    export(module['assembled_file'],selected)
    print('CREATED FUSION',module['weapon_kind'],flush=True)
for module in modules:
    module['assembled_triangles']=sum(len(bpy.data.objects[n].data.loop_triangles) for n in
        catalog['shared_body']+[module['free_arm']]+module['parts']+
        (catalog['optional_lamp'] if module['weapon_kind'] not in ('warhammer','spear','bow') else []))
catalog['unique_mesh_count']=len(new_objects)
(P/'corruption_catalog.json').write_text(json.dumps(catalog,ensure_ascii=False,indent=2),encoding='utf-8')
default=set(common+[free['l']]+[bpy.data.objects[n] for n in modules[0]['parts']]+lamp)
for obj in new_objects:obj.hide_render=obj not in default;obj.hide_set(obj not in default)
rig.animation_data.action=next(t.strips[0].action for t in rig.animation_data.nla_tracks if t.name=='HERO_Idle')
scene.frame_set(1)
bpy.ops.wm.save_as_mainfile(filepath=str(P/'hero_corruption_v1.blend'))
print('BUILT',len(modules),'forms',len(new_objects),'unique meshes',flush=True)
