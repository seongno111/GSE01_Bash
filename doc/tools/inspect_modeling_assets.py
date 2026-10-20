"""Read-only GLB/catalog inspection; does not build or launch the game or Blender."""

import argparse
import hashlib
import json
import math
import struct
from pathlib import Path

import numpy as np


def load_glb(path):
    content = path.read_bytes()
    magic, version, size = struct.unpack_from("<III", content)
    assert magic == 0x46546C67 and version == 2 and size == len(content), path
    chunks = {}
    offset = 12
    while offset < size:
        length, kind = struct.unpack_from("<II", content, offset)
        offset += 8
        assert offset + length <= size
        chunks[kind] = content[offset:offset + length]
        offset += length
    return json.loads(chunks[0x4E4F534A]), chunks[0x004E4942], content


def accessor(document, binary, index):
    item = document["accessors"][index]
    assert "sparse" not in item, "Inspection fixture requires non-sparse export"
    types = {5120: "i1", 5121: "u1", 5122: "<i2", 5123: "<u2", 5125: "<u4", 5126: "<f4"}
    widths = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}
    dtype = np.dtype(types[item["componentType"]])
    width = widths[item["type"]]
    view = document["bufferViews"][item["bufferView"]]
    offset = view.get("byteOffset", 0) + item.get("byteOffset", 0)
    stride = view.get("byteStride", dtype.itemsize * width)
    assert offset + max(0, item["count"] - 1) * stride + width * dtype.itemsize <= len(binary)
    values = np.ndarray((item["count"], width), dtype=dtype, buffer=binary,
                        offset=offset, strides=(stride, dtype.itemsize)).astype(np.float64)
    if item.get("normalized"):
        assert dtype.kind in "iu"
        values /= np.iinfo(dtype).max
        values = np.maximum(values, -1)
    assert np.isfinite(values).all()
    return values


def node_matrix(node):
    if "matrix" in node:
        return np.array(node["matrix"], dtype=float).reshape(4, 4).T
    x, y, z, w = node.get("rotation", [0, 0, 0, 1])
    matrix = np.array([
        [1-2*(y*y+z*z), 2*(x*y-z*w), 2*(x*z+y*w), 0],
        [2*(x*y+z*w), 1-2*(x*x+z*z), 2*(y*z-x*w), 0],
        [2*(x*z-y*w), 2*(y*z+x*w), 1-2*(x*x+y*y), 0],
        [0, 0, 0, 1]], dtype=float)
    matrix[:, :3] *= node.get("scale", [1, 1, 1])
    matrix[:3, 3] = node.get("translation", [0, 0, 0])
    return matrix


def inspect(path, metadata):
    document, binary, raw = load_glb(path)
    nodes = document["nodes"]
    names = [node.get("name", "") for node in nodes]
    assert len([name for name in names if name]) == len(set(name for name in names if name))
    assert not document.get("extensionsRequired")
    parents = {}
    for index, node in enumerate(nodes):
        for child in node.get("children", []):
            assert child not in parents
            parents[child] = index
    world = {}

    def evaluate(index, visiting=None):
        if index in world:
            return world[index]
        visiting = set() if visiting is None else visiting
        assert index not in visiting, "Cyclic transform graph"
        visiting.add(index)
        matrix = node_matrix(nodes[index])
        world[index] = evaluate(parents[index], visiting) @ matrix if index in parents else matrix
        visiting.remove(index)
        return world[index]

    for index in range(len(nodes)):
        evaluate(index)

    vertices = triangles = weighted = 0
    for mesh in document["meshes"]:
        for primitive in mesh["primitives"]:
            assert primitive.get("mode", 4) == 4 and not primitive.get("targets")
            attributes = primitive["attributes"]
            positions = accessor(document, binary, attributes["POSITION"])
            indices = accessor(document, binary, primitive["indices"])
            assert len(indices) % 3 == 0 and indices.min() >= 0 and indices.max() < len(positions)
            vertices += len(positions)
            triangles += len(indices) // 3
            if "JOINTS_0" in attributes:
                weights = accessor(document, binary, attributes["WEIGHTS_0"])
                assert np.all(weights >= 0) and np.max(np.abs(weights.sum(axis=1) - 1)) < 1e-5
                weighted += len(weights)
    bind_error = 0
    for index, node in enumerate(nodes):
        if "skin" not in node or "mesh" not in node:
            continue
        skin = document["skins"][node["skin"]]
        inverse = accessor(document, binary, skin["inverseBindMatrices"]).reshape(-1, 4, 4).transpose(0, 2, 1)
        assert len(inverse) == len(skin["joints"])
        for primitive in document["meshes"][node["mesh"]]["primitives"]:
            joints = accessor(document, binary, primitive["attributes"]["JOINTS_0"])
            assert joints.max() < len(inverse)
        mesh_inverse = np.linalg.inv(world[index])
        for joint, matrix in zip(skin["joints"], inverse):
            error = np.abs(mesh_inverse @ world[joint] @ matrix - np.eye(4)).max()
            bind_error = max(bind_error, float(error))
    assert bind_error < 1e-4, (path, bind_error)

    clip_names = set()
    for animation in document.get("animations", []):
        name = animation["name"]
        assert name not in clip_names and name in metadata
        clip_names.add(name)
        duration = 0
        for channel in animation["channels"]:
            sampler = animation["samplers"][channel["sampler"]]
            times = accessor(document, binary, sampler["input"]).ravel()
            values = accessor(document, binary, sampler["output"])
            assert np.all(np.diff(times) > 0) and times[0] >= 0
            assert sampler.get("interpolation", "LINEAR") in ("STEP", "LINEAR", "CUBICSPLINE")
            assert len(values) == len(times) * (3 if sampler.get("interpolation") == "CUBICSPLINE" else 1)
            if channel["target"]["path"] == "rotation":
                assert np.max(np.abs(np.linalg.norm(values, axis=1) - 1)) < 1e-4
            if metadata[name]["loop"]:
                assert np.max(np.abs(values[0] - values[-1])) < 1e-4
            duration = max(duration, float(times[-1]))
        assert abs(duration - metadata[name]["duration_seconds"]) < .04
        for marker in metadata[name]["events"]:
            assert 0 <= marker["time_seconds"] <= duration + .001
    return document, {
        "file": str(path), "sha256": hashlib.sha256(raw).hexdigest(), "bytes": len(raw),
        "meshes": len(document["meshes"]), "nodes": len(nodes), "vertices": vertices,
        "triangles": triangles, "weighted_vertices": weighted,
        "skins": len(document.get("skins", [])), "clips": len(clip_names),
        "maximum_bind_palette_identity_error": bind_error,
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2] / "Modeling")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    report = {"method": "Read-only Python GLB/catalog inspection; engine was not built or executed", "packages": []}
    for version in [1, 2, 3]:
        package = args.root / f"LowPolyKit_v{version}"
        catalog = json.loads((package / "asset_catalog.json").read_text(encoding="utf-8-sig"))
        assignments = json.loads((package / "npc_appearance_assignments.json").read_text(encoding="utf-8-sig"))
        assert catalog["units"] in ("meter", "meters")
        assert assignments["schema_version"] == catalog["schema_version"]
        metadata = {}
        if version == 3:
            animations = json.loads((package / "animation_catalog.json").read_text(encoding="utf-8-sig"))
            metadata = {clip["name"]: clip for clip in animations["clips"]}
            assert len(metadata) == len(animations["clips"])
        npc, npc_report = inspect(package / ("npc/npc_parts_rigged.glb" if version == 3 else "npc_parts.glb"), metadata)
        props, props_report = inspect(package / ("props/props_library.glb" if version == 3 else "props.glb"), metadata)
        names = {node["name"] for node in npc["nodes"] if "mesh" in node}
        prop_names = {node["name"] for node in props["nodes"] if "mesh" in node}
        parts = {(item["slot_index"], item["part_index"]): item["mesh"] for item in catalog["parts"]}
        assert all(name in names for name in parts.values())
        assert all(item["mesh"] in prop_names for item in catalog["props"])
        ids = set()
        for item in assignments["npcs"]:
            assert item["npc_id"] not in ids
            ids.add(item["npc_id"])
        for item in catalog["npc_presets"] + assignments["npcs"]:
            assert 0 <= item["role_index"] < 5 and len(item["part_indices"]) == len(catalog["slots"])
            assert all(part == -1 or (slot, part) in parts for slot, part in enumerate(item["part_indices"]))
        entry = {"version": version, "assigned_npcs": len(ids), "slots": len(catalog["slots"]),
                 "parts": len(parts), "props": len(catalog["props"]), "files": [npc_report, props_report]}
        if version == 3:
            all_clips = {clip["name"] for clip in npc["animations"]}
            for monster in catalog["monsters"]:
                document, detail = inspect(package / "monsters" / (monster["name"] + "_rigged.glb"), metadata)
                assert monster["mesh"] in {node.get("name") for node in document["nodes"] if "mesh" in node}
                all_clips.update(clip["name"] for clip in document["animations"])
                entry["files"].append(detail)
            assert set(metadata) == all_clips
            assert {item["npc_action_index"] for item in animations["npc_action_map"]} == set(range(19))
            for action in animations["npc_action_map"]:
                for field in ("prepare", "approach", "perform", "recover", "enter", "exit"):
                    name = action.get(field)
                    assert not name or name == "role_attack" or name in all_clips
                assert all(name in all_clips for name in action.get("perform_by_role", {}).values())
            entry["unique_clips"] = len(all_clips)
        report["packages"].append(entry)
    if args.output:
        args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"passed": True, "packages": [{key: item[key] for key in ("version", "assigned_npcs", "parts", "props")} for item in report["packages"]]}))


if __name__ == "__main__":
    main()
