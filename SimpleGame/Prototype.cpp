#include "stdafx.h"
#include "Prototype.h"
#include "GameRules.h"
#include "NpcWorld.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <chrono>

using namespace GameRules;

Prototype::~Prototype() = default;

Prototype::Prototype()
    : m_WorldRoot(m_Scene.Spawn<::Actor>(m_Scene.Root(), "World").Id()),
      m_GameplayRoot(m_Scene.Spawn<::Actor>(m_Scene.Root(), "Gameplay").Id()),
      m_EffectsRoot(m_Scene.Spawn<::Actor>(m_Scene.Root(), "Effects").Id()),
      m_UIRoot(m_Scene.Spawn<::Actor>(m_Scene.Root(), "UI").Id()),
      m_Player(m_Scene.Spawn<PlayerActor>(m_GameplayRoot, *this)),
      m_Echoes(m_Scene, m_GameplayRoot, "Echoes"),
      m_Enemies(m_Scene, m_GameplayRoot, "Enemies"),
      m_Projectiles(m_Scene, m_GameplayRoot, "PlayerProjectiles"),
      m_EnemyProjectiles(m_Scene, m_GameplayRoot, "EnemyProjectiles"),
      m_Loot(m_Scene, m_GameplayRoot, "Loot"),
      m_CombatText(m_Scene, m_UIRoot, "CombatText")
{
    m_Scene.Spawn<RangeIndicatorActor>(m_Player.Id(), *this);
    for (int i = 0; i < 6; ++i)
    {
        m_Scene.Spawn<AtmosphereActor>(m_EffectsRoot, *this, false, i);
    }
    for (int i = 0; i < 45; ++i)
    {
        m_Scene.Spawn<AtmosphereActor>(m_EffectsRoot, *this, true, i);
    }
    m_Scene.Spawn<HUDActor>(m_UIRoot, *this);
}

void Prototype::Notice(const std::string& text)
{
    m_Message = text;
    m_MessageTime = 5;
}

void Prototype::Die(const ::Actor* killer, bool unlawful)
{
    if (m_NpcWorld)
    {
        m_NpcWorld->RecordWorldEvent(unlawful ? "murder" : "death",
                                     "플레이어 사망",
                                     m_Player.WorldPosition(),
                                     killer,
                                     &m_Player);
    }
    // Evaluate the pre-death state: reaching the threshold on this death
    // mutates the survivor; dying while already mutated leaves an echo.
    if (m_Player.corruption >= MutationThreshold)
    {
        const float echoHealth = MaxHealth() + m_Player.corruption;
        const float echoDamage =
            12.0f + m_Player.corruption * 0.08f + (m_Player.level - 1) * 2.0f + m_Player.weaponRank;
        m_Echoes.Spawn(*this,
                       m_NextEcho++,
                       m_Player.WorldX(),
                       m_Player.WorldY(),
                       m_Player.corruption,
                       echoHealth,
                       true,
                       echoHealth,
                       echoDamage,
                       m_Player.level);
        Notice("과거의 네가 이 땅에 남았다. 그것은 네 힘을 기억한다.");
    }
    else
    {
        Notice(m_Player.corruption + 25 >= MutationThreshold
                   ? "몸이 괴물로 변했다. 성물만이 너를 되돌릴 수 있다. [R]"
                   : "죽음에서 돌아왔다. 오염 +25. 네 안의 힘이 강해진다.");
    }
    ++m_Player.deaths;
    m_Player.corruption = (std::min)(150, m_Player.corruption + 25);
    m_Player.SetWorldPosition(m_ShrineX + 2, m_ShrineY + 1);
    m_Player.ResetAnimation();
    m_Player.attack = 0;
    m_Player.walking = false;
    m_Player.running = false;
    m_CameraX = m_Player.WorldX();
    m_CameraY = m_Player.WorldY();
    m_Player.health = MaxHealth();
    m_Player.hitCooldown = 4;
    m_Projectiles.clear();
    m_EnemyProjectiles.clear();
    m_TargetId = InvalidActor;
    m_Player.shotCooldown = 0;

    for (EnemyActor& enemy : m_Enemies)
    {
        if (enemy.outpost >= 0)
        {
            continue;
        }
        enemy.SetWorldPosition(enemy.homeX, enemy.homeY);
        enemy.ResetAnimation();
        enemy.health = enemy.maxHealth;
        enemy.attackWindup = 0;
        enemy.burstWindup = 0;
        enemy.burstCooldown = 5;
    }
}

void Prototype::Key(unsigned char key, bool down)
{
    key = static_cast<unsigned char>(std::tolower(key));
    const bool pressed = down && !m_Keys[key];
    m_Keys[key] = down;
    if (!pressed)
    {
        return;
    }
    if (m_NpcWorld && m_NpcWorld->Key(key))
    {
        return;
    }
    if (key == 'h')
    {
        m_Help = !m_Help;
    }
    if (key == 'g')
    {
        m_Grid = !m_Grid;
    }
    if (key == '+' || key == '=')
    {
        m_Zoom = (std::min)(1.6f, m_Zoom + 0.1f);
    }
    if (key == '-')
    {
        m_Zoom = (std::max)(0.65f, m_Zoom - 0.1f);
    }
    if (key == 'k')
    {
        Die();
    }
    if (key == 'r')
    {
        if (m_Player.corruption < MutationThreshold)
        {
            Notice("아직 인간의 모습이다. 성물은 변이한 뒤에 사용할 수 있다.");
        }
        else if (m_Player.relics == 0)
        {
            Notice("남은 성물이 없다. 시연을 다시 시작하면 하나를 지급받는다.");
        }
        else
        {
            --m_Player.relics;
            m_Player.corruption = 0;
            Notice("성물이 인간의 모습을 되찾아 주었다. 네 과거는 여전히 남아 있다.");
        }
    }
    if (key == 'e')
    {
        if (Distance(m_Player.WorldX() - m_ShrineX, m_Player.WorldY() - m_ShrineY) > 2.8)
        {
            Notice("돌길을 따라 성소로 향하라. 불 가까이에서 정화할 수 있다.");
        }
        else if (m_Player.corruption >= MutationThreshold)
        {
            Notice("이 몸은 불로 씻어낼 수 없다. 인간으로 돌아가려면 성물이 필요하다.");
        }
        else
        {
            m_Player.corruption = 0;
            m_Player.health = MaxHealth();
            Notice("불이 너를 품는다. 오염이 씻겨 나가고 생명력이 회복되었다.");
        }
    }
    if (key == 'e')
    {
        CompleteLevelOne();
    }

    if (key == 'f')
    {
        m_Player.autoFire = !m_Player.autoFire;
        Notice(m_Player.autoFire ? "자동 사격 켜짐. 사거리 안의 보이는 적을 공격합니다."
                                 : "자동 사격 꺼짐. SPACE를 누르고 있으면 사격합니다.");
    }

    if (key == 'q')
    {
        if (m_Player.potions <= 0)
        {
            Notice("회복약이 없습니다. 붉은 회복 아이템을 찾거나 성소로 돌아가세요.");
        }
        else if (m_Player.health >= MaxHealth())
        {
            Notice("생명력이 가득 차 있습니다.");
        }
        else
        {
            --m_Player.potions;
            m_Player.health = (std::min)(MaxHealth(), m_Player.health + MaxHealth() * 0.45f);
            Notice("회복약 사용. 생명력을 회복했습니다.");
        }
    }

    if (key == 'u')
    {
        if (m_Player.weaponRank >= 10)
        {
            Notice("현재 무기는 최대 강화 단계입니다.");
        }
        else if (m_Player.shards < 3)
        {
            Notice("무기 강화에는 강화 파편 3개가 필요합니다.");
        }
        else
        {
            m_Player.shards -= 3;
            ++m_Player.weaponRank;
            Notice("무기 +" + std::to_string(m_Player.weaponRank)
                   + " 강화! 발사체 피해가 4 증가했습니다.");
        }
    }

    if (key == 't')
    {
        if (m_Player.corruption >= MutationThreshold)
        {
            Notice("괴물화한 몸은 정화석으로 되돌릴 수 없습니다. 성물 [R]이 필요합니다.");
        }
        else if (m_Player.corruption == 0 || m_Player.purificationStones == 0)
        {
            Notice(m_Player.corruption == 0 ? "씻어낼 오염이 없습니다." : "정화석이 없습니다.");
        }
        else
        {
            --m_Player.purificationStones;
            m_Player.corruption = (std::max)(0, m_Player.corruption - 25);
            Notice("정화석으로 오염 25를 씻어냈습니다. 오염으로 얻은 힘도 줄어듭니다.");
        }
    }
}

void Prototype::Update(float dt)
{
    m_UpdateStats = {};
    const auto updateStart = std::chrono::steady_clock::now();
    InitializeLevelOne();
    m_Time += dt;
    m_MessageTime = (std::max)(0.0f, m_MessageTime - dt);
    m_Scene.Update(dt, TickGroup::Player);
    m_ShrineX = NearestShrine(m_Player.WorldX());
    m_ShrineY = NearestShrine(m_Player.WorldY());
    UpdateGameplay(dt);
    const auto npcStart = std::chrono::steady_clock::now();
    m_UpdateStats.gameplayMs =
        std::chrono::duration<double, std::milli>(npcStart - updateStart).count();
    if (m_NpcWorld)
    {
        m_NpcWorld->Update(dt);
        const auto& stats = m_NpcWorld->Performance();
        m_UpdateStats.npcExportMs = stats.exportMs;
        m_UpdateStats.pathCalls += stats.pathCalls;
        m_UpdateStats.pathNodes += stats.pathNodes;
    }
    m_Scene.UpdateVisuals(dt);
    const auto streamStart = std::chrono::steady_clock::now();
    m_UpdateStats.npcMs = std::chrono::duration<double, std::milli>(streamStart - npcStart).count();
    const double blend = 1.0 - std::exp(-8.0 * dt);
    m_CameraX += (m_Player.WorldX() - m_CameraX) * blend;
    m_CameraY += (m_Player.WorldY() - m_CameraY) * blend;
    Stream();
    m_Scene.CollectGarbage();
    m_UpdateStats.streamMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - streamStart)
            .count();
}

Point Prototype::Project(double x, double y) const
{
    const double dx = x - m_CameraX;
    const double dy = y - m_CameraY;
    return {float((dx - dy) * 40 * m_Zoom + m_Width * 0.5),
            float((dx + dy) * 20 * m_Zoom + m_Height * 0.73)};
}

void Prototype::Draw(Renderer& r)
{
    m_Width = r.Width();
    m_Height = r.Height();
    r.Begin(Color(0.065f, 0.095f, 0.10f));
    m_Scene.DrawWorld(r);
    r.BeginUI();
    m_Scene.DrawUI(r);
    r.EndFrame();
}
