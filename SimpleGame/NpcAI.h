#pragma once
#include "GameActors.h"
#include <array>
#include <deque>
#include <map>
#include <vector>

class NpcWorld;
using NpcEntity = std::uint64_t;
constexpr NpcEntity NoNpcEntity = 0;
constexpr NpcEntity PlayerEntity = 1;

enum class NpcRole
{
    Priest,
    Knight,
    Adventurer,
    Raider,
    Merchant
};
enum class NpcGoal
{
    Health,
    Wealth,
    Wisdom,
    Faith,
    Power
};
enum class NpcMode
{
    Daily,
    Alert,
    Combat,
    Evacuate,
    Interaction,
    Dead
};
enum class NpcAction
{
    Observe,
    Work,
    Rest,
    Eat,
    Patrol,
    Hunt,
    Raid,
    Trade,
    Bless,
    Pray,
    Train,
    Reflect,
    Heal,
    Purify,
    DawnOil,
    Flee,
    Attack,
    Guard,
    ReturnHome,
    Count
};
enum class NpcPhase
{
    Prepare,
    Approach,
    Perform,
    Recover,
    Complete
};

constexpr size_t NpcActionCount = static_cast<size_t>(NpcAction::Count);
const char* NpcRoleName(NpcRole role);
const char* NpcGoalName(NpcGoal goal);
const char* NpcActionName(NpcAction action);
const char* NpcActivityName(NpcRole role, NpcAction action);
const char* NpcActionId(NpcAction action);
const char* NpcModeName(NpcMode mode);
const char* NpcPhaseName(NpcPhase phase);

struct NpcWeapon
{
    const char* name;
    float range;
    float windup;
    float recovery;
    float damageScale;
    float speed;
};

const NpcWeapon& NpcWeaponFor(NpcRole role);

struct NpcProfile
{
    std::uint64_t id = 0;
    std::string name;
    NpcRole role = NpcRole::Adventurer;
    NpcGoal goal = NpcGoal::Wealth;
    float courage = 0.5f;
    float compassion = 0.5f;
    float duty = 0.5f;
    float greed = 0.5f;
    float faith = 0.5f;
    float caution = 0.5f;
    float raidWeight = 0;
    float corruptionWeight = 0;
    float temperature = 1;
};

struct NpcVitals
{
    float health = 100;
    float maxHealth = 100;
    float fatigue = 0.15f;
    float hunger = 0.15f;
    float morale = 0.8f;
    float corruption = 0;
    float holyPower = 20;
    float strength = 10;
    float wisdom = 0;
    int gold = 30;
    int food = 3;
    int medicine = 1;
    int goods = 0;
    int dawnOil = 0;
    bool alive = true;
    bool frenzy = false;
};

struct NpcMemory
{
    NpcEntity entity = NoNpcEntity;
    WorldPoint position;
    double seenAt = -100;
    float healthRatio = 1;
    bool hostile = false;
    std::string source = "목격";
};

struct NpcRelation
{
    float trust = 0;
    float grudge = 0;
};

struct NpcCandidate
{
    NpcAction action = NpcAction::Observe;
    NpcEntity target = NoNpcEntity;
    WorldPoint destination;
    bool allowed = false;
    std::string reason;
    float base = 0;
    float personality = 0;
    float need = 0;
    float relation = 0;
    float situation = 0;
    float goal = 0;
    float cost = 0;
    float continuity = 1;
    float repetition = 1;
    float weight = 0;
    double probability = 0;
};

struct NpcExecution
{
    NpcAction action = NpcAction::Observe;
    NpcPhase phase = NpcPhase::Complete;
    NpcEntity target = NoNpcEntity;
    WorldPoint destination;
    std::uint64_t id = 0;
    double startedAt = 0;
    double phaseUntil = 0;
    bool applied = false;
    bool playerRequest = false;
    std::uint64_t reservation = 0;
};

class NpcActor final : public GameActor
{
public:

    NpcActor(Prototype& game, NpcWorld& world, NpcProfile profile, WorldPoint home);
    void Update(float dt) override;
    void Draw(Renderer& renderer) override;
    void DrawOverlay(Renderer& renderer) override;

    bool HasOverlay() const override
    {
        return true;
    }

    NpcEntity Entity() const
    {
        return 100 + profile.id;
    }

    NpcProfile profile;
    NpcVitals vitals;
    WorldPoint home;
    NpcMode mode = NpcMode::Daily;
    NpcExecution execution;
    std::vector<NpcMemory> knowledge;
    std::map<NpcEntity, NpcRelation> relations;
    std::array<double, NpcActionCount> cooldown{};
    std::array<std::uint64_t, NpcActionCount> routeVisits{};
    std::map<std::pair<NpcAction, NpcEntity>, double> failedUntil;
    std::deque<std::pair<NpcAction, double>> history;
    std::vector<NpcCandidate> candidates;
    std::vector<WorldPoint> path;
    size_t pathIndex = 0;
    double nextPath = 0;
    double nextSense = 0;
    double nextDecision = 0;
    double calmSince = 0;
    double lastHurt = -100;
    std::uint64_t randomState = 1;
    std::uint64_t decisions = 0;
    double lastRoll = 0;
    WorldPoint activityAnchor;
    double stationaryTime = 0;
    double distanceTravelled = 0;
    double stalledTime = 0;
    std::uint64_t completedActions = 0;
    std::uint64_t failedActions = 0;
    bool distant = false;
    std::string lastResult = "마을에 도착했다";

private:

    NpcWorld& m_World;
};

// Scores only eligible actions; one RNG sample per decision, no per-weight noise.
class NpcBrain
{
public:

    static void Score(NpcActor& npc, double now);
    static size_t Choose(NpcActor& npc);
};

class NpcMissileActor final : public GameActor
{
public:

    NpcMissileActor(Prototype& game,
                    NpcWorld& world,
                    NpcEntity owner,
                    NpcEntity target,
                    NpcRole role,
                    WorldPoint origin,
                    WorldPoint aim,
                    float damage);
    void Update(float dt) override;
    void Draw(Renderer& renderer) override;

private:

    NpcWorld& m_World;
    NpcEntity m_Owner;
    NpcEntity m_Target;
    NpcRole m_Role;
    WorldPoint m_Direction;
    float m_Damage;
    float m_Remaining;
};
