#pragma once

#include <Jolt/Physics/Body/BodyActivationListener.h>

// A body activation listener gets notified when bodies activate and go to sleep
// Note that this is called from a job so whatever you do here needs to be thread safe
class ZaryaBodyActivationListener : public JPH::BodyActivationListener
{
public:
    virtual void OnBodyActivated(const JPH::BodyID& inBodyID, uint64_t inBodyUserData) override;
    virtual void OnBodyDeactivated(const JPH::BodyID& inBodyID, uint64_t inBodyUserData) override;
};