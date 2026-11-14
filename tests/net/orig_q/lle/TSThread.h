// ne_queues_test stand-in: a thread of the world
#pragma once
#include "q_world.h"
#include "UString.h"
class TSThread {
public:
    explicit TSThread(int inID) : mID(inID) {}
    void SetName(const UString& inName) { gQ->Line("thread name t%d %s", this->mID, inName.mText.c_str()); }
    int mID;
};
