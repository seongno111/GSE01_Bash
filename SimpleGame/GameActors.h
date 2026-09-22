#pragma once
#include "Actor.h"
#include "Renderer.h"

class Prototype;

struct WorldTileData
{
    std::int64_t x;
    std::int64_t y;
    float taint;
    unsigned variant;
    int prop;
    bool road;
};

enum class LootType
{
    Upgrade,
    Healing,
    Essence,
    Purification,
    Magnet
};

class GameActor : public Actor
{
public:

    GameActor(Prototype& game, const std::string& name, TickGroup tick, ActorLayer layer);
    bool InView() const override;

protected:

    Prototype& m_Game;
};

class PlayerActor final : public GameActor
{
public:

    explicit PlayerActor(Prototype& game);
    void Update(float dt) override;
    void Draw(Renderer& renderer) override;
    int level = 1;
    int experience = 0;
    int shards = 0;
    int weaponRank = 0;
    int purificationStones = 0;
    int potions = 2;
    int corruption = 0;
    int deaths = 0;
    int relics = 1;
    float magnetTime = 0;
    float shotCooldown = 0;
    float range = 6;
    float health = 100;
    float hitCooldown = 0;
    float attack = 0;
    bool autoFire = false;
    bool walking = false;
};

class EnemyActor : public GameActor
{
public:

    explicit EnemyActor(Prototype& game, int tier = 0);
    void Update(float dt) override;
    void Draw(Renderer& renderer) override;
    void DrawOverlay(Renderer& renderer) override;

    bool HasOverlay() const override
    {
        return true;
    }

    double homeX = 0;
    double homeY = 0;
    float health = 0;
    float maxHealth = 0;
    float damage = 0;
    float speed = 0;
    float respawn = 0;
    float attackWindup = 0;
    float recovery = 0;
    int experience = 0;
    int tier = 0;
    bool alive = true;
    float burstCooldown = 5;
    float burstWindup = 0;
};

class EchoActor final : public GameActor
{
public:

    EchoActor(Prototype& game,
              std::uint64_t number,
              double x,
              double y,
              int corruption,
              float health,
              bool alive,
              float maxHealth,
              float damage,
              int level);
    void Update(float dt) override;
    void Draw(Renderer& renderer) override;
    void DrawOverlay(Renderer& renderer) override;

    bool HasOverlay() const override
    {
        return true;
    }

    std::uint64_t id;
    int corruption;
    float health;
    bool alive;
    float maxHealth;
    float damage;
    int level;
};

class ProjectileActor final : public GameActor
{
public:

    ProjectileActor(Prototype& game,
                    double x,
                    double y,
                    double directionX,
                    double directionY,
                    float damage,
                    float remaining,
                    bool hostile = false);
    void Update(float dt) override;
    void Draw(Renderer& renderer) override;
    double directionX;
    double directionY;
    float damage;
    float remaining;
    bool alive = true;
    bool hostile;
};

class LootActor final : public GameActor
{
public:

    LootActor(Prototype& game, double x, double y, int amount, LootType type);
    void Update(float dt) override;
    void Draw(Renderer& renderer) override;
    int amount;
    LootType type;
    float remaining = 60;
};

class CombatTextActor final : public GameActor
{
public:

    CombatTextActor(Prototype& game, double x, double y, std::string text, Color color);
    void Update(float dt) override;
    void Draw(Renderer& renderer) override;
    std::string text;
    Color color;
    float remaining = 1.1f;
};

class TerrainActor final : public GameActor
{
public:

    TerrainActor(Prototype& game, WorldTileData tile);
    void Draw(Renderer& renderer) override;

private:

    WorldTileData m_Tile;
};

class PropActor final : public GameActor
{
public:

    PropActor(Prototype& game, int kind, unsigned seed = 0);
    void Draw(Renderer& renderer) override;
    double ColliderRadius() const override;

private:

    int m_Kind;
    unsigned m_Seed;
};

class AtmosphereActor final : public GameActor
{
public:

    AtmosphereActor(Prototype& game, bool ash, int index);
    void Draw(Renderer& renderer) override;

    bool InView() const override
    {
        return true;
    }

private:

    bool m_Ash;
    int m_Index;
};

class RangeIndicatorActor final : public GameActor
{
public:

    explicit RangeIndicatorActor(Prototype& game);
    void Draw(Renderer& renderer) override;
};

class HUDActor final : public GameActor
{
public:

    explicit HUDActor(Prototype& game);
    void Draw(Renderer& renderer) override;

    bool InView() const override
    {
        return true;
    }
};
