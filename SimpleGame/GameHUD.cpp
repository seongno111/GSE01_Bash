#include "stdafx.h"
#include "Prototype.h"
#include "GameRules.h"
#include "NpcWorld.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>

using namespace GameRules;

void Prototype::Interface(Renderer& r)
{
    const float w = float(m_Width);
    const float h = float(m_Height);
    DrawLevelOneUI(r);

    r.Rect(0, 0, w, 105, Color(0.025f, 0.035f, 0.042f, 0.92f));
    r.Rect(28, 24, 3, 49, Gold);
    r.Text(45, 17, "성화의 파편", Paper, 2.3f);
    r.Text(46, 53, "성당 마을 · 살아 움직이는 변경", Muted, 1.4f);
    r.Text(46, 78, "불을 찾아라. 네가 누구인지 잊지 마라.", Gold, 1.2f);
    const float panelX = w - 305;
    r.Text(panelX,
           16,
           m_Player.corruption >= MutationThreshold ? "상태 / 괴물화" : "상태 / 인간",
           m_Player.corruption >= MutationThreshold ? Violet : Gold,
           1.6f);
    r.Text(panelX,
           40,
           std::to_string(int(m_Player.health)) + "/" + std::to_string(int(MaxHealth())),
           Muted,
           1.1f);
    r.Rect(panelX + 83, 43, 180, 7, Color(0.14f, 0.16f, 0.16f));
    r.Rect(panelX + 83, 43, 180 * m_Player.health / MaxHealth(), 7, Color(0.58f, 0.29f, 0.24f));
    r.Text(panelX, 59, "오염 " + std::to_string(m_Player.corruption), Paper, 1.2f);
    r.Rect(panelX, 80, 263, 5, Color(0.15f, 0.16f, 0.17f));
    r.Rect(panelX, 80, 263 * m_Player.corruption / 150.0f, 5, Violet);
    r.Rect(panelX + 263 * 100 / 150.0f, 76, 2, 13, Gold);
    if (m_Help)
    {
        r.Rect(24, 125, 255, 290, Color(0.025f, 0.035f, 0.042f, 0.86f));
        r.Text(40, 139, "파밍과 성장", Gold, 1.5f);
        r.Text(
            40,
            171,
            "WASD   이동\nSPACE 유지   자동 조준\nF   자동 사격 전환\nU   파편 3개로 무기 강화\nQ  "
            " 회복약 사용\nT   정화석 사용\nE   성소 회복·보고\nR   성물 사용\nH   안내 숨기기",
            Paper,
            1.4f);
        const auto& post = r.Post().settings;
        const auto onOff = [](bool value)
        {
            return value ? "켜짐" : "꺼짐";
        };
        std::ostringstream effects;
        effects << "P   전체 후처리: " << onOff(post.enabled) << "\nB   블룸: " << onOff(post.bloom)
                << "\nV   비네트: " << onOff(post.vignette)
                << "\nN   가장자리 블러: " << onOff(post.edgeBlur)
                << "\n[ / ]   노출: " << std::fixed << std::setprecision(2) << post.exposure
                << "\n0   효과 기본값 복원";
        r.Rect(w - 280, 125, 255, 205, Color(0.025f, 0.035f, 0.042f, 0.86f));
        r.Text(w - 264, 139, "빛과 그림자", Gold, 1.5f);
        r.Text(w - 264,
               171,
               r.Post().Available()
                   ? effects.str()
                   : "후처리 초기화 실패\n기본 렌더링으로 실행 중\n콘솔 오류를 확인해 주세요.",
               Paper,
               1.3f);
    }
    const Point fire = Project(m_ShrineX, m_ShrineY);
    if (Distance(m_Player.WorldX() - m_ShrineX, m_Player.WorldY() - m_ShrineY) < 2.8)
    {
        const std::string text =
            m_Player.corruption >= MutationThreshold ? "성물이 필요하다 [R]" : "[E] 휴식과 정화";
        r.Text(fire.x - r.TextWidth(text, 1.4f) * 0.5f, fire.y + 38 * m_Zoom, text, Gold, 1.4f);
    }
    if (m_MessageTime > 0)
    {
        float scale = 1.5f;
        while (scale > 0.7f && r.TextWidth(m_Message, scale) > w - 70)
        {
            scale -= 0.05f;
        }
        const float length = r.TextWidth(m_Message, scale);
        r.Rect((w - length) * 0.5f - 15,
               h - 113,
               length + 30,
               39,
               Color(0.025f, 0.035f, 0.042f, 0.93f));
        r.Text((w - length) * 0.5f, h - 105, m_Message, Paper, scale);
    }
    r.Rect(0, h - 57, w, 57, Color(0.025f, 0.035f, 0.042f, 0.94f));
    r.Line({27, h - 57}, {w - 27, h - 57}, 1, Gold.Shade(0.45f));
    std::ostringstream stats;
    stats << "무기 +" << m_Player.weaponRank << " · 피해 " << int(ProjectileDamage())
          << " · 발사 간격 " << std::fixed << std::setprecision(2) << ProjectileCooldown() << "초";
    r.Text(28, h - 46, stats.str(), Paper, 1.3f);
    r.Text(28,
           h - 21,
           "파편 " + std::to_string(m_Player.shards) + " · 회복약(Q) "
               + std::to_string(m_Player.potions) + " · 정화석(T) "
               + std::to_string(m_Player.purificationStones) + " · 성물(R) "
               + std::to_string(m_Player.relics),
           Muted,
           1.1f);
    if (m_Grid)
    {
        std::ostringstream status;
        status << "시드 " << m_WorldSeed;
        r.Text(w - 370, h - 36, status.str(), Gold, 1.1f);
    }
    else
    {
        r.Text(w - 322,
               h - 36,
               "성소까지 "
                   + std::to_string(
                       int(Distance(m_Player.WorldX() - m_ShrineX, m_Player.WorldY() - m_ShrineY)))
                   + " 걸음",
               Gold,
               1.3f);
    }
}

void Prototype::DrawLevelOneUI(Renderer& r)
{
    if (m_NpcWorld)
    {
        m_NpcWorld->DrawHUD(r);
        return;
    }
    const Color text(0.82f, 0.84f, 0.78f);
    const Color panel(0.025f, 0.035f, 0.042f, 0.91f);

    double targetX = 0;
    double targetY = 0;
    if (TargetPosition(targetX, targetY))
    {
        const Point p = Project(targetX, targetY);
        for (int i = 0; i < 24; ++i)
        {
            const float angle = i * 6.2831853f / 24;
            const float next = (i + 1) * 6.2831853f / 24;
            r.Line({p.x + 25 * m_Zoom * std::cos(angle), p.y + 12 * m_Zoom * std::sin(angle)},
                   {p.x + 25 * m_Zoom * std::cos(next), p.y + 12 * m_Zoom * std::sin(next)},
                   m_Zoom,
                   DamageColor);
        }
    }

    // This compact objective panel remains visible when the help panels are hidden.
    std::string objective;
    double goalX = 8;
    double goalY = 3;
    if (m_LevelOneComplete)
    {
        objective = "입문 완료 · 자유 파밍 / 적은 일정 시간 뒤 재등장";
    }
    else if (m_FarmKills < 3)
    {
        objective = "1. SPACE로 사냥하기 " + std::to_string(m_FarmKills) + "/3";
    }
    else if (m_CollectedItems < 4)
    {
        objective = "2. 드랍 아이템에 접근해 자동 습득 " + std::to_string(m_CollectedItems) + "/4";
    }
    else if (m_Player.level < 3 || m_FarmKills < 6)
    {
        objective = "3. 레벨 3 / 적 6마리 처치 (현재 " + std::to_string(m_Player.level) + " / "
                    + std::to_string(m_FarmKills) + ")";
        goalX = 12;
        goalY = 5;
    }
    else if (!m_GuardianDefeated)
    {
        objective = "4. 보스 처치 · 충격파와 붉은 공격 예고를 피하세요";
        if (const auto* boss = dynamic_cast<const EnemyActor*>(m_Scene.Find(m_GuardianId)))
        {
            goalX = boss->homeX;
            goalY = boss->homeY;
        }
    }
    else
    {
        objective = "5. 시작 성소로 귀환하여 E · 보스 처치 보고";
        goalX = 0;
        goalY = 0;
    }

    const float width = (std::min)(560.0f, float(m_Width) - 48);
    const float top = float(m_Height) - 213;
    r.Rect(24, top, width, 62, panel);
    const float scale =
        (std::max)(0.8f, (std::min)(1.25f, (width - 25) / (r.TextWidth(objective, 1.0f) + 1)));
    r.Text(36, top + 8, objective, text, scale);

    std::ostringstream combat;
    combat << (m_Player.autoFire ? "F 자동 사격 켜짐" : "SPACE 유지: 사격 / F 자동 사격")
           << " · 사거리 6 · " << (m_TargetId ? "표적 포착" : "사거리 내 표적 없음");
    r.Text(36, top + 35, combat.str(), DamageColor, 1.0f);

    if (!m_LevelOneComplete)
    {
        const Point goal = Project(goalX, goalY);
        const Point player = Project(m_Player.WorldX(), m_Player.WorldY());
        const float dx = goal.x - player.x;
        const float dy = goal.y - player.y;
        const float length = std::sqrt(dx * dx + dy * dy);
        if (length > 60)
        {
            const Point end{player.x + dx / length * 49, player.y + dy / length * 49};
            const Point base{player.x + dx / length * 35, player.y + dy / length * 35};
            r.Triangle(end,
                       {base.x - dy / length * 5, base.y + dx / length * 5},
                       {base.x + dy / length * 5, base.y - dx / length * 5},
                       ExperienceColor);
        }
    }

    const float xpTop = float(m_Height) - 142;
    r.Rect(24, xpTop, width, 21, panel);
    const float fraction =
        m_Player.level >= MaximumLevel ? 1.0f : float(m_Player.experience) / ExperienceRequired();
    r.Rect(24, xpTop, width * fraction, 3, ExperienceColor);
    const std::string xp =
        "레벨 " + std::to_string(m_Player.level) + " · 경험치 "
        + (m_Player.level >= MaximumLevel ? "최대 레벨"
                                          : std::to_string(m_Player.experience) + " / "
                                                + std::to_string(ExperienceRequired()));
    r.Text(36, xpTop + 5, xp, ExperienceColor, 1.0f);

    if (m_Player.magnetTime > 0)
    {
        r.Text(float(m_Width) - 280,
               float(m_Height) - 85,
               "자석 강화 " + std::to_string(int(std::ceil(m_Player.magnetTime))) + "초",
               ExperienceColor,
               1.2f);
    }

    if (const auto* actor = dynamic_cast<const EnemyActor*>(m_Scene.Find(m_GuardianId)))
    {
        const EnemyActor& boss = *actor;
        if (boss.alive && boss.VisibleInHierarchy()
            && Length(m_Player.WorldX() - boss.WorldX(), m_Player.WorldY() - boss.WorldY()) < 12)
        {
            const float bossWidth = (std::min)(440.0f, float(m_Width) * 0.36f);
            const float bossLeft = (float(m_Width) - bossWidth) * 0.5f;
            r.Rect(bossLeft, 111, bossWidth, 42, panel);
            const std::string title =
                boss.health <= boss.maxHealth * 0.4f ? "경작지 수문장 · 격노" : "경작지 수문장";
            r.Text(bossLeft + 10, 116, title, text, 1.25f);
            r.Rect(bossLeft + 10, 143, bossWidth - 20, 4, Color(0.18f, 0.12f, 0.13f));
            r.Rect(bossLeft + 10,
                   143,
                   (bossWidth - 20) * (boss.health / boss.maxHealth),
                   4,
                   Color(0.85f, 0.23f, 0.18f));
        }
    }
}
