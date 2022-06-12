#pragma once

#include "Engine/Systems/System.hpp"
#include "Engine/Scene/Scene.hpp"
#include "Engine2/Scene2.hpp"

class TransformUpdateSystem
    : public System
{
public:
    TransformUpdateSystem(Scene2* scene_);
    ~TransformUpdateSystem() override;

    void Process(float deltaSeconds) override;

private:

    Scene2* scene = nullptr;
};