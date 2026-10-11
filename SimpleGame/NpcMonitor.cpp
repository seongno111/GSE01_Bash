#include "stdafx.h"
#include "NpcWorld.h"
#include "Prototype.h"
#include <Windows.h>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <locale>
#include <sstream>
#include <initializer_list>
#include <utility>

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
    const auto villageAt = [this](WorldPoint p)
    {
        return InVillage(p) ? S("cathedral") : "null";
    };
    std::vector<std::string> npcs;
    int npcAlive = 0;
    int npcCombat = 0;
    int residents = 0;
    int residentAlive = 0;
    int present = 0;
    int presentCombat = 0;
    int presentFrenzy = 0;
    int residentGold = 0;
    double residentHealth = 0;
    double residentCorruption = 0;
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
        npcAlive += v.alive;
        npcCombat += active && combat;
        if (InVillage(npc->home))
        {
            ++residents;
            if (v.alive)
            {
                ++residentAlive;
                residentGold += v.gold;
                residentHealth += v.maxHealth > 0 ? v.health / v.maxHealth : 0;
                residentCorruption += v.corruption;
            }
        }
        if (active && InVillage(npc->WorldPosition()))
        {
            ++present;
            presentCombat += combat;
            presentFrenzy += v.frenzy;
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
        npcs.push_back(O({{"id", S(std::to_string(npc->Entity()))},
                          {"name", S(npc->profile.name)},
                          {"role", S(NpcRoleName(npc->profile.role))},
                          {"goal", S(NpcGoalName(npc->profile.goal))},
                          {"homeVillageId", villageAt(npc->home)},
                          {"currentVillageId", villageAt(npc->WorldPosition())},
                          {"position", Position(npc->WorldPosition())},
                          {"home", Position(npc->home)},
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
    int villageMobs = 0;
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
        villageMobs += active && InVillage(actor.WorldPosition());
        const auto location = villageAt(actor.WorldPosition());
        monsters.push_back(O({{"id", S("actor-" + std::to_string(actor.Id()))},
                              {"kind", S(kind)},
                              {"name", S(name)},
                              {"alive", B(alive)},
                              {"active", B(active)},
                              {"enabled", B(actor.EnabledInHierarchy())},
                              {"health", N(health)},
                              {"maxHealth", N(maxHealth)},
                              {"position", Position(actor.WorldPosition())},
                              {"currentVillageId", location}}));
    };
    // Enumerate current collections, including later spawns and mutation remnants.
    for (const auto& enemy : m_Game.m_Enemies)
    {
        addMonster(enemy,
                   enemy.tier == 2 ? "boss" : "monster",
                   enemy.tier == 2   ? "경작지 수문장"
                   : enemy.tier == 1 ? "오염된 약탈자"
                                     : "메마른 망자",
                   enemy.alive,
                   enemy.health,
                   enemy.maxHealth);
    }
    for (const auto& echo : m_Game.m_Echoes)
    {
        echoActive += echo.alive && echo.EnabledInHierarchy();
        addMonster(echo, "echo", "변이 잔재", echo.alive, echo.health, echo.maxHealth);
    }
    const auto village =
        O({{"id", S("cathedral")},
           {"name", S("성당 마을")},
           {"bounds", O({{"minX", N(-10)}, {"maxX", N(3)}, {"minY", N(-6)}, {"maxY", N(10)}})},
           {"status", S(presentCombat || presentFrenzy || villageMobs ? "위험" : "평온")},
           {"residents", N(residents)},
           {"aliveResidents", N(residentAlive)},
           {"deadResidents", N(residents - residentAlive)},
           {"presentNpcs", N(present)},
           {"combatNpcs", N(presentCombat)},
           {"frenzyNpcs", N(presentFrenzy)},
           {"activeMonsters", N(villageMobs)},
           {"residentGold", N(residentGold)},
           {"averageHealthRatio", residentAlive ? N(residentHealth / residentAlive) : "null"},
           {"averageCorruption", residentAlive ? N(residentCorruption / residentAlive) : "null"},
           {"treasury", N(m_Treasury)},
           {"food", N(m_MarketFood)},
           {"medicine", N(m_MarketMedicine)},
           {"goods", N(m_MarketGoods)}});
    std::vector<std::string> events;
    for (const auto& event : m_Events)
    {
        events.push_back(S(event));
    }
    const auto json = O({{"schemaVersion", N(1)},
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
                         {"villages", A({village})},
                         {"npcs", A(npcs)},
                         {"monsters", A(monsters)},
                         {"events", A(events)}});
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
