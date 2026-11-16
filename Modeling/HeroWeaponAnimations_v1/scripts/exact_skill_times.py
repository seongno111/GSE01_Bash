"""Map baked skill samples to the exact game-time anchors in the specification."""
import json
import struct
from skill_motion import skill_time


def retime(path, skills):
    raw=path.read_bytes()
    length=struct.unpack_from('<I',raw,12)[0]
    doc=json.loads(raw[20:20+length])
    buffer=bytearray(raw[28+length:])
    for action in doc['animations']:
        spec=skills.get(action['name'])
        if spec is None:
            continue
        mapped={}
        for sampler in action['samplers']:
            i=sampler['input']
            if i not in mapped:
                accessor=doc['accessors'][i]
                view=doc['bufferViews'][accessor['bufferView']]
                offset=view.get('byteOffset',0)+accessor.get('byteOffset',0)
                assert accessor['type']=='SCALAR' and accessor['componentType']==5126
                times=[struct.unpack_from('<f',buffer,offset+n*view.get('byteStride',4))[0] for n in range(accessor['count'])]
                divisions=spec.get('sample_divisions',1)
                mapped_times=[skill_time(spec,1+round((t-times[0])*60)/divisions) for t in times]
                assert all(b>a for a,b in zip(mapped_times,mapped_times[1:]))
                buffer.extend(b'\0'*((-len(buffer))%4))
                data=struct.pack('<'+'f'*len(mapped_times),*mapped_times)
                doc['bufferViews'].append(dict(buffer=0,byteOffset=len(buffer),byteLength=len(data)))
                mapped[i]=len(doc['accessors'])
                doc['accessors'].append(dict(bufferView=len(doc['bufferViews'])-1,componentType=5126,type='SCALAR',
                                             count=len(mapped_times),min=[mapped_times[0]],max=[mapped_times[-1]]))
                buffer.extend(data)
            sampler['input']=mapped[i]
    doc['buffers'][0]['byteLength']=len(buffer)
    header=json.dumps(doc,ensure_ascii=False,separators=(',',':')).encode('utf-8')
    header+=b' '*((-len(header))%4)
    total=28+len(header)+len(buffer)
    tmp=path.with_suffix('.glb.tmp')
    tmp.write_bytes(struct.pack('<4sIII4s',b'glTF',2,total,len(header),b'JSON')+header+
                    struct.pack('<I4s',len(buffer),b'BIN\0')+buffer)
    tmp.replace(path)


for export in EXPORTS:
    retime(export,{spec['name']:spec for spec in SKILLS})
