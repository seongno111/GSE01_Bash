#pragma once

#include <array>
#include <cstddef>

// Stable identities for saves, monitoring and future model/catalog bindings.
enum class MonsterKind
{
    WitheredHollow = 0,
    TaintedRaider = 1,
    FieldWarden = 2,
    AshWolf = 3,
    BlightBear = 4,
    ThornStag = 5,
    HighDemon = 6,
    FallenPaladin = 7,
    FallenPriest = 8,
    Count
};

enum class MonsterAbility
{
    None,
    Pounce,
    Slam,
    Charge,
    Rift,
    Ward,
    Mend
};

enum class MonsterState
{
    Patrol,
    Chase,
    Flank,
    Windup,
    Charging,
    Guarding,
    Recovering,
    Fleeing,
    Returning,
    Supporting
};

struct MonsterDefinition
{
    MonsterKind kind;
    const char* id;
    const char* name;
    const char* family;
    const char* modelKey;
    float health;
    float damage;
    float moveSpeed;
    int experience;
    float armor;
    float hitRadius;
    float detectionRange;
    float leashRange;
    float attackRange;
    float attackWindup;
    float attackRecovery;
    float specialWindup;
    float specialCooldown;
    float specialRange;
    float specialRadius;
    float specialDamage;
    float respawnSeconds;
    float visualHeight;
    MonsterAbility ability;
    bool boss;
};

extern const std::array<MonsterDefinition, static_cast<size_t>(MonsterKind::Count)> MonsterCatalog;
const MonsterDefinition& MonsterStats(MonsterKind kind);
const char* MonsterStateName(MonsterState state);
const char* MonsterAbilityName(MonsterAbility ability);
MonsterKind OutpostMonster(size_t outpost, int slot);
