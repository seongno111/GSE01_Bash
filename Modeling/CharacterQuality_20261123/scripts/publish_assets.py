"""Publish reviewed modeling assets to their established package paths."""
import hashlib
import json
import shutil
from pathlib import Path

P=Path(__file__).resolve().parents[1]
BASE=P.parent
validation=json.loads((P/'validation.json').read_text('utf-8'))
playback=json.loads((P/'playback_validation.json').read_text('utf-8'))
assert validation['all_checks_passed'] and playback['all_checks_passed']
assert all(validation['checks'].values()) and all(playback['checks'].values())
staged=P/'staged'
deliveries=[f for f in staged.rglob('*') if f.is_file() and f.suffix in ('.glb','.blend','.json')]
assert sum(f.suffix=='.glb' for f in deliveries)==17
assert sum(f.suffix=='.blend' for f in deliveries)==2
backup=P/'backups'
for source in deliveries:
    relative=source.relative_to(staged)
    target=BASE/relative
    previous=backup/relative
    if target.exists() and not previous.exists():
        previous.parent.mkdir(parents=True,exist_ok=True)
        shutil.copyfile(target,previous)
for source in deliveries:
    target=BASE/source.relative_to(staged)
    target.parent.mkdir(parents=True,exist_ok=True)
    if not target.exists() or hashlib.sha256(source.read_bytes()).digest()!=hashlib.sha256(target.read_bytes()).digest():
        shutil.copyfile(source,target)
    assert hashlib.sha256(source.read_bytes()).digest()==hashlib.sha256(target.read_bytes()).digest()
for package in ('LowPolyKit_v3','Monsters_v1'):
    target=BASE/package/'validation.json'
    previous=backup/package/'validation.json'
    if not previous.exists():shutil.copyfile(target,previous)
    current=dict(date='2026-11-23',all_checks_passed=True,
        character_quality_revision='2026-11-23',
        validation_report='../CharacterQuality_20261123/validation.json',
        playback_report='../CharacterQuality_20261123/playback_validation.json',
        checks=validation['checks'],
        files={name:metrics for name,metrics in validation['files'].items() if name.startswith(package+'/')})
    target.write_text(json.dumps(current,ensure_ascii=False,indent=2),encoding='utf-8')
manifest=[]
paths=[BASE/f.relative_to(staged) for f in deliveries]
paths.extend(BASE/package/'validation.json' for package in ('LowPolyKit_v3','Monsters_v1'))
paths.extend(f for f in (P/'previews').glob('*') if f.is_file())
paths.extend([P/'validation.json',P/'playback_validation.json',P/'README.md'])
for file in sorted(paths):
    manifest.append(dict(file=file.relative_to(BASE).as_posix(),bytes=file.stat().st_size,
                         sha256=hashlib.sha256(file.read_bytes()).hexdigest()))
(P/'delivery_files.json').write_text(json.dumps(dict(date='2026-11-23',files=manifest),ensure_ascii=False,indent=2),encoding='utf-8')
print('Published 17 character GLBs, two Blender sources, current catalogs and validation reports.')
