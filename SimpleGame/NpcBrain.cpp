#include "stdafx.h"
#include "NpcAI.h"
#include <algorithm>
#include <cmath>

const NpcWeapon& NpcWeaponFor(NpcRole role)
{
    static const NpcWeapon weapons[] = {{"성탄", 5.5f, 0.7f, 0.65f, 1.3f, 7.0f},
                                        {"검격", 1.5f, 0.4f, 0.45f, 1.5f, 0},
                                        {"활", 6.5f, 0.55f, 0.5f, 1.1f, 11.0f},
                                        {"투척 단검", 4.2f, 0.3f, 0.4f, 0.9f, 9.0f},
                                        {"투석", 3.8f, 0.6f, 0.7f, 0.7f, 6.5f}};
    return weapons[static_cast<size_t>(role)];
}

const char* NpcRoleName(NpcRole role)
{
    static const char* names[] = {"성직자", "기사", "모험가", "약탈자", "상인"};
    return names[static_cast<size_t>(role)];
}

const char* NpcGoalName(NpcGoal goal)
{
    static const char* names[] = {"건강", "부", "깨달음", "신앙", "무력"};
    return names[static_cast<size_t>(goal)];
}

const char* NpcActionName(NpcAction action)
{
    static const char* names[] = {"관찰",
                                  "생산",
                                  "휴식",
                                  "식사",
                                  "순찰",
                                  "토벌 탐색",
                                  "약탈",
                                  "거래",
                                  "축복",
                                  "기도",
                                  "훈련",
                                  "명상",
                                  "치료",
                                  "정화",
                                  "성유 사용",
                                  "대피",
                                  "공격",
                                  "경계",
                                  "귀환"};
    return names[static_cast<size_t>(action)];
}

const char* NpcActionId(NpcAction action)
{
    static const char* names[] = {"idle_observe",
                                  "work",
                                  "rest",
                                  "eat",
                                  "patrol",
                                  "hunt",
                                  "raid",
                                  "trade",
                                  "bless",
                                  "pray",
                                  "train",
                                  "reflect",
                                  "heal_ally",
                                  "purify",
                                  "use_dawn_oil",
                                  "flee",
                                  "attack",
                                  "guard",
                                  "return_home"};
    return names[static_cast<size_t>(action)];
}

const char* NpcActivityName(NpcRole role, NpcAction action)
{
    if (action == NpcAction::Attack)
    {
        return NpcWeaponFor(role).name;
    }
    if (action == NpcAction::Patrol)
    {
        static const char* names[] = {"방문 구호", "경계 순찰", "탐사", "매복지 탐색", "상권 순회"};
        return names[static_cast<size_t>(role)];
    }
    return NpcActionName(action);
}

const char* NpcModeName(NpcMode mode)
{
    static const char* names[] = {"일상", "경계", "전투", "대피", "상호작용", "사망"};
    return names[static_cast<size_t>(mode)];
}

const char* NpcPhaseName(NpcPhase phase)
{
    static const char* names[] = {"준비", "접근", "수행", "회복", "완료"};
    return names[static_cast<size_t>(phase)];
}

void NpcBrain::Score(NpcActor& npc, double now)
{
    const auto& p = npc.profile;
    const auto& v = npc.vitals;
    const float injury = 1 - v.health / v.maxHealth;
    const bool danger = npc.mode == NpcMode::Combat || npc.mode == NpcMode::Evacuate;
    double maximum = 0;
    for (auto& c : npc.candidates)
    {
        if (!c.allowed)
        {
            c.weight = 0;
            c.probability = 0;
            continue;
        }
        const auto a = c.action;
        if (a == NpcAction::Attack || a == NpcAction::Hunt || a == NpcAction::Train)
        {
            c.personality = 18 * p.courage + 8 * p.duty - 12 * p.caution;
            c.need = -20 * injury - 15 * v.fatigue;
            c.goal = p.goal == NpcGoal::Power ? 25.0f : 0;
        }
        if (a == NpcAction::Raid)
        {
            c.personality = 20 * p.greed + 15 * p.raidWeight - 20 * p.compassion;
            c.need = 20 * (1 - (std::min)(1.0f, float(v.gold) / 80));
            c.situation += 30 * p.raidWeight - 20 * injury;
            c.goal = p.goal == NpcGoal::Wealth ? 30.0f : 0;
        }
        if (a == NpcAction::Heal || a == NpcAction::Bless)
        {
            c.personality = 20 * p.compassion + 8 * p.duty - 10 * p.greed;
            c.goal = p.goal == NpcGoal::Health ? 20.0f : 0;
            const auto relation = npc.relations.find(c.target);
            if (relation != npc.relations.end())
            {
                c.relation = 20 * relation->second.trust - 15 * relation->second.grudge;
            }
            for (const auto& memory : npc.knowledge)
            {
                if (memory.entity == c.target)
                {
                    c.situation += 30 * (1 - memory.healthRatio);
                }
            }
            if (c.target == npc.Entity())
            {
                c.need = 30 * injury;
            }
        }
        if (a == NpcAction::Rest)
        {
            c.personality = 12 * p.caution;
            c.need = 30 * (std::max)(v.fatigue, injury);
            c.goal = p.goal == NpcGoal::Health ? 20.0f : 0;
        }
        if (a == NpcAction::Eat)
        {
            c.need = 30 * v.hunger;
        }
        if (a == NpcAction::Trade || a == NpcAction::Work)
        {
            c.personality = 18 * p.greed + 5 * p.duty;
            c.need = 15 * v.hunger + (v.medicine == 0 ? 10.0f : 0);
            c.goal = p.goal == NpcGoal::Wealth ? 25.0f : 0;
        }
        if (a == NpcAction::Pray || a == NpcAction::Purify)
        {
            c.personality = 20 * p.faith;
            c.need = a == NpcAction::Purify ? v.corruption * 0.3f : (100 - v.holyPower) * 0.25f;
            c.goal = p.goal == NpcGoal::Faith ? 25.0f : 0;
        }
        if (a == NpcAction::Reflect)
        {
            c.personality = 12 * p.caution + 8 * p.faith;
            c.goal = p.goal == NpcGoal::Wisdom ? 30.0f : 0;
        }
        if (a == NpcAction::Patrol || a == NpcAction::Guard)
        {
            c.personality = 20 * p.duty;
            c.situation += danger ? 15.0f : 0;
        }
        if (a == NpcAction::Flee)
        {
            c.personality = 20 * p.caution - 10 * p.courage;
            c.need = 30 * injury + 20 * (1 - v.morale);
            c.situation += danger ? 30.0f : 0;
        }
        if (a == NpcAction::Patrol || a == NpcAction::Hunt)
        {
            c.situation += float((std::min)(25.0, npc.stationaryTime));
            if (a == NpcAction::Patrol)
            {
                c.goal = 15;
            }
        }
        if (a == NpcAction::Attack)
        {
            c.situation += 30;
        }
        c.personality = std::clamp(c.personality, -20.0f, 20.0f);
        c.need = std::clamp(c.need, -20.0f, 30.0f);
        c.relation = std::clamp(c.relation, -25.0f, 25.0f);
        c.situation = std::clamp(c.situation, -40.0f, 40.0f);
        c.goal = std::clamp(c.goal, -20.0f, 30.0f);
        c.cost = std::clamp(c.cost, 0.0f, 30.0f);
        const bool sustained = a == NpcAction::Patrol || a == NpcAction::Flee;
        int repetitions = 0;
        for (const auto& recent : npc.history)
        {
            if (recent.first == a && now - recent.second <= 30)
            {
                ++repetitions;
            }
        }
        c.repetition = sustained ? 1.0f : (std::max)(0.35f, 1.0f / (1 + repetitions));
        c.continuity = sustained && npc.execution.action == a ? 1.2f : 1.0f;
        c.weight =
            std::clamp(c.base + c.personality + c.need + c.relation + c.situation + c.goal - c.cost,
                       0.0f,
                       100.0f)
            * c.repetition * c.continuity;
        if (!std::isfinite(c.weight))
        {
            c.allowed = false;
            c.reason = "잘못된 점수";
            c.weight = 0;
        }
        maximum = (std::max)(maximum, double(c.weight));
    }
    double total = 0;
    const double temperature = std::clamp(double(p.temperature), 0.5, 1.5);
    for (auto& c : npc.candidates)
    {
        c.probability =
            maximum > 0 && c.weight > 0 ? std::pow(c.weight / maximum, 1 / temperature) : 0;
        total += c.probability;
    }
    if (total > 0)
    {
        for (auto& c : npc.candidates)
        {
            c.probability /= total;
        }
    }
}

size_t NpcBrain::Choose(NpcActor& npc)
{
    // SplitMix64: fixed algorithm and 53-bit [0,1) conversion, independent per NPC.
    npc.randomState += 0x9E3779B97F4A7C15ull;
    auto value = npc.randomState;
    value = (value ^ (value >> 30)) * 0xBF58476D1CE4E5B9ull;
    value = (value ^ (value >> 27)) * 0x94D049BB133111EBull;
    value ^= value >> 31;
    npc.lastRoll = double(value >> 11) / 9007199254740992.0;
    double sum = 0;
    size_t last = npc.candidates.size();
    for (size_t i = 0; i < npc.candidates.size(); ++i)
    {
        const auto& c = npc.candidates[i];
        if (c.probability > 0)
        {
            last = i;
            sum += c.probability;
            if (sum > npc.lastRoll)
            {
                return i;
            }
        }
    }
    return last;
}
