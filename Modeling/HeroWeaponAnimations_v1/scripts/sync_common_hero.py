"""Copy the revised common actions into the original hero asset layout."""
import json
import struct
import sys
from pathlib import Path

package = Path(__file__).resolve().parents[1]
stage = Path(next(a.split('=', 1)[1] for a in sys.argv if a.startswith('out=')))
before = package / 'revisions/20261110_before_dynamics'


def read_glb(path):
    data = path.read_bytes()
    length = struct.unpack_from('<I', data, 12)[0]
    return json.loads(data[20:20+length]), data[28+length:]


base, base_data = read_glb(before/'hero_rigged.glb')
library, source_data = read_glb(stage/'hero_weapon_animation_library.glb')
output_data = bytearray(base_data)
nodes = {n['name']: i for i,n in enumerate(base['nodes'])}
clips = {a['name']: a for a in library['animations']}
common_skill='HERO_Sword_SkillWhirlwind'
base['animations'].append({'name':common_skill})
accessors = {}


def transfer(index):
    if index in accessors:
        return accessors[index]
    entry = library['accessors'][index].copy()
    view = library['bufferViews'][entry['bufferView']].copy()
    offset = view.get('byteOffset', 0)
    output_data.extend(b'\0'*((-len(output_data)) % 4))
    view['byteOffset'] = len(output_data)
    output_data.extend(source_data[offset:offset+view['byteLength']])
    entry['bufferView'] = len(base['bufferViews'])
    base['bufferViews'].append(view)
    accessors[index] = len(base['accessors'])
    base['accessors'].append(entry)
    return accessors[index]


for old in base['animations']:
    revised = clips[old['name']]
    old['samplers'] = [
        dict(s, input=transfer(s['input']), output=transfer(s['output']))
        for s in revised['samplers']
    ]
    old['channels'] = [
        dict(c, target=dict(c['target'], node=nodes[library['nodes'][c['target']['node']]['name']]))
        for c in revised['channels']
    ]

output_data.extend(b'\0'*((-len(output_data)) % 4))
base['buffers'][0]['byteLength'] = len(output_data)
js = json.dumps(base, ensure_ascii=False, separators=(',', ':')).encode('utf-8')
js += b' '*((-len(js)) % 4)
total = 28 + len(js) + len(output_data)
(stage/'hero_rigged.glb').write_bytes(
    struct.pack('<4sIII4s', b'glTF', 2, total, len(js), b'JSON') + js
    + struct.pack('<I4s', len(output_data), b'BIN\0') + output_data
)

if 'binary-only' not in sys.argv:
    import bpy
    bpy.ops.wm.open_mainfile(filepath=str(before/'hero_v1.blend'))
    rig = bpy.data.objects['RIG_HERO_HoodedLantern']
    ad = rig.animation_data
    names = [track.name for track in ad.nla_tracks]
    ad.action = None
    for track in list(ad.nla_tracks):
        ad.nla_tracks.remove(track)
    for name in names:
        bpy.data.actions.remove(bpy.data.actions[name])
    names.append(common_skill)
    with bpy.data.libraries.load(str(stage/'hero_weapon_animations_v1.blend'), link=False) as (src, dst):
        dst.actions = names.copy()
    actions = {a.name:a for a in dst.actions}
    for name in names:
        track = ad.nla_tracks.new()
        track.name = name
        track.mute = True
        track.strips.new(name, 1, actions[name]).extrapolation = 'NOTHING'
    ad.action = actions['HERO_Idle']
    bpy.context.scene.frame_set(1)
    bpy.ops.wm.save_as_mainfile(filepath=str(stage/'hero_v1.blend'))
