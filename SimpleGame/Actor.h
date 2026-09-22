#pragma once
#include <cstdint>
#include <string>
#include <vector>

class Renderer;
class SceneGraph;
using ActorId = std::uint64_t;
constexpr ActorId InvalidActor = 0;

struct WorldPoint
{
    double x = 0;
    double y = 0;
};

enum class TickGroup
{
    None,
    Player,
    Projectile,
    Enemy,
    Loot,
    Effect
};

enum class ActorLayer
{
    None,
    Ground,
    World,
    Effects,
    Atmosphere,
    Overlay,
    HUD
};

// A logical object, not an individual drawing primitive. SceneGraph owns its
// lifetime; children inherit translation, activation and visibility.
class Actor
{
public:

    explicit Actor(std::string name = "Actor",
                   TickGroup tick = TickGroup::None,
                   ActorLayer layer = ActorLayer::None);
    virtual ~Actor() = default;
    Actor(const Actor&) = delete;
    Actor& operator=(const Actor&) = delete;

    ActorId Id() const;
    ActorId ParentId() const;
    const std::string& Name() const;
    const std::vector<ActorId>& Children() const;
    WorldPoint LocalPosition() const;
    WorldPoint WorldPosition() const;
    double WorldX() const;
    double WorldY() const;
    void SetLocalPosition(double x, double y);
    void SetWorldPosition(double x, double y);
    void TranslateWorld(double x, double y);
    void SetEnabled(bool enabled);
    void SetVisible(bool visible);
    bool EnabledInHierarchy() const;
    bool VisibleInHierarchy() const;
    void Destroy();

    virtual void Update(float dt);
    virtual void Draw(Renderer& renderer);
    virtual void DrawOverlay(Renderer& renderer);
    virtual bool InView() const;
    virtual bool HasOverlay() const;
    virtual double ColliderRadius() const;

private:

    friend class SceneGraph;
    SceneGraph* m_Scene = nullptr;
    ActorId m_Id = InvalidActor;
    ActorId m_Parent = InvalidActor;
    std::vector<ActorId> m_Children;
    std::string m_Name;
    WorldPoint m_Position;
    mutable WorldPoint m_WorldPosition;
    mutable bool m_WorldDirty = true;
    TickGroup m_Tick;
    ActorLayer m_Layer;
    bool m_Enabled = true;
    bool m_Visible = true;
    bool m_Destroyed = false;
};
