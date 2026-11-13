// ne_msgmgr_test stand-in
#pragma once
#include "lle/TSCriticalSection.h"
struct TSScheduler {
    static long CreateCriticalSection(TSCriticalSection*& outSection) { outSection = new TSCriticalSection; return 0; }
    static void DestroyCriticalSection(TSCriticalSection* inSection) { delete inSection; }
};
