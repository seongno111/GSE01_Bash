#include "stdafx.h"
#include "MonsterDefinitions.h"

const std::array<MonsterDefinition, static_cast<size_t>(MonsterKind::Count)> MonsterCatalog = {
    {{MonsterKind::WitheredHollow,
      "withered_hollow",
      "메마른 망자",
      "오염된 인간",
      "WitheredHollow",
      45,
      8,
      1.15f,
      20,
      0,
      .40f,
      8,
      7,
      .90f,
      .50f,
      1.15f,
      0,
      0,
      0,
      0,
      0,
      25,
      68,
      MonsterAbility::None,
      false},
     {MonsterKind::TaintedRaider,
      "tainted_raider",
      "오염된 약탈자",
      "오염된 인간",
      "TaintedRaider",
      75,
      12,
      1.50f,
      30,
      0,
      .40f,
      8,
      7,
      .90f,
      .50f,
      1.15f,
      0,
      0,
      0,
      0,
      0,
      25,
      76,
      MonsterAbility::None,
      false},
     {MonsterKind::FieldWarden,
      "field_warden",
      "경작지 수문장",
      "오염된 인간",
      "FieldWarden",
      380,
      20,
      1.20f,
      80,
      0,
      .58f,
      8,
      7,
      .90f,
      .65f,
      1.15f,
      .90f,
      4.5f,
      8,
      0,
      14,
      45,
      112,
      MonsterAbility::None,
      true},
     {MonsterKind::AshWolf,
      "ash_wolf",
      "잿니 늑대",
      "오염된 야수",
      "AshWolf",
      58,
      9,
      2.65f,
      26,
      3,
      .38f,
      9,
      13,
      1.0f,
      .35f,
      .75f,
      .45f,
      5,
      4,
      .8f,
      14,
      28,
      44,
      MonsterAbility::Pounce,
      false},
     {MonsterKind::BlightBear,
      "blight_bear",
      "역병 등곰",
      "오염된 야수",
      "BlightBear",
      220,
      20,
      1.35f,
      64,
      18,
      .66f,
      7,
      10,
      1.4f,
      .80f,
      1.4f,
      .95f,
      7,
      2.6f,
      2.2f,
      28,
      45,
      70,
      MonsterAbility::Slam,
      false},
     {MonsterKind::ThornStag,
      "thorn_stag",
      "가시관 사슴",
      "오염된 야수",
      "ThornStag",
      98,
      12,
      2.25f,
      38,
      6,
      .46f,
      8,
      14,
      1.15f,
      .55f,
      1.1f,
      .90f,
      6,
      6,
      1.0f,
      24,
      32,
      92,
      MonsterAbility::Charge,
      false},
     {MonsterKind::HighDemon,
      "high_demon",
      "검은 성화의 대공 아자르",
      "고위 악마",
      "HighDemonAzar",
      680,
      24,
      1.30f,
      180,
      28,
      .78f,
      12,
      18,
      7.5f,
      1.1f,
      1.6f,
      1.4f,
      10,
      9,
      2.8f,
      34,
      90,
      140,
      MonsterAbility::Rift,
      true},
     {MonsterKind::FallenPaladin,
      "fallen_paladin",
      "서약을 삼킨 성기사",
      "타락한 성직자",
      "FallenPaladin",
      185,
      18,
      1.65f,
      62,
      32,
      .46f,
      9,
      13,
      1.45f,
      .70f,
      1.15f,
      .60f,
      10,
      4,
      0,
      0,
      45,
      100,
      MonsterAbility::Ward,
      false},
     {MonsterKind::FallenPriest,
      "fallen_priest",
      "잿빛 고해 사제",
      "타락한 성직자",
      "FallenPriest",
      112,
      11,
      1.45f,
      48,
      8,
      .40f,
      10,
      14,
      6.5f,
      .85f,
      1.25f,
      1.2f,
      8,
      7,
      0,
      28,
      40,
      94,
      MonsterAbility::Mend,
      false}}};

const MonsterDefinition& MonsterStats(MonsterKind kind)
{
    const auto index = static_cast<size_t>(kind);
    return MonsterCatalog[index < MonsterCatalog.size() ? index : 0];
}

const char* MonsterStateName(MonsterState state)
{
    switch (state)
    {
    case MonsterState::Chase:
        return "추격";
    case MonsterState::Flank:
        return "측면 협공";
    case MonsterState::Windup:
        return "공격 예고";
    case MonsterState::Charging:
        return "돌진";
    case MonsterState::Guarding:
        return "타락한 방벽";
    case MonsterState::Recovering:
        return "공격 회복";
    case MonsterState::Fleeing:
        return "거리 확보";
    case MonsterState::Returning:
        return "영역 복귀";
    case MonsterState::Supporting:
        return "오염 봉합 시전";
    default:
        return "배회";
    }
}

const char* MonsterAbilityName(MonsterAbility ability)
{
    switch (ability)
    {
    case MonsterAbility::Pounce:
        return "잿니 도약";
    case MonsterAbility::Slam:
        return "역병 내려찍기";
    case MonsterAbility::Charge:
        return "가시관 돌진";
    case MonsterAbility::Rift:
        return "검은 성화 균열";
    case MonsterAbility::Ward:
        return "타락한 방벽";
    case MonsterAbility::Mend:
        return "오염 봉합";
    default:
        return "없음";
    }
}

MonsterKind OutpostMonster(size_t outpost, int slot)
{
    using K = MonsterKind;
    if (outpost == 1 && slot == 0)
    {
        return K::HighDemon;
    }
    const std::array<std::array<K, 12>, 5> rosters = {{{{K::FallenPaladin,
                                                         K::FallenPriest,
                                                         K::TaintedRaider,
                                                         K::WitheredHollow,
                                                         K::AshWolf,
                                                         K::WitheredHollow,
                                                         K::TaintedRaider,
                                                         K::AshWolf,
                                                         K::WitheredHollow,
                                                         K::WitheredHollow,
                                                         K::TaintedRaider,
                                                         K::WitheredHollow}},
                                                       {{K::WitheredHollow,
                                                         K::FallenPriest,
                                                         K::WitheredHollow,
                                                         K::TaintedRaider,
                                                         K::WitheredHollow,
                                                         K::AshWolf,
                                                         K::FallenPaladin,
                                                         K::WitheredHollow,
                                                         K::TaintedRaider,
                                                         K::WitheredHollow,
                                                         K::AshWolf,
                                                         K::WitheredHollow}},
                                                       {{K::BlightBear,
                                                         K::ThornStag,
                                                         K::AshWolf,
                                                         K::AshWolf,
                                                         K::WitheredHollow,
                                                         K::AshWolf,
                                                         K::TaintedRaider,
                                                         K::AshWolf,
                                                         K::ThornStag,
                                                         K::WitheredHollow,
                                                         K::AshWolf,
                                                         K::WitheredHollow}},
                                                       {{K::FallenPaladin,
                                                         K::BlightBear,
                                                         K::TaintedRaider,
                                                         K::FallenPriest,
                                                         K::WitheredHollow,
                                                         K::TaintedRaider,
                                                         K::WitheredHollow,
                                                         K::AshWolf,
                                                         K::TaintedRaider,
                                                         K::WitheredHollow,
                                                         K::WitheredHollow,
                                                         K::WitheredHollow}},
                                                       {{K::ThornStag,
                                                         K::AshWolf,
                                                         K::BlightBear,
                                                         K::AshWolf,
                                                         K::AshWolf,
                                                         K::WitheredHollow,
                                                         K::AshWolf,
                                                         K::TaintedRaider,
                                                         K::AshWolf,
                                                         K::WitheredHollow,
                                                         K::AshWolf,
                                                         K::WitheredHollow}}}};
    return rosters[outpost % rosters.size()][static_cast<size_t>(slot) % 12];
}
