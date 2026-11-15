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

// FUNCTION: LLE 0x1008109c
// Writes the reference-leak report's title (nothing tracks references any more: the original's debug map was
// never filled, so its report was the title alone)
void UReferenceCounter::DumpAllLeakedReferences(FSFileSpec& inFile)
{
    fprintf(inFile, "REFERENCE COUNTED MEMORY LEAKS\n\n");
}

// Compiler-generated for this unit (names in analysis/lle/names.txt):
// TEMPLATE: LLE 0x1008150a
// TEMPLATE: LLE 0x10081532
// TEMPLATE: LLE 0x1008155d
// TEMPLATE: LLE 0x10081585
// TEMPLATE: LLE 0x100815b0
// TEMPLATE: LLE 0x100815d8
// TEMPLATE: LLE 0x10081603
// TEMPLATE: LLE 0x100818b1
// TEMPLATE: LLE 0x100818eb
// TEMPLATE: LLE 0x1008195c
// TEMPLATE: LLE 0x100819cd
// TEMPLATE: LLE 0x10081a83
// TEMPLATE: LLE 0x10081af4
// TEMPLATE: LLE 0x10081b26
// TEMPLATE: LLE 0x10081b58
// TEMPLATE: LLE 0x10081b8d
// TEMPLATE: LLE 0x10081bbf
// TEMPLATE: LLE 0x10081d53
// TEMPLATE: LLE 0x10081d7c
// SYNTHETIC: LLE 0x1008104b
// SYNTHETIC: LLE 0x10081055
// SYNTHETIC: LLE 0x10081075
// SYNTHETIC: LLE 0x10081081
