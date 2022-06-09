#include "ZaryaBodyActivationListener.hpp"

#include "Utils/Logger.hpp"

void ZaryaBodyActivationListener::OnBodyActivated(const JPH::BodyID& /*inBodyID*/, uint64_t /*inBodyUserData*/)
{
    LogI << "A body got activated\n";
}

void ZaryaBodyActivationListener::OnBodyDeactivated(const JPH::BodyID& /*inBodyID*/, uint64_t /*inBodyUserData*/)
{
    LogI << "A body went to sleep\n";
}