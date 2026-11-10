#pragma once
#include <array>
#include <algorithm>
#include <cmath>

struct HeroWeapon
{
    const char* key;
    const char* name;
    bool ranged;
    float damage;
    float cooldown;
    float range;

    float Damage(int level, int rank, int corruption) const
    {
        return damage + 5.0f * (level - 1) + 4.0f * rank + .3f * corruption;
    }

    float Cooldown(int level) const
    {
        return (std::max)(cooldown * .25f, cooldown * std::pow(.94f, float(level - 1)));
    }
};

inline constexpr std::array<HeroWeapon, 7> HeroWeapons = {
    {{"WPN_Staff_00_CinderCrook", "재의 순례 지팡이", true, 24, .90f, 6},
     {"WPN_Bow_00_ThornRecurve", "가시 사냥 활", true, 20, .65f, 9},
     {"WPN_Mace_00_FlangedIron", "흑철 날개 철퇴", false, 38, .95f, 1.6f},
     {"WPN_Axe_00_Riven", "갈라진 전투 도끼", false, 44, 1.10f, 1.9f},
     {"WPN_Hammer_00_Warden", "수문장 전투 망치", false, 65, 1.55f, 1.8f},
     {"WPN_Spear_00_AshPike", "잿빛 장창", false, 31, .85f, 3.0f},
     {"WPN_Dagger_00_Rondel", "황동 가드 단검", false, 15, .32f, 1.2f}}};
