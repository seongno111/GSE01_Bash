"""Shared locomotion, sword and reaction performances for the original hero."""
import bpy
from math import sin, cos, pi
from mathutils import Quaternion, Vector
from dynamic_motion import ease, window, pace


def blend(a, b, weight):
    if isinstance(a, (list, tuple)):
        return tuple(blend(x, y, weight) for x, y in zip(a, b))
    return a + (b - a) * weight


def key_pose(keys, phase, contacts=()):
    for i,((start, a), (end, b)) in enumerate(zip(keys, keys[1:])):
        if phase <= end:
            mode=('strike' if end in contacts else 'carry' if start in contacts else
                  'settle' if i==len(keys)-2 else 'gather')
            weight=pace((phase-start)/(end-start),mode)
            return {name:blend(a[name],b[name],weight) for name in a}
    return keys[-1][1]


def gait_foot(phase, side, run):
    p = (phase + (.5 if side < 0 else 0)) % 1
    duty = .41 if run else .61
    stride = .312 if run else .195
    if p < duty:
        u = p / duty
        y = stride * (2 * u - 1)
        lift = 0
        rock = -9 * (1 - ease(u / .22)) + (19 if run else 13) * ease((u - .58) / .42)
    else:
        u = (p - duty) / (1 - duty)
        y = stride * (1 - 2 * pace(u))
        lift = (.244 if run else .143) * sin(pi * u) ** 1.45
        rock = blend(19 if run else 13, -9, ease(u))
    return side * .147, y, lift, rock


def make_hero_pose(pose, **kwargs):
    p = pose(rp=(-.382,-.18,.89), rd=(-.19,-.94,.36), lp=(.424,-.22,.89),
             ld=(.02,-.25,.97), crouch=.026, yaw=2, pitch=3)
    p.update(shift=(0,0), feet=((.147,0,0,0),(-.147,0,0,0)), hip_roll=0,
             look=0, head_pitch=0, cape=(2,0), lamp_swing=0)
    p.update(kwargs)
    return p


CAPE_HEMS=('skirt_back','skirt_l','skirt_r')


def cape_floor_weights(cape):
    groups={g.index for g in cape.vertex_groups if g.name in CAPE_HEMS}
    return [sum(g.weight for g in v.groups if g.group in groups) for v in cape.data.vertices]


def raise_cape_hem(rig, cape, hem_weights, set_world):
    drape=cape.evaluated_get(bpy.context.evaluated_depsgraph_get())
    lift=max((.001-(drape.matrix_world@v.co).z)/w for v,w in zip(drape.data.vertices,hem_weights) if w>.08)
    if lift>0:
        for hem in CAPE_HEMS:
            m=rig.pose.bones[hem].matrix.copy()
            m.translation.z+=min(.46,lift)
            set_world(hem,m)


def bake_common_actions(rig, scene, original, metadata, hero, pose, animate_pose, rest, set_world, blade_direction):
    def make(**kwargs):
        return make_hero_pose(pose, **kwargs)

    standing = make()

    def idle(u):
        breath = (1-cos(2*pi*u)) / 2
        sway = sin(2*pi*u)
        return make(shift=(.022*sway,-.009*breath), crouch=.026+.018*breath,
                    twist=-4*sway, yaw=2+6*sway, pitch=3+3*breath,
                    look=5*sway, head_pitch=2*breath, cape=(2+4*breath,5*sway),
                    lp=(.424,-.22-.025*breath,.89+.025*breath), lamp_swing=5*sway)

    def gait(u, run):
        wave = cos(2*pi*u)
        side = sin(2*pi*u)
        bob = cos(4*pi*u-.76)
        arm_swing = (.145 if run else .090)*wave
        return make(feet=tuple(gait_foot(u,sign,run) for sign in (1,-1)),
                    crouch=(.091 if run else .056)+(.039 if run else .023)*bob,
                    shift=((.035 if run else .027)*side,-.033 if run else -.008),
                    hip_roll=-(5 if run else 3)*side, twist=(9 if run else 6)*wave,
                    yaw=-(16 if run else 11)*wave, pitch=(19 if run else 9)+4*bob,
                    look=3*wave, head_pitch=2*bob,
                    rp=(-.375,-.208+arm_swing,.92+.055*wave),
                    rd=(-.21,-.89,.42+.11*wave),
                    lp=(.414,-.24-.60*arm_swing,.91-.047*wave),
                    cape=((20 if run else 11)+6*bob,10*side),
                    lamp_swing=(16 if run else 9)*sin(2*pi*u-.6))

    def transition(u, start):
        endpoint = gait(.5 if start else 0,False)
        weight = pace(u,'strike' if start else 'settle')
        a, b = (standing,endpoint) if start else (endpoint,standing)
        p = {name:blend(a[name],b[name],weight) for name in a}
        impulse = sin(pi*u)**2
        p['crouch'] += (.046 if start else .066)*impulse
        p['pitch'] += (11 if start else 15)*impulse
        p['shift'] = (p['shift'][0]-.022*impulse,p['shift'][1]-.076*impulse)
        p['cape'] = (p['cape'][0]+17*impulse,9*impulse)
        p['lamp_swing'] += (-17 if start else 21)*impulse
        feet = list(p['feet'])
        index = 1 if start else 0
        x,y,z,rock = feet[index]
        feet[index] = (x,y,z+.091*impulse,rock-9*impulse)
        p['feet'] = tuple(feet)
        return p

    sword_stance = ((.153,-.025,0,0),(-.153,.03,0,0))
    wind = make(rp=(-.431,-.10,1.25),rd=(-.50,.21,.84),lp=(.459,-.19,.90),
                feet=sword_stance,crouch=.079,yaw=-20,pitch=-5,twist=-10,
                shift=(-.063,.052),look=-6,head_pitch=-5,cape=(-3,12),lamp_swing=12)
    strike = make(rp=(-.195,-.383,1.06),rd=(.43,-.90,.03),lp=(.397,-.18,.82),
                  feet=((.161,-.15,0,0),(-.153,.03,0,0)),crouch=.112,yaw=19,
                  pitch=18,twist=8,shift=(.06,-.116),head_pitch=5,cape=(16,-14),lamp_swing=-21)
    through = make(rp=(-.13,-.28,.97),rd=(.78,-.56,-.27),lp=(.410,-.16,.83),
                   feet=strike['feet'],crouch=.118,yaw=33,pitch=20,twist=14,
                   shift=(.07,-.105),head_pitch=8,cape=(24,-16),lamp_swing=-13)

    def sword(u):
        t = u*.85
        p = key_pose([(0,standing),(.12,make(feet=sword_stance,crouch=.046,shift=(-.018,.01))),
                      (.255,wind),(.40,strike),(.51,through),(.85,standing)],t,contacts=(.40,))
        x,y,z,rock = p['feet'][0]
        p['feet'] = ((x,y,z+.071*window(t,.255,.32,.397)+.052*window(t,.51,.66,.85),rock),p['feet'][1])
        return p

    def guard(u):
        breath=(1-cos(2*pi*u))/2
        sway=sin(2*pi*u)
        return make(feet=((.165,-.093,0,0),(-.160,.074,0,0)),
                    rp=(-.264,-.327,1.062+.028*breath),rd=(.36,-.15,.94),
                    lp=(.426,-.252,.88),crouch=.099+.026*breath,
                    shift=(.024*sway,-.043-.025*breath),yaw=-12+7*sway,
                    twist=-6+4*sway,pitch=12+5*breath,head_pitch=4,
                    cape=(8+5*breath,-6*sway),lamp_swing=7*sway)

    def hit(u):
        recoil=make(rp=(-.445,.007,.89),rd=(-.45,-.67,.62),lp=(.454,-.12,1.02),
                    crouch=.117,shift=(.038,.107),yaw=23,twist=12,pitch=-20,
                    look=-11,head_pitch=-13,hip_roll=8,cape=(14,16),lamp_swing=-24)
        recovery=make(rp=(-.390,-.19,.87),lp=(.419,-.24,.85),
                      crouch=.084,shift=(.013,.022),yaw=-7,pitch=11,
                      head_pitch=7,cape=(17,-7),lamp_swing=19)
        return key_pose([(0,standing),(.20,recoil),(.57,recovery),(1,standing)],u,contacts=(.20,))

    def lamp(u):
        raise_lamp=make(lp=(.39,-.364,1.27),ld=(.07,-.95,.30),
                        shift=(.038,-.058),yaw=17,twist=6,pitch=12,look=22,
                        head_pitch=5,crouch=.067,cape=(9,9),lamp_swing=-11)
        search=dict(raise_lamp,lp=(.412,-.361,1.36),look=36,head_pitch=-6,
                    shift=(.057,-.05),yaw=23,lamp_swing=6,cape=(12,12))
        focus=dict(search,lp=(.364,-.41,1.25),look=12,head_pitch=11,
                   shift=(.03,-.079),lamp_swing=-6,crouch=.082)
        return key_pose([(0,standing),(.29,raise_lamp),(.47,search),(.66,focus),(1,standing)],u)

    directions={'HERO_Idle':idle,'HERO_Walk':lambda u:gait(u,False),
                'HERO_Run':lambda u:gait(u,True),'HERO_MoveStart':lambda u:transition(u,True),
                'HERO_MoveStop':lambda u:transition(u,False),'HERO_AttackLongsword':sword,
                'HERO_Guard':guard,'HERO_Hit':hit,'HERO_LampInspect':lamp}
    rig.animation_data.action=None
    animate_pose(standing,'longsword')
    stand_basis={b.name:b.matrix_basis.copy() for b in rig.pose.bones}
    cape=next(o for o in hero if o.name.startswith('HERO_CAPE_'))
    hem_weights=cape_floor_weights(cape)
    body=[o for o in hero if not o.name.startswith(('HERO_CAPE_','HERO_WEAPON_'))]

    def shift_hips(height):
        m=rig.pose.bones['hips'].matrix.copy()
        m.translation.z+=height
        set_world('hips',m)

    def mesh_floor(objects):
        dg=bpy.context.evaluated_depsgraph_get()
        return min((o.matrix_world@v.co).z for obj in objects for o in [obj.evaluated_get(dg)] for v in o.data.vertices)

    braced=make(feet=((.155,-.10,0,0),(-.165,.065,0,0)),
                crouch=.255,shift=(.042,-.08),pitch=35,twist=-9,yaw=13,
                rp=(-.365,-.23,.69),rd=(-.76,-.64,.12),lp=(.40,-.22,.77),
                head_pitch=12,cape=(34,-8),lamp_swing=-15)
    rising=make(feet=((.149,-.04,0,0),(-.156,.028,0,0)),
                crouch=.101,shift=(.016,-.056),pitch=17,yaw=7,head_pitch=7,
                cape=(10,6),lamp_swing=14)

    def fallen_pose(name, u, start, end):
        # Faster fall after the knee buckle, then a short settling hold. Rising
        # starts with the body low before the legs take the weight.
        if name=='HERO_Death':
            fraction=key_pose([(0,{'v':0}),(.19,{'v':.18}),(.40,{'v':.34}),(.73,{'v':.91}),(.90,{'v':1}),(1,{'v':1})],u)['v']
            lean=sin(pi*fraction)**2
            fade=1-ease(u/.14)
        else:
            fraction=key_pose([(0,{'v':0}),(.18,{'v':.08}),(.41,{'v':.36}),(.74,{'v':.83}),(1,{'v':1})],u)['v']
            lean=-sin(pi*fraction)**2
            fade=ease((u-.83)/.17)
        source=start+(end-start)*fraction
        scene.frame_set(int(source),subframe=source-int(source))
        for b in rig.pose.bones:
            if fade:
                loc,rot,scale=b.matrix_basis.decompose()
                target,orient,target_scale=stand_basis[b.name].decompose()
                b.location=loc.lerp(target,fade)
                b.rotation_quaternion=rot.slerp(orient,fade)
                b.scale=scale.lerp(target_scale,fade)
        bpy.context.view_layer.update()
        hip=rig.pose.bones['hips'];m=hip.matrix.copy()
        rot=Quaternion((0,1,0),lean*.22)@m.to_quaternion()
        m.translation.x+=.055*lean
        x=m.translation.copy()
        set_world('hips',rot.to_matrix().to_4x4())
        m=rig.pose.bones['hips'].matrix.copy()
        m.translation=x
        set_world('hips',m)
        for side,amount in [('l',.20),('r',-.14)]:
            b=rig.pose.bones['upper_arm_'+side]
            b.rotation_quaternion=b.rotation_quaternion@Quaternion((1,0,0),amount*lean)
        bpy.context.view_layer.update()
        collapse=fraction if name=='HERO_Death' else 1-fraction
        grounded=ease((collapse-.54)/.34)
        if grounded:
            # The longer legs settle with the body instead of levering the
            # shoulder blades off the floor through the old short-leg bend.
            for side in ('l','r'):
                for prefix in ('thigh','shin','foot','toe'):
                    bone=rig.pose.bones[prefix+'_'+side]
                    loc,rot,scale=bone.matrix_basis.decompose()
                    angle=(.11 if side=='l' else -.07) if prefix=='foot' else 0
                    neutral=Quaternion((0,1,0),angle)
                    bone.location=loc*(1-grounded)
                    bone.rotation_quaternion=rot.slerp(neutral,grounded)
                    bone.scale=scale.lerp(Vector((1,1,1)),grounded)
            bpy.context.view_layer.update()
            weapon=rig.pose.bones['weapon_r'];m=weapon.matrix.copy()
            world_direction=(m.to_quaternion()@rest['weapon_r'].to_quaternion().inverted())@blade_direction
            target=world_direction.rotation_difference(Vector((-.96,-.27,.03)).normalized())
            rot=Quaternion().slerp(target,grounded)@m.to_quaternion()
            aligned=rot.to_matrix().to_4x4()
            aligned.translation=m.translation
            set_world('weapon_r',aligned)
            shift_hips((.001-mesh_floor(body))*grounded)
        raise_cape_hem(rig,cape,hem_weights,set_world)
        lowest=mesh_floor(hero)
        if lowest<.001:
            shift_hips(.001-lowest)
        if name=='HERO_GetUp' and u>.35:
            old_pose={b.name:b.matrix_basis.copy() for b in rig.pose.bones}
            recovery=key_pose([(.35,braced),(.64,braced),(.82,rising),(1,standing)],u)
            animate_pose(recovery,'longsword')
            weight=1-ease((u-.35)/.29)
            if weight:
                for b in rig.pose.bones:
                    loc,rot,scale=b.matrix_basis.decompose()
                    previous,orient,previous_scale=old_pose[b.name].decompose()
                    b.location=loc.lerp(previous,weight)
                    b.rotation_quaternion=rot.slerp(orient,weight)
                    b.scale=scale.lerp(previous_scale,weight)
                bpy.context.view_layer.update()
            raise_cape_hem(rig,cape,hem_weights,set_world)
            lowest=mesh_floor(hero)
            if lowest<.001:
                shift_hips(.001-lowest)

    results={}
    settled=None
    for item in metadata:
        name=item['name']
        before=original[name]
        lo,hi=before.frame_range
        frames=item['end_frame']
        samples=[]
        if name not in directions:
            rig.animation_data.action=before
        else:
            rig.animation_data.action=None
        for frame in range(1,frames+1):
            u=(frame-1)/(frames-1)
            if name in directions:
                animate_pose(directions[name](u),'longsword')
            else:
                fallen_pose(name,u,lo,hi)
            samples.append([b.matrix_basis.copy() for b in rig.pose.bones])
        if item['loop']:
            samples[-1]=samples[0]
        if name=='HERO_Death':
            samples[0]=list(stand_basis.values())
            settled=samples[-1]
        elif name=='HERO_GetUp':
            if settled:
                samples[0]=settled
            samples[-1]=list(stand_basis.values())
        before.name=name+'__BeforeDynamics'
        act=bpy.data.actions.new(name)
        act.use_fake_user=True
        rig.animation_data.action=act
        previous={}
        for frame,matrices in enumerate(samples,1):
            for b,m in zip(rig.pose.bones,matrices):
                loc,rot,scale=m.decompose()
                if b.name in previous and rot.dot(previous[b.name])<0:
                    rot=-rot
                previous[b.name]=rot
                b.location=loc
                b.rotation_quaternion=rot
                b.scale=scale
                for channel in ('location','rotation_quaternion','scale'):
                    b.keyframe_insert(channel,frame=frame,group=b.name)
        for layer in act.layers:
            for strip in layer.strips:
                for bag in strip.channelbags:
                    for curve in bag.fcurves:
                        for key in curve.keyframe_points:
                            key.interpolation='LINEAR'
        for event in item['events']:
            act.pose_markers.new(event['name']).frame=event['frame']
        results[name]=act
        print('BAKED COMMON',name,frames,flush=True)
    for act in original.values():
        bpy.data.actions.remove(act)
    return results
