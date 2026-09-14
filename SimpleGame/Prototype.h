#pragma once
#include "Renderer.h"
#include <cstdint>
#include <map>
#include <string>
#include <vector>

class Prototype
{
public:

    void Update(float dt);
    void Draw(Renderer& renderer);
    void Key(unsigned char key, bool down);

private:

    struct Tile
    {
        std::int64_t x;
        std::int64_t y;
        float taint;
        unsigned variant;
        int prop;
        bool road;
    };

    struct Echo
    {
        std::uint64_t id;
        double x;
        double y;
        int corruption;
        float health;
        bool alive;
        float maxHealth;
        float damage;
        int level;
    };

    struct FarmEnemy
    {
        double x;
        double y;
        double homeX;
        double homeY;
        float health;
        float maxHealth;
        float damage;
        float speed;
        float respawn = 0;
        float attackWindup = 0;
        float recovery = 0;
        int experience;
        int tier;
        bool alive = true;
        float burstCooldown = 5;
        float burstWindup = 0;
    };

    struct Projectile
    {
        double x;
        double y;
        double directionX;
        double directionY;
        float damage;
        float remaining;
        bool alive = true;
    };

    enum class LootType
    {
        Upgrade,
        Healing,
        Essence,
        Purification,
        Magnet
    };

    struct Loot
    {
        double x;
        double y;
        int amount;
        LootType type;
        float remaining = 60;
    };

    struct CombatText
    {
        double x;
        double y;
        std::string text;
        Color color;
        float remaining = 1.1f;
    };

    void InitializeLevelOne();
    void GenerateLevelOneMap();
    int LevelOneCell(std::int64_t x, std::int64_t y) const;
    void CollectLoot(Loot& loot);
    void UpdateLevelOne(float dt);
    void UpdateEnemies(float dt);
    void MoveFarmEnemy(FarmEnemy& enemy, double targetX, double targetY, float speed, float dt);
    void UpdateProjectiles(float dt);
    void SelectTarget();
    bool ClearShot(double fromX, double fromY, double toX, double toY) const;
    bool TargetPosition(double& x, double& y) const;
    void FireProjectile();
    void DefeatEnemy(FarmEnemy& enemy);
    void AddExperience(int amount);
    void AddCombatText(double x, double y, const std::string& text, Color color);
    void CompleteLevelOne();
    float MaxHealth() const;
    float ProjectileDamage() const;
    float ProjectileCooldown() const;
    int ExperienceRequired() const;
    void DrawFarmEnemy(Renderer& r, Point p, const FarmEnemy& enemy);
    void DrawCombatEffects(Renderer& r);
    void DrawLevelOneUI(Renderer& r);

    using ChunkKey = std::pair<std::int64_t, std::int64_t>;
    void Stream();
    Tile MakeTile(std::int64_t x, std::int64_t y) const;
    bool Blocked(double x, double y) const;
    void Move(double& x, double& y, double dx, double dy, float radius);
    void Die();
    void Notice(const std::string& text);
    Point Project(double x, double y) const;
    void Block(Renderer& r, Point p, float width, float depth, float height, Color color);
    void Tree(Renderer& r, Point p, unsigned seed);
    void Chapel(Renderer& r, Point p);
    void Shrine(Renderer& r, Point p);
    void Actor(Renderer& r, Point p, int corruption, bool hostile);
    void Interface(Renderer& r);
    std::map<ChunkKey, std::vector<Tile>> m_Chunks;
    std::vector<Echo> m_Echoes;
    std::vector<FarmEnemy> m_FarmEnemies;
    std::vector<Projectile> m_Projectiles;
    std::vector<Projectile> m_EnemyProjectiles;
    std::vector<Loot> m_Loot;
    std::vector<CombatText> m_CombatText;
    int m_Level = 1;
    int m_Experience = 0;
    int m_FarmKills = 0;
    int m_Shards = 0;
    int m_CollectedItems = 0;
    int m_WeaponRank = 0;
    int m_PurificationStones = 0;
    float m_MagnetTime = 0;
    std::uint64_t m_WorldSeed = 0;
    std::vector<bool> m_LevelWalkable;
    int m_Potions = 2;
    int m_TargetKind = 0;
    size_t m_TargetIndex = 0;
    float m_ShotCooldown = 0;
    float m_Range = 6.0f;
    bool m_AutoFire = false;
    bool m_LevelOneStarted = false;
    bool m_GuardianDefeated = false;
    bool m_LevelOneComplete = false;
    bool m_GuardianAnnounced = false;
    bool m_Keys[256] = {};
    double m_X = 2;
    double m_Y = 1;
    double m_CameraX = 2;
    double m_CameraY = 1;
    double m_ShrineX = 0;
    double m_ShrineY = 0;
    float m_Time = 0;
    float m_Zoom = 1.0f;
    float m_Health = 100;
    float m_HitCooldown = 0;
    float m_Attack = 0;
    float m_MessageTime = 10;
    int m_Corruption = 0;
    int m_Deaths = 0;
    int m_Relics = 1;
    int m_Width = 1280;
    int m_Height = 800;
    int m_Radius = 3;
    bool m_Grid = false;
    bool m_Help = true;
    bool m_Walking = false;
    std::uint64_t m_NextEcho = 1;
    std::string m_Message =
        "레벨 1 · 버려진 경작지. 동쪽 사냥터에서 SPACE를 누르고 적을 사냥하세요.";
};
