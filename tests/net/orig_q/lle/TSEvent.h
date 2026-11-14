// ne_queues_test stand-in: an event of the world
#pragma once
#include "q_world.h"
const unsigned long kTSWaitInfinite = kQInfinite;
class TSEvent {
public:
    explicit TSEvent(int inID) : mID(inID) {}
    long Set() { gQ->Set(this->mID); return 0; }
    long Clear() { gQ->Clear(this->mID); return 0; }
    long Wait(unsigned long inTimeout) { return gQ->Wait(this->mID, inTimeout); }
    int mID;
};
