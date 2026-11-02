"""Read-only checks of the current shared locomotion delivery; no engine or Blender execution."""

import argparse
import hashlib
import json
from pathlib import Path

import numpy as np

from inspect_modeling_assets import accessor, inspect, load_glb


def verify(root):
    modeling = root / "Modeling"
    package = modeling / "LocomotionShared_20261029"
    catalog = json.loads((package / "locomotion_catalog.json").read_text(encoding="utf-8-sig"))
    validation = json.loads((package / "validation.json").read_text(encoding="utf-8-sig"))
    delivery = json.loads((package / "delivery_files.json").read_text(encoding="utf-8-sig"))
    assert catalog["schema_version"] == 1 and catalog["in_place"] is True
    assert catalog["apply_baked_root_yaw"] is False
    assert catalog["gltf_front"] == "+Z" and catalog["gltf_up"] == "Y"
    assert len(catalog["rigs"]) == 13 and len(catalog["files"]) == 18
    assert all(value is True for value in validation["checks"].values())
    retired = json.loads((modeling / "DirectionalMotion_20261029/motion_catalog.json")
                         .read_text(encoding="utf-8-sig"))
    assert retired["active"] is False
    rigs = {item["prefix"]: item for item in catalog["rigs"]}
    assert len(rigs) == 13
    hero = json.loads((modeling / "Hero_v1/hero_catalog.json").read_text(encoding="utf-8-sig"))
    npc = json.loads((modeling / "LowPolyKit_v3/animation_catalog.json").read_text(encoding="utf-8-sig"))
    monsters = json.loads((modeling / "Monsters_v1/monster_catalog.json").read_text(encoding="utf-8-sig"))
    metadata = {item["name"]: item for item in hero["clips"] + npc["clips"] + monsters["clips"]}
    expected = {item["path"]: item for item in delivery["files"]}
    # Verify only current deliverables. Backups and retired directional packs are not applied.
    verified_files = []
    for name, item in expected.items():
        path = (modeling / name).resolve()
        assert path.is_relative_to(modeling.resolve())
        data = path.read_bytes()
        assert len(data) == item["bytes"], name
        assert hashlib.sha256(data).hexdigest() == item["sha256"], name
        verified_files.append(name)
    reports = []
    unique_clips = set()
    for file in catalog["files"]:
        path = modeling / file["file"]
        rig = rigs[file["prefix"]]
        document, report = inspect(path, metadata, {"KHR_lights_punctual"})
        _, binary, _ = load_glb(path)
        clips = {item["name"]: item for item in document["animations"]}
        assert len(clips) == file["clips"] == rig["animation_count"]
        assert not any("Screen" in name or "_Turn" in name for name in clips), str(path)
        assert sum(name == rig["walk_clip"] for name in clips) == 1
        assert sum(name == rig["run_clip"] for name in clips) == 1
        assert rig["idle_clip"] in clips
        roots = [index for index, node in enumerate(document["nodes"])
                 if node.get("name") == "root"]
        assert len(roots) == 1
        for name in [rig["walk_clip"], rig["run_clip"]]:
            assert metadata[name]["loop"] is True
            assert metadata[name].get("root_yaw_degrees", 0) == 0
            # Retained root channels must really be neutral, not just labelled as such.
            for channel in clips[name]["channels"]:
                if channel["target"]["node"] != roots[0]:
                    continue
                values = accessor(document, binary, clips[name]["samplers"][channel["sampler"]]["output"])
                target = channel["target"]["path"]
                if target == "rotation":
                    assert np.allclose(values[:, :3], 0, atol=1e-5)
                    assert np.allclose(np.abs(values[:, 3]), 1, atol=1e-5)
                elif target == "translation":
                    assert np.allclose(values, 0, atol=1e-5)
                elif target == "scale":
                    assert np.allclose(values, 1, atol=1e-5)
        assert report["sha256"] == expected[file["file"]]["sha256"]
        unique_clips.update(clips)
        report.update({"prefix": file["prefix"], "idle": rig["idle_clip"],
                       "walk": rig["walk_clip"], "run": rig["run_clip"],
                       "neutralRootVerified": True, "directionalCopies": 0})
        reports.append(report)
    assert set(metadata) == unique_clips
    return {"method": "Read-only GLB/catalog and delivery hash inspection; no build, game or Blender run",
            "policy": delivery["policy"], "rigCount": len(rigs), "glbCount": len(reports),
            "uniqueClips": len(unique_clips), "deliveryFilesVerified": verified_files,
            "files": reports}


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    report = verify(args.root)
    if args.output:
        args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"passed": True, "rigs": report["rigCount"], "glbs": report["glbCount"],
                      "uniqueClips": report["uniqueClips"],
                      "verifiedDeliveryFiles": len(report["deliveryFilesVerified"])}))
