#include "TransformUpdateSystem.hpp"

#include "Engine2/Components2.hpp"
#include "Utils/Assert.hpp"

TransformUpdateSystem::TransformUpdateSystem(Scene2* scene_) :
    scene(scene_)
{
}

TransformUpdateSystem::~TransformUpdateSystem()
{
}

void TransformUpdateSystem::Process(float /*dt*/)
{
    // todo: add transform animation tracks
    auto view = scene->view<TransformComponent>();
    for (auto entity : view) // todo: observer instead of ALL + update only self and children
    {
        // auto& tr = scene->get<TransformComponent>(entity);

        scene->patch<TransformComponent>(entity, [this](TransformComponent& tr) {
            tr.worldTransform = TransformComponent::AccumulateTransform(tr);
        });
    }
}
