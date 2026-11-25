// C:\EEX\Empire Earth\Tech Tree\EETechTreeNode.cpp (file name inferred: no __FILE__; RTTI EETechTreeNode >
// RTSTechTreeNode, vtables 0x446150 / 0x446174). One node of a player's tech tree: 0x1cedce..0x1cfeef (the ctor to the
// inline RTSTechTreeNode members that close the unit; the next unit, EEUIECampaignEditorEvents.cpp, opens with its
// globals chain 0x1cfeef). Classes in ee/EEPlayer.h. Unit flags: the exe's module set.

#include <windows.h>
#include <stdlib.h>
#include <algorithm>

#include "lle/LowLevelEngine.h"
#include "lle/UResourceFile.h"
#include "lle/UIHotKey.h"

#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEGameVariant.h"
#include "ee/EEUnitSetManager.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EEEvent.h"
#include "ee/EEMedia.h"
#include "ee/EESoundObject.h"
#include "ee/EEButtonObject.h"
#include "ee/EEAEManager.h"
#include "ee/EmpireEarth.h"
#include "ee/EEStringIDs.h"
#include "ee/EETechCapture.h"
#include "rust/game/gamecpp.h"

// ------------------------------------------------------------------------- round 9, lane D: the nodes' and epochs' C++


#define P64(x) ((int64_t)(intptr_t)(x))
#define TNODE ((EETechTreeNode*)p)
#define TEPOCH ((EETechTreeEpoch*)p)
#define TVARIANT EEGameVariant::GetGameVariant(gWorld.mGameOptions.mVariant)

double EECore10OpAF(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);   // EEGraphics.cpp
double EECore9OpD7F(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    case O::TkEpochCostFactor: return gWorldTable.GetFloatValue(kWorldUnitEpochCostFactor);
    case O::TkCostSetFactor: return TVARIANT->GetCostSetFactor();
    case O::TkStatsCostScale: return ((EEUnitStats*)p)->GetCostScale();
    case O::TkJitFactor: return gWorld.mGameOptions.mResources == 4 ? gWorldTable.GetFloatValue(kWorldJitManufacturingCostFactorDeathmatch) : gWorldTable.GetFloatValue(kWorldJitManufacturingCostFactor);
    default: return EECore10OpAF(inOp, p, a, f);
    }
}
