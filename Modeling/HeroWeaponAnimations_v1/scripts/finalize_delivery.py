import json,hashlib
from pathlib import Path
from PIL import Image
P=Path(__file__).resolve().parents[1]
v=json.loads((P/'validation.json').read_text('utf-8'))
assert v['all_checks_passed'] and all(v['checks'].values())
cat=json.loads((P/'animation_catalog.json').read_text('utf-8'))
assert len(cat['new_clips'])==37 and len(cat['common_clips'])==12 and len(cat['skills'])==8
for s in cat['common_clips']+cat['new_clips']:assert (P/'previews'/(s['name']+'.png')).is_file()
for eq in cat['equipment']:
 im=Image.open(P/'previews'/('hero_'+eq['weapon_kind']+'.gif'));assert im.n_frames>5
files=[]
for p in sorted(P.rglob('*')):
 if not p.is_file() or p in [P/'delivery_files.json',P/'delivery_overview.json'] or p.suffix in ['.log','.blend1','.pyc'] or any(n in p.parts for n in ('_frames','_stage_dynamics','_stage_five_head','__pycache__')):continue
 files.append(dict(file=p.relative_to(P).as_posix(),bytes=p.stat().st_size,sha256=hashlib.sha256(p.read_bytes()).hexdigest()))
date=cat.get('hammer_animation_revision',{}).get('date',cat['date'])
manifest=dict(date=date,files=files,total_files=len(files),total_bytes=sum(f['bytes'] for f in files))
(P/'delivery_files.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8')
summary=dict(date=date,retargeted_actions=49,skill_actions=8,weapon_types=8,common_hero_actions=12,
             body_to_head_ratio=v['metrics']['body_to_head_ratio'],hero_triangles=v['metrics']['triangles_after'],
             total_library_actions=49,glb_files=9,blender_sources=2,checks_passed=len(v['checks']),
             game_code_changed=False,build_run=False,delivery_files=len(files),delivery_bytes=manifest['total_bytes'])
(P/'delivery_overview.json').write_text(json.dumps(summary,ensure_ascii=False,indent=2),encoding='utf-8')
assert all(hashlib.sha256((P/f['file']).read_bytes()).hexdigest()==f['sha256'] for f in files)
print(json.dumps(summary,indent=2))
