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

bool NpcWorld::ExportMonitor()
{
    if (m_LogDirectory.empty())
    {
        return false;
    }
    const auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::system_clock::now().time_since_epoch())
                               .count();
    if (m_MonitorSession.empty())
    {
        m_MonitorSession = std::to_string(timestamp) + "-" + std::to_string(GetCurrentProcessId());
    }
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
    for (const auto id : m_Actors)
    {
        const auto* npc = dynamic_cast<const NpcActor*>(m_Game.Scene().Find(id));
        if (!npc)
        {
            continue;
        }
        const auto& v = npc->vitals;
        const auto& run = npc->execution;
        const bool combat = npc->mode == NpcMode::Combat || npc->mode == NpcMode::Evacuate;
        const bool active = v.alive && npc->EnabledInHierarchy();
        const auto foundConduct = m_Conduct.find(id);
        const auto conduct = foundConduct == m_Conduct.end() ? Conduct{} : foundConduct->second;
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
        const int location = VillageAt(npc->WorldPosition());
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
            O({{"id", S(std::to_string(npc->Entity()))},
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
               {"currentVillageId", villageAt(npc->WorldPosition())},
               {"position", Position(npc->WorldPosition())},
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
    const auto addMonster = [&](const Actor& actor,
                                const std::string& kind,
                                const std::string& name,
                                bool alive,
                                float health,
                                float maxHealth)
    {
        const bool active = alive && actor.EnabledInHierarchy();
        mobActive += active;
        mobDead += !alive && actor.EnabledInHierarchy();
        mobInactive += !actor.EnabledInHierarchy();
        const int village = VillageAt(actor.WorldPosition());
        if (active && village >= 0)
        {
            ++villageStats[village].villageMobs;
        }
        const auto location = villageAt(actor.WorldPosition());
        const auto* enemy = dynamic_cast<const EnemyActor*>(&actor);
        monsters.push_back(O(
            {{"id", S("actor-" + std::to_string(actor.Id()))},
             {"kind", S(kind)},
             {"name", S(name)},
             {"speciesId", enemy ? S(enemy->Definition().id) : S("mutation_echo")},
             {"family", enemy ? S(enemy->Definition().family) : S("죽음의 잔재")},
             {"modelKey", enemy ? S(enemy->Definition().modelKey) : S("DeathEcho")},
             {"ability", enemy ? S(MonsterAbilityName(enemy->Definition().ability)) : S("없음")},
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
             {"armor", enemy ? N(enemy->Definition().armor) : "null"},
             {"moveSpeed", enemy ? N(enemy->speed * (enemy->enraged ? 1.3f : 1)) : "null"},
             {"experience", enemy ? N(enemy->experience) : "null"},
             {"attackRange", enemy ? N(enemy->Definition().attackRange) : "null"},
             {"specialCooldown", enemy ? N(enemy->specialCooldown) : "null"},
             {"specialWindup", enemy ? N(enemy->specialWindup) : "null"},
             {"guardRemaining", enemy ? N(enemy->guardRemaining) : "null"},
             {"enraged", B(enemy && enemy->enraged)},
             {"packNearby", enemy ? N(enemy->packNearby) : "null"},
             {"alive", B(alive)},
             {"active", B(active)},
             {"enabled", B(actor.EnabledInHierarchy())},
             {"health", N(health)},
             {"maxHealth", N(maxHealth)},
             {"position", Position(actor.WorldPosition())},
             {"outpostId",
              enemy && enemy->outpost >= 0 ? S(Outposts[size_t(enemy->outpost)].id) : "null"},
             {"raidVillageId",
              enemy && enemy->raidVillage >= 0 ? S(Villages[size_t(enemy->raidVillage)].id)
                                               : "null"},
             {"destination", enemy && enemy->outpost >= 0 ? Position(enemy->routeTarget) : "null"},
             {"currentVillageId", location}}));
    };
    // Enumerate current collections, including later spawns and mutation remnants.
    for (const auto& enemy : m_Game.m_Enemies)
    {
        addMonster(enemy,
                   enemy.Definition().boss ? "boss" : "monster",
                   enemy.Definition().name,
                   enemy.alive,
                   enemy.health,
                   enemy.maxHealth);
    }
    for (const auto& echo : m_Game.m_Echoes)
    {
        echoActive += echo.alive && echo.EnabledInHierarchy();
        addMonster(echo, "echo", "변이 잔재", echo.alive, echo.health, echo.maxHealth);
    }
    std::vector<std::string> structuredEvents;
    for (const auto& event : m_WorldEvents)
    {
        const int village = VillageAt(event.position);
        if (m_Time - event.time <= 60 && village >= 0)
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
        const auto& society = m_Societies[i];
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
        const auto& economy = m_Economies[i];
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
                    B(m_WorldEvents.empty() || m_WorldEvents.front().id == 1
                      || m_WorldEvents.front().time <= m_Time - 60)}})},
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
    for (const auto& event : m_Events)
    {
        events.push_back(S(event));
    }
    std::vector<std::string> eventCounts;
    for (const auto& count : m_EventCounts)
    {
        eventCounts.push_back(O({{"type", S(count.first)}, {"count", N(double(count.second))}}));
    }
    std::vector<std::string> outposts;
    for (size_t i = 0; i < m_Outposts.size(); ++i)
    {
        const auto& camp = m_Outposts[i];
        int alive = 0;
        int raiding = 0;
        int garrison = 0;
        int target = -1;
        for (auto id : camp.members)
        {
            const auto* enemy = dynamic_cast<const EnemyActor*>(m_Game.Scene().Find(id));
            if (enemy && enemy->alive)
            {
                ++alive;
                if (enemy->raidVillage >= 0)
                {
                    ++raiding;
                    target = enemy->raidVillage;
                }
                else if (!enemy->returningToOutpost
                         && std::hypot(enemy->WorldX() - Outposts[i].center.x,
                                       enemy->WorldY() - Outposts[i].center.y)
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
    for (const auto& post : m_CarrierPosts)
    {
        const auto* bird = m_Game.Scene().Find(post.bird);
        carrierPosts.push_back(O(
            {{"id", S(std::to_string(post.id))},
             {"sourceVillageId", S(Villages[post.source].id)},
             {"destinationVillageId", S(Villages[post.destination].id)},
             {"departedAt", N(post.departedAt)},
             {"arrivesAt", N(post.arrivesAt)},
             {"speed", N(GameRules::CarrierPigeonSpeed)},
             {"reports", N(double(post.reports.size()))},
             {"position", Position(bird ? bird->WorldPosition() : Villages[post.source].center)}}));
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
           {"sessionId", S(m_MonitorSession)},
           {"sequence", N(double(++m_MonitorSequence))},
           {"generatedAtUnixMs", std::to_string(timestamp)},
           {"simulationTime", N(m_Time)},
           {"worldSeed", S(std::to_string(m_Seed))},
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
                m_WorldEvents.empty() ? "null" : S(std::to_string(m_WorldEvents.front().id))},
               {"nextId", S(std::to_string(m_NextWorldEvent))}})}});
    std::error_code error;
    std::filesystem::create_directories(m_LogDirectory, error);
    const auto temporary = m_LogDirectory / "monitor.json.tmp";
    std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
    file << json;
    file.close();
    const bool success = !error && bool(file)
                         && MoveFileExW(temporary.c_str(),
                                        (m_LogDirectory / "monitor.json").c_str(),
                                        MOVEFILE_REPLACE_EXISTING);
    if (!success && !m_MonitorWarning)
    {
        std::cerr << "[NpcMonitor] Snapshot write failed: " << m_LogDirectory << '\n';
    }
    m_MonitorWarning = !success;
    return success;
}
