"""Pose and timing direction for the hero's weapon actions."""
from math import cos, sin, pi


def ease(value):
    value = max(0, min(1, value))
    return value * value * (3 - 2 * value)


def pace(u, phase='gather'):
    u=max(0,min(1,u))
    if phase=='gather':
        return u**3*(10-15*u+6*u*u)
    if phase=='strike':
        a,b=.22,1.34
    elif phase=='carry':
        a,b=1.44,.22
    elif phase=='accelerate':
        a,b=.12,1.70
    elif phase=='brake':
        a,b=1.72,.12
    else:
        a,b=1.13,0
    return ease(u)+a*u*(1-u)**2-b*u*u*(1-u)


def segment_progress(spec, keys, index, u):
    if spec is None or spec['loop']:
        return pace(u)
    start,end=keys[index][0],keys[index+1][0]
    # Apply impact pacing to the strike, cast release, or skill contact.
    contacts=[e['time_seconds'] for e in spec['events']
              if e['name'] in ('melee_contact','cast_release','skill_contact')]
    if any(abs(end-e)<.009 for e in contacts):
        return pace(u,'accelerate')
    if any(abs(start-e)<.009 for e in contacts):
        return pace(u,'brake')
    return pace(u,'settle' if index==len(keys)-2 else 'gather')


def retarget_pose(p, kind):
    p=dict(p)
    for side in ('rp','lp'):
        x,y,z=p[side]
        if side=='rp' and kind=='bow':
            y+=.18*p['lp'][1]
        else:
            y*=1.18
        p[side]=(.88*x,y,z+.212+.095*(z-.90))
    x,y=p.get('shift',(0,0))
    p['shift']=(1.15*x,1.16*y)
    p['crouch']*=1.27
    if kind=='warhammer':
        p['crouch']=min(.54,p['crouch']+.048*p.get('ground',0))
    p['feet']=tuple((.90*x,1.23*y,1.12*lift,1.10*rock) for x,y,lift,rock in p['feet'])
    flare,drift=p.get('cape',(0,0))
    p['cape']=(flare*1.16,drift*1.18)
    p['lamp_swing']=p.get('lamp_swing',0)*1.18
    return p


def window(t, start, peak, end):
    if t < peak:
        return ease((t - start) / (peak - start))
    return 1 - ease((t - peak) / (end - peak))


STANCE = {
    'staff': (.043, .04), 'mace': (.05, .045), 'axe': (.055, .06),
    'warhammer': (.065, .07), 'spear': (.09, .07), 'dagger': (.055, .055),
    'bow': (.065, .06),
}


def footwork(p, t, step_start, plant, recover, finish, distance, lift, rear_rock):
    """Move the lead foot in the air and keep both anchors fixed during the strike."""
    lead, rear = p['feet']
    advance = ease((t - step_start) / (plant - step_start))
    retreat = ease((t - recover) / (finish - recover))
    air = sin(pi * advance) + .72 * sin(pi * retreat)
    x, y, z, rock = lead
    rx, ry, rz, rr = rear
    p['feet'] = (
        (x, y - distance * (advance - retreat), z + lift * air, rock - 8 * air),
        (rx, ry, rz, rr + rear_rock * window(t, step_start, plant, recover)),
    )
    return p


# Lead step, forward hip shift, torso rotation, and compression at the plant.
ATTACK_DRIVE = {
    'CastBolt': (.065, .098, 24, .039),
    'CastSweep': (.095, .089, 38, .046),
    'Strike': (.129, .126, 34, .081),
    'Sweep': (.098, .100, 46, .064),
    'Chop': (.145, .138, 36, .087),
    'Cleave': (.151, .121, 48, .080),
    'Smash': (.133, .124, 17, .098),
    'Uppercut': (.110, .098, 17, .035),
    'Thrust': (.217, .150, 14, .058),
    'Stab': (.142, .131, 32, .055),
    'Slash': (.087, .092, 39, .043),
}


def weapon_step(kind, name, keys, contact):
    profile = ATTACK_DRIVE[name]
    if kind == 'warhammer':
        if name == 'Smash':
            return profile, (.54,.92,1.22,.142,.076,17)
        return profile, (.33,.77,1.04,.125,.074,14)
    if kind == 'spear' and name == 'Sweep':
        profile = (.120, .104, 24, .060)
    wind = keys[1][0]
    start = wind - (contact - wind) * .56
    plant = contact - (contact - wind) * .24
    return profile, (start, plant, keys[-2][0] + .035, profile[0], .073, 21)


def direct_weapon(spec, p, t):
    """Layer grounded body mechanics over the authored grip targets."""
    kind = spec['weapon_kind']
    name = spec['name'].split('_')[-1]
    duration = spec['duration_seconds']
    authored_shift = p.get('shift',(0,0))
    authored_cape = p.get('cape',(2,0))
    phase = t / duration
    left, right = STANCE[kind]
    p = dict(p, feet=((.155, -left, 0, 0), (-.155, right, 0, 0)))
    p['shift'] = (.008, -.016)
    p['crouch'] += .012
    p['twist'] += p['yaw'] * .15
    p['pitch'] += 3
    p['look'] = p['yaw'] * .12
    p['cape'] = (2, 0)
    p['lamp_swing'] = 0
    if spec['loop']:
        pulse = (1 - cos(2 * pi * phase)) / 2
        counter = sin(2 * pi * phase)
        amount = .67 if name == 'Aim' else 1
        if name == 'Guard':
            amount = 1.3
        p['shift'] = (.008 + .029 * counter * amount, -.016 - .022 * pulse * amount)
        p['crouch'] += .017 * pulse * amount
        p['pitch'] += 4 * pulse * amount
        p['twist'] += 4 * counter * amount
        p['look'] += 6 * counter * amount
        p['cape'] = (2 + 4 * pulse, 5 * counter)
        p['lamp_swing'] = 6 * counter
        return p
    if kind == 'warhammer':
        p['shift']=(.008+authored_shift[0],-.016+authored_shift[1])
        p['cape']=authored_cape
        _,step=weapon_step(kind,name,spec['keyposes'],spec['events'][0]['time_seconds'])
        return footwork(p,t,*step[:3],duration,*step[3:])
    if kind == 'bow':
        if name == 'Draw':
            brace = window(t, 0, .52, duration)
            p['shift'] = (.008 + .042 * brace, -.016 - .043 * brace)
            p['twist'] -= 10 * brace
            p['hip_roll'] = 6 * brace
            p['crouch'] += .025 * brace
        elif name == 'Nock':
            load = sin(pi * phase) ** 2
            p['look'] += 12 * load
            p['head_pitch'] = 7 * load
            p['crouch'] += .016 * load
        elif name == 'Release':
            recoil = window(t, .10, .18, .42)
            p['shift'] = (.008 - .038 * recoil, -.016 + .045 * recoil)
            p['twist'] += 8 * recoil
            p['hip_roll'] = -6 * recoil
            p['pitch'] -= 8 * recoil
            p['cape'] = (2 + 10 * recoil, 5 * recoil)
        else:
            p['head_pitch'] = 5 * sin(pi * phase) ** 2
        return p
    contact = spec['events'][0]['time_seconds']
    keys = spec['keyposes']
    wind = keys[1][0]
    follow_time = keys[-2][0]
    (_, forward, turn, compression), step = weapon_step(kind, name, keys, contact)
    step_start, plant, recover, distance, lift, rock = step
    load = window(t, 0, step_start, plant)
    drive = window(t, step_start, plant, duration)
    follow = window(t, plant, follow_time, duration)
    shoulder = window(t, wind, contact, duration)
    p['shift'] = (.008 - .087 * load + .070 * drive,
                  -.016 + .074 * load - forward * drive)
    p['hip_roll'] = -8 * load + 8 * drive
    p['crouch'] += .050 * load + compression * drive + .022 * follow
    if name == 'Uppercut':
        p['crouch'] += .066 * load - .042 * shoulder
    if kind in ('mace', 'axe', 'staff', 'dagger'):
        attack_weight = max(load, shoulder, follow)
        p['yaw'] = p['yaw'] * (1 - attack_weight) - .85 * turn * load
        p['yaw'] += turn * shoulder + .33 * turn * follow
        p['twist'] = p['twist'] * (1 - attack_weight) - 12 * load + .42 * turn * drive
        x, y, z = p['lp']
        p['lp'] = (x + .062 * shoulder, y - .082 * load + .094 * shoulder,
                   z + .058 * load - .061 * follow)
    else:
        p['twist'] += -13 * load + .55 * turn * drive
        p['yaw'] += -9 * load + turn * shoulder
    p['pitch'] += -8 * load + (14 if name != 'Uppercut' else -11) * drive
    p['look'] += 4 * load - 4 * follow
    p['head_pitch'] = -3 * load + 6 * follow
    p['cape'] = (2 - 7 * load + 21 * follow, 15 * load - 17 * follow)
    p['lamp_swing'] = 17 * load - 27 * shoulder + 17 * follow
    return footwork(p, t, step_start, plant, recover, duration, distance, lift, rock)


def sharpen_weapon_keys(spec):
    """Compress the release out of anticipation while keeping event frames."""
    keys = spec['keyposes']
    if spec['loop'] or spec['weapon_kind'] == 'bow' or spec['weapon_kind']=='warhammer':
        return
    start, wind, contact, follow, finish = keys
    peak = contact[0]
    fraction = {'axe':.55, 'mace':.57,
                'spear':.55, 'dagger':.54, 'staff':.60}[spec['weapon_kind']]
    wind_time = peak - (peak - wind[0]) * fraction
    follow_time = peak + (follow[0] - peak) * .71
    spec['keyposes'] = [start, (wind_time, wind[1]), contact, (follow_time, follow[1]), finish]
