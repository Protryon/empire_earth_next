// C:\EEX\Empire Earth\AI Thread\EEAIAStarBoundedPathGoalModifiers.cpp (file name inferred: no __FILE__; RTTI
// classes EEAIBoundedPathGoalModifierBase .. EEAIGPGModifierSpaceRoutePlanner, vtables 0x439bc4..0x439ce0). The path
// goals' modifiers: 0xf53b..0x10bc7. Port: the modifiers are Rust objects (ee_sim::astar modifier_obj.rs, PORT_LEARNINGS
// "AI Thread cluster, round 4"); what is left is the two facts they ask of the C++ and the gates' part tests that sat
// in this unit (EEUCGate::IsSidePart, out of line after its first user).
#include <windows.h>
#include <vector>
#include <map>
#include <math.h>
#include <algorithm>

#include "lle/LowLevelEngine.h"
#include "lle/U2DRectangle.h"

#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEInfluenceTable.h"
#include "rust/sim/astar.h"

// The attackable modifier's references of its walker and target (modifier_obj.rs)
extern "C" void ee_astar_mod_host_unit_reference(uint64_t inUnit, bool inAdd)
{
    EEComplexUnit* theUnit = (EEComplexUnit*)(uintptr_t)inUnit;
    if (inAdd)
        theUnit->Reference();
    else
        theUnit->Dereference();
}

// The enemy avoidance's influence table (its Rust table) and region size, NULL without one
extern "C" const void* ee_astar_mod_host_influence(int32_t* outRegionSize)
{
    EEInfluenceTable* theTable = gWorld.mInfluenceTable;
    *outRegionSize = theTable ? (int32_t)theTable->mRegionSize : 0;
    return theTable ? theTable->mTable : NULL;
}

// Returns whether a gate part is an end part (last, with a part before it) or the middle part
// (EEUCGate::IsSidePart: out of line after its first user Virtual10)
bool EEUnitClass::IsSidePart(EEComplexUnit* inGate)
{
    return inGate->mNextPart == NULL && inGate->mPreviousPart != NULL || this->IsMiddlePart(inGate);
}

// Returns whether a gate part is second to last with a part before it (the middle of a three-part gate)
bool EEUnitClass::IsMiddlePart(EEComplexUnit* inGate)
{
    return inGate->mNextPart != NULL && inGate->mNextPart->mNextPart == NULL && inGate->mPreviousPart != NULL;
}
