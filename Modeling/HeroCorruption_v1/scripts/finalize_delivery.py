"""Describe selection sets and record checksums for the reviewed asset delivery."""
import hashlib
import json
from pathlib import Path

P = Path(__file__).resolve().parents[1]
catalog = json.loads((P / 'corruption_catalog.json').read_text('utf-8'))
validation = json.loads((P / 'validation.json').read_text('utf-8'))
catalog.update(appearance_revision='20261124_asymmetric_weapon_arm_fusion',
               default_module_index=0, height_without_hood_m=1.84,
               body_to_head_ratio=1.84 / .365)
indices = {p['mesh']: p['part_index'] for p in catalog['parts']}
for module in catalog['modules']:
    two_handed = module['weapon_kind'] in ('bow', 'warhammer', 'spear')
    module['show_base_parts'] = catalog['shared_body'] + [module['free_arm']]
    module['hide_base_parts'] = [catalog['free_arms'][module['side']]]
    if two_handed:
        module['hide_base_parts'] += catalog['optional_lamp']
    else:
        module['show_base_parts'] += catalog['optional_lamp']
    module['assembly_parts'] = module['show_base_parts'] + module['parts']
    module['assembly_part_indices'] = [indices[n] for n in module['assembly_parts']]
    module['arm_profile'] = {
        key: value for key, value in validation['files'][module['assembled_file']].items()
        if key in ('fused_forearm_radius_m', 'free_forearm_radius_m', 'forearm_flesh_radius_ratio')
    }
    assert set(module['show_base_parts']).isdisjoint(module['hide_base_parts'])
    assert len(set(module['assembly_parts'])) == len(module['assembly_parts'])
(P / 'corruption_catalog.json').write_text(json.dumps(catalog, ensure_ascii=False, indent=2), encoding='utf-8')

for filename in ('validation.json', 'playback_validation.json', 'bow_validation.json'):
    assert json.loads((P / filename).read_text('utf-8'))['all_checks_passed'], filename

def record(path, relative_to):
    return dict(path=path.relative_to(relative_to).as_posix(), bytes=path.stat().st_size,
                sha256=hashlib.sha256(path.read_bytes()).hexdigest())

files = sorted(path for path in P.rglob('*') if path.is_file()
               and path.suffix in ('.blend', '.glb', '.json', '.md', '.py', '.jpg', '.png', '.gif')
               and path.name != 'delivery_files.json'
               and not any(part in ('_frames', '__pycache__') for part in path.parts)
               and not path.name.startswith('inspect_') and path.name != 'inventory.json')
sources = [P / catalog[key] for key in ('source_common', 'source_library', 'source_reference')]
sources += [P / module['source_equipped_file'] for module in catalog['modules']]
manifest = dict(date='2026-11-24', asset_id=catalog['asset_id'],
                files=[record(path, P) for path in files],
                source_files=[record(path.resolve(), P.parent) for path in sorted(set(sources))],
                scope='Modeling assets only; equipment and corruption state selection require game integration.')
(P / 'delivery_files.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding='utf-8')
print('Finalized', len(files), 'delivery files and', len(set(sources)), 'source references')
