// ne_msgmgr_test stand-in: a set event is a world line
#pragma once
#include "net_world.h"
class TSEvent {
public:
    explicit TSEvent(const char* inName) : mName(inName) {}
    void Set() { gWorld->Line("event %s", this->mName); }
    const char* mName;
};
