// ne_queues_test stand-in: a record file of the world
#pragma once
#include "q_world.h"
enum { kfsaRead = 1, kfsaWrite = 2 };
class FSFileSpec {
public:
    explicit FSFileSpec(int inID) : mID(inID) {}
    long Create() { return gQ->FileCreate(this->mID); }
    long Delete() { return gQ->FileDelete(this->mID); }
    long Open(unsigned long inAccess) { return gQ->FileOpen(this->mID, inAccess); }
    long Close() { gQ->FileClose(this->mID); return 0; }
    long Write(const unsigned char* inData, unsigned long inSize, unsigned long& outWritten)
    {
        outWritten = inSize;
        return gQ->FileWrite(this->mID, inData, inSize);
    }
    // FSFile's unsigned long adapters
    long Write(const unsigned long& inValue)
    {
        uint32_t theValue = (uint32_t)inValue;
        return gQ->FileWrite(this->mID, (const unsigned char*)&theValue, 4);
    }
    long Read(unsigned long& outValue)
    {
        uint32_t theValue = (uint32_t)outValue;
        long theResult = gQ->FileReadU32(this->mID, theValue);
        outValue = theValue;
        return theResult;
    }
    long Read(unsigned char* outData, unsigned long inSize, unsigned long& outRead)
    {
        outRead = inSize;
        return gQ->FileRead(this->mID, outData, inSize);
    }
    int mID;
};
