#include "stdafx.h"
#include "ModelLibrary.h"
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
                    "Village catalog exceeds 16 MiB");
            std::ifstream stream(path, std::ios::binary);
            return Json::parse(stream);
        }

        float FootprintScale(const Asset& asset, const Pose& pose, int node, int kind)
        {
            const auto& world = pose.world.at(node);
            float extent = 0;
            for (const auto& primitive : asset.meshes.at(asset.nodes.at(node).mesh).primitives)
            {
                for (const auto& vertex : primitive.vertices)
                {
                    const auto& p = vertex.position;
                    const float x = world[0] * p[0] + world[4] * p[1] + world[8] * p[2] + world[12];
                    const float z =
                        world[2] * p[0] + world[6] * p[1] + world[10] * p[2] + world[14];
                    extent = (std::max)(extent, (std::max)(std::abs(x), std::abs(z)));
                }
            }
            Require(std::isfinite(extent) && extent > .001f,
                    "Village mesh has no ground footprint");
            // Four-unit housing blocks retain at least 0.7 units of visual clearance.
            const float target = kind == 2 ? 1.2f : kind == 5 ? 2.15f : 1.65f;
            return (std::min)(1.0f, target / extent);
        }

        void CheckLandmark(const Asset& asset,
                           const Pose& pose,
                           int node,
                           float scale,
                           const Json& record,
                           const std::string& village,
                           const std::string& revision)
        {
            const auto& world = pose.world.at(node);
            float low = 0;
            float high = 0;
            bool first = true;
            size_t triangles = 0;
            for (const auto& primitive : asset.meshes.at(asset.nodes.at(node).mesh).primitives)
            {
                triangles += primitive.indices.size() / 3;
                for (const auto& vertex : primitive.vertices)
                {
                    const auto& p = vertex.position;
                    const float y = world[1] * p[0] + world[5] * p[1] + world[9] * p[2] + world[13];
                    low = first ? y : (std::min)(low, y);
                    high = first ? y : (std::max)(high, y);
                    first = false;
                }
            }
            const float height = (high - low) * scale;
            Require(std::isfinite(height) && height > 0, "Landmark has invalid height");
            if (record.contains("suggested_runtime_scale"))
            {
                const float expected = record.at("suggested_runtime_scale");
                Require(std::isfinite(expected) && std::abs(expected - scale) < .001f,
                        "Landmark GLB and catalog scale do not match");
            }
            if (record.contains("runtime_scaled_height"))
            {
                const float expected = record.at("runtime_scaled_height");
                Require(std::isfinite(expected) && std::abs(expected - height) < .01f,
                        "Landmark GLB and catalog height do not match");
            }
            if (record.contains("triangles"))
            {
                Require(record.at("triangles").get<size_t>() == triangles,
                        "Landmark GLB and catalog triangle count do not match");
            }
            std::cout << "[VillageLandmark] village=" << village << " revision=" << revision
                      << " model=" << asset.nodes.at(node).name << " scale=" << scale
                      << " height=" << height << " triangles=" << triangles << '\n';
        }
    } // namespace

    void ModelLibrary::LoadVillageBuildings(const std::filesystem::path& path)
    {
        const auto catalog = ReadCatalog(path / "village_catalog.json");
        const auto validation = ReadCatalog(path / "validation.json");
        const std::string revision = catalog.value("revision", std::string("original"));
        Require(catalog.at("schema_version") == 1 && catalog.at("units") == "meter",
                "Unsupported village catalog schema or units");
        for (const auto& check : validation.at("checks").items())
        {
            Require(check.value().is_boolean() && check.value().get<bool>(),
                    "Village export check failed: " + check.key());
        }
        auto asset = ReadCached(path / "village_buildings.glb");
        Require(asset->skins.empty() && asset->animations.empty(),
                "Village buildings must be static");
        Pose bind;
        SamplePose(*asset, -1, 0, bind);
        std::map<std::string, int> nodes;
        std::map<std::string, std::string> owners;
        std::map<std::string, std::string> types;
        std::map<std::string, const Json*> records;
        for (const auto& entry : catalog.at("assets"))
        {
            const std::string key = entry.at("modelKey");
            const auto found = asset->nodeByName.find(key);
            Require(found != asset->nodeByName.end() && asset->nodes[found->second].mesh >= 0,
                    "Village mesh is missing: " + key);
            Require(nodes.emplace(key, found->second).second,
                    "Duplicate village model key: " + key);
            owners.emplace(key, entry.at("theme").get<std::string>());
            types.emplace(key, entry.at("type").get<std::string>());
            records.emplace(key, &entry);
        }
        std::map<std::pair<std::string, int>, std::vector<BuildingVisual>> props;
        std::map<std::string, BuildingVisual> gates;
        std::set<std::string> used;
        for (const auto& theme : catalog.at("themes"))
        {
            const std::string id = theme.at("village_id");
            Require(!id.empty() && gates.count(id) == 0, "Duplicate or empty village theme id");
            const auto& choices = theme.at("game_kind_choices");
            for (int kind : {2, 5, 7, 8})
            {
                const auto& candidates = choices.at(std::to_string(kind));
                Require(candidates.is_array() && !candidates.empty(),
                        "Village theme has no building choices");
                Require(candidates.size() == (kind == 7 ? 2 : 1),
                        "Unexpected village building variant count");
                auto& output = props[{id, kind}];
                for (const auto& candidate : candidates)
                {
                    const std::string key = candidate;
                    const auto node = nodes.find(key);
                    Require(node != nodes.end() && owners.at(key) == id,
                            "Building belongs to another village: " + key);
                    const std::string expected = kind == 2   ? "WALL"
                                                 : kind == 5 ? "CHAPEL"
                                                 : kind == 8 ? "WORKSHOP"
                                                             : "HOUSE_";
                    Require(types.at(key).find(expected) == 0, "Incorrect building type: " + key);
                    const float scale = FootprintScale(*asset, bind, node->second, kind);
                    if (kind == 5)
                    {
                        Require(theme.value("landmark_model", key) == key,
                                "Village landmark key differs from kind 5 mapping");
                        CheckLandmark(*asset,
                                      bind,
                                      node->second,
                                      scale,
                                      *records.at(key),
                                      id,
                                      revision);
                    }
                    output.push_back({{asset, {node->second}}, scale});
                    used.insert(key);
                    std::cout << "[VillageModels] theme=" << id << " kind=" << kind
                              << " model=" << key << " scale=" << scale << '\n';
                }
            }
            const std::string gate = theme.at("additional_gate");
            const auto node = nodes.find(gate);
            Require(node != nodes.end() && owners.at(gate) == id && types.at(gate) == "GATE",
                    "Invalid village gate: " + gate);
            gates.emplace(id, BuildingVisual{{asset, {node->second}}, 1});
            used.insert(gate);
        }
        Require(gates.size() == 6 && props.size() == 24 && nodes.size() == 36 && used.size() == 36,
                "Village pack must contain six complete building themes");
        // An invalid optional pack must not replace a previously valid set of theme bindings.
        m_VillageProps = std::move(props);
        m_VillageGates = std::move(gates);
        std::cout << "[VillageModels] ready themes=" << m_VillageGates.size()
                  << " meshes=" << nodes.size() << " sharedFiles=1 revision=" << revision << '\n';
    }

    BuildingVisual
    ModelLibrary::VillageProp(const std::string& village, int kind, unsigned variant) const
    {
        const auto found = m_VillageProps.find({village, kind});
        if (found == m_VillageProps.end() || found->second.empty())
        {
            return {};
        }
        return found->second[variant % found->second.size()];
    }

    BuildingVisual ModelLibrary::VillageGate(const std::string& village) const
    {
        const auto found = m_VillageGates.find(village);
        return found == m_VillageGates.end() ? BuildingVisual{} : found->second;
    }
} // namespace Models
