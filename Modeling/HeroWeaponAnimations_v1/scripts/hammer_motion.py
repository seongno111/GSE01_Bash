"""Author the hammer as a rigid two-hand swing in one vertical plane."""
from math import cos, pi, radians, sin

from mathutils import Matrix, Vector

from dynamic_motion import ease, pace, window


# Start and end of the windup, impact hold, and the size of the attack arc.
# Uppercut is the existing name of the second normal-attack slot.
SWINGS = {
    'HERO_Hammer_Smash': (.65, 1.09, .832, -.37, .349),
    'HERO_Hammer_Uppercut': (.55, .93, .81, -.37, .365),
    'HERO_Hammer_SkillGroundSlam': (.36, .80, .96, -.42, .310),
}
HEAD = Vector((0, 0, .53))
DOWN_ANGLE = radians(-58)
SHAFT_LEAD = radians(28)
GROUND_SLAM = 'HERO_Hammer_SkillGroundSlam'


def shoulder_drive(name, time, contact):
    """Protract the leading shoulder as the full-width stroke passes the chest."""
    if name != GROUND_SLAM:
        return 0
    windup = SWINGS[name][0]
    travel = (time-windup)/(contact-windup)
    return radians(11)*window(travel,.71,.785,.85)


def body_keys(start, change, name, contact, finish):
    windup, hold, *_ = SWINGS['HERO_Hammer_' + name]
    skill = name == 'SkillGroundSlam'
    start = change(start, head_pitch=start.get('head_pitch', 0))
    load = change(start, shift=(.006, -.144), crouch=.067, pitch=12,
                  yaw=-7, twist=-3, head_pitch=-6, cape=(1, 5))
    drive = change(load, shift=(-.025, -.25), crouch=.092, pitch=28,
                   yaw=-10, twist=-5, head_pitch=4, cape=(9, -1))
    reach = change(drive, shift=(.063, -.220), crouch=.105, pitch=37,
                   yaw=-12, twist=4, head_pitch=8, cape=(13, -2))
    compress = change(reach, shift=(.110, -.273), crouch=.296, pitch=33,
                      yaw=-15, twist=4, head_pitch=12, cape=(17, -3))
    hit = change(compress, shift=(.081, -.226), crouch=.381 if skill else .35,
                 pitch=27, yaw=-11, twist=-3, head_pitch=13, cape=(20, -3))
    settle = change(hit, crouch=hit['crouch']+.008, cape=(22, -3))
    lift = change(hit, shift=(.057, -.242), crouch=.285, pitch=14,
                  yaw=-7, twist=0, head_pitch=11, cape=(17, -2))
    rise = change(drive, shift=(.017, -.139), crouch=.232, pitch=9,
                  yaw=-10, head_pitch=7, cape=(11, -1))
    if skill:
        raise_arms = change(start, shift=(-.034, -.058), crouch=.031, pitch=0,
                            yaw=-13, twist=2, head_pitch=-8, cape=(-3, 1))
        load = change(load, shift=(.015, -.104), crouch=.033, pitch=-4,
                      yaw=-14, twist=-4, head_pitch=-14, cape=(-5, 2))
        drive = change(drive, shift=(-.04, -.280), crouch=.03, pitch=21)
        reach = change(reach, shift=(.012, -.265), crouch=.008, pitch=26,
                       yaw=-9, twist=-2)
        extend = change(reach, shift=(.021, -.264), crouch=.024, pitch=29,
                        yaw=-11, twist=3)
        compress = change(compress, shift=(.038, -.243), crouch=.077, pitch=35,
                          yaw=-4, twist=11)
        fold = change(compress, shift=(.087, -.296), crouch=.184, pitch=36,
                      yaw=-7, twist=13)
        follow = change(fold, shift=(.081, -.306), crouch=.278, pitch=44,
                        yaw=-14, twist=-6)
        hit = change(hit, shift=(.125, -.17), crouch=.320, pitch=49,
                     head_pitch=19, cape=(28, -3))
        settle = change(hit, crouch=.310, pitch=49, cape=(25, -2))
        lift = change(lift, shift=(.103, -.215), crouch=.316, pitch=23)
    stroke = contact-windup
    keys = [(windup, load), (windup + .57*stroke, drive)]
    if skill:
        keys = [(windup*.50, raise_arms), *keys, (windup + .675*stroke, reach),
                (windup + .732*stroke, extend), (windup + .765*stroke, compress),
                (windup + .845*stroke, fold),
                (windup + .925*stroke, follow)]
    else:
        keys += [(windup + .78*stroke, reach), (windup + .85*stroke, compress)]
    keys += [(contact, hit), (hold, settle), (hold + .29*(finish-hold), lift),
             (hold + .51*(finish-hold), rise)]
    if skill:
        # Skills already carry the Ready offsets, while normals layer them during baking.
        keys = [(t, change(p, shift=(p['shift'][0]+.008, p['shift'][1]-.016),
                           crouch=p['crouch']+.012, pitch=p['pitch']+3,
                           twist=p['twist']+p['yaw']*.15)) for t,p in keys]
    return [(0,start), *keys, (finish,start)]


def weapon_rotation(angle, lean):
    shaft = Vector((-lean, -cos(angle), sin(angle))).normalized()
    face = Vector((0, -sin(angle), -cos(angle)))
    return Matrix((face, shaft.cross(face), shaft)).transposed().to_quaternion()


def arc_pose(angle, pivot, radius, overhead=False):
    phase = sin(pi*ease((angle+radians(48))/radians(116)))
    shaft_angle = angle + SHAFT_LEAD - radians(20)*phase
    # Clear the hood on the right while keeping the two fists within reach.
    low = ease((radians(94)-angle)/radians(62))
    high = ease((angle-radians(100))/radians(22))
    clearance = sin(pi*ease((angle+radians(39))/radians(59)))
    lean = .18+.18*low+.08*high-.055*clearance
    if overhead:
        wind = ease((angle-radians(83))/radians(39))
        shaft_angle += radians(23)*wind
        lean += .52*wind+.04*low
    rotation = weapon_rotation(shaft_angle, lean)
    center = pivot + Vector((0, -cos(angle), sin(angle)))*radius
    return center - rotation @ HEAD, rotation


def swing_target(name, time, contact, finish, ready_grip, ready_rotation, mesh):
    windup, hold, radius, x, forward = SWINGS[name]
    overhead = name == GROUND_SLAM
    pivot = Vector((x, -forward, 0))
    grip, impact_rotation = arc_pose(DOWN_ANGLE, pivot, radius, overhead)
    pivot.z = .001 - min((impact_rotation @ (v-HEAD)).z for v in mesh) - radius*sin(DOWN_ANGLE)
    if time < windup:
        grip, rotation = arc_pose(DOWN_ANGLE+pi, pivot, radius, overhead)
        amount = pace(time/windup)
        lift = ready_grip.lerp(grip, amount)
        if overhead:
            clear = sin(pi*amount)**2
            lift.x -= .18*clear
            lift.z += .145*clear
        return lift, ready_rotation.slerp(rotation, amount)
    if time <= contact:
        u = (time-windup)/(contact-windup)
        # Accelerate through the entire release and stop against the floor.
        travel = u*u*(1.34-.34*u)
        return arc_pose(DOWN_ANGLE+pi*(1-travel), pivot, radius, overhead)
    grip.z += pivot.z
    rotation = impact_rotation
    if time <= hold:
        return grip, rotation
    amount = pace((time-hold)/(finish-hold))
    lift = grip.lerp(ready_grip, amount)
    lift.z += .19*sin(pi*amount)
    clear = sin(pi*amount)*(1-amount)
    lift.y -= .24*clear + .050*sin(pi*ease((amount-.70)/.30))
    lift.x -= .055*clear
    pick_up = weapon_rotation(radians(84), .09)
    if amount < .57:
        q = rotation.slerp(pick_up, ease(amount/.57))
    else:
        q = pick_up.slerp(ready_rotation, ease((amount-.57)/.43))
    return lift,q
