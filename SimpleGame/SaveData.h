#pragma once
#include "NpcWorld.h"
#include <utility>

namespace Saves
{
    struct Player
    {
        ActorId actor = InvalidActor;
        WorldPoint position;
        bool enabled = true;
        decltype(std::declval<PlayerActor>().level) level{};
        decltype(std::declval<PlayerActor>().experience) experience{};
        decltype(std::declval<PlayerActor>().shards) shards{};
        decltype(std::declval<PlayerActor>().weaponRank) weaponRank{};
        decltype(std::declval<PlayerActor>().weapon) weapon{};
        decltype(std::declval<PlayerActor>().purificationStones) purificationStones{};
        decltype(std::declval<PlayerActor>().potions) potions{};
        decltype(std::declval<PlayerActor>().gold) gold{};
        decltype(std::declval<PlayerActor>().corruption) corruption{};
        decltype(std::declval<PlayerActor>().deaths) deaths{};
        decltype(std::declval<PlayerActor>().relics) relics{};
        decltype(std::declval<PlayerActor>().magnetTime) magnetTime{};
        decltype(std::declval<PlayerActor>().shotCooldown) shotCooldown{};
        decltype(std::declval<PlayerActor>().health) health{};
        decltype(std::declval<PlayerActor>().hitCooldown) hitCooldown{};
        decltype(std::declval<PlayerActor>().autoFire) autoFire{};

        static Player Capture(const PlayerActor& value)
        {
            Player result;
            result.actor = value.Id();
            result.position = value.WorldPosition();
            result.enabled = value.EnabledInHierarchy();
            result.level = value.level;
            result.experience = value.experience;
            result.shards = value.shards;
            result.weaponRank = value.weaponRank;
            result.weapon = value.weapon;
            result.purificationStones = value.purificationStones;
            result.potions = value.potions;
            result.gold = value.gold;
            result.corruption = value.corruption;
            result.deaths = value.deaths;
            result.relics = value.relics;
            result.magnetTime = value.magnetTime;
            result.shotCooldown = value.shotCooldown;
            result.health = value.health;
            result.hitCooldown = value.hitCooldown;
            result.autoFire = value.autoFire;
            return result;
        }

        void Apply(PlayerActor& value) const
        {
            value.level = level;
            value.experience = experience;
            value.shards = shards;
            value.weaponRank = weaponRank;
            value.weapon = weapon;
            value.purificationStones = purificationStones;
            value.potions = potions;
            value.gold = gold;
            value.corruption = corruption;
            value.deaths = deaths;
            value.relics = relics;
            value.magnetTime = magnetTime;
            value.shotCooldown = shotCooldown;
            value.health = health;
            value.hitCooldown = hitCooldown;
            value.autoFire = autoFire;
            value.SetWorldPosition(position.x, position.y);
            value.SetEnabled(enabled);
            value.ResetAnimation();
        }
        NLOHMANN_DEFINE_TYPE_INTRUSIVE(Player,
                                       actor,
                                       position,
                                       enabled,
                                       level,
                                       experience,
                                       shards,
                                       weaponRank,
                                       weapon,
                                       purificationStones,
                                       potions,
                                       gold,
                                       corruption,
                                       deaths,
                                       relics,
                                       magnetTime,
                                       shotCooldown,
                                       health,
                                       hitCooldown,
                                       autoFire)
    };

    struct Enemy
    {
        ActorId actor = InvalidActor;
        WorldPoint position;
        bool enabled = true;
        decltype(std::declval<EnemyActor>().kind) kind{};
        decltype(std::declval<EnemyActor>().homeX) homeX{};
        decltype(std::declval<EnemyActor>().homeY) homeY{};
        decltype(std::declval<EnemyActor>().health) health{};
        decltype(std::declval<EnemyActor>().maxHealth) maxHealth{};
        decltype(std::declval<EnemyActor>().damage) damage{};
        decltype(std::declval<EnemyActor>().speed) speed{};
        decltype(std::declval<EnemyActor>().respawn) respawn{};
        decltype(std::declval<EnemyActor>().recovery) recovery{};
        decltype(std::declval<EnemyActor>().experience) experience{};
        decltype(std::declval<EnemyActor>().tier) tier{};
        decltype(std::declval<EnemyActor>().alive) alive{};
        decltype(std::declval<EnemyActor>().burstCooldown) burstCooldown{};
        decltype(std::declval<EnemyActor>().outpost) outpost{};
        decltype(std::declval<EnemyActor>().raidVillage) raidVillage{};
        decltype(std::declval<EnemyActor>().returningHome) returningHome{};
        decltype(std::declval<EnemyActor>().patrolRadius) patrolRadius{};
        decltype(std::declval<EnemyActor>().raidUntil) raidUntil{};
        decltype(std::declval<EnemyActor>().specialCooldown) specialCooldown{};
        decltype(std::declval<EnemyActor>().guardRemaining) guardRemaining{};
        decltype(std::declval<EnemyActor>().enraged) enraged{};
        decltype(std::declval<EnemyActor>().wardEndedAt) wardEndedAt{};
        decltype(std::declval<EnemyActor>().lastHurtTime) lastHurtTime{};

        static Enemy Capture(const EnemyActor& value)
        {
            Enemy result;
            result.actor = value.Id();
            result.position = value.WorldPosition();
            result.enabled = value.EnabledInHierarchy();
            result.kind = value.kind;
            result.homeX = value.homeX;
            result.homeY = value.homeY;
            result.health = value.health;
            result.maxHealth = value.maxHealth;
            result.damage = value.damage;
            result.speed = value.speed;
            result.respawn = value.respawn;
            result.recovery = value.recovery;
            result.experience = value.experience;
            result.tier = value.tier;
            result.alive = value.alive;
            result.burstCooldown = value.burstCooldown;
            result.outpost = value.outpost;
            result.raidVillage = value.raidVillage;
            result.returningHome = value.returningHome;
            result.patrolRadius = value.patrolRadius;
            result.raidUntil = value.raidUntil;
            result.specialCooldown = value.specialCooldown;
            result.guardRemaining = value.guardRemaining;
            result.enraged = value.enraged;
            result.wardEndedAt = value.wardEndedAt;
            result.lastHurtTime = value.lastHurtTime;
            return result;
        }

        void Apply(EnemyActor& value) const
        {
            value.kind = kind;
            value.homeX = homeX;
            value.homeY = homeY;
            value.health = health;
            value.maxHealth = maxHealth;
            value.damage = damage;
            value.speed = speed;
            value.respawn = respawn;
            value.recovery = recovery;
            value.experience = experience;
            value.tier = tier;
            value.alive = alive;
            value.burstCooldown = burstCooldown;
            value.outpost = outpost;
            value.raidVillage = raidVillage;
            value.returningHome = returningHome;
            value.patrolRadius = patrolRadius;
            value.raidUntil = raidUntil;
            value.specialCooldown = specialCooldown;
            value.guardRemaining = guardRemaining;
            value.enraged = enraged;
            value.wardEndedAt = wardEndedAt;
            value.lastHurtTime = lastHurtTime;
            value.SetWorldPosition(position.x, position.y);
            value.SetEnabled(enabled);
            value.ResetAnimation();
        }
        NLOHMANN_DEFINE_TYPE_INTRUSIVE(Enemy,
                                       actor,
                                       position,
                                       enabled,
                                       kind,
                                       homeX,
                                       homeY,
                                       health,
                                       maxHealth,
                                       damage,
                                       speed,
                                       respawn,
                                       recovery,
                                       experience,
                                       tier,
                                       alive,
                                       burstCooldown,
                                       outpost,
                                       raidVillage,
                                       returningHome,
                                       patrolRadius,
                                       raidUntil,
                                       specialCooldown,
                                       guardRemaining,
                                       enraged,
                                       wardEndedAt,
                                       lastHurtTime)
    };

    struct Echo
    {
        ActorId actor = InvalidActor;
        WorldPoint position;
        bool enabled = true;
        decltype(std::declval<EchoActor>().id) id{};
        decltype(std::declval<EchoActor>().corruption) corruption{};
        decltype(std::declval<EchoActor>().health) health{};
        decltype(std::declval<EchoActor>().alive) alive{};
        decltype(std::declval<EchoActor>().maxHealth) maxHealth{};
        decltype(std::declval<EchoActor>().damage) damage{};
        decltype(std::declval<EchoActor>().level) level{};

        static Echo Capture(const EchoActor& value)
        {
            Echo result;
            result.actor = value.Id();
            result.position = value.WorldPosition();
            result.enabled = value.EnabledInHierarchy();
            result.id = value.id;
            result.corruption = value.corruption;
            result.health = value.health;
            result.alive = value.alive;
            result.maxHealth = value.maxHealth;
            result.damage = value.damage;
            result.level = value.level;
            return result;
        }

        void Apply(EchoActor& value) const
        {
            value.id = id;
            value.corruption = corruption;
            value.health = health;
            value.alive = alive;
            value.maxHealth = maxHealth;
            value.damage = damage;
            value.level = level;
            value.SetWorldPosition(position.x, position.y);
            value.SetEnabled(enabled);
            value.ResetAnimation();
        }
        NLOHMANN_DEFINE_TYPE_INTRUSIVE(Echo,
                                       actor,
                                       position,
                                       enabled,
                                       id,
                                       corruption,
                                       health,
                                       alive,
                                       maxHealth,
                                       damage,
                                       level)
    };

    struct Loot
    {
        ActorId actor = InvalidActor;
        WorldPoint position;
        bool enabled = true;
        decltype(std::declval<LootActor>().amount) amount{};
        decltype(std::declval<LootActor>().type) type{};
        decltype(std::declval<LootActor>().remaining) remaining{};

        static Loot Capture(const LootActor& value)
        {
            Loot result;
            result.actor = value.Id();
            result.position = value.WorldPosition();
            result.enabled = value.EnabledInHierarchy();
            result.amount = value.amount;
            result.type = value.type;
            result.remaining = value.remaining;
            return result;
        }

        void Apply(LootActor& value) const
        {
            value.amount = amount;
            value.type = type;
            value.remaining = remaining;
            value.SetWorldPosition(position.x, position.y);
            value.SetEnabled(enabled);
            value.ResetAnimation();
        }
        NLOHMANN_DEFINE_TYPE_INTRUSIVE(Loot, actor, position, enabled, amount, type, remaining)
    };

    struct Npc
    {
        ActorId actor = InvalidActor;
        WorldPoint position;
        bool enabled = true;
        decltype(std::declval<NpcActor>().profile) profile{};
        decltype(std::declval<NpcActor>().vitals) vitals{};
        decltype(std::declval<NpcActor>().birth) birth{};
        decltype(std::declval<NpcActor>().mission) mission{};
        decltype(std::declval<NpcActor>().nextMigration) nextMigration{};
        decltype(std::declval<NpcActor>().migrations) migrations{};
        decltype(std::declval<NpcActor>().migrationReason) migrationReason{};
        decltype(std::declval<NpcActor>().migrationHome) migrationHome{};
        decltype(std::declval<NpcActor>().villageKnowledge) villageKnowledge{};
        decltype(std::declval<NpcActor>().home) home{};
        decltype(std::declval<NpcActor>().knowledge) knowledge{};
        decltype(std::declval<NpcActor>().relations) relations{};
        decltype(std::declval<NpcActor>().cooldown) cooldown{};
        decltype(std::declval<NpcActor>().routeVisits) routeVisits{};
        decltype(std::declval<NpcActor>().history) history{};
        decltype(std::declval<NpcActor>().nextSense) nextSense{};
        decltype(std::declval<NpcActor>().nextDecision) nextDecision{};
        decltype(std::declval<NpcActor>().calmSince) calmSince{};
        decltype(std::declval<NpcActor>().lastHurt) lastHurt{};
        decltype(std::declval<NpcActor>().randomState) randomState{};
        decltype(std::declval<NpcActor>().decisions) decisions{};
        decltype(std::declval<NpcActor>().lastRoll) lastRoll{};
        decltype(std::declval<NpcActor>().activityAnchor) activityAnchor{};
        decltype(std::declval<NpcActor>().stationaryTime) stationaryTime{};
        decltype(std::declval<NpcActor>().distanceTravelled) distanceTravelled{};
        decltype(std::declval<NpcActor>().completedActions) completedActions{};
        decltype(std::declval<NpcActor>().failedActions) failedActions{};
        decltype(std::declval<NpcActor>().lastResult) lastResult{};

        static Npc Capture(const NpcActor& value)
        {
            Npc result;
            result.actor = value.Id();
            result.position = value.WorldPosition();
            result.enabled = value.EnabledInHierarchy();
            result.profile = value.profile;
            result.vitals = value.vitals;
            result.birth = value.birth;
            result.mission = value.mission;
            result.nextMigration = value.nextMigration;
            result.migrations = value.migrations;
            result.migrationReason = value.migrationReason;
            result.migrationHome = value.migrationHome;
            result.villageKnowledge = value.villageKnowledge;
            result.home = value.home;
            result.knowledge = value.knowledge;
            result.relations = value.relations;
            result.cooldown = value.cooldown;
            result.routeVisits = value.routeVisits;
            result.history = value.history;
            result.nextSense = value.nextSense;
            result.nextDecision = value.nextDecision;
            result.calmSince = value.calmSince;
            result.lastHurt = value.lastHurt;
            result.randomState = value.randomState;
            result.decisions = value.decisions;
            result.lastRoll = value.lastRoll;
            result.activityAnchor = value.activityAnchor;
            result.stationaryTime = value.stationaryTime;
            result.distanceTravelled = value.distanceTravelled;
            result.completedActions = value.completedActions;
            result.failedActions = value.failedActions;
            result.lastResult = value.lastResult;
            return result;
        }

        void Apply(NpcActor& value) const
        {
            value.profile = profile;
            value.vitals = vitals;
            value.birth = birth;
            value.mission = mission;
            value.nextMigration = nextMigration;
            value.migrations = migrations;
            value.migrationReason = migrationReason;
            value.migrationHome = migrationHome;
            value.villageKnowledge = villageKnowledge;
            value.home = home;
            value.knowledge = knowledge;
            value.relations = relations;
            value.cooldown = cooldown;
            value.routeVisits = routeVisits;
            value.history = history;
            value.nextSense = nextSense;
            value.nextDecision = nextDecision;
            value.calmSince = calmSince;
            value.lastHurt = lastHurt;
            value.randomState = randomState;
            value.decisions = decisions;
            value.lastRoll = lastRoll;
            value.activityAnchor = activityAnchor;
            value.stationaryTime = stationaryTime;
            value.distanceTravelled = distanceTravelled;
            value.completedActions = completedActions;
            value.failedActions = failedActions;
            value.lastResult = lastResult;
            value.SetWorldPosition(position.x, position.y);
            value.SetEnabled(enabled);
            value.ResetAnimation();
        }
        NLOHMANN_DEFINE_TYPE_INTRUSIVE(Npc,
                                       actor,
                                       position,
                                       enabled,
                                       profile,
                                       vitals,
                                       birth,
                                       mission,
                                       nextMigration,
                                       migrations,
                                       migrationReason,
                                       migrationHome,
                                       villageKnowledge,
                                       home,
                                       knowledge,
                                       relations,
                                       cooldown,
                                       routeVisits,
                                       history,
                                       nextSense,
                                       nextDecision,
                                       calmSince,
                                       lastHurt,
                                       randomState,
                                       decisions,
                                       lastRoll,
                                       activityAnchor,
                                       stationaryTime,
                                       distanceTravelled,
                                       completedActions,
                                       failedActions,
                                       lastResult)
    };

    struct Snapshot
    {
        std::uint64_t seed = 0;
        float time = 0;
        int farmKills = 0;
        int collectedItems = 0;
        bool guardianDefeated = false;
        bool levelComplete = false;
        bool guardianAnnounced = false;
        ActorId guardian = InvalidActor;
        std::uint64_t nextEcho = 1;
        Player player;
        float skillCooldown = 0;
        std::vector<Enemy> enemies;
        std::vector<Echo> echoes;
        std::vector<Loot> loot;
        std::vector<Npc> npcs;
        NpcWorld::SavedProgress world;
        NLOHMANN_DEFINE_TYPE_INTRUSIVE(Snapshot,
                                       seed,
                                       time,
                                       farmKills,
                                       collectedItems,
                                       guardianDefeated,
                                       levelComplete,
                                       guardianAnnounced,
                                       guardian,
                                       nextEcho,
                                       player,
                                       enemies,
                                       echoes,
                                       loot,
                                       npcs,
                                       world)
    };

    void Validate(const Snapshot& state);
} // namespace Saves
