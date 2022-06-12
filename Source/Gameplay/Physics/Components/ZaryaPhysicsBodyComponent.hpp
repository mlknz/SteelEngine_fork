#pragma once

#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Body/MotionType.h>

#include "Gameplay/Physics/PhysicsConstants.hpp"

struct ZaryaPhysicsBody final
{
    ZaryaPhysicsBody() = delete;
    ZaryaPhysicsBody(JPH::BodyCreationSettings&& ci) : bodyCreationSettings(std::move(ci)) {};

    JPH::BodyCreationSettings bodyCreationSettings;
    JPH::BodyID bodyID;
    JPH::EMotionType motionType;
    bool isConstructed = false;
};