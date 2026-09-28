#pragma once
#include "Actor.h"
#include <memory>
#include <unordered_map>
#include <utility>
#include <array>
#include <map>
#include <stdexcept>
#include <type_traits>

class SceneGraph
{
public:

    SceneGraph();
    SceneGraph(const SceneGraph&) = delete;
    SceneGraph& operator=(const SceneGraph&) = delete;
    ActorId Root() const;
    Actor* Find(ActorId id);
    const Actor* Find(ActorId id) const;

    template <class T, class... Args> T& Spawn(ActorId parent, Args&&... args)
    {
        auto actor = std::make_unique<T>(std::forward<Args>(args)...);
        T& result = *actor;
        Adopt(parent, std::move(actor));
        return result;
    }

    // A rejected reparent leaves the graph intact. IDs never change or recycle.
    bool Reparent(ActorId actor, ActorId parent, bool keepWorldPosition = true);
    void Destroy(ActorId id);
    void CollectGarbage();
    void Update(float dt, TickGroup group);
    void DrawWorld(Renderer& renderer);
    void DrawUI(Renderer& renderer);
    bool Blocked(double x, double y) const;
    void NotifyTransformChanged(ActorId id);

    struct DrawStats
    {
        size_t worldActors = 0;
        size_t uiActors = 0;
        double worldListMs = 0;
        double uiListMs = 0;
    };

    const DrawStats& RenderingStats() const
    {
        return m_DrawStats;
    }

    size_t ActorCount() const
    {
        return m_Actors.size() - m_DestroyedActors.size();
    }

private:

    template <class T> friend class ActorCollection;
    void RegisterMember(ActorId group, ActorId member);
    const std::vector<ActorId>& Members(ActorId group) const;

    void Adopt(ActorId parent, std::unique_ptr<Actor> actor);
    std::vector<ActorId> DrawList(bool ui) const;
    void RebuildCollisionGrid() const;
    std::unordered_map<ActorId, std::unique_ptr<Actor>> m_Actors;
    std::array<std::vector<ActorId>, 6> m_TickActors;
    std::vector<ActorId> m_DrawActors;
    std::vector<ActorId> m_DestroyedActors;
    std::unordered_map<ActorId, std::vector<ActorId>> m_Members;
    mutable std::map<std::pair<std::int64_t, std::int64_t>, std::vector<ActorId>> m_CollisionGrid;
    mutable bool m_CollisionDirty = true;
    ActorId m_NextId = 1;
    ActorId m_Root = InvalidActor;
    unsigned m_TraversalDepth = 0;
    mutable DrawStats m_DrawStats;
};

// A typed membership view of graph-owned actors, independent of reparenting.
// Iterators store indices/IDs, not vector element addresses; destruction is
// deferred until the end of the frame and pending actors are skipped immediately.
template <class T> class ActorCollection
{
public:

    ActorCollection(SceneGraph& scene, ActorId parent, const std::string& name)
        : m_Scene(scene),
          m_Group(scene.Spawn<Actor>(parent, name).Id())
    {
    }

    template <class... Args> T& Spawn(Args&&... args)
    {
        return SpawnAs<T>(std::forward<Args>(args)...);
    }

    template <class U, class... Args> U& SpawnAs(Args&&... args)
    {
        static_assert(std::is_base_of<T, U>::value, "Actor type must match its collection");
        U& actor = m_Scene.Spawn<U>(m_Group, std::forward<Args>(args)...);
        // Gameplay spawn coordinates are world coordinates, even when the
        // collection's default parent has moved. Generic SceneGraph::Spawn
        // instead preserves the constructor's local coordinates.
        const WorldPoint position = actor.LocalPosition();
        actor.SetWorldPosition(position.x, position.y);
        m_Scene.RegisterMember(m_Group, actor.Id());
        return actor;
    }

    class Iterator
    {
    public:

        Iterator(const ActorCollection& owner, size_t index)
            : m_Owner(owner),
              m_Index(index),
              m_Limit(owner.Children().size())
        {
            Skip();
        }

        T& operator*() const
        {
            return static_cast<T&>(*m_Owner.m_Scene.Find(m_Owner.Children()[m_Index]));
        }

        Iterator& operator++()
        {
            ++m_Index;
            Skip();
            return *this;
        }

        bool operator!=(const Iterator& other) const
        {
            return m_Index != other.m_Index;
        }

    private:

        void Skip()
        {
            const auto& children = m_Owner.Children();
            while (m_Index < m_Limit && !m_Owner.m_Scene.Find(children[m_Index]))
            {
                ++m_Index;
            }
        }

        const ActorCollection& m_Owner;
        size_t m_Index;
        size_t m_Limit;
    };

    Iterator begin() const
    {
        return Iterator(*this, 0);
    }

    Iterator end() const
    {
        return Iterator(*this, Children().size());
    }

    void clear()
    {
        for (ActorId id : Children())
        {
            m_Scene.Destroy(id);
        }
    }

private:

    const std::vector<ActorId>& Children() const
    {
        // Membership is independent of hierarchy: reparenting an enemy to a
        // moving parent must not remove it from combat queries.
        return m_Scene.Members(m_Group);
    }

    SceneGraph& m_Scene;
    ActorId m_Group;
};
