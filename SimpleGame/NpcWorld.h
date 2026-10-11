#pragma once
#include "NpcAI.h"
#include <filesystem>
#include <chrono>

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
    bool HitByMonster(double x, double y, float damage);
    Actor* EnemyTarget(const EnemyActor& enemy) const;
    void MonsterDamage(Actor& target, float damage);

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

    bool ExportLog();

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
    std::uint64_t Resource(NpcAction action, NpcEntity target) const;
    void Release(NpcActor& npc);
    void Damage(NpcEntity attacker, NpcEntity target, float amount, bool robbery = false);
    void Crime(NpcEntity attacker, NpcEntity target);
    void Event(const std::string& text);
    void LogDecision(const NpcActor& npc);
    bool RequestInteraction(NpcAction action);
    bool InVillage(WorldPoint p) const;
    bool ExportMonitor();

    Prototype& m_Game;
    std::uint64_t m_Seed;
    std::vector<ActorId> m_Actors;
    std::vector<ActorId> m_Enemies;
    std::map<std::uint64_t, Reservation> m_Reservations;
    std::deque<Request> m_Requests;
    std::deque<std::string> m_Log;
    std::deque<std::string> m_Events;
    std::filesystem::path m_LogDirectory;
    double m_Time = 0;
    double m_Accumulator = 0;
    double m_NextExport = 10;
    std::uint64_t m_NextExecution = 1;
    size_t m_Selected = 0;
    int m_Treasury = 600;
    int m_MarketFood = 60;
    int m_MarketMedicine = 30;
    int m_MarketGoods = 0;
    bool m_PK = false;
    bool m_Inspector = true;
    bool m_LogWarning = false;
    bool m_MonitorWarning = false;
    std::string m_MonitorSession;
    std::uint64_t m_MonitorSequence = 0;
    std::chrono::steady_clock::time_point m_NextMonitor{};
};
