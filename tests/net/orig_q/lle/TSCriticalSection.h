// ne_queues_test stand-in: the world's threads run one at a time, so a lock has nothing to do (the Rust's lock is
// its own and makes no call out)
#pragma once
class TSCriticalSection {
public:
    void Enter() {}
    void Leave() {}
};
