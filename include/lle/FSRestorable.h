// C:\EEX\Low-Level Engine\File System\FSRestorable.h (file name inferred): something saved and restored through an
// FSFile. Vtable 0x10097538 (no dtor slot). Inline members emitted with File System.cpp's inlines (0x4199..0x41b2).
// The two pure slots are named after what FSChunk calls through them (Load +0x04, Save +0x0c); (provisional names)
#ifndef LLE_FSRESTORABLE_H
#define LLE_FSRESTORABLE_H

#include "lle/LLEForward.h"

class LLE_API FSRestorable {
public:
    // implicit default ctor (0x419e), copy ctor (0x41a7) and operator= (folded 0xd5a1)
    virtual void GetChecksum(ESChecksum& ioChecksum) const {}                   // +0x00 (folded 0x58f8e)
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile) = 0;   // +0x04
    virtual long RepairPointers(unsigned long inFirst, unsigned long inSecond) { return kErrorOK; }   // +0x08 0x4199
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile) = 0;  // +0x0c
};

#endif
