// New (port), TEST ONLY: the simulation capture (ee/EESimCapture.h; tools/sim_capture.py). Written from the thread
// that runs the leaf (the physics tick in EE-SIM); the lock keeps records whole if another thread captures too.
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <vector>

#include "ee/EESimCapture.h"

namespace {

std::mutex sCaptureLock;

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

void EESimCaptureRecord::Commit()
{
    FILE* theFile = CaptureFile();
    if (theFile == NULL)
        return;
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
