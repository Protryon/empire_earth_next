// C:\EEX\Empire Earth\Calamities\EECCLineOfSight.cpp (file name inferred: no __FILE__). The line-of-sight calamity
// class: 0xb1f4f..0xb2034. Unit flags: the exe's module set. Reset is ICF-folded (0x157269).
// Data (.rdata, in order): -10000.0f 0x43cac4, the vtable 0x43cacc (COL 0x43cac8).
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/FSRestorable.h"

#include "ee/EEEffectManager.h"
#include "ee/EETechCapture.h"
#include "ee/EECalamityUpdate.h"
#include "ee/EECalamity.h"
#include "ee/EECalamityClass.h"
#include "ee/EEGlobalCalamityClass.h"
#include "ee/EECCLineOfSight.h"
#include "ee/EEAreaEffect.h"
#include "ee/EEUnit.h"

// FUNCTION: EXE 0x4b202f
// Always allows the cast (returns 0)
long EECCLineOfSight::CanCastAt(EEComplexUnit* inUnit, GE3DPoint& inLocation, unsigned long inX, unsigned long inY,
                                bool inCheck, EEUnit* inTargetUnit)
{
    // the rules are Rust (ee_tech calamity_cast.rs; ee/EECalamityUpdate.h)
    EECalamityCanCastScope theCapture;   // CCAS (TEST ONLY)
    return theCapture.Record(this, inUnit, inLocation, inX, inY, inCheck, inTargetUnit,
                             EECalamityCanCastAt(this, EeCalamityCastMode::Global, inUnit, inX, inY, inCheck, inTargetUnit));
}
