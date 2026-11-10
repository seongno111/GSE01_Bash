#include "stdafx.h"
#include "Prototype.h"
#include "HeroWeapons.h"
#include "Villages.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace
{
    float ShopScale(int width, int height)
    {
        return (std::min)(float(width) / 1000, float(height) / 680);
    }
} // namespace

bool Prototype::NearBlacksmith() const
{
    for (const auto& village : Villages)
    {
        if (std::hypot(m_Player.WorldX() - village.center.x - 6,
                       m_Player.WorldY() - village.center.y - 2)
            <= 2.8)
        {
            return true;
        }
    }
    return false;
}

bool Prototype::CloseWeaponShop()
{
    const bool wasOpen = m_WeaponShopOpen;
    m_WeaponShopOpen = false;
    return wasOpen;
}

void Prototype::WeaponShopClick(float x, float y)
{
    if (!m_WeaponShopOpen || !NearBlacksmith() || m_Player.health <= 0)
    {
        CloseWeaponShop();
        return;
    }
    const float scale = ShopScale(m_Width, m_Height);
    x = (x - (m_Width - 1000 * scale) * .5f) / scale;
    y = (y - (m_Height - 680 * scale) * .5f) / scale;
    if (x >= 870 && x <= 975 && y >= 15 && y <= 60)
    {
        CloseWeaponShop();
        return;
    }
    for (size_t i = 0; i < HeroWeapons.size(); ++i)
    {
        const float left = 25 + float(i % 4) * 240;
        const float top = 95 + float(i / 4) * 270;
        if (x < left || x > left + 225 || y < top || y > top + 250)
        {
            continue;
        }
        if (!m_ModelAssets.HeroWeaponModel(HeroWeapons[i].key, true))
        {
            Notice("무기 모델을 불러오지 못했습니다. 콘솔을 확인하세요.");
            return;
        }
        m_Player.weapon = int(i);
        m_Player.range = HeroWeapons[i].range;
        // Switching cannot bypass recovery from a previous attack.
        m_Player.shotCooldown = (std::max)(m_Player.shotCooldown, ProjectileCooldown());
        m_Player.ResetAnimation();
        Notice(std::string(HeroWeapons[i].name) + " 장착");
        return;
    }
}

void Prototype::DrawWeaponShop(Renderer& r)
{
    for (const auto& village : Villages)
    {
        const auto position = Project(village.center.x + 6, village.center.y + 2);
        if (position.x > 0 && position.x < m_Width && position.y > 100 && position.y < m_Height)
        {
            r.Text(position.x - 40,
                   position.y - 85 * m_Zoom,
                   "대장간 [E]",
                   Color(1, .75f, .3f),
                   1.3f);
        }
    }
    if (!m_WeaponShopOpen)
    {
        return;
    }
    const float s = ShopScale(m_Width, m_Height);
    const float ox = (m_Width - 1000 * s) * .5f;
    const float oy = (m_Height - 680 * s) * .5f;
    r.Rect(0, 0, float(m_Width), float(m_Height), Color(.01f, .01f, .02f, .92f));
    r.Rect(ox, oy, 1000 * s, 680 * s, Color(.06f, .065f, .07f));
    r.Text(ox + 25 * s, oy + 20 * s, "대장간 · 무기 교체", Color(1, .8f, .4f), 2 * s);
    r.Text(ox + 25 * s,
           oy + 57 * s,
           "무기를 클릭해 무료 장착 · 강화와 성장 보너스는 모든 무기에 적용",
           Color(.8f, .8f, .8f),
           1.1f * s);
    r.Text(ox + 880 * s, oy + 25 * s, "닫기 [ESC]", Color(1, 1, 1), 1.2f * s);
    for (size_t i = 0; i < HeroWeapons.size(); ++i)
    {
        const auto& weapon = HeroWeapons[i];
        const float x = ox + (25 + float(i % 4) * 240) * s;
        const float y = oy + (95 + float(i / 4) * 270) * s;
        r.Rect(x,
               y,
               225 * s,
               250 * s,
               m_Player.weapon == int(i) ? Color(.25f, .20f, .10f) : Color(.10f, .12f, .14f));
        const auto model = m_ModelAssets.HeroWeaponModel(weapon.key, false);
        if (model)
        {
            r.DrawModel(model, nullptr, {x + 112 * s, y + 90 * s}, .65f * s, .7f, false);
        }
        r.Text(x + 10 * s, y + 135 * s, weapon.name, Color(1, .85f, .55f), 1.2f * s);
        std::ostringstream specs;
        const float damage =
            weapon.Damage(m_Player.level, m_Player.weaponRank, m_Player.corruption);
        const float cooldown = WeaponCooldown(int(i));
        specs << (weapon.ranged ? "원거리" : "근접") << " · 사거리 " << weapon.range << "\n공격력 "
              << std::fixed << std::setprecision(1) << damage << "\n공격 간격 "
              << std::setprecision(2) << cooldown << "초"
              << "\n"
              << (!model                      ? "모델 로딩 실패"
                  : m_Player.weapon == int(i) ? "장착 중"
                                              : "클릭하여 장착");
        r.Text(x + 10 * s, y + 165 * s, specs.str(), Color(.9f, .9f, .9f), 1.1f * s);
    }
}
