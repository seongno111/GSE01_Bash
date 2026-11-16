#include "stdafx.h"
#include "SaveData.h"
#include "HeroWeapons.h"
#include "Outposts.h"
#include <cmath>
#include <set>
#include <stdexcept>

namespace Saves
{
    void Validate(const Snapshot& state)
    {
        const auto require = [](bool good, const char* text)
        {
            if (!good)
            {
                throw std::runtime_error(text);
            }
        };
        const auto position = [&require](WorldPoint point)
        {
            require(std::isfinite(point.x) && std::isfinite(point.y)
                        && std::abs(point.x) <= 100000000 && std::abs(point.y) <= 100000000,
                    "Invalid saved position");
        };
        require(state.time >= 0 && std::isfinite(state.time) && state.world.Time >= 0,
                "Invalid saved world clock");
        require(!state.npcs.empty() && state.npcs.size() < 10000 && !state.enemies.empty()
                    && state.enemies.size() < 10000 && state.echoes.size() < 100000
                    && state.loot.size() < 100000,
                "Saved actor count outside limits");
        require(state.world.Outposts.size() == Outposts.size()
                    && state.world.Enemies.size() == state.enemies.size(),
                "Saved world layout is incompatible");
        const auto& player = state.player;
        require(std::isfinite(state.skillCooldown) && state.skillCooldown >= 0,
                "Invalid saved weapon skill cooldown");
        require(player.level >= 1 && player.level <= 1000000 && player.weapon >= 0
                    && size_t(player.weapon) < HeroWeapons.size() && player.health > 0
                    && player.experience >= 0 && player.corruption >= 0 && player.weaponRank >= 0
                    && player.gold >= 0 && player.shards >= 0 && player.potions >= 0
                    && player.relics >= 0 && player.purificationStones >= 0,
                "Invalid saved player progression");
        std::set<ActorId> ids;
        const auto actor = [&require, &position, &ids](const auto& item)
        {
            require(item.actor != InvalidActor && ids.insert(item.actor).second,
                    "Duplicate saved actor ID");
            position(item.position);
        };
        const auto knowledge = [&require, &position](const auto& reports)
        {
            for (const auto& report : reports)
            {
                require(report.first < Villages.size()
                            && report.second.sourceVillage < Villages.size(),
                        "Invalid village knowledge");
                position(report.second.position);
            }
        };
        actor(player);
        std::set<ActorId> enemies;
        for (const auto& enemy : state.enemies)
        {
            actor(enemy);
            enemies.insert(enemy.actor);
            require(int(enemy.kind) >= 0 && int(enemy.kind) < int(MonsterKind::Count)
                        && enemy.tier >= 0 && enemy.tier <= 2 && enemy.maxHealth > 0
                        && enemy.outpost >= -1 && enemy.outpost < int(Outposts.size())
                        && enemy.raidVillage >= -1 && enemy.raidVillage < int(Villages.size()),
                    "Invalid saved monster");
            position({enemy.homeX, enemy.homeY});
        }
        require(enemies.count(state.guardian) != 0, "Saved guardian is missing");
        std::set<ActorId> worldEnemies(state.world.Enemies.begin(), state.world.Enemies.end());
        require(worldEnemies == enemies, "Saved enemy references do not match");
        for (const auto& camp : state.world.Outposts)
        {
            for (auto member : camp.members)
            {
                require(enemies.count(member) != 0, "Missing outpost member");
            }
        }
        std::set<std::uint64_t> echoes;
        for (const auto& echo : state.echoes)
        {
            actor(echo);
            require(echo.id > 0 && echo.id < state.nextEcho && echoes.insert(echo.id).second
                        && echo.maxHealth > 0 && echo.level >= 1,
                    "Invalid mutation echo");
        }
        for (const auto& loot : state.loot)
        {
            actor(loot);
            require(int(loot.type) >= 0 && int(loot.type) <= int(LootType::Magnet)
                        && loot.amount > 0,
                    "Invalid saved loot");
        }
        for (size_t i = 0; i < state.npcs.size(); ++i)
        {
            const auto& npc = state.npcs[i];
            actor(npc);
            require(npc.profile.id == i + 1 && npc.profile.village < Villages.size()
                        && int(npc.profile.role) >= 0
                        && int(npc.profile.role) <= int(NpcRole::Merchant)
                        && int(npc.profile.goal) >= 0
                        && int(npc.profile.goal) <= int(NpcGoal::Power) && npc.vitals.maxHealth > 0
                        && npc.vitals.moveSpeed > 0 && int(npc.mission.type) >= 0
                        && int(npc.mission.type) <= int(VillageMission::Relocate)
                        && npc.mission.destination < Villages.size(),
                    "Invalid saved NPC");
            position(npc.home);
            position(npc.migrationHome);
            position(npc.activityAnchor);
            knowledge(npc.villageKnowledge);
            for (const auto& memory : npc.knowledge)
            {
                position(memory.position);
            }
            for (const auto& item : npc.history)
            {
                require(int(item.first) >= 0 && int(item.first) < int(NpcAction::Count),
                        "Invalid NPC action history");
            }
        }
        for (const auto& society : state.world.Societies)
        {
            knowledge(society.known);
        }
        for (const auto& post : state.world.CarrierPosts)
        {
            require(post.source < Villages.size() && post.destination < Villages.size()
                        && post.arrivesAt > post.departedAt,
                    "Invalid saved carrier post");
            knowledge(post.reports);
        }
    }
} // namespace Saves
