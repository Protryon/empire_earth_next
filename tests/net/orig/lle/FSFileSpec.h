// ne_msgmgr_test stand-in: a file of the world (a name, a length, a read position); every effect is a world line
#pragma once
#include "net_world.h"
enum { kFileAccessRead = 1 };
class FSFileSpec {
public:
    FSFileSpec() : mID(0) {}
    ~FSFileSpec() { if (this->mID != 0) gWorld->FileFree(this->mID); }
    FSFileSpec& operator=(const FSFileSpec& inFile) { this->mID = gWorld->FileClone(inFile.mID); return *this; }
    long Open(int inAccess) { return gWorld->FileOpen(this->mID); }
    void Close() { gWorld->FileClose(this->mID); }
    bool GetIsOpen() const { return gWorld->File(this->mID).mOpen; }
    void GetLength(unsigned long& outLength) const { outLength = gWorld->File(this->mID).mLength; }
    long Read(void* outData, unsigned long inSize, unsigned long& outRead)
        { return gWorld->FileRead(this->mID, (unsigned char*)outData, inSize, outRead); }
    const char* GetFile() const { return gWorld->File(this->mID).mName.c_str(); }
    int mID;
};
