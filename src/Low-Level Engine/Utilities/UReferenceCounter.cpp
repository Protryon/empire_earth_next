// C:\EEX\Low-Level Engine\Utilities\UReferenceCounter.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x1008104b..0x10081d93: the sReferenceDebugInfo init chain (guarded atexit dtor), then the reference-tracking
// members (DumpAllLeakedReferences, DereferenceWithoutDecrementingCount, Dereference, Reference, GetBucketMap), then
// the map COMDATs. The tracking had no caller (its map stayed empty): only the report's title is left (LLE lane
// round 6). The inline members are emitted by File System.cpp (0x1000436c..). URegistry.cpp (now UConfig.cpp) starts at
// 0x10081d93.
#include <windows.h>
#include <stdio.h>
#include <map>

#include "lle/LowLevelEngine.h"

// Writes the reference-leak report's title (nothing tracks references any more: the original's debug map was
// never filled, so its report was the title alone)
void UReferenceCounter::DumpAllLeakedReferences(FSFileSpec& inFile)
{
    fprintf(inFile, "REFERENCE COUNTED MEMORY LEAKS\n\n");
}
