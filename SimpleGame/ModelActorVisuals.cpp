#include "stdafx.h"
#include "Prototype.h"
#include "GameRules.h"
#include "NpcWorld.h"

#include <algorithm>
#include <cmath>

bool GameActor::DrawModel(Renderer& renderer,
                          const std::string& binding,
                          const std::function<Models::ModelInstance()>& factory,
                          const std::string& requestedClip,
                          float normalizedTime,
                          Color tint)
{
    if (binding != m_ModelBinding)
    {
        auto model = factory();
        if (model.asset != m_Model.asset || (model && !m_ModelAnimation))
        {
            m_ModelAnimation =
                model ? std::make_unique<Models::AnimationPlayer>(model.asset) : nullptr;
            m_ModelLastDraw = m_Game.Time();
            m_ModelStarted = false;
        }
        m_Model = std::move(model);
        m_ModelBinding = binding;
    }
    if (!m_Model || !m_ModelAnimation)
    {
        return false;
    }
    auto clip = m_Model.asset->animationByName.find(requestedClip);
    if (clip == m_Model.asset->animationByName.end() && requestedClip.find("HERO_") == 0)
    {
        const std::string fallback = requestedClip == "HERO_AttackLongsword"
                                         ? "NPC_AttackAdventurer"
                                         : "NPC_" + requestedClip.substr(5);
        clip = m_Model.asset->animationByName.find(fallback);
    }
    if (clip == m_Model.asset->animationByName.end())
    {
        for (const char* idle : {"HERO_Idle", "NPC_Idle"})
        {
            clip = m_Model.asset->animationByName.find(idle);
            if (clip != m_Model.asset->animationByName.end())
            {
                break;
            }
        }
    }
    const auto sprite = SpritePose();
    if (clip != m_Model.asset->animationByName.end())
    {
        const auto& animation = m_Model.asset->animations[clip->second];
        m_ModelAnimation->Play(animation.name, m_ModelStarted ? .12f : 0, animation.loop);
        m_ModelStarted = true;
        const float elapsed =
            m_ModelLastDraw >= 0 ? (std::max)(0.0f, m_Game.Time() - m_ModelLastDraw) : 0;
        m_ModelEvents.clear();
        m_ModelAnimation->Advance(elapsed, m_ModelEvents);
        if (normalizedTime >= 0)
        {
            m_ModelAnimation->SetTime(std::clamp(normalizedTime, 0.0f, 1.0f) * animation.duration);
        }
        else if (animation.loop && sprite.moving
                 && (animation.name.find("Walk") != std::string::npos
                     || animation.name.find("Run") != std::string::npos
                     || animation.name.find("Flee") != std::string::npos))
        {
            // Preserve the existing smoothed gait phase rather than restarting on 20 Hz AI ticks.
            m_ModelAnimation->SetTime(sprite.walkCycle * animation.duration);
        }
    }
    m_ModelLastDraw = m_Game.Time();
    return renderer.DrawModel(m_Model,
                              &m_ModelAnimation->CurrentPose(),
                              m_Game.Project(WorldX(), WorldY()),
                              m_Game.Zoom(),
                              sprite.facingLeft,
                              true,
                              tint);
}

bool NpcActor::DrawNpcModel(Renderer& renderer)
{
    const bool mutant = vitals.corruption >= GameRules::MutationThreshold;
    const bool performing = execution.phase == NpcPhase::Perform;
    const bool recovering = execution.phase == NpcPhase::Recover;
    const bool attacking =
        (execution.action == NpcAction::Attack || execution.action == NpcAction::Raid)
        && (performing || recovering);
    const auto* action = m_Game.ModelAssets().NpcAction(static_cast<int>(execution.action));
    int equipmentAction = -1;
    std::string clip = SpritePose().moving ? "NPC_Walk" : "NPC_Idle";
    float progress = -1;
    if (!vitals.alive)
    {
        clip = mutant ? "MON_03_DeadHold" : "NPC_DeadHold";
        progress = 1;
    }
    else if (mutant)
    {
        clip = attacking ? "MON_03_Attack" : SpritePose().moving ? "MON_03_Walk" : "MON_03_Idle";
    }
    else if (action)
    {
        if (performing || attacking)
        {
            clip = action->perform;
            const auto roleClip = action->performByRole.find(static_cast<int>(profile.role));
            if (roleClip != action->performByRole.end())
            {
                clip = roleClip->second;
            }
        }
        else if (recovering)
        {
            clip = action->recover;
        }
        if ((performing || recovering) && action->overridesHand)
        {
            equipmentAction = static_cast<int>(execution.action);
        }
    }
    if (attacking)
    {
        const auto& weapon = NpcWeaponFor(profile.role);
        const double duration = weapon.windup + weapon.recovery;
        const double remaining = (std::max)(0.0, execution.phaseUntil - m_World.Time());
        progress = static_cast<float>(performing ? (weapon.windup - remaining) / duration
                                                 : 1 - remaining / duration);
        progress = std::clamp(progress, 0.0f, 1.0f);
    }
    else if (performing && action && action->visualEventsOnly && vitals.alive && !mutant)
    {
        const auto& weapon = NpcWeaponFor(profile.role);
        const double duration = weapon.windup + weapon.recovery;
        progress = static_cast<float>(std::fmod(m_World.Time() - execution.startedAt, duration)
                                      / duration);
    }
    const std::string binding =
        "npc/" + std::to_string(profile.id) + "/" + std::to_string(static_cast<int>(profile.role))
        + (mutant ? "/mutated/" : "/human/") + std::to_string(equipmentAction);
    return DrawModel(
        renderer,
        binding,
        [&]()
        {
            return mutant ? m_Game.ModelAssets().Monster(3)
                          : m_Game.ModelAssets().Npc(profile.id,
                                                     static_cast<int>(profile.role),
                                                     equipmentAction);
        },
        clip,
        progress,
        vitals.frenzy ? Color(1, .55f, .7f) : Color());
}
