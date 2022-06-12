#pragma once

#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include "Engine/Systems/System.hpp"
#include "Engine/Scene/Scene.hpp"
#include "Engine2/Scene2.hpp"

#include "Gameplay/Physics/ZaryaBroadPhase.hpp"
#include "Gameplay/Physics/ZaryaBodyActivationListener.hpp"
#include "Gameplay/Physics/ZaryaContactListener.hpp"

class PhysicsSystem
    : public System
{
public:
    PhysicsSystem(Scene2* scene_);
    ~PhysicsSystem() override;

    void Process(float deltaSeconds) override;

private:
    void Cleanup();
    void OnSceneMajorChange();
    void ConstructPhysicsBody(entt::registry&, entt::entity);

    void AddFloorBody();

    Scene2* scene = nullptr;

    float timeSinceUpdate = 0.0f;
    float physicsUpdateStepSeconds = 0.0166666f; // todo: switch to 30fps if FPS is < 60
    float physicsTimeSpeed = 1.0f; // todo: make tunable from GUI

    JPH::TempAllocatorImpl tempAllocator;
    JPH::JobSystemThreadPool jobSystem;

    JPH::PhysicsSystem physicsSystem;

    BPLayerInterfaceImpl bpLayerInterface;
    ZaryaBodyActivationListener bodyActivationListener;
    ZaryaContactListener contactListener;
};