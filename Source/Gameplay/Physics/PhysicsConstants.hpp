#pragma once

// 
// Layer that objects can be in, determines which other objects it can collide with
// Typically you at least want to have 1 layer for moving bodies and 1 layer for static bodies, but you can have more
// layers if you want. E.g. you could have a layer for high detail collision (which is not used by the physics simulation
// but only if you do collision testing).
namespace PhysicsLayers
{
    static constexpr uint8_t NON_MOVING = 0;
    static constexpr uint8_t MOVING = 1;
    static constexpr uint8_t NUM_LAYERS = 2;
};

