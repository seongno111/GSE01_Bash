#include "stdafx.h"
#include "Prototype.h"
#include "GameRules.h"

#include <algorithm>
#include <cmath>

void Prototype::DrawMonsterIndicators(Renderer& r, Point p, const EnemyActor& enemy)
{
    const float z = m_Zoom;
    if (enemy.specialWindup > 0)
    {
        const float radius =
            enemy.Definition().specialRadius > 0 ? enemy.Definition().specialRadius : 1.2f;
        const Point warning = Project(enemy.specialAim.x, enemy.specialAim.y);
        const float scale = z * radius;
        const Color tint = enemy.kind == MonsterKind::FallenPriest ? Color(.65f, .20f, .8f, .24f)
                                                                   : Color(1, .16f, .06f, .26f);
        r.DrawCachedMesh(
            "monster-proxy/v1/warning",
            warning,
            scale,
            [&]()
            {
                r.Ellipse(warning, 56.57f * scale, 28.28f * scale, Color(), 24);
            },
            tint);
    }
    if (enemy.attackWindup > 0 || enemy.guardRemaining > 0)
    {
        r.DrawCachedMesh(
            "monster-proxy/v1/action-ring",
            p,
            z,
            [&]()
            {
                r.Ellipse(p, 27 * z, 12 * z, Color(), 16);
            },
            enemy.guardRemaining > 0 ? Color(.46f, .30f, .85f, .4f) : Color(1, .25f, .08f, .25f));
    }
}

void Prototype::DrawMonsterProxy(Renderer& r, Point p, const EnemyActor& enemy)
{
    const float z = m_Zoom;
    const bool animal = enemy.kind == MonsterKind::AshWolf || enemy.kind == MonsterKind::BlightBear
                        || enemy.kind == MonsterKind::ThornStag;
    const auto pose = enemy.SpritePose();
    const float facing = pose.facingLeft ? -1.0f : 1.0f;
    const std::string key = std::string("monster-proxy/v1/") + enemy.Definition().id;
    const Color body = enemy.kind == MonsterKind::AshWolf         ? Color(.28f, .32f, .30f)
                       : enemy.kind == MonsterKind::BlightBear    ? Color(.30f, .23f, .17f)
                       : enemy.kind == MonsterKind::ThornStag     ? Color(.31f, .32f, .18f)
                       : enemy.kind == MonsterKind::HighDemon     ? Color(.36f, .10f, .15f)
                       : enemy.kind == MonsterKind::FallenPaladin ? Color(.29f, .32f, .39f)
                                                                  : Color(.31f, .16f, .38f);
    r.DrawCachedMesh(
        key + "/shadow",
        p,
        z,
        [&]()
        {
            r.Ellipse(p, animal ? 30 * z : 24 * z, 9 * z, Color(.01f, .015f, .02f, .5f), 16);
        });

    const int legs = animal ? 4 : 2;
    const float legLength = enemy.kind == MonsterKind::BlightBear  ? 22
                            : enemy.kind == MonsterKind::ThornStag ? 29
                            : animal                               ? 17
                                                                   : 25;
    for (int i = 0; i < legs; ++i)
    {
        const float phase = pose.walkCycle * 6.2831853f + (i == 0 || i == 3 ? 0 : 3.1415926f);
        const float stride = std::sin(phase) * 4 * pose.locomotionWeight;
        const float lift = (std::max)(0.0f, std::cos(phase)) * 4 * pose.locomotionWeight;
        const float offset =
            animal ? (i < 2 ? -18.0f : 19.0f) + (i % 2 ? 4.0f : -3.0f) : (i == 0 ? -9.0f : 9.0f);
        const Point foot{p.x + (offset + stride) * z, p.y - lift * z};
        r.DrawCachedMesh(key + "/leg",
                         foot,
                         z,
                         [&]()
                         {
                             r.Line({foot.x, foot.y - legLength * z},
                                    foot,
                                    animal ? 5 * z : 8 * z,
                                    body.Shade(.65f));
                         });
    }

    // Finite species/facing keys; frame time and pose never enter cached body geometry.
    r.DrawCachedMesh(
        key + (pose.facingLeft ? "/left" : "/right"),
        p,
        z,
        [&]()
        {
            const auto point = [&](float x, float y)
            {
                return Point{p.x + x * z * facing, p.y + y * z};
            };
            const Color ember = Color(.96f, .27f, .12f).Radiance(3);
            if (animal)
            {
                const bool bear = enemy.kind == MonsterKind::BlightBear;
                const bool stag = enemy.kind == MonsterKind::ThornStag;
                r.Ellipse(point(-2,
                                bear   ? -36
                                : stag ? -37
                                       : -27),
                          (bear ? 36 : 27) * z,
                          (bear ? 24 : 15) * z,
                          body,
                          12);
                r.Line(point(-25, -29), point(-39, stag ? -42 : -18), 5 * z, body.Shade(.8f));
                const Point head = point(bear ? 31 : stag ? 28 : 30, bear ? -47 : stag ? -62 : -35);
                if (stag)
                {
                    r.Line(point(15, -36), point(23, -62), 13 * z, body);
                }
                r.Ellipse(head, (bear ? 18 : 12) * z, (bear ? 17 : 10) * z, body.Shade(1.2f), 10);
                r.Triangle(point(18,
                                 bear   ? -58
                                 : stag ? -68
                                        : -39),
                           point(18,
                                 bear   ? -69
                                 : stag ? -78
                                        : -53),
                           point(29,
                                 bear   ? -59
                                 : stag ? -65
                                        : -42),
                           body);
                r.Ellipse({head.x + 7 * z * facing, head.y - 2 * z}, 2.3f * z, 2 * z, ember, 8);
                if (stag)
                {
                    for (int side = -1; side <= 1; side += 2)
                    {
                        const float x = 22.0f + side * 5;
                        r.Line(point(x, -67),
                               point(x + side * 8, -92),
                               3 * z,
                               Color(.55f, .47f, .31f));
                        r.Line(point(x + side * 4, -80),
                               point(x + side * 14, -84),
                               2 * z,
                               Color(.55f, .47f, .31f));
                    }
                }
                else if (bear)
                {
                    for (int i = 0; i < 3; ++i)
                    {
                        r.Ellipse(point(-14 + i * 12, -51),
                                  5 * z,
                                  6 * z,
                                  Color(.43f, .52f, .13f),
                                  8);
                    }
                }
                else
                {
                    r.Triangle(point(31, -28),
                               point(40, -28),
                               point(35, -19),
                               Color(.76f, .70f, .52f));
                }
            }
            else if (enemy.kind == MonsterKind::HighDemon)
            {
                r.Triangle(point(-20, -92), point(-66, -104), point(-34, -35), body.Shade(.55f));
                r.Triangle(point(20, -92), point(66, -104), point(34, -35), body.Shade(.55f));
                r.Triangle(point(0, -105), point(-30, -19), point(30, -19), body);
                r.Ellipse(point(0, -110), 17 * z, 19 * z, body.Shade(1.2f), 10);
                r.Triangle(point(-13, -119),
                           point(-30, -142),
                           point(-7, -125),
                           Color(.47f, .35f, .27f));
                r.Triangle(point(13, -119),
                           point(30, -142),
                           point(7, -125),
                           Color(.47f, .35f, .27f));
                r.Ellipse(point(0, -70), 8 * z, 14 * z, ember, 10);
                r.Line(point(-22, -71), point(-43, -35), 9 * z, body);
                r.Line(point(22, -71), point(43, -35), 9 * z, body);
            }
            else
            {
                const bool paladin = enemy.kind == MonsterKind::FallenPaladin;
                r.Triangle(point(0, -68), point(-23, -17), point(23, -17), body);
                r.Ellipse(point(0, -78), 13 * z, 16 * z, body.Shade(1.25f), 10);
                r.Rect(p.x - 7 * z, p.y - 81 * z, 14 * z, 4 * z, ember);
                if (paladin)
                {
                    r.Quad(point(-31, -64),
                           point(-12, -62),
                           point(-14, -29),
                           point(-27, -35),
                           body.Shade(1.5f));
                    r.Line(point(24, -70), point(28, -15), 5 * z, Color(.64f, .61f, .57f));
                    r.Line(point(15, -56), point(34, -57), 4 * z, Color(.61f, .45f, .18f));
                }
                else
                {
                    r.Triangle(point(0, -101), point(-15, -78), point(15, -78), body.Shade(.7f));
                    r.Line(point(29, -10), point(29, -99), 4 * z, Color(.36f, .25f, .18f));
                    r.Ellipse(point(29, -99), 6 * z, 8 * z, Color(.7f, .3f, .84f).Radiance(3), 8);
                }
            }
        },
        enemy.enraged ? Color(1, .62f, .55f) : Color());
}
