"""Copy the verified hero animation buffers onto the new modular skin exports."""
import copy
import json
import struct
from pathlib import Path

P=Path(__file__).resolve().parents[1]
catalog=json.loads((P/'corruption_catalog.json').read_text('utf-8'))


def read(path):
    raw=path.read_bytes()
    length=struct.unpack_from('<I',raw,12)[0]
    return json.loads(raw[20:20+length]),raw[28+length:]


def write(path,document,binary):
    binary.extend(b'\0'*(-len(binary)%4))
    document['buffers'][0]['byteLength']=len(binary)
    text=json.dumps(document,ensure_ascii=False,separators=(',',':')).encode()
    text+=b' '*(-len(text)%4)
    path.write_bytes(struct.pack('<4sIII4s',b'glTF',2,28+len(text)+len(binary),len(text),b'JSON')+
                     text+struct.pack('<I4s',len(binary),b'BIN\0')+binary)


def copy_clips(path,sources):
    doc,original=read(path)
    binary=bytearray(original)
    names={n['name']:i for i,n in enumerate(doc['nodes']) if 'name' in n}
    actions={}
    for source_file,wanted,mapping,morph_only in sources:
        source,data=read(source_file)
        accessors={};views={}
        def transfer(index):
            if index not in accessors:
                value=copy.deepcopy(source['accessors'][index])
                assert 'sparse' not in value
                view_id=value['bufferView']
                if view_id not in views:
                    view=copy.deepcopy(source['bufferViews'][view_id])
                    offset=view.get('byteOffset',0)
                    binary.extend(b'\0'*(-len(binary)%4))
                    view['byteOffset']=len(binary)
                    binary.extend(data[offset:offset+view['byteLength']])
                    views[view_id]=len(doc['bufferViews']);doc['bufferViews'].append(view)
                value['bufferView']=views[view_id]
                accessors[index]=len(doc['accessors']);doc['accessors'].append(value)
            return accessors[index]
        for original_action in source['animations']:
            name=original_action['name']
            if name not in wanted:continue
            action=actions.setdefault(name,dict(name=name,channels=[],samplers=[]))
            for channel in original_action['channels']:
                target=channel['target']
                if morph_only and target['path']!='weights':continue
                source_name=source['nodes'][target['node']]['name']
                destination=mapping.get(source_name,source_name)
                if destination not in names:continue
                sampler=copy.deepcopy(original_action['samplers'][channel['sampler']])
                sampler['input']=transfer(sampler['input']);sampler['output']=transfer(sampler['output'])
                new_channel=copy.deepcopy(channel)
                new_channel['target']['node']=names[destination]
                new_channel['sampler']=len(action['samplers'])
                action['samplers'].append(sampler);action['channels'].append(new_channel)
    assert actions
    doc['animations']=list(actions.values())
    doc.setdefault('extras',{}).update(asset_id=catalog['asset_id'],
        animation_source='Existing verified hero channels, copied without resampling')
    write(path,doc,binary)


library=P/catalog['source_library']
all_names={s['name'] for s in catalog['common_clips']+catalog['weapon_clips']}
common={s['name'] for s in catalog['common_clips'] if s['name']!='HERO_Sword_SkillWhirlwind'}
bow=next(m for m in catalog['modules'] if m['weapon_kind']=='bow')
bow_names={s['name'] for s in catalog['weapon_clips'] if s['weapon_kind']=='bow'}
copy_clips(P/catalog['part_library_file'],[
    (library,all_names,{},False),(P/bow['source_equipped_file'],bow_names,bow['source_node_map'],True)])
copy_clips(P/catalog['shared_body_file'],[(library,all_names,{},False)])
for module in catalog['modules']:
    kind=module['weapon_kind']
    desired=common|({'HERO_Sword_SkillWhirlwind'} if kind=='longsword' else
                    {s['name'] for s in catalog['weapon_clips'] if s['weapon_kind']==kind})
    sources=[(library,desired,{},False)]
    if kind=='bow':sources.append((P/module['source_equipped_file'],bow_names,module['source_node_map'],True))
    copy_clips(P/module['assembled_file'],sources)
    module['clip_names']=sorted(desired)
catalog['bone_animation_count']=len(all_names)
catalog['module_index_scope']='Corruption appearance modules. Resolve equipment by weapon_key.'
(P/'corruption_catalog.json').write_text(json.dumps(catalog,ensure_ascii=False,indent=2),encoding='utf-8')
print('Attached',len(all_names),'original hero animations and exact bow morph channels')
