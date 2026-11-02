#include "stdafx.h"
#include "WorldTerrain.h"
#include "Dependencies/nlohmann/json.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace Models
{
    namespace
    {
        using Json = nlohmann::json;

        void Require(bool condition, std::string_view message)
        {
            if (!condition)
            {
                throw std::runtime_error(std::string(message));
            }
        }

        Json ReadCatalog(const std::filesystem::path& path)
        {
            Require(std::filesystem::file_size(path) < 16 * 1024 * 1024, "Chunk catalog too large");
            std::ifstream stream(path, std::ios::binary);
            return Json::parse(stream);
        }

        Terrain::Biome ParseBiome(const std::string& id)
        {
            for (int i = 0; i < int(Terrain::Biome::Count); ++i)
            {
                if (id == Terrain::BiomeId(static_cast<Terrain::Biome>(i)))
                {
                    return static_cast<Terrain::Biome>(i);
                }
            }
            throw std::runtime_error("Unknown chunk biome: " + id);
        }

        ModelInstance FindMeshInstance(const std::shared_ptr<const Asset>& asset,
                                       const std::string& name)
        {
            const auto found = asset->nodeByName.find(name);
            Require(found != asset->nodeByName.end() && asset->nodes.at(found->second).mesh >= 0,
                    "Missing chunk mesh: " + name);
            return {asset, {found->second}};
        }

        float Extent(const ModelInstance& model)
        {
            float extent = 0;
            for (const auto& primitive :
                 model.asset->meshes.at(model.asset->nodes.at(model.nodes.front()).mesh).primitives)
            {
                for (const auto& vertex : primitive.vertices)
                {
                    extent = (std::max)(extent,
                                        (std::max)(std::abs(vertex.position[0]),
                                                   std::abs(vertex.position[2])));
                }
            }
            return extent;
        }

        void CheckStaticLibrary(const Asset& asset)
        {
            Require(asset.skins.empty() && asset.animations.empty() && asset.textures.empty(),
                    "World terrain requires static untextured meshes");
            Pose bind;
            SamplePose(asset, -1, 0, bind);
            for (size_t node = 0; node < asset.nodes.size(); ++node)
            {
                if (asset.nodes[node].mesh < 0)
                {
                    continue;
                }
                for (int i = 0; i < 16; ++i)
                {
                    const float expected = i % 5 == 0 ? 1.0f : 0.0f;
                    Require(std::abs(bind.world[node][i] - expected) < .0001f,
                            "Terrain nodes must have baked identity transforms");
                }
            }
            for (const auto& material : asset.materials)
            {
                Require(material.alphaMode == 0, "World terrain material must be opaque");
            }
        }
    } // namespace

    void ModelLibrary::LoadWorldChunks(const std::filesystem::path& path)
    {
        const auto source = ReadCatalog(path / "chunk_catalog.json");
        const auto validation = ReadCatalog(path / "validation.json");
        Require(source.at("schema_version") == 1 && source.at("chunk_side") == 8
                    && source.at("minimum_main_route_width") == 3,
                "Unsupported world chunk layout");
        for (const auto& check : validation.at("checks").items())
        {
            Require(check.value().is_boolean() && check.value().get<bool>(),
                    "World chunk export check failed: " + check.key());
        }
        auto ground = ReadCached(path / "world_chunk_library.glb");
        auto props = ReadCached(path / "chunk_shared_props.glb");
        CheckStaticLibrary(*ground);
        CheckStaticLibrary(*props);
        auto catalog = std::make_shared<Terrain::Catalog>();
        std::map<std::string, Terrain::Prop> shared;
        for (const std::string key : source.at("shared_props"))
        {
            Terrain::Prop prop;
            prop.model = FindMeshInstance(props, key);
            prop.key = key;
            prop.extent = Extent(prop.model);
            Require(std::isfinite(prop.extent) && prop.extent > 0 && prop.extent < 8,
                    "Invalid shared terrain prop footprint");
            prop.canopy = key == "ENV_CanopyTree_Canopy";
            shared.emplace(key, std::move(prop));
        }
        const auto anchor = [&](const Json& input, WorldPoint offset, bool blocking)
        {
            const std::string name = input.at("asset");
            Require(shared.count(name) != 0, "Unknown terrain prop: " + name);
            auto prop = shared.at(name);
            const auto& position = input.at("position_uvz");
            Require(position.size() == 3, "Terrain anchor requires UV and height");
            prop.elevation = position.at(2);
            Require(std::isfinite(prop.elevation) && std::abs(prop.elevation) <= 2,
                    "Invalid terrain prop elevation");
            prop.position = {position.at(0).get<double>() - .5 - offset.x,
                             position.at(1).get<double>() - .5 - offset.y};
            Require(std::isfinite(prop.position.x) && std::isfinite(prop.position.y),
                    "Invalid terrain anchor position");
            const bool candidate = input.value("blocking_candidate", blocking);
            // Open arches/canopies are visual only. Solid props get conservative simple colliders.
            prop.radius = candidate && !prop.canopy ? (std::min)(.65f, prop.extent * .45f) : 0;
            return prop;
        };
        for (const auto& item : source.at("assets"))
        {
            Terrain::Ground output;
            output.key = item.at("modelKey");
            output.model = FindMeshInstance(ground, item.at("mesh"));
            const auto& mesh = ground->meshes.at(ground->nodes.at(output.model.nodes.front()).mesh);
            Require(mesh.primitives.size() == 1, "Chunk ground must be one primitive");
            Require(mesh.primitives.front().indices.size() / 3
                        == item.at("triangles").get<size_t>(),
                    "Chunk triangle count differs from catalog");
            for (const auto& vertex : mesh.primitives.front().vertices)
            {
                Require(std::isfinite(vertex.position[0]) && std::isfinite(vertex.position[1])
                            && std::isfinite(vertex.position[2]) && vertex.position[0] >= -.501f
                            && vertex.position[0] <= 7.501f && vertex.position[2] >= -.501f
                            && vertex.position[2] <= 7.501f,
                        "Invalid 8x8 chunk ground bounds");
            }
            const auto& offset = item.at("recipe_world_offset");
            if (item.contains("optional_shared_props"))
            {
                for (const auto& input : item.at("optional_shared_props"))
                {
                    output.props.push_back(anchor(input, {offset.at(0), offset.at(1)}, true));
                }
            }
            Require(catalog->grounds.emplace(output.key, std::move(output)).second,
                    "Duplicate chunk model key");
        }
        std::set<std::string> used;
        const auto parts = [&](const Json& recipe, size_t count)
        {
            std::vector<std::string> result(count);
            for (const auto& part : recipe.at("parts"))
            {
                const int x = part.at("chunk_offset").at(0);
                const int y = part.at("chunk_offset").at(1);
                const int index = y * 2 + x;
                const std::string key = part.at("modelKey");
                Require(x >= 0 && x < (count == 1 ? 1 : 2) && y >= 0 && y < (count == 4 ? 2 : 1)
                            && index >= 0 && size_t(index) < count && result[index].empty()
                            && catalog->grounds.count(key) != 0,
                        "Invalid terrain recipe part");
                result[index] = key;
                Require(used.insert(key).second, "Chunk model occurs in more than one recipe");
            }
            Require(std::none_of(result.begin(),
                                 result.end(),
                                 [](const auto& key)
                                 {
                                     return key.empty();
                                 }),
                    "Incomplete terrain recipe");
            if (recipe.contains("optional_shared_props"))
            {
                for (const auto& input : recipe.at("optional_shared_props"))
                {
                    const int x = input.at("owner_chunk").at(0);
                    const int y = input.at("owner_chunk").at(1);
                    Require(x >= 0 && x < 2 && y >= 0 && y < (count == 4 ? 2 : 1),
                            "Invalid terrain prop owner chunk");
                    const int index = y * 2 + x;
                    catalog->grounds.at(result.at(index))
                        .props.push_back(anchor(input, {double(x * 8), double(y * 8)}, false));
                }
            }
            return result;
        };
        for (const auto& recipe : source.at("base_and_place_recipes"))
        {
            if (recipe.at("kind") == "base_biome")
            {
                catalog->bases.at(size_t(ParseBiome(recipe.at("biome_id"))))
                    .push_back(parts(recipe, 1)[0]);
            }
            else
            {
                Terrain::Place place;
                place.id = recipe.at("place_id");
                place.variant = recipe.at("variant_id");
                place.biome = ParseBiome(recipe.at("base_biome_example"));
                const auto keys = parts(recipe, 4);
                std::copy(keys.begin(), keys.end(), place.parts.begin());
                catalog->places.push_back(std::move(place));
            }
        }
        for (const auto& recipe : source.at("transitions"))
        {
            const auto keys = parts(recipe, 2);
            Require(catalog->transitions
                        .emplace(std::make_pair(ParseBiome(recipe.at("from")),
                                                ParseBiome(recipe.at("to"))),
                                 std::array<std::string, 2>{keys[0], keys[1]})
                        .second,
                    "Duplicate biome transition");
        }
        Require(catalog->grounds.size() == 88 && used.size() == 88 && shared.size() == 11
                    && catalog->places.size() == 12 && catalog->transitions.size() == 8,
                "Incomplete world terrain pack");
        for (int i = 0; i < int(Terrain::Biome::Count); ++i)
        {
            Require(catalog->bases.at(i).size() == 4, "Biome must contain four base variants");
            if (i > 0)
            {
                const auto a = static_cast<Terrain::Biome>(i - 1);
                const auto b = static_cast<Terrain::Biome>(i);
                Require(catalog->transitions.count({a, b}) || catalog->transitions.count({b, a}),
                        "Missing ecological step transition");
            }
        }
        m_WorldChunks = std::move(catalog);
        std::cout << "[WorldChunks] ready grounds=88 baseVariants=24 placeVariants=12"
                     " transitionPairs=8 sharedProps=11 sourceFiles=2\n";
    }
} // namespace Models
