// ne_server_test stand-in for the engine headers NESynchronousServer.cpp sees: ne_client_test's (the error codes,
// ESVERIFY_AT as a world line, the world's clock and floating-point control, strings, events with WaitAny, threads,
// critical sections) and the server's few more: kErrorNotFound, the read access, PlatSleep, the transferred files.
#pragma once
#include "../../orig_c/lle/LowLevelEngine.h"
#include "s_world.h"
enum { kErrorNotFound = 0x10 };
enum { kFileAccessRead = 1 };
inline void PlatSleep(unsigned long inMs) { gQ->Line("sleep %lu", inMs); }

// a file a transfer sends: its name and length; the world reads it (bytes from its id) and may refuse to open it
class FSFileSpec {
public:
    FSFileSpec() {}
    FSFileSpec(int inID, unsigned long inLength) : mID(inID), mLength(inLength)
    {
        snprintf(this->mName, sizeof(this->mName), "save%d.ees", inID);
    }
    FSFileSpec& operator=(const FSFileSpec& inSpec)
    {
        this->mID = inSpec.mID;
        this->mLength = inSpec.mLength;
        memcpy(this->mName, inSpec.mName, sizeof(this->mName));
        this->mOpen = inSpec.mOpen;
        this->mPos = inSpec.mPos;
        return *this;
    }
    bool GetIsOpen() const { return this->mOpen; }
    long Open(unsigned long inAccess)
    {
        long r = gQ->Next() % 6 == 0 ? 9 : 0;
        if (r == 0) {
            this->mOpen = true;
            this->mPos = 0;
        }
        gQ->Line("file open s%d %lu = %ld", this->mID, inAccess, r);
        return r;
    }
    long Close()
    {
        this->mOpen = false;
        gQ->Line("file close s%d", this->mID);
        return 0;
    }
    long GetLength(unsigned long& outLength) const
    {
        outLength = this->mLength;
        gQ->Line("file length s%d = %lu", this->mID, this->mLength);
        return 0;
    }
    long Read(unsigned char* outData, unsigned long inSize, unsigned long& outRead)
    {
        unsigned long n = this->mLength - this->mPos < inSize ? this->mLength - this->mPos : inSize;
        for (unsigned long i = 0; i < n; i++)
            outData[i] = (unsigned char)(this->mID * 31 + this->mPos + i);
        this->mPos += n;
        outRead = n;
        long r = gQ->Next() % 40 == 0 ? 9 : 0;
        gQ->Line("file read s%d %lu = %ld %lu", this->mID, inSize, r, n);
        return r;
    }
    const char* GetFile() const { return this->mName; }
    int mTok = ++gS->mNextFileTok;   // this object (copies have their own)
    int mID = 0;
    unsigned long mLength = 0;
    char mName[32] = {};
    bool mOpen = false;
    unsigned long mPos = 0;
};
