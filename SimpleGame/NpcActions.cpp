#include "stdafx.h"
#include "NpcWorld.h"
#include "Prototype.h"
#include "GameRules.h"
#include <algorithm>
#include <cmath>

using namespace GameRules;

bool NpcWorld::Walkable(double x, double y) const
{
    return !m_Game.Blocked(x - 0.2, y) && !m_Game.Blocked(x + 0.2, y) && !m_Game.Blocked(x, y - 0.2)
           && !m_Game.Blocked(x, y + 0.2);
}

bool NpcWorld::BuildPath(NpcActor& npc, WorldPoint target)
{
    ++m_Performance.pathCalls;
    npc.pathIndex = 0;
    npc.nextPath = m_Time + 2.5;
    return m_Game.FindRoute(npc.WorldPosition(), target, npc.path, m_Performance.pathNodes);
}

bool NpcWorld::Travel(NpcActor& npc, WorldPoint target, float dt)
{
    if (m_Time >= npc.nextPath || npc.pathIndex >= npc.path.size())
    {
        if (!BuildPath(npc, target))
        {
            return false;
        }
    }
    if (npc.pathIndex >= npc.path.size())
    {
        return true;
    }
    const auto point = npc.path[npc.pathIndex];
    const double dx = point.x - npc.WorldX();
    const double dy = point.y - npc.WorldY();
    const double distance = Length(dx, dy);
    if (distance < 0.18)
    {
        ++npc.pathIndex;
        return true;
    }
    const float speed =
        npc.vitals.moveSpeed * (npc.execution.action == NpcAction::Flee ? 1.4f : 1.0f);
    const double step = (std::min)(distance, double(speed * dt));
    const auto before = npc.WorldPosition();
    m_Game.MoveActor(npc, dx / distance * step, dy / distance * step, 0.2f);
    const double moved = Length(npc.WorldX() - before.x, npc.WorldY() - before.y);
    npc.distanceTravelled += moved;
    npc.stalledTime = moved < step * 0.1 ? npc.stalledTime + dt : 0;
    if (npc.stalledTime > 1.5)
    {
        npc.stalledTime = 0;
        return false;
    }
    npc.vitals.fatigue = (std::min)(1.0f, npc.vitals.fatigue + dt * 0.003f);
    return true;
}

bool NpcWorld::Start(NpcActor& npc, const NpcCandidate& choice, bool request)
{
    if (!choice.allowed || !npc.vitals.alive)
    {
        return false;
    }
    if (choice.target && !Visible(npc, choice.target) && choice.target != npc.Entity())
    {
        return false;
    }
    const auto resource = Resource(npc, choice.action, choice.target);
    if (!CanReserve(resource, npc.Entity()))
    {
        return false;
    }
    if (!BuildPath(npc, choice.destination))
    {
        npc.failedUntil[{choice.action, choice.target}] = m_Time + 3;
        return false;
    }
    Release(npc);
    npc.execution = {};
    npc.execution.action = choice.action;
    npc.execution.phase = NpcPhase::Prepare;
    npc.execution.destination = choice.destination;
    npc.execution.target = choice.target;
    npc.execution.id = m_NextExecution++;
    npc.execution.startedAt = m_Time;
    npc.execution.timeBudget =
        std::clamp(Length(choice.destination.x - npc.WorldX(), choice.destination.y - npc.WorldY())
                           / (std::max)(0.5f, npc.vitals.moveSpeed) * 1.6
                       + 12,
                   15.0,
                   90.0);
    npc.stalledTime = 0;
    npc.execution.phaseUntil = m_Time + 0.2;
    npc.execution.playerRequest = request;
    npc.execution.reservation = resource;
    if (resource)
    {
        m_Reservations[resource] = {npc.Entity(), m_Time + npc.execution.timeBudget + 2};
    }
    npc.nextDecision = m_Time + (npc.mode == NpcMode::Combat ? 0.35 : 2.5);
    if (request)
    {
        npc.mode = NpcMode::Interaction;
    }
    return true;
}

void NpcWorld::Finish(NpcActor& npc, bool success, const std::string& reason)
{
    const auto action = npc.execution.action;
    Release(npc);
    npc.lastResult = reason;
    npc.path.clear();
    if (success)
    {
        ++npc.completedActions;
        ++npc.routeVisits[static_cast<size_t>(action)];
        npc.history.push_back({action, m_Time});
        while (npc.history.size() > 3)
        {
            npc.history.pop_front();
        }
    }
    else
    {
        ++npc.failedActions;
        if (action == NpcAction::Patrol || action == NpcAction::Hunt)
        {
            ++npc.routeVisits[static_cast<size_t>(action)];
        }
        npc.failedUntil[{action, npc.execution.target}] = m_Time + 3;
    }
    m_Log.push_back("t=" + std::to_string(m_Time) + " npc=" + std::to_string(npc.profile.id)
                    + " outcome=" + (success ? "complete" : "failed")
                    + " action=" + NpcActionId(action) + " reason=" + reason
                    + " distance=" + std::to_string(npc.distanceTravelled));
    while (m_Log.size() > 512)
    {
        m_Log.pop_front();
    }
    if (npc.execution.playerRequest)
    {
        m_Game.Notice(npc.profile.name + ": " + reason);
    }
    npc.execution.phase = NpcPhase::Complete;
    npc.nextDecision = m_Time + (success ? 0.25 : 0.5);
    for (auto it = npc.failedUntil.begin(); it != npc.failedUntil.end();)
    {
        if (it->second <= m_Time)
        {
            it = npc.failedUntil.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void NpcWorld::Execute(NpcActor& npc, float dt)
{
    auto& run = npc.execution;
    if (run.phase == NpcPhase::Complete)
    {
        return;
    }
    if (m_Time - run.startedAt > run.timeBudget)
    {
        Finish(npc, false, "수행 시간 초과: 경로나 대상을 다시 찾는다");
        return;
    }
    if (run.reservation && !CanReserve(run.reservation, npc.Entity()))
    {
        Finish(npc, false, "예약을 잃어 행동을 중단했다");
        return;
    }
    if (run.phase == NpcPhase::Prepare)
    {
        if (m_Time >= run.phaseUntil)
        {
            run.phase = NpcPhase::Approach;
        }
        return;
    }
    const bool attack = run.action == NpcAction::Attack || run.action == NpcAction::Raid;
    const auto& weapon = NpcWeaponFor(npc.profile.role);
    const double reach = run.action == NpcAction::Attack ? weapon.range * 0.85
                         : run.action == NpcAction::Raid ? 1.0
                         : run.target                    ? 1.5
                                                         : 0.7;
    if (run.phase == NpcPhase::Approach)
    {
        if (run.target && run.target != npc.Entity())
        {
            const auto memory = std::find_if(npc.knowledge.begin(),
                                             npc.knowledge.end(),
                                             [&](const auto& item)
                                             {
                                                 return item.entity == run.target;
                                             });
            if (memory == npc.knowledge.end() || m_Time - memory->seenAt > 3)
            {
                Finish(npc, false, "대상을 놓쳤다. 마지막 관측 이후 수색 종료");
                return;
            }
            run.destination = memory->position;
        }
        const double distance =
            Length(npc.WorldX() - run.destination.x, npc.WorldY() - run.destination.y);
        const bool mustLeaveSanctuary = attack && InsideSanctuary(npc.WorldX(), npc.WorldY());
        const bool obstructed =
            attack
            && !m_Game.ClearShot(npc.WorldX(), npc.WorldY(), run.destination.x, run.destination.y);
        if (distance > reach || mustLeaveSanctuary || obstructed)
        {
            if (!Travel(npc, run.destination, dt))
            {
                Finish(npc, false, "도달할 수 없는 경로");
            }
            return;
        }
        run.phase = NpcPhase::Perform;
        const double duration = attack ? weapon.windup
                                : run.action == NpcAction::Rest || run.action == NpcAction::Pray
                                        || run.action == NpcAction::Reflect
                                    ? 4.0
                                    : 2.0;
        run.phaseUntil = m_Time + duration;
        return;
    }
    if (run.phase == NpcPhase::Perform && m_Time >= run.phaseUntil)
    {
        if (run.applied || !Apply(npc))
        {
            Finish(npc, false, "조건 변경: 자원·거리·자격을 다시 확인해야 한다");
            return;
        }
        run.applied = true;
        const double cooldown = run.action == NpcAction::Raid      ? 12
                                : run.action == NpcAction::Heal    ? 8
                                : run.action == NpcAction::Pray    ? 18
                                : run.action == NpcAction::Reflect ? 22
                                : run.action == NpcAction::Guard   ? 8
                                : attack                           ? 1
                                                                   : 3;
        npc.cooldown[static_cast<size_t>(run.action)] = m_Time + cooldown;
        run.phase = NpcPhase::Recover;
        run.phaseUntil = m_Time + (attack ? weapon.recovery : 0.3);
    }
    if (run.phase == NpcPhase::Recover && m_Time >= run.phaseUntil)
    {
        Finish(npc, true, npc.lastResult);
    }
}

bool NpcWorld::Apply(NpcActor& npc)
{
    const int location = VillageAt(npc.WorldPosition());
    auto& economy = m_Economies[location >= 0 ? size_t(location) : npc.profile.village];
    auto& v = npc.vitals;
    const auto& run = npc.execution;
    const auto action = run.action;
    if (!v.alive || run.applied)
    {
        return false;
    }
    if (!run.target
        && Length(npc.WorldX() - run.destination.x, npc.WorldY() - run.destination.y) > 0.85)
    {
        return false;
    }
    Actor* target = Resolve(run.target);
    const auto& weapon = NpcWeaponFor(npc.profile.role);
    const double effectRange = action == NpcAction::Attack ? weapon.range : 1.6;
    if (run.target
        && (!Alive(run.target) || !target
            || Length(npc.WorldX() - target->WorldX(), npc.WorldY() - target->WorldY())
                   > effectRange
            || !m_Game.ClearShot(npc.WorldX(), npc.WorldY(), target->WorldX(), target->WorldY())))
    {
        return false;
    }
    if (action == NpcAction::Attack || action == NpcAction::Raid)
    {
        if (!target
            || Length(npc.WorldX() - target->WorldX(), npc.WorldY() - target->WorldY())
                   > (action == NpcAction::Raid ? 1.2 : weapon.range)
            || InsideSanctuary(npc.WorldX(), npc.WorldY())
            || InsideSanctuary(target->WorldX(), target->WorldY()))
        {
            return false;
        }
        if (action == NpcAction::Attack && !Hostile(npc, run.target))
        {
            return false;
        }
        if (action == NpcAction::Raid && npc.profile.role != NpcRole::Raider)
        {
            return false;
        }
        const float damage = v.strength * (1 + v.corruption * 0.004f)
                             * (action == NpcAction::Raid ? 1.0f : weapon.damageScale);
        if (action == NpcAction::Attack && weapon.speed > 0)
        {
            if (npc.profile.role == NpcRole::Priest)
            {
                if (v.holyPower < 4)
                {
                    return false;
                }
                v.holyPower -= 4;
            }
            m_Game.Scene().Spawn<NpcMissileActor>(m_Game.m_EffectsRoot,
                                                  m_Game,
                                                  *this,
                                                  npc.Entity(),
                                                  run.target,
                                                  npc.profile.role,
                                                  npc.WorldPosition(),
                                                  target->WorldPosition(),
                                                  damage);
            npc.lastResult = std::string(weapon.name) + " 발사";
        }
        else
        {
            Damage(npc.Entity(), run.target, damage, action == NpcAction::Raid);
            npc.lastResult = action == NpcAction::Raid ? "실제 재산을 빼앗았다" : "검격 명중";
        }
        return true;
    }
    if (action == NpcAction::Heal || action == NpcAction::Bless)
    {
        if (!target || Hostile(npc, run.target))
        {
            return false;
        }
        if (action == NpcAction::Bless && (npc.profile.role != NpcRole::Priest || v.holyPower < 8))
        {
            return false;
        }
        if (action == NpcAction::Heal && v.medicine <= 0)
        {
            return false;
        }
        auto* patient = Npc(run.target);
        float& health = patient ? patient->vitals.health : m_Game.Player().health;
        const float maximum = patient ? patient->vitals.maxHealth : m_Game.MaxHealth();
        const bool canCleanse = action == NpcAction::Bless && patient
                                && patient->vitals.corruption > 0
                                && patient->vitals.corruption < MutationThreshold;
        if (health >= maximum && !run.playerRequest && !canCleanse)
        {
            return false;
        }
        if (action == NpcAction::Heal)
        {
            --v.medicine;
        }
        else
        {
            v.holyPower -= 8;
        }
        health = (std::min)(maximum, health + 32);
        if (canCleanse)
        {
            patient->vitals.corruption = (std::max)(0.0f, patient->vitals.corruption - 8);
        }
        if (patient)
        {
            VillageIncident(npc.profile.village, patient->profile.village, 3);
            patient->relations[npc.Entity()].trust =
                (std::min)(1.0f, patient->relations[npc.Entity()].trust + 0.15f);
        }
        npc.lastResult = canCleanse ? "축복으로 상처와 변이 이전의 오염을 덜었다"
                                    : "상처를 보살폈다. 과거의 원한은 남아 있다";
        Event(
            npc.profile.name
            + (action == NpcAction::Bless ? "가 성력을 사용해 축복했다" : "가 치료제를 사용했다"));
        RecordWorldEvent(action == NpcAction::Bless ? "blessing" : "heal",
                         npc.profile.name + "의 " + NpcActionName(action),
                         target->WorldPosition(),
                         &npc,
                         target);
        return true;
    }
    if (action == NpcAction::Trade)
    {
        if (location < 0
            || (run.playerRequest && VillageAt(m_Game.Player().WorldPosition()) != location))
        {
            return false;
        }
        if (run.playerRequest)
        {
            if (Hostile(npc, PlayerEntity) || m_Game.Player().gold < 12 || economy.medicine < 1)
            {
                return false;
            }
            m_Game.Player().gold -= 12;
            ++m_Game.Player().potions;
            --economy.medicine;
            economy.treasury += 10;
            v.gold += 2;
            npc.lastResult = "금화 12개로 회복약을 샀습니다";
        }
        else if (v.goods > 0 && economy.treasury >= 8)
        {
            --v.goods;
            ++economy.goods;
            economy.treasury -= 8;
            v.gold += 8;
            npc.lastResult = "전리품을 금화 8개에 판매했다";
        }
        else if (v.food < 3 && v.gold >= 6 && economy.food > 0)
        {
            ++v.food;
            --economy.food;
            v.gold -= 6;
            economy.treasury += 6;
            npc.lastResult = "식량을 구입했다";
        }
        else if (v.medicine < 2 && v.gold >= 12 && economy.medicine > 0)
        {
            ++v.medicine;
            --economy.medicine;
            v.gold -= 12;
            economy.treasury += 12;
            npc.lastResult = "치료제를 구입했다";
        }
        else
        {
            return false;
        }
        Event(npc.profile.name + ": " + npc.lastResult);
        RecordWorldEvent("trade",
                         npc.lastResult,
                         npc.WorldPosition(),
                         &npc,
                         run.playerRequest ? &m_Game.Player() : nullptr);
        return true;
    }
    switch (action)
    {
    case NpcAction::Observe:
        npc.lastResult =
            run.playerRequest
                ? std::string("저는 ") + NpcRoleName(npc.profile.role) + "입니다. 지금은 "
                      + NpcGoalName(npc.profile.goal)
                      + "을 추구하고 있어요. 거래는 Y, 축복은 성직자에게 O로 요청하세요."
                : "주변을 관찰했다";
        break;
    case NpcAction::Work:
        if (economy.treasury < 5)
        {
            return false;
        }
        economy.treasury -= 5;
        v.gold += 5;
        ++economy.food;
        v.fatigue = (std::min)(1.0f, v.fatigue + 0.08f);
        if (economy.goods > 0)
        {
            --economy.goods;
            ++economy.medicine;
        }
        npc.lastResult = "식량을 생산하고 마을 예산에서 임금을 받았다";
        break;
    case NpcAction::Rest:
        v.fatigue = (std::max)(0.0f, v.fatigue - 0.3f);
        v.health = (std::min)(v.maxHealth, v.health + 15);
        npc.lastResult = "휴식으로 체력과 피로를 회복했다";
        break;
    case NpcAction::Eat:
        if (v.food <= 0)
        {
            return false;
        }
        --v.food;
        v.hunger = (std::max)(0.0f, v.hunger - 0.6f);
        npc.lastResult = "보유 식량을 먹었다";
        break;
    case NpcAction::Pray:
        v.holyPower = (std::min)(100.0f, v.holyPower + 12);
        npc.lastResult = "기도로 성력이 자랐다";
        break;
    case NpcAction::Train:
        v.strength = (std::min)(35.0f, v.strength + 0.6f);
        v.maxHealth = (std::min)(180.0f, v.maxHealth + 0.5f);
        v.fatigue = (std::min)(1.0f, v.fatigue + 0.12f);
        npc.lastResult = "훈련으로 무력과 최대 체력이 자랐다";
        break;
    case NpcAction::Reflect:
        v.wisdom = (std::min)(100.0f, v.wisdom + 1);
        v.morale = (std::min)(1.0f, v.morale + 0.1f);
        npc.lastResult = "명상으로 깨달음을 얻었다";
        break;
    case NpcAction::Purify:
        if (v.corruption <= 0 || v.corruption >= MutationThreshold || v.gold < 3)
        {
            return false;
        }
        v.gold -= 3;
        economy.treasury += 3;
        v.corruption = (std::max)(0.0f, v.corruption - 30);
        if (v.corruption < 60)
        {
            v.frenzy = false;
        }
        npc.lastResult = "성당에서 정화했다. 관계의 원한은 남는다";
        break;
    case NpcAction::DawnOil:
        if (v.corruption < MutationThreshold || v.dawnOil <= 0)
        {
            return false;
        }
        --v.dawnOil;
        v.corruption = 0;
        v.frenzy = false;
        npc.lastResult = "새벽의 성유를 소모해 변이를 되돌렸다";
        break;
    default:
        npc.lastResult = std::string(NpcActionName(action)) + "을 마쳤다";
        break;
    }
    return true;
}

bool NpcWorld::Crime(NpcEntity attacker, NpcEntity target)
{
    const Actor* source = Resolve(attacker);
    if (!source || attacker >= 10000)
    {
        return false;
    }
    const auto* initiator = Npc(attacker);
    const auto* victimNpc = Npc(target);
    const bool beginsCrime = initiator ? initiator->vitals.frenzy
                                             || initiator->profile.role == NpcRole::Raider
                                             || !Hostile(*initiator, target)
                                       : victimNpc && !Hostile(*victimNpc, PlayerEntity);
    const auto* victimActor = Resolve(target);
    const auto conflict =
        std::make_pair(source->Id(), victimActor ? victimActor->Id() : InvalidActor);
    if (beginsCrime)
    {
        m_UnlawfulConflicts.insert(conflict);
    }
    const bool unlawful =
        beginsCrime || m_UnlawfulConflicts.find(conflict) != m_UnlawfulConflicts.end();
    for (auto id : m_Actors)
    {
        auto* witness = dynamic_cast<NpcActor*>(m_Game.Scene().Find(id));
        if (!witness || !witness->vitals.alive || witness->Entity() == attacker)
        {
            continue;
        }
        const bool victim = witness->Entity() == target;
        if (!victim && !unlawful)
        {
            continue;
        }
        if (!victim && (!Visible(*witness, attacker) || !Visible(*witness, target)))
        {
            continue;
        }
        // Ordinary witnesses fear the attacker; knights/priests also defend the community.
        witness->vitals.morale = (std::max)(0.0f, witness->vitals.morale - 0.05f);
        if (victim || witness->profile.role == NpcRole::Knight
            || witness->profile.role == NpcRole::Priest)
        {
            witness->relations[attacker].grudge = 1;
            witness->relations[attacker].trust = -1;
            witness->lastHurt = m_Time;
            witness->nextSense = 0;
            witness->nextDecision = m_Time;
        }
    }
    return unlawful;
}

void NpcWorld::Damage(NpcEntity attacker, NpcEntity target, float amount, bool robbery)
{
    Actor* actor = Resolve(target);
    if (!actor || !Alive(target) || InsideSanctuary(actor->WorldX(), actor->WorldY()))
    {
        return;
    }
    NpcActor* source = Npc(attacker);
    NpcActor* victim = Npc(target);
    if (victim)
    {
        const bool defending = victim->execution.action == NpcAction::Guard
                               && victim->execution.phase == NpcPhase::Perform;
        amount *= defending ? 0.5f : 1.0f;
        amount *= 100.0f / (100.0f + victim->vitals.armor * 5.0f);
        const bool unlawful = Crime(attacker, target);
        if (source && source->profile.village != victim->profile.village && (unlawful || robbery))
        {
            VillageIncident(source->profile.village, victim->profile.village, -4);
        }
        victim->vitals.health = (std::max)(0.0f, victim->vitals.health - amount);
        victim->vitals.morale = (std::max)(0.0f, victim->vitals.morale - 0.12f);
        victim->lastHurt = m_Time;
        victim->nextSense = 0;
        if (source && robbery)
        {
            const int taken = (std::min)(12, victim->vitals.gold);
            victim->vitals.gold -= taken;
            source->vitals.gold += taken;
            Event(source->profile.name + "가 " + victim->profile.name + "에게 금화 "
                  + std::to_string(taken) + "개를 약탈했다");
            if (taken > 0)
            {
                RecordWorldEvent("raid",
                                 source->profile.name + "의 금화 약탈 " + std::to_string(taken),
                                 victim->WorldPosition(),
                                 source,
                                 victim);
            }
        }
        if (victim->vitals.health <= 0)
        {
            victim->vitals.alive = false;
            victim->mode = NpcMode::Dead;
            Release(*victim);
            victim->execution.phase = NpcPhase::Complete;
            if (attacker == PlayerEntity)
            {
                m_Game.Player().gold += victim->vitals.gold;
                victim->vitals.gold = 0;
            }
            Event(victim->profile.name + " 사망. 일반 주민은 귀환자가 아니므로 부활하지 않는다.");
            const auto* killer = Resolve(attacker);
            RecordWorldEvent(killer && unlawful ? "murder"
                             : killer           ? "kill"
                                                : "death",
                             victim->profile.name + (killer && unlawful ? " 살해됨" : " 사망"),
                             victim->WorldPosition(),
                             killer,
                             victim);
        }
    }
    else if (target == PlayerEntity)
    {
        if (m_Game.Player().hitCooldown > 0)
        {
            return;
        }
        m_Game.Player().health -= amount;
        m_Game.Player().hitCooldown = 0.8f;
        bool unlawful = false;
        if (source)
        {
            source->relations[PlayerEntity].grudge = 1;
            if (robbery)
            {
                const int taken = (std::min)(12, m_Game.Player().gold);
                m_Game.Player().gold -= taken;
                source->vitals.gold += taken;
                Event(source->profile.name + "가 플레이어의 금화 " + std::to_string(taken)
                      + "개를 약탈했다");
                if (taken > 0)
                {
                    RecordWorldEvent("raid",
                                     "플레이어의 금화 약탈 " + std::to_string(taken),
                                     m_Game.Player().WorldPosition(),
                                     source,
                                     &m_Game.Player());
                }
            }
            unlawful = Crime(attacker, target);
        }
        if (m_Game.Player().health <= 0)
        {
            m_Game.Die(source, unlawful);
        }
    }
    else if (auto* enemy = dynamic_cast<EnemyActor*>(actor))
    {
        enemy->health -= amount;
        if (enemy->health <= 0)
        {
            m_Game.DefeatEnemy(*enemy, false);
            RecordWorldEvent("hunt", "몬스터 토벌", enemy->WorldPosition(), source, enemy);
            if (source)
            {
                const int village = VillageAt(enemy->WorldPosition());
                if (village >= 0)
                {
                    VillageIncident(source->profile.village, size_t(village), 2);
                }
                ++source->vitals.goods;
                source->vitals.strength = (std::min)(35.0f, source->vitals.strength + 0.3f);
                Event(source->profile.name + "가 토벌 전리품을 얻었다");
            }
        }
    }
    m_Game.AddCombatText(actor->WorldX(),
                         actor->WorldY(),
                         "-" + std::to_string(int(amount)),
                         DamageColor);
}

bool NpcWorld::PlayerCanTarget(const NpcActor& npc) const
{
    return npc.vitals.alive && !InsideSanctuary(npc.WorldX(), npc.WorldY())
           && !InsideSanctuary(m_Game.Player().WorldX(), m_Game.Player().WorldY())
           && (m_PK || Hostile(npc, PlayerEntity));
}

bool NpcWorld::HitByPlayer(double x, double y, float damage, bool allowPK)
{
    if (InsideSanctuary(m_Game.Player().WorldX(), m_Game.Player().WorldY()))
    {
        return false;
    }
    for (auto id : m_Actors)
    {
        auto* npc = dynamic_cast<NpcActor*>(m_Game.Scene().Find(id));
        if (npc && npc->vitals.alive && (allowPK || Hostile(*npc, PlayerEntity))
            && !InsideSanctuary(npc->WorldX(), npc->WorldY())
            && Length(x - npc->WorldX(), y - npc->WorldY()) < 0.38)
        {
            Damage(PlayerEntity, npc->Entity(), damage);
            return true;
        }
    }
    return false;
}

bool NpcWorld::HitByMonster(double x, double y, float damage)
{
    for (auto id : m_Actors)
    {
        auto* npc = dynamic_cast<NpcActor*>(m_Game.Scene().Find(id));
        if (npc && npc->vitals.alive && !InsideSanctuary(npc->WorldX(), npc->WorldY())
            && Length(x - npc->WorldX(), y - npc->WorldY()) < 0.38)
        {
            Damage(0, npc->Entity(), damage);
            return true;
        }
    }
    return false;
}

Actor* NpcWorld::EnemyTarget(const EnemyActor& enemy) const
{
    Actor* chosen = nullptr;
    double nearest = 8;
    const auto consider = [&](Actor& target, bool alive)
    {
        const double distance =
            Length(target.WorldX() - enemy.WorldX(), target.WorldY() - enemy.WorldY());
        if (alive && target.EnabledInHierarchy()
            && !InsideSanctuary(target.WorldX(), target.WorldY()) && distance < nearest
            && m_Game.ClearShot(enemy.WorldX(), enemy.WorldY(), target.WorldX(), target.WorldY()))
        {
            nearest = distance;
            chosen = &target;
        }
    };
    consider(m_Game.Player(), m_Game.Player().health > 0);
    for (auto id : m_Actors)
    {
        auto* npc = dynamic_cast<NpcActor*>(m_Game.Scene().Find(id));
        if (npc)
        {
            consider(*npc, npc->vitals.alive);
        }
    }
    return chosen;
}

void NpcWorld::MonsterDamage(Actor& target, float damage)
{
    Damage(0, Entity(target), damage);
}
