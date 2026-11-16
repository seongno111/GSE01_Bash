"""Pose and timing direction for the 2026-11-10 animation revision."""
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
    else:
        a,b=1.13,0
    return ease(u)+a*u*(1-u)**2-b*u*u*(1-u)


def segment_progress(spec, keys, index, u):
    if spec is None or spec['loop']:
        return pace(u)
    start,end=keys[index][0],keys[index+1][0]
    events=[e['time_seconds'] for e in spec['events']]
    if spec['weapon_kind']=='warhammer':
        u=max(0,min(1,u))
        if any(abs(end-e)<.009 for e in events):
            return ease(u)+.40*u*(1-u)**2-1.10*u*u*(1-u)
        if any(abs(start-e)<.009 for e in events):
            return ease(u)+1.10*u*(1-u)**2-.30*u*u*(1-u)
        return ease(u)
    if any(abs(end-e)<.009 for e in events):
        return pace(u,'strike')
    if any(abs(start-e)<.009 for e in events):
        return pace(u,'carry')
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


def direct_weapon(spec, p, t):
    """Layer grounded body mechanics over the authored grip targets."""
    kind = spec['weapon_kind']
    name = spec['name'].split('_')[-1]
    duration = spec['duration_seconds']
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
    if kind == 'bow':
        if name == 'Draw':
            load = sin(pi * phase) ** 2
            p['shift'] = (.008 + .021 * load, -.016 - .026 * load)
            p['twist'] -= 5 * load
        elif name == 'Nock':
            load = sin(pi * phase) ** 2
            p['look'] += 12 * load
            p['head_pitch'] = 7 * load
            p['crouch'] += .016 * load
        elif name == 'Release':
            load = window(t, .10, .17, .40)
            p['shift'] = (.008 - .028 * load, -.016 + .034 * load)
            p['pitch'] -= 6 * load
            p['cape'] = (2 + 7 * load, 4 * load)
        else:
            p['head_pitch'] = 5 * sin(pi * phase) ** 2
        return p
    contact = spec['events'][0]['time_seconds']
    keys = spec['keyposes']
    wind_time = keys[1][0]
    load = window(t, 0, wind_time, contact)
    drive = window(t, wind_time, contact, duration)
    follow = window(t, contact, min(contact + .17, duration - .04), duration)
    heavy = {'warhammer':1.25, 'axe':1.08, 'mace':.9, 'spear':.83,
             'dagger':.72, 'staff':.8}[kind]
    p['shift'] = (.008 - .060 * load * heavy + .052 * drive * heavy,
                  -.016 + .058 * load * heavy - .105 * drive * heavy)
    p['crouch'] += .028 * load + .033 * follow * heavy
    if kind == 'warhammer':
        p['twist'] += -5 * load + 8 * drive
        p['yaw'] += 5 * load + 20 * drive
    else:
        p['twist'] += (14 * load - 12 * drive) * heavy
        p['yaw'] += (13 * load - 14 * drive) * heavy
    if kind in ('mace', 'axe', 'staff', 'dagger'):
        amount = max(load, drive, follow)
        shoulder_turn = -23 * load + 23 * drive + 14 * follow
        p['yaw'] = p['yaw'] * (1 - amount) + shoulder_turn
        p['twist'] = p['twist'] * (1 - amount) + .45 * shoulder_turn
    p['pitch'] += (-6 * load + 11 * drive) * heavy
    p['head_pitch'] = -4 * load + 5 * follow
    p['cape'] = (2 - 5 * load + 14 * follow, 10 * (load - drive))
    p['lamp_swing'] = (12 * load - 18 * drive + 12 * follow) * heavy
    if name in ('Stab', 'Thrust'):
        land = contact - .025
        retrieve = min(contact + .11, duration - .075)
        progress = ease((t - wind_time) / (land - wind_time))
        back = ease((t - retrieve) / (duration - retrieve))
        lift = .075 * (sin(pi * progress) + .65 * sin(pi * back))
        step = (.15 if name == 'Thrust' else .12) * (progress - back)
        p['feet'] = ((.155, -left - step, lift, -4 * sin(pi * progress)), (-.155, right, 0, 0))
    return p


def sharpen_weapon_keys(spec):
    """Compress the release out of anticipation while keeping event frames."""
    keys = spec['keyposes']
    if spec['loop'] or spec['weapon_kind'] == 'bow':
        return
    start, wind, contact, follow, finish = keys
    peak = contact[0]
    fraction = {'warhammer':.88, 'axe':.72, 'mace':.70,
                'spear':.62, 'dagger':.58, 'staff':.67}[spec['weapon_kind']]
    fast_wind = peak - (peak - wind[0]) * fraction
    spec['keyposes'] = [start, (fast_wind, wind[1]), contact, follow, finish]
