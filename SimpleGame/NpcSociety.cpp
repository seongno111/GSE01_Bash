#include "stdafx.h"
#include "NpcWorld.h"
#include "Prototype.h"
#include "GameRules.h"
#include <algorithm>
#include <cmath>

using namespace GameRules;

const char* VillageMissionName(VillageMission mission)
{
    switch (mission)
    {
    case VillageMission::Courier:
        return "교역·소문 사절";
    case VillageMission::Blessing:
        return "이웃 마을 축복";
    case VillageMission::Support:
        return "마을 방위 지원";
    case VillageMission::Raid:
        return "마을 약탈 원정";
    case VillageMission::Relocate:
        return "새 마을로 이사";
    default:
        return "마을 일상";
    }
}

void NpcWorld::VillageIncident(size_t from, size_t to, float amount)
{
    if (from == to || from >= Villages.size() || to >= Villages.size())
    {
        return;
    }
    m_Societies[to].incidents[from] =
        std::clamp(m_Societies[to].incidents[from] + amount, -80.0f, 40.0f);
    m_Societies[from].incidents[to] =
        std::clamp(m_Societies[from].incidents[to] + amount * .5f, -80.0f, 40.0f);
}

void NpcWorld::InitializeSociety()
{
    for (size_t village = 0; village < Villages.size(); ++village)
    {
        auto& society = m_Societies[village];
        society.nextDispatch = 12 + village * 5;
        society.nextPost = 3 + village * 2;
        // Road signs reveal nearby locations, but not an unseen community's character.
        std::vector<std::pair<double, size_t>> neighbors;
        for (size_t other = 0; other < Villages.size(); ++other)
        {
            if (other != village)
            {
                neighbors.push_back({Length(Villages[village].center.x - Villages[other].center.x,
                                            Villages[village].center.y - Villages[other].center.y),
                                     other});
            }
        }
        std::sort(neighbors.begin(), neighbors.end());
        for (size_t i = 0; i < 2; ++i)
        {
            const size_t other = neighbors[i].second;
            society.known[other] = {Villages[other].center, 0, -1, village, false};
        }
    }
    UpdateSociety();
}

void NpcWorld::UpdateSociety()
{
    for (size_t village = 0; village < Villages.size(); ++village)
    {
        auto& society = m_Societies[village];
        float sum = 0;
        int residents = 0;
        society.threatened = false;
        for (auto id : m_Actors)
        {
            const auto* npc = dynamic_cast<const NpcActor*>(m_Game.Scene().Find(id));
            if (!npc || !npc->vitals.alive)
            {
                continue;
            }
            if (npc->profile.village == village)
            {
                const auto& p = npc->profile;
                sum += .55f * p.compassion + .45f * p.duty - .45f * p.greed - .65f * p.raidWeight;
                ++residents;
            }
            if (VillageAt(npc->WorldPosition()) == int(village)
                && (npc->mode == NpcMode::Combat || npc->vitals.frenzy))
            {
                society.threatened = true;
            }
        }
        for (auto id : m_Enemies)
        {
            const auto* enemy = dynamic_cast<const EnemyActor*>(m_Game.Scene().Find(id));
            if (enemy && enemy->alive && VillageAt(enemy->WorldPosition()) == int(village))
            {
                society.threatened = true;
            }
        }
        society.alignment = residents ? std::clamp(sum / residents, -1.0f, 1.0f) : 0;
        auto& own = society.known[village];
        if (own.observedAt < 0 || m_Time - own.observedAt >= 30
            || own.threatened != society.threatened)
        {
            own = {Villages[village].center,
                   society.alignment,
                   m_Time,
                   village,
                   society.threatened,
                   GeographicRisk(village)};
        }
        for (const auto& entry : society.known)
        {
            const size_t other = entry.first;
            const auto& knowledge = entry.second;
            const float affinity = knowledge.observedAt >= 0
                                       ? 35 - 65 * std::abs(society.alignment - knowledge.alignment)
                                       : 0;
            society.relations[other] =
                std::clamp(affinity + society.incidents[other], -100.0f, 100.0f);
        }
        if (m_Time < society.nextDispatch)
        {
            continue;
        }
        society.nextDispatch = m_Time + 45 + village * 2;
        if (residents < 4 || society.threatened)
        {
            continue;
        }
        std::vector<std::pair<double, size_t>> destinations;
        for (const auto& entry : society.known)
        {
            if (entry.first == village)
            {
                continue;
            }
            if (society.alignment >= -.3f && entry.second.observedAt >= 0
                && society.relations[entry.first] <= -20)
            {
                continue;
            }
            const auto p = entry.second.position;
            destinations.push_back(
                {Length(p.x - Villages[village].center.x, p.y - Villages[village].center.y),
                 entry.first});
        }
        std::sort(destinations.begin(), destinations.end());
        if (destinations.empty())
        {
            continue;
        }
        size_t destination =
            destinations[society.dispatches % (std::min)(size_t(3), destinations.size())].second;
        if (society.alignment >= .3f)
        {
            for (const auto& entry : destinations)
            {
                const auto& report = society.known.at(entry.second);
                if (report.threatened && report.observedAt >= 0 && m_Time - report.observedAt < 90
                    && society.relations[entry.second] >= 20)
                {
                    destination = entry.second;
                    break;
                }
            }
        }
        VillageMission mission = VillageMission::Courier;
        if (society.alignment < -.3f)
        {
            mission = VillageMission::Raid;
        }
        else if (society.alignment >= .3f && society.relations[destination] >= 20
                 && society.known[destination].observedAt >= 0)
        {
            const auto& report = society.known[destination];
            mission =
                (report.threatened && m_Time - report.observedAt < 90) || society.dispatches % 2
                    ? VillageMission::Support
                    : VillageMission::Blessing;
        }
        std::vector<NpcActor*> eligible;
        int away = 0;
        for (auto id : m_Actors)
        {
            auto* npc = dynamic_cast<NpcActor*>(m_Game.Scene().Find(id));
            if (!npc || !npc->vitals.alive || npc->profile.village != village)
            {
                continue;
            }
            away += npc->mission.type != VillageMission::None;
            if (npc->mission.type == VillageMission::None && npc->mode == NpcMode::Daily
                && !npc->vitals.frenzy && npc->vitals.health > npc->vitals.maxHealth * .7f
                && npc->vitals.fatigue < .65f && VillageAt(npc->WorldPosition()) == int(village)
                && !(npc->execution.playerRequest && npc->execution.phase != NpcPhase::Complete))
            {
                eligible.push_back(npc);
            }
        }
        if (mission == VillageMission::Blessing
            && std::none_of(eligible.begin(),
                            eligible.end(),
                            [](const NpcActor* npc)
                            {
                                return npc->profile.role == NpcRole::Priest
                                       && npc->vitals.holyPower >= 8;
                            }))
        {
            mission = VillageMission::Support;
        }
        const auto suitability = [mission](const NpcActor* npc)
        {
            const auto role = npc->profile.role;
            if (mission == VillageMission::Blessing)
            {
                return role == NpcRole::Priest ? 3 : role == NpcRole::Knight ? 2 : 0;
            }
            if (mission == VillageMission::Support)
            {
                return role == NpcRole::Knight ? 3 : role == NpcRole::Adventurer ? 2 : 0;
            }
            if (mission == VillageMission::Raid)
            {
                return role == NpcRole::Raider ? 3 : 0;
            }
            return role == NpcRole::Merchant ? 3 : role == NpcRole::Adventurer ? 2 : 1;
        };
        std::stable_sort(eligible.begin(),
                         eligible.end(),
                         [&](const auto* a, const auto* b)
                         {
                             return suitability(a) > suitability(b);
                         });
        const int count =
            (std::min)(mission == VillageMission::Courier ? 1 : 2, residents / 2 - away);
        int sent = 0;
        for (auto* npc : eligible)
        {
            if (sent >= count || suitability(npc) == 0)
            {
                break;
            }
            if (npc->execution.phase != NpcPhase::Complete)
            {
                Finish(*npc, false, "마을 원정 임무 인계");
            }
            Release(*npc);
            npc->execution = {};
            npc->mission = {mission,
                            destination,
                            false,
                            false,
                            m_Time
                                + JourneyBudget(npc->WorldPosition(),
                                                Villages[destination].center,
                                                npc->vitals.moveSpeed),
                            0,
                            0,
                            0};
            npc->villageKnowledge = society.known;
            npc->lastResult =
                std::string(VillageMissionName(mission)) + ": " + Villages[destination].name;
            npc->nextPath = 0;
            RecordWorldEvent("expedition", npc->lastResult, npc->WorldPosition(), npc);
            ++sent;
        }
        if (sent)
        {
            ++society.dispatches;
        }
    }
}

void NpcWorld::ExchangeVillageKnowledge(NpcActor& npc, size_t village)
{
    if (VillageAt(npc.WorldPosition()) != int(village))
    {
        return;
    }
    auto& society = m_Societies[village];
    // A visitor observes this settlement. Origin and observation time survive retransmission.
    npc.villageKnowledge[village] = society.known.at(village);
    NpcActor* contact = nullptr;
    for (auto id : NearbyNpcs(npc.WorldPosition(), 3.5))
    {
        auto* other = dynamic_cast<NpcActor*>(m_Game.Scene().Find(id));
        if (other && other != &npc && other->vitals.alive && other->profile.village == village
            && !Hostile(npc, other->Entity()) && !Hostile(*other, npc.Entity())
            && Length(npc.WorldX() - other->WorldX(), npc.WorldY() - other->WorldY()) < 3
            && Visible(npc, other->Entity()))
        {
            contact = other;
            break;
        }
    }
    if (!contact)
    {
        return;
    }
    const auto merge = [](auto& destination, const auto& source)
    {
        bool changed = false;
        for (const auto& item : source)
        {
            const auto found = destination.find(item.first);
            if (found == destination.end() || item.second.observedAt > found->second.observedAt)
            {
                destination[item.first] = item.second;
                changed = true;
            }
        }
        return changed;
    };
    const bool learned = merge(npc.villageKnowledge, society.known);
    const bool delivered = merge(society.known, npc.villageKnowledge);
    merge(contact->villageKnowledge, npc.villageKnowledge);
    if ((learned || delivered) && npc.profile.village != village)
    {
        RecordWorldEvent("village_rumor",
                         npc.profile.name + "와 " + contact->profile.name
                             + "가 마을 위치·성향·위험 소식을 교환했다",
                         npc.WorldPosition(),
                         &npc,
                         contact);
    }
    else if (delivered)
    {
        RecordWorldEvent("village_rumor",
                         npc.profile.name + "가 주변 마을의 관측 소식을 고향에 전달했다",
                         npc.WorldPosition(),
                         &npc,
                         contact);
    }
}

bool NpcWorld::UpdateMission(NpcActor& npc, float dt)
{
    auto& mission = npc.mission;
    if (mission.type == VillageMission::Relocate)
    {
        return UpdateMigration(npc, dt);
    }
    if (mission.type == VillageMission::None)
    {
        return false;
    }
    if (!mission.returning
        && (m_Time > mission.deadline || npc.vitals.health < npc.vitals.maxHealth * .4f
            || npc.vitals.fatigue > .85f || npc.vitals.frenzy))
    {
        mission.returning = true;
        mission.arrived = false;
        mission.deadline =
            m_Time + JourneyBudget(npc.WorldPosition(), npc.home, npc.vitals.moveSpeed);
        npc.nextPath = 0;
    }
    if (mission.returning && m_Time > mission.deadline)
    {
        RecordWorldEvent("expedition_failed",
                         npc.profile.name + "의 귀환 경로 지연: 현지 생존 행동으로 전환",
                         npc.WorldPosition(),
                         &npc);
        mission = {};
        Finish(npc, false, "원정 종료: 귀환 시간 초과");
        return false;
    }
    if (npc.mode != NpcMode::Daily
        || (npc.execution.playerRequest && npc.execution.phase != NpcPhase::Complete))
    {
        return false;
    }
    if (npc.execution.phase != NpcPhase::Complete && npc.execution.action != NpcAction::Patrol)
    {
        return false;
    }
    if (mission.type == VillageMission::Raid && !mission.returning && !mission.arrived
        && npc.profile.role == NpcRole::Raider && mission.pathFailures == 0
        && m_Time >= mission.nextService && m_Time >= npc.cooldown[size_t(NpcAction::Raid)])
    {
        mission.nextService = m_Time + 2;
        for (const auto& memory : npc.knowledge)
        {
            const auto* victim = Npc(memory.entity);
            if (!victim || !victim->vitals.alive || victim->profile.village == npc.profile.village
                || InsideSanctuary(victim->WorldX(), victim->WorldY())
                || !Visible(npc, memory.entity))
            {
                continue;
            }
            int companions = 0;
            for (auto id : m_Actors)
            {
                const auto* other = dynamic_cast<const NpcActor*>(m_Game.Scene().Find(id));
                if (other && other != victim && other->vitals.alive
                    && other->profile.village == victim->profile.village
                    && Length(other->WorldX() - victim->WorldX(),
                              other->WorldY() - victim->WorldY())
                           < 4)
                {
                    ++companions;
                }
            }
            if (companions == 0)
            {
                NpcCandidate ambush;
                ambush.allowed = true;
                ambush.action = NpcAction::Raid;
                ambush.target = memory.entity;
                ambush.destination = victim->WorldPosition();
                if (Start(npc, ambush))
                {
                    return true;
                }
            }
        }
    }
    const size_t destination = mission.returning ? npc.profile.village : mission.destination;
    if (mission.arrived && VillageAt(npc.WorldPosition()) != int(destination))
    {
        mission.arrived = false;
        npc.nextPath = 0;
    }
    const auto center = Villages[destination].center;
    WorldPoint goal = mission.returning ? npc.home : WorldPoint{center.x - 4, center.y + 3};
    if (mission.type == VillageMission::Raid && !mission.returning)
    {
        goal = {center.x + 1, center.y + 6};
    }
    if (Length(npc.WorldX() - goal.x, npc.WorldY() - goal.y) > .8 && !mission.arrived)
    {
        if (npc.execution.phase == NpcPhase::Complete)
        {
            Release(npc);
            npc.execution = {};
            npc.execution.id = m_NextExecution++;
            npc.execution.startedAt = m_Time;
        }
        npc.execution.action = NpcAction::Patrol;
        npc.execution.phase = NpcPhase::Approach;
        npc.execution.destination = goal;
        npc.execution.target = NoNpcEntity;
        if (mission.pathFailures > 0 && m_Time < mission.nextService)
        {
            return true;
        }
        if (!Travel(npc, goal, dt))
        {
            ++mission.pathFailures;
            mission.nextService = m_Time + 2;
            npc.nextPath = 0;
            if (mission.pathFailures >= 4 && !mission.returning)
            {
                mission.returning = true;
                mission.deadline =
                    m_Time + JourneyBudget(npc.WorldPosition(), npc.home, npc.vitals.moveSpeed);
                mission.pathFailures = 0;
                RecordWorldEvent("expedition_failed",
                                 npc.profile.name + "의 원정 경로 차단, 귀환",
                                 npc.WorldPosition(),
                                 &npc);
            }
        }
        else
        {
            mission.pathFailures = 0;
        }
        return true;
    }
    npc.execution.phase = NpcPhase::Complete;
    if (mission.returning)
    {
        ExchangeVillageKnowledge(npc, destination);
        RecordWorldEvent("expedition_return",
                         npc.profile.name + "가 원정을 마치고 귀환했다",
                         npc.WorldPosition(),
                         &npc);
        mission = {};
        npc.path.clear();
        npc.nextDecision = m_Time;
        return true;
    }
    if (!mission.arrived)
    {
        mission.arrived = true;
        mission.serviceUntil = m_Time + 30;
        mission.nextService = m_Time;
        RecordWorldEvent("expedition_arrival",
                         npc.profile.name + "가 " + Villages[destination].name + "에 도착했다",
                         npc.WorldPosition(),
                         &npc);
    }
    if (m_Time >= mission.serviceUntil)
    {
        mission.returning = true;
        mission.arrived = false;
        mission.deadline =
            m_Time + JourneyBudget(npc.WorldPosition(), npc.home, npc.vitals.moveSpeed);
        mission.nextService = 0;
        npc.path.clear();
        npc.nextPath = 0;
        return true;
    }
    if (m_Time < mission.nextService)
    {
        return true;
    }
    mission.nextService = m_Time + 3;
    ExchangeVillageKnowledge(npc, destination);
    NpcCandidate choice;
    choice.allowed = true;
    choice.destination = npc.WorldPosition();
    if (mission.type == VillageMission::Courier && npc.profile.role == NpcRole::Merchant
        && VillageAt(npc.WorldPosition()) == int(destination)
        && (npc.vitals.goods > 0 || npc.vitals.food < 3 || npc.vitals.medicine < 2))
    {
        choice.action = NpcAction::Trade;
        if (Start(npc, choice))
        {
            return true;
        }
    }
    for (const auto& memory : npc.knowledge)
    {
        auto* other = Npc(memory.entity);
        if (!other || !other->vitals.alive || !Visible(npc, memory.entity))
        {
            continue;
        }
        if (mission.type == VillageMission::Raid && npc.profile.role == NpcRole::Raider
            && other->profile.village != npc.profile.village
            && m_Time >= npc.cooldown[size_t(NpcAction::Raid)]
            && !InsideSanctuary(other->WorldX(), other->WorldY()))
        {
            choice.action = NpcAction::Raid;
        }
        else if (mission.type == VillageMission::Blessing && npc.profile.role == NpcRole::Priest
                 && !Hostile(npc, memory.entity) && npc.vitals.holyPower >= 8
                 && m_Time >= npc.cooldown[size_t(NpcAction::Bless)]
                 && (other->vitals.health < other->vitals.maxHealth * .95f
                     || other->vitals.corruption > 5))
        {
            choice.action = NpcAction::Bless;
        }
        else
        {
            continue;
        }
        choice.target = memory.entity;
        choice.destination = other->WorldPosition();
        if (Start(npc, choice))
        {
            if (choice.action == NpcAction::Bless)
            {
                npc.lastResult = "이웃 마을 주민에게 축복을 준비한다";
            }
            return true;
        }
    }
    // Circulate among actual residents, so diplomats meet somebody and guards see threats.
    std::vector<NpcActor*> locals;
    for (auto id : m_Actors)
    {
        auto* other = dynamic_cast<NpcActor*>(m_Game.Scene().Find(id));
        if (other && other->vitals.alive && other->profile.village == destination
            && VillageAt(other->WorldPosition()) == int(destination))
        {
            locals.push_back(other);
        }
    }
    if (!locals.empty())
    {
        const auto* other = locals[(npc.profile.id + size_t(m_Time / 8)) % locals.size()];
        goal = other->WorldPosition();
    }
    choice.action = NpcAction::Patrol;
    choice.target = 0;
    choice.destination = goal;
    // A short local walk is handled by the ordinary action executor while visiting.
    if (Start(npc, choice))
    {
        npc.execution.action = NpcAction::Hunt;
    }
    return true;
}
