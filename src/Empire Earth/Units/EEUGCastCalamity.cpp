// C:\EEX\Empire Earth\Units\EEUGCastCalamity.cpp (file name inferred: no __FILE__; the key "EEUGCastCalamity-Data").
// Casting a calamity: 0x220986..0x2213dd. Unit flags: the exe's module set. Data: the key 0x563550 (guard byte
// 0x563548); vtable 0x44748c. Its Virtual70 (true) is the ICF representative of every goal's `return true` slot.

#include <windows.h>
#include <stdio.h>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "lle/USingleton.h"

#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGCastCalamity.h"
#include "ee/EEUnitSetManager.h"
#include "ee/EECalamityManager.h"
#include "ee/EECalamityClass.h"
#include "ee/EEAreaUtilities.h"

const UString EEUGCastCalamity::sData("EEUGCastCalamity-Data");
// SYNTHETIC: EXE 0x620986   (wrapper)
// SYNTHETIC: EXE 0x620990   (ctor)
// SYNTHETIC: EXE 0x6209a1   (registrar)
// SYNTHETIC: EXE 0x6209ad   (dtor)

// FUNCTION: EXE 0x6209c9
// Creates a calamity-casting goal from a source unit on a target unit or tile, referencing both
EEUGCastCalamity::EEUGCastCalamity(EEComplexUnit* inSource, EEUnit* inTarget, long inTileX, long inTileY,
                                   long inCalamityType, bool inArg6)
    : EEUnitGoal(kGoalCastCalamity, inArg6)
{
    this->mSource = inSource;
    if (inSource != NULL)
        inSource->Reference();
    this->mTarget = inTarget;
    if (inTarget != NULL)
        inTarget->Reference();
    this->mTileX = inTileX;
    this->mTileY = inTileY;
    this->mCalamityType = inCalamityType;
}

// FUNCTION: EXE 0x620a12
// Returns reacquire-goal as the first action
long EEUGCastCalamity::GetStartAction()
{
    return kActionReacquireGoal;
}

// FUNCTION: EXE 0x620a18
// Returns true: the goal suits any unit
bool EEUGCastCalamity::Suits(EEUnit* inUnit)
{
    return true;
}

// SYNTHETIC: EXE 0x620a1d
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x620a39
// Creates an empty calamity goal for loading, with no source, target, tile or calamity type
EEUGCastCalamity::EEUGCastCalamity(bool inArg)
    : EEUnitGoal(kGoalCastCalamity, inArg)
{
    this->mSource = NULL;
    this->mTarget = NULL;
    this->mTileX = -10000;
    this->mTileY = -10000;
    this->mCalamityType = 100000;
}

// FUNCTION: EXE 0x620a6d
// Destroys the goal, releasing the source and target
EEUGCastCalamity::~EEUGCastCalamity()
{
    if (this->mSource != NULL)
        this->mSource->Dereference();
    if (this->mTarget != NULL)
        this->mTarget->Dereference();
}

// FUNCTION: EXE 0x620a96
// Has the calamity's class place it at the target tile ahead of the cast
void EEUGCastCalamity::Prepare(EEComplexUnit* inUnit)
{
    EETechTreeNode* theNode = inUnit->mPlayer->mTechnologies->Get(this->mCalamityType);
    if (theNode == NULL)
        return;
    EECalamityClass* theClass =
        EECalamityManager::GetInstance()->GetCalamityClass((EECalamityEnum)theNode->mData->mCalamity);
    if (theClass != NULL)
        theClass->Place(inUnit, this->mTileX, this->mTileY);
}

// FUNCTION: EXE 0x620ae6
// Stops the unit and casts the calamity on the target unit, at the tile or globally (by its kind); resets saved group
// slot 6 of the source's player and takes the calamity's power cost from the unit when the cast succeeded
void EEUGCastCalamity::Cast(EEComplexUnit* inUnit)
{
    bool theResult = false;
    inUnit->SetCurrentPhysicsAction(6);
    inUnit->DropPath();
    EEPlayer* thePlayer = inUnit->mPlayer;
    EETechTreeNode* theNode = thePlayer->mTechnologies->Get(this->mCalamityType);
    if (theNode == NULL)
        return;
    EECalamityClass* theClass =
        EECalamityManager::GetInstance()->GetCalamityClass((EECalamityEnum)theNode->mData->mCalamity);
    if (theClass == NULL)
        return;
    switch (theClass->GetTargetKind()) {
    case 3:
        theResult = EECalamityManager::GetInstance()->CreateUnitCalamity(theNode->mData->GetCalamity(),
            this->mSource->GetUnitID(), this->mTarget->GetUnitID(), thePlayer->GetPlayerID());
        break;
    case 2:
        theResult = EECalamityManager::GetInstance()->CreateLocationCalamity(theNode->mData->GetCalamity(),
            this->mSource->GetUnitID(), this->mTileX, this->mTileY, thePlayer->GetPlayerID());
        break;
    case 1:
        theResult = EECalamityManager::GetInstance()->CreateGlobalCalamity(theNode->mData->GetCalamity(),
            this->mSource->GetUnitID(), thePlayer->GetPlayerID());
        break;
    }
    this->mSource->mPlayer->mSavedGroups.Reset(6, 1);
    if (theResult)
        inUnit->SetPower(inUnit->GetPower() - theClass->GetCost());
}

// FUNCTION: EXE 0x620c15
// Heads for the calamity's target location and tells the unit's active group
void EEUGCastCalamity::GetDestination(EEComplexUnit* inUnit, GE3DPoint& outLocation)
{
    this->GetTargetLocation(outLocation);
    EEActiveGroup* theGroup = inUnit->mActiveGroup;
    if (theGroup != NULL)
        theGroup->TellGroup(inUnit, outLocation);
}

// FUNCTION: EXE 0x620c39
// Writes the source and target ids, tile and calamity type to the out-of-synch dump
void EEUGCastCalamity::DumpOutOfSynch(FILE* inFile)
{
    fprintf(inFile, "OUT OF SYNCH DUMP:  EEUGCastCalamity Source Unit ID #%08x\n", this->mSource ? this->mSource->mUnitID : -1);
    fprintf(inFile, "OUT OF SYNCH DUMP:  EEUGCastCalamity Target Unit ID #%08x\n", this->mTarget ? this->mTarget->mUnitID : -1);
    fprintf(inFile, "OUT OF SYNCH DUMP:  EEUGCastCalamity Tile X: %d\n", this->mTileX);
    fprintf(inFile, "OUT OF SYNCH DUMP:  EEUGCastCalamity Tile Y: %d\n", this->mTileY);
    fprintf(inFile, "OUT OF SYNCH DUMP:  EEUGCastCalamity Calamity Type: %d\n", this->mCalamityType);
}

// FUNCTION: EXE 0x620ca5
// Returns the base goal's checksum plus the source and target ids, tile and calamity type
long EEUGCastCalamity::GetOutOfSynchChecksum()
{
    long theChecksum = 0;
    theChecksum += this->mSource ? this->mSource->mUnitID : 0;
    theChecksum += this->mTarget ? this->mTarget->mUnitID : 0;
    theChecksum += this->mTileX + this->mTileY + this->mCalamityType;
    return EEUnitGoal::GetOutOfSynchChecksum() + theChecksum;
}

// FUNCTION: EXE 0x620cda
// Returns where the calamity lands: the target unit, the source itself for calamity types 0x77c/0x1b6c, else the
// tile; raised to the source's height when it flies
void EEUGCastCalamity::GetTargetLocation(GE3DPoint& outLocation)
{
    if (this->mTarget != NULL)
        this->mTarget->GetPosition(outLocation);
    else if (this->mCalamityType == 0x77c || this->mCalamityType == 0x1b6c)
        this->mSource->GetPosition(outLocation);
    else
        ::gWorld.mMap->GetTileLocation(this->mTileX, this->mTileY, outLocation);
    if (this->mSource->mHeight > 0.0f)
        outLocation.SetZ(this->mSource->mHeight);
}

// FUNCTION: EXE 0x620d3f
// Returns a copy of the goal for the same source, target, tile and calamity type
EEUnitGoal* EEUGCastCalamity::Clone()
{
    return new EEUGCastCalamity(this->mSource, this->mTarget, this->mTileX, this->mTileY, this->mCalamityType, this->GetB1C());
}

// FUNCTION: EXE 0x620d78
// Appends the source and target ids, tile and calamity type to the one-line debug text
void EEUGCastCalamity::GetDebugLine(UWideString& ioText)
{
    UWideString theText;
    sprintf(theText, "cc: %d,%d,%d,%d,%d", this->mSource->mUnitID, this->mTarget ? this->mTarget->mUnitID : -1, this->mTileX, this->mTileY,
            this->mCalamityType);
    ioText += theText;
}

// FUNCTION: EXE 0x620dd6
// Appends "Casting Calamity", the target's name and the calamity's tile and type to the debug text
void EEUGCastCalamity::GetDebugText(UWideString& ioText)
{
    UWideString theText;
    ioText += "Casting Calamity";
    if (this->mTarget != NULL) {
        sprintf(theText, "Cast On Unit: %s\n", (const char*)this->mTarget->mUnitClass->mName);
        ioText += theText;
    }
    // the line used to print the source and target ids and the tile x (it was passed five values for three)
    sprintf(theText, "Calamity Location/Type: (%d,%d) - Type:%d", this->mTileX, this->mTileY, this->mCalamityType);
    ioText += theText;
}

// FUNCTION: EXE 0x620e6e
// Returns whether the unit can cast the calamity at the target tile now
bool EEUGCastCalamity::CanCast(EEComplexUnit* inUnit)
{
    GE3DPoint theLocation;
    bool theResult = false;
    EETechTreeNode* theNode = inUnit->mPlayer->mTechnologies->Get(this->mCalamityType);
    if (theNode != NULL) {
        EECalamityClass* theClass =
            EECalamityManager::GetInstance()->GetCalamityClass((EECalamityEnum)theNode->mData->mCalamity);
        if (theClass != NULL)
            theResult = theClass->EECalamityClass::CanCastAt(inUnit, theLocation, this->mTileX, this->mTileY, true, NULL) == 0;
    }
    return theResult;
}

// FUNCTION: EXE 0x620ee6
// Returns the target unit's position, else the target tile's location
void EEUGCastCalamity::GetGoalLocation(GE3DPoint& outLocation)
{
    if (this->mTarget != NULL)
        this->mTarget->GetPosition(outLocation);
    else
        ::gWorld.mMap->GetTileLocation(this->mTileX, this->mTileY, outLocation);
}

// FUNCTION: EXE 0x620f12
// Runs the current action: pre-cast placement, casting (animation 0xb) or facing the target (animation 0xd)
void EEUGCastCalamity::Step(EEComplexUnit* inUnit)
{
    unsigned long theCommand = inUnit->mCommand ? inUnit->mCommand->mType : 0;
    switch (theCommand) {
    case kActionCalamityPreCast:
        this->Prepare(inUnit);
        break;
    case kActionCastCalamity:
        inUnit->SetCurrentPhysicsAction(1);
        inUnit->SetAnimation(0xb, -1, 0);
        this->Cast(inUnit);
        break;
    case kActionFaceEnemyUnit:
        inUnit->SetCurrentPhysicsAction(1);
        inUnit->SetAnimation(0xd, -1, 0);
        inUnit->DropPath();
        break;
    }
}

// FUNCTION: EXE 0x620f94
// Sets the casting unit, moving the reference
void EEUGCastCalamity::SetOwner(EEComplexUnit* inSource)
{
    if (this->mSource != NULL)
        this->mSource->Dereference();
    this->mSource = inSource;
    if (inSource != NULL)
        inSource->Reference();
}

// FUNCTION: EXE 0x620fb5
// Reads the source and target ids (resolved by RepairPointers), tile and calamity type from the saved game; returns
// the first read error
long EEUGCastCalamity::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    unsigned long theUnitID = -1;
    long theResult = EEUnitGoal::Load(inMajorVersion, inMinorVersion, inFile);
    if (theResult != 0)
        return theResult;
    theResult = inFile.Read(theUnitID);
    if (theResult != 0)
        return theResult;
    this->mSource = reinterpret_cast<EEComplexUnit*>(theUnitID);
    theResult = inFile.Read(theUnitID);
    if (theResult != 0)
        return theResult;
    this->mTarget = reinterpret_cast<EEUnit*>(theUnitID);
    theResult = inFile.Read(this->mTileX);
    if (theResult != 0)
        return theResult;
    theResult = inFile.Read(this->mTileY);
    if (theResult == 0)
        theResult = inFile.Read(this->mCalamityType);
    return theResult;
}

// FUNCTION: EXE 0x62102d
// Turns the loaded ids back into source and target, referencing them; returns 3 and clears both when the source is
// gone
long EEUGCastCalamity::RepairPointers(unsigned long inFirst, unsigned long inSecond)
{
    long theResult = 3;
    if (reinterpret_cast<unsigned long>(this->mSource) != -1) {
        this->mSource = dynamic_cast<EEComplexUnit*>(::gWorld.GetUnit(reinterpret_cast<unsigned long>(this->mSource)));
        if (this->mSource != NULL) {
            this->mSource->Reference();
            theResult = 0;
            if (reinterpret_cast<unsigned long>(this->mTarget) != -1) {
                this->mTarget = static_cast<EEUnit*>(::gWorld.GetUnit(reinterpret_cast<unsigned long>(this->mTarget)));
                if (this->mTarget != NULL)
                    this->mTarget->Reference();
            } else
                this->mTarget = NULL;
        } else
            this->mTarget = NULL;
    } else {
        this->mSource = NULL;
        this->mTarget = NULL;
    }
    return theResult;
}

// FUNCTION: EXE 0x621099
// Writes the source and target ids (-1 when none), tile and calamity type; returns the first write error
long EEUGCastCalamity::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{

    long theResult = EEUnitGoal::Save(inMajorVersion, inMinorVersion, outFile);
    if (theResult != 0)
        return theResult;

    theResult = outFile.Write(this->mSource ? this->mSource->GetUnitID() : -1);
    if (theResult != 0)
        return theResult;

    theResult = outFile.Write(this->mTarget ? this->mTarget->GetUnitID() : -1);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mTileX);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mTileY);
    if (theResult == 0)
        theResult = outFile.Write(this->mCalamityType);
    return theResult;
}

// FUNCTION: EXE 0x621133
// Answers the casting triggers (target still valid, facing, within the calamity's range, destroyed, undetectable,
// facing not required); a living destroyed-test falls through to the undetectable test; others go to the common goal
bool EEUGCastCalamity::EvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent)
{
    GE3DPoint theUnitPosition;
    GE3DPoint theTargetPosition;
    unsigned long theRange = 1000;
    switch (inEvent) {
    case kTriggerCalamityTargetStillValid:
        return this->CanCast(inUnit);
    case kTriggerFacingEnemyUnit:
        if (this->mTarget != NULL)
            return inUnit->GetUnitClass()->FacesTarget(inUnit, this->mTarget, false);
        this->GetGoalLocation(theTargetPosition);
        return inUnit->GetUnitClass()->FacesPoint(inUnit, theTargetPosition, false);
    case kTriggerUnitInWeaponRange: {
            EETechTreeNode* theNode = inUnit->mPlayer->mTechnologies->Get(this->mCalamityType);
            if (theNode != NULL) {
                EECalamityClass* theClass =
                    EECalamityManager::GetInstance()->GetCalamityClass((EECalamityEnum)theNode->mData->mCalamity);
                if (theClass != NULL) {
                    EEUnitSet* theSet = gUnitSetManager.GetSet(0x91);
                    if (theSet != NULL && theSet->Contains(inUnit->mUnitClass->mUnitType))
                        theRange = theClass->mRange + UMFloatToLong(inUnit->mStats->mRange);
                    else if (theNode->mData->mCalamity == 0x17)
                        theRange = UMFloatToLong(inUnit->mStats->mRange);
                    else
                        theRange = theClass->mRange;
                }
            }
            if (this->mTarget != NULL) {
                if (theRange < 1) {
                    GE3DPoint theTarget;
                    GE3DPoint thePosition;
                    this->mTarget->GetPosition(theTarget);
                    inUnit->GetPosition(thePosition);
                    return EEIsInContactRange(inUnit, this->mTarget, thePosition.DistanceToSquared(theTarget));
                }
                return inUnit->DistanceSquaredToUnit(this->mTarget) <= theRange * theRange;
            } else {
                inUnit->GetPosition(theUnitPosition);
                this->GetTargetLocation(theTargetPosition);
                return theUnitPosition.DistanceToSquared(theTargetPosition) <= theRange * theRange;
            }
        }
    case kTriggerEnemyUnitDestroyed:
        if (this->mTarget != NULL) {
            if (this->mTarget->mHitPoints == 0)
                return true;
            if (this->mTarget->mTileX == -10000)
                return true;
        }
    case kTriggerEnemyUnitUndetectable:
        if (this->mTarget != NULL && this->mTarget->mComplexUnit->mIsCloakable && this->mTarget->mComplexUnit->GetIsCloaked()
            && this->mTarget->mPlayer != inUnit->mPlayer && inUnit->mPlayer->mEnemies[this->mTarget->mPlayer->mPlayerID] != 0
            && this->mTarget->mUnitClass->mCloakRadius == 0)
            return true;
        return false;
    case kTriggerUnitFacingNotRequired:
        return true;
    }
    return EEUnitGoal::EvaluateCondition(inUnit, inEvent);
}

// FUNCTION: EXE 0x6213b7
// Replaces a target whose complex unit is the old unit with the new unit (moving the reference)
void EEUGCastCalamity::ReplaceUnit(EEComplexUnit* inOld, EEComplexUnit* inNew, EEComplexUnit*)
{
    if (this->mTarget == NULL || this->mTarget->mComplexUnit != inOld)
        return;
    this->mTarget->Dereference();
    this->mTarget = inNew;
    inNew->Reference();
}

// The slots ICF-folded into other units' identical bodies:
// ICF-FOLDED: EXE 0x557269
// Does nothing
void EEUGCastCalamity::Start(EEComplexUnit* inUnit)
{
}
