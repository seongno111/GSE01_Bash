#include "stdafx.h"
#include "Prototype.h"
#include "GameRules.h"

#include <algorithm>
#include <cmath>

using namespace GameRules;

void Prototype::CollectLoot(LootActor& loot)
{
    ++m_CollectedItems;
    switch (loot.type)
    {
    case LootType::Upgrade:
        m_Player.shards += loot.amount;
        AddCombatText(m_Player.WorldX(),
                      m_Player.WorldY(),
                      "강화 파편 +" + std::to_string(loot.amount),
                      DamageColor);
        break;
    case LootType::Healing:
        m_Player.health = (std::min)(MaxHealth(), m_Player.health + MaxHealth() * 0.35f);
        AddCombatText(m_Player.WorldX(),
                      m_Player.WorldY(),
                      "생명력 회복",
                      Color(0.95f, 0.40f, 0.38f));
        break;
    case LootType::Essence:
        AddExperience(loot.amount);
        AddCombatText(m_Player.WorldX(),
                      m_Player.WorldY(),
                      "불의 정수 +" + std::to_string(loot.amount) + " 경험치",
                      DamageColor);
        break;
    case LootType::Purification:
        m_Player.purificationStones += loot.amount;
        AddCombatText(m_Player.WorldX(),
                      m_Player.WorldY(),
                      "정화석 +1 [T]",
                      Color(0.48f, 0.81f, 1));
        break;
    case LootType::Magnet:
        m_Player.magnetTime = 15;
        AddCombatText(m_Player.WorldX(), m_Player.WorldY(), "자석 효과 15초", ExperienceColor);
        break;
    }
}

float Prototype::MaxHealth() const
{
    return 100.0f + 15.0f * (m_Player.level - 1);
}

float Prototype::ProjectileDamage() const
{
    return 24.0f + 5.0f * (m_Player.level - 1) + 4.0f * m_Player.weaponRank
           + 0.3f * m_Player.corruption;
}

float Prototype::ProjectileCooldown() const
{
    return (std::max)(0.22f, 0.90f * std::pow(0.94f, float(m_Player.level - 1)));
}

int Prototype::ExperienceRequired() const
{
    return 40 + 25 * (m_Player.level - 1);
}

void Prototype::AddCombatText(double x, double y, const std::string& text, Color color)
{
    m_CombatText.Spawn(*this, x, y, text, color);
}

void Prototype::AddExperience(int amount)
{
    if (m_Player.level >= MaximumLevel)
    {
        return;
    }

    m_Player.experience += amount;
    while (m_Player.level < MaximumLevel && m_Player.experience >= ExperienceRequired())
    {
        m_Player.experience -= ExperienceRequired();
        ++m_Player.level;
        m_Player.health = (std::min)(MaxHealth(), m_Player.health + 15.0f + MaxHealth() * 0.20f);
        m_Player.shotCooldown = (std::min)(m_Player.shotCooldown, ProjectileCooldown());
        Notice("레벨 " + std::to_string(m_Player.level)
               + " 달성! 최대 체력·발사체 피해 증가, 발사 대기시간 감소.");
        AddCombatText(m_Player.WorldX(), m_Player.WorldY(), "레벨 상승!", ExperienceColor);
    }

    if (m_Player.level == MaximumLevel)
    {
        m_Player.experience = 0;
    }
}
