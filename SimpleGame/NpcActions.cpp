#include "stdafx.h"
#include "NpcWorld.h"
#include "Prototype.h"
#include "GameRules.h"
#include <algorithm>
#include <cmath>
#include <queue>

using namespace GameRules;

bool NpcWorld::Walkable(double x, double y) const
{
    return !m_Game.Blocked(x - 0.2, y) && !m_Game.Blocked(x + 0.2, y) && !m_Game.Blocked(x, y - 0.2)
           && !m_Game.Blocked(x, y + 0.2);
}

bool NpcWorld::BuildPath(NpcActor& npc, WorldPoint target)
{
    npc.path.clear();
    npc.pathIndex = 0;
    npc.nextPath = m_Time + 1;
    const auto clearRoute = [&](WorldPoint from, WorldPoint to)
    {
        const int steps = (std::max)(1, int(std::ceil(Length(from.x - to.x, from.y - to.y) / 0.2)));
        for (int i = 1; i <= steps; ++i)
        {
            const double t = double(i) / steps;
            if (!Walkable(from.x + (to.x - from.x) * t, from.y + (to.y - from.y) * t))
            {
                return false;
            }
        }
        return true;
    };
    if (clearRoute(npc.WorldPosition(), target))
    {
        npc.path.push_back(target);
        return true;
    }
    constexpr int Left = -12;
    constexpr int Top = -8;
    constexpr int Width = 37;
    constexpr int Height = 24;
    const auto cell = [Left, Top, Width, Height](int x, int y)
    {
        return x < Left || x >= Left + Width || y < Top || y >= Top + Height
                   ? -1
                   : (y - Top) * Width + x - Left;
    };
    const int start = cell(int(std::round(npc.WorldX())), int(std::round(npc.WorldY())));
    if (start < 0)
    {
        return false;
    }
    std::array<int, Width * Height> previous;
    previous.fill(-1);
    std::queue<int> pending;
    pending.push(start);
    previous[start] = start;
    int goal = -1;
    const int dx[] = {1, -1, 0, 0};
    const int dy[] = {0, 0, 1, -1};
    while (!pending.empty())
    {
        const int current = pending.front();
        pending.pop();
        const int x = current % Width + Left;
        const int y = current / Width + Top;
        if (Length(x - target.x, y - target.y) < 0.85 && Walkable(x, y)
            && clearRoute({double(x), double(y)}, target))
        {
            goal = current;
            break;
        }
        for (int direction = 0; direction < 4; ++direction)
        {
            const int nx = x + dx[direction];
            const int ny = y + dy[direction];
            const int next = cell(nx, ny);
            const WorldPoint from =
                current == start ? npc.WorldPosition() : WorldPoint{double(x), double(y)};
            if (next >= 0 && previous[next] < 0 && Walkable(nx, ny)
                && clearRoute(from, {double(nx), double(ny)}))
            {
                previous[next] = current;
                pending.push(next);
            }
        }
    }
    if (goal < 0)
    {
        return false;
    }
    for (int at = goal; at != start; at = previous[at])
    {
        npc.path.push_back({double(at % Width + Left), double(at / Width + Top)});
    }
    std::reverse(npc.path.begin(), npc.path.end());
    const WorldPoint last{double(goal % Width + Left), double(goal / Width + Top)};
    if (clearRoute(last, target))
    {
        npc.path.push_back(target);
    }
    return true;
}

bool NpcWorld::Travel(NpcActor& npc, WorldPoint target, float dt)
{
    if (m_Time >= npc.nextPath || npc.path.empty())
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
    const float speed = npc.execution.action == NpcAction::Flee ? 2.8f : 2.0f;
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
    const auto resource = Resource(choice.action, choice.target);
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
    npc.stalledTime = 0;
    npc.execution.phaseUntil = m_Time + 0.2;
    npc.execution.playerRequest = request;
    npc.execution.reservation = resource;
    if (resource)
    {
        m_Reservations[resource] = {npc.Entity(), m_Time + 18};
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
    if (m_Time - run.startedAt > 15)
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
        if (health >= maximum && !run.playerRequest)
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
        if (patient)
        {
            patient->relations[npc.Entity()].trust =
                (std::min)(1.0f, patient->relations[npc.Entity()].trust + 0.15f);
        }
        npc.lastResult = "상처를 보살폈다. 오염과 과거의 원한은 별개다";
        Event(
            npc.profile.name
            + (action == NpcAction::Bless ? "가 성력을 사용해 축복했다" : "가 치료제를 사용했다"));
        return true;
    }
    if (action == NpcAction::Trade)
    {
        if (!InVillage(npc.WorldPosition())
            || (run.playerRequest && !InVillage(m_Game.Player().WorldPosition())))
        {
            return false;
        }
        if (run.playerRequest)
        {
            if (Hostile(npc, PlayerEntity) || m_Game.Player().gold < 12 || m_MarketMedicine < 1)
            {
                return false;
            }
            m_Game.Player().gold -= 12;
            ++m_Game.Player().potions;
            --m_MarketMedicine;
            m_Treasury += 10;
            v.gold += 2;
            npc.lastResult = "금화 12개로 회복약을 샀습니다";
        }
        else if (v.goods > 0 && m_Treasury >= 8)
        {
            --v.goods;
            ++m_MarketGoods;
            m_Treasury -= 8;
            v.gold += 8;
            npc.lastResult = "전리품을 금화 8개에 판매했다";
        }
        else if (v.food < 3 && v.gold >= 6 && m_MarketFood > 0)
        {
            ++v.food;
            --m_MarketFood;
            v.gold -= 6;
            m_Treasury += 6;
            npc.lastResult = "식량을 구입했다";
        }
        else if (v.medicine < 2 && v.gold >= 12 && m_MarketMedicine > 0)
        {
            ++v.medicine;
            --m_MarketMedicine;
            v.gold -= 12;
            m_Treasury += 12;
            npc.lastResult = "치료제를 구입했다";
        }
        else
        {
            return false;
        }
        Event(npc.profile.name + ": " + npc.lastResult);
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
        if (m_Treasury < 5)
        {
            return false;
        }
        m_Treasury -= 5;
        v.gold += 5;
        ++m_MarketFood;
        v.fatigue = (std::min)(1.0f, v.fatigue + 0.08f);
        if (m_MarketGoods > 0)
        {
            --m_MarketGoods;
            ++m_MarketMedicine;
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
        m_Treasury += 3;
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

void NpcWorld::Crime(NpcEntity attacker, NpcEntity target)
{
    const Actor* source = Resolve(attacker);
    if (!source || attacker >= 10000)
    {
        return;
    }
    const auto* initiator = Npc(attacker);
    const auto* victimNpc = Npc(target);
    const bool unlawful = initiator ? initiator->vitals.frenzy
                                          || initiator->profile.role == NpcRole::Raider
                                          || !Hostile(*initiator, target)
                                    : victimNpc && !Hostile(*victimNpc, PlayerEntity);
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
        Crime(attacker, target);
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
            }
            Crime(attacker, target);
        }
        if (m_Game.Player().health <= 0)
        {
            m_Game.Die();
        }
    }
    else if (auto* enemy = dynamic_cast<EnemyActor*>(actor))
    {
        enemy->health -= amount;
        if (enemy->health <= 0)
        {
            m_Game.DefeatEnemy(*enemy, false);
            if (source)
            {
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
