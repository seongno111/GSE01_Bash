#include "stdafx.h"
#include "ModelLibrary.h"
#include "Dependencies/nlohmann/json.hpp"

#include <Windows.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>

namespace Models
{
    namespace
    {
        using Json = nlohmann::json;

        Json ReadJson(const std::filesystem::path& path)
        {
            if (std::filesystem::file_size(path) > 16 * 1024 * 1024)
            {
                throw std::runtime_error("Model catalog exceeds 16 MiB");
            }
            std::ifstream stream(path, std::ios::binary);
            return Json::parse(stream);
        }

        void Check(bool condition, const std::string& message)
        {
            if (!condition)
            {
                throw std::runtime_error(message);
            }
        }

        int MeshNode(const Asset& asset, const std::string& name)
        {
            const auto found = asset.nodeByName.find(name);
            Check(found != asset.nodeByName.end() && asset.nodes[found->second].mesh >= 0,
                  "Catalog mesh node is missing: " + name);
            return found->second;
        }

        void ApplyMetadata(Asset& asset, const Json& catalog)
        {
            std::set<std::string> matched;
            for (const auto& entry : catalog.at("clips"))
            {
                const std::string name = entry.at("name");
                const auto found = asset.animationByName.find(name);
                if (found == asset.animationByName.end())
                {
                    continue;
                }
                auto& clip = asset.animations[found->second];
                Check(matched.insert(name).second, "Duplicate animation metadata: " + name);
                const float duration = entry.at("duration_seconds");
                Check(std::isfinite(duration) && std::abs(clip.duration - duration) < .04f,
                      "Animation duration differs from catalog: " + name);
                clip.loop = entry.value("loop", false);
                clip.rootYawDegrees = entry.value("root_yaw_degrees", 0.0f);
                Check(std::isfinite(clip.rootYawDegrees), "Invalid root yaw metadata");
                for (const auto& marker : entry.at("events"))
                {
                    AnimationEvent event;
                    event.name = marker.at("name");
                    event.time = marker.at("time_seconds");
                    Check(std::isfinite(event.time) && event.time >= 0
                              && event.time <= clip.duration + .001f,
                          "Animation event lies outside clip: " + name);
                    event.time = (std::min)(event.time, clip.duration);
                    clip.events.push_back(std::move(event));
                }
                std::stable_sort(clip.events.begin(),
                                 clip.events.end(),
                                 [](const auto& a, const auto& b)
                                 {
                                     return a.time < b.time;
                                 });
            }
            Check(matched.size() == asset.animations.size(),
                  "Animation catalog is missing exported clips");
        }

        std::vector<std::filesystem::path> DiscoverRoots(const std::filesystem::path& requested)
        {
            if (!requested.empty())
            {
                return {requested};
            }
            std::vector<std::filesystem::path> roots;
            char* configured = nullptr;
            size_t size = 0;
            if (_dupenv_s(&configured, &size, "EMBER_MODELING_ROOT") == 0 && configured)
            {
                roots.push_back(std::filesystem::u8path(configured));
            }
            std::free(configured);
            std::array<wchar_t, 32768> executable{};
            const DWORD length = GetModuleFileNameW(nullptr,
                                                    executable.data(),
                                                    static_cast<DWORD>(executable.size()));
            std::vector<std::filesystem::path> bases{std::filesystem::current_path()};
            if (length > 0 && length < executable.size())
            {
                bases.insert(bases.begin(), std::filesystem::path(executable.data()).parent_path());
            }
            for (auto base : bases)
            {
                for (int depth = 0; depth < 5 && !base.empty(); ++depth)
                {
                    roots.push_back(base / L"Modeling");
                    const auto parent = base.parent_path();
                    if (parent == base)
                    {
                        break;
                    }
                    base = parent;
                }
            }
            return roots;
        }
    } // namespace

    std::shared_ptr<Asset> ModelLibrary::ReadCached(const std::filesystem::path& path)
    {
        const auto key = std::filesystem::weakly_canonical(path);
        const auto found = m_Cache.find(key);
        if (found != m_Cache.end())
        {
            ++m_CacheHits;
            return found->second;
        }
        const auto start = std::chrono::steady_clock::now();
        std::shared_ptr<Asset> asset;
        try
        {
            asset = LoadGltf(key);
        }
        catch (const std::exception& error)
        {
            throw std::runtime_error(key.u8string() + ": " + error.what());
        }
        size_t vertices = 0;
        size_t triangles = 0;
        for (const auto& mesh : asset->meshes)
        {
            for (const auto& primitive : mesh.primitives)
            {
                vertices += primitive.vertices.size();
                triangles += primitive.indices.size() / 3;
            }
        }
        const auto ms =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start)
                .count();
        std::cout << "[ModelAssets] file=" << key.filename().u8string()
                  << " meshes=" << asset->meshes.size() << " vertices=" << vertices
                  << " triangles=" << triangles << " materials=" << asset->materials.size()
                  << " skins=" << asset->skins.size() << " clips=" << asset->animations.size()
                  << " loadMs=" << ms << '\n';
        m_Cache.emplace(key, asset);
        return asset;
    }

    std::shared_ptr<const Asset> ModelLibrary::Load(const std::filesystem::path& path)
    {
        return ReadCached(path);
    }

    bool ModelLibrary::Initialize(const std::filesystem::path& modelingRoot)
    {
        try
        {
            std::set<std::filesystem::path> visited;
            for (const auto& root : DiscoverRoots(modelingRoot))
            {
                if (!std::filesystem::is_directory(root))
                {
                    continue;
                }
                std::vector<std::pair<int, std::filesystem::path>> packages;
                if (std::filesystem::is_regular_file(root / "asset_catalog.json"))
                {
                    packages.emplace_back(0, root);
                }
                for (const auto& entry : std::filesystem::directory_iterator(root))
                {
                    const std::string name = entry.path().filename().u8string();
                    const std::string prefix = "LowPolyKit_v";
                    if (!entry.is_directory() || name.compare(0, prefix.size(), prefix) != 0)
                    {
                        continue;
                    }
                    const std::string digits = name.substr(prefix.size());
                    if (!digits.empty() && digits.size() <= 4
                        && digits.find_first_not_of("0123456789") == std::string::npos)
                    {
                        packages.emplace_back(std::stoi(digits), entry.path());
                    }
                }
                std::sort(packages.begin(),
                          packages.end(),
                          [](const auto& a, const auto& b)
                          {
                              return a.first > b.first;
                          });
                for (const auto& package : packages)
                {
                    const auto key = std::filesystem::weakly_canonical(package.second);
                    if (!visited.insert(key).second)
                    {
                        continue;
                    }
                    try
                    {
                        // Commit only a complete package; keep the existing resource set on failure.
                        ModelLibrary candidate;
                        candidate.LoadPackage(key);
                        *this = std::move(candidate);
                        std::cout << "[ModelAssets] ready package=" << m_Package.u8string()
                                  << " schema=" << m_Schema
                                  << " npcAppearances=" << m_Appearances.size()
                                  << " sharedFiles=" << m_Cache.size() << '\n';
                        return true;
                    }
                    catch (const std::exception& error)
                    {
                        std::cerr << "[ModelAssets] skipped package=" << key.u8string()
                                  << " reason=" << error.what() << '\n';
                    }
                }
            }
        }
        catch (const std::exception& error)
        {
            std::cerr << "[ModelAssets] discovery failed: " << error.what() << '\n';
        }
        std::cerr << "[ModelAssets] No complete model package; existing visuals remain available\n";
        return false;
    }

    bool ModelLibrary::LoadPackage(const std::filesystem::path& path)
    {
        const auto catalog = ReadJson(path / "asset_catalog.json");
        const auto assignments = ReadJson(path / "npc_appearance_assignments.json");
        m_Schema = catalog.at("schema_version");
        Check(m_Schema >= 1 && m_Schema <= 3, "Unsupported asset catalog schema");
        const std::string units = catalog.value("units", std::string());
        Check(units == "meter" || units == "meters", "Models must use meters");
        Check(assignments.at("schema_version") == catalog.at("schema_version"),
              "Assignment schema mismatch");
        if (m_Schema >= 2)
        {
            const auto validation = ReadJson(path / "validation.json");
            for (const auto& check : validation.at("checks").items())
            {
                Check(check.value().is_boolean() && check.value().get<bool>(),
                      "Failed export check: " + check.key());
            }
        }
        m_Package = path;
        auto npc =
            ReadCached(path / (m_Schema >= 3 ? "npc/npc_parts_rigged.glb" : "npc_parts.glb"));
        m_NpcParts = npc;
        m_Props = ReadCached(path / (m_Schema >= 3 ? "props/props_library.glb" : "props.glb"));
        for (const auto& slot : catalog.at("slots"))
        {
            const int index = slot.at("slot_index");
            Check(index >= 0 && index < 128, "Slot index out of supported range");
            Check(m_Slots.emplace(slot.at("name").get<std::string>(), index).second,
                  "Duplicate slot name");
        }
        Check(m_Slots.size() == assignments.at("slot_order").size(),
              "Assignment slot count mismatch");
        for (const auto& slot : m_Slots)
        {
            Check(static_cast<size_t>(slot.second) < m_Slots.size()
                      && assignments.at("slot_order").at(slot.second) == slot.first,
                  "Assignment slot order mismatch");
        }
        for (const auto& part : catalog.at("parts"))
        {
            const int slot = part.at("slot_index");
            const int index = part.at("part_index");
            Check(slot >= 0 && static_cast<size_t>(slot) < m_Slots.size() && index >= 0,
                  "Invalid catalog part index");
            Check(m_PartNodes.emplace(std::make_pair(slot, index), MeshNode(*npc, part.at("mesh")))
                      .second,
                  "Duplicate part index");
        }
        const auto readAppearance = [&](const Json& input)
        {
            Appearance appearance;
            appearance.role = input.at("role_index");
            appearance.parts = input.at("part_indices").get<std::vector<int>>();
            Check(appearance.role >= 0 && appearance.role < 5
                      && appearance.parts.size() == m_Slots.size(),
                  "Invalid NPC appearance");
            for (size_t slot = 0; slot < appearance.parts.size(); ++slot)
            {
                const int part = appearance.parts[slot];
                Check(part == -1 || m_PartNodes.count({static_cast<int>(slot), part}) != 0,
                      "Appearance references an unknown part");
            }
            return appearance;
        };
        for (const auto& preset : catalog.at("npc_presets"))
        {
            auto appearance = readAppearance(preset);
            m_Presets[appearance.role].push_back(std::move(appearance));
        }
        Check(m_Presets.size() == 5, "Every NPC role needs a fallback preset");
        for (const auto& input : assignments.at("npcs"))
        {
            const auto id = input.at("npc_id").get<std::uint64_t>();
            Check(id > 0 && m_Appearances.emplace(id, readAppearance(input)).second,
                  "Duplicate NPC id");
        }
        if (catalog.contains("headgear_rules"))
        {
            for (const auto& rule : catalog.at("headgear_rules"))
            {
                m_Headgear.emplace(
                    rule.at("part_index").get<int>(),
                    std::make_pair(rule.value("hide_hair", false), rule.value("hide_head", false)));
            }
        }
        for (const auto& prop : catalog.at("props"))
        {
            m_PropNodes.emplace(prop.at("prop_index").get<int>(),
                                MeshNode(*m_Props, prop.at("mesh")));
        }
        if (m_Schema >= 3)
        {
            const auto metadata = ReadJson(path / "animation_catalog.json");
            Check(metadata.at("schema_version") == 3, "Unsupported animation catalog schema");
            Check(!npc->skins.empty() && !npc->animations.empty(), "NPC rig or clips missing");
            ApplyMetadata(*npc, metadata);
            for (const auto& weapon : catalog.at("npc_weapons").items())
            {
                const int role = std::stoi(weapon.key());
                const int part = weapon.value();
                const auto hand = m_Slots.find("accessory_hand");
                Check(role >= 0 && role < 5 && hand != m_Slots.end()
                          && m_PartNodes.count({hand->second, part}) != 0,
                      "Invalid role weapon mapping");
                m_Weapons.emplace(role, part);
            }
            for (const auto& mapping : catalog.at("game_prop_mapping"))
            {
                const int prop = mapping.at("prop_index");
                Check(m_PropNodes.count(prop) != 0, "Game prop mapping references missing mesh");
                m_GameProps.emplace(mapping.at("game_kind").get<int>(), prop);
            }
            for (const auto& monster : catalog.at("monsters"))
            {
                const std::string name = monster.at("name");
                Check(name.find_first_of("/\\:") == std::string::npos,
                      "Invalid monster asset name");
                auto asset = ReadCached(path / "monsters" / (name + "_rigged.glb"));
                Check(!asset->skins.empty() && !asset->animations.empty(),
                      "Monster rig or clips missing");
                ApplyMetadata(*asset, metadata);
                m_Monsters.emplace(monster.at("monster_index").get<int>(),
                                   ModelInstance{asset, {MeshNode(*asset, monster.at("mesh"))}});
            }
            for (const auto& input : metadata.at("npc_action_map"))
            {
                ActionVisual action;
                action.prepare = input.value("prepare", std::string());
                action.approach = input.value("approach", std::string());
                action.perform = input.value("perform", std::string());
                action.recover = input.value("recover", std::string());
                action.enter = input.value("enter", std::string());
                action.exit = input.value("exit", std::string());
                action.blendSeconds = input.value("blend_seconds", .12f);
                action.visualEventsOnly = input.value("visual_events_only", false);
                if (input.contains("equipment_hand_index"))
                {
                    action.overridesHand = true;
                    action.handEquipment = input.at("equipment_hand_index");
                }
                if (input.contains("perform_by_role"))
                {
                    for (const auto& role : input.at("perform_by_role").items())
                    {
                        action.performByRole.emplace(std::stoi(role.key()),
                                                     role.value().get<std::string>());
                    }
                }
                const auto validateClip = [&](const std::string& name)
                {
                    Check(name.empty() || (name == "role_attack" && !action.performByRole.empty())
                              || npc->animationByName.count(name) != 0,
                          "NPC action clip missing: " + name);
                };
                for (const auto& name : {action.prepare,
                                         action.approach,
                                         action.perform,
                                         action.recover,
                                         action.enter,
                                         action.exit})
                {
                    validateClip(name);
                }
                for (const auto& role : action.performByRole)
                {
                    validateClip(role.second);
                }
                m_Actions.emplace(input.at("npc_action_index").get<int>(), std::move(action));
            }
            for (const auto& input : metadata.at("monster_state_map"))
            {
                const int index = input.at("monster_index");
                const auto found = m_Monsters.find(index);
                Check(found != m_Monsters.end(), "Unknown monster in animation state map");
                for (const std::string state : {"idle",
                                                "travel",
                                                "chase",
                                                "attack",
                                                "hit",
                                                "death",
                                                "spawn",
                                                "burst",
                                                "enrage"})
                {
                    if (input.contains(state) && !input.at(state).is_null())
                    {
                        const std::string name = input.at(state);
                        Check(found->second.asset->animationByName.count(name) != 0,
                              "Missing monster state clip");
                        m_MonsterClips.emplace(std::make_pair(index, state), name);
                    }
                }
            }
        }
        const auto heroFolder = path.parent_path() / "Hero_v1";
        if (std::filesystem::is_regular_file(heroFolder / "hero_catalog.json"))
        {
            try
            {
                const auto heroCatalog = ReadJson(heroFolder / "hero_catalog.json");
                Check(heroCatalog.at("schema_version") == 1, "Unsupported hero catalog schema");
                const auto validation = ReadJson(heroFolder / "validation.json");
                for (const auto& check : validation.at("checks").items())
                {
                    Check(check.value().is_boolean() && check.value().get<bool>(),
                          "Hero export validation failed: " + check.key());
                }
                auto hero = ReadCached(heroFolder / "hero_rigged.glb");
                ApplyMetadata(*hero, heroCatalog);
                ModelInstance instance;
                instance.asset = hero;
                for (const auto& part : heroCatalog.at("parts"))
                {
                    instance.nodes.push_back(MeshNode(*hero, part.at("mesh")));
                }
                Check(!instance.nodes.empty() && !hero->skins.empty(),
                      "Hero meshes or skin missing");
                m_Hero = std::move(instance);
            }
            catch (const std::exception& error)
            {
                std::cerr << "[ModelAssets] hero unavailable: " << error.what() << '\n';
            }
        }
        const auto species = path.parent_path() / "Monsters_v1";
        if (std::filesystem::is_regular_file(species / "monster_catalog.json"))
        {
            try
            {
                LoadMonsterSpecies(species);
            }
            catch (const std::exception& error)
            {
                std::cerr << "[ModelAssets] monster species unavailable: " << error.what() << '\n';
            }
        }
        const auto villages = path.parent_path() / "Villages_v1";
        if (std::filesystem::is_regular_file(villages / "village_catalog.json"))
        {
            try
            {
                LoadVillageBuildings(villages);
            }
            catch (const std::exception& error)
            {
                std::cerr << "[ModelAssets] village themes unavailable: " << error.what() << '\n';
            }
        }
        return true;
    }

    ModelInstance ModelLibrary::Assemble(const Appearance& appearance, int actionIndex) const
    {
        auto parts = appearance.parts;
        const auto hide = [&](const char* name)
        {
            const auto found = m_Slots.find(name);
            if (found != m_Slots.end())
            {
                parts[found->second] = -1;
            }
        };
        const auto skirt = m_Slots.find("skirt");
        if (skirt != m_Slots.end() && parts[skirt->second] >= 0)
        {
            for (const char* name : {"pelvis", "thigh_l", "thigh_r", "shin_l", "shin_r"})
            {
                hide(name);
            }
        }
        const auto gear = m_Slots.find("headgear");
        if (gear != m_Slots.end())
        {
            const auto rule = m_Headgear.find(parts[gear->second]);
            if (rule != m_Headgear.end())
            {
                if (rule->second.first)
                {
                    hide("hair");
                }
                if (rule->second.second)
                {
                    hide("head");
                }
            }
        }
        const auto weapon = m_Weapons.find(appearance.role);
        const auto hand = m_Slots.find("accessory_hand");
        if (weapon != m_Weapons.end() && hand != m_Slots.end())
        {
            parts[hand->second] = weapon->second;
        }
        const auto* action = NpcAction(actionIndex);
        if (action && action->overridesHand && hand != m_Slots.end())
        {
            const int part = action->handEquipment;
            if (part == -1 || m_PartNodes.count({hand->second, part}) != 0)
            {
                parts[hand->second] = part;
            }
        }
        if (appearance.role == 1 && !m_Weapons.empty())
        {
            hide("accessory_waist");
        }
        ModelInstance instance;
        instance.asset = m_NpcParts;
        for (size_t slot = 0; slot < parts.size(); ++slot)
        {
            if (parts[slot] >= 0)
            {
                const auto node = m_PartNodes.find({static_cast<int>(slot), parts[slot]});
                if (node != m_PartNodes.end())
                {
                    instance.nodes.push_back(node->second);
                }
            }
        }
        return instance;
    }

    ModelInstance ModelLibrary::Npc(std::uint64_t id, int role, int action) const
    {
        const auto found = m_Appearances.find(id);
        if (found != m_Appearances.end() && found->second.role == role)
        {
            return Assemble(found->second, action);
        }
        const auto fallback = m_Presets.find(role);
        return fallback == m_Presets.end() || fallback->second.empty()
                   ? ModelInstance{}
                   : Assemble(fallback->second[id % fallback->second.size()], action);
    }

    ModelInstance ModelLibrary::Player(int role) const
    {
        return m_Hero ? m_Hero : Npc(0, role);
    }

    ModelInstance ModelLibrary::Monster(int index) const
    {
        const auto found = m_Monsters.find(index);
        return found == m_Monsters.end() ? ModelInstance{} : found->second;
    }

    ModelInstance ModelLibrary::Prop(int propIndex) const
    {
        const auto found = m_PropNodes.find(propIndex);
        return found == m_PropNodes.end() ? ModelInstance{}
                                          : ModelInstance{m_Props, {found->second}};
    }

    ModelInstance ModelLibrary::GameProp(int gameKind) const
    {
        const auto found = m_GameProps.find(gameKind);
        return found == m_GameProps.end() ? ModelInstance{} : Prop(found->second);
    }

    const ActionVisual* ModelLibrary::NpcAction(int action) const
    {
        const auto found = m_Actions.find(action);
        return found == m_Actions.end() ? nullptr : &found->second;
    }

    std::string ModelLibrary::MonsterClip(int index, const std::string& state) const
    {
        const auto found = m_MonsterClips.find({index, state});
        return found == m_MonsterClips.end() ? "" : found->second;
    }
} // namespace Models
