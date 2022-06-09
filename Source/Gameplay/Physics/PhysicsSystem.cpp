#include "PhysicsSystem.hpp"

#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>

#include <Jolt/Physics/PhysicsSettings.h>

#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>

#include "Gameplay/Physics/PhysicsConstants.hpp"
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

PhysicsSystem::PhysicsSystem() :
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

    AddFloorBody();
}

PhysicsSystem::~PhysicsSystem()
{
    Cleanup();
}

void PhysicsSystem::Cleanup()
{
    if (JPH::Factory::sInstance)
    {
        delete JPH::Factory::sInstance;
        JPH::Factory::sInstance = nullptr;
    }

    // Remove and destroy the floor
    //body_interface.RemoveBody(floor->GetID());
    //body_interface.DestroyBody(floor->GetID());
}

void PhysicsSystem::Process(float /*dt*/)
{
    // TODO: We simulate the physics world in discrete time steps. 60 Hz is a good rate to update the physics system.
    const float cDeltaTime = 1.0f / 60.0f;

    // body_interface.IsActive(sphere_id)
    // 
    //// Output current position and velocity of the sphere
    //Vec3 position = body_interface.GetCenterOfMassPosition(sphere_id);
    //Vec3 velocity = body_interface.GetLinearVelocity(sphere_id);
    //std::cout << "Step " << step << ": Position = (" << position.GetX() << ", " << position.GetY() << ", " << position.GetZ() << "), Velocity = (" << velocity.GetX() << ", " << velocity.GetY() << ", " << velocity.GetZ() << ")" << endl;


    // Step the world
    physicsSystem.Update(cDeltaTime, SPhysicsSystem::CollisionStepsPerUpdate, SPhysicsSystem::IntegrationSubStepsPerUpdate, &tempAllocator, &jobSystem);
}

void PhysicsSystem::OnSceneMajorChange()
{
    // Optional step: Before starting the physics simulation you can optimize the broad phase. This improves collision detection performance (it's pointless here because we only have 2 bodies).
    // You should definitely not call this every frame or when e.g. streaming in a new level section as it is an expensive operation.
    // Instead insert all new objects in batches instead of 1 at a time to keep the broad phase efficient.
    physicsSystem.OptimizeBroadPhase();
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

    // Create the settings for the body itself. Note that here you can also set other properties like the restitution / friction.
    JPH::BodyCreationSettings floor_settings(floor_shape, JPH::Vec3(0.0f, -1.0f, 0.0f), JPH::Quat::sIdentity(), JPH::EMotionType::Static, PhysicsLayers::NON_MOVING);

    // Create the actual rigid body
    JPH::Body* floor = body_interface.CreateBody(floor_settings); // Note that if we run out of bodies this can return nullptr

    // Add it to the world
    body_interface.AddBody(floor->GetID(), JPH::EActivation::DontActivate);

    //BodyCreationSettings sphere_settings(new SphereShape(0.5f), Vec3(0.0f, 2.0f, 0.0f), Quat::sIdentity(), EMotionType::Dynamic, Layers::MOVING);
    //BodyID sphere_id = body_interface.CreateAndAddBody(sphere_settings, EActivation::Activate);

    //// Now you can interact with the dynamic body, in this case we're going to give it a velocity.
    //// (note that if we had used CreateBody then we could have set the velocity straight on the body before adding it to the physics system)
    //body_interface.SetLinearVelocity(sphere_id, Vec3(0.0f, -5.0f, 0.0f));

    // Remove the sphere from the physics system. Note that the sphere itself keeps all of its state and can be re-added at any time.
    // body_interface.RemoveBody(sphere_id);

    // Destroy the sphere. After this the sphere ID is no longer valid.
    // body_interface.DestroyBody(sphere_id);
}