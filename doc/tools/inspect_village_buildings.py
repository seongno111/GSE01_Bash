"""Inspect village GLB bindings/clearance without building or running the game."""

import argparse
from collections import Counter
import hashlib
import json
import math
from pathlib import Path
import re
import xml.etree.ElementTree as ET

import numpy as np

from inspect_modeling_assets import accessor, inspect, load_glb, node_matrix


def geometry_hash(document, binary, node):
    faces = []
    for primitive in document["meshes"][node["mesh"]]["primitives"]:
        positions = accessor(document, binary, primitive["attributes"]["POSITION"])
        indices = accessor(document, binary, primitive["indices"]).ravel().astype(int).reshape(-1, 3)
        material = document["materials"][primitive["material"]].get("name", "")
        for triangle in positions[indices]:
            vertices = tuple(tuple(float(component) for component in vertex) for vertex in triangle)
            # Preserve winding while allowing a different first vertex of the same triangle.
            canonical = min(vertices, vertices[1:] + vertices[:1], vertices[2:] + vertices[:2])
            faces.append((material, canonical))
    encoded = json.dumps(sorted(faces), separators=(",", ":")).encode("utf-8")
    return hashlib.sha256(encoded).hexdigest()


def verify(root):
    package = root / "Modeling/Villages_v1"
    catalog = json.loads((package / "village_catalog.json").read_text(encoding="utf-8-sig"))
    validation = json.loads((package / "validation.json").read_text(encoding="utf-8-sig"))
    assert all(value is True for value in validation["checks"].values())
    assert catalog["schema_version"] == 1 and catalog["units"] == "meter"
    document, detail = inspect(package / "village_buildings.glb", {})
    _, binary, _ = load_glb(package / "village_buildings.glb")
    assert not document.get("images") and not document.get("skins") and not document.get("animations")
    assert all(material.get("alphaMode", "OPAQUE") == "OPAQUE" for material in document["materials"])
    nodes = {node["name"]: node for node in document["nodes"]}
    assets = {item["modelKey"]: item for item in catalog["assets"]}
    assert len(nodes) == len(assets) == len(catalog["assets"]) == 36
    assert set(nodes) == set(assets)
    villages = (root / "SimpleGame/Villages.h").read_text(encoding="utf-8")
    ids = re.findall(r'\{"([a-z]+)",\s*"', villages)
    assert len(ids) == 6
    assert set(ids) == {theme["village_id"] for theme in catalog["themes"]}
    themes = []
    for theme in catalog["themes"]:
        village = theme["village_id"]
        choices = theme["game_kind_choices"]
        assert set(choices) == {"2", "5", "7", "8"}
        assert [len(choices[kind]) for kind in ["2", "5", "7", "8"]] == [1, 1, 2, 1]
        records = []
        for kind, names in choices.items():
            for name in names:
                assert assets[name]["theme"] == village and assets[name]["game_prop_kind"] == int(kind)
                node = nodes[name]
                # These libraries deliberately bake each mesh around a shared origin.
                assert not node.get("children")
                matrix = node_matrix(node)
                points = []
                for primitive in document["meshes"][node["mesh"]]["primitives"]:
                    values = accessor(document, binary, primitive["attributes"]["POSITION"])
                    homogeneous = np.column_stack([values, np.ones(len(values))])
                    points.extend((homogeneous @ matrix.T)[:, :3])
                points = np.array(points)
                extent = float(np.max(np.abs(points[:, [0, 2]])))
                target = 1.2 if kind == "2" else 2.15 if kind == "5" else 1.65
                scale = min(1.0, target / extent)
                assert 0 < scale <= 1 and extent * scale <= target + 1e-6
                height = float(np.ptp(points[:, 1]) * scale)
                record = {"kind": int(kind), "model": name, "scale": scale,
                          "groundHalfExtent": extent * scale, "scaledHeight": height}
                if kind == "5" and assets[name].get("role") == "village_landmark":
                    assert theme["landmark_model"] == name
                    assert abs(assets[name]["suggested_runtime_scale"] - scale) < .001
                    assert abs(assets[name]["runtime_scaled_height"] - height) < .01
                    assert assets[name]["runtime_scaled_door_height"] >= 1.8
                    assert height >= 5, "Landmark lost its intended tall silhouette"
                    triangles = sum(document["accessors"][item["indices"]]["count"] // 3
                                    for item in document["meshes"][node["mesh"]]["primitives"])
                    assert triangles == assets[name]["triangles"]
                    record.update({"role": "village_landmark", "triangles": triangles,
                                   "revision": assets[name]["landmark_revision"]})
                records.append(record)
        gate = theme["additional_gate"]
        assert assets[gate]["theme"] == village and assets[gate]["type"] == "GATE"
        gate_node = nodes[gate]
        assert np.allclose(node_matrix(gate_node), np.eye(4)), "Gate clearance assumes baked origin"
        overlaps = 0
        for primitive in document["meshes"][gate_node["mesh"]]["primitives"]:
            positions = accessor(document, binary, primitive["attributes"]["POSITION"])
            indices = accessor(document, binary, primitive["indices"]).ravel().astype(int).reshape(-1, 3)
            triangles = positions[indices]
            low = triangles.min(axis=1)
            high = triangles.max(axis=1)
            # A conservative triangle AABB check of the central 0.6m passage, up to a 2m hood.
            overlap = ((low[:, 0] < .3) & (high[:, 0] > -.3) &
                       (low[:, 1] < 2.0) & (high[:, 1] > .08) &
                       (low[:, 2] < .3) & (high[:, 2] > -.3))
            overlaps += int(overlap.sum())
        assert overlaps == 0, (gate, overlaps)
        variants = Counter()
        for x in [-14, -10, -6, 6, 10, 14]:
            for y in [-14, -10, -6, 6, 10, 14]:
                variant = (((x + 18) // 4) + ((y + 18) // 4)) & 1
                variants[choices["7"][variant]] += 1
        assert sorted(variants.values()) == [18, 18]
        for x, y in [(18, 0), (-18, 0), (0, 18), (0, -18)]:
            yaw = (math.pi / 2 if x > 0 else -math.pi / 2) if abs(x) > abs(y) else (0 if y > 0 else math.pi)
            forward = np.array([math.sin(yaw), math.cos(yaw)])
            assert np.dot(forward, np.array([x, y])) > 0
        themes.append({"villageId": village, "buildings": records, "gate": gate,
                       "nominalHouseVariants": dict(variants), "gatesPerVillage": 4,
                       "centralGatePassageOverlaps": overlaps})
    namespace = {"m": "http://schemas.microsoft.com/developer/msbuild/2003"}
    for project in ["SimpleGame.vcxproj", "SimpleGame.vcxproj.filters"]:
        xml = ET.parse(root / "SimpleGame" / project)
        assert sum(item.get("Include") == "ModelVillageBuildings.cpp"
                   for item in xml.findall(".//m:ClCompile", namespace)) == 1
    xml = ET.parse(root / "SimpleGame/SimpleGame.vcxproj")
    assert "**\\village_catalog.json" in xml.find(".//m:PrototypeModels", namespace).get("Include")
    revision_report = {}
    backup = package / "revisions/20261027_before_landmarks/village_buildings.glb"
    if catalog.get("revision") == "landmarks_20261027" and backup.exists():
        old_document, old_binary, _ = load_glb(backup)
        old_nodes = {node["name"]: node for node in old_document["nodes"]}
        changed = []
        unchanged = []
        for name, node in nodes.items():
            before = geometry_hash(old_document, old_binary, old_nodes[name])
            after = geometry_hash(document, binary, node)
            (unchanged if before == after else changed).append(name)
        assert set(changed) == {item["modelKey"] for item in assets.values() if item["type"] == "CHAPEL"}
        assert len(unchanged) == 30
        revision_report = {"changedLandmarks": changed, "unchangedOtherModels": len(unchanged)}
    delivery_path = package / "landmark_delivery.json"
    if delivery_path.exists():
        delivery = json.loads(delivery_path.read_text(encoding="utf-8-sig"))
        assert delivery["revision"] == catalog["revision"]
        runtime_files = {f"{village}.glb" for village in ids} | {
            "village_buildings.glb", "village_catalog.json", "validation.json"}
        verified = []
        for item in delivery["files"]:
            if item["path"] in runtime_files:
                data = (package / item["path"]).read_bytes()
                assert len(data) == item["bytes"]
                assert hashlib.sha256(data).hexdigest() == item["sha256"], item["path"]
                verified.append(item["path"])
        assert set(verified) == runtime_files
        revision_report["deliveryFilesVerified"] = verified
    return {"method": "Read-only GLB/catalog/source inspection; game and compiler were not run",
            "assetAuthored": catalog["authored"], "sharedLibrary": detail,
            "revision": catalog.get("revision", "original"), "revisionInspection": revision_report,
            "themeCount": len(themes), "modelCount": len(assets), "themes": themes,
            "housingMinimumVisualGap": 4 - 2 * 1.65,
            "note": "Gate collision is intentionally unchanged: decorative actor with an open center."}


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    report = verify(args.root)
    if args.output:
        args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"passed": True, "themes": report["themeCount"], "models": report["modelCount"],
                      "gatesPerVillage": 4, "houseVariantsPerVillage": [18, 18]}))
