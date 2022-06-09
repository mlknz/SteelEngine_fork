#pragma once

#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include "Engine/Systems/System.hpp"

#include "Gameplay/Physics/ZaryaBroadPhase.hpp"
#include "Gameplay/Physics/ZaryaBodyActivationListener.hpp"
#include "Gameplay/Physics/ZaryaContactListener.hpp"

class PhysicsSystem
    : public System
{
public:
    PhysicsSystem();
    ~PhysicsSystem() override;

    void Process(float deltaSeconds) override;

private:
    void Cleanup();
    void OnSceneMajorChange();

    void AddFloorBody();

    JPH::TempAllocatorImpl tempAllocator;
    JPH::JobSystemThreadPool jobSystem;

    JPH::PhysicsSystem physicsSystem;

    BPLayerInterfaceImpl bpLayerInterface;
    ZaryaBodyActivationListener bodyActivationListener;
    ZaryaContactListener contactListener;
};