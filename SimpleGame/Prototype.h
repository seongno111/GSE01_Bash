#pragma once
#include "Renderer.h"
#include "GameActors.h"
#include "SceneGraph.h"
#include "ModelLibrary.h"
#include "WorldNavigation.h"
#include <cstdint>
#include <map>
#include <string>
#include <vector>

class NpcWorld;
class ProgressSave;

class Prototype
{
public:

    Prototype();
    ~Prototype();
    void Update(float dt);
    void Draw(Renderer& renderer);
    void Key(unsigned char key, bool down);
    void WeaponShopClick(float x, float y);
    bool CloseWeaponShop();
    void SaveOnExit();

    void SetRunHeld(bool held)
    {
        m_RunHeld = held;
    }

    // Shared world services for new actor subclasses and future levels.
    SceneGraph& Scene()
    {
        return m_Scene;
    }

    const SceneGraph& Scene() const
    {
        return m_Scene;
    }

    Models::ModelLibrary& ModelAssets()
    {
        return m_ModelAssets;
    }

    const Models::ModelLibrary& ModelAssets() const
    {
        return m_ModelAssets;
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

    struct UpdateStats
    {
        double gameplayMs = 0;
        double npcMs = 0;
        double streamMs = 0;
        double npcExportMs = 0;
        double monitorCaptureMs = 0;
        double monitorWorkerMs = 0;
        double monitorAgeMs = -1;
        std::uint64_t monitorSkipped = 0;
        bool monitorPending = false;
        std::uint64_t pathCalls = 0;
        std::uint64_t pathNodes = 0;
        std::uint64_t pathDeferred = 0;
        std::uint64_t pathCompleted = 0;
        size_t pathPending = 0;
        double pathMs = 0;
    };

    const UpdateStats& UpdatingStats() const
    {
        return m_UpdateStats;
    }

    Point Project(double x, double y) const;
    bool Blocked(double x, double y) const;
    bool ClearShot(double fromX, double fromY, double toX, double toY) const;
    void MoveActor(::Actor& actor, double dx, double dy, float radius);
    enum class RouteStatus
    {
        Pending,
        Found,
        Unreachable
    };

    RouteStatus
    RequestRoute(const ::Actor& actor, WorldPoint target, std::vector<WorldPoint>& path);
    void CancelRoute(ActorId actor);

    template <class T = EnemyActor, class... Args> T& SpawnEnemy(Args&&... args)
    {
        return m_Enemies.SpawnAs<T>(*this, std::forward<Args>(args)...);
    }

private:

    friend class NpcWorld;
    friend class ProgressSave;

    friend class GameActor;
    friend class PlayerActor;
    friend class EnemyActor;
    friend class EchoActor;
    friend class ProjectileActor;
    friend class LootActor;
    friend class CombatTextActor;
    friend class TerrainActor;
    friend class WorldChunkActor;
    friend class PropActor;
    friend class AtmosphereActor;
    friend class RangeIndicatorActor;
    friend class HUDActor;

    bool NearBlacksmith() const;
    void DrawWeaponShop(Renderer& renderer);
    bool m_WeaponShopOpen = false;
    void ProcessNavigation();
    void InitializeLevelOne();
    void GenerateLevelOneMap();
    int LevelOneCell(std::int64_t x, std::int64_t y) const;
    void CollectLoot(LootActor& loot);
    void UpdateGameplay(float dt);
    void MoveEnemy(EnemyActor& enemy, double targetX, double targetY, float speed, float dt);
    void SelectTarget();
    bool TargetPosition(double& x, double& y, ActorId target = InvalidActor) const;
    void FireProjectile();
    void ResolvePlayerAttack();
    void BeginWeaponSkill();
    bool UpdateWeaponSkill();
    void ResolveWeaponSkill();
    void DrawWeaponSkill(Renderer& renderer);
    std::vector<ActorId> WeaponSkillTargets() const;
    void ApplyPlayerHit(::Actor& target, float damage, float stun = 0);
    float WeaponCooldown(int weapon) const;
    void DefeatEnemy(EnemyActor& enemy, bool rewardPlayer = true);
    void AddExperience(int amount);
    void AddCombatText(double x, double y, const std::string& text, Color color);
    void CompleteLevelOne();
    float MaxHealth() const;
    float ProjectileDamage() const;
    float ProjectileCooldown() const;
    int ExperienceRequired() const;
    void DrawEnemyMesh(Renderer& r, Point p, const EnemyActor& enemy);
    void DrawMonsterProxy(Renderer& r, Point p, const EnemyActor& enemy);
    void DrawMonsterIndicators(Renderer& r, Point p, const EnemyActor& enemy);
    void InitializeMonsterEcology();
    bool FindMonsterSpawn(WorldPoint desired, WorldPoint& result) const;
    void DrawLevelOneUI(Renderer& r);

    using ChunkKey = std::pair<std::int64_t, std::int64_t>;
    void Stream();
    WorldTileData MakeTile(std::int64_t x, std::int64_t y) const;
    bool CanOccupy(double x, double y, double radius) const;
    bool ClearMovement(WorldPoint from, WorldPoint to, double radius) const;
    void Move(double& x, double& y, double dx, double dy, float radius);
    void Die(const ::Actor* killer = nullptr, bool unlawful = false);
    void Notice(const std::string& text);
    void Block(Renderer& r, Point p, float width, float depth, float height, Color color);
    void Tree(Renderer& r, Point p, unsigned seed);
    void Chapel(Renderer& r, Point p);
    void Shrine(Renderer& r, Point p);
    void
    DrawCharacter(Renderer& r, Point p, int corruption, bool hostile, const UnitSpritePose& pose);
    void Interface(Renderer& r);
    void DrawTile(Renderer& r, const WorldTileData& tile, Point p);
    void DrawProp(Renderer& r, Point p, int kind, unsigned seed);
    Models::ModelLibrary m_ModelAssets;
    std::unique_ptr<Terrain::Sampler> m_Terrain;
    SceneGraph m_Scene;
    ActorId m_WorldRoot;
    ActorId m_GameplayRoot;
    ActorId m_EffectsRoot;
    ActorId m_UIRoot;
    PlayerActor& m_Player;
    UpdateStats m_UpdateStats;
    NavigationState m_Navigation;
    std::map<ChunkKey, ActorId> m_Chunks;
    ChunkKey m_StreamCenter{};
    int m_StreamRadius = -1;
    ActorCollection<EchoActor> m_Echoes;
    ActorCollection<EnemyActor> m_Enemies;
    ActorCollection<ProjectileActor> m_Projectiles;
    ActorCollection<ProjectileActor> m_EnemyProjectiles;
    ActorCollection<LootActor> m_Loot;
    ActorCollection<CombatTextActor> m_CombatText;
    std::unique_ptr<NpcWorld> m_NpcWorld;
    std::unique_ptr<ProgressSave> m_ProgressSave;
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
    bool m_RunHeld = false;
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
    bool m_Help = false;
    std::uint64_t m_NextEcho = 1;
    std::string m_Message =
        "성당 마을 · C로 주민 선택, Y 거래, O 축복, I 상태 확인. J는 PK 전환입니다.";
};
