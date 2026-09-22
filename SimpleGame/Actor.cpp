#include "stdafx.h"
#include "Actor.h"
#include "SceneGraph.h"
#include <utility>

Actor::Actor(std::string name, TickGroup tick, ActorLayer layer)
    : m_Name(std::move(name)),
      m_Tick(tick),
      m_Layer(layer)
{
}

ActorId Actor::Id() const
{
    return m_Id;
}

ActorId Actor::ParentId() const
{
    return m_Parent;
}

const std::string& Actor::Name() const
{
    return m_Name;
}

const std::vector<ActorId>& Actor::Children() const
{
    return m_Children;
}

WorldPoint Actor::LocalPosition() const
{
    return m_Position;
}

WorldPoint Actor::WorldPosition() const
{
    if (m_WorldDirty)
    {
        const Actor* parent = m_Scene ? m_Scene->Find(m_Parent) : nullptr;
        const WorldPoint origin = parent ? parent->WorldPosition() : WorldPoint{};
        m_WorldPosition = {origin.x + m_Position.x, origin.y + m_Position.y};
        m_WorldDirty = false;
    }
    return m_WorldPosition;
}

double Actor::WorldX() const
{
    return WorldPosition().x;
}

double Actor::WorldY() const
{
    return WorldPosition().y;
}

void Actor::SetLocalPosition(double x, double y)
{
    if (m_Position.x == x && m_Position.y == y)
    {
        return;
    }
    m_Position = {x, y};
    m_WorldDirty = true;
    if (m_Scene)
    {
        m_Scene->NotifyTransformChanged(m_Id);
    }
}

void Actor::SetWorldPosition(double x, double y)
{
    const Actor* parent = m_Scene ? m_Scene->Find(m_Parent) : nullptr;
    const WorldPoint origin = parent ? parent->WorldPosition() : WorldPoint{};
    SetLocalPosition(x - origin.x, y - origin.y);
}

void Actor::TranslateWorld(double x, double y)
{
    SetLocalPosition(m_Position.x + x, m_Position.y + y);
}

void Actor::SetEnabled(bool enabled)
{
    if (m_Enabled == enabled)
    {
        return;
    }
    m_Enabled = enabled;
    if (m_Scene)
    {
        m_Scene->NotifyTransformChanged(m_Id);
    }
}

void Actor::SetVisible(bool visible)
{
    m_Visible = visible;
}

bool Actor::EnabledInHierarchy() const
{
    const Actor* parent = m_Scene ? m_Scene->Find(m_Parent) : nullptr;
    return !m_Destroyed && m_Enabled && (!parent || parent->EnabledInHierarchy());
}

bool Actor::VisibleInHierarchy() const
{
    const Actor* parent = m_Scene ? m_Scene->Find(m_Parent) : nullptr;
    return !m_Destroyed && m_Visible && (!parent || parent->VisibleInHierarchy());
}

void Actor::Destroy()
{
    if (m_Scene)
    {
        m_Scene->Destroy(m_Id);
    }
}

void Actor::Update(float)
{
}

void Actor::Draw(Renderer&)
{
}

void Actor::DrawOverlay(Renderer&)
{
}

bool Actor::InView() const
{
    return true;
}

bool Actor::HasOverlay() const
{
    return false;
}

double Actor::ColliderRadius() const
{
    return 0;
}
