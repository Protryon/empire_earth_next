// C:\EEX\Empire Earth\AI Thread\EECPASGSubmarine.cpp (file name inferred: no __FILE__). 0x513b7..0x51805.
#include <windows.h>
#include <vector>
#include <set>
#include <map>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GE3DLine.h"
#include "lle/GEViewport.h"
#include "lle/GETransformation.h"
#include "lle/GEPhysicalModel.h"
#include "lle/GETerrainMesh.h"
#include "lle/FSRestorable.h"
#include "lle/USingleton.h"

#include "ee/EmpireEarth.h"
#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EECalamity.h"
#include "ee/EECalamityClass.h"
#include "ee/EECalamityManager.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EECPILocation.h"
#include "ee/EECPASTargetUnit.h"
#include "ee/EECPAttackSubGroup.h"
#include "ee/EECPAttackSubGroupUnit.h"
#include "ee/EECPAttackGroup.h"
#include "ee/EECPASGSea.h"
#include "ee/EECPASGSubmarine.h"

#include "ee/EEComputerPlayer.h"
#include "ee/EEAIAStarStorage.h"
#include "ee/RTSRegionTable.h"
#include "ee/EECPAAirTargetFilters.h"
#include "ee/EECPAreaTargetCollector.h"

// FUNCTION: EXE 0x4513b7
// Creates a submarine attack sub-group for the attack group, not yet able to attack ships
EECPASGSubmarine::EECPASGSubmarine(EECPAttackGroup* inGroup, long inKind) : EECPASGSea(inGroup, inKind), mCanAttackShips(false)
{
}

// SYNTHETIC: EXE 0x4513da
// (the scalar deleting destructor; 0x1c bytes)

// FUNCTION: EXE 0x4513f6
// Destroys the submarine sub-group (base members only)
EECPASGSubmarine::~EECPASGSubmarine()
{
}

// FUNCTION: EXE 0x451401
// Clears the can-attack-ships flag, then shuts down as a naval sub-group
void EECPASGSubmarine::Shutdown()
{
    this->mCanAttackShips = false;
    EECPASGSea::Shutdown();
}

// FUNCTION: EXE 0x45140d
// Sets and returns whether any unit's ammunition does damage against ships
bool EECPASGSubmarine::UpdateCanAttackShips()
{
    this->mCanAttackShips = false;
    for (long i = this->mUnits.size() - 1; i >= 0 && !this->mCanAttackShips; i--)
        this->mCanAttackShips = gU4E8708[3][this->mUnits[i]->GetUnit()->GetUnitClass()->mAmmoType] != 0.0f;
    return this->mCanAttackShips;
}

// FUNCTION: EXE 0x451479
// Picks units to split off: the largest group when units sit in several bodies of water, else all kind-0x15
// units (never all of them), else the first half
void EECPASGSubmarine::PickSplitUnits(std::vector<EEComplexUnit*>& outUnits)
{
    std::multimap<long, EECPAttackSubGroupUnit*> theRanked;
    std::map<unsigned short, std::vector<EECPAttackSubGroupUnit*>*> theGroups;
    std::vector<EECPAttackSubGroupUnit*>* theLargest = NULL;
    long theHalf = this->GetNumUnits() / 2;
    if (this->GetNumUnits() > 1) {
        this->GroupUnitsByWaterArea(theGroups, 1, true);
        if (theGroups.size() > 1) {
            std::map<unsigned short, std::vector<EECPAttackSubGroupUnit*>*>::iterator theGroup;
            for (theGroup = theGroups.begin(); theGroup != theGroups.end(); ++theGroup)
                if (theLargest == NULL || theGroup->second->size() > theLargest->size())
                    theLargest = theGroup->second;
            if (theLargest != NULL) {
                for (long i = theLargest->size() - 1; i >= 0; i--)
                    outUnits.push_back(((*theLargest)[i])->GetUnit());
            }
        }
    }
    if (theLargest == NULL && this->GetNumUnits() > 1) {
        for (long i = this->GetNumUnits() - 1; i >= 0; i--) {
            EECPAttackSubGroupUnit* theUnit = i < this->GetNumUnits() ? this->mUnits[i] : NULL;
            long theKind = this->GetUnitUnit(i)->GetUnitClass()->GetAIKind();
            theRanked.insert(std::multimap<long, EECPAttackSubGroupUnit*>::value_type(theKind, theUnit));
        }
        std::multimap<long, EECPAttackSubGroupUnit*>::iterator theEntry;
        for (theEntry = theRanked.begin(); theEntry != theRanked.end(); ++theEntry)
            if (theEntry->first == 0x15)
                outUnits.push_back(theEntry->second->GetUnit());
        if (outUnits.size() == theRanked.size() && outUnits.size() > 1)
            outUnits.pop_back();
        if (outUnits.size() == 0 && this->GetNumUnits() - theHalf > 0) {
            for (long i = theHalf - 1; i >= 0; i--) {
                EEComplexUnit* theUnit = this->GetUnitUnit(i);
                if (theUnit != NULL)
                    outUnits.push_back(theUnit);
            }
        }
    }
    std::map<unsigned short, std::vector<EECPAttackSubGroupUnit*>*>::iterator theGroup;
    for (theGroup = theGroups.begin(); theGroup != theGroups.end(); ++theGroup)
        delete theGroup->second;
}

// FUNCTION: EXE 0x451722
// Adds every unit in the bounds that passes the submarine target filter (ships only if it can attack them,
// enemies optional) to the targets
void EECPASGSubmarine::CollectTargets(EEAIAStarBounds* inBounds, bool inIncludeEnemies)
{
    std::vector<EEComplexUnit*> theUnits;
    EECPASeaTargetFilter theFilter;
    theFilter.mGroup = this;
    theFilter.mPlayer = this->mPlayer2;
    theFilter.mPlayerID = this->mTask.GetPlayerID();
    theFilter.mIncludeEnemies = inIncludeEnemies;
    theFilter.mAlternate = true;
    theFilter.mAllowNaval = this->mCanAttackShips;
    theFilter.mAllowShore = this->mCanAttackShore;
    if (EECollectASGTargets(*inBounds, theUnits, theFilter, true) != 0) {
        this->mTargets.reserve(theUnits.size());
        const std::vector<EEComplexUnit*>& theConst = theUnits;
        for (long i = theConst.size() - 1; i >= 0; --i) {
            EECPASTargetUnit* theTarget = EECPAttackSubGroup::NewTarget(theConst[i]);
            this->mTargets.push_back(theTarget);
        }
    }
}

// FUNCTION: EXE 0x4517f1
// Refreshes the units, then recomputes whether the group can attack ships
void EECPASGSubmarine::RefreshUnits()
{
    EECPAttackSubGroup::RefreshUnits();
    this->UpdateCanAttackShips();
}


// ICF-FOLDED: EXE 0x67c0b4
// Returns false: submarine groups never ask to split
// (no address of its own: folded into the shared `xor al, al; ret` body the vtable slot points at)
bool EECPASGSubmarine::DecideSplit()
{
    return false;
}
