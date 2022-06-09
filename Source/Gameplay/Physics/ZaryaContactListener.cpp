#include "ZaryaContactListener.hpp"

#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Collision/CollideShape.h>

#include "Utils/Logger.hpp"

JPH::ValidateResult ZaryaContactListener::OnContactValidate(const JPH::Body& /*inBody1*/, const JPH::Body& /*inBody2*/, const JPH::CollideShapeResult& /*inCollisionResult*/)
{
     LogI << "Contact validate callback\n";

    // Allows you to ignore a contact before it is created (using layers to not make objects collide is cheaper!)
    return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
}

void ZaryaContactListener::OnContactAdded(const JPH::Body& /*inBody1*/, const JPH::Body& /*inBody2*/, const JPH::ContactManifold& /*inManifold*/, JPH::ContactSettings& /*ioSettings*/)
{
    LogI << "A contact was added\n";
}

void ZaryaContactListener::OnContactPersisted(const JPH::Body& /*inBody1*/, const JPH::Body& /*inBody2*/, const JPH::ContactManifold& /*inManifold*/, JPH::ContactSettings& /*ioSettings*/)
{
    LogI << "A contact was persisted\n";
}

void ZaryaContactListener::OnContactRemoved(const JPH::SubShapeIDPair& /*inSubShapePair*/)
{
    LogI << "A contact was removed\n";
}