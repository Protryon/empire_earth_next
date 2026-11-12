// New (port), TEST ONLY: the simulation capture (ee/EESimCapture.h; tools/sim_capture.py). Written from the thread
// that runs the leaf (the physics tick in EE-SIM); the lock keeps records whole if another thread captures too.
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <vector>

#include "ee/EESimCapture.h"

namespace {

std::mutex sCaptureLock;
thread_local unsigned int sOnlyTags[4] = {};
thread_local unsigned int sOnlyCount = 0;
std::atomic<bool> sExclusive{false};
std::atomic<bool> sClosed{false};
thread_local bool sExclusiveOwner = false;

FILE* CaptureFile()
{
    // opened once, at the first captured call (the game changes into its data root first: the path is absolute)
    static FILE* sFile = []() -> FILE* {
        const char* thePath = std::getenv("EE_SIM_CAPTURE");
        if (thePath == NULL || thePath[0] == 0)
            return NULL;
        return std::fopen(thePath, "wb");
    }();
    return sFile;
}

}   // namespace

bool EESimCaptureRecord::Active()
{
    return CaptureFile() != NULL;
}

void EESimCaptureRecord::Only(unsigned int inTag)
{
    sOnlyTags[0] = inTag;
    sOnlyCount = inTag != 0 ? 1 : 0;
}

void EESimCaptureRecord::Only(const unsigned int* inTags, unsigned int inCount)
{
    sOnlyCount = inCount < 4 ? inCount : 4;
    for (unsigned int i = 0; i < sOnlyCount; i++)
        sOnlyTags[i] = inTags[i];
}

void EESimCaptureRecord::Exclusive(bool inOn)
{
    sExclusiveOwner = inOn;
    sExclusive.store(inOn);
}

void EESimCaptureRecord::Close()
{
    sClosed.store(true);
}

void EESimCaptureRecord::Commit()
{
    FILE* theFile = CaptureFile();
    if (theFile == NULL || sClosed.load() || (sExclusive.load() && !sExclusiveOwner))
        return;
    if (sOnlyCount != 0) {
        bool theKept = false;
        for (unsigned int i = 0; i < sOnlyCount; i++)
            theKept = theKept || this->mTag == sOnlyTags[i];
        if (!theKept)
            return;
    }
    unsigned int theHeader[2] = {this->mTag, (unsigned int)this->mData.size()};
    std::lock_guard<std::mutex> theGuard(sCaptureLock);
    std::fwrite(theHeader, 4, 2, theFile);
    if (!this->mData.empty())
        std::fwrite(this->mData.data(), 1, this->mData.size(), theFile);
}

void EESimCaptureRecord::Flush()
{
    FILE* theFile = CaptureFile();
    if (theFile == NULL)
        return;
    std::lock_guard<std::mutex> theGuard(sCaptureLock);
    std::fflush(theFile);
}
