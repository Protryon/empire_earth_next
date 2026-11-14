// ne_client_test stand-in: a received file of the world
#pragma once
#include "c_world.h"
class FSFile {
public:
    explicit FSFile(int inID) : mID(inID) { gQ->Line("fsfile new f%d", inID); }
    ~FSFile() { gQ->Line("fsfile free f%d", this->mID); }
    long Write(const unsigned char* inData, unsigned long inSize, unsigned long& outWritten)
    {
        long r = gQ->Next() % 8 == 0 ? 9 : 0;
        gQ->Line("fsfile write f%d %s = %ld", this->mID, CHex(inData, inSize).c_str(), r);
        outWritten = r == 0 ? inSize : 0;
        return r;
    }
    long Close() { gQ->Line("fsfile close f%d", this->mID); return 0; }
    long Delete() { gQ->Line("fsfile delete f%d", this->mID); return 0; }
    int mID;
};
