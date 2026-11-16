"""Eight skill performances based on the project's weapon skill specification."""
from math import pi, sin
from dynamic_motion import direct_weapon, ease


def register_skills(clip, ready, change, pose):
    from common_motion import make_hero_pose
    def base(kind):
        if kind == 'longsword':
            start = make_hero_pose(pose)
        else:
            start = direct_weapon(dict(weapon_kind=kind,name='_Ready',duration_seconds=1,loop=True),ready[kind],0)
        return dict(start,ground=0,head_pitch=start.get('head_pitch',0))

    def add(kind, action, label, duration, contacts, poses):
        clip(kind, 'Skill'+action, duration, poses,
             events=[('skill_contact',t) for t in contacts], description=label,
             skill_label=label)

    p=base('staff')
    raised=change(p,rp=(-.38,-.22,1.26),rd=(-.22,-.12,1),pitch=0,yaw=-22,twist=-8,
                  shift=(-.043,.035),crouch=.058,look=-8,head_pitch=-7,cape=(3,8),lamp_swing=12)
    planted=change(p,rp=(-.32,-.33,.69),rd=(-.07,-.13,1),shift=(.018,-.064),crouch=.18,
                   yaw=17,twist=8,pitch=19,head_pitch=13,cape=(15,-6),lamp_swing=-19,ground=1)
    pulse=change(planted,rp=(-.32,-.33,.70),shift=(.009,-.04),pitch=9,crouch=.138,
                 head_pitch=5,cape=(9,-3),lamp_swing=4)
    add('staff','AshField','잔불의 문양',1.40,[.45,.80,1.15],
        [(0,p),(.30,raised),(.45,planted),(.66,pulse),(.80,planted),
         (1.01,pulse),(1.15,planted),(1.40,p)])

    p=base('bow')
    nocked=change(p,lp=(.08,-.21,1.04),yaw=-28,twist=-5,head_pitch=7,crouch=.066)
    loaded=change(p,lp=(.045,-.555,1.245),rp=(.045,-.065,1.245),ld=(0,0,1),
                  yaw=-67,twist=-11,shift=(.03,-.03),crouch=.075,draw=1,
                  pitch=4,look=0,cape=(9,-7))
    snap=change(loaded,rp=(.031,-.026,1.252),draw=0,shift=(.014,.001),
                pitch=-3,cape=(14,-4))
    recoil=change(snap,lp=(.065,-.48,1.24),rp=(.005,.082,1.25),yaw=-54,
                  twist=-7,crouch=.086,cape=(18,3),head_pitch=-4)
    add('bow','ArrowFan','가시 부채살',.95,[.55],
        [(0,p),(.22,nocked),(.465,loaded),(32/60,loaded),(.55,snap),(.66,recoil),(.95,p)])

    p=base('mace')
    tucked=change(p,rp=(-.27,-.09,1.075),rd=(-.28,.06,1),yaw=-25,twist=-10,
                  shift=(-.055,.047),crouch=.118,pitch=1,head_pitch=-5,cape=(4,7))
    drive=change(p,rp=(-.22,-.383,1.01),rd=(.07,-.91,-.41),yaw=23,twist=12,
                 shift=(.038,-.105),crouch=.123,pitch=23,cape=(18,-8),lamp_swing=-22)
    checked=change(drive,rp=(-.19,-.337,.95),rd=(.23,-.78,-.58),shift=(.035,-.084),
                   crouch=.142,pitch=18,lamp_swing=8)
    add('mace','GuardBreak','방패 분쇄',.82,[.38],[(0,p),(.25,tucked),(.38,drive),(.49,checked),(.82,p)])

    p=base('axe')
    coiled=change(p,rp=(-.44,-.06,1.16),rd=(-.93,.19,.31),yaw=-39,twist=-15,
                  shift=(-.074,.051),crouch=.121,pitch=7,look=-9,cape=(-2,14),roll=-68)
    across=change(p,rp=(-.30,-.348,1.04),rd=(.06,-.995,.07),yaw=3,twist=4,
                  shift=(.003,-.079),crouch=.142,pitch=16,cape=(13,5),roll=-69)
    cut=change(p,rp=(-.17,-.36,.97),rd=(.88,-.47,-.09),yaw=30,twist=15,
               shift=(.071,-.104),crouch=.148,pitch=22,cape=(22,-16),lamp_swing=-22,roll=-64)
    through=change(cut,rp=(-.14,-.23,.93),rd=(.94,.07,-.28),yaw=40,twist=17,
                   shift=(.078,-.055),crouch=.133,cape=(23,-19),lamp_swing=7)
    add('axe','Execution','사형 집행',1.05,[.55],
        [(0,p),(.33,coiled),(.46,across),(.55,cut),(.69,through),(1.05,p)])

    p=base('warhammer')
    raised=change(p,rp=(-.35,-.258,1.39),rd=(-.61,-.03,.79),yaw=-10,twist=-7,
                  shift=(-.045,.02),crouch=.039,pitch=-8,head_pitch=-8,cape=(-3,10))
    swinging=change(p,rp=(-.246,-.37,1.055),rd=(-.69,-.68,.24),yaw=-7,twist=-5,
                    shift=(.003,-.038),crouch=.162,pitch=13,cape=(12,-2))
    drop=change(p,rp=(-.19,-.38,.79),rd=(-.56,-.61,-.55),yaw=-6,twist=-3,
                shift=(.012,-.035),crouch=.268,pitch=21,cape=(15,-5))
    ground=change(p,rp=(-.115,-.468,.635),rd=(-.53,-.45,-.72),yaw=-6,twist=-2,
                  shift=(.025,-.048),crouch=.399,pitch=25,head_pitch=16,cape=(25,-8),ground=1)
    absorb=change(ground,rp=(-.117,-.465,.635),crouch=.405,pitch=26,head_pitch=17,cape=(26,-10))
    heave=change(p,rp=(-.215,-.412,.84),rd=(-.67,-.61,-.38),yaw=-6,twist=-3,
                 shift=(.019,-.042),crouch=.243,pitch=23,head_pitch=9,cape=(18,-5))
    reset=change(p,rp=(-.244,-.33,1.045),rd=(-.76,-.30,.58),yaw=-8,twist=-2,
                 shift=(.013,-.036),crouch=.119,pitch=12,head_pitch=4,cape=(12,-2))
    add('warhammer','GroundSlam','대지 강타',1.35,[.72],
        [(0,p),(.37,raised),(.53,swinging),(.64,drop),(.72,ground),(.83,absorb),
         (.99,heave),(1.19,reset),(1.35,p)])

    p=base('spear')
    chamber=change(p,rp=(-.24,.078,.96),rd=(.26,-.96,.04),yaw=-46,twist=-14,
                   shift=(-.037,.069),crouch=.103,pitch=5,cape=(1,9))
    reach=change(p,rp=(-.115,-.30,1.01),rd=(.05,-1,.02),yaw=-36,twist=-6,
                 shift=(.055,-.115),crouch=.145,pitch=24,cape=(18,-10),
                 feet=((.163,-.255,0,0),p['feet'][1]))
    retract=change(p,rp=(-.17,-.10,.99),rd=(.13,-.99,.055),yaw=-40,twist=-7,
                   shift=(.031,-.072),crouch=.121,pitch=15,cape=(12,-6),feet=reach['feet'])
    add('spear','LongThrust','잿빛 관통',.86,[.40],
        [(0,p),(.23,chamber),(.40,reach),(.53,retract),(.86,p)])

    p=base('dagger')
    low=change(p,rp=(-.36,-.18,.96),rd=(-.17,-.90,.35),yaw=-16,twist=-9,
               shift=(-.045,.031),crouch=.132,pitch=13,cape=(4,7))
    first=change(p,rp=(-.24,-.422,.981),rd=(.06,-1,.02),yaw=18,twist=11,
                 shift=(.029,-.084),crouch=.143,pitch=21,cape=(13,-7),lamp_swing=-13)
    second=change(first,rp=(-.29,-.36,.985),rd=(-.91,-.40,.025),yaw=-17,twist=-9,
                  shift=(-.019,-.071),cape=(15,9),lamp_swing=11)
    final=change(first,rp=(-.218,-.452,.982),rd=(.025,-1,.015),yaw=23,twist=13,
                 shift=(.036,-.105),crouch=.157,cape=(20,-11),lamp_swing=-19)
    add('dagger','Flurry','세 번의 상흔',.67,[.14,.27,.40],
        [(0,p),(.073,low),(.14,first),(.20,change(first,rp=(-.19,-.29,1.0),rd=(.73,-.68,.07))),
         (.27,second),(.335,low),(.40,final),(.49,change(final,rp=(-.26,-.31,.96))),(.67,p)])

    p=base('longsword')
    coil=change(p,rp=(-.411,-.17,1.08),rd=(-.73,-.61,.22),yaw=-28,twist=-11,
                shift=(-.019,.008),crouch=.105,pitch=6,cape=(1,14))
    edge=change(p,rp=(-.37,-.244,1.032),rd=(-.82,-.57,.04),yaw=15,twist=7,
                shift=(.014,-.03),crouch=.104,pitch=12,lp=(.386,-.15,.92),
                cape=(23,-16),lamp_swing=21)
    decel=change(edge,rp=(-.36,-.23,.91),rd=(-.70,-.70,.18),crouch=.128,
                 yaw=23,twist=9,cape=(25,-13),lamp_swing=-13)
    add('longsword','Whirlwind','잿바람 회전베기',1.12,[.35,.75],
        [(0,p),(.15,coil),(.28,edge),(.95,decel),(1.12,p)])


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


def direct_skill(spec, p, t):
    if spec['name']=='HERO_Spear_SkillLongThrust':
        x,y,z,rock=p['feet'][0]
        step=ease((t-.23)/.17)
        recover=ease((t-.53)/.33)
        p['feet']=((x,y,.105*sin(pi*step)+.071*sin(pi*recover),rock),p['feet'][1])
    elif spec['name']=='HERO_Sword_SkillWhirlwind' and .15<=t<=.95:
        u=(t-.15)/.8
        if u<.25:
            x=u/.25
            arc=.25*x*x*(2-x)
        elif u>.75:
            x=(1-u)/.25
            arc=1-.25*x*x*(2-x)
        else:
            arc=u
        p['spin']=4*pi*arc
        lift=.075*sin(pi*u)**2
        p['feet']=((.157,-.027,lift*max(0,sin(4*pi*arc)),3),
                   (-.157,.030,lift*max(0,-sin(4*pi*arc)),3))
    return p
