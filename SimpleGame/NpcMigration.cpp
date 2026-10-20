#include "stdafx.h"
#include "NpcWorld.h"
#include "Prototype.h"
#include "GameRules.h"
#include "Outposts.h"
#include <algorithm>
#include <cmath>

using namespace GameRules;

float NpcWorld::GeographicRisk(size_t village) const
{
    const auto center = Villages[village].center;
    double pressure = 0;
    // Camps are local geographical landmarks. Only delivered village reports
    // reveal a distant community's disposition; no unseen NPC positions are read.
    for (const auto& camp : Outposts)
    {
        const double gap = (std::max)(0.0,
                                      Length(camp.center.x - center.x, camp.center.y - center.y)
                                          - VillageHalfExtent - OutpostHalfExtent);
        pressure += 0.4 * std::exp(-gap / 55);
    }
    const auto& society = m_Societies[village];
    for (const auto& report : society.known)
    {
        if (report.first == village || report.second.observedAt < 0
            || m_Time - report.second.observedAt > 240 || society.relations[report.first] >= -20)
        {
            continue;
        }
        const auto other = report.second.position;
        const double gap =
            (std::max)(0.0, Length(other.x - center.x, other.y - center.y) - VillageHalfExtent * 2);
        pressure += 0.8 * std::exp(-gap / VillageRoadGap);
    }
    return float(std::clamp(pressure, 0.0, 1.5));
}

bool NpcWorld::FindMigrationHome(size_t village, WorldPoint& home) const
{
    const auto center = Villages[village].center;
    for (size_t attempt = 0; attempt < 289; ++attempt)
    {
        const size_t slot = (attempt * 67 + size_t(m_Time) + village * 19) % 289;
        const WorldPoint candidate{center.x - 16 + double(slot % 17) * 2,
                                   center.y - 16 + double(slot / 17) * 2};
        if (InsideSanctuary(candidate.x, candidate.y) || !Walkable(candidate.x, candidate.y))
        {
            continue;
        }
        const bool occupied =
            std::any_of(m_Actors.begin(),
                        m_Actors.end(),
                        [&](ActorId id)
                        {
                            const auto* npc = static_cast<const NpcActor*>(m_Game.Scene().Find(id));
                            if (!npc || !npc->vitals.alive)
                            {
                                return false;
                            }
                            const bool reserved = npc->mission.type == VillageMission::Relocate
                                                  && npc->mission.destination == village;
                            const auto place = reserved ? npc->migrationHome : npc->home;
                            return (reserved || npc->profile.village == village)
                                   && Length(place.x - candidate.x, place.y - candidate.y) < 1.5;
                        });
        if (!occupied)
        {
            home = candidate;
            return true;
        }
    }
    return false;
}

void NpcWorld::UpdateMigrationChoices()
{
    std::array<int, Villages.size()> population{};
    std::array<int, Villages.size()> leaving{};
    std::array<int, Villages.size()> arriving{};
    for (auto id : m_Actors)
    {
        const auto* npc = static_cast<const NpcActor*>(m_Game.Scene().Find(id));
        if (npc && npc->vitals.alive)
        {
            ++population[npc->profile.village];
            if (npc->mission.type == VillageMission::Relocate)
            {
                ++leaving[npc->profile.village];
                ++arriving[npc->mission.destination];
            }
        }
    }
    for (size_t origin = 0; origin < Villages.size(); ++origin)
    {
        if (population[origin] - leaving[origin] <= VillageMigrationFloor)
        {
            continue;
        }
        NpcActor* chosen = nullptr;
        size_t destination = origin;
        float bestImprovement = 4;
        std::string reason;
        for (auto id : m_Actors)
        {
            auto* npc = static_cast<NpcActor*>(m_Game.Scene().Find(id));
            if (!npc || !npc->vitals.alive || npc->profile.village != origin
                || npc->mission.type != VillageMission::None || npc->mode != NpcMode::Daily
                || npc->vitals.frenzy || m_Time < npc->nextMigration
                || npc->vitals.health < npc->vitals.maxHealth * .65f || npc->vitals.fatigue > .6f
                || VillageAt(npc->WorldPosition()) != int(origin)
                || (npc->execution.playerRequest && npc->execution.phase != NpcPhase::Complete))
            {
                continue;
            }
            const auto& p = npc->profile;
            const float character =
                .55f * p.compassion + .45f * p.duty - .45f * p.greed - .65f * p.raidWeight;
            const float safety = 50 * (1 - p.courage * .5f);
            const auto score = [&](const VillageKnowledge& report)
            {
                return 60 * (1 - std::abs(character - report.alignment))
                       - safety * (report.geographicRisk + (report.threatened ? .8f : 0));
            };
            const float current = score(m_Societies[origin].known.at(origin));
            // Residents read their own town's delivered reports, not another
            // town's live alignment. The original source/time survives forwarding.
            for (const auto& entry : m_Societies[origin].known)
            {
                const size_t target = entry.first;
                const auto& report = entry.second;
                if (target == origin || report.observedAt < 0 || m_Time - report.observedAt > 240
                    || report.threatened || std::abs(character - report.alignment) > .55f
                    || population[target] + arriving[target] >= VillagePopulationCapacity)
                {
                    continue;
                }
                const float improvement = score(report) - current;
                if (improvement > bestImprovement)
                {
                    chosen = npc;
                    destination = target;
                    bestImprovement = improvement;
                    reason = "전달된 마을 소문에 따른 성향 적합성·몬스터 거점·적대 마을 위험 비교: "
                             + std::to_string(int(current)) + " → "
                             + std::to_string(int(score(report)));
                }
            }
        }
        WorldPoint newHome;
        if (!chosen || !FindMigrationHome(destination, newHome))
        {
            continue;
        }
        if (chosen->execution.phase != NpcPhase::Complete)
        {
            Finish(*chosen, false, "새 거주지로 이사 준비");
        }
        Release(*chosen);
        chosen->execution = {};
        chosen->path.clear();
        chosen->nextPath = 0;
        chosen->migrationHome = newHome;
        chosen->migrationReason = reason;
        chosen->nextMigration = m_Time + 240 + chosen->profile.id % 60;
        chosen->villageKnowledge = m_Societies[origin].known;
        chosen->mission = {
            VillageMission::Relocate,
            destination,
            false,
            false,
            m_Time + JourneyBudget(chosen->WorldPosition(), newHome, chosen->vitals.moveSpeed),
            0,
            0,
            0};
        chosen->lastResult = std::string(Villages[destination].name) + "로 이사: " + reason;
        ++leaving[origin];
        ++arriving[destination];
        RecordWorldEvent("migration_departure",
                         chosen->lastResult,
                         chosen->WorldPosition(),
                         chosen);
    }
}

bool NpcWorld::UpdateMigration(NpcActor& npc, float dt)
{
    const auto cancel = [&](const std::string& reason)
    {
        RecordWorldEvent("migration_failed",
                         npc.profile.name + ": " + reason,
                         npc.WorldPosition(),
                         &npc);
        Release(npc);
        npc.mission = {};
        npc.execution = {};
        npc.path.clear();
        npc.nextDecision = m_Time;
        npc.nextMigration = m_Time + 120;
        npc.lastResult = reason;
    };
    if (m_Time >= npc.mission.deadline || npc.vitals.frenzy
        || npc.vitals.health < npc.vitals.maxHealth * .3f)
    {
        cancel("이사 중 위험 또는 이동 시간 초과로 보류");
        return false;
    }
    if (npc.mode != NpcMode::Daily
        || (npc.execution.phase != NpcPhase::Complete && npc.execution.action != NpcAction::Patrol))
    {
        return false;
    }
    const size_t target = npc.mission.destination;
    if (Length(npc.WorldX() - npc.migrationHome.x, npc.WorldY() - npc.migrationHome.y) > .8)
    {
        if (m_Time < npc.mission.nextService)
        {
            return true;
        }
        npc.execution.action = NpcAction::Patrol;
        npc.execution.phase = NpcPhase::Approach;
        npc.execution.destination = npc.migrationHome;
        if (!Travel(npc, npc.migrationHome, dt))
        {
            npc.nextPath = 0;
            npc.mission.nextService = m_Time + 2;
            if (++npc.mission.pathFailures >= 4)
            {
                cancel("이사 경로가 막혀 현지 생존 행동으로 전환");
                return false;
            }
        }
        else
        {
            npc.mission.pathFailures = 0;
        }
        return true;
    }
    if (!m_Game.Scene().Reparent(npc.Id(), m_VillageRoots[target], true))
    {
        cancel("새 마을의 거주지 등록 실패");
        return false;
    }
    const size_t origin = npc.profile.village;
    ++m_Societies[origin].emigrants;
    ++m_Societies[target].immigrants;
    ++npc.migrations;
    npc.profile.village = target;
    npc.home = npc.migrationHome;
    npc.activityAnchor = npc.WorldPosition();
    npc.stationaryTime = 0;
    npc.path.clear();
    Release(npc);
    npc.execution = {};
    npc.mission = {};
    npc.nextDecision = m_Time;
    npc.lastResult =
        std::string(Villages[origin].name) + " → " + Villages[target].name + " 이사 완료";
    ExchangeVillageKnowledge(npc, target);
    RecordWorldEvent("migration_arrival",
                     npc.profile.name + ": " + npc.lastResult,
                     npc.WorldPosition(),
                     &npc);
    return true;
}
