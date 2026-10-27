"""Read-only inspection of the delivered species GLBs and gameplay/catalog contracts."""

import argparse
import json
from pathlib import Path
import xml.etree.ElementTree as ET

import numpy as np

from inspect_modeling_assets import accessor, inspect, load_glb, node_matrix
from inspect_monster_definitions import inspect as inspect_definitions


def verify(root):
    package = root / "Modeling/Monsters_v1"
    catalog = json.loads((package / "monster_catalog.json").read_text(encoding="utf-8-sig"))
    validation = json.loads((package / "validation.json").read_text(encoding="utf-8-sig"))
    assert catalog["schema_version"] == 1 and catalog["units"] == "meter"
    assert catalog["up_gltf"] == "Y" and catalog["front_gltf"] == "+Z"
    assert all(value is True for value in validation["checks"].values())
    stats = inspect_definitions(root)["monsters"]
    assert len(catalog["monsters"]) == 6
    assert {item["monster_kind"] for item in catalog["monsters"]} == set(range(3, 9))
    assert len({item["modelKey"] for item in catalog["monsters"]}) == 6
    flattened = {item["name"]: item for item in catalog["clips"]}
    reports = []
    for entry in catalog["monsters"]:
        definition = stats[entry["monster_kind"]]
        key = entry["modelKey"]
        assert key == definition["modelKey"] and entry["speciesId"] == definition["id"]
        assert entry["file"] == key + ".glb"
        metadata = {item["name"]: item for item in entry["clips"]}
        assert len(metadata) == len(entry["clips"])
        assert all(flattened[name] == value for name, value in metadata.items())
        document, report = inspect(package / entry["file"], metadata)
        _, binary, _ = load_glb(package / entry["file"])
        assert set(metadata) == {item["name"] for item in document["animations"]}
        assert not document.get("images") and not document.get("textures")
        assert all(material.get("alphaMode", "OPAQUE") == "OPAQUE"
                   for material in document["materials"])
        nodes = document["nodes"]
        mesh_nodes = {node["name"]: node for node in nodes if "mesh" in node}
        assert set(mesh_nodes) == set(entry["meshes"])
        assert report["triangles"] == entry["triangle_count"]
        assert all("skin" in node for node in mesh_nodes.values())
        bones = {node["name"] for skin in document["skins"] for node in
                 [nodes[index] for index in skin["joints"]]}
        assert {item["name"] for item in entry["bones"]} <= bones
        if entry["rig_family"] == "quadruped":
            assert all(name in bones for name in ("front_l_paw", "front_r_paw",
                                                  "hind_l_paw", "hind_r_paw"))

        # Measure bind geometry in glTF world coordinates, not Blender Z-up catalog bounds.
        parents = {child: parent for parent, node in enumerate(nodes)
                   for child in node.get("children", [])}
        world = {}

        def evaluate(index):
            if index not in world:
                local = node_matrix(nodes[index])
                world[index] = evaluate(parents[index]) @ local if index in parents else local
            return world[index]

        points = []
        for index, node in enumerate(nodes):
            if "mesh" not in node:
                continue
            for primitive in document["meshes"][node["mesh"]]["primitives"]:
                positions = accessor(document, binary, primitive["attributes"]["POSITION"])
                points.extend((np.column_stack([positions, np.ones(len(positions))])
                               @ evaluate(index).T)[:, :3])
        points = np.array(points)
        height = float(np.ptp(points[:, 1]))
        assert abs(height - entry["height_m"]) < .01

        basic = entry["basic_attack"]
        assert abs(basic["windup_seconds"] - definition["attackWindup"]) < .001
        assert abs(basic["recovery_seconds"] - definition["attackRecovery"]) < .001
        attack = metadata[basic["clip"]]
        assert abs(attack["duration_seconds"] - definition["attackWindup"]
                   - definition["attackRecovery"]) < .04
        assert any(abs(event["time_seconds"] - definition["attackWindup"]) < .001
                   for event in attack["events"])
        special = entry["special"]
        assert abs(special["activation_seconds"] - definition["specialWindup"]) < .001
        for phase in ["prepare", "active", "recover"]:
            assert special[phase] in metadata
        assert any(abs(event["time_seconds"] - definition["specialWindup"]) < .001
                   for event in metadata[special["prepare"]]["events"])
        full_suffix = {"Slam": "Slam", "Rift": "Rift", "Mend": "Mend"}.get(definition["ability"])
        if full_suffix:
            full = metadata[f"{key}_{full_suffix}"]
            recovery = .5 if definition["ability"] == "Mend" else definition["attackRecovery"]
            assert abs(full["duration_seconds"] - definition["specialWindup"] - recovery) < .04
        if definition["ability"] in ("Pounce", "Charge", "Ward"):
            assert metadata[special["active"]]["loop"]
        for state in ["Patrol", "Chase", "Flank", "Guarding", "Fleeing", "Returning"]:
            name = entry["state_map"][state]
            assert name is None or name in metadata
        if key == "AshWolf":
            assert "AshWolf_FlankR" in metadata
        # Actions must animate; DeadHold intentionally freezes the final death pose.
        for animation in document["animations"]:
            if animation["name"].endswith("_DeadHold"):
                continue
            assert any(float(np.ptp(accessor(document, binary,
                        animation["samplers"][channel["sampler"]]["output"]), axis=0).max()) > 1e-5
                       for channel in animation["channels"]), animation["name"]
        report.update({"modelKey": key, "speciesId": entry["speciesId"],
                       "monsterKind": entry["monster_kind"], "height": height,
                       "runtimeScale": 1, "renderMeshNodes": entry["meshes"],
                       "basicAttack": basic, "special": special,
                       "specialPlayback": "full" if full_suffix else "phases"})
        reports.append(report)
    assert sum(item["clips"] for item in reports) == len(flattened) == 123
    ns = {"m": "http://schemas.microsoft.com/developer/msbuild/2003"}
    for project in ["SimpleGame.vcxproj", "SimpleGame.vcxproj.filters"]:
        xml = ET.parse(root / "SimpleGame" / project)
        for source in ["ModelMonsterSpecies.cpp", "MonsterModelVisuals.cpp"]:
            assert sum(item.get("Include") == source
                       for item in xml.findall(".//m:ClCompile", ns)) == 1
    xml = ET.parse(root / "SimpleGame/SimpleGame.vcxproj")
    assert "**\\monster_catalog.json" in xml.find(".//m:PrototypeModels", ns).get("Include")
    return {"method": "Read-only GLB/catalog/source inspection; no compiler, game or Blender execution",
            "speciesCount": len(reports), "clipCount": 123, "models": reports,
            "legacyModelIndices": "Unchanged; species are bound by separate modelKey lookup",
            "gameplayEvents": "Existing AI performs damage/healing; animation markers are visual only"}


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    report = verify(args.root)
    if args.output:
        args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"passed": True, "species": report["speciesCount"], "clips": report["clipCount"]}))
