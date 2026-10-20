#pragma once

// Render-only state shared by all units; independent of simulation tick frequency.
struct UnitSpritePose
{
    bool moving = false;
    bool facingLeft = false;
    float walkCycle = 0;
    float locomotionWeight = 0;
    float attackProgress = -1;
};
