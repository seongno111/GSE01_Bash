"""Inspect source/catalog contracts without compiling or launching the game."""

import argparse
from collections import Counter
import hashlib
import json
import math
from pathlib import Path
import re
import xml.etree.ElementTree as ET


def inspect(root):
    source = root / "SimpleGame/MonsterDefinitions.cpp"
    text = source.read_text(encoding="utf-8")
    catalog = text[:text.index("const MonsterDefinition& MonsterStats")]
    fields = ["health", "damage", "moveSpeed", "experience", "armor", "hitRadius",
              "detectionRange", "leashRange", "attackRange", "attackWindup", "attackRecovery",
              "specialWindup", "specialCooldown", "specialRange", "specialRadius", "specialDamage",
              "respawnSeconds", "visualHeight"]
    monsters = []
    for match in re.finditer(r"\{MonsterKind::(\w+),([^{}]+)\}", catalog):
        tokens = [part.strip() for part in match[2].split(",")]
        assert len(tokens) == 24, (match[1], len(tokens))
        entry = {"enum": match[1]}
        entry.update(zip(["id", "name", "family", "modelKey"],
                         [json.loads(item) for item in tokens[:4]]))
        entry.update(zip(fields, [float(item.rstrip("f")) for item in tokens[4:22]]))
        entry["ability"] = tokens[22].removeprefix("MonsterAbility::")
        entry["boss"] = tokens[23] == "true"
        assert all(math.isfinite(entry[field]) for field in fields)
        assert entry["health"] > 0 and entry["damage"] > 0 and entry["moveSpeed"] > 0
        assert entry["armor"] >= 0 and entry["hitRadius"] > 0 and entry["experience"] > 0
        assert entry["detectionRange"] >= entry["attackRange"]
        assert entry["respawnSeconds"] > 0 and entry["attackWindup"] > 0 and entry["attackRecovery"] > 0
        if len(monsters) >= 3:
            assert entry["leashRange"] >= entry["detectionRange"]
            assert entry["specialWindup"] > 0 and entry["specialCooldown"] > 0
        monsters.append(entry)
    assert len(monsters) == 9
    assert [item["enum"] for item in monsters[:3]] == ["WitheredHollow", "TaintedRaider", "FieldWarden"]
    for field in ["id", "modelKey", "enum"]:
        assert len({item[field] for item in monsters}) == 9
    assert {item["ability"] for item in monsters[3:]} == {"Pounce", "Slam", "Charge", "Rift", "Ward", "Mend"}
    roster_source = text[text.index("rosters ="):]
    roster_names = re.findall(r"K::(\w+)", roster_source)
    assert len(roster_names) == 60
    known = {item["enum"] for item in monsters}
    assert set(roster_names) <= known
    outposts = []
    for camp in range(5):
        roster = [roster_names[camp * 12 + slot % 12] for slot in range(36)]
        if camp == 1:
            roster[0] = "HighDemon"
        outposts.append({"outpostIndex": camp, "initial": dict(Counter(roster[:12])),
                         "capacity": dict(Counter(roster))})
    initial = Counter()
    for camp in outposts:
        initial.update(camp["initial"])
    assert set(item["enum"] for item in monsters[3:]) <= set(initial)
    assert initial["HighDemon"] == 1 and sum(initial.values()) == 60
    assert sum(camp["capacity"].get("HighDemon", 0) for camp in outposts) == 1
    namespace = {"m": "http://schemas.microsoft.com/developer/msbuild/2003"}
    modules = ["MonsterDefinitions", "MonsterBehavior", "MonsterSpawning", "MonsterVisuals"]
    for project in ["SimpleGame.vcxproj", "SimpleGame.vcxproj.filters"]:
        document = ET.parse(root / "SimpleGame" / project)
        for module in modules:
            assert len([item for item in document.findall(".//m:ClCompile", namespace)
                        if item.get("Include") == module + ".cpp"]) == 1
    for module in modules:
        assert (root / "SimpleGame" / (module + ".cpp")).read_text(encoding="utf-8").startswith('#include "stdafx.h"')
    return {"schemaVersion": 1, "method": "Read-only source/catalog inspection; no C++ build or game execution",
            "sourceSha256": hashlib.sha256(source.read_bytes()).hexdigest(),
            "monsterCount": len(monsters), "newMonsterCount": 6, "monsters": monsters,
            "nominalOutpostRosters": outposts,
            "note": "Rosters are requested allocations; blocked spawns are skipped and logged at runtime."}


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    report = inspect(args.root)
    if args.output:
        args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"passed": True, "monsterCount": report["monsterCount"], "newMonsterCount": 6,
                      "nominalInitialOutpostPopulation": 60, "initialHighDemons": 1}))
