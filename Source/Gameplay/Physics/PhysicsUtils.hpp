#pragma once

#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>

#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>

namespace ZaryaPhysics
{
    inline JPH::BodyCreationSettings ParsePhysicsBodyCreateInfo()
    {
        using namespace JPH;
        BodyCreationSettings sphere_settings(new BoxShape(JPH::Vec3Arg(0.5f, 0.5f, 0.5f)), Vec3(0.0f, 2.0f, 0.0f), Quat::sIdentity(), EMotionType::Dynamic, PhysicsLayers::MOVING);

        return sphere_settings;
    }
};

