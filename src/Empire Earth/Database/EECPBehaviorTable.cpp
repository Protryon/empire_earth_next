// C:\EEX\Empire Earth\Database\EECPBehaviorTable.cpp (file name inferred: no __FILE__, no RTTI; the loader of
// db\dbcpbehavior.dat, Empire Earth.cpp 0x141f20; it sorts between EEColorTable.cpp and EEDatabase.cpp). 0xd5f7e..
// 0xd6035: the record array's init chain (the inline ctor in a loop, no atexit), Load. Unit flags: the exe's
// module set.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/FSFileSpec.h"

#include "ee/EECPBehavior.h"

EECPBehavior gCPBehaviors[129];

// FUNCTION: EXE 0x4d5f97
// Loads the computer-player behaviour values into their slots of the table, squaring the maximum attack response
// range; returns the last read status
// NEAR: the original squares mF30 with a second memory read (fld [x]; fmul [x]), ours reuses st(0)
long EECPBehavior::Load(FSFileSpec* inFile)
{
    unsigned long theCount = 0;
    long theResult;
    unsigned long theRead = 0;
    EECPBehavior theRecord;
    inFile->Open(kFileAccessRead);
    theResult = inFile->Read(theCount);
    if (theResult == kErrorOK) {
        for (unsigned long i = 0; i < theCount; i++) {
            theResult = inFile->Read(reinterpret_cast<unsigned char*>(&theRecord), sizeof(theRecord), theRead);
            if (!(theRecord.mIndex > kCPBehaviorNone))
                continue;
            gCPBehaviors[theRecord.mIndex] = theRecord;
            if (theRecord.mIndex == kCPBehaviorMaximumAttackResponseRange)
                gCPBehaviors[kCPBehaviorMaximumAttackResponseRange].mFloatValue *= gCPBehaviors[kCPBehaviorMaximumAttackResponseRange].mFloatValue;
        }
    }
    inFile->Close();
    return theResult;
}

// SYNTHETIC: EXE 0x4d5f7e
// SYNTHETIC: EXE 0x4d5f83
