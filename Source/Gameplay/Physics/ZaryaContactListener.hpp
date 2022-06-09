#pragma once

#include <Jolt/Physics/Collision/ContactListener.h>

// A body activation listener gets notified when bodies activate and go to sleep
// Note that this is called from a job so whatever you do here needs to be thread safe.
class ZaryaContactListener : public JPH::ContactListener
{
public:
    virtual JPH::ValidateResult OnContactValidate(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::CollideShapeResult& inCollisionResult) override;

    virtual void OnContactAdded(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings) override;

    virtual void OnContactPersisted(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings) override;

    virtual void OnContactRemoved(const JPH::SubShapeIDPair& inSubShapePair) override;
};