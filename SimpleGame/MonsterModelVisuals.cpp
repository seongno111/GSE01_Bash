#include "stdafx.h"
#include "Prototype.h"

#include <algorithm>

bool EnemyActor::DrawSpeciesModel(Renderer& renderer)
{
    const auto& stats = Definition();
    const auto* visual = m_Game.ModelAssets().MonsterSpecies(stats.modelKey);
    if (!visual)
    {
        return false;
    }

    const auto sprite = SpritePose();
    // Interpolate visual timers between the 20 Hz AI updates without firing gameplay early.
    const float extra = std::clamp(m_Game.Time() - visualTimerSampleAt, 0.0f, .05f);
    const auto remaining = [extra](float timer)
    {
        return (std::max)(0.0f, timer - extra);
    };
    const auto duration = [&](const std::string& name)
    {
        return visual->model.asset->animations.at(visual->model.asset->animationByName.at(name))
            .duration;
    };
    std::string clip = visual->idle;
    float progress = -1;
    if (specialWindup > 0)
    {
        clip = visual->special.empty() ? visual->prepare : visual->special;
        progress = (stats.specialWindup - remaining(specialWindup)) / duration(clip);
    }
    else if (chargeRemaining > 0)
    {
        // Travel loops are in-place. AI alone advances position and applies collisions.
        clip = visual->active;
    }
    else if (attackWindup > 0 || (recovery > 0 && !specialRecovery))
    {
        clip = visual->attack;
        progress = attackWindup > 0
                       ? (stats.attackWindup - remaining(attackWindup)) / duration(clip)
                       : 1 - remaining(recovery) / duration(clip);
    }
    else if (guardRemaining > 0)
    {
        // Ward lasts independently of the short AI recovery timer.
        clip = visual->active;
        progress = (2.4f - remaining(guardRemaining)) / duration(clip);
    }
    else if (recovery > 0 && specialRecovery)
    {
        clip = visual->special.empty() ? visual->recover : visual->special;
        progress = 1 - remaining(recovery) / duration(clip);
    }
    else if (stats.ability == MonsterAbility::Ward && m_Game.Time() >= wardEndedAt
             && m_Game.Time() - wardEndedAt < duration(visual->recover))
    {
        clip = visual->recover;
        progress = (m_Game.Time() - wardEndedAt) / duration(clip);
    }
    else if (sprite.moving)
    {
        const bool running = state == MonsterState::Chase || state == MonsterState::Flank
                             || state == MonsterState::Fleeing;
        clip = running ? visual->run : visual->walk;
        // Direction comes only from actor yaw; all travel shares the same gait phase.
        progress = sprite.walkCycle;
    }

    return DrawModel(
        renderer,
        std::string("species/") + stats.modelKey,
        [visual]()
        {
            return visual->model;
        },
        clip,
        progress,
        enraged ? Color(1, .62f, .55f) : Color());
}
