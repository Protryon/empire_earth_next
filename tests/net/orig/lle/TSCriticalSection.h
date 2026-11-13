// ne_msgmgr_test stand-in: the sequences run on one thread
#pragma once
class TSCriticalSection {
public:
    void Enter() {}
    void Leave() {}
};
