// C:\EEX\Empire Earth\AI Thread\EECPResourceCenter.cpp (file name inferred: no __FILE__; the class-static key
// "EECPResourceCenter-Data"). A computer player's resource center: 0x8bbbf..0x8e702 (the key chain first; the vector /
// sort / find_if COMDATs close the unit).
//
// AI lane B, round 2: the bodies are Rust's (ee_sim::cp::resource_center, rust/sim/cp.h): FindBuildSite, GetCenter,
// GetMoraleFill, IsInAreaEffect, GetMinEffectRadius, FindResourceUnit, IsWithinRadius, AdoptDropSite's test, Contains,
// Load, Save and Restore. What stays here: the members other files read (mUnits, mUnit, mIsActive), the references
// and the vector's growth (the constructor, destructor, AddUnit, SetUnit, ReleaseUnit), the forwarders with their TEST
// ONLY PLCE scopes, EEComplexUnit::DistanceSquaredToUnit (the original's, defined in this unit), and
// ee_cp_center_host: the centres and area effects the Rust bodies ask.
#include <windows.h>
#include <stdint.h>
#include <vector>
#include <deque>
#include <map>
#include <algorithm>

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
#include "lle/USingleton.h"

#include "ee/EEAIFOUnitScreeners.h"
#include "ee/EmpireEarth.h"
#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEAEManager.h"
#include "ee/EEAreaEffectTemplate.h"
#include "ee/EEAreaEffectKindIDs.h"
#include "ee/RTSRegionTable.h"
#include "ee/EEUnitCollector.h"
#include "ee/EEAreaUtilities.h"
#include "ee/AStarGraphableObjectBase.h"
#include "ee/EECPPlacement.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEUnitUtilities.h"
#include "ee/EECPResourceCenter.h"
#include "ee/EEAIKindIDs.h"
#include "ee/EECPPlaceCapture.h"
#include "rust/sim/cp.h"

const UString EECPResourceCenter::sData("EECPResourceCenter-Data");

static_assert(sizeof(EEUnitHandle<EEUnit>) == 4, "a centre's unit handle is its unit id");

EeCpPlaceCtx EECPPlaceContext(EEComputerPlayer* inPlayer);   // EECPPlacement.cpp

static uint64_t EECPRCHandle(const void* inObject)
{
    return (uint64_t)(uintptr_t)inObject;
}

template <class T> static T* EECPRCObject(uint64_t inHandle)
{
    return reinterpret_cast<T*>((uintptr_t)inHandle);
}

// Creates a resource center for the computer player, holding the given resource unit if any, with no main building
EECPResourceCenter::EECPResourceCenter(EEComputerPlayer* inPlayer, EEUnit* inUnit)
{
    this->mUnit = NULL;
    this->mPlayer = inPlayer;
    if (inUnit != NULL)
        this->AddUnit(inUnit);
}

// Destroys the resource center, releasing its resource units
EECPResourceCenter::~EECPResourceCenter()
{
    for (unsigned long i = 0; i < this->mUnits.size(); i++)
        this->mUnits[i]->Dereference();
    this->mUnits.clear();
}

// Adds a resource unit to the center, taking a reference on it
void EECPResourceCenter::AddUnit(EEUnit* inUnit)
{
    EECPPlaceScope theScope(kPlceCenterAddUnit, this->mPlayer);   // PLCE (TEST ONLY)
    theScope.U(inUnit);
    inUnit->Reference();
    this->mUnits.push_back(inUnit);
    theScope.State(this);
}

#define EECPRC_CONTEXT EeCpPlaceCtx theContext = EECPPlaceContext(this->mPlayer)

// Returns whether the center holds the unit
bool EECPResourceCenter::Contains(EEUnit* inUnit)
{
    EECPPlaceScope theScope(kPlceCenterContains, this->mPlayer);   // PLCE (TEST ONLY)
    theScope.U(inUnit);
    EECPRC_CONTEXT;
    bool theResult = ee_cp_center_contains(&theContext, EECPRCHandle(this), EECPRCHandle(inUnit));
    theScope.I(theResult);
    return theResult;
}

// Makes the player's drop site (not a granary) the center's main building if the center has none and the site is
// within drop-off distance of one of its resource units
void EECPResourceCenter::AdoptDropSite(EEComplexUnit* inUnit)
{
    EECPPlaceScope theScope(kPlceCenterAdoptDropSite, this->mPlayer);   // PLCE (TEST ONLY)
    theScope.CU(inUnit).State(this);
    EECPRC_CONTEXT;
    if (ee_cp_center_adopts(&theContext, EECPRCHandle(this), EECPRCHandle(static_cast<EEUnit*>(inUnit)))) {
        this->mUnit = inUnit;
        inUnit->Reference();
    }
    theScope.State(this);
}

// Returns the squared distance from this unit's position to another unit's position (its tile if not a complex unit)
float EEComplexUnit::DistanceSquaredToUnit(EEUnit* inOther)
{
    float theDeltaY;
    float theDeltaX;
    EEComplexUnit* theOther = inOther->mComplexUnit;
    if (theOther != NULL) {
        theDeltaY = this->mPhysicalModel.GetLocation().GetY() - theOther->mPhysicalModel.GetLocation().GetY();
        theDeltaX = this->mPhysicalModel.GetLocation().GetX() - theOther->mPhysicalModel.GetLocation().GetX();
    } else {
        theDeltaY = this->mPhysicalModel.GetLocation().GetY() - inOther->GetTileY();
        theDeltaX = this->mPhysicalModel.GetLocation().GetX() - inOther->GetTileX();
    }
    return theDeltaX * theDeltaX + theDeltaY * theDeltaY;
}

// The owner's living, unfinished houses (unit type 0x25, +0x34c clear) whose tile lies in mRect (0x8da4d, out of
// line) (provisional name; layout inferred: 0x14 bytes on GetMoraleFill's frame)
struct EECPRCBuildingInRectangle {
    EECPRCBuildingInRectangle(EEPlayer* inPlayer, const U2DRectangle<long>& inRect) : mPlayer(inPlayer), mRect(inRect) {}
    bool operator()(EEComplexUnit* inUnit) const
    {
        return inUnit->mPlayer == this->mPlayer && inUnit->GetUnitClass()->mUnitType == kUnitTypeHouse && !inUnit->mIsBuilt &&
               inUnit->mHitPoints > 0 && this->mRect.Contains(U2DPoint<long>(inUnit->GetTileX(), inUnit->GetTileY()));
    }

    EEPlayer* mPlayer;                      // +0x00
    U2DRectangle<long> mRect;               // +0x04
};

// The owner's living houses (unit type 0x25) whose tile lies in mRect, finished or not (0x8d6e0, out of line;
// EECPRCBuildingInRectangle without the +0x34c test) (provisional name; layout inferred: 0x14 bytes on FindBuildSite's
// frame)
struct EECPRCLiveBuildingInRectangle {
    EECPRCLiveBuildingInRectangle(EEPlayer* inPlayer, const U2DRectangle<long>& inRect) : mPlayer(inPlayer), mRect(inRect) {}
    bool operator()(EEComplexUnit* inUnit) const
    {
        return inUnit->mPlayer == this->mPlayer && inUnit->GetUnitClass()->mUnitType == kUnitTypeHouse && inUnit->mHitPoints > 0 &&
               this->mRect.Contains(U2DPoint<long>(inUnit->GetTileX(), inUnit->GetTileY()));
    }

    EEPlayer* mPlayer;                      // +0x00
    U2DRectangle<long> mRect;               // +0x04
};

// the unit's first morale-from-buildings effect (NULL: none)
static EEAreaEffect* EECPRCMoraleEffect(EEComplexUnit* inUnit)
{
    for (std::vector<EEAreaEffect*>::iterator theIterator = inUnit->mAreaEffects.mItems.begin();
         theIterator != inUnit->mAreaEffects.mItems.end(); ++theIterator) {
        if ((*theIterator)->mKind == kAreaEffectMoraleFromBuildings)
            return *theIterator;
    }
    return NULL;
}

// The centres' and area effects' world (ee_sim::cp::place_game's EE_CP_CENTER_* operations; `inA` the centre)
extern "C" int64_t ee_cp_center_host(const EeCpPlaceCtx* inContext, int32_t inOp, uint64_t inA, uint64_t inB,
    EeCpPlaceIo* io)
{
    EEComputerPlayer* thePlayer = static_cast<EEComputerPlayer*>(EECPRCObject<EEPlayer>(inContext->player));
    EECPResourceCenter* theCenter = EECPRCObject<EECPResourceCenter>(inA);
    const int64_t* v = io->v;
    switch (inOp) {
    case EE_CP_CENTER_UNITS:
        for (size_t i = 0; i < theCenter->mUnits.size(); i++)
            ee_cp_place_push(io->list, (int64_t)theCenter->mUnits[i].GetSavedID(), 0);
        return 0;
    case EE_CP_CENTER_UNIT:
        return (int64_t)theCenter->mUnit.GetSavedID();
    case EE_CP_CENTER_ACTIVE:
        return theCenter->mIsActive ? 1 : 0;
    case EE_CP_CENTER_SET_ORDER:
        for (size_t i = 0; i < io->input_len && i < theCenter->mUnits.size(); i++)
            theCenter->mUnits[i] = EEUnitHandle<EEUnit>::FromSavedID((uint32_t)io->input[2 * i]);
        return 0;
    case EE_CP_CENTER_CLEAR:
        theCenter->mUnits.clear();
        return 0;
    case EE_CP_CENTER_ADD_UNIT:
        theCenter->AddUnit(EECPRCObject<EEUnit>(inB));
        return 0;
    case EE_CP_CENTER_RESTORE_MAIN:
        if ((int32_t)(uint32_t)v[0] != -1) {
            theCenter->mUnit = dynamic_cast<EEComplexUnit*>(gWorld.GetUnit((uint32_t)v[0]));
            if (theCenter->GetUnit() != NULL)
                theCenter->GetUnit()->Reference();
            else
                return 3;
        } else
            theCenter->mUnit = NULL;
        return 0;
    case EE_CP_CENTER_DISTANCE_TO_UNIT:
        io->f[0] = static_cast<EEComplexUnit*>(EECPRCObject<EEUnit>(inA))->DistanceSquaredToUnit(EECPRCObject<EEUnit>(inB));
        return 0;
    case EE_CP_CENTER_HAS_AREA_EFFECT:
        return gAEManager.HasAreaEffect((long)v[0], (long)v[1], (long)v[2], (long)v[3]) ? 1 : 0;
    case EE_CP_CENTER_MORALE_RADIUS: {
        EEAreaEffect* theEffect = EECPRCMoraleEffect(static_cast<EEComplexUnit*>(EECPRCObject<EEUnit>(inA)));
        if (theEffect == NULL)
            return 0;
        io->f[0] = theEffect->GetRadius();
        return 1;
    }
    case EE_CP_CENTER_MORALE_RECOUNT: {
        EEAreaEffect* theEffect = EECPRCMoraleEffect(static_cast<EEComplexUnit*>(EECPRCObject<EEUnit>(inA)));
        if (theEffect == NULL)
            return 0;
        theEffect->MarkDirty();
        theEffect->Recount(0);
        io->v[0] = theEffect->GetLevel();
        io->v[1] = theEffect->mTemplate->mMaxLevel;
        io->f[0] = theEffect->GetRadius();
        return 1;
    }
    case EE_CP_CENTER_EFFECT_TEMPLATES: {
        EEUnitClassUpgrade* theUpgrade = thePlayer->mUnitClassUpgrades[(long)v[0]];
        for (unsigned long i = 0; i < 4; i++)
            io->v[i] = (int64_t)(uint32_t)theUpgrade->mAreaEffectTemplates[i];
        return 0;
    }
    case EE_CP_CENTER_TEMPLATE_RADIUS:
        io->f[0] = gAEManager.mTemplates[(unsigned long)v[0]]->mRadius;
        return 0;
    case EE_CP_PLACE_UNITS_IN_RECT: {
        U2DRectangle<long> theRect;
        theRect.mLeft = (long)v[0];
        theRect.mTop = (long)v[1];
        theRect.mRight = (long)v[2];
        theRect.mBottom = (long)v[3];
        std::vector<EEComplexUnit*> theUnits;
        for (size_t i = 0; i < io->input_len; i++)
            theUnits.push_back(static_cast<EEComplexUnit*>(EECPRCObject<EEUnit>((uint64_t)io->input[2 * i])));
        if (v[4] == 1)
            EEGetUnitsInRectangle(theRect, theUnits, EECPRCLiveBuildingInRectangle(thePlayer, theRect), true);
        else
            EEGetUnitsInRectangle(theRect, theUnits, EECPRCBuildingInRectangle(thePlayer, theRect), true);
        for (size_t i = 0; i < theUnits.size(); i++)
            ee_cp_place_push(io->list, (int64_t)EECPRCHandle(static_cast<EEUnit*>(theUnits[i])), 0);
        return 0;
    }
    }
    return 0;
}

// Returns the free tile nearest the middle of the resource units (or next to the single unit or main building),
// else the first candidate tile; leaves outCenter unchanged when there is nothing to measure
void EECPResourceCenter::GetCenter(EETileCoordinate& outCenter)
{
    EECPPlaceScope theScope(kPlceCenterGetCenter, this->mPlayer);   // PLCE (TEST ONLY)
    theScope.State(this);
    bool theScopeSet = this->mUnits.size() > 0 || this->GetUnit() != NULL;
    EECPRC_CONTEXT;
    int32_t theTile[2] = {(int32_t)outCenter.mX, (int32_t)outCenter.mY};
    ee_cp_center_get_center(&theContext, EECPRCHandle(this), theTile);
    outCenter.mX = theTile[0];
    outCenter.mY = theTile[1];
    theScope.I(theScopeSet ? outCenter.mX : 0).I(theScopeSet ? outCenter.mY : 0);
}

// The file host of Load: the saved game's primitives and the centre's lists (ee_sim::cp::resource_center's
// EE_CP_CENTER_FILE_* operations)
struct EECPRCFileContext {
    EECPResourceCenter* mCenter;
    FSFile* mFile;
};

static int32_t EECPRCFileOperation(void* inContext, int32_t inOp, uint32_t inIndex, EeUnitsPlayerSaveIo* io)
{
    EECPRCFileContext& theContext = *static_cast<EECPRCFileContext*>(inContext);
    EECPResourceCenter* theCenter = theContext.mCenter;
    switch (inOp) {
    case EE_CP_CENTER_FILE_CLEAR:
        theCenter->mUnits.clear();
        return kErrorOK;
    case EE_CP_CENTER_FILE_PUSH_ID:
        theCenter->mUnits.push_back(EEUnitHandle<EEUnit>::FromSavedID((uint32_t)io->i));   // ids until Restore
        return kErrorOK;
    case EE_CP_CENTER_FILE_SET_MAIN:
        theCenter->mUnit = EEUnitHandle<EEComplexUnit>::FromSavedID((uint32_t)io->i);
        return kErrorOK;
    case EE_CP_CENTER_FILE_ACTIVE:
        io->b = theCenter->mIsActive ? 1 : 0;
        return kErrorOK;
    case EE_CP_CENTER_FILE_SET_ACTIVE:
        theCenter->mIsActive = io->b != 0;
        return kErrorOK;
    }
    return EESavePrimitive(*theContext.mFile, inOp, io);
}

// Reads the resource unit ids and the main building's id (resolved by Restore) and, from version 0.141 on, the
// active flag; returns the first read error
long EECPResourceCenter::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    EECPPlaceScope theScope(kPlceCenterLoad, this->mPlayer);   // PLCE (TEST ONLY)
    theScope.I((long)inMajorVersion).I((long)inMinorVersion);
    EECPRCFileContext theFile = {this, &inFile};
    long theResult = ee_cp_center_load(&theFile, &EECPRCFileOperation, (uint32_t)inMinorVersion);
    theScope.I(theResult).State(this).I(inMinorVersion >= 0x8d && theResult == kErrorOK ? (this->mIsActive ? 1 : 0) : 2);
    return theResult;
}

// Turns the loaded ids back into units (dropping missing and duplicate ones) and references the main building;
// returns 3 if the main building no longer exists
long EECPResourceCenter::Restore(unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    EECPPlaceScope theScope(kPlceCenterRestore, this->mPlayer);   // PLCE (TEST ONLY)
    theScope.I((long)inMajorVersion).I((long)inMinorVersion).State(this);
    EECPRC_CONTEXT;
    long theResult = ee_cp_center_restore(&theContext, EECPRCHandle(this));
    theScope.I(theResult).State(this);
    return theResult;
}

// Returns whether a unit's tile is within the player's drop-off distance of the center
bool EECPResourceCenter::IsWithinRadius(EEUnit* inUnit)
{
    EECPPlaceScope theScope(kPlceCenterIsWithinRadius, this->mPlayer);   // PLCE (TEST ONLY)
    theScope.U(inUnit).State(this);
    EECPRC_CONTEXT;
    bool theResult = ee_cp_center_is_within_radius(&theContext, EECPRCHandle(this), EECPRCHandle(inUnit));
    theScope.I(theResult);
    return theResult;
}

// Writes the ids of the valid resource units, the main building's id (-1 for none) and the active flag; returns
// the first write error
long EECPResourceCenter::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    EECPPlaceScope theScope(kPlceCenterSave, this->mPlayer);   // PLCE (TEST ONLY)
    theScope.I((long)inMajorVersion).I((long)inMinorVersion).State(this);
    EECPRC_CONTEXT;
    long theResult = ee_cp_center_save(&theContext, EECPRCHandle(this), &outFile, &EESaveFilePrimitive);
    theScope.I(theResult);
    return theResult;
}

// Sets the center's main building, moving the reference from the old one
void EECPResourceCenter::SetUnit(EEComplexUnit* inUnit)
{
    EECPPlaceScope theScope(kPlceCenterSetUnit, this->mPlayer);   // PLCE (TEST ONLY)
    theScope.CU(inUnit);
    if (this->mUnit != NULL)
        this->mUnit->Dereference();
    this->mUnit = inUnit;
    if (inUnit != NULL)
        inUnit->Reference();
    theScope.State(this);
}

// Releases and clears the main building if it is the given unit
void EECPResourceCenter::ReleaseUnit(EEComplexUnit* inUnit)
{
    EECPPlaceScope theScope(kPlceCenterReleaseUnit, this->mPlayer);   // PLCE (TEST ONLY)
    theScope.CU(inUnit);
    if (this->mUnit == inUnit) {
        this->mUnit->Dereference();
        this->mUnit = NULL;
    }
    theScope.State(this);
}

// Sets whether the center is active; activating it also computes its center (the result is discarded)
void EECPResourceCenter::SetActive(bool inFlag, long inArg2)
{
    EECPPlaceScope theScope(kPlceCenterSetActive, this->mPlayer);   // PLCE (TEST ONLY)
    theScope.I(inFlag).I(inArg2);
    this->mIsActive = inFlag;
    if (inFlag) {
        EECPRC_CONTEXT;
        int32_t theCenter[2] = {0, 0};
        ee_cp_center_get_center(&theContext, EECPRCHandle(this), theCenter);
    }
    theScope.State(this);
}
