#include "PhysicsSystem.hpp"

#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>

#include <Jolt/Physics/PhysicsSettings.h>

#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>

#include "Engine2/Components2.hpp"
#include "Gameplay/Physics/PhysicsConstants.hpp"
#include "Gameplay/Physics/Components/ZaryaPhysicsBodyComponent.hpp"
#include "Utils/Assert.hpp"

namespace SPhysicsSystem
{
    // This is the max amount of rigid bodies that you can add to the physics system. If you try to add more you'll get an error.
    // Note: This value is low because this is a simple test. For a real project use something in the order of 65536.
    const uint32_t MaxBodies = 1024 * 16;

    // This determines how many mutexes to allocate to protect rigid bodies from concurrent access. Set it to 0 for the default settings.
    const uint32_t NumBodyMutexes = 0;

    // This is the max amount of body pairs that can be queued at any time (the broad phase will detect overlapping
    // body pairs based on their bounding boxes and will insert them into a queue for the narrowphase). If you make this buffer
    // too small the queue will fill up and the broad phase jobs will start to do narrow phase work. This is slightly less efficient.
    // Note: This value is low because this is a simple test. For a real project use something in the order of 65536.
    const uint32_t MaxBodyPairs = 1024;

    // This is the maximum size of the contact constraint buffer. If more contacts (collisions between bodies) are detected than this
    // number then these contacts will be ignored and bodies will start interpenetrating / fall through the world.
    // Note: This value is low because this is a simple test. For a real project use something in the order of 65536.
    const uint32_t MaxContactConstraints = 1024;

    const uint32_t TempAllocatorSize = 10 * 1024 * 1024;

    // If you take larger steps than 1 / 60th of a second you need to do multiple collision steps in order to keep the simulation stable. Do 1 collision step per 1 / 60th of a second (round up).
    const int CollisionStepsPerUpdate = 1;

    // If you want more accurate step results you can do multiple sub steps within a collision step. Usually you would set this to 1.
    const int IntegrationSubStepsPerUpdate = 1;

    static bool ObjectsCanCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2)
    {
        switch (inObject1)
        {
        case PhysicsLayers::NON_MOVING:
            return inObject2 == PhysicsLayers::MOVING; // Non moving only collides with moving
        case PhysicsLayers::MOVING:
            return true; // Moving collides with everything
        default:
            Assert(false);
            return false;
        }
    };

    static bool BroadPhaseCanCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2)
    {
        switch (inLayer1)
        {
        case PhysicsLayers::NON_MOVING:
            return inLayer2 == BroadPhaseLayers::MOVING;
        case PhysicsLayers::MOVING:
            return true;
        default:
            Assert(false);
            return false;
        }
    }
}

PhysicsSystem::PhysicsSystem(Scene2* scene_) :
    scene(scene_),
    tempAllocator(SPhysicsSystem::TempAllocatorSize),
    jobSystem(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, std::thread::hardware_concurrency() - 1)
{
    using namespace SPhysicsSystem;

    Cleanup();

    JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();

    physicsSystem.Init(MaxBodies, NumBodyMutexes, MaxBodyPairs, MaxContactConstraints, bpLayerInterface, BroadPhaseCanCollide, ObjectsCanCollide);

    physicsSystem.SetBodyActivationListener(&bodyActivationListener);
    physicsSystem.SetContactListener(&contactListener);

    scene->on_construct<ZaryaPhysicsBody>().connect<&PhysicsSystem::ConstructPhysicsBody>(this);

    AddFloorBody();
}

PhysicsSystem::~PhysicsSystem()
{
    scene->on_construct<ZaryaPhysicsBody>().disconnect<&PhysicsSystem::ConstructPhysicsBody>(this);
    Cleanup();
}

void PhysicsSystem::Cleanup()
{
    if (JPH::Factory::sInstance)
    {
        delete JPH::Factory::sInstance;
        JPH::Factory::sInstance = nullptr;
    }

    // Remove body from the physics system. Note that the body itself keeps all of its state and can be re-added at any time.
    //body_interface.RemoveBody(floor->GetID());
    // Destroy body. After this the body ID is no longer valid.
    //body_interface.DestroyBody(floor->GetID());
}

void PhysicsSystem::Process(float deltaSeconds)
{
    timeSinceUpdate += deltaSeconds;

    if (timeSinceUpdate < physicsUpdateStepSeconds)
    {
        return;
    }

    timeSinceUpdate -= physicsUpdateStepSeconds;

    const float dt = physicsUpdateStepSeconds * physicsTimeSpeed;
    physicsSystem.Update(dt, SPhysicsSystem::CollisionStepsPerUpdate, SPhysicsSystem::IntegrationSubStepsPerUpdate, &tempAllocator, &jobSystem);

    JPH::BodyInterface& bodyInterface = physicsSystem.GetBodyInterface();
    // Update visual transforms
    auto view = scene->view<const ZaryaPhysicsBody>();

    JPH::Vec3 pos;
    JPH::Quat rot;
    glm::quat myQuat;
    glm::mat4 finalTransform;

    for (auto entity : view)
    {
        const auto &zaryaPhysicsBody = view.get<const ZaryaPhysicsBody>(entity);
        
        if (zaryaPhysicsBody.motionType == JPH::EMotionType::Static)
        {
            continue;
        }

        if (!bodyInterface.IsAdded(zaryaPhysicsBody.bodyID))
        {
            continue;
        }
        
        bodyInterface.GetPositionAndRotation(zaryaPhysicsBody.bodyID, pos, rot);

        myQuat = glm::quat(rot.GetW(), rot.GetX(), rot.GetY(), rot.GetZ());

        finalTransform = glm::translate(glm::identity<glm::mat4>(), glm::vec3(pos.GetX(), pos.GetY(), pos.GetZ())) * glm::mat4_cast(myQuat);

        // set of patched objects can be used with checking if transform changed
        scene->patch<TransformComponent>(entity, [&finalTransform](TransformComponent& tr) {
            tr.localTransform = finalTransform;
        });
    }
}

void PhysicsSystem::OnSceneMajorChange()
{
    // Optional step: Before starting the physics simulation you can optimize the broad phase. This improves collision detection performance.
    // You should definitely not call this every frame or when e.g. streaming in a new level section as it is an expensive operation.
    // Instead insert all new objects in batches instead of 1 at a time to keep the broad phase efficient.
    physicsSystem.OptimizeBroadPhase();
}

void PhysicsSystem::ConstructPhysicsBody(entt::registry& registry, entt::entity entity)
{
    registry.patch<ZaryaPhysicsBody>(entity, [this](ZaryaPhysicsBody& zaryaPhysicsBody) {
        JPH::BodyInterface& bodyInterface = physicsSystem.GetBodyInterface();
        JPH::BodyID newBodyID = bodyInterface.CreateAndAddBody(zaryaPhysicsBody.bodyCreationSettings, JPH::EActivation::Activate);
        zaryaPhysicsBody.bodyID = newBodyID;
        zaryaPhysicsBody.motionType = zaryaPhysicsBody.bodyCreationSettings.mMotionType;
        zaryaPhysicsBody.isConstructed = true;

        bodyInterface.SetLinearVelocity(zaryaPhysicsBody.bodyID, JPH::Vec3(0.0f, 5.0f, 0.0f));
    });
}

void PhysicsSystem::AddFloorBody()
{
    // The main way to interact with the bodies in the physics system is through the body interface. There is a locking and a non-locking
    // variant of this. We're going to use the locking version (even though we're not planning to access bodies from multiple threads)
    JPH::BodyInterface& body_interface = physicsSystem.GetBodyInterface();

    // Next we can create a rigid body to serve as the floor, we make a large box
    // Create the settings for the collision volume (the shape). 
    // Note that for simple shapes (like boxes) you can also directly construct a BoxShape.
    JPH::BoxShapeSettings floor_shape_settings(JPH::Vec3(100.0f, 1.0f, 100.0f));

    // Create the shape
    JPH::ShapeSettings::ShapeResult floor_shape_result = floor_shape_settings.Create();
    JPH::ShapeRefC floor_shape = floor_shape_result.Get(); // We don't expect an error here, but you can check floor_shape_result for HasError() / GetError()

    JPH::Quat floorRot = JPH::Quat::sRotation(JPH::Vec3Arg(0.0f, 0.0f, 1.0f), 0.4f); //JPH::Quat::sIdentity()
    // Create the settings for the body itself. Note that here you can also set other properties like the restitution / friction.
    JPH::BodyCreationSettings floor_settings(floor_shape, JPH::Vec3(0.0f, -2.0f, 0.0f), floorRot, JPH::EMotionType::Static, PhysicsLayers::NON_MOVING);

    // Create the actual rigid body
    JPH::Body* floor = body_interface.CreateBody(floor_settings); // Note that if we run out of bodies this can return nullptr

    // Add it to the world
    body_interface.AddBody(floor->GetID(), JPH::EActivation::DontActivate);
}