#include "stdafx.h"
#include "SceneGraph.h"
#include <algorithm>
#include <stdexcept>
#include <cmath>

namespace
{
    struct TraversalScope
    {
        unsigned& depth;

        explicit TraversalScope(unsigned& value)
            : depth(value)
        {
            ++depth;
        }

        ~TraversalScope()
        {
            --depth;
        }
    };
} // namespace

SceneGraph::SceneGraph()
{
    auto root = std::make_unique<Actor>("Scene");
    root->m_Id = m_NextId++;
    root->m_Scene = this;
    m_Root = root->Id();
    m_Actors.emplace(m_Root, std::move(root));
}

ActorId SceneGraph::Root() const
{
    return m_Root;
}

Actor* SceneGraph::Find(ActorId id)
{
    return const_cast<Actor*>(static_cast<const SceneGraph&>(*this).Find(id));
}

const Actor* SceneGraph::Find(ActorId id) const
{
    const auto it = m_Actors.find(id);
    return it == m_Actors.end() || it->second->m_Destroyed ? nullptr : it->second.get();
}

void SceneGraph::Adopt(ActorId parentId, std::unique_ptr<Actor> actor)
{
    Actor* parent = Find(parentId);
    if (!parent)
    {
        throw std::invalid_argument("Actor parent is not in this scene");
    }
    actor->m_Id = m_NextId++;
    actor->m_Parent = parentId;
    actor->m_Scene = this;
    actor->m_WorldDirty = true;
    const ActorId id = actor->Id();
    if (actor->m_Tick != TickGroup::None)
    {
        m_TickActors[static_cast<size_t>(actor->m_Tick)].push_back(id);
    }
    if (actor->m_Layer != ActorLayer::None)
    {
        m_DrawActors.push_back(id);
    }
    if (actor->ColliderRadius() > 0)
    {
        m_CollisionDirty = true;
    }
    m_Actors.emplace(id, std::move(actor));
    parent->m_Children.push_back(id);
}

bool SceneGraph::Reparent(ActorId id, ActorId parentId, bool keepWorldPosition)
{
    Actor* actor = Find(id);
    Actor* parent = Find(parentId);
    if (!actor || !parent || id == m_Root)
    {
        return false;
    }
    for (const Actor* ancestor = parent; ancestor; ancestor = Find(ancestor->ParentId()))
    {
        if (ancestor == actor)
        {
            return false;
        }
    }
    if (actor->ParentId() == parentId)
    {
        return true;
    }
    const WorldPoint world = actor->WorldPosition();
    auto& siblings = Find(actor->ParentId())->m_Children;
    siblings.erase(std::remove(siblings.begin(), siblings.end(), id), siblings.end());
    parent->m_Children.push_back(id);
    actor->m_Parent = parentId;
    if (keepWorldPosition)
    {
        actor->SetWorldPosition(world.x, world.y);
    }
    NotifyTransformChanged(id);
    return true;
}

void SceneGraph::Destroy(ActorId id)
{
    Actor* actor = Find(id);
    if (!actor || id == m_Root)
    {
        return;
    }
    for (ActorId child : actor->m_Children)
    {
        Destroy(child);
    }
    if (actor->ColliderRadius() > 0)
    {
        m_CollisionDirty = true;
    }
    actor->m_Destroyed = true;
    m_DestroyedActors.push_back(id);
}

void SceneGraph::CollectGarbage()
{
    if (m_TraversalDepth != 0 || m_DestroyedActors.empty())
    {
        return;
    }
    const auto removeMissing = [this](std::vector<ActorId>& ids)
    {
        ids.erase(std::remove_if(ids.begin(),
                                 ids.end(),
                                 [this](ActorId id)
                                 {
                                     return !Find(id);
                                 }),
                  ids.end());
    };
    for (auto& ids : m_TickActors)
    {
        removeMissing(ids);
    }
    removeMissing(m_DrawActors);
    for (auto& entry : m_Members)
    {
        removeMissing(entry.second);
    }
    for (ActorId id : m_DestroyedActors)
    {
        const auto actor = m_Actors.find(id);
        if (actor == m_Actors.end())
        {
            continue;
        }
        const auto parent = m_Actors.find(actor->second->ParentId());
        if (parent != m_Actors.end())
        {
            auto& children = parent->second->m_Children;
            children.erase(std::remove(children.begin(), children.end(), id), children.end());
        }
        m_Actors.erase(actor);
        m_Members.erase(id);
    }
    m_DestroyedActors.clear();
}

void SceneGraph::Update(float dt, TickGroup group)
{
    TraversalScope scope(m_TraversalDepth);
    const std::vector<ActorId> actors = m_TickActors[static_cast<size_t>(group)];
    // Each phase uses a stable snapshot: spawned actors join the next phase;
    // actors destroyed by a callback are skipped without invalidating iteration.
    for (ActorId id : actors)
    {
        Actor* actor = Find(id);
        if (actor && actor->EnabledInHierarchy())
        {
            actor->Update(dt);
        }
    }
}

std::vector<ActorId> SceneGraph::DrawList(bool ui) const
{
    struct Entry
    {
        ActorId id;
        ActorLayer layer;
        double depth;
        double x;
    };

    std::vector<Entry> entries;
    for (ActorId id : m_DrawActors)
    {
        const Actor* actor = Find(id);
        if (!actor)
        {
            continue;
        }
        const bool screen = actor->m_Layer >= ActorLayer::Overlay;
        if ((ui ? screen || actor->HasOverlay() : !screen) && actor->VisibleInHierarchy()
            && actor->InView())
        {
            const WorldPoint point = actor->WorldPosition();
            entries.push_back({id, actor->m_Layer, point.x + point.y, point.x});
        }
    }
    std::sort(entries.begin(),
              entries.end(),
              [](const Entry& a, const Entry& b)
              {
                  if (a.layer != b.layer)
                  {
                      return a.layer < b.layer;
                  }
                  if (a.layer <= ActorLayer::Effects && a.depth != b.depth)
                  {
                      return a.depth < b.depth;
                  }
                  if (a.layer == ActorLayer::Ground && a.x != b.x)
                  {
                      return a.x < b.x;
                  }
                  return a.id < b.id;
              });
    std::vector<ActorId> actors;
    actors.reserve(entries.size());
    for (const Entry& entry : entries)
    {
        actors.push_back(entry.id);
    }
    return actors;
}

void SceneGraph::DrawWorld(Renderer& renderer)
{
    TraversalScope scope(m_TraversalDepth);
    for (ActorId id : DrawList(false))
    {
        Actor* actor = Find(id);
        if (actor && actor->VisibleInHierarchy())
        {
            actor->Draw(renderer);
        }
    }
}

void SceneGraph::DrawUI(Renderer& renderer)
{
    TraversalScope scope(m_TraversalDepth);
    for (ActorId id : DrawList(true))
    {
        Actor* actor = Find(id);
        if (!actor || !actor->VisibleInHierarchy())
        {
            continue;
        }
        if (actor->m_Layer >= ActorLayer::Overlay)
        {
            actor->Draw(renderer);
        }
        else
        {
            actor->DrawOverlay(renderer);
        }
    }
}

void SceneGraph::RegisterMember(ActorId group, ActorId member)
{
    if (!Find(group) || !Find(member))
    {
        throw std::invalid_argument("Invalid actor membership");
    }
    auto& members = m_Members[group];
    if (std::find(members.begin(), members.end(), member) == members.end())
    {
        members.push_back(member);
    }
}

const std::vector<ActorId>& SceneGraph::Members(ActorId group) const
{
    static const std::vector<ActorId> empty;
    const auto found = m_Members.find(group);
    return found == m_Members.end() ? empty : found->second;
}

void SceneGraph::NotifyTransformChanged(ActorId id)
{
    Actor* actor = Find(id);
    if (!actor)
    {
        return;
    }
    actor->m_WorldDirty = true;
    if (actor->ColliderRadius() > 0)
    {
        m_CollisionDirty = true;
    }
    for (ActorId child : actor->Children())
    {
        NotifyTransformChanged(child);
    }
}

void SceneGraph::RebuildCollisionGrid() const
{
    if (!m_CollisionDirty)
    {
        return;
    }
    m_CollisionGrid.clear();
    for (const auto& entry : m_Actors)
    {
        const Actor& actor = *entry.second;
        const double radius = actor.ColliderRadius();
        if (radius <= 0 || !actor.EnabledInHierarchy())
        {
            continue;
        }
        const WorldPoint p = actor.WorldPosition();
        const auto minX = static_cast<std::int64_t>(std::floor((p.x - radius) / 4));
        const auto maxX = static_cast<std::int64_t>(std::floor((p.x + radius) / 4));
        const auto minY = static_cast<std::int64_t>(std::floor((p.y - radius) / 4));
        const auto maxY = static_cast<std::int64_t>(std::floor((p.y + radius) / 4));
        for (auto y = minY; y <= maxY; ++y)
        {
            for (auto x = minX; x <= maxX; ++x)
            {
                m_CollisionGrid[{x, y}].push_back(actor.Id());
            }
        }
    }
    m_CollisionDirty = false;
}

bool SceneGraph::Blocked(double x, double y) const
{
    RebuildCollisionGrid();
    const auto cell = m_CollisionGrid.find({static_cast<std::int64_t>(std::floor(x / 4)),
                                            static_cast<std::int64_t>(std::floor(y / 4))});
    if (cell == m_CollisionGrid.end())
    {
        return false;
    }
    for (ActorId id : cell->second)
    {
        const Actor* actor = Find(id);
        if (!actor || !actor->EnabledInHierarchy())
        {
            continue;
        }
        const WorldPoint p = actor->WorldPosition();
        const double radius = actor->ColliderRadius();
        const double dx = x - p.x;
        const double dy = y - p.y;
        if (dx * dx + dy * dy < radius * radius)
        {
            return true;
        }
    }
    return false;
}
