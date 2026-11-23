"""Eight skill performances based on the project's weapon skill specification."""
from math import cos, pi, radians, sin
from dynamic_motion import direct_weapon, ease, footwork, window
from hammer_motion import body_keys


def register_skills(clip, ready, change, pose):
    from common_motion import make_hero_pose
    def base(kind):
        if kind == 'longsword':
            start = make_hero_pose(pose)
        else:
            start = direct_weapon(dict(weapon_kind=kind,name='_Ready',duration_seconds=1,loop=True),ready[kind],0)
        return dict(start, ground=0, hip_roll=0, head_pitch=start.get('head_pitch',0))

    def add(kind, action, label, duration, contacts, poses):
        clip(kind, 'Skill'+action, duration, poses,
             events=[('skill_contact',t) for t in contacts], description=label,
             skill_label=label)

    p=base('staff')
    raised=change(p,rp=(-.36,-.18,1.30),rd=(-.22,-.12,1),lp=(.43,-.30,1.0),
                  pitch=-7,yaw=-30,twist=-15,hip_roll=-8,shift=(-.076,.046),crouch=.062,
                  head_pitch=-7,cape=(3,10),lamp_swing=17)
    planted=change(p,rp=(-.32,-.33,.69),rd=(-.07,-.13,1),lp=(.44,-.10,.79),
                   shift=(.056,-.111),crouch=.213,yaw=24,twist=15,hip_roll=7,
                   pitch=27,head_pitch=13,cape=(22,-12),lamp_swing=-24,ground=1)
    pulse=change(planted,rp=(-.35,-.30,.91),shift=(-.008,-.032),pitch=5,crouch=.114,
                 yaw=-9,twist=-6,hip_roll=-3,head_pitch=5,cape=(9,5),lamp_swing=8,ground=0)
    add('staff','AshField','잔불의 문양',1.40,[.45,.80,1.15],
        [(0,p),(.35,raised),(.45,planted),(.52,planted),(.72,pulse),(.80,planted),
         (.86,planted),(1.065,pulse),(1.15,planted),(1.21,planted),(1.40,p)])

    p=base('bow')
    nocked=change(p,lp=(.08,-.21,1.04),yaw=-28,twist=-5,head_pitch=7,crouch=.066)
    loaded=change(p,lp=(.045,-.555,1.245),rp=(.045,-.065,1.245),ld=(0,0,1),
                  yaw=-67,twist=-21,shift=(.054,-.054),crouch=.095,hip_roll=5,draw=1,
                  pitch=4,look=0,cape=(9,-7))
    snap=change(loaded,rp=(.041,-.062,1.245),draw=0,shift=(.014,.001),
                pitch=-3,cape=(14,-4))
    recoil=change(snap,lp=(.065,-.48,1.24),rp=(.005,.082,1.25),yaw=-48,
                  twist=-3,crouch=.086,hip_roll=-4,cape=(18,3),head_pitch=-4)
    add('bow','ArrowFan','가시 부채살',.95,[.55],
        [(0,p),(.22,nocked),(.487,loaded),(32/60,loaded),(.55,snap),(.646,recoil),(.95,p)])

    p=base('mace')
    tucked=change(p,rp=(-.42,-.012,1.20),rd=(-.36,.31,1),lp=(.42,-.29,.97),
                  yaw=-37,twist=-16,hip_roll=-8,shift=(-.084,.063),
                  crouch=.135,pitch=-4,head_pitch=-5,cape=(4,12))
    drive=change(p,rp=(-.22,-.399,1.01),rd=(.07,-.91,-.41),lp=(.47,-.10,.84),
                 yaw=33,twist=18,hip_roll=9,shift=(.082,-.144),
                 crouch=.170,pitch=27,cape=(18,-8),lamp_swing=-27)
    checked=change(drive,rp=(-.19,-.337,.95),rd=(.23,-.78,-.58),shift=(.035,-.084),
                   crouch=.142,pitch=18,lamp_swing=8)
    add('mace','GuardBreak','방패 분쇄',.82,[.38],
        [(0,p),(.28,tucked),(.38,drive),(.46,checked),(.52,checked),(.82,p)])

    p=base('axe')
    coiled=change(p,rp=(-.42,-.044,1.19),rd=(-.93,.29,.31),lp=(.42,-.27,.95),
                  yaw=-46,twist=-22,hip_roll=-10,shift=(-.097,.066),
                  crouch=.139,pitch=3,look=-4,cape=(-3,17),roll=-68)
    across=change(coiled,shift=(.023,-.078),twist=3,hip_roll=6,crouch=.166,pitch=13)
    cut=change(p,rp=(-.17,-.40,1.02),rd=(.42,-.90,-.12),lp=(.47,-.07,.82),
               yaw=34,twist=19,hip_roll=11,shift=(.088,-.146),crouch=.169,
               pitch=26,cape=(22,-16),lamp_swing=-28,roll=-64)
    through=change(cut,rp=(-.14,-.23,.93),rd=(.94,.07,-.28),yaw=40,twist=17,
                   shift=(.078,-.055),crouch=.133,cape=(23,-19),lamp_swing=7)
    add('axe','Execution','사형 집행',1.05,[.55],
        [(0,p),(.35,coiled),(.463,across),(.55,cut),(.635,through),(.70,through),(1.05,p)])

    p=base('warhammer')
    add('warhammer','GroundSlam','대지 강타',1.35,[.72],
        body_keys(p,change,'SkillGroundSlam',.72,1.35))

    p=base('spear')
    chamber=change(p,rp=(-.25,.109,.96),rd=(.26,-.96,.04),yaw=-53,twist=-23,
                   shift=(-.055,.092),hip_roll=-8,crouch=.103,pitch=0,cape=(1,12))
    reach=change(p,rp=(-.115,-.36,1.01),rd=(.02,-1,.02),yaw=-33,twist=5,
                 shift=(.071,-.169),hip_roll=9,crouch=.177,pitch=28,cape=(18,-10))
    retract=change(p,rp=(-.17,-.10,.99),rd=(.13,-.99,.055),yaw=-40,twist=-7,
                   shift=(.031,-.072),crouch=.121,pitch=15,cape=(12,-6),feet=reach['feet'])
    add('spear','LongThrust','잿빛 관통',.86,[.40],
        [(0,p),(.289,chamber),(.40,reach),(.49,retract),(.55,retract),(.86,p)])

    p=base('dagger')
    low=change(p,rp=(-.41,-.087,1.018),rd=(-.17,-.90,.35),yaw=-28,twist=-16,
               shift=(-.053,.024),hip_roll=-7,crouch=.148,pitch=9,cape=(4,7))
    first=change(p,rp=(-.24,-.422,.981),rd=(.06,-1,.02),yaw=30,twist=15,
                 shift=(.050,-.103),hip_roll=7,crouch=.148,pitch=24,cape=(13,-7),lamp_swing=-18)
    second=change(first,rp=(-.27,-.352,1.04),rd=(-.52,-.84,.025),yaw=-27,twist=-13,
                  shift=(-.052,-.105),hip_roll=-7,pitch=16,cape=(15,12),lamp_swing=18)
    final=change(first,rp=(-.207,-.463,.975),rd=(.025,-1,.015),yaw=39,twist=21,
                 shift=(.072,-.146),hip_roll=11,crouch=.187,cape=(20,-14),lamp_swing=-28)
    add('dagger','Flurry','세 번의 상흔',.67,[.14,.27,.40],
        [(0,p),(.083,low),(.14,first),(.211,change(first,rp=(-.18,-.27,1.055),rd=(.85,-.52,.07))),
         (.27,second),(.344,change(low,shift=(-.041,-.074))),(.40,final),
         (.475,change(final,rp=(-.23,-.322,.94),pitch=28)),(.67,p)])

    p=base('longsword')
    coil=change(p,rp=(-.411,-.17,1.08),rd=(-.73,-.61,.22),yaw=-28,twist=-11,
                shift=(-.019,.008),crouch=.105,pitch=6,cape=(1,14))
    edge=change(p,rp=(-.29,-.19,1.00),rd=(-.98,-.19,.04),yaw=17,twist=9,
                shift=(.035,-.021),hip_roll=5,crouch=.108,pitch=12,lp=(.39,-.070,.92),
                cape=(23,-16),lamp_swing=21)
    collect=change(coil,rp=(-.33,-.16,1.08),rd=(-.79,-.37,.23),yaw=-17,crouch=.096)
    decel=change(edge,rp=(-.32,-.20,.93),rd=(-.70,-.70,.18),crouch=.115,
                 yaw=19,twist=9,cape=(25,-13),lamp_swing=-13)
    add('longsword','Whirlwind','잿바람 회전베기',1.12,[.35,.75],
        [(0,p),(.15,coil),(.28,collect),(.35,edge),(.53,collect),
         (.66,collect),(.75,edge),(.91,decel),(1.02,decel),(1.12,p)])


def skill_time(spec, frame):
    anchors=[(1,0)]+[(e['frame'],e['time_seconds']) for e in spec['events']]
    anchors.append((spec['end_frame'],spec['duration_seconds']))
    for (a,x),(b,y) in zip(anchors,anchors[1:]):
        if frame<=b:
            return x+(y-x)*(frame-a)/(b-a)
    return spec['duration_seconds']


def bow_mode(spec, p, t):
    if spec['name']!='HERO_Bow_SkillArrowFan':
        return None
    if t>=.55:
        return 'release_recoil'
    if t<.22:
        return ('skill_nock',ease(t/.22))
    return 0


def bow_arrow_visible(name,t):
    if name=='HERO_Bow_SkillArrowFan':
        return .18<=t<.55
    return (name in ('HERO_Bow_Draw','HERO_Bow_Aim')
            or (name=='HERO_Bow_Nock' and t>=.25)
            or (name=='HERO_Bow_Release' and t<.1)
            or (name=='HERO_Bow_Lower' and t<.5))


SKILL_STEPS = {
    # Start and land before impact. Hold the lead anchor through the follow-through.
    'staff': (.219, .411, 1.215, .094, .063, 13),
    'mace': (.207, .338, .523, .163, .076, 23),
    'axe': (.294, .489, .702, .149, .081, 24),
    'warhammer': (.273, .588, 1.021, .143, .083, 15),
    'spear': (.201, .359, .557, .242, .094, 23),
    'dagger': (.018, .120, .475, .153, .062, 18),
}


def whirlwind_turns(t):
    # The blade crosses the facing direction at the same angle on both contacts.
    # Slopes in turns per second keep the rotation continuous through each cut.
    knots = [(.13, 0, 0), (.35, .23, 2.65), (.55, .73, 1.68),
             (.75, 1.23, 2.65), (.93, 1.78, 2.82), (1.05, 2, 0)]
    if t <= knots[0][0]:
        return 0
    for (start, a, va), (end, b, vb) in zip(knots, knots[1:]):
        if t <= end:
            span = end - start
            u = (t - start) / span
            return (a + ease(u) * (b - a)
                    + span * u * (1 - u) * ((1 - u) * va - u * vb))
    return 2


def whirlwind_feet(feet, angle):
    # Alternate the support leg every sixth of a revolution. Undo the root turn
    # below each ankle to keep its world anchor fixed while it bears the weight.
    result = []
    for side, (x, y, z, rock) in enumerate(feet):
        heading = 0
        air = 0
        for step in range(side, 12, 2):
            start = step * 60
            if angle < start:
                break
            target = 720 if step >= 10 else start + 90
            travel = ease((angle - start) / 60)
            heading += (target - heading) * travel
            if angle < start + 60:
                air = sin(pi * travel)
                break
        offset = radians(heading - angle)
        # Rotate in final rig dimensions, then undo the stance scaling applied by retarget_pose.
        result.append((x * cos(offset) - y * 1.23 / .90 * sin(offset),
                       x * .90 / 1.23 * sin(offset) + y * cos(offset),
                       z + .079 * air, rock - 11 * air))
    return tuple(result)


def direct_skill(spec, p, t):
    kind = spec['weapon_kind']
    if kind in SKILL_STEPS:
        start, plant, recover, distance, lift, rock = SKILL_STEPS[kind]
        p = footwork(p, t, start, plant, recover, spec['duration_seconds'], distance, lift, rock)
    elif kind == 'longsword':
        turns = whirlwind_turns(t)
        p['spin'] = 2 * pi * turns
        p['feet'] = whirlwind_feet(p['feet'], 360 * turns)
    elif kind == 'bow':
        brace = window(t, .13, .38, .65)
        p['hip_roll'] += 3 * brace
    return p
