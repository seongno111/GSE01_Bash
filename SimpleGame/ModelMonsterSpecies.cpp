#include "stdafx.h"
#include "ModelLibrary.h"
#include "MonsterDefinitions.h"
#include "Dependencies/nlohmann/json.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>

namespace Models
{
    namespace
    {
        using Json = nlohmann::json;

        void Require(bool condition, const std::string& message)
        {
            if (!condition)
            {
                throw std::runtime_error(message);
            }
        }

        Json ReadCatalog(const std::filesystem::path& path)
        {
            Require(std::filesystem::file_size(path) <= 16 * 1024 * 1024,
                    "Monster catalog exceeds 16 MiB");
            std::ifstream stream(path, std::ios::binary);
            return Json::parse(stream);
        }

        float Duration(const Asset& asset, const std::string& clip)
        {
            const auto found = asset.animationByName.find(clip);
            Require(found != asset.animationByName.end(), "Missing monster clip: " + clip);
            const float duration = asset.animations.at(found->second).duration;
            Require(std::isfinite(duration) && duration > 0,
                    "Invalid monster clip length: " + clip);
            return duration;
        }
    } // namespace

    void ModelLibrary::LoadMonsterSpecies(const std::filesystem::path& path)
    {
        const auto catalog = ReadCatalog(path / "monster_catalog.json");
        const auto validation = ReadCatalog(path / "validation.json");
        Require(catalog.at("schema_version") == 1 && catalog.at("units") == "meter"
                    && catalog.at("up_gltf") == "Y" && catalog.at("front_gltf") == "+Z",
                "Unsupported monster catalog coordinates or schema");
        for (const auto& check : validation.at("checks").items())
        {
            Require(check.value().is_boolean() && check.value().get<bool>(),
                    "Monster export validation failed: " + check.key());
        }

        std::map<std::string, MonsterVisual> visuals;
        size_t totalClips = 0;
        for (const auto& entry : catalog.at("monsters"))
        {
            const int kind = entry.at("monster_kind");
            Require(kind >= static_cast<int>(MonsterKind::AshWolf)
                        && kind < static_cast<int>(MonsterKind::Count),
                    "Unknown monster species kind");
            const auto& stats = MonsterStats(static_cast<MonsterKind>(kind));
            const std::string key = entry.at("modelKey");
            Require(key == stats.modelKey && entry.at("speciesId") == stats.id,
                    "Monster identity differs from gameplay definitions: " + key);
            const std::string file = entry.at("file");
            Require(file == key + ".glb" && key.find_first_of("/\\:") == std::string::npos,
                    "Invalid monster asset filename");
            auto asset = ReadCached(path / file);
            Require(!asset->skins.empty() && !asset->animations.empty(),
                    "Monster species requires a skeleton and animations: " + key);

            std::set<std::string> matched;
            for (const auto& metadata : entry.at("clips"))
            {
                const std::string name = metadata.at("name");
                Require(matched.insert(name).second, "Duplicate monster clip metadata");
                const float duration = metadata.at("duration_seconds");
                Require(std::isfinite(duration)
                            && std::abs(Duration(*asset, name) - duration) < .04f,
                        "Monster clip duration differs from catalog: " + name);
                auto& clip = asset->animations.at(asset->animationByName.at(name));
                clip.loop = metadata.at("loop");
                clip.rootYawDegrees = metadata.value("root_yaw_degrees", 0.0f);
                Require(std::isfinite(clip.rootYawDegrees), "Invalid monster root yaw");
                clip.events.clear();
                for (const auto& marker : metadata.at("events"))
                {
                    const float time = marker.at("time_seconds");
                    Require(std::isfinite(time) && time >= 0 && time <= clip.duration + .001f,
                            "Monster animation event outside clip: " + name);
                    clip.events.push_back(
                        {marker.at("name").get<std::string>(), (std::min)(time, clip.duration)});
                }
                std::stable_sort(clip.events.begin(),
                                 clip.events.end(),
                                 [](const auto& a, const auto& b)
                                 {
                                     return a.time < b.time;
                                 });
            }
            Require(matched.size() == asset->animations.size(), "Monster clip metadata incomplete");

            MonsterVisual visual;
            visual.model.asset = asset;
            std::set<int> selected;
            size_t triangles = 0;
            for (const std::string mesh : entry.at("meshes"))
            {
                const auto found = asset->nodeByName.find(mesh);
                Require(found != asset->nodeByName.end(), "Missing monster mesh: " + mesh);
                const auto& node = asset->nodes.at(found->second);
                Require(node.mesh >= 0 && node.skin >= 0 && selected.insert(found->second).second,
                        "Monster mesh must be unique and skinned: " + mesh);
                visual.model.nodes.push_back(found->second);
                for (const auto& primitive : asset->meshes.at(node.mesh).primitives)
                {
                    triangles += primitive.indices.size() / 3;
                }
            }
            const auto meshNodes = std::count_if(asset->nodes.begin(),
                                                 asset->nodes.end(),
                                                 [](const auto& node)
                                                 {
                                                     return node.mesh >= 0;
                                                 });
            Require(!selected.empty() && selected.size() == static_cast<size_t>(meshNodes),
                    "Monster catalog omits mesh attachments: " + key);
            Require(triangles == entry.at("triangle_count").get<size_t>(),
                    "Monster triangle count differs from catalog: " + key);
            visual.height = entry.at("height_m");
            Require(std::isfinite(visual.height) && visual.height > 0, "Invalid monster height");
            const auto& attack = entry.at("basic_attack");
            const float windup = attack.at("windup_seconds");
            const float recovery = attack.at("recovery_seconds");
            const float activation = entry.at("special").at("activation_seconds");
            Require(std::isfinite(windup) && std::isfinite(recovery) && std::isfinite(activation)
                        && std::abs(windup - stats.attackWindup) < .001f
                        && std::abs(recovery - stats.attackRecovery) < .001f
                        && std::abs(activation - stats.specialWindup) < .001f,
                    "Monster animation timing differs from gameplay: " + key);
            const auto clip = [&](const std::string& name)
            {
                Duration(*asset, name);
                return name;
            };
            visual.idle = clip(key + "_Idle");
            visual.attack = clip(attack.at("clip"));
            Require(std::abs(Duration(*asset, visual.attack) - windup - recovery) < .04f,
                    "Monster basic attack length differs from gameplay");
            visual.prepare = clip(entry.at("special").at("prepare"));
            visual.active = clip(entry.at("special").at("active"));
            visual.recover = clip(entry.at("special").at("recover"));
            // Full clips keep impact and recovery continuous; travel/ward use separate phases.
            const char* full = stats.ability == MonsterAbility::Slam   ? "_Slam"
                               : stats.ability == MonsterAbility::Rift ? "_Rift"
                               : stats.ability == MonsterAbility::Mend ? "_Mend"
                                                                       : "";
            if (*full)
            {
                visual.special = clip(key + full);
                const float expectedRecovery =
                    stats.ability == MonsterAbility::Mend ? .5f : stats.attackRecovery;
                Require(std::abs(Duration(*asset, visual.special) - activation - expectedRecovery)
                            < .04f,
                        "Monster special length differs from gameplay");
            }
            for (const std::string state :
                 {"Patrol", "Chase", "Flank", "Guarding", "Fleeing", "Returning"})
            {
                const auto& value = entry.at("state_map").at(state);
                if (!value.is_null())
                {
                    visual.states.emplace(state, clip(value));
                }
            }
            Require(visuals.emplace(key, std::move(visual)).second,
                    "Duplicate monster species: " + key);
            totalClips += matched.size();
            std::cout << "[MonsterModels] species=" << stats.id << " model=" << key
                      << " meshes=" << selected.size() << " clips=" << matched.size()
                      << " triangles=" << triangles << " height=" << entry.at("height_m")
                      << " scale=1\n";
        }
        Require(visuals.size() == static_cast<size_t>(MonsterKind::Count) - 3,
                "Monster species pack must contain all six identities");
        m_MonsterSpecies = std::move(visuals);
        std::cout << "[MonsterModels] ready species=" << m_MonsterSpecies.size()
                  << " clips=" << totalClips << " legacyIndicesUnchanged=1\n";
    }

    const MonsterVisual* ModelLibrary::MonsterSpecies(const std::string& modelKey) const
    {
        const auto found = m_MonsterSpecies.find(modelKey);
        return found == m_MonsterSpecies.end() ? nullptr : &found->second;
    }
} // namespace Models
