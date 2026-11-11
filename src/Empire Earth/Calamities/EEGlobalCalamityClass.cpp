// C:\EEX\Empire Earth\Calamities\EEGlobalCalamityClass.cpp (file name inferred). The calamity classes that are not
// cast at a location. Every member is trivial and ICF-folded into another unit's identical function, so the unit
// has no code range of its own (like the engine's GELight.cpp). Unit flags: the exe's module set.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "lle/FSRestorable.h"

#include "ee/EEEffectManager.h"
#include "ee/EETechCapture.h"
#include "ee/EECalamity.h"
#include "ee/EECalamityClass.h"
#include "ee/EEGlobalCalamityClass.h"

// ICF-FOLDED: EXE 0x636201
// (`return 1`)
long EEGlobalCalamityClass::GetTargetKind()
{
    return 1;
}

// ICF-FOLDED: EXE 0x4b202f
// (`return 0`)
long EEGlobalCalamityClass::CanCastAt(EEComplexUnit* inUnit, GE3DPoint& inLocation, unsigned long inX,
                                      unsigned long inY, bool inCheck, EEUnit* inTargetUnit)
{
    EECalamityCanCastScope theCapture;   // CCAS (TEST ONLY)
    return theCapture.Record(this, inUnit, inLocation, inX, inY, inCheck, inTargetUnit, 0);
}
