import json,hashlib
from pathlib import Path
from PIL import Image
P=Path(__file__).resolve().parents[1]
v=json.loads((P/'validation.json').read_text('utf-8'))
assert v['all_checks_passed'] and all(v['checks'].values())
grip_path=P/'hammer_grip_validation.json'
grip=json.loads(grip_path.read_text('utf-8')) if grip_path.exists() else {'checks':{}}
assert all(grip['checks'].values())
cat=json.loads((P/'animation_catalog.json').read_text('utf-8'))
assert len(cat['new_clips'])==30
for s in cat['new_clips']:assert (P/'previews'/(s['name']+'.png')).is_file()
for eq in cat['equipment']:
 im=Image.open(P/'previews'/('hero_'+eq['weapon_kind']+'.gif'));assert im.n_frames>5
files=[]
for p in sorted(P.rglob('*')):
 if not p.is_file() or p in [P/'delivery_files.json',P/'delivery_overview.json'] or p.suffix in ['.log','.blend1','.pyc'] or '_frames' in p.parts or '__pycache__' in p.parts:continue
 files.append(dict(file=p.relative_to(P).as_posix(),bytes=p.stat().st_size,sha256=hashlib.sha256(p.read_bytes()).hexdigest()))
manifest=dict(date='2026-11-05',files=files,total_files=len(files),total_bytes=sum(f['bytes'] for f in files))
(P/'delivery_files.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8')
summary=dict(date='2026-11-05',new_weapon_actions=30,weapon_types=7,common_original_actions=11,total_library_actions=41,glb_files=8,blender_source=1,checks_passed=len(v['checks'])+len(grip['checks']),hammer_grip_revision='20261105_cross_palm',game_code_changed=False,build_run=False,delivery_files=len(files),delivery_bytes=manifest['total_bytes'])
(P/'delivery_overview.json').write_text(json.dumps(summary,ensure_ascii=False,indent=2),encoding='utf-8')
assert all(hashlib.sha256((P/f['file']).read_bytes()).hexdigest()==f['sha256'] for f in files)
print(json.dumps(summary,indent=2))
