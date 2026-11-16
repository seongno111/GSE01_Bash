"""Author and bake weapon actions. Run with Blender 5.2 --background --python."""
import bpy, math, json, hashlib, struct, sys, shutil
from pathlib import Path
from mathutils import Vector, Matrix, Quaternion

BASE=Path(__file__).resolve().parents[2]
PACKAGE=Path(__file__).resolve().parents[1]
OUT=next((Path(a.split('=',1)[1]).resolve() for a in sys.argv if a.startswith('out=')),PACKAGE)
OUT.mkdir(exist_ok=True)
(OUT/'previews').mkdir(exist_ok=True)
RAD=math.pi/180
only_kind=next((a.split('=',1)[1] for a in sys.argv if a.startswith('only=')),None)
sys.path.insert(0,str(PACKAGE/'scripts'))
from dynamic_motion import direct_weapon, sharpen_weapon_keys, STANCE, segment_progress, retarget_pose
from common_motion import bake_common_actions, blend, cape_floor_weights, raise_cape_hem
from skill_motion import register_skills, skill_time, direct_skill, bow_mode, bow_arrow_visible
from hero_model import reshape_hero, HEIGHT, HEAD_CHIN, HOOD_TOP
source_paths=[BASE/'Hero_v1/hero_v1.blend',BASE/'Hero_v1/hero_rigged.glb',BASE/'HeroWeapons_v1/hero_weapons_v1.blend',BASE/'HeroWeapons_v1/weapon_catalog.json']
source_hashes={str(p.relative_to(BASE)):hashlib.sha256(p.read_bytes()).hexdigest() for p in source_paths}
baseline=PACKAGE/'revisions/20261110_before_dynamics/hero_v1.blend'
bpy.ops.wm.open_mainfile(filepath=str(baseline if baseline.exists() else source_paths[0]))
scene=bpy.context.scene
rig=bpy.data.objects['RIG_HERO_HoodedLantern']
hero=[o for o in bpy.data.objects if o.type=='MESH' and o.name.startswith('HERO_')]
lantern_light=bpy.data.objects.get('LIGHT_HERO_Lantern')
keep=set(hero+[rig]+([lantern_light] if lantern_light else []))
for o in list(bpy.data.objects):
    if o not in keep: bpy.data.objects.remove(o,do_unlink=True)
for c in bpy.data.collections: c.hide_render=False; c.hide_viewport=False
for o in keep: o.hide_render=False; o.hide_viewport=False; o.hide_set(False)
old_actions={t.name:t.strips[0].action for t in rig.animation_data.nla_tracks}
for t in list(rig.animation_data.nla_tracks): rig.animation_data.nla_tracks.remove(t)
rig.animation_data.action=None
for p in rig.pose.bones:
    p.rotation_mode='QUATERNION'
    p.matrix_basis=Matrix.Identity(4)
bpy.context.view_layer.update()
reshape_hero(rig,hero,lantern_light)
rest={b.name:b.matrix_local.copy() for b in rig.data.bones}
boots={side:[o.matrix_world@v.co for o in hero if o.name.startswith('HERO_FOOT_'+side.upper()+'_') for v in o.data.vertices] for side in ('l','r')}
cape=next(o for o in hero if o.name.startswith('HERO_CAPE_'))
hem_weights=cape_floor_weights(cape)
for p in rig.pose.bones: p.rotation_mode='QUATERNION'; p.matrix_basis=Matrix.Identity(4)
scene.render.fps=60

wc=json.loads((BASE/'HeroWeapons_v1/weapon_catalog.json').read_text('utf-8'))
weapon_by_kind={w['kind']:w for w in wc['weapons']}
keys={k:weapon_by_kind[k]['weapon_key'] for k in weapon_by_kind}
keys['longsword']='HERO_Longsword'
with bpy.data.libraries.load(str(source_paths[2]),link=False) as (src,dst):
    dst.objects=[n for n in src.objects if (n.startswith(('WPN_','AMMO_')) and not n.endswith('.001'))]
weapons={}
for o in dst.objects:
    if o: scene.collection.objects.link(o); o.hide_render=False; o.hide_viewport=False; o.hide_set(False); weapons[o.name]=o
for o in weapons.values():
    if o.type=='MESH': o.data=o.data.copy()
ground_geometry={kind:[o.matrix_local@v.co for o in weapons[keys[kind]].children if o.type=='MESH' for v in o.data.vertices] for kind in ('staff','warhammer')}

def pose(rp=(-.34,-.18,1.04),rd=(0,-.12,1),lp=(.43,-.18,.85),ld=(0,0,1),yaw=0,pitch=0,crouch=.035,twist=0,roll=0,draw=0):
    return dict(rp=list(rp),rd=list(rd),lp=list(lp),ld=list(ld),yaw=yaw,pitch=pitch,crouch=crouch,twist=twist,roll=roll,draw=draw)

ready={
 'staff':pose(rp=(-.39,-.14,.99),rd=(-.04,-.12,1),yaw=8),
 'mace':pose(rp=(-.35,-.20,1.03),rd=(-.15,-.25,1),yaw=8),
 'axe':pose(rp=(-.36,-.19,1.04),rd=(-.16,-.23,1),yaw=10,roll=-70),
 'warhammer':pose(rp=(-.26,-.24,1.06),rd=(-.72,-.18,.67),yaw=-10,crouch=.06),
 'spear':pose(rp=(-.15,-.08,1.0),rd=(.25,-.96,.12),yaw=-45,crouch=.055),
 'dagger':pose(rp=(-.30,-.25,.98),rd=(.12,-.88,.46),yaw=15,crouch=.055),
 'bow':pose(rp=(-.3,-.23,.98),rd=(0,0,1),lp=(.31,-.19,.99),ld=(0,-.05,1),yaw=-15,crouch=.045),
}
def change(p,**kw): q={k:(v[:] if isinstance(v,list) else v) for k,v in p.items()}; q.update(kw); return q

specs=[]
def clip(kind,action,duration,poses,loop=False,events=(),description='',start='Ready',end='Ready',skill_label=None):
    prefix={'warhammer':'Hammer','longsword':'Sword'}.get(kind,kind.title())
    n='HERO_'+prefix+'_'+action
    spec=dict(name=n,weapon_kind=kind,weapon_key=keys[kind],duration_seconds=duration,fps=60,start_frame=1,end_frame=round(duration*60)+1,loop=loop,keyposes=poses,events=[dict(name=a,time_seconds=t if skill_label else round(t*60)/60,frame=round(t*60)+1) for a,t in events],description=description,start_pose=start,end_pose=end,blend_in_seconds=.12,blend_out_seconds=.12,root_translation='in_place',root_yaw_degrees=0)
    if skill_label:
        spec.update(skill_label=skill_label,kind='weapon_skill',tags=['weapon_skill'])
    if n=='HERO_Hammer_SkillGroundSlam':
        spec['sample_divisions']=4
    if kind=='longsword':
        spec.update(root_yaw_degrees=720,start_pose='Idle',end_pose='Idle')
    specs.append(spec)

for k,p in ready.items(): clip(k,'Ready',2,[(0,p),(1,change(p,pitch=p['pitch']+1,crouch=p['crouch']+.005)),(2,p)],True,description='Weapon-specific breathing ready stance')
p=ready['staff']
clip('staff','CastBolt',1.3,[(0,p),(.38,change(p,rp=(-.4,.01,1.15),rd=(-.1,.25,1),yaw=20,crouch=.06)),(.62,change(p,rp=(-.24,-.34,1.15),rd=(0,-.94,.34),yaw=-12,pitch=6)),(.82,change(p,rp=(-.26,-.29,1.16),rd=(0,-.82,.57),yaw=-8)),(1.3,p)],events=[('cast_release',.62)],description='Anticipate, point crook crystal forward, release and recover')
clip('staff','CastSweep',1.65,[(0,p),(.5,change(p,rp=(-.41,-.03,1.22),rd=(-.7,-.2,.7),yaw=25,crouch=.07)),(.85,change(p,rp=(-.19,-.32,1.18),rd=(.65,-.6,.45),yaw=-22,pitch=7)),(1.05,change(p,rp=(-.17,-.27,1.16),rd=(.5,-.6,.65),yaw=-18)),(1.65,p)],events=[('cast_release',.85)],description='Broad lateral ritual sweep; no spell VFX')
c=change(p,rp=(-.30,-.27,1.17),rd=(0,-.55,.84),yaw=-5)
clip('staff','Channel',2,[(0,c),(.5,change(c,pitch=2,crouch=.04)),(1,c),(1.5,change(c,pitch=-1,crouch=.03)),(2,c)],True,[('channel_tick',.5),('channel_tick',1.5)],'Continuous focus loop',start='Channel',end='Channel')

p=ready['mace']
clip('mace','Strike',1.25,[(0,p),(.4,change(p,rp=(-.4,.025,1.30),rd=(-.3,.35,.88),yaw=22,crouch=.065)),(.62,change(p,rp=(-.27,-.34,.99),rd=(.15,-.9,-.4),yaw=-15,pitch=9,crouch=.08)),(.8,change(p,rp=(-.24,-.29,.96),rd=(.4,-.8,-.25),yaw=-18,crouch=.08)),(1.25,p)],events=[('melee_contact',.62)],description='Compact weighted downward flanged-head strike')
clip('mace','Sweep',1.15,[(0,p),(.35,change(p,rp=(-.43,-.07,1.1),rd=(-.8,-.25,.35),yaw=28)),(.55,change(p,rp=(-.15,-.32,1.02),rd=(.75,-.65,.08),yaw=-20,crouch=.07)),(.74,change(p,rp=(-.12,-.27,1.03),rd=(.85,-.25,.3),yaw=-25)),(1.15,p)],events=[('melee_contact',.55)],description='Short waist-height crushing arc')
g=change(p,rp=(-.25,-.29,1.12),rd=(.12,-.12,1),yaw=-5,crouch=.06)
clip('mace','Guard',2,[(0,g),(1,change(g,crouch=.066)),(2,g)],True,description='Raised head guards chest',start='Guard',end='Guard')

p=ready['axe']
clip('axe','Chop',1.3,[(0,p),(.43,change(p,rp=(-.35,.005,1.33),rd=(-.2,.25,1),yaw=23,crouch=.065)),(.67,change(p,rp=(-.23,-.33,1.0),rd=(.15,-.88,-.45),yaw=-17,pitch=9,crouch=.085)),(.86,change(p,rp=(-.20,-.26,.96),rd=(.4,-.7,-.6),yaw=-21)),(1.3,p)],events=[('melee_contact',.67)],description='High diagonal axe chop with delayed recovery')
clip('axe','Cleave',1.5,[(0,p),(.48,change(p,rp=(-.43,.02,1.16),rd=(-.8,-.05,.6),yaw=30,crouch=.07)),(.78,change(p,rp=(-.17,-.30,1.08),rd=(.6,-.78,.05),yaw=-23,crouch=.075)),(1.02,change(p,rp=(-.13,-.22,1.02),rd=(.85,-.25,.2),yaw=-30)),(1.5,p)],events=[('melee_contact',.78)],description='Broad edge-leading heavy cleave')
g=change(p,rp=(-.24,-.29,1.13),rd=(.22,-.12,1),yaw=-7,crouch=.065)
clip('axe','Guard',2,[(0,g),(1,change(g,crouch=.071)),(2,g)],True,description='Axe haft and head raised',start='Guard',end='Guard')

p=ready['warhammer']
clip('warhammer','Smash',1.8,[(0,p),(.7,change(p,rp=(-.43,-.16,1.27),rd=(-.58,.07,.81),yaw=-15,crouch=.045,pitch=-4)),(1.0,change(p,rp=(-.40,-.28,.96),rd=(-.58,-.63,-.52),yaw=-10,crouch=.13,pitch=12)),(1.22,change(p,rp=(-.40,-.29,.92),rd=(-.57,-.63,-.55),yaw=-12,crouch=.135,pitch=13)),(1.8,p)],events=[('melee_contact',1.0)],description='Two-handed diagonal smash; opposed cross-palm grip and strike-aligned hammer face')
clip('warhammer','Uppercut',1.65,[(0,p),(.55,change(p,rp=(-.43,-.24,.92),rd=(-.62,-.62,-.48),yaw=5,crouch=.14,pitch=8)),(.85,change(p,rp=(-.40,-.30,1.11),rd=(-.60,-.53,.60),yaw=-12,crouch=.06,pitch=-3)),(1.08,change(p,rp=(-.43,-.22,1.22),rd=(-.63,-.29,.72),yaw=-18)),(1.65,p)],events=[('melee_contact',.85)],description='Rising hammer strike with neutral wrists and fixed support-hand spacing')
g=change(p,rp=(-.22,-.28,1.12),rd=(-.68,-.22,.70),yaw=-15,crouch=.075)
clip('warhammer','Guard',2,[(0,g),(1,change(g,crouch=.081)),(2,g)],True,description='Two hands brace hammer vertically',start='Guard',end='Guard')

p=ready['spear']
clip('spear','Thrust',1.1,[(0,p),(.3,change(p,rp=(-.17,.015,1.01),rd=(.23,-.97,.10),yaw=-47,crouch=.08)),(.52,change(p,rp=(-.10,-.16,1.08),rd=(.22,-.97,.06),yaw=-40,crouch=.075,pitch=5)),(.66,change(p,rp=(-.11,-.14,1.07),rd=(.22,-.97,.06),yaw=-42,crouch=.075)),(1.1,p)],events=[('melee_contact',.52)],description='Two-handed straight thrust and recoil')
clip('spear','Sweep',1.5,[(0,p),(.45,change(p,rp=(-.16,-.06,1.03),rd=(-.1,-.99,.1),yaw=-55,crouch=.08)),(.75,change(p,rp=(-.08,-.23,1.04),rd=(.65,-.75,.10),yaw=-52,crouch=.09)),(.95,change(p,rp=(-.06,-.17,1.04),rd=(.8,-.59,.12),yaw=-53)),(1.5,p)],events=[('melee_contact',.75)],description='Wide spear sweep with support hand retained on shaft')
g=change(p,rp=(-.14,-.15,1.03),rd=(.65,-.65,.4),yaw=-35,crouch=.075)
clip('spear','Guard',2,[(0,g),(1,change(g,crouch=.081)),(2,g)],True,description='Diagonal two-handed shaft guard',start='Guard',end='Guard')

p=ready['dagger']
clip('dagger','Stab',.75,[(0,p),(.2,change(p,rp=(-.38,-.12,1.03),rd=(.1,-.93,.35),yaw=23,crouch=.075)),(.35,change(p,rp=(-.22,-.39,1.07),rd=(0,-1,.05),yaw=-12,crouch=.075,pitch=5)),(.45,change(p,rp=(-.22,-.36,1.07),rd=(0,-1,.05),yaw=-10)),(.75,p)],events=[('melee_contact',.35)],description='Fast direct point-first stab')
clip('dagger','Slash',.85,[(0,p),(.24,change(p,rp=(-.42,-.13,1.12),rd=(-.55,-.8,.1),yaw=25)),(.42,change(p,rp=(-.16,-.33,1.02),rd=(.7,-.7,-.1),yaw=-20,crouch=.07)),(.56,change(p,rp=(-.12,-.26,1.0),rd=(.9,-.3,0),yaw=-24)),(.85,p)],events=[('melee_contact',.42)],description='Quick close-range diagonal slice')
g=change(p,rp=(-.25,-.3,1.12),rd=(0,-.6,.8),yaw=-4,crouch=.07)
clip('dagger','Guard',2,[(0,g),(1,change(g,crouch=.076)),(2,g)],True,description='Compact close-quarters guard',start='Guard',end='Guard')

p=ready['bow']
nock=change(p,lp=(.08,-.20,1.02),ld=(0,0,1),yaw=-30,draw=0)
aim=change(p,rp=(.05,-.06,1.20),lp=(.05,-.55,1.20),ld=(0,0,1),yaw=-65,draw=1,crouch=.055)
clip('bow','Nock',.75,[(0,p),(.25,change(nock,rp=(-.22,-.06,1.06))),(.75,nock)],events=[('arrow_show',.25)],description='Bring arrow to resting nocking point',start='Ready',end='Nocked')
clip('bow','Draw',.8,[(0,nock),(.8,aim)],events=[('draw_complete',.8)],description='Extend bow arm, draw nock back; synchronized bow morph',start='Nocked',end='Aim')
clip('bow','Aim',2,[(0,aim),(1,change(aim,crouch=.058)),(2,aim)],True,description='Full draw aim loop',start='Aim',end='Aim')
recoil=change(aim,draw=.1,rp=(.03,.015,1.20),yaw=-63)
clip('bow','Release',.55,[(0,aim),(.10,aim),(.15,recoil),(.3,change(recoil,draw=0)),(.55,p)],events=[('projectile_release',.10),('arrow_hide',.10)],description='Release string, small recoil and lower; projectile motion belongs to game',start='Aim',end='Ready')
clip('bow','Lower',.65,[(0,aim),(.35,nock),(.65,p)],events=[('arrow_hide',.5)],description='Controlled undraw and lower without firing',start='Aim',end='Ready')

def interpolate(keys,t,spec=None):
    for i in range(len(keys)-1):
        ta,a=keys[i];tb,b=keys[i+1]
        if t<=tb+1e-6:
            u=segment_progress(spec,keys,i,(t-ta)/(tb-ta))
            result={k:blend(a[k],b[k],u) for k in a}
            for axis in ('rd','ld'):
                result[axis]=Vector(a[axis]).normalized().slerp(Vector(b[axis]).normalized(),u)
            return result
    return keys[-1][1]

def basis(direction,roll=0):
    z=Vector(direction).normalized(); x=Vector((1,0,0));x-=z*x.dot(z)
    if x.length<.01: x=Vector((0,1,0));x-=z*x.dot(z)
    x.normalize();y=z.cross(x).normalized()
    return Matrix((x,y,z)).transposed().to_quaternion() @ Quaternion((0,0,1),roll*RAD)
def transform(pos,q): return Matrix.Translation(Vector(pos)) @ q.to_matrix().to_4x4()
def update(): bpy.context.view_layer.update()
def set_world(name,mat): rig.pose.bones[name].matrix=mat;update()
def align(name,a,b):
    bone=rig.data.bones[name]
    q=(bone.tail_local-bone.head_local).normalized().rotation_difference((b-a).normalized()) @ rest[name].to_quaternion()
    set_world(name,transform(a,q))

reach_stats={}
def solve_chain(a_name,b_name,target,pole,arm_side=None):
    a=rig.pose.bones[a_name].head.copy(); target=Vector(target)
    l1=rig.data.bones[a_name].length;l2=rig.data.bones[b_name].length
    d=target-a;length=d.length;axis=d.normalized()
    dist=max(abs(l1-l2)+.002,min(length,l1+l2-.002))
    actual=a+axis*dist
    pole=Vector(pole);bend=pole-a-axis*(pole-a).dot(axis)
    if arm_side:
        sign=1 if arm_side=='l' else -1
        bend=Vector((0,-1,0)).rotation_difference(axis)@Vector((sign*.62,0,-.78))
    if bend.length<.001: bend=Vector((0,-1,0))-axis*axis.dot(Vector((0,-1,0)))
    bend.normalize();along=(l1*l1-l2*l2+dist*dist)/(2*dist)
    height=math.sqrt(max(0,l1*l1-along*along))
    elbow=a+axis*along+bend*height
    align(a_name,a,elbow);align(b_name,elbow,actual)
    return actual-target

def arm(side,grip,orientation,front_clearance=0):
    wn='weapon_'+side;hn='hand_'+side
    desired=transform(grip,orientation @ rest[wn].to_quaternion())
    h=desired @ rest[wn].inverted() @ rest[hn]
    # Pole puts elbow beside the rib cage, away from face and weapon path.
    shoulder=rig.pose.bones['upper_arm_'+side].head.copy()
    clearance=max(0,h.translation.y-shoulder.y+front_clearance) if front_clearance else 0
    h.translation.y-=clearance
    sign=1 if side=='l' else -1
    pole=shoulder+Vector((sign*.32,.16,-.20))
    shift=solve_chain('upper_arm_'+side,'forearm_'+side,h.translation,pole,arm_side=side)
    h.translation+=shift
    set_world(hn,h)
    # Keep weapon node rigidly attached with its original local bind transform.
    rig.pose.bones[wn].matrix_basis=Matrix.Identity(4);update()
    return rig.pose.bones[wn].head.copy(),shift.length

def paired_grip(grip,q,offset,other_q=None):
    # Authoring-only projection keeps BOTH wrists within their unchanged chain
    # reach, so baked support hands cannot slide away from the weapon.
    g=Vector(grip);other_q=other_q or q
    for _ in range(32):
        for side,pos,ori in [('r',g,q),('l',g+q@Vector(offset),other_q)]:
            wn='weapon_'+side;hn='hand_'+side
            wrist=(transform(pos,ori@rest[wn].to_quaternion()) @ rest[wn].inverted() @ rest[hn]).translation
            shoulder=rig.pose.bones['upper_arm_'+side].head
            delta=wrist-shoulder;limit=rig.data.bones['upper_arm_'+side].length+rig.data.bones['forearm_'+side].length-.004
            if delta.length>limit: g+=delta.normalized()*(limit-delta.length)
    return g

HAMMER_SUPPORT=-.30
def hammer_frame(direction):
    z=Vector(direction).normalized()
    x=Vector((0,-1,0));x-=z*x.dot(z);x.normalize()
    return Matrix((x,z.cross(x).normalized(),z)).transposed().to_quaternion()

def hammer_hand_frame(side,grip,q,direction=None):
    shaft=q@Vector((0,0,1))
    forward=Vector(direction) if direction is not None else Vector(grip)-rig.pose.bones['upper_arm_'+side].head
    forward-=shaft*forward.dot(shaft)
    if forward.length<.001:forward=q@Vector((0,-1,0))
    forward.normalize()
    across=shaft*(1 if side=='r' else -1)
    return Matrix((across,forward,across.cross(forward).normalized())).transposed().to_quaternion()

def hammer_arm(side,grip,q):
    wn='weapon_'+side;hn='hand_'+side
    grip=Vector(grip);offset=(rest[hn].inverted()@rest[wn]).translation
    shoulder=rig.pose.bones['upper_arm_'+side].head.copy();shaft=q@Vector((0,0,1));sign=1 if side=='r' else -1
    l1=rig.data.bones['upper_arm_'+side].length;l2=rig.data.bones['forearm_'+side].length
    length=l2+offset.y;cross=sign*offset.z;radius=math.hypot(length,cross)
    a=grip-shaft*(sign*offset.x)-shoulder;projected=a-shaft*a.dot(shaft)
    cosine=(a.length_squared+radius*radius-l1*l1)/(2*radius*projected.length)
    cosine=max(-1,min(1,cosine));u=projected.normalized();v=shaft.cross(u)
    sine=math.sqrt(max(0,1-cosine*cosine))
    across=u*cosine+sign*v*sine
    forward=(length*across-cross*shaft.cross(across))/radius
    hq=hammer_hand_frame(side,grip,q,forward)
    wrist=grip-hq@offset;elbow=wrist-forward*l2
    align('upper_arm_'+side,shoulder,elbow);align('forearm_'+side,elbow,wrist)
    set_world(hn,transform(wrist,hq))
    set_world(wn,transform(grip,q@rest[wn].to_quaternion()))
    return grip,abs((elbow-shoulder).length-l1)

def hammer_pair(grip,q,floor=None,lock_floor=False):
    g=Vector(grip);shaft=q@Vector((0,0,1))
    forward=q@Vector((1,0,0))
    # Jointly fit both grips into the neutral-wrist reach region. Its meridian
    # is a circle (upper-arm radius) around the forearm+fist offset.
    for _ in range(160 if floor is not None else 64):
        changed=False
        ahead=min((g-rig.pose.bones['upper_arm_'+side].head).dot(forward) for side in ('r','l'))
        if ahead<.115:
            g+=forward*(.115-ahead)
            changed=True
        for side in ('r','l'):
            pos=g if side=='r' else g+shaft*HAMMER_SUPPORT
            sign=1 if side=='r' else -1
            offset=(rest['hand_'+side].inverted()@rest['weapon_'+side]).translation
            l1=rig.data.bones['upper_arm_'+side].length-.0005;l2=rig.data.bones['forearm_'+side].length
            radius=math.hypot(l2+offset.y,offset.z)
            a=pos-shaft*(sign*offset.x)-rig.pose.bones['upper_arm_'+side].head
            z=a.dot(shaft);perp=a-shaft*z;r=perp.length;distance=math.hypot(r-radius,z)
            if distance>l1:
                ratio=l1/distance;g+=perp.normalized()*((r-radius)*(ratio-1))+shaft*(z*(ratio-1))
                changed=True
            elif z*z+(r+radius)**2<l1*l1:
                g+=perp.normalized()*(math.sqrt(l1*l1-z*z)-radius-r)
                changed=True
        if floor is not None and (g.z<floor or lock_floor):
            changed |= abs(g.z-floor)>1e-7
            g.z=floor
        if not changed:break
    right,er=hammer_arm('r',g,q)
    left,el=hammer_arm('l',right+q@Vector((0,0,HAMMER_SUPPORT)),q)
    return max(er,el)

def animate_pose(p,kind,t=0):
    p=retarget_pose(p,kind)
    for b in rig.pose.bones: b.matrix_basis=Matrix.Identity(4)
    hip=rest['hips'].translation.copy()
    dx,dy=p.get('shift',(0,0));hip+=Vector((dx,dy,-p['crouch']))
    hip_rotation=Quaternion((0,0,1),p['twist']*RAD) @ Quaternion((0,1,0),p.get('hip_roll',0)*RAD) @ rest['hips'].to_quaternion()
    hip=transform(hip,hip_rotation)
    set_world('hips',hip)
    targets={}
    lower=0
    for s,foot in zip(('l','r'),p.get('feet',((.14,0,0,0),(-.14,0,0,0)))):
        x,y,lift,rock=foot
        base=rig.data.bones['foot_'+s].head_local
        rotation=Quaternion((1,0,0),rock*RAD)
        depth=min((rotation@(v-base)).z for v in boots[s])
        target=Vector((x,y,lift-depth+.001))
        thigh=rig.pose.bones['thigh_'+s].head
        reach=rig.data.bones['thigh_'+s].length+rig.data.bones['shin_'+s].length-.004
        planar=(thigh.x-x)**2+(thigh.y-y)**2
        lower=max(lower,thigh.z-target.z-math.sqrt(max(.02,reach**2-planar)))
        targets[s]=(target,rotation)
    if lower:
        hip.translation.z-=lower
        set_world('hips',hip)
    for n,f in [('spine',.35),('chest',.65)]:
        b=rig.pose.bones[n];b.rotation_quaternion=Quaternion((0,1,0),p['yaw']*f*RAD) @ Quaternion((1,0,0),p['pitch']*f*RAD);update()
    h=rig.pose.bones['head'].matrix.copy()
    look=Quaternion((0,0,1),p.get('look',0)*RAD) @ Quaternion((1,0,0),(2+p.get('head_pitch',0))*RAD)
    set_world('head',transform(h.translation,look @ rest['head'].to_quaternion()))
    for s,(target,rotation) in targets.items():
        shift=solve_chain('thigh_'+s,'shin_'+s,target,(target.x,-.7,.4))
        set_world('foot_'+s,transform(target+shift,rotation @ rest['foot_'+s].to_quaternion()))
    q=basis(p['rd'],p['roll'])
    if kind=='longsword':
        q=q@sword_basis
    grip=Vector(p['rp'])
    if kind=='warhammer':
        q=hammer_frame(p['rd'])
    contact_z=None
    ground_weight=p.get('ground',0)
    if 'ground' in p and kind in ground_geometry:
        contact_z=.001-min((q@v).z for v in ground_geometry[kind])
        if kind=='warhammer':
            grip.z=max(grip.z,contact_z)
        if ground_weight:
            grip.z=blend(grip.z,contact_z,ground_weight)
    if kind=='warhammer':
        err=hammer_pair(grip,q,contact_z,ground_weight>.999);err2=0
    elif kind=='bow':
        bq=basis(p['ld']);bg,err=arm('l',p['lp'],bq)
        nock=bg+bq@Vector((0,.20+.29*p['draw'],0))
        if isinstance(t,tuple):
            if t[0]=='nock':
                u=max(0,min(1,t[1]/.4));u=u*u*(3-2*u)
                nock=Vector(p['rp']).lerp(nock,u)
            elif t[0]=='skill_nock':
                nock=Vector(p['rp']).lerp(nock,t[1])
            else:
                u=max(0,min(1,(t[1]-.35)/.3));u=u*u*(3-2*u)
                nock=nock.lerp(Vector(p['rp']),u)
        if t=='nock_early': nock=Vector(p['rp'])
        if t=='release_recoil': nock=Vector(p['rp'])
        _,err2=arm('r',nock,bq)
    else:
        actual,err=arm('r',grip,q,front_clearance=0 if kind=='spear' else .095)
        if kind in ('spear','warhammer'):
            support=.32 if kind=='spear' else -.22
            fitted=paired_grip(grip,q,(0,0,support))
            actual,err=arm('r',fitted,q)
            _,err2=arm('l',actual+q@Vector((0,0,support)),q)
        else:
            _,err2=arm('l',p['lp'],basis(p['ld']),front_clearance=.075)
            # Keep the lamp hanging vertically, with subtle keyed pendulum motion.
            n='lamp_pivot'
            m=rig.pose.bones[n].matrix.copy()
            swing=Quaternion((1,0,0),p.get('lamp_swing',0)*RAD)
            set_world(n,transform(m.translation,swing@rest[n].to_quaternion()))
    flare,drift=p.get('cape',(0,0))
    for n,weight in [('skirt_back',1),('skirt_l',.78),('skirt_r',.78)]:
        pb=rig.pose.bones[n]
        wr=Quaternion((1,0,0),(flare*weight-p['pitch']*.18)*RAD) @ Quaternion((0,0,1),drift*RAD)
        set_world(n,transform(pb.head,wr@pb.matrix.to_quaternion()))
    if p.get('spin',0):
        root=rest['root']
        set_world('root',transform(root.translation,Quaternion((0,0,1),p['spin'])@root.to_quaternion()))
    update()
    if p['crouch']>.15 or kind=='warhammer' and p.get('ground',0):
        raise_cape_hem(rig,cape,hem_weights,set_world)
    return max(err,err2)

new_actions={};metrics={}
blade=next(o for o in hero if o.name.startswith('HERO_WEAPON_R_00'))
grip=rig.data.bones['weapon_r'].head_local
sword_tip=max((blade.matrix_world@v.co for v in blade.data.vertices),key=lambda v:(v-grip).length_squared)
sword_basis=(sword_tip-grip).normalized().rotation_difference(Vector((0,0,1)))
common_source=[s for s in json.loads((BASE/'Hero_v1/hero_catalog.json').read_text('utf-8'))['clips'] if s['kind']!='weapon_skill']
old_actions=bake_common_actions(rig,scene,old_actions,common_source,hero,pose,animate_pose,rest,set_world,(sword_tip-grip).normalized())
for spec in specs:
    sharpen_weapon_keys(spec)
register_skills(clip,ready,change,pose)
existing_actions={}
if only_kind:
    metrics=json.loads((OUT/'authoring_metrics.json').read_text('utf-8'))
    wanted={s['name'] for s in specs if s['weapon_kind']!=only_kind}
    import_source=OUT/'_action_import_source.blend'
    shutil.copyfile(OUT/'hero_weapon_animations_v1.blend',import_source)
    with bpy.data.libraries.load(str(import_source),link=False) as (src,dst):
        dst.actions=[n for n in src.actions if n in wanted]
    existing_actions={a.name:a for a in dst.actions}
for spec in specs:
    if only_kind and spec['weapon_kind']!=only_kind:
        new_actions[spec['name']]=existing_actions[spec['name']];continue
    name=spec['name']; act=bpy.data.actions.new(name);act.use_fake_user=True;rig.animation_data.action=act
    max_reach=0;first=None;last_q={}
    divisions=spec.get('sample_divisions',1)
    last_frame=1+(spec['end_frame']-1)*divisions
    for f in range(1,last_frame+1):
        scene.frame_set(f)
        skill='skill_label' in spec
        t=skill_time(spec,1+(f-1)/divisions) if skill else (f-1)/60
        p=interpolate(spec['keyposes'],t,spec)
        mode=0
        if name=='HERO_Bow_Nock': mode=('nock',t)
        if name=='HERO_Bow_Ready': mode='nock_early'
        if name=='HERO_Bow_Release' and t>.10: mode='release_recoil'
        if name=='HERO_Bow_Lower': mode=('lower',t)
        if skill:
            p=direct_skill(spec,p,t)
            skill_mode=bow_mode(spec,p,t)
            if skill_mode is not None:mode=skill_mode
        else:
            p=direct_weapon(spec,p,t)
        max_reach=max(max_reach,animate_pose(p,spec['weapon_kind'],mode))
        for b in rig.pose.bones:
            q=b.rotation_quaternion.copy()
            if b.name in last_q and q.dot(last_q[b.name])<0: b.rotation_quaternion=-q
            last_q[b.name]=b.rotation_quaternion.copy()
            b.keyframe_insert('location',frame=f,group=b.name)
            b.keyframe_insert('rotation_quaternion',frame=f,group=b.name)
            b.keyframe_insert('scale',frame=f,group=b.name)
    for layer in act.layers:
        for strip in layer.strips:
            for bag in strip.channelbags:
                for fc in bag.fcurves:
                    for k in fc.keyframe_points:k.interpolation='LINEAR'
    for ev in spec['events']: act.pose_markers.new(ev['name']).frame=ev['frame']
    new_actions[name]=act;metrics[name]={'maximum_authored_target_reach_projection_m':max_reach}
    print('BAKED',name,spec['end_frame'],max_reach,flush=True)

def tracks(obj,actions):
    obj.animation_data_create();obj.animation_data.action=None
    for tr in list(obj.animation_data.nla_tracks): obj.animation_data.nla_tracks.remove(tr)
    for name,act in actions.items():
        tr=obj.animation_data.nla_tracks.new();tr.name=name;tr.mute=True
        st=tr.strips.new(name,1,act);st.action_frame_start=1;st.action_frame_end=act.frame_range[1]
        st.blend_type='REPLACE';st.extrapolation='NOTHING'

all_actions={**old_actions,**new_actions}
tracks(rig,all_actions)
rig.animation_data.action=old_actions['HERO_Idle'];scene.frame_set(1)

# Retain original standalone models. In this source only, add skin weights that
# attach each new weapon to the original hero joint with its exact bind matrix.
equipment={}
for kind in ready:
    root=weapons[keys[kind]];group=[];side='l' if kind=='bow' else 'r';bone='weapon_'+side
    mount=Matrix.Translation(rig.data.bones[bone].head_local)
    for o in list(root.children):
        if o.type!='MESH':continue
        o.parent=None;o.matrix_world=Matrix.Identity(4)
        o.data.transform(mount)
        vg=o.vertex_groups.new(name=bone);vg.add(list(range(len(o.data.vertices))),1,'REPLACE')
        mod=o.modifiers.new('HeroRigidBind','ARMATURE');mod.object=rig
        o.parent=rig;group.append(o)
    equipment[kind]=group

bowbody=weapons[keys['bow']+'__Body'];bowstring=weapons[keys['bow']+'__String']
bowgrip=rig.data.bones['weapon_l'].head_local
for o in [bowbody,bowstring]:
    o.shape_key_add(name='Basis');drawkey=o.shape_key_add(name='Draw')
    for i,v in enumerate(o.data.vertices):
        local=v.co-bowgrip;z=local.z
        if o==bowstring:
            # String mesh contains short segments: move central nock most.
            fraction=min(1,abs(z)/.65)
            weight=max(0,1-fraction)
            drawkey.data[i].co.y+=.29*weight+.055*fraction*fraction
            drawkey.data[i].co.z-=math.copysign(.018*fraction*fraction,z)
        else:
            weight=min(1,abs(z)/.66)
            drawkey.data[i].co.y+=.055*weight*weight
            drawkey.data[i].co.z-=math.copysign(.018*weight*weight,z)

arrow=weapons[keys['arrow']+'__Body'];arrow.parent=None;arrow.matrix_world=Matrix.Identity(4)
arrow.data.transform(Matrix.Translation(rig.data.bones['weapon_r'].head_local))
vg=arrow.vertex_groups.new(name='weapon_r');vg.add(list(range(len(arrow.data.vertices))),1,'REPLACE')
mod=arrow.modifiers.new('HeroArrowBind','ARMATURE');mod.object=rig;arrow.parent=rig
arrow.shape_key_add(name='Basis');hide_arrow=arrow.shape_key_add(name='Hide')
for v in hide_arrow.data: v.co=rig.data.bones['weapon_r'].head_local
bow_specs=[s for s in specs if s['weapon_kind']=='bow']
for o in [bowbody,bowstring]:
    acts={}
    for s in bow_specs:
        datablock=o.data.shape_keys;datablock.animation_data_create();a=bpy.data.actions.new(s['name']+'__'+o.name);datablock.animation_data.action=a
        for f in range(1,s['end_frame']+1):
            t=(f-1)/60;v=interpolate(s['keyposes'],t,s)['draw'];datablock.key_blocks['Draw'].value=v;datablock.key_blocks['Draw'].keyframe_insert('value',frame=f)
        acts[s['name']]=a
    tracks(o.data.shape_keys,acts)
arrow_actions={}
for s in bow_specs:
    ad=arrow.data.shape_keys;ad.animation_data_create();act=bpy.data.actions.new(s['name']+'__ArrowVisibility');ad.animation_data.action=act
    for f in range(1,s['end_frame']+1):
        t=(f-1)/60;n=s['name']
        visible=bow_arrow_visible(n,t)
        hide_arrow.value=0 if visible else 1;hide_arrow.keyframe_insert('value',frame=f)
    for la in act.layers:
        for st in la.strips:
            for bag in st.channelbags:
                for fc in bag.fcurves:
                    for kp in fc.keyframe_points:kp.interpolation='CONSTANT'
    arrow_actions[s['name']]=act
tracks(arrow.data.shape_keys,arrow_actions)

def select_only(obs):
    bpy.ops.object.select_all(action='DESELECT')
    for o in obs: o.hide_set(False);o.hide_viewport=False;o.select_set(True)
    bpy.context.view_layer.objects.active=rig
def export(path,obs):
    select_only(obs)
    bpy.ops.export_scene.gltf(filepath=str(path),export_format='GLB',use_selection=True,export_animations=True,export_animation_mode='NLA_TRACKS',export_frame_range=False,export_force_sampling=True,export_optimize_animation_size=False,export_anim_slide_to_zero=True,export_yup=True,export_skins=True,export_morph=True,export_morph_animation=True,export_extras=True,export_lights=True)

tracks(rig,all_actions)
base_objects=[rig]+hero+([lantern_light] if lantern_light else [])
export(OUT/'hero_weapon_animation_library.glb',base_objects)
timed_exports=[OUT/'hero_weapon_animation_library.glb']
for kind in ready:
    if only_kind and kind!=only_kind:continue
    acts={n:a for n,a in new_actions.items() if next(s['weapon_kind'] for s in specs if s['name']==n)==kind}
    tracks(rig,acts)
    parts=[o for o in hero if not o.name.startswith('HERO_WEAPON_R_') and (kind not in ('bow','spear','warhammer') or not o.name.startswith('HERO_WEAPON_L_'))]
    selected=[rig]+parts+equipment[kind]+([arrow] if kind=='bow' else [])
    if lantern_light and kind not in ('bow','spear','warhammer'):
        selected.append(lantern_light)
    path=OUT/('hero_'+kind+'_actions.glb')
    export(path,selected)
    timed_exports.append(path)

for spec in specs:
    divisions=spec.get('sample_divisions',1)
    if divisions==1 or only_kind and spec['weapon_kind']!=only_kind:continue
    action=new_actions[spec['name']]
    for layer in action.layers:
        for strip in layer.strips:
            for bag in strip.channelbags:
                for curve in bag.fcurves:
                    for key in curve.keyframe_points:
                        key.co.x=1+(key.co.x-1)/divisions
                    curve.update()

tracks(rig,all_actions)
for group in equipment.values():
    for o in group:o.hide_render=True;o.hide_set(True)
arrow.hide_render=True;arrow.hide_set(True)
for o in hero:o.hide_render=False;o.hide_set(False)
rig.animation_data.action=old_actions['HERO_Idle'];scene.frame_set(1)
scene.frame_start=1;scene.frame_end=121
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'hero_weapon_animations_v1.blend'))
if only_kind:import_source.unlink()

clip_metadata=[{k:v for k,v in s.items() if k!='keyposes'} for s in specs]
common_skill=[s for s in clip_metadata if s['weapon_kind']=='longsword']
equipped_clips=[s for s in clip_metadata if s['weapon_kind']!='longsword']
catalog=dict(schema_version=1,date='2026-11-10',animation_revision='20261110_five_head_hero',asset_id='HERO_WeaponActions_v1',rig=rig.name,bones=list(rest),hero_source_hashes=source_hashes,bind_revision='five_head_hero',animation_sample_fps=60,axes=dict(blender='Z up, -Y front',gltf='Y up, +Z front'),library_file='hero_weapon_animation_library.glb',original_clips=list(old_actions),common_clips=common_source+common_skill,new_clip_count=len(equipped_clips),new_clips=equipped_clips,skills=[s for s in clip_metadata if 'skill_label' in s],locomotion=dict(walk='HERO_Walk',run='HERO_Run',direction='actor rotation in game',weapon_locomotion_duplicates=False,layering='For moving with two-handed equipment, use shared Walk/Run lower-body and weapon Ready upper-body mask.'),upper_body_mask=['spine','chest','neck','head','jaw','clavicle_l','upper_arm_l','forearm_l','hand_l','weapon_l','lamp_pivot','lamp_body','clavicle_r','upper_arm_r','forearm_r','hand_r','weapon_r'],equipment=[])
for kind in ready:
    catalog['equipment'].append(dict(weapon_kind=kind,weapon_key=keys[kind],file='hero_'+kind+'_actions.glb',clip_names=[s['name'] for s in specs if s['weapon_kind']==kind],hand='left' if kind=='bow' else 'right',two_handed=kind in ('bow','warhammer','spear'),hide_original_meshes=[o.name for o in hero if o.name.startswith('HERO_WEAPON_R_') or (kind in ('bow','warhammer','spear') and o.name.startswith('HERO_WEAPON_L_'))],hide_lamp_light=kind in ('bow','warhammer','spear'),weapon_parts=[o.name for o in equipment[kind]],shared_bone_bind='Original hero rest skin unchanged. Equipped weapon meshes are rigid-weighted in this pack; do not add a second static weapon.',bow_morph='Draw 0..1, baked per action on Body and String' if kind=='bow' else None,arrow_mesh=arrow.name if kind=='bow' else None))
    if kind=='warhammer':catalog['equipment'][-1].update(grip_revision='20261105_cross_palm',support_offset_weapon_local_z_m=HAMMER_SUPPORT,grip_description='Opposed cross-palm grips, neutral wrists; hammer face aligned with swing plane')
    catalog['equipment'][-1]['ready_foot_anchors_blender_xy']=[[.155*.90,-STANCE[kind][0]*1.23],[-.155*.90,STANCE[kind][1]*1.23]]
    catalog['equipment'][-1]['shared_bone_bind']='Five-head hero bind. Weapon meshes use rigid weights on the original named weapon sockets.'

common_names=list(old_actions)+[s['name'] for s in common_skill]
tracks(rig,{name:all_actions[name] for name in common_names})
for obj in list(bpy.data.objects):
    if obj not in keep:bpy.data.objects.remove(obj,do_unlink=True)
for name,action in new_actions.items():
    if name not in common_names:bpy.data.actions.remove(action)
common_path=OUT/'hero_rigged.glb'
export(common_path,base_objects)
timed_exports.append(common_path)
rig.animation_data.action=old_actions['HERO_Idle']
scene.frame_set(1)
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'hero_v1.blend'))
hero_metadata=json.loads((BASE/'Hero_v1/hero_catalog.json').read_text('utf-8'))
hero_metadata.update(height_without_hood_m=HEIGHT,hood_height_m=HOOD_TOP,
                     head_height_m=HEIGHT-HEAD_CHIN,body_to_head_ratio=HEIGHT/(HEIGHT-HEAD_CHIN),
                     style='five-head tailored dark fantasy',animation_revision=catalog['animation_revision'],
                     rig_compatibility='33 original bone names and hierarchy, revised bind and skin',
                     clips=catalog['common_clips'])
hero_metadata['bones']=[dict(name=b.name,parent=b.parent.name if b.parent else None,
                             head_z_up=list(b.head_local),tail_z_up=list(b.tail_local)) for b in rig.data.bones]
for part in hero_metadata['parts']:
    obj=bpy.data.objects[part['mesh']]
    obj.data.calc_loop_triangles()
    part.update(vertices=len(obj.data.vertices),triangles=len(obj.data.loop_triangles),
                binding='tailored_five_head_bind')
(OUT/'hero_catalog.json').write_text(json.dumps(hero_metadata,ensure_ascii=False,indent=2),encoding='utf-8')
(OUT/'animation_catalog.json').write_text(json.dumps(catalog,ensure_ascii=False,indent=2),encoding='utf-8')
(OUT/'authoring_metrics.json').write_text(json.dumps(metrics,indent=2),encoding='utf-8')
(OUT/'authoring_keyposes.json').write_text(json.dumps(specs,indent=2),encoding='utf-8')
import runpy
if not only_kind or only_kind=='bow':runpy.run_path(str(PACKAGE/'scripts/bake_bow_channels.py'),init_globals={'OUTPUT_DIR':OUT})
runpy.run_path(str(PACKAGE/'scripts/exact_skill_times.py'),init_globals={'EXPORTS':timed_exports,'SKILLS':catalog['skills']})
print('DELIVERED',len(all_actions),'hero actions',flush=True)
