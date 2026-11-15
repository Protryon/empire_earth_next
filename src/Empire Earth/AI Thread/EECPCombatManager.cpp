// C:\EEX\Empire Earth\AI Thread\EECPCombatManager.cpp (RECONSTRUCTION). The combat manager's military class tests
// (ee/EECPCombatManager.h); stage 5 deleted the combat tree the rest of it ran (docs/AI_RESEARCH.md §8.89).
#include <windows.h>
#include <vector>
#include <deque>
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
#include "lle/FSFile.h"
#include "lle/U2DPoint.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"
#include "lle/USingleton.h"
#include "ee/EmpireEarth.h"
#include "ee/EEPlayer.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EECPCombatManager.h"

// FUNCTION: EXE 0x471d74
EECPCombatManager::EECPCombatManager(EEComputerPlayer* inPlayer) : mPlayer(inPlayer)
{
}

// FUNCTION: EXE 0x471ea6
EECPCombatManager::~EECPCombatManager()
{
}

long EECPCombatManager::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    (void)inMajorVersion;
    (void)inMinorVersion;
    (void)inFile;
    return 0;
}

long EECPCombatManager::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    (void)inMajorVersion;
    (void)inMinorVersion;
    (void)outFile;
    return 0;
}

// FUNCTION: EXE 0x47377e
// Returns true for a live, AI-controlled unit of a military class that can move
bool EECPCombatManager::IsMilitaryUnit(EEComplexUnit* inUnit)
{
    if (!inUnit->mIsDetachedFromAI && this->IsMilitaryClass(inUnit->mUnitClass) && inUnit->mStats && inUnit->mHitPoints > 0 && !inUnit->mPreviousPart
        && inUnit->GetMaxVelocity() > 0.0f)
        return true;
    return false;
}

// FUNCTION: EXE 0x4737c9
// Returns true for a mobile military class (not a citizen, building, projectile or naval mine)
bool EECPCombatManager::IsMilitaryClass(EEUnitClass* inClass)
{
    bool theResult = false;
    if (inClass && inClass->mMaxVelocity > 0.0f && inClass->mCategory != kUnitCategoryLandCitizen && inClass->mCategory != kUnitCategoryNavalCitizen && !inClass->mIsProjectile) {
        switch (inClass->mCategory) {
        case kUnitCategoryAmbient:
        case kUnitCategoryResource:
        case kUnitCategoryBuilding:
        case kUnitCategoryLandCitizen:
        case kUnitCategoryProjectile:
        case kUnitCategoryBomb:
        case kUnitCategoryMissile:
        case kUnitCategoryBuilding19:
        case kUnitCategoryTorpedo:
        case kUnitCategoryNavalCitizen:
        case kUnitCategoryMiningBuilding:
        case kUnitCategoryGate:
        case kUnitCategoryFortress:
        case kUnitCategoryBuildingPriest:
            theResult = false;
            break;
        case kUnitCategoryAirplane:
        case kUnitCategoryNaval:
        case kUnitCategoryLand:
        case kUnitCategoryHelicopter:
        case kUnitCategoryLand8:
        case kUnitCategorySubmergedNaval:
        case kUnitCategoryLandPriest:
        case kUnitCategoryNavalPriest:
        case kUnitCategoryLand15:
        case kUnitCategoryLandTransport:
        case kUnitCategoryHelicopterTransport:
        case kUnitCategoryNavalTransport:
        case kUnitCategoryNavalBuilding:
        case kUnitCategoryLandMilitaryHero:
        case kUnitCategoryHelicopterPriest:
        case kUnitCategorySeaAnimal:
        case kUnitCategoryAirAnimal:
        case kUnitCategorySpaceship:
        case kUnitCategorySpaceBuilding:
        case kUnitCategorySpaceTransport:
        case kUnitCategoryPlanetSpaceship:
            theResult = inClass->mUnitType != kUnitTypeS10NavalMine;
            break;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x475122
// Returns true when the unit type's class is in a military category
bool EECPCombatManager::IsMilitaryType(unsigned long inUnitType)
{
    EEUnitClass* theClass = gUnitClassTable[inUnitType];
    bool theResult = false;
    if (theClass) {
        switch (theClass->mCategory) {
        case kUnitCategoryAirplane:
        case kUnitCategoryNaval:
        case kUnitCategoryLand:
        case kUnitCategoryHelicopter:
        case kUnitCategoryLand8:
        case kUnitCategorySubmergedNaval:
        case kUnitCategoryLandPriest:
        case kUnitCategoryNavalPriest:
        case kUnitCategoryLand15:
        case kUnitCategoryLandTransport:
        case kUnitCategoryHelicopterTransport:
        case kUnitCategoryNavalTransport:
        case kUnitCategoryNavalBuilding:
        case kUnitCategoryLandMilitaryHero:
        case kUnitCategoryHelicopterPriest:
        case kUnitCategorySeaAnimal:
        case kUnitCategoryAirAnimal:
        case kUnitCategorySpaceship:
        case kUnitCategorySpaceBuilding:
        case kUnitCategorySpaceTransport:
        case kUnitCategoryPlanetSpaceship:
            theResult = true;
            break;
        case kUnitCategoryAmbient:
        case kUnitCategoryResource:
        case kUnitCategoryBuilding:
        case kUnitCategoryLandCitizen:
        case kUnitCategoryProjectile:
        case kUnitCategoryBomb:
        case kUnitCategoryMissile:
        case kUnitCategoryBuilding19:
        case kUnitCategoryTorpedo:
        case kUnitCategoryNavalCitizen:
        case kUnitCategoryMiningBuilding:
        case kUnitCategoryGate:
        case kUnitCategoryFortress:
        case kUnitCategoryBuildingPriest:
            theResult = false;
            break;
        default:
            theResult = false;
            break;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x47517d
// Returns true when the unit's type is military
bool EECPCombatManager::IsMilitaryTypeUnit(EEUnit* inUnit)
{
    if (inUnit)
        return EECPCombatManager::IsMilitaryType(inUnit->mUnitClass->mUnitType);
    return false;
}
