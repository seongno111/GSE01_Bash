#include "stdafx.h"
#include "NpcWorld.h"
#include "Prototype.h"
#include "GameRules.h"
#include <algorithm>
#include <sstream>
#include <iomanip>

using namespace GameRules;

NpcActor* NpcWorld::Selected() const
{
    return m_Actors.empty() ? nullptr
                            : dynamic_cast<NpcActor*>(m_Game.Scene().Find(m_Actors[m_Selected]));
}

bool NpcWorld::RequestInteraction(NpcAction action)
{
    auto* npc = Selected();
    if (!npc || !npc->vitals.alive)
    {
        m_Game.Notice("살아 있는 주민을 C로 선택하세요.");
        return false;
    }
    if (Length(npc->WorldX() - m_Game.Player().WorldX(), npc->WorldY() - m_Game.Player().WorldY())
            > 2.2
        || !m_Game.ClearShot(npc->WorldX(),
                             npc->WorldY(),
                             m_Game.Player().WorldX(),
                             m_Game.Player().WorldY()))
    {
        m_Game.Notice(npc->profile.name + "에게 가까이 다가가세요. (2걸음 이내)");
        return false;
    }
    const bool committedAttack =
        npc->execution.phase != NpcPhase::Complete
        && (npc->execution.action == NpcAction::Attack || npc->execution.action == NpcAction::Raid);
    if (committedAttack || Hostile(*npc, PlayerEntity) || npc->mode == NpcMode::Combat
        || npc->mode == NpcMode::Evacuate)
    {
        m_Game.Notice("거절: 적대 관계이거나 지금은 위험합니다.");
        return false;
    }
    if (action == NpcAction::Bless
        && (npc->profile.role != NpcRole::Priest || npc->vitals.holyPower < 8))
    {
        m_Game.Notice("축복은 성력이 8 이상인 성직자에게 요청하세요.");
        return false;
    }
    if (action == NpcAction::Trade
        && (!InVillage(npc->WorldPosition()) || m_Game.Player().gold < 12 || m_MarketMedicine < 1))
    {
        m_Game.Notice("거래에는 마을 내 주민, 금화 12개, 시장의 회복약 재고가 필요합니다.");
        return false;
    }
    if (std::any_of(m_Requests.begin(),
                    m_Requests.end(),
                    [&](const Request& r)
                    {
                        return r.npc == npc->Entity();
                    })
        || (npc->execution.playerRequest && npc->execution.phase != NpcPhase::Complete))
    {
        m_Game.Notice("이미 요청을 처리하고 있습니다.");
        return false;
    }
    if (npc->execution.phase != NpcPhase::Complete)
    {
        Finish(*npc, false, "플레이어 요청을 먼저 처리한다");
    }
    m_Requests.push_back({npc->Entity(), action, m_Time + 10});
    npc->nextSense = 0;
    npc->nextDecision = m_Time;
    m_Game.Notice(npc->profile.name + ": 요청을 확인했습니다. 가까이에서 기다려 주세요.");
    return true;
}

bool NpcWorld::Key(unsigned char key)
{
    if (key == 'c')
    {
        for (size_t step = 1; step <= m_Actors.size(); ++step)
        {
            const size_t index = (m_Selected + step) % m_Actors.size();
            const auto* actor = m_Game.Scene().Find(m_Actors[index]);
            if (actor
                && Length(actor->WorldX() - m_Game.Player().WorldX(),
                          actor->WorldY() - m_Game.Player().WorldY())
                       < 16)
            {
                m_Selected = index;
                break;
            }
        }
        return true;
    }
    if (key == 'i')
    {
        m_Inspector = !m_Inspector;
        m_Game.m_Help = false;
        return true;
    }
    if (key == 'j')
    {
        m_PK = !m_PK;
        m_Game.Notice(
            m_PK ? "PK 켜짐: SPACE/F 사격에 중립 주민도 표적이 됩니다. 목격자는 보복합니다."
                 : "PK 꺼짐: 적대 주민과 괴물만 표적이 됩니다. 기존 원한은 유지됩니다.");
        return true;
    }
    if (key == 'y')
    {
        RequestInteraction(NpcAction::Trade);
        return true;
    }
    if (key == 'o')
    {
        RequestInteraction(NpcAction::Bless);
        return true;
    }
    if (key == 'l')
    {
        m_Game.Notice(ExportLog() ? "NpcAI/latest.log에 최근 판단과 현재 상태를 기록했습니다."
                                  : "NPC 로그 저장에 실패했습니다. 콘솔과 쓰기 권한을 확인하세요.");
        return true;
    }
    if (key == 'e' && !InsideSanctuary(m_Game.Player().WorldX(), m_Game.Player().WorldY()))
    {
        RequestInteraction(NpcAction::Observe);
        return true;
    }
    return false;
}

void NpcWorld::DrawHUD(Renderer& r)
{
    const float width = float(r.Width());
    const float height = float(r.Height());
    const Color panel(0.025f, 0.035f, 0.042f, 0.93f);

    struct Landmark
    {
        WorldPoint position;
        const char* name;
        float offset;
    };

    const Landmark landmarks[] = {{{-3, -3}, "성당", 225},
                                  {{-7, 0}, "시장", 88},
                                  {{-8, 5}, "숙소 · 작업장", 100},
                                  {{1, 8}, "훈련장", 65}};
    for (const auto& landmark : landmarks)
    {
        const auto p = m_Game.Project(landmark.position.x, landmark.position.y);
        if (p.x >= 0 && p.x < width && p.y > 0 && p.y < height)
        {
            r.Text(p.x - r.TextWidth(landmark.name, 1) * 0.5f,
                   p.y - landmark.offset * m_Game.Zoom(),
                   landmark.name,
                   Gold,
                   1);
        }
    }
    const float stripWidth = (std::min)(660.0f, width - 48);
    r.Rect(24, height - 213, stripWidth, 92, panel);
    r.Text(36,
           height - 204,
           "성당 마을 · 금화 " + std::to_string(m_Game.Player().gold) + " · PK "
               + (m_PK ? "켜짐" : "꺼짐") + " · 주민 " + std::to_string(m_Actors.size()),
           Gold,
           1.2f);
    r.Text(36,
           height - 177,
           "C 주민 선택 · I AI 상태 · J PK · Y 회복약 구매 · O 축복 요청",
           Paper,
           1.0f);
    r.Text(36,
           height - 153,
           "E 대화/정화 · L 로그 · 플레이어 레벨 " + std::to_string(m_Game.Player().level)
               + " / 경험치 " + std::to_string(m_Game.Player().experience),
           Muted,
           1.0f);
    if (!m_Inspector)
    {
        return;
    }
    const auto* npc = Selected();
    if (!npc)
    {
        return;
    }
    const float left = (std::max)(24.0f, width - 414);
    const float top = 118;
    const float bottom = height - 235;
    r.Rect(left, top, 390, (std::max)(120.0f, bottom - top), panel);
    float y = top + 12;
    const auto line = [&](const std::string& text, Color color = Paper)
    {
        if (y + 20 < bottom)
        {
            const float scale = (std::min)(1.0f, 362.0f / (r.TextWidth(text, 1) + 1));
            r.Text(left + 12, y, text, color, scale);
        }
        y += 23;
    };
    line(npc->profile.name + " / " + NpcRoleName(npc->profile.role)
             + " / 목표: " + NpcGoalName(npc->profile.goal),
         Gold);
    line(std::string(NpcModeName(npc->mode)) + " · "
         + NpcActivityName(npc->profile.role, npc->execution.action) + " / "
         + NpcPhaseName(npc->execution.phase));
    line(std::string("공격: ") + NpcWeaponFor(npc->profile.role).name + " · 이동 "
         + std::to_string(int(npc->distanceTravelled)) + " · 완료/실패 "
         + std::to_string(npc->completedActions) + "/" + std::to_string(npc->failedActions));
    const auto& v = npc->vitals;
    line("체력 " + std::to_string(int(v.health)) + "/" + std::to_string(int(v.maxHealth))
         + " · 금화 " + std::to_string(v.gold));
    line("무력 " + std::to_string(int(v.strength)) + " · 성력 " + std::to_string(int(v.holyPower))
         + " · 깨달음 " + std::to_string(int(v.wisdom)));
    line("오염 " + std::to_string(int(v.corruption)) + (v.frenzy ? " / 광폭" : " / 자아 유지"),
         v.frenzy ? Violet : Muted);
    line("약탈 성향 " + std::to_string(int(npc->profile.raidWeight * 100)) + "% · 오염 공격 성향 "
         + std::to_string(int(npc->profile.corruptionWeight * 100)) + "%");
    line("식량 " + std::to_string(v.food) + " · 치료제 " + std::to_string(v.medicine) + " · 전리품 "
         + std::to_string(v.goods));
    line("피로 " + std::to_string(int(v.fatigue * 100)) + "% · 허기 "
         + std::to_string(int(v.hunger * 100)) + "%");
    line("판단 #" + std::to_string(npc->decisions) + " · 예약 "
         + std::to_string(npc->execution.reservation));
    std::vector<const NpcCandidate*> ranked;
    for (const auto& c : npc->candidates)
    {
        ranked.push_back(&c);
    }
    std::stable_sort(ranked.begin(),
                     ranked.end(),
                     [](const auto* a, const auto* b)
                     {
                         return a->probability > b->probability;
                     });
    for (size_t i = 0; i < (std::min)(size_t(3), ranked.size()); ++i)
    {
        const auto& c = *ranked[i];
        line(std::string(NpcActionName(c.action)) + " " + std::to_string(int(c.probability * 100))
                 + "% · " + c.reason,
             c.allowed ? Paper : Muted);
    }
    line(npc->lastResult, Gold);
    if (!m_Events.empty())
    {
        line(m_Events.back(), Muted);
    }
}
