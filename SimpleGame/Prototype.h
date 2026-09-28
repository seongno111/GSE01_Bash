#pragma once
#include "Renderer.h"
#include "GameActors.h"
#include "SceneGraph.h"
#include <cstdint>
#include <map>
#include <string>
#include <vector>

class Prototype
{
public:

    Prototype();
    void Update(float dt);
    void Draw(Renderer& renderer);
    void Key(unsigned char key, bool down);

    // Shared world services for new actor subclasses and future levels.
    SceneGraph& Scene()
    {
        return m_Scene;
    }

    const SceneGraph& Scene() const
    {
        return m_Scene;
    }

    PlayerActor& Player()
    {
        return m_Player;
    }

    const PlayerActor& Player() const
    {
        return m_Player;
    }

    float Zoom() const
    {
        return m_Zoom;
    }

    float Time() const
    {
        return m_Time;
    }

    WorldPoint CameraPosition() const
    {
        return {m_CameraX, m_CameraY};
    }

    bool GridEnabled() const
    {
        return m_Grid;
    }

    size_t LoadedChunkCount() const
    {
        return m_Chunks.size();
    }

    Point Project(double x, double y) const;
    bool Blocked(double x, double y) const;
    bool ClearShot(double fromX, double fromY, double toX, double toY) const;
    void MoveActor(::Actor& actor, double dx, double dy, float radius);

    template <class T = EnemyActor, class... Args> T& SpawnEnemy(Args&&... args)
    {
        return m_Enemies.SpawnAs<T>(*this, std::forward<Args>(args)...);
    }

private:

    friend class GameActor;
    friend class PlayerActor;
    friend class EnemyActor;
    friend class EchoActor;
    friend class ProjectileActor;
    friend class LootActor;
    friend class CombatTextActor;
    friend class TerrainActor;
    friend class PropActor;
    friend class AtmosphereActor;
    friend class RangeIndicatorActor;
    friend class HUDActor;

    void InitializeLevelOne();
    void GenerateLevelOneMap();
    int LevelOneCell(std::int64_t x, std::int64_t y) const;
    void CollectLoot(LootActor& loot);
    void UpdateGameplay(float dt);
    void MoveEnemy(EnemyActor& enemy, double targetX, double targetY, float speed, float dt);
    void SelectTarget();
    bool TargetPosition(double& x, double& y) const;
    void FireProjectile();
    void DefeatEnemy(EnemyActor& enemy);
    void AddExperience(int amount);
    void AddCombatText(double x, double y, const std::string& text, Color color);
    void CompleteLevelOne();
    float MaxHealth() const;
    float ProjectileDamage() const;
    float ProjectileCooldown() const;
    int ExperienceRequired() const;
    void DrawEnemyMesh(Renderer& r, Point p, const EnemyActor& enemy);
    void DrawLevelOneUI(Renderer& r);

    using ChunkKey = std::pair<std::int64_t, std::int64_t>;
    void Stream();
    WorldTileData MakeTile(std::int64_t x, std::int64_t y) const;
    void Move(double& x, double& y, double dx, double dy, float radius);
    void Die();
    void Notice(const std::string& text);
    void Block(Renderer& r, Point p, float width, float depth, float height, Color color);
    void Tree(Renderer& r, Point p, unsigned seed);
    void Chapel(Renderer& r, Point p);
    void Shrine(Renderer& r, Point p);
    void DrawCharacter(Renderer& r, Point p, int corruption, bool hostile);
    void Interface(Renderer& r);
    void DrawTile(Renderer& r, const WorldTileData& tile, Point p);
    void DrawProp(Renderer& r, Point p, int kind, unsigned seed);
    SceneGraph m_Scene;
    ActorId m_WorldRoot;
    ActorId m_GameplayRoot;
    ActorId m_EffectsRoot;
    ActorId m_UIRoot;
    PlayerActor& m_Player;
    std::map<ChunkKey, ActorId> m_Chunks;
    ActorCollection<EchoActor> m_Echoes;
    ActorCollection<EnemyActor> m_Enemies;
    ActorCollection<ProjectileActor> m_Projectiles;
    ActorCollection<ProjectileActor> m_EnemyProjectiles;
    ActorCollection<LootActor> m_Loot;
    ActorCollection<CombatTextActor> m_CombatText;
    int m_FarmKills = 0;
    int m_CollectedItems = 0;
    std::uint64_t m_WorldSeed = 0;
    std::vector<bool> m_LevelWalkable;
    ActorId m_TargetId = InvalidActor;
    ActorId m_GuardianId = InvalidActor;
    bool m_LevelOneStarted = false;
    bool m_GuardianDefeated = false;
    bool m_LevelOneComplete = false;
    bool m_GuardianAnnounced = false;
    bool m_Keys[256] = {};
    double m_CameraX = 2;
    double m_CameraY = 1;
    double m_ShrineX = 0;
    double m_ShrineY = 0;
    float m_Time = 0;
    float m_Zoom = 1.0f;
    float m_MessageTime = 10;
    int m_Width = 1280;
    int m_Height = 800;
    int m_Radius = 3;
    bool m_Grid = false;
    bool m_Help = true;
    std::uint64_t m_NextEcho = 1;
    std::string m_Message =
        "레벨 1 · 버려진 경작지. 동쪽 사냥터에서 SPACE를 누르고 적을 사냥하세요.";
};
