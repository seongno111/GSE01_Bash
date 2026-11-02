"""Inspect actual terrain delivery and engine resource contracts without building or executing it."""

import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import xml.etree.ElementTree as ET

import numpy as np

from inspect_modeling_assets import accessor, inspect, load_glb, node_matrix


def verify(root):
    package = root / "Modeling/WorldChunks_v1"
    catalog = json.loads((package / "chunk_catalog.json").read_text(encoding="utf-8-sig"))
    validation = json.loads((package / "validation.json").read_text(encoding="utf-8-sig"))
    delivery = json.loads((package / "delivery_files.json").read_text(encoding="utf-8-sig"))
    assert catalog["schema_version"] == 1 and catalog["chunk_side"] == 8
    assert catalog["minimum_main_route_width"] == 3
    assert all(value is True for value in validation["checks"].values())
    assert hashlib.sha256((root / catalog["source_document"]).read_bytes()).hexdigest() == catalog["source_document_sha256"]
    assert hashlib.sha256((root / "doc/world_biomes.design.json").read_bytes()).hexdigest() == catalog["source_design_json_sha256"]
    verified = []
    for item in delivery["files"]:
        path = (root / "Modeling" / item["path"]).resolve()
        assert path.is_relative_to((root / "Modeling").resolve())
        data = path.read_bytes()
        assert len(data) == item["bytes"], item["path"]
        assert hashlib.sha256(data).hexdigest() == item["sha256"], item["path"]
        verified.append(item["path"])
    ground, ground_report = inspect(package / "world_chunk_library.glb", {})
    shared, shared_report = inspect(package / "chunk_shared_props.glb", {})
    _, binary, _ = load_glb(package / "world_chunk_library.glb")
    assert len(ground["nodes"]) == len(ground["meshes"]) == 88
    assert len(shared["nodes"]) == len(shared["meshes"]) == 11
    for document in [ground, shared]:
        assert not any(document.get(key) for key in ["skins", "animations", "textures", "images"])
        assert all(material.get("alphaMode", "OPAQUE") == "OPAQUE"
                   for material in document["materials"])
        assert all(np.allclose(node_matrix(node), np.eye(4), atol=1e-5)
                   for node in document["nodes"])
    nodes = {node["name"]: node for node in ground["nodes"]}
    shared_names = {node["name"] for node in shared["nodes"]}
    assert shared_names == set(catalog["shared_props"])
    records = {}
    for item in catalog["assets"]:
        assert item["modelKey"] == item["mesh"] and item["modelKey"] not in records
        node = nodes[item["mesh"]]
        primitives = ground["meshes"][node["mesh"]]["primitives"]
        assert len(primitives) == item["material_primitives"] == 1
        primitive = primitives[0]
        points = accessor(ground, binary, primitive["attributes"]["POSITION"])
        colors = accessor(ground, binary, primitive["attributes"]["COLOR_0"])
        assert np.all((colors >= 0) & (colors <= 1))
        assert np.allclose(points[:, [0, 2]].min(axis=0), [-.5, -.5], atol=1e-5)
        assert np.allclose(points[:, [0, 2]].max(axis=0), [7.5, 7.5], atol=1e-5)
        edges = np.any(np.isclose(points[:, [0, 2]], -.5) | np.isclose(points[:, [0, 2]], 7.5), axis=1)
        assert points[edges, 1].max() < 1e-5 and points[edges, 1].min() >= -.201
        triangles = len(accessor(ground, binary, primitive["indices"])) // 3
        assert triangles == item["triangles"]
        # Actual meshes must remain in the same footprint after centered quarter turns.
        flat = points[:, [0, 2]] - 3.5
        for turn in [flat, flat[:, ::-1] * [1, -1], -flat, flat[:, ::-1] * [-1, 1]]:
            rotated = turn + 3.5
            assert rotated.min() >= -.50001 and rotated.max() <= 7.50001
        for prop in item.get("optional_shared_props", []):
            assert prop["asset"] in shared_names
            assert len(prop["position_uvz"]) == 3 and abs(prop["position_uvz"][2]) <= 2
        records[item["modelKey"]] = {"biome": item["biome_id"], "variant": item["variant_id"],
                                     "triangles": triangles, "primitives": 1,
                                     "boundsXZ": [-.5, 7.5], "boundaryTopHeight": 0}
    references = []
    base_counts = Counter()
    place_counts = Counter()
    transitions = set()
    for recipe in catalog["base_and_place_recipes"] + catalog["transitions"]:
        if "from" in recipe:
            span = (2, 1)
            transitions.add((recipe["from"], recipe["to"]))
            assert recipe["transition_width_world"] == 16
        elif recipe["kind"] == "base_biome":
            span = (1, 1)
            base_counts[recipe["biome_id"]] += 1
        else:
            span = (2, 2)
            assert recipe["span_world"] == [16, 16]
            place_counts[recipe["place_id"]] += 1
        offsets = {tuple(part["chunk_offset"]): part["modelKey"] for part in recipe["parts"]}
        assert len(offsets) == len(recipe["parts"]) == span[0] * span[1]
        assert set(offsets) == {(x, y) for x in range(span[0]) for y in range(span[1])}
        for offset, key in offsets.items():
            assert key in records
            item = next(a for a in catalog["assets"] if a["modelKey"] == key)
            assert item["recipe_world_offset"] == [offset[0] * 8, offset[1] * 8]
            references.append(key)
        for prop in recipe.get("optional_shared_props", []):
            assert tuple(prop["owner_chunk"]) in offsets
            assert prop["asset"] in shared_names
            assert abs(prop["position_uvz"][2]) <= 2
    assert len(references) == len(set(references)) == len(records) == 88
    assert sorted(base_counts.values()) == [4] * 6
    assert sorted(place_counts.values()) == [3] * 4 and len(transitions) == 8
    sequence = ["ash_grassland", "bell_mire", "dark_canopy_forest", "broken_crown_hills",
                "dead_furnace_wastes", "glass_ash_desert"]
    for a, b in zip(sequence, sequence[1:]):
        assert (a, b) in transitions or (b, a) in transitions
    ns = {"m": "http://schemas.microsoft.com/developer/msbuild/2003"}
    for project in ["SimpleGame.vcxproj", "SimpleGame.vcxproj.filters"]:
        xml = ET.parse(root / "SimpleGame" / project)
        for module in ["ModelWorldChunks.cpp", "WorldTerrain.cpp", "WorldTerrainActors.cpp"]:
            assert sum(item.get("Include") == module for item in xml.findall(".//m:ClCompile", ns)) == 1
            assert (root / "SimpleGame" / module).read_text(encoding="utf-8").startswith('#include "stdafx.h"')
        assert sum(item.get("Include") == "WorldTerrain.h" for item in xml.findall(".//m:ClInclude", ns)) == 1
    xml = ET.parse(root / "SimpleGame/SimpleGame.vcxproj")
    models = xml.find(".//m:PrototypeModels", ns)
    assert "**\\chunk_catalog.json" in models.get("Include")
    assert "WorldChunks_v1\\CHUNK_*__*.glb" in models.get("Exclude")
    vs = (root / "SimpleGame/Shaders/Model.vs").read_text(encoding="utf-8")
    fs = (root / "SimpleGame/Shaders/Model.fs").read_text(encoding="utf-8")
    outputs = set(re.findall(r"(?:flat\s+)?out\s+(\w+)\s+(v_\w+);", vs))
    inputs = set(re.findall(r"(?:flat\s+)?in\s+(\w+)\s+(v_\w+);", fs))
    assert outputs == inputs
    renderer = (root / "SimpleGame/RendererModels.cpp").read_text(encoding="utf-8")
    for name in ["u_GroundMaskEnabled", "u_GroundMask", "u_GroundMaskOffset"]:
        assert name in fs and name in renderer
    return {"method": "Read-only actual GLB, catalog, delivery hash and source contract inspection; no C++/GLSL compilation, game or Blender execution",
            "groundLibrary": ground_report, "sharedPropLibrary": shared_report,
            "baseBiomes": dict(base_counts), "placeVariants": dict(place_counts),
            "transitionPairs": sorted(transitions), "groundModels": records,
            "deliveryFilesVerified": verified, "shaderVaryingContract": True,
            "nativeAndCenteredQuarterTurnFootprints": "verified on actual vertices",
            "runtimeNavigationAndPerformance": "not executed; require user build/play validation"}


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    report = verify(args.root)
    if args.output:
        args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"passed": True, "grounds": len(report["groundModels"]), "biomes": len(report["baseBiomes"]),
                      "placeTypes": len(report["placeVariants"]), "sharedProps": report["sharedPropLibrary"]["meshes"],
                      "verifiedDeliveryFiles": len(report["deliveryFilesVerified"])}))
