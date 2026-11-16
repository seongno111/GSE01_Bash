#pragma once
#include <array>
#include <algorithm>
#include <cmath>

enum class HeroSkillKind
{
    AshField,
    ArrowFan,
    GuardBreak,
    Execution,
    GroundSlam,
    LongThrust,
    Flurry,
    Whirlwind
};

struct HeroSkill
{
    HeroSkillKind kind;
    const char* name;
    const char* clip;
    float cooldown;
    float duration;
    float firstHit;
    float hitInterval;
    int hits;
    float range;
    float width; // Ground radius, cone half-angle in degrees, or thrust half-width.
    float damageScale;
};

inline constexpr char HeroLongswordKey[] = "HERO_Longsword";

struct HeroWeapon
{
    const char* key;
    const char* name;
    bool ranged;
    float damage;
    float cooldown;
    float range;
    HeroSkill skill;

    float Damage(int level, int rank, int corruption) const
    {
        return damage + 5.0f * (level - 1) + 4.0f * rank + .3f * corruption;
    }

    float Cooldown(int level) const
    {
        return (std::max)(cooldown * .25f, cooldown * std::pow(.94f, float(level - 1)));
    }
};

inline constexpr std::array<HeroWeapon, 8> HeroWeapons = {{{"WPN_Staff_00_CinderCrook",
                                                            "재의 순례 지팡이",
                                                            true,
                                                            24,
                                                            .90f,
                                                            6,
                                                            {HeroSkillKind::AshField,
                                                             "잔불의 문양",
                                                             "HERO_Staff_SkillAshField",
                                                             9,
                                                             1.40f,
                                                             .45f,
                                                             .35f,
                                                             3,
                                                             6,
                                                             2.2f,
                                                             .65f}},
                                                           {"WPN_Bow_00_ThornRecurve",
                                                            "가시 사냥 활",
                                                            true,
                                                            20,
                                                            .65f,
                                                            9,
                                                            {HeroSkillKind::ArrowFan,
                                                             "가시 부채살",
                                                             "HERO_Bow_SkillArrowFan",
                                                             8,
                                                             .95f,
                                                             .55f,
                                                             0,
                                                             1,
                                                             10,
                                                             24,
                                                             .80f}},
                                                           {"WPN_Mace_00_FlangedIron",
                                                            "흑철 날개 철퇴",
                                                            false,
                                                            38,
                                                            .95f,
                                                            1.6f,
                                                            {HeroSkillKind::GuardBreak,
                                                             "방패 분쇄",
                                                             "HERO_Mace_SkillGuardBreak",
                                                             7,
                                                             .82f,
                                                             .38f,
                                                             0,
                                                             1,
                                                             2.2f,
                                                             28,
                                                             1.35f}},
                                                           {"WPN_Axe_00_Riven",
                                                            "갈라진 전투 도끼",
                                                            false,
                                                            44,
                                                            1.10f,
                                                            1.9f,
                                                            {HeroSkillKind::Execution,
                                                             "사형 집행",
                                                             "HERO_Axe_SkillExecution",
                                                             9,
                                                             1.05f,
                                                             .55f,
                                                             0,
                                                             1,
                                                             3,
                                                             85,
                                                             1.45f}},
                                                           {"WPN_Hammer_00_Warden",
                                                            "수문장 전투 망치",
                                                            false,
                                                            65,
                                                            1.55f,
                                                            1.8f,
                                                            {HeroSkillKind::GroundSlam,
                                                             "대지 강타",
                                                             "HERO_Hammer_SkillGroundSlam",
                                                             12,
                                                             1.35f,
                                                             .72f,
                                                             0,
                                                             1,
                                                             4.5f,
                                                             60,
                                                             .85f}},
                                                           {"WPN_Spear_00_AshPike",
                                                            "잿빛 장창",
                                                            false,
                                                            31,
                                                            .85f,
                                                            3.0f,
                                                            {HeroSkillKind::LongThrust,
                                                             "잿빛 관통",
                                                             "HERO_Spear_SkillLongThrust",
                                                             7,
                                                             .86f,
                                                             .40f,
                                                             0,
                                                             1,
                                                             5.5f,
                                                             .45f,
                                                             1.6f}},
                                                           {"WPN_Dagger_00_Rondel",
                                                            "황동 가드 단검",
                                                            false,
                                                            15,
                                                            .32f,
                                                            1.2f,
                                                            {HeroSkillKind::Flurry,
                                                             "세 번의 상흔",
                                                             "HERO_Dagger_SkillFlurry",
                                                             5,
                                                             .67f,
                                                             .14f,
                                                             .13f,
                                                             3,
                                                             1.65f,
                                                             0,
                                                             1.05f}},
                                                           {HeroLongswordKey,
                                                            "순례자의 장검",
                                                            false,
                                                            34,
                                                            .78f,
                                                            2.0f,
                                                            {HeroSkillKind::Whirlwind,
                                                             "잿바람 회전베기",
                                                             "HERO_Sword_SkillWhirlwind",
                                                             8,
                                                             1.12f,
                                                             .35f,
                                                             .40f,
                                                             2,
                                                             2.65f,
                                                             0,
                                                             .9f}}}};
