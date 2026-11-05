#include "stdafx.h"
#include "NpcWorld.h"
#include "Prototype.h"
#include "Outposts.h"
#include <Windows.h>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <locale>
#include <sstream>
#include <initializer_list>
#include <utility>
#include <algorithm>
#include <exception>

void NpcWorld::RecordWorldEvent(const std::string& type,
                                const std::string& text,
                                WorldPoint position,
                                const Actor* actor,
                                const Actor* subject,
                                std::uint64_t sourceEvent)
{
    WorldEvent event;
    event.id = m_NextWorldEvent++;
    event.time = m_Time;
    event.type = type;
    event.text = text;
    event.position = position;
    event.sourceEvent = sourceEvent;
    const auto name = [this](const Actor& value)
    {
        if (value.Id() == m_Game.Player().Id())
        {
            return std::string("플레이어");
        }
        if (const auto* enemy = dynamic_cast<const EnemyActor*>(&value))
        {
            return std::string(enemy->Definition().name);
        }
        if (dynamic_cast<const EchoActor*>(&value))
        {
            return std::string("변이 잔재");
        }
        return value.Name();
    };
    if (actor)
    {
        event.actor = actor->Id();
        event.actorName = name(*actor);
        if (type == "murder" || type == "raid")
        {
            ++m_Conduct[actor->Id()].violence;
        }
        if (type == "heal" || type == "blessing" || type == "hunt")
        {
            ++m_Conduct[actor->Id()].care;
        }
    }
    if (subject)
    {
        event.subject = subject->Id();
        event.subjectName = name(*subject);
    }
    if (type != "rumor")
    {
        for (const auto id : NearbyNpcs(position, 8.5))
        {
            const auto* npc = dynamic_cast<const NpcActor*>(m_Game.Scene().Find(id));
            if (!npc || !npc->vitals.alive || !npc->EnabledInHierarchy())
            {
                continue;
            }
            const double dx = npc->WorldX() - position.x;
            const double dy = npc->WorldY() - position.y;
            if (id == event.actor
                || (std::sqrt(dx * dx + dy * dy) <= 8
                    && m_Game.ClearShot(npc->WorldX(), npc->WorldY(), position.x, position.y)))
            {
                event.knowers.push_back(id);
            }
        }
    }
    ++m_EventCounts[type];
    m_WorldEvents.push_back(event);
    while (m_WorldEvents.size() > 2048)
    {
        m_WorldEvents.pop_front();
    }
    m_Log.push_back("t=" + std::to_string(m_Time) + " eventId=" + std::to_string(event.id)
                    + " type=" + type + " actor=" + std::to_string(event.actor)
                    + " subject=" + std::to_string(event.subject) + " text=" + text);
    while (m_Log.size() > 512)
    {
        m_Log.pop_front();
    }
}

void NpcWorld::SpreadRumors()
{
    // Facts retain their provenance. Hearing a rumor does not grant sight or hostility.
    const size_t batch = (std::min)(size_t(48), m_Actors.size());
    const size_t first = m_RumorCursor;
    m_RumorCursor = m_Actors.empty() ? 0 : (m_RumorCursor + batch) % m_Actors.size();
    for (size_t index = 0; index < batch; ++index)
    {
        const ActorId id = m_Actors[(first + index) % m_Actors.size()];
        auto* speaker = dynamic_cast<NpcActor*>(m_Game.Scene().Find(id));
        if (!speaker || !speaker->vitals.alive || !speaker->EnabledInHierarchy()
            || speaker->mode != NpcMode::Daily)
        {
            continue;
        }
        const int location = VillageAt(speaker->WorldPosition());
        if (location >= 0)
        {
            ExchangeVillageKnowledge(*speaker, size_t(location));
        }
        bool shared = false;
        size_t examined = 0;
        for (auto it = m_WorldEvents.rbegin(); it != m_WorldEvents.rend() && examined < 64;
             ++it, ++examined)
        {
            if (it->type == "rumor" || m_Time - it->time > 120
                || std::find(it->knowers.begin(), it->knowers.end(), id) == it->knowers.end())
            {
                continue;
            }
            for (const auto recipient : NearbyNpcs(speaker->WorldPosition(), 3))
            {
                const auto* listener =
                    dynamic_cast<const NpcActor*>(m_Game.Scene().Find(recipient));
                if (!listener || recipient == id || !listener->vitals.alive
                    || !listener->EnabledInHierarchy() || listener->mode != NpcMode::Daily
                    || std::find(it->knowers.begin(), it->knowers.end(), recipient)
                           != it->knowers.end())
                {
                    continue;
                }
                const double dx = speaker->WorldX() - listener->WorldX();
                const double dy = speaker->WorldY() - listener->WorldY();
                if (std::sqrt(dx * dx + dy * dy) > 2.5 || !Visible(*speaker, listener->Entity()))
                {
                    continue;
                }
                const auto sourceId = it->id;
                const auto text =
                    speaker->profile.name + " → " + listener->profile.name + ": " + it->text;
                it->knowers.push_back(recipient);
                Event("소문: " + text);
                RecordWorldEvent("rumor",
                                 text,
                                 listener->WorldPosition(),
                                 speaker,
                                 listener,
                                 sourceId);
                shared = true;
                break;
            }
            if (shared)
            {
                // Appending can invalidate deque iterators; never advance after recording.
                break;
            }
        }
    }
}

namespace
{
    // Small JSON encoder for read-only telemetry; all strings pass through escaping.
    std::string S(const std::string& value)
    {
        std::string result = "\"";
        constexpr char Hex[] = "0123456789abcdef";
        for (const unsigned char c : value)
        {
            if (c == '"' || c == '\\')
            {
                result += '\\';
                result += char(c);
            }
            else if (c < 0x20)
            {
                result += "\\u00";
                result += Hex[c >> 4];
                result += Hex[c & 15];
            }
            else
            {
                result += char(c);
            }
        }
        return result + '"';
    }

    std::string N(double value)
    {
        if (!std::isfinite(value))
        {
            return "null";
        }
        std::ostringstream out;
        out.imbue(std::locale::classic());
        out << std::setprecision(12) << value;
        return out.str();
    }

    std::string B(bool value)
    {
        return value ? "true" : "false";
    }

    std::string O(std::initializer_list<std::pair<std::string, std::string>> fields)
    {
        std::string result = "{";
        for (const auto& field : fields)
        {
            if (result.size() > 1)
            {
                result += ',';
            }
            result += S(field.first) + ':' + field.second;
        }
        return result + '}';
    }

    std::string A(const std::vector<std::string>& values)
    {
        std::string result = "[";
        for (const auto& value : values)
        {
            if (result.size() > 1)
            {
                result += ',';
            }
            result += value;
        }
        return result + ']';
    }

    std::string Position(WorldPoint p)
    {
        return O({{"x", N(p.x)}, {"y", N(p.y)}});
    }
} // namespace

// Values owned by the export job. The worker never reads actors or the scene graph.
struct NpcWorld::MonitorSnapshot
{
    struct Npc
    {
        ActorId id = InvalidActor;
        NpcEntity entity = NoNpcEntity;
        WorldPoint position;
        bool active = false;
        NpcProfile profile;
        NpcVitals vitals;
        NpcBirthStats birth{};
        NpcMission mission;
        NpcExecution execution;
        WorldPoint home;
        WorldPoint migrationHome;
        NpcMode mode = NpcMode::Daily;
        double nextMigration = 0;
        unsigned migrations = 0;
        std::string migrationReason;
        bool distant = false;
        double stationaryTime = 0;
        double distanceTravelled = 0;
        std::uint64_t completedActions = 0;
        std::uint64_t failedActions = 0;
        std::uint64_t decisions = 0;
        std::string lastResult;
        std::vector<NpcCandidate> candidates;
        // Diagnostic details are copied only when latest.log is requested.
        std::vector<NpcMemory> knowledge;
        std::map<NpcEntity, NpcRelation> relations;
        std::array<double, NpcActionCount> cooldown{};
        std::uint64_t randomState = 0;
    };

    struct Monster
    {
        ActorId id = InvalidActor;
        WorldPoint position;
        bool enabled = false;
        bool echo = false;
        bool alive = false;
        float health = 0;
        float maxHealth = 0;
        MonsterKind kind = MonsterKind::WitheredHollow;
        ActorId combatTarget = InvalidActor;
        MonsterState state = MonsterState::Patrol;
        float attackWindup = 0;
        float recovery = 0;
        float damage = 0;
        float speed = 0;
        int experience = 0;
        float specialCooldown = 0;
        float specialWindup = 0;
        float guardRemaining = 0;
        bool enraged = false;
        int packNearby = 0;
        int outpost = -1;
        int raidVillage = -1;
        WorldPoint routeTarget;
        bool returningHome = false;
    };

    struct Carrier
    {
        std::uint64_t id = 0;
        size_t source = 0;
        size_t destination = 0;
        double departedAt = 0;
        double arrivesAt = 0;
        size_t reports = 0;
        size_t events = 0;
        WorldPoint position;
    };

    std::vector<Npc> npcs;
    std::vector<Monster> monsters;
    std::vector<Carrier> carrierPosts;
    std::array<VillageSociety, Villages.size()> societies;
    std::array<VillageEconomy, Villages.size()> economies;
    std::vector<EnemyOutpost> outposts;
    std::deque<WorldEvent> worldEvents;
    std::map<std::string, std::uint64_t> eventCounts;
    std::map<ActorId, Conduct> conduct;
    std::deque<std::string> events;
    std::deque<std::string> log;
    std::filesystem::path directory;
    std::string session;
    std::uint64_t sequence = 0;
    std::uint64_t nextWorldEvent = 0;
    std::uint64_t seed = 0;
    double time = 0;
    long long timestamp = 0;
    size_t spatialBuckets = 0;
    bool diagnosticLog = false;
};

NpcWorld::~NpcWorld()
{
    // A job owns all serialized state, and joins before world members are destroyed.
    if (m_MonitorExport.valid())
    {
        m_MonitorExport.wait();
    }
}

std::shared_ptr<NpcWorld::MonitorSnapshot> NpcWorld::CaptureMonitorSnapshot()
{
    auto snapshot = std::make_shared<MonitorSnapshot>();
    snapshot->timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                              std::chrono::system_clock::now().time_since_epoch())
                              .count();
    if (m_MonitorSession.empty())
    {
        m_MonitorSession =
            std::to_string(snapshot->timestamp) + "-" + std::to_string(GetCurrentProcessId());
    }
    snapshot->session = m_MonitorSession;
    snapshot->sequence = ++m_MonitorSequence;
    snapshot->time = m_Time;
    snapshot->seed = m_Seed;
    snapshot->nextWorldEvent = m_NextWorldEvent;
    snapshot->directory = m_LogDirectory;
    snapshot->societies = m_Societies;
    snapshot->economies = m_Economies;
    snapshot->outposts = m_Outposts;
    snapshot->worldEvents = m_WorldEvents;
    snapshot->eventCounts = m_EventCounts;
    snapshot->conduct = m_Conduct;
    snapshot->events = m_Events;
    snapshot->diagnosticLog = m_DiagnosticLogRequested;
    if (snapshot->diagnosticLog)
    {
        snapshot->log = m_Log;
        snapshot->spatialBuckets = m_NpcSpatialIndex.size();
    }
    snapshot->npcs.reserve(m_Actors.size());
    for (const auto id : m_Actors)
    {
        const auto* npc = dynamic_cast<const NpcActor*>(m_Game.Scene().Find(id));
        if (!npc)
        {
            continue;
        }
        MonitorSnapshot::Npc record;
        record.id = id;
        record.entity = npc->Entity();
        record.position = npc->WorldPosition();
        record.active = npc->EnabledInHierarchy();
        record.profile = npc->profile;
        record.vitals = npc->vitals;
        record.birth = npc->birth;
        record.mission = npc->mission;
        record.execution = npc->execution;
        record.home = npc->home;
        record.migrationHome = npc->migrationHome;
        record.mode = npc->mode;
        record.nextMigration = npc->nextMigration;
        record.migrations = npc->migrations;
        record.migrationReason = npc->migrationReason;
        record.distant = npc->distant;
        record.stationaryTime = npc->stationaryTime;
        record.distanceTravelled = npc->distanceTravelled;
        record.completedActions = npc->completedActions;
        record.failedActions = npc->failedActions;
        record.decisions = npc->decisions;
        record.lastResult = npc->lastResult;
        record.candidates = npc->candidates;
        if (snapshot->diagnosticLog)
        {
            record.knowledge = npc->knowledge;
            record.relations = npc->relations;
            record.cooldown = npc->cooldown;
            record.randomState = npc->randomState;
        }
        snapshot->npcs.push_back(std::move(record));
    }
    for (const auto& enemy : m_Game.m_Enemies)
    {
        MonitorSnapshot::Monster record;
        record.id = enemy.Id();
        record.position = enemy.WorldPosition();
        record.enabled = enemy.EnabledInHierarchy();
        record.alive = enemy.alive;
        record.health = enemy.health;
        record.maxHealth = enemy.maxHealth;
        record.kind = enemy.kind;
        record.combatTarget = enemy.combatTarget;
        record.state = enemy.state;
        record.attackWindup = enemy.attackWindup;
        record.recovery = enemy.recovery;
        record.damage = enemy.damage;
        record.speed = enemy.speed;
        record.experience = enemy.experience;
        record.specialCooldown = enemy.specialCooldown;
        record.specialWindup = enemy.specialWindup;
        record.guardRemaining = enemy.guardRemaining;
        record.enraged = enemy.enraged;
        record.packNearby = enemy.packNearby;
        record.outpost = enemy.outpost;
        record.raidVillage = enemy.raidVillage;
        record.routeTarget = enemy.routeTarget;
        record.returningHome = enemy.returningHome;
        snapshot->monsters.push_back(std::move(record));
    }
    for (const auto& echo : m_Game.m_Echoes)
    {
        MonitorSnapshot::Monster record;
        record.id = echo.Id();
        record.position = echo.WorldPosition();
        record.enabled = echo.EnabledInHierarchy();
        record.echo = true;
        record.alive = echo.alive;
        record.health = echo.health;
        record.maxHealth = echo.maxHealth;
        snapshot->monsters.push_back(std::move(record));
    }
    for (const auto& post : m_CarrierPosts)
    {
        const auto* bird = m_Game.Scene().Find(post.bird);
        snapshot->carrierPosts.push_back(
            {post.id,
             post.source,
             post.destination,
             post.departedAt,
             post.arrivesAt,
             post.reports.size(),
             post.events.size(),
             bird ? bird->WorldPosition() : Villages[post.source].center});
    }
    return snapshot;
}

void NpcWorld::PollMonitorExport()
{
    if (m_MonitorExport.valid()
        && m_MonitorExport.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
    {
        try
        {
            const auto result = m_MonitorExport.get();
            m_LastMonitorWorkerMs = result.workerMs;
            if (!result.monitorWritten && !m_MonitorWarning)
            {
                std::cerr << "[NpcMonitor] Snapshot write failed: " << m_LogDirectory << '\n';
            }
            if (!result.logWritten && !m_LogWarning)
            {
                std::cerr << "[NpcAI] Diagnostic snapshot write failed.\n";
            }
            m_MonitorWarning = !result.monitorWritten;
            m_LogWarning = !result.logWritten;
            if (result.monitorWritten)
            {
                m_LastMonitorCompletedAt = m_MonitorCapturedAt;
            }
        }
        catch (const std::exception& error)
        {
            std::cerr << "[NpcMonitor] Export job failed: " << error.what() << '\n';
        }
    }
    m_Performance.monitorWorkerMs = m_LastMonitorWorkerMs;
    m_Performance.monitorAgeMs =
        m_LastMonitorCompletedAt == std::chrono::steady_clock::time_point{}
            ? -1
            : std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now()
                                                        - m_LastMonitorCompletedAt)
                  .count();
    m_Performance.monitorPending = m_MonitorExport.valid();
    m_Performance.monitorSkipped = m_MonitorSkipped;
}

bool NpcWorld::ExportMonitor()
{
    if (m_LogDirectory.empty())
    {
        return false;
    }
    PollMonitorExport();
    if (m_MonitorExport.valid())
    {
        // Bound the exporter to one job. A later tick captures the newest world.
        ++m_MonitorSkipped;
        m_Performance.monitorSkipped = m_MonitorSkipped;
        return true;
    }

    const auto start = std::chrono::steady_clock::now();
    try
    {
        auto snapshot = CaptureMonitorSnapshot();
        m_MonitorExport = std::async(std::launch::async,
                                     [snapshot]()
                                     {
                                         const auto start = std::chrono::steady_clock::now();
                                         auto result = WriteMonitorSnapshot(*snapshot);
                                         result.workerMs =
                                             std::chrono::duration<double, std::milli>(
                                                 std::chrono::steady_clock::now() - start)
                                                 .count();
                                         return result;
                                     });
        m_MonitorCapturedAt = start;
        m_DiagnosticLogRequested = false;
        m_Performance.monitorPending = true;
    }
    catch (const std::exception& error)
    {
        std::cerr << "[NpcMonitor] Cannot start export job: " << error.what() << '\n';
        return false;
    }
    m_Performance.monitorCaptureMs +=
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    return true;
}

bool NpcWorld::ExportLog()
{
    m_DiagnosticLogRequested = true;
    return ExportMonitor();
}

NpcWorld::MonitorExportResult NpcWorld::WriteMonitorSnapshot(const MonitorSnapshot& snapshot)
{
    const auto villageAt = [](WorldPoint p)
    {
        const int index = VillageAt(p);
        return index >= 0 ? S(Villages[index].id) : "null";
    };

    struct VillageStats
    {
        int residents = 0;
        int residentAlive = 0;
        int present = 0;
        int presentCombat = 0;
        int presentFrenzy = 0;
        int residentGold = 0;
        double residentHealth = 0;
        double residentCorruption = 0;
        int goodPresent = 0;
        int neutralPresent = 0;
        int harmfulPresent = 0;
        double presentAlignment = 0;
        double presentMorale = 0;
        int villageMobs = 0;
        int recentMurders = 0;
        int recentRaids = 0;
        int recentDeaths = 0;
        int recentHunts = 0;
        int recentRumors = 0;
        int migratingOut = 0;
        int migratingIn = 0;
    };

    std::array<VillageStats, Villages.size()> villageStats{};
    std::vector<std::string> npcs;
    int npcAlive = 0;
    int npcCombat = 0;
    for (const auto& record : snapshot.npcs)
    {
        const auto* npc = &record;
        const auto id = record.id;
        const auto& v = npc->vitals;
        const auto& run = npc->execution;
        const bool combat = npc->mode == NpcMode::Combat || npc->mode == NpcMode::Evacuate;
        const bool active = v.alive && npc->active;
        const auto foundConduct = snapshot.conduct.find(id);
        const auto conduct =
            foundConduct == snapshot.conduct.end() ? Conduct{} : foundConduct->second;
        const auto& p = npc->profile;
        const double disposition = std::clamp(
            100.0 * (0.55 * p.compassion + 0.45 * p.duty - 0.45 * p.greed - 0.65 * p.raidWeight)
                + (std::min)(20, conduct.care * 4) - (std::min)(50, conduct.violence * 10),
            -100.0,
            100.0);
        const char* alignment = disposition >= 30    ? "선한 성향"
                                : disposition <= -30 ? "악한 성향"
                                                     : "중립 성향";
        npcAlive += v.alive;
        npcCombat += active && combat;
        auto& homeStats = villageStats[p.village];
        ++homeStats.residents;
        if (v.alive && npc->mission.type == VillageMission::Relocate)
        {
            ++homeStats.migratingOut;
            ++villageStats[npc->mission.destination].migratingIn;
        }
        if (v.alive)
        {
            ++homeStats.residentAlive;
            homeStats.residentGold += v.gold;
            homeStats.residentHealth += v.maxHealth > 0 ? v.health / v.maxHealth : 0;
            homeStats.residentCorruption += v.corruption;
        }
        const int location = VillageAt(npc->position);
        if (active && location >= 0)
        {
            auto& town = villageStats[location];
            ++town.present;
            town.presentCombat += combat;
            town.presentFrenzy += v.frenzy;
            town.goodPresent += disposition >= 30;
            town.harmfulPresent += disposition <= -30;
            town.neutralPresent += disposition > -30 && disposition < 30;
            town.presentAlignment += disposition;
            town.presentMorale += v.morale;
        }
        std::vector<std::string> candidates;
        for (const auto& c : npc->candidates)
        {
            candidates.push_back(O({{"id", S(NpcActionId(c.action))},
                                    {"name", S(NpcActivityName(npc->profile.role, c.action))},
                                    {"allowed", B(c.allowed)},
                                    {"reason", S(c.reason)},
                                    {"probability", N(c.probability)},
                                    {"weight", N(c.weight)},
                                    {"target", S(std::to_string(c.target))}}));
        }
        const auto& mission = npc->mission;
        const auto& birth = npc->birth;
        npcs.push_back(
            O({{"id", S(std::to_string(npc->entity))},
               {"name", S(npc->profile.name)},
               {"role", S(NpcRoleName(npc->profile.role))},
               {"goal", S(NpcGoalName(npc->profile.goal))},
               {"actorId", S("actor-" + std::to_string(id))},
               {"alignment",
                O({{"label", S(alignment)},
                   {"score", N(disposition)},
                   {"careEvents", N(conduct.care)},
                   {"violentEvents", N(conduct.violence)}})},
               {"personality",
                O({{"compassion", N(p.compassion)},
                   {"duty", N(p.duty)},
                   {"greed", N(p.greed)},
                   {"raidWeight", N(p.raidWeight)},
                   {"courage", N(p.courage)},
                   {"faith", N(p.faith)}})},
               {"homeVillageId", S(Villages[p.village].id)},
               {"currentVillageId", villageAt(npc->position)},
               {"position", Position(npc->position)},
               {"home", Position(npc->home)},
               {"migration",
                O({{"active", B(npc->mission.type == VillageMission::Relocate)},
                   {"completed", N(npc->migrations)},
                   {"reason", S(npc->migrationReason)},
                   {"reservedHome", Position(npc->migrationHome)},
                   {"nextReviewAfter", N(npc->nextMigration)}})},
               {"alive", B(v.alive)},
               {"active", B(active)},
               {"distant", B(npc->distant)},
               {"mode", S(NpcModeName(npc->mode))},
               {"health", N(v.health)},
               {"maxHealth", N(v.maxHealth)},
               {"gold", N(v.gold)},
               {"corruption", N(v.corruption)},
               {"frenzy", B(v.frenzy)},
               {"holyPower", N(v.holyPower)},
               {"strength", N(v.strength)},
               {"wisdom", N(v.wisdom)},
               {"fatigue", N(v.fatigue)},
               {"hunger", N(v.hunger)},
               {"morale", N(v.morale)},
               {"food", N(v.food)},
               {"medicine", N(v.medicine)},
               {"goods", N(v.goods)},
               {"weapon", S(NpcWeaponFor(npc->profile.role).name)},
               {"armor", N(v.armor)},
               {"moveSpeed", N(v.moveSpeed)},
               {"birth",
                O({{"health", N(birth.health)},
                   {"strength", N(birth.strength)},
                   {"holyPower", N(birth.holyPower)},
                   {"wisdom", N(birth.wisdom)},
                   {"armor", N(birth.armor)},
                   {"speed", N(birth.speed)},
                   {"gold", N(birth.gold)}})},
               {"mission",
                O({{"name", S(VillageMissionName(mission.type))},
                   {"active", B(mission.type != VillageMission::None)},
                   {"villageId",
                    mission.type == VillageMission::None ? "null"
                                                         : S(Villages[mission.destination].id)},
                   {"stage",
                    S(mission.returning ? "귀환"
                      : mission.arrived ? "현지 활동"
                                        : "이동")},
                   {"deadline", N(mission.deadline)}})},
               {"action", S(NpcActivityName(npc->profile.role, run.action))},
               {"actionId", S(NpcActionId(run.action))},
               {"phase", S(NpcPhaseName(run.phase))},
               {"target", S(std::to_string(run.target))},
               {"destination", Position(run.destination)},
               {"lastResult", S(npc->lastResult)},
               {"stationarySeconds", N(npc->stationaryTime)},
               {"distanceTravelled", N(npc->distanceTravelled)},
               {"completed", N(double(npc->completedActions))},
               {"failed", N(double(npc->failedActions))},
               {"decisions", N(double(npc->decisions))},
               {"candidates", A(candidates)}}));
    }
    std::vector<std::string> monsters;
    int mobActive = 0;
    int mobDead = 0;
    int mobInactive = 0;
    int echoActive = 0;
    const auto addMonster = [&](const MonitorSnapshot::Monster& actor)
    {
        const auto* enemy = actor.echo ? nullptr : &actor;
        const auto& stats = MonsterStats(actor.kind);
        const std::string kind = actor.echo ? "echo" : stats.boss ? "boss" : "monster";
        const std::string name = actor.echo ? "변이 잔재" : stats.name;
        const bool alive = actor.alive;
        const float health = actor.health;
        const float maxHealth = actor.maxHealth;
        const bool active = alive && actor.enabled;
        mobActive += active;
        mobDead += !alive && actor.enabled;
        mobInactive += !actor.enabled;
        const int village = VillageAt(actor.position);
        if (active && village >= 0)
        {
            ++villageStats[village].villageMobs;
        }
        const auto location = villageAt(actor.position);
        monsters.push_back(O(
            {{"id", S("actor-" + std::to_string(actor.id))},
             {"kind", S(kind)},
             {"name", S(name)},
             {"speciesId", enemy ? S(MonsterStats(enemy->kind).id) : S("mutation_echo")},
             {"family", enemy ? S(MonsterStats(enemy->kind).family) : S("죽음의 잔재")},
             {"modelKey", enemy ? S(MonsterStats(enemy->kind).modelKey) : S("DeathEcho")},
             {"ability",
              enemy ? S(MonsterAbilityName(MonsterStats(enemy->kind).ability)) : S("없음")},
             {"combatTargetId",
              enemy && enemy->combatTarget != InvalidActor
                  ? S("actor-" + std::to_string(enemy->combatTarget))
                  : "null"},
             {"state",
              enemy ? S(!alive                    ? "사망"
                        : !active                 ? "비활성"
                        : enemy->attackWindup > 0 ? "공격 예고"
                        : enemy->recovery > 0     ? "공격 회복"
                                                  : MonsterStateName(enemy->state))
                    : S("추격")},
             {"damage", enemy ? N(enemy->damage) : "null"},
             {"armor", enemy ? N(MonsterStats(enemy->kind).armor) : "null"},
             {"moveSpeed", enemy ? N(enemy->speed * (enemy->enraged ? 1.3f : 1)) : "null"},
             {"experience", enemy ? N(enemy->experience) : "null"},
             {"attackRange", enemy ? N(MonsterStats(enemy->kind).attackRange) : "null"},
             {"specialCooldown", enemy ? N(enemy->specialCooldown) : "null"},
             {"specialWindup", enemy ? N(enemy->specialWindup) : "null"},
             {"guardRemaining", enemy ? N(enemy->guardRemaining) : "null"},
             {"enraged", B(enemy && enemy->enraged)},
             {"packNearby", enemy ? N(enemy->packNearby) : "null"},
             {"alive", B(alive)},
             {"active", B(active)},
             {"enabled", B(actor.enabled)},
             {"health", N(health)},
             {"maxHealth", N(maxHealth)},
             {"position", Position(actor.position)},
             {"outpostId",
              enemy && enemy->outpost >= 0 ? S(Outposts[size_t(enemy->outpost)].id) : "null"},
             {"raidVillageId",
              enemy && enemy->raidVillage >= 0 ? S(Villages[size_t(enemy->raidVillage)].id)
                                               : "null"},
             {"destination", enemy && enemy->outpost >= 0 ? Position(enemy->routeTarget) : "null"},
             {"currentVillageId", location}}));
    };
    for (const auto& monster : snapshot.monsters)
    {
        echoActive += monster.echo && monster.alive && monster.enabled;
        addMonster(monster);
    }
    std::vector<std::string> structuredEvents;
    for (const auto& event : snapshot.worldEvents)
    {
        const int village = VillageAt(event.position);
        if (snapshot.time - event.time <= 60 && village >= 0)
        {
            auto& town = villageStats[village];
            town.recentMurders += event.type == "murder";
            town.recentRaids += event.type == "raid";
            town.recentDeaths += event.type == "death" || event.type == "kill";
            town.recentHunts += event.type == "hunt";
            town.recentRumors += event.type == "rumor" || event.type == "village_rumor"
                                 || event.type == "pigeon_rumor";
        }
        std::vector<std::string> knowers;
        for (const auto actorId : event.knowers)
        {
            knowers.push_back(S("actor-" + std::to_string(actorId)));
        }
        structuredEvents.push_back(
            O({{"id", S(std::to_string(event.id))},
               {"time", N(event.time)},
               {"type", S(event.type)},
               {"text", S(event.text)},
               {"position", Position(event.position)},
               {"villageId", villageAt(event.position)},
               {"actorId", event.actor ? S("actor-" + std::to_string(event.actor)) : "null"},
               {"actorName", S(event.actorName)},
               {"subjectId", event.subject ? S("actor-" + std::to_string(event.subject)) : "null"},
               {"subjectName", S(event.subjectName)},
               {"sourceEventId", event.sourceEvent ? S(std::to_string(event.sourceEvent)) : "null"},
               {"knowers", A(knowers)}}));
    }
    std::vector<std::string> villages;
    for (size_t i = 0; i < Villages.size(); ++i)
    {
        std::vector<std::string> knowledge;
        const auto& society = snapshot.societies[i];
        for (const auto& entry : society.known)
        {
            if (entry.first == i)
            {
                continue;
            }
            const auto& report = entry.second;
            const float relation = society.relations[entry.first];
            knowledge.push_back(
                O({{"villageId", S(Villages[entry.first].id)},
                   {"position", Position(report.position)},
                   {"alignment", report.observedAt >= 0 ? N(report.alignment * 100) : "null"},
                   {"observedAt", N(report.observedAt)},
                   {"sourceVillageId", S(Villages[report.sourceVillage].id)},
                   {"threatened", B(report.threatened)},
                   {"geographicRisk", N(report.geographicRisk)},
                   {"relation", N(relation)},
                   {"stance",
                    S(relation >= 20    ? "우호"
                      : relation <= -20 ? "경계"
                                        : "중립")}}));
        }
        const auto& definition = Villages[i];
        const auto& town = villageStats[i];
        const auto& economy = snapshot.economies[i];
        const double morale = town.present ? town.presentMorale / town.present : 0;
        const char* atmosphere =
            town.villageMobs || town.presentCombat || town.presentFrenzy ? "전투와 위협"
            : town.recentMurders || town.recentDeaths                    ? "불안과 애도"
            : town.recentRaids || (town.present && morale < 0.4)         ? "긴장과 공포"
            : !town.present                                              ? "적막"
            : town.harmfulPresent > town.goodPresent                     ? "약탈과 불신"
            : town.goodPresent > town.harmfulPresent && town.goodPresent * 2 >= town.present
                ? "신뢰와 안정"
                : "다양한 성향이 공존";
        villages.push_back(
            O({{"id", S(definition.id)},
               {"name", S(definition.name)},
               {"culture", S(definition.culture)},
               {"knowledge", A(knowledge)},
               {"expeditions", N(society.dispatches)},
               {"nextDispatch", N(society.nextDispatch)},
               {"geographicRisk", N(society.known.at(i).geographicRisk)},
               {"postsSent", N(society.postsSent)},
               {"postsReceived", N(society.postsReceived)},
               {"immigrants", N(society.immigrants)},
               {"emigrants", N(society.emigrants)},
               {"migratingIn", N(town.migratingIn)},
               {"migratingOut", N(town.migratingOut)},
               {"center", Position(definition.center)},
               {"bounds",
                O({{"minX", N(definition.center.x - GameRules::VillageHalfExtent)},
                   {"maxX", N(definition.center.x + GameRules::VillageHalfExtent)},
                   {"minY", N(definition.center.y - GameRules::VillageHalfExtent)},
                   {"maxY", N(definition.center.y + GameRules::VillageHalfExtent)}})},
               {"status",
                S(town.presentCombat || town.presentFrenzy || town.villageMobs ? "위험" : "평온")},
               {"atmosphere", S(atmosphere)},
               {"alignment",
                O({{"good", N(town.goodPresent)},
                   {"neutral", N(town.neutralPresent)},
                   {"evil", N(town.harmfulPresent)},
                   {"averageScore",
                    town.present ? N(town.presentAlignment / town.present) : "null"}})},
               {"averageMorale", town.present ? N(morale) : "null"},
               {"recentEvents",
                O({{"windowSeconds", N(60)},
                   {"murder", N(town.recentMurders)},
                   {"raid", N(town.recentRaids)},
                   {"death", N(town.recentDeaths)},
                   {"hunt", N(town.recentHunts)},
                   {"rumor", N(town.recentRumors)},
                   {"windowComplete",
                    B(snapshot.worldEvents.empty() || snapshot.worldEvents.front().id == 1
                      || snapshot.worldEvents.front().time <= snapshot.time - 60)}})},
               {"residents", N(town.residents)},
               {"aliveResidents", N(town.residentAlive)},
               {"deadResidents", N(town.residents - town.residentAlive)},
               {"presentNpcs", N(town.present)},
               {"combatNpcs", N(town.presentCombat)},
               {"frenzyNpcs", N(town.presentFrenzy)},
               {"activeMonsters", N(town.villageMobs)},
               {"residentGold", N(town.residentGold)},
               {"averageHealthRatio",
                town.residentAlive ? N(town.residentHealth / town.residentAlive) : "null"},
               {"averageCorruption",
                town.residentAlive ? N(town.residentCorruption / town.residentAlive) : "null"},
               {"treasury", N(economy.treasury)},
               {"food", N(economy.food)},
               {"medicine", N(economy.medicine)},
               {"goods", N(economy.goods)}}));
    }
    std::vector<std::string> events;
    for (const auto& event : snapshot.events)
    {
        events.push_back(S(event));
    }
    std::vector<std::string> eventCounts;
    for (const auto& count : snapshot.eventCounts)
    {
        eventCounts.push_back(O({{"type", S(count.first)}, {"count", N(double(count.second))}}));
    }
    std::vector<std::string> outposts;
    for (size_t i = 0; i < snapshot.outposts.size(); ++i)
    {
        const auto& camp = snapshot.outposts[i];
        int alive = 0;
        int raiding = 0;
        int garrison = 0;
        int target = -1;
        for (auto id : camp.members)
        {
            const auto found = std::find_if(snapshot.monsters.begin(),
                                            snapshot.monsters.end(),
                                            [id](const auto& monster)
                                            {
                                                return monster.id == id && !monster.echo;
                                            });
            const auto* enemy = found == snapshot.monsters.end() ? nullptr : &*found;
            if (enemy && enemy->alive)
            {
                ++alive;
                if (enemy->raidVillage >= 0)
                {
                    ++raiding;
                    target = enemy->raidVillage;
                }
                else if (!enemy->returningHome
                         && std::hypot(enemy->position.x - Outposts[i].center.x,
                                       enemy->position.y - Outposts[i].center.y)
                                < OutpostGarrisonRadius)
                {
                    ++garrison;
                }
            }
        }
        outposts.push_back(
            O({{"id", S(Outposts[i].id)},
               {"name", S(Outposts[i].name)},
               {"position", Position(Outposts[i].center)},
               {"alive", N(alive)},
               {"garrison", N(garrison)},
               {"raiding", N(raiding)},
               {"capacity", N(OutpostCapacity)},
               {"halfExtent", N(OutpostHalfExtent)},
               {"garrisonRadius", N(OutpostGarrisonRadius)},
               {"waveSize", N(OutpostWaveSize)},
               {"threshold", N(OutpostWaveThreshold)},
               {"radius", N(camp.radius)},
               {"waves", N(camp.waves)},
               {"spawned", N(camp.spawned)},
               {"nextSpawn", N(camp.nextSpawn)},
               {"nextWave", N(camp.nextWave)},
               {"targetVillageId", target < 0 ? "null" : S(Villages[size_t(target)].id)}}));
    }
    std::vector<std::string> carrierPosts;
    for (const auto& post : snapshot.carrierPosts)
    {
        carrierPosts.push_back(O({{"id", S(std::to_string(post.id))},
                                  {"sourceVillageId", S(Villages[post.source].id)},
                                  {"destinationVillageId", S(Villages[post.destination].id)},
                                  {"departedAt", N(post.departedAt)},
                                  {"arrivesAt", N(post.arrivesAt)},
                                  {"speed", N(GameRules::CarrierPigeonSpeed)},
                                  {"reports", N(double(post.reports))},
                                  {"position", Position(post.position)}}));
    }
    std::vector<std::string> routes;
    for (size_t from = 0; from < Villages.size(); ++from)
    {
        for (size_t to = from + 1; to < Villages.size(); ++to)
        {
            const auto a = Villages[from].center;
            const auto b = Villages[to].center;
            if (std::abs(a.x - b.x) + std::abs(a.y - b.y) != VillageSpacing)
            {
                continue;
            }
            routes.push_back(
                O({{"fromVillageId", S(Villages[from].id)},
                   {"toVillageId", S(Villages[to].id)},
                   {"gateDistance", N(GameRules::VillageRoadGap)},
                   {"walkingSeconds", N(GameRules::VillageRoadGap / GameRules::PlayerWalkSpeed)}}));
        }
    }
    const auto json =
        O({{"schemaVersion", N(4)},
           {"type", S("world.snapshot")},
           {"sessionId", S(snapshot.session)},
           {"sequence", N(double(snapshot.sequence))},
           {"generatedAtUnixMs", std::to_string(snapshot.timestamp)},
           {"simulationTime", N(snapshot.time)},
           {"worldSeed", S(std::to_string(snapshot.seed))},
           {"scope", S("현재 등록된 Actor")},
           {"totals",
            O({{"npcs", N(double(npcs.size()))},
               {"aliveNpcs", N(npcAlive)},
               {"deadNpcs", N(double(npcs.size()) - npcAlive)},
               {"combatNpcs", N(npcCombat)},
               {"monsters", N(double(monsters.size()))},
               {"activeMonsters", N(mobActive)},
               {"deadMonsters", N(mobDead)},
               {"inactiveMonsters", N(mobInactive)},
               {"activeEchoes", N(echoActive)}})},
           {"villages", A(villages)},
           {"npcs", A(npcs)},
           {"monsters", A(monsters)},
           {"outposts", A(outposts)},
           {"carrierPosts", A(carrierPosts)},
           {"villageRoutes", A(routes)},
           {"settlementRules",
            O({{"initialPopulationPerVillage", N(GameRules::VillageInitialPopulation)},
               {"migrationFloor", N(GameRules::VillageMigrationFloor)},
               {"populationCapacity", N(GameRules::VillagePopulationCapacity)},
               {"halfExtent", N(GameRules::VillageHalfExtent)},
               {"centerSpacing", N(VillageSpacing)},
               {"roadGap", N(GameRules::VillageRoadGap)},
               {"walkSpeed", N(GameRules::PlayerWalkSpeed)}})},
           {"events", A(structuredEvents)},
           {"recentMessages", A(events)},
           {"eventCounts", A(eventCounts)},
           {"eventRetention",
            O({{"capacity", N(2048)},
               {"firstId",
                snapshot.worldEvents.empty() ? "null"
                                             : S(std::to_string(snapshot.worldEvents.front().id))},
               {"nextId", S(std::to_string(snapshot.nextWorldEvent))}})}});

    MonitorExportResult result;
    std::error_code error;
    std::filesystem::create_directories(snapshot.directory, error);
    const auto temporary = snapshot.directory / "monitor.json.tmp";
    std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
    file << json;
    file.close();
    result.monitorWritten = !error && bool(file)
                            && MoveFileExW(temporary.c_str(),
                                           (snapshot.directory / "monitor.json").c_str(),
                                           MOVEFILE_REPLACE_EXISTING);
    if (snapshot.diagnosticLog)
    {
        result.logWritten = WriteDiagnosticSnapshot(snapshot);
    }
    return result;
}

bool NpcWorld::WriteDiagnosticSnapshot(const MonitorSnapshot& snapshot)
{
    std::error_code error;
    std::filesystem::create_directories(snapshot.directory, error);
    const auto temporary = snapshot.directory / "latest.log.tmp";
    std::ofstream file(temporary, std::ios::trunc);
    if (error || !file)
    {
        return false;
    }
    file << "NPC AI schema=4 seed=" << snapshot.seed << " fixed_step=0.05 time=" << snapshot.time
         << " villages=" << Villages.size()
         << "\nBounded diagnostic snapshot; not a loadable save.\n";
    file << "WORLD_SCALE villageHalfExtent=" << GameRules::VillageHalfExtent
         << " initialResidents=" << GameRules::VillageInitialPopulation
         << " totalNpcs=" << snapshot.npcs.size() << " roadGap=" << GameRules::VillageRoadGap
         << " spacing=" << GameRules::ShrineSpacing
         << " walkingSeconds=" << GameRules::VillageRoadGap / GameRules::PlayerWalkSpeed
         << " pigeonSpeed=" << GameRules::CarrierPigeonSpeed
         << " migrationFloor=" << GameRules::VillageMigrationFloor
         << " populationCapacity=" << GameRules::VillagePopulationCapacity
         << " spatialBuckets=" << snapshot.spatialBuckets << '\n';
    for (size_t i = 0; i < Villages.size(); ++i)
    {
        const auto& economy = snapshot.economies[i];
        file << "VILLAGE " << Villages[i].id << " name=" << Villages[i].name
             << " culture=" << Villages[i].culture << " center=" << Villages[i].center.x << ','
             << Villages[i].center.y << " treasury=" << economy.treasury << " food=" << economy.food
             << " medicine=" << economy.medicine << " goods=" << economy.goods << '\n';
    }
    for (const auto& record : snapshot.npcs)
    {
        const auto* n = &record;
        file << "NPC " << n->profile.id << ' ' << n->profile.name
             << " village=" << Villages[n->profile.village].id
             << " role=" << NpcRoleName(n->profile.role) << " goal=" << NpcGoalName(n->profile.goal)
             << " position=" << n->position.x << ',' << n->position.y << " hp=" << n->vitals.health
             << '/' << n->vitals.maxHealth << " gold=" << n->vitals.gold
             << " holy=" << n->vitals.holyPower << " strength=" << n->vitals.strength
             << " armor=" << n->vitals.armor << " moveSpeed=" << n->vitals.moveSpeed
             << " birthHp=" << n->birth.health << " birthStrength=" << n->birth.strength
             << " mission=" << VillageMissionName(n->mission.type)
             << " missionVillage=" << Villages[n->mission.destination].id
             << " returning=" << n->mission.returning << " missionDeadline=" << n->mission.deadline
             << " wisdom=" << n->vitals.wisdom << " corruption=" << n->vitals.corruption
             << " raidWeight=" << n->profile.raidWeight
             << " corruptionWeight=" << n->profile.corruptionWeight << " rng=" << n->randomState
             << " seq=" << n->decisions << " alive=" << n->vitals.alive
             << " phase=" << NpcPhaseName(n->execution.phase) << " effect=" << n->execution.applied
             << " reservation=" << n->execution.reservation << " distant=" << n->distant
             << " weapon=" << NpcWeaponFor(n->profile.role).name
             << " stationary=" << n->stationaryTime << " distance=" << n->distanceTravelled
             << " migrations=" << n->migrations << " migrationReason=" << n->migrationReason
             << " migrationHome=" << n->migrationHome.x << ',' << n->migrationHome.y
             << " completed=" << n->completedActions << " failed=" << n->failedActions
             << " destination=" << n->execution.destination.x << ',' << n->execution.destination.y
             << '\n';
        for (const auto& memory : n->knowledge)
        {
            file << "  knowledge " << memory.entity << " at=" << memory.seenAt
                 << " source=" << memory.source << " last=" << memory.position.x << ','
                 << memory.position.y << " hostile=" << memory.hostile << '\n';
        }
        for (const auto& relation : n->relations)
        {
            file << "  relation " << relation.first << " trust=" << relation.second.trust
                 << " grudge=" << relation.second.grudge << '\n';
        }
        for (size_t i = 0; i < NpcActionCount; ++i)
        {
            if (n->cooldown[i] > snapshot.time)
            {
                file << "  cooldown " << NpcActionId(static_cast<NpcAction>(i)) << '='
                     << n->cooldown[i] - snapshot.time << '\n';
            }
        }
    }
    for (size_t village = 0; village < snapshot.societies.size(); ++village)
    {
        const auto& society = snapshot.societies[village];
        file << "SOCIETY village=" << Villages[village].id << " alignment=" << society.alignment
             << " threatened=" << society.threatened << " dispatches=" << society.dispatches
             << " nextDispatch=" << society.nextDispatch << " postsSent=" << society.postsSent
             << " postsReceived=" << society.postsReceived << " immigrants=" << society.immigrants
             << " emigrants=" << society.emigrants << '\n';
        for (const auto& item : society.known)
        {
            file << "  villageKnowledge target=" << Villages[item.first].id
                 << " observedAt=" << item.second.observedAt
                 << " alignment=" << item.second.alignment << " position=" << item.second.position.x
                 << ',' << item.second.position.y
                 << " source=" << Villages[item.second.sourceVillage].id
                 << " geographicRisk=" << item.second.geographicRisk
                 << " relation=" << society.relations[item.first] << '\n';
        }
    }
    for (size_t i = 0; i < snapshot.outposts.size(); ++i)
    {
        const auto& camp = snapshot.outposts[i];
        file << "OUTPOST id=" << Outposts[i].id << " radius=" << camp.radius
             << " spawned=" << camp.spawned << " waves=" << camp.waves
             << " nextSpawn=" << camp.nextSpawn << " nextWave=" << camp.nextWave << '\n';
    }
    for (const auto& line : snapshot.log)
    {
        file << line << '\n';
    }
    for (const auto& post : snapshot.carrierPosts)
    {
        file << "PIGEON id=" << post.id << " from=" << Villages[post.source].id
             << " to=" << Villages[post.destination].id << " departed=" << post.departedAt
             << " arrives=" << post.arrivesAt << " reports=" << post.reports
             << " events=" << post.events << '\n';
    }
    file.close();
    return bool(file)
           && MoveFileExW(temporary.c_str(),
                          (snapshot.directory / "latest.log").c_str(),
                          MOVEFILE_REPLACE_EXISTING);
}
