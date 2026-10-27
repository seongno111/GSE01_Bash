#pragma once
#include "NpcAI.h"
#include "Villages.h"
#include <filesystem>
#include <chrono>
#include <set>

class NpcWorld
{
public:

    NpcWorld(Prototype& game, std::uint64_t seed);
    void Update(float dt);
    void UpdateActor(NpcActor& npc, float dt);
    bool Key(unsigned char key);
    void DrawHUD(Renderer& renderer);
    bool PlayerCanTarget(const NpcActor& npc) const;
    bool HitByPlayer(double x, double y, float damage, bool allowPK);
    bool HitByMonster(double x, double y, float damage, ActorId source = InvalidActor);
    Actor* EnemyTarget(const EnemyActor& enemy) const;
    void MonsterDamage(Actor& target, float damage, const EnemyActor* source = nullptr);

    const std::vector<ActorId>& Actors() const
    {
        return m_Actors;
    }

    bool PKEnabled() const
    {
        return m_PK;
    }

    NpcActor* Selected() const;

    double Time() const
    {
        return m_Time;
    }

    std::uint64_t Seed() const
    {
        return m_Seed;
    }

    struct PerformanceStats
    {
        std::uint64_t pathCalls = 0;
        std::uint64_t pathNodes = 0;
        double exportMs = 0;
    };

    const PerformanceStats& Performance() const
    {
        return m_Performance;
    }

    bool ExportLog();
    void RecordWorldEvent(const std::string& type,
                          const std::string& text,
                          WorldPoint position,
                          const Actor* actor = nullptr,
                          const Actor* subject = nullptr,
                          std::uint64_t sourceEvent = 0);

private:

    friend class NpcMissileActor;

    struct Reservation
    {
        NpcEntity owner = NoNpcEntity;
        double until = 0;
    };

    struct Request
    {
        NpcEntity npc = NoNpcEntity;
        NpcAction action = NpcAction::Observe;
        double expires = 0;
    };

    struct WorldEvent
    {
        std::uint64_t id = 0;
        double time = 0;
        std::string type;
        std::string text;
        WorldPoint position;
        ActorId actor = InvalidActor;
        std::string actorName;
        ActorId subject = InvalidActor;
        std::string subjectName;
        std::uint64_t sourceEvent = 0;
        std::vector<ActorId> knowers;
    };

    struct Conduct
    {
        int care = 0;
        int violence = 0;
    };

    NpcActor* Npc(NpcEntity entity) const;
    Actor* Resolve(NpcEntity entity) const;
    NpcEntity Entity(const Actor& actor) const;
    bool Alive(NpcEntity entity) const;
    bool Visible(const NpcActor& npc, NpcEntity entity) const;
    bool Hostile(const NpcActor& npc, NpcEntity entity) const;
    void Sense(NpcActor& npc);
    void Decide(NpcActor& npc);
    void BuildCandidates(NpcActor& npc);
    WorldPoint ActivityPoint(const NpcActor& npc, NpcAction action) const;
    bool Start(NpcActor& npc, const NpcCandidate& choice, bool request = false);
    void Execute(NpcActor& npc, float dt);
    bool Apply(NpcActor& npc);
    void Finish(NpcActor& npc, bool success, const std::string& reason);
    bool Travel(NpcActor& npc, WorldPoint target, float dt);
    bool BuildPath(NpcActor& npc, WorldPoint target);
    bool Walkable(double x, double y) const;
    bool CanReserve(std::uint64_t resource, NpcEntity owner) const;
    std::uint64_t Resource(const NpcActor& npc, NpcAction action, NpcEntity target) const;
    void Release(NpcActor& npc);
    void Damage(NpcEntity attacker, NpcEntity target, float amount, bool robbery = false);
    bool Crime(NpcEntity attacker, NpcEntity target);
    void SpreadRumors();
    void Event(const std::string& text);
    void LogDecision(const NpcActor& npc);
    bool RequestInteraction(NpcAction action);
    bool InVillage(WorldPoint p) const;
    bool ExportMonitor();
    void InitializeSociety();
    void UpdateSociety();
    bool UpdateMission(NpcActor& npc, float dt);
    void ExchangeVillageKnowledge(NpcActor& npc, size_t village);
    void InitializeOutposts();
    void UpdateOutposts();
    bool UpdateOutpostEnemy(EnemyActor& enemy, float dt);
    void VillageIncident(size_t from, size_t to, float amount);
    void UpdateCommunication();
    void UpdateMigrationChoices();
    bool UpdateMigration(NpcActor& npc, float dt);
    float GeographicRisk(size_t village) const;
    bool FindMigrationHome(size_t village, WorldPoint& home) const;
    double JourneyBudget(WorldPoint from, WorldPoint to, float speed) const;
    void RebuildNpcSpatialIndex();
    std::vector<ActorId> NearbyNpcs(WorldPoint position, double radius) const;

    friend class EnemyActor;

    struct VillageSociety
    {
        float alignment = 0;
        bool threatened = false;
        double nextDispatch = 0;
        unsigned dispatches = 0;
        double nextPost = 3;
        unsigned postsSent = 0;
        unsigned postsReceived = 0;
        unsigned immigrants = 0;
        unsigned emigrants = 0;
        std::map<size_t, VillageKnowledge> known;
        std::array<float, Villages.size()> relations{};
        std::array<float, Villages.size()> incidents{};
    };

    struct EnemyOutpost
    {
        std::vector<ActorId> members;
        double nextSpawn = 0;
        double nextExpansion = 40;
        double nextWave = 45;
        float radius = 18;
        unsigned waves = 0;
        unsigned spawned = 0;
    };

    std::array<VillageSociety, Villages.size()> m_Societies{};
    std::array<ActorId, Villages.size()> m_VillageRoots{};
    std::vector<EnemyOutpost> m_Outposts;
    double m_NextWorldSimulation = 0;
    double m_NextMigrationReview = 25;
    size_t m_RumorCursor = 0;
    std::map<std::pair<int, int>, std::vector<ActorId>> m_NpcSpatialIndex;

    struct CarrierPost
    {
        std::uint64_t id = 0;
        size_t source = 0;
        size_t destination = 0;
        double departedAt = 0;
        double arrivesAt = 0;
        ActorId bird = InvalidActor;
        std::map<size_t, VillageKnowledge> reports;
        std::vector<WorldEvent> events;
    };

    std::vector<CarrierPost> m_CarrierPosts;
    std::uint64_t m_NextCarrierPost = 1;

    PerformanceStats m_Performance;
    Prototype& m_Game;
    std::uint64_t m_Seed;
    std::vector<ActorId> m_Actors;
    std::vector<ActorId> m_Enemies;
    std::map<std::uint64_t, Reservation> m_Reservations;
    std::deque<Request> m_Requests;
    std::deque<std::string> m_Log;
    std::deque<std::string> m_Events;
    std::deque<WorldEvent> m_WorldEvents;
    std::map<std::string, std::uint64_t> m_EventCounts;
    std::map<ActorId, Conduct> m_Conduct;
    std::set<std::pair<ActorId, ActorId>> m_UnlawfulConflicts;
    std::uint64_t m_NextWorldEvent = 1;
    double m_NextRumor = 3;
    std::filesystem::path m_LogDirectory;
    double m_Time = 0;
    double m_Accumulator = 0;
    double m_NextExport = 10;
    std::uint64_t m_NextExecution = 1;
    size_t m_Selected = 0;

    struct VillageEconomy
    {
        int treasury = 4200;
        int food = 420;
        int medicine = 210;
        int goods = 0;
    };

    std::array<VillageEconomy, Villages.size()> m_Economies{};
    bool m_PK = false;
    bool m_Inspector = true;
    bool m_LogWarning = false;
    bool m_MonitorWarning = false;
    std::string m_MonitorSession;
    std::uint64_t m_MonitorSequence = 0;
    std::chrono::steady_clock::time_point m_NextMonitor{};
};
