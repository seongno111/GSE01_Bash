#include "stdafx.h"
#include "ModelLibrary.h"
#include "HeroWeapons.h"
#include "Dependencies/nlohmann/json.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <cmath>
#include <set>
#include <string_view>

namespace
{
    void CheckSharedHeroBind(const Models::Asset& common,
                             const Models::Asset& equipped,
                             const nlohmann::json& bones)
    {
        const auto parentName = [](const Models::Asset& asset, int parent) -> std::string_view
        {
            return parent < 0 ? std::string_view{} : std::string_view(asset.nodes.at(parent).name);
        };
        for (const auto& bone : bones)
        {
            const auto name = bone.get<std::string>();
            const auto& a = common.nodes.at(common.nodeByName.at(name));
            const auto& b = equipped.nodes.at(equipped.nodeByName.at(name));
            const auto left = a.bind.LocalMatrix();
            const auto right = b.bind.LocalMatrix();
            if (parentName(common, a.parent) != parentName(equipped, b.parent)
                || !std::equal(left.begin(),
                               left.end(),
                               right.begin(),
                               [](float x, float y)
                               {
                                   return std::abs(x - y) < .0001f;
                               }))
            {
                throw std::runtime_error("Equipped hero uses a different bind pose: " + name);
            }
        }
    }
} // namespace

void Models::ModelLibrary::LoadHeroWeapons(const std::filesystem::path& folder)
{
    if (!m_Hero)
    {
        return;
    }
    LoadHeroWeaponAnimations(folder.parent_path() / "HeroWeaponAnimations_v1");
    ModelInstance sword{m_Hero.asset, {}};
    for (int node : m_Hero.nodes)
    {
        if (m_Hero.asset->nodes[node].name.find("HERO_WEAPON_R_") == 0)
        {
            sword.nodes.push_back(node);
        }
    }
    if (sword)
    {
        m_WeaponPreviews.emplace(HeroLongswordKey, std::move(sword));
        m_EquippedHeroes.try_emplace(HeroLongswordKey, m_Hero);
    }
    try
    {
        std::ifstream input(folder / "weapon_catalog.json");
        const auto catalog = nlohmann::json::parse(input);
        if (catalog.at("schema_version") != 1)
        {
            throw std::runtime_error("Unsupported weapon catalog");
        }
        auto arrow = ReadCached(folder / "AMMO_Arrow_00_Iron.glb");
        m_Arrow.asset = arrow;
        for (size_t i = 0; i < arrow->nodes.size(); ++i)
        {
            if (arrow->nodes[i].mesh >= 0)
            {
                m_Arrow.nodes.push_back(int(i));
            }
        }
        for (const auto& definition : HeroWeapons)
        {
            if (std::string_view(definition.key) == HeroLongswordKey)
            {
                continue;
            }
            try
            {
                const nlohmann::json* entry = nullptr;
                for (const auto& item : catalog.at("weapons"))
                {
                    if (item.at("weapon_key") == definition.key)
                    {
                        entry = &item;
                        break;
                    }
                }
                if (!entry)
                {
                    throw std::runtime_error("Weapon catalog entry missing");
                }
                auto weapon = ReadCached(folder / (std::string(definition.key) + ".glb"));
                if (!weapon->skins.empty() || !weapon->textures.empty())
                {
                    throw std::runtime_error("Weapon attachment requires rigid untextured assets");
                }
                ModelInstance preview{weapon, {}};
                for (size_t i = 0; i < weapon->nodes.size(); ++i)
                {
                    if (weapon->nodes[i].mesh >= 0)
                    {
                        preview.nodes.push_back(int(i));
                    }
                }
                if (m_EquippedHeroes.count(definition.key))
                {
                    m_WeaponPreviews.emplace(definition.key, std::move(preview));
                    continue;
                }
                auto hero = std::make_shared<Asset>(*m_Hero.asset);
                hero->source = folder / (std::string(definition.key) + "_equipped");
                ModelInstance equipped{hero, {}};
                const std::string bone = entry->at("hero_bone");
                const int parent = hero->nodeByName.at(bone);
                const auto mount = catalog.at("hero_rest_mounts_gltf")
                                       .at(bone)
                                       .at("matrix_column_major")
                                       .get<Matrix>();
                for (int node : m_Hero.nodes)
                {
                    const auto& name = hero->nodes[node].name;
                    if (name.find("HERO_WEAPON_R_") == std::string::npos
                        && (bone != "weapon_l" || name.find("HERO_WEAPON_L_") == std::string::npos))
                    {
                        equipped.nodes.push_back(node);
                    }
                }
                if (bone == "weapon_l")
                {
                    hero->pointLights.clear();
                }
                const int meshOffset = int(hero->meshes.size());
                const int materialOffset = int(hero->materials.size());
                const int nodeOffset = int(hero->nodes.size());
                hero->materials.insert(hero->materials.end(),
                                       weapon->materials.begin(),
                                       weapon->materials.end());
                for (auto mesh : weapon->meshes)
                {
                    for (auto& primitive : mesh.primitives)
                    {
                        if (primitive.material >= 0)
                        {
                            primitive.material += materialOffset;
                        }
                    }
                    hero->meshes.push_back(std::move(mesh));
                }
                for (auto node : weapon->nodes)
                {
                    if (node.parent < 0)
                    {
                        node.parent = parent;
                        node.bind.matrix = Multiply(mount, node.bind.LocalMatrix());
                        node.bind.matrixAuthored = true;
                    }
                    else
                    {
                        node.parent += nodeOffset;
                    }
                    if (node.mesh >= 0)
                    {
                        node.mesh += meshOffset;
                    }
                    hero->nodeByName[node.name] = int(hero->nodes.size());
                    hero->nodes.push_back(std::move(node));
                }
                for (int node : weapon->evaluationOrder)
                {
                    hero->evaluationOrder.push_back(node + nodeOffset);
                }
                for (int node : preview.nodes)
                {
                    equipped.nodes.push_back(node + nodeOffset);
                }
                m_WeaponPreviews.emplace(definition.key, std::move(preview));
                m_EquippedHeroes.emplace(definition.key, std::move(equipped));
            }
            catch (const std::exception& error)
            {
                std::cerr << "[Weapons] " << definition.key << " - " << error.what() << '\n';
            }
        }
    }
    catch (const std::exception& error)
    {
        std::cerr << "[Weapons] " << error.what() << '\n';
    }
    for (const auto& weapon : HeroWeapons)
    {
        if (const auto* clip = HeroWeaponSkill(weapon))
        {
            std::cout << "[HeroSkill] weapon=" << weapon.key << " clip=" << clip->name
                      << " duration=" << clip->duration << " contacts=" << weapon.skill.hits
                      << '\n';
        }
    }
}

Models::ModelInstance Models::ModelLibrary::HeroWeaponModel(const std::string& key,
                                                            bool equipped) const
{
    const auto& models = equipped ? m_EquippedHeroes : m_WeaponPreviews;
    const auto found = models.find(key);
    return found == models.end() ? ModelInstance{} : found->second;
}

void Models::ModelLibrary::LoadHeroWeaponAnimations(const std::filesystem::path& folder)
{
    if (!std::filesystem::is_regular_file(folder / "animation_catalog.json"))
    {
        return;
    }
    try
    {
        std::ifstream input(folder / "animation_catalog.json");
        const auto catalog = nlohmann::json::parse(input);
        if (catalog.at("schema_version") != 1)
        {
            throw std::runtime_error("Unsupported hero animation catalog");
        }
        const auto commonClips = catalog.at("original_clips").get<std::vector<std::string>>();
        for (const auto& equipment : catalog.at("equipment"))
        {
            const std::string key = equipment.at("weapon_key");
            try
            {
                // The delivered meshes already contain skinned weapons. Do not attach them twice.
                auto hero = ReadCached(folder / equipment.at("file").get<std::string>());
                CheckSharedHeroBind(*m_Hero.asset, *hero, catalog.at("bones"));
                HeroWeaponVisual visual;
                const auto names = equipment.at("clip_names").get<std::vector<std::string>>();
                visual.prefix = names.at(0).substr(0, names.at(0).rfind('_'));
                for (const auto& name : names)
                {
                    auto& clip = hero->animations.at(hero->animationByName.at(name));
                    const auto metadata = std::find_if(catalog.at("new_clips").begin(),
                                                       catalog.at("new_clips").end(),
                                                       [&](const auto& item)
                                                       {
                                                           return item.at("name") == name;
                                                       });
                    if (metadata == catalog.at("new_clips").end()
                        || std::abs(clip.duration - metadata->at("duration_seconds").get<float>())
                               > .02f)
                    {
                        throw std::runtime_error("Missing or mismatched animation metadata: "
                                                 + name);
                    }
                    clip.loop = metadata->at("loop");
                    clip.events.clear();
                    for (const auto& event : metadata->at("events"))
                    {
                        const float time = event.at("time_seconds");
                        if (!std::isfinite(time) || time < 0 || time > clip.duration + .001f)
                        {
                            throw std::runtime_error("Animation event outside clip: " + name);
                        }
                        clip.events.push_back({event.at("name").get<std::string>(), time});
                    }
                }
                // Map by bone name: node ordering differs between exported GLBs.
                for (const auto& original : m_Hero.asset->animations)
                {
                    if (hero->animationByName.count(original.name)
                        || std::find(commonClips.begin(), commonClips.end(), original.name)
                               == commonClips.end())
                    {
                        continue;
                    }
                    auto clip = original;
                    for (auto& channel : clip.channels)
                    {
                        channel.node =
                            hero->nodeByName.at(m_Hero.asset->nodes.at(channel.node).name);
                    }
                    hero->animationByName.emplace(clip.name, int(hero->animations.size()));
                    hero->animations.push_back(std::move(clip));
                }
                // Compose Ready upper body with common locomotion once at load time.
                // Playback keeps one pose sample and one draw, including two-handed grips.
                std::set<int> upper;
                for (const auto& bone : catalog.at("upper_body_mask"))
                {
                    upper.insert(hero->nodeByName.at(bone.get<std::string>()));
                }
                const auto ready =
                    hero->animations.at(hero->animationByName.at(visual.prefix + "_Ready"));
                for (const char* movement : {"Walk", "Run"})
                {
                    auto clip = hero->animations.at(
                        hero->animationByName.at(std::string("HERO_") + movement));
                    clip.name = visual.prefix + "_" + movement;
                    clip.channels.erase(std::remove_if(clip.channels.begin(),
                                                       clip.channels.end(),
                                                       [&](const Channel& channel)
                                                       {
                                                           return upper.count(channel.node) != 0;
                                                       }),
                                        clip.channels.end());
                    for (auto channel : ready.channels)
                    {
                        if (upper.count(channel.node) || channel.path == ChannelPath::Weights)
                        {
                            const float ratio = clip.duration / ready.duration;
                            for (float& time : channel.times)
                            {
                                time *= ratio;
                            }
                            if (channel.interpolation == Interpolation::CubicSpline)
                            {
                                for (size_t k = 0; k < channel.values.size(); ++k)
                                {
                                    if (k % 3 != 1)
                                    {
                                        for (float& tangent : channel.values[k])
                                        {
                                            tangent /= ratio;
                                        }
                                    }
                                }
                            }
                            clip.channels.push_back(std::move(channel));
                        }
                    }
                    hero->animationByName[clip.name] = int(hero->animations.size());
                    hero->animations.push_back(std::move(clip));
                }
                if (equipment.at("weapon_kind") == "bow")
                {
                    HeroAttackVisual attack;
                    for (const char* stage : {"Nock", "Draw", "Aim", "Release"})
                    {
                        const auto& clip = hero->animations.at(
                            hero->animationByName.at(visual.prefix + "_" + stage));
                        for (const auto& event : clip.events)
                        {
                            if (event.name == "projectile_release")
                            {
                                attack.contact = attack.duration + event.time;
                            }
                        }
                        const float duration = std::string(stage) == "Aim" ? .12f : clip.duration;
                        if (std::string(stage) == "Aim")
                        {
                            visual.cancelAfter = attack.duration;
                        }
                        attack.stages.push_back({clip.name, duration, clip.duration});
                        attack.duration += duration;
                    }
                    visual.attacks.push_back(std::move(attack));
                    const auto& lower =
                        hero->animations.at(hero->animationByName.at(visual.prefix + "_Lower"));
                    visual.cancel = {{{lower.name, lower.duration, lower.duration}},
                                     lower.duration,
                                     -1};
                }
                else
                {
                    for (const auto& name : names)
                    {
                        const auto& clip = hero->animations.at(hero->animationByName.at(name));
                        for (const auto& event : clip.events)
                        {
                            if (event.name == "melee_contact" || event.name == "cast_release")
                            {
                                visual.attacks.push_back(
                                    {{{clip.name, clip.duration, clip.duration}},
                                     clip.duration,
                                     event.time});
                            }
                        }
                    }
                }
                if (visual.attacks.empty())
                {
                    throw std::runtime_error("Weapon has no attack timing");
                }
                const auto hidden =
                    equipment.at("hide_original_meshes").get<std::vector<std::string>>();
                ModelInstance model{hero, {}};
                for (size_t node = 0; node < hero->nodes.size(); ++node)
                {
                    if (hero->nodes[node].mesh >= 0
                        && std::find(hidden.begin(), hidden.end(), hero->nodes[node].name)
                               == hidden.end())
                    {
                        model.nodes.push_back(int(node));
                    }
                }
                if (equipment.at("hide_lamp_light").get<bool>())
                {
                    hero->pointLights.clear();
                }
                m_EquippedHeroes[key] = std::move(model);
                m_HeroWeaponActions[key] = std::move(visual);
                std::cout << "[HeroAnimation] weapon=" << key << " clips=" << names.size()
                          << " attacks=" << m_HeroWeaponActions.at(key).attacks.size()
                          << " layeredMovement=2\n";
            }
            catch (const std::exception& error)
            {
                std::cerr << "[HeroAnimation] " << key << " - " << error.what() << '\n';
            }
        }
    }
    catch (const std::exception& error)
    {
        std::cerr << "[HeroAnimation] " << error.what() << '\n';
    }
}

const Models::HeroWeaponVisual*
Models::ModelLibrary::HeroWeaponActions(const std::string& key) const
{
    const auto found = m_HeroWeaponActions.find(key);
    return found == m_HeroWeaponActions.end() ? nullptr : &found->second;
}

const Models::Animation* Models::ModelLibrary::HeroWeaponSkill(const HeroWeapon& weapon) const
{
    const auto model = m_EquippedHeroes.find(weapon.key);
    if (model == m_EquippedHeroes.end())
    {
        return nullptr;
    }
    const auto& asset = *model->second.asset;
    const auto found = asset.animationByName.find(weapon.skill.clip);
    if (found == asset.animationByName.end())
    {
        return nullptr;
    }
    const auto& clip = asset.animations.at(found->second);
    if (clip.loop || !std::isfinite(clip.duration) || clip.duration <= 0)
    {
        return nullptr;
    }
    int contacts = 0;
    float previous = 0;
    for (const auto& event : clip.events)
    {
        if (event.name == "skill_contact")
        {
            if (!std::isfinite(event.time) || event.time <= previous || event.time >= clip.duration)
            {
                return nullptr;
            }
            previous = event.time;
            ++contacts;
        }
    }
    return contacts == weapon.skill.hits ? &clip : nullptr;
}

void Models::ModelLibrary::LoadCorruptedHero(const std::filesystem::path& folder)
{
    if (!std::filesystem::is_regular_file(folder / "corruption_catalog.json"))
    {
        return;
    }
    try
    {
        std::ifstream input(folder / "corruption_catalog.json");
        const auto catalog = nlohmann::json::parse(input);
        if (catalog.at("schema_version") != 1 || catalog.at("units") != "meter"
            || catalog.at("up_gltf") != "Y" || catalog.at("front_gltf") != "+Z")
        {
            throw std::runtime_error("Unsupported corrupted hero catalog");
        }
        std::ifstream validationInput(folder / "validation.json");
        const auto validation = nlohmann::json::parse(validationInput);
        if (!validation.at("all_checks_passed").get<bool>())
        {
            throw std::runtime_error("Corrupted hero export validation failed");
        }
        auto asset = ReadCached(folder / catalog.at("part_library_file").get<std::string>());
        nlohmann::json bones = nlohmann::json::array();
        for (const auto& bone : catalog.at("bone_schema"))
        {
            bones.push_back(bone.at("name"));
        }
        CheckSharedHeroBind(*m_Hero.asset, *asset, bones);
        std::set<std::string> matched;
        for (const char* group : {"common_clips", "weapon_clips"})
        {
            for (const auto& metadata : catalog.at(group))
            {
                const std::string name = metadata.at("name");
                if (!matched.insert(name).second)
                {
                    throw std::runtime_error("Duplicate corrupted hero clip metadata: " + name);
                }
                auto& clip = asset->animations.at(asset->animationByName.at(name));
                const float duration = metadata.at("duration_seconds");
                if (!std::isfinite(duration) || std::abs(clip.duration - duration) > .04f)
                {
                    throw std::runtime_error("Corrupted hero clip length mismatch: " + name);
                }
                clip.loop = metadata.at("loop");
                clip.events.clear();
                for (const auto& marker : metadata.at("events"))
                {
                    const float time = marker.at("time_seconds");
                    if (!std::isfinite(time) || time < 0 || time > duration + .001f)
                    {
                        throw std::runtime_error("Invalid corrupted hero animation event");
                    }
                    clip.events.push_back({marker.at("name").get<std::string>(), time});
                }
            }
        }
        if (matched.size() != asset->animations.size())
        {
            throw std::runtime_error("Corrupted hero animation metadata incomplete");
        }
        std::map<int, int> parts;
        for (const auto& part : catalog.at("parts"))
        {
            const int node = asset->nodeByName.at(part.at("mesh").get<std::string>());
            if (asset->nodes[node].mesh < 0 || asset->nodes[node].skin < 0
                || !parts.emplace(part.at("part_index").get<int>(), node).second)
            {
                throw std::runtime_error("Invalid corrupted hero part index or skin");
            }
        }

        std::map<std::string, ModelInstance> models;
        for (const auto& module : catalog.at("modules"))
        {
            const std::string key = module.at("weapon_key");
            if (std::none_of(HeroWeapons.begin(),
                             HeroWeapons.end(),
                             [&](const HeroWeapon& weapon)
                             {
                                 return key == weapon.key;
                             }))
            {
                throw std::runtime_error("Unknown corrupted hero equipment: " + key);
            }
            ModelInstance model{asset, {}};
            std::set<int> selection;
            const auto indices = module.at("assembly_part_indices").get<std::vector<int>>();
            const auto names = module.at("assembly_parts").get<std::vector<std::string>>();
            if (indices.size() != names.size())
            {
                throw std::runtime_error("Corrupted hero assembly selection mismatch");
            }
            for (size_t i = 0; i < indices.size(); ++i)
            {
                const int node = parts.at(indices[i]);
                if (asset->nodes[node].name != names[i] || !selection.insert(node).second)
                {
                    throw std::runtime_error("Duplicate or mismatched corrupted hero part");
                }
                model.nodes.push_back(node);
            }
            for (const auto& name : module.at("clip_names"))
            {
                if (!asset->animationByName.count(name.get<std::string>()))
                {
                    throw std::runtime_error("Corrupted hero weapon clip missing");
                }
            }

            // Reuse the equipped upper-body locomotion, mapping all nodes by name.
            if (const auto* actions = HeroWeaponActions(key))
            {
                const auto source = HeroWeaponModel(key, true);
                for (const char* movement : {"Walk", "Run"})
                {
                    const std::string name = actions->prefix + "_" + movement;
                    auto clip = source.asset->animations.at(source.asset->animationByName.at(name));
                    for (auto& channel : clip.channels)
                    {
                        std::string node = source.asset->nodes.at(channel.node).name;
                        const auto remapped = module.at("source_node_map").find(node);
                        if (remapped != module.at("source_node_map").end())
                        {
                            node = remapped->get<std::string>();
                        }
                        channel.node = asset->nodeByName.at(node);
                    }
                    asset->animationByName.emplace(name, int(asset->animations.size()));
                    asset->animations.push_back(std::move(clip));
                }
            }
            if (!models.emplace(key, std::move(model)).second)
            {
                throw std::runtime_error("Duplicate corrupted hero weapon key");
            }
            std::cout << "[HeroCorruption] weapon=" << key << " parts=" << selection.size()
                      << " sharedMeshes=" << asset->meshes.size() << '\n';
        }
        if (models.size() != HeroWeapons.size())
        {
            throw std::runtime_error("Corrupted hero pack must cover all equipped weapons");
        }
        m_CorruptedHeroes = std::move(models);
    }
    catch (const std::exception& error)
    {
        std::cerr << "[HeroCorruption] unavailable: " << error.what() << '\n';
    }
}

const Models::ModelInstance* Models::ModelLibrary::CorruptedHero(const std::string& key) const
{
    const auto found = m_CorruptedHeroes.find(key);
    return found == m_CorruptedHeroes.end() ? nullptr : &found->second;
}
