// C:\EEX\Empire Earth\Scenario Manager\EEEvent.cpp (file name inferred: no __FILE__): 0x1ca4ee..0x1cba2f,
// the data-driven events and event effects. Unit flags: the exe module set.
#include <windows.h>
#include <vector>
#include <set>
#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GERectangle.h"
#include "ee/EmpireEarth.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEUserInterface.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicObject.h"
#include "ee/EESoundObject.h"
#include "ee/EEButtonObject.h"
#include "lle/FSFile.h"
#include "ee/EEEvent.h"
#include "ee/EEGameVariant.h"
#include "lle/UResourceFile.h"
#include "ee/EEUnitSetManager.h"

class EEUnitSetFunctorModifyPathfindingBehavior : public EEUnitSetFunctor {
public:
    EEUnitSetFunctorModifyPathfindingBehavior(EEPlayer* inPlayer, long inType, bool inEnabled)
        : mPlayer(inPlayer), mType(inType), mEnabled(inEnabled) {}
    virtual bool Execute(long inUnitType);
    EEPlayer* mPlayer;
    long mType;
    bool mEnabled;
};

// this unit's globals (ee/EEEvent.h): the effect table (.bss 0x531ff8, 0x532018.. are this unit's) and the switch
// (.data 0x4c5a84 = 1, between EESMObject's and EETechTree's data)
EEEventEffect gEventEffects[2000];
bool gEventsEnabled = true;

// chain at 0x5ca4ee (wrapper)
//   object 0x95cf80  ctor ??0EEEvent@@QAE@XZ  dtor ??1EEEvent@@UAE@XZ
//   guarded dtor (byte ptr [0x95cf78], 1): a class static data member (statics of one class share the guard byte, one bit each) unless the ctor is guarded too (template static)
//   object bound as ?sEvents@EEEvent@@2PAV1@A

// SYNTHETIC: EXE 0x5ca4ee   (wrapper)

// SYNTHETIC: EXE 0x5ca4f8   (ctor)

// SYNTHETIC: EXE 0x5ca514   (registrar)

// SYNTHETIC: EXE 0x5ca520   (dtor)

// FUNCTION: EXE 0x5ca54c
// Creates an event with no effects
EEEvent::EEEvent() : mEffects(NULL), mNumEffects(0), mID(0)
{
}

// SYNTHETIC: EXE 0x5ca560
// (the scalar deleting destructor, emitted with the vtable; slot EEEvent+0x0)

// FUNCTION: EXE 0x5ca57c
// Destroys the event, freeing its effect index list
EEEvent::~EEEvent()
{
    if (this->mEffects != NULL) {
        delete[] this->mEffects;
    }
}

EEEvent EEEvent::sEvents[1500];

// FUNCTION: EXE 0x5ca591
// Returns an event effect applied to a float stat: base + offset, value * factor + base, or the set value (else 0)
float EEApplyEventEffect(float inValue, float inBase, EEEventEffect* inEffect)
{
    float theResult = 0.0f;
    if (inEffect->mOffset != 0.0f)
        theResult = inBase + inEffect->mOffset;
    else if (inEffect->mFactor != 0.0f)
        theResult = inValue * inEffect->mFactor + inBase;
    else if (inEffect->mValue != 0.0f)
        theResult = inEffect->mValue;
    return theResult;
}

// FUNCTION: EXE 0x5ca5df
// Returns an event effect applied to an integer stat: base + offset, value * factor + base, or the set value
long EEApplyEventEffect(long inValue, long inBase, EEEventEffect* inEffect)
{
    long theResult = 0;
    if (inEffect->mOffset != 0.0f)
        theResult = (long)inEffect->mOffset + inBase;
    else if (inEffect->mFactor != 0.0f)
        theResult = (long)(inValue * inEffect->mFactor + 0.0001f) + inBase;
    else if (inEffect->mValue != 0.0f)
        theResult = (long)inEffect->mValue;
    return theResult;
}

// FUNCTION: EXE 0x5ca63b
// Loads the 2000 event effects and the 1500 events with their effect lists; returns the last read's result
long EEEvent::LoadEvents(FSFile& inEvents, FSFile& inEffects)
{
    unsigned long theRead = 0;
    long theResult;
    char theName[100];
    inEvents.Open(kFileAccessRead);
    inEffects.Open(kFileAccessRead);
    inEffects.Read((char*)gEventEffects, sizeof(gEventEffects), theRead);
    for (long i = 0; i < 1500; i++) {
        inEvents.Read(theName, 100, theRead);
        inEvents.Read(EEEvent::sEvents[i].mID);
        theResult = inEvents.Read(EEEvent::sEvents[i].mNumEffects);
        if (EEEvent::sEvents[i].mNumEffects > 0) {
            EEEvent::sEvents[i].mEffects = new long[EEEvent::sEvents[i].mNumEffects];
            theResult = inEvents.Read(reinterpret_cast<char*>(EEEvent::sEvents[i].mEffects), EEEvent::sEvents[i].mNumEffects * 4, theRead);
        } else
            EEEvent::sEvents[i].mEffects = NULL;
    }
    return theResult;
}

extern EEGraphicObject* gGraphicObjectU4E8434;   // the built-in graphic objects outside gMediaTable (EEGraphicObject.cpp)
extern EEGraphicObject* gGraphicObjectU4E8438;
extern EEGraphicObject* gGraphicObjectU4E843C;
extern EEGraphicObject* gGraphicObjectU4E8440;

// FUNCTION: EXE 0x5ca6d9
// Unloads the four built-in graphic objects and deletes those that are not in the media table
void EEUnloadBuiltInGraphicObjects()
{
    EEGraphicObject* theObject4 = gGraphicObjectU4E8434;
    EEGraphicObject* theObject3 = gGraphicObjectU4E8438;
    EEGraphicObject* theObject2 = gGraphicObjectU4E843C;
    EEGraphicObject* theObject1 = gGraphicObjectU4E8440;
    if (theObject1 != NULL) {
        theObject1->UnloadMedia();
        if (theObject1->mMediaIndex == -1)
            delete theObject1;
    }
    if (theObject2 != NULL) {
        theObject2->UnloadMedia();
        if (theObject2->mMediaIndex == -1)
            delete theObject2;
    }
    if (theObject3 != NULL) {
        theObject3->UnloadMedia();
        if (theObject3->mMediaIndex == -1)
            delete theObject3;
    }
    if (theObject4 == NULL)
        return;
    theObject4->UnloadMedia();
    if (theObject4->mMediaIndex == -1)
        delete theObject4;
}

// FUNCTION: EXE 0x5ca76c
// Runs every effect of an event for a player (a given player's own index wins); returns the last effect's result
long EEEvent::Trigger(long inEvent, long inPlayerIndex, long inID, EEPlayer* inPlayer, EEComplexUnit* inUnit)
{
    long theResult = 0;
    long thePlayerIndex = inPlayer != NULL ? inPlayer->mPlayerID : inPlayerIndex;
    // the range test written out (Get()'s inline form keeps a NULL path the original threads away); the table
    // holds events 0..1499 (event 1500 used to read one entry past it)
    if (inEvent >= 0 && inEvent < 1500) {
        EEEvent* theEvent = &EEEvent::sEvents[inEvent];
        if (theEvent != NULL && theEvent->mEffects != NULL) {
            for (unsigned long i = 0; i < theEvent->mNumEffects; i++)
                theResult = EEEvent::RunEffect(theEvent->mEffects[i], thePlayerIndex, inID, inPlayer, inUnit);
        }
    }
    return theResult;
}

long Fn1CA9CF(EEEventEffect* inEffect, long inPlayerIndex, EEPlayer* inPlayer);
long Fn1CAEA4(EEEventEffect* inEffect, long inPlayerIndex, EEPlayer* inPlayer);
void Fn1CB964(EEEventEffect* inEffect, long inIndex, EEPlayer* inPlayer, EEUnitClass* inClass, EEUnitClassUpgrade* inUpgrade);
long Fn1CAAED(EEEventEffect* inEffect, long inPlayerIndex, long inID, EEPlayer* inPlayer);
long Fn1CABBF(EEEventEffect* inEffect, long inPlayerIndex, long inID, EEPlayer* inPlayer);
long Fn1CACB4(EEEventEffect* inEffect, long inPlayerIndex, long inID, EEComplexUnit* inUnit);
long Fn1CAA9A(EEEventEffect* inEffect, long inPlayerIndex, EEPlayer* inPlayer);
long PlayEventSound(EEEventEffect* inEffect);
long ResearchEventTechnology(EEEventEffect* inEffect, long inPlayerIndex, EEPlayer* inPlayer);
long Fn1CB8F6(EEEventEffect* inEffect, long inPlayerIndex, EEPlayer* inPlayer);
long SetEventPrerequisite(EEEventEffect* inEffect, long inPlayerIndex, long inPrerequisite, EEPlayer* inPlayer);
// kind 7: an empty handler (`return 0`, ICF-folded into 0xfca98; provisional name, defined at the end of the unit)
long EEEventKind7(EEEventEffect* inEffect, long inPlayerIndex, EEPlayer* inPlayer);

// FUNCTION: EXE 0x5ca7cf
// Runs one event effect by kind (stats, button, model, sounds, research, unit upgrade ...); returns 9 for a bad id or
// while loading a save unless the effect runs on load, 3 for an unknown kind
long EEEvent::RunEffect(unsigned long inEffect, long inPlayerIndex, long inID, EEPlayer* inPlayer, EEComplexUnit* inUnit)
{
    long theResult = 9;
    long thePlayerIndex = inPlayer != NULL ? inPlayer->mPlayerID : inPlayerIndex;
    if (inEffect < 2000 && (!gLoadingSavedGame || gEventEffects[inEffect].mRunOnLoad == 1)) {
        switch (gEventEffects[inEffect].mKind) {
        case 2:
            theResult = Fn1CAEA4(&gEventEffects[inEffect], thePlayerIndex, inPlayer);
            break;
        case 8:
            theResult = SetEventPrerequisite(&gEventEffects[inEffect], thePlayerIndex, inID, inPlayer);
            break;
        case 9:
            theResult = Fn1CB8F6(&gEventEffects[inEffect], thePlayerIndex, inPlayer);
            break;
        case 6:
            theResult = Fn1CAAED(&gEventEffects[inEffect], thePlayerIndex, inID, inPlayer);
            break;
        case 12:
        case 13:
        case 14:
            theResult = Fn1CAA9A(&gEventEffects[inEffect], thePlayerIndex, inPlayer);
            break;
        case 10:
            theResult = ResearchEventTechnology(&gEventEffects[inEffect], thePlayerIndex, inPlayer);
            break;
        case 1:
            theResult = Fn1CA9CF(&gEventEffects[inEffect], thePlayerIndex, inPlayer);
            break;
        case 7:
            theResult = EEEventKind7(&gEventEffects[inEffect], thePlayerIndex, inPlayer);
            break;
        case 11:
            theResult = PlayEventSound(&gEventEffects[inEffect]);
            break;
        case 15:
        case 16:
        case 17:
        case 18:
        case 20:
        case 21:
            theResult = Fn1CABBF(&gEventEffects[inEffect], thePlayerIndex, inID, inPlayer);
            break;
        case 19:
            if (gEventsEnabled && !gLoadingSavedGame)
                theResult = Fn1CACB4(&gEventEffects[inEffect], thePlayerIndex, inID, inUnit);
            else
                theResult = 0;
            break;
        case 22:
            theResult = 0;
            break;
        default:
            theResult = 3;
            break;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x5ca962
// Sets the unit type's area effect template and rebuilds the area effects of the player's living units of that type
long Fn1CA962(EEEventEffect* inEffect, long inPlayerIndex, EEPlayer* inPlayer,
              EEUnitClass* inClass, EEUnitClassUpgrade* inUpgrade)
{
    if (inPlayer == NULL)
        inPlayer = gWorld.mPlayers[inPlayerIndex];
    if (inClass != NULL) {
        inUpgrade->mAreaEffectTemplates[0] = inEffect->mAreaEffect;
        if (!gLoadingSavedGame) {
            for (EEUnit* theUnit = inPlayer->mFirstUnit; theUnit != NULL;
                 theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
                if (theUnit->mUnitClass->mUnitType == inClass->mUnitType &&
                    theUnit->mComplexUnit != NULL && theUnit->GetHitPoints() > 0)
                    theUnit->mComplexUnit->MakeAreaEffects();
            }
        }
    }
    return 0;
}

// FUNCTION: EXE 0x5ca9cf
// Gives a tech node and its unit class a new command button, refreshing the local player's saved groups of that
// class; returns 9 for a missing node or button
long Fn1CA9CF(EEEventEffect* inEffect, long inPlayerIndex, EEPlayer* inPlayer)
{
    if (inPlayer == NULL)
        inPlayer = gWorld.mPlayers[inPlayerIndex];
    EETechTreeNode* theNode = inPlayer->GetTechTree()->Get(inEffect->mTechnology);
    EEButtonObject* theButton = EEButtonObject::Get(inEffect->mButton);
    long theResult = 9;
    if (theNode != NULL && theButton != NULL) {
        theNode->SetButton(inEffect->mButton);
        EEUnitClass* theClass = gUnitClassTable[theNode->mData->mUnitClass];
        if (theClass != NULL) {
            inPlayer->SetClassButton(theClass->mUnitType, theButton);
            if (!gLoadingSavedGame && inPlayer == gWorld.GetCurrentPlayer()) {
                for (unsigned long i = 0; i < 20; i++) {
                    if (inPlayer->mGroups[i]->GetFirstUnit() != NULL && inPlayer->mGroups[i]->GetFirstUnit()->mUnitClass == theClass
                        && gUserInterface != NULL)
                        gUserInterface->PostRefreshSavedGroup(i + 1);
                }
            }
        }
        theResult = 0;
    }
    return theResult;
}

// FUNCTION: EXE 0x5caa9a
// Sets the player's tech tree background media and posts a background refresh (twice for the local player)
long Fn1CAA9A(EEEventEffect* inEffect, long inPlayerIndex, EEPlayer* inPlayer)
{
    if (inPlayer == NULL)
        inPlayer = gWorld.mPlayers[inPlayerIndex];
    if (inPlayer == NULL)
        return 0;
    inPlayer->GetTechTree()->mMediaID = inEffect->mMediaIndex;
    gUserInterface->PostUpdateBackgroundMedia();
    if (inPlayer == gWorld.GetCurrentPlayer())
        gUserInterface->PostUpdateBackgroundMedia();
    return 0;
}

// FUNCTION: EXE 0x5caaed
// Swaps the unit type's model for the player when this is its latest model effect, updating its units; 9 on failure
long Fn1CAAED(EEEventEffect* inEffect, long inPlayerIndex, long inID, EEPlayer* inPlayer)
{
    if (inPlayer == NULL)
        inPlayer = gWorld.mPlayers[inPlayerIndex];
    EEUnitClass* theClass = gUnitClassTable[inEffect->mUnitClass];
    if (theClass == NULL || !EETechTree::IsLatestEffect(inPlayer->GetTechTree(), inID,
                                               theClass->GetUnitType(), inEffect->mKind))
        return 9;
    EEUnitClassUpgrade* theUpgrade = inPlayer->mUnitClassUpgrades[theClass->GetUnitType()];
    if (theUpgrade == NULL)
        return 9;
    EEGraphicObject* theModel = static_cast<EEGraphicObject*>(EEMedia::Get(inEffect->mMediaIndex));
    if (theModel == NULL)
        return 9;
    EEGraphicObject* theOldModel = theUpgrade->mModel;
    inPlayer->SetClassModel(theClass->GetUnitType(), theModel);
    for (EEUnit* theUnit = inPlayer->mFirstUnit; theUnit != NULL;
         theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
        if (theUnit->mComplexUnit != NULL && theUnit->mComplexUnit->mUnitClass == theClass &&
            theUpgrade->GetMediaCount() > 0)
            theUnit->mComplexUnit->SwapGraphic(theOldModel, theModel);
    }
    return 0;
    return 9;
}

// FUNCTION: EXE 0x5cabbf
// Sets a unit type's acknowledge, creation, death or selection sound when this is its latest such effect; 9 on failure
long Fn1CABBF(EEEventEffect* inEffect, long inPlayerIndex, long inID, EEPlayer* inPlayer)
{
    if (inPlayer == NULL)
        inPlayer = gWorld.mPlayers[inPlayerIndex];
    EEUnitClass* theClass = gUnitClassTable[inEffect->mUnitClass];
    long theResult = 9;
    if (theClass != NULL) {
        if (EETechTree::IsLatestEffect(inPlayer->GetTechTree(), inID, theClass->GetUnitType(), inEffect->mKind)) {
            EEUnitClassUpgrade* theUpgrade = inPlayer->GetUpgrade(theClass->GetUnitType());
            if (theUpgrade != NULL) {
                EESoundObject* theSound = EESoundObject::Get(inEffect->mSoundID);
                if (theSound != NULL) {
                    switch (inEffect->mKind) {
                    case 0xf:
                        inPlayer->SetClassAcknowledgeSound(theClass->GetUnitType(), theSound, 0);
                        break;
                    case 0x10:
                        inPlayer->SetClassCreationSound(theClass->GetUnitType(), theSound);
                        break;
                    case 0x11:
                        inPlayer->SetClassDeathSound(theClass->GetUnitType(), theSound);
                        break;
                    case 0x12:
                        inPlayer->SetClassSelectionSound(theClass->GetUnitType(), theSound, 0);
                        break;
                    case 0x14:
                        inPlayer->SetClassAcknowledgeSound(theClass->GetUnitType(), theSound, 1);
                        break;
                    case 0x15:
                        inPlayer->SetClassSelectionSound(theClass->GetUnitType(), theSound, 1);
                        break;
                    }
                    theResult = 0;
                }
            }
        }
    }
    return theResult;
}

void EEReplaceQueuedTechnology(EEPlayer* inPlayer, EEUnitClass* inClass, long inNewTechnology);  // 0x244deb (ee/EEUnitUtilities.h)
long EEGetPlayerUnitsOfType(long inUnitType, long inPlayer, std::vector<EEUnit*>& outUnits);   // 0x24c395 (ee/EEAreaUtilities.h)

// FUNCTION: EXE 0x5cacb4
// Replaces the player's units of one class by the effect's new class (kind 0x13), or only inUnit when it is in the
// effect's unit set and inID is a kind-3 tech node; returns 3 when no unit qualifies
long Fn1CACB4(EEEventEffect* inEffect, long inPlayerIndex, long inID, EEComplexUnit* inUnit)
{
    std::vector<EEUnit*> theUnits;
    EEPlayer* thePlayer = gWorld.mPlayers[inPlayerIndex];
    unsigned long theFrom = inEffect->mUnitClass;
    bool theInSet = false;
    unsigned long theTo = inEffect->mNewUnitClass;
    long theResult = 0;
    if (inUnit != NULL && thePlayer != NULL && thePlayer->GetTechTree()->Get(inID) != NULL
        && thePlayer->GetTechTree()->Get(inID)->mData->mKind == 3) {
        theFrom = inUnit->GetClass()->mUnitType;
        theInSet = gUnitSetManager.GetSet(inEffect->mUnitSet)->Contains(theFrom);
    }
    if (inUnit != NULL && theInSet)
        theUnits.push_back(inUnit);
    else {
        EEUnitClassUpgrade* theToUpgrade = thePlayer->mUnitClassUpgrades[theTo];
        EEUnitClassUpgrade* theFromUpgrade = thePlayer->mUnitClassUpgrades[theFrom];
        theFromUpgrade->CopyUpgradeLevels(theToUpgrade);
        EEReplaceQueuedTechnology(thePlayer, theFromUpgrade->mUnitClass, theToUpgrade->mUnitClass->mTechnology);
        EEGetPlayerUnitsOfType(theFrom, inPlayerIndex, theUnits);
    }
    if (theUnits.size() != 0) {
        EEUnitClass* theClass = gUnitClassTable[theTo];
        if (theClass != NULL && inUnit != NULL && theInSet) {
            EETechTreeNode* theNode = thePlayer->GetTechTree()->Get(theClass->GetTechnology());
            if (theNode != NULL)
                theNode->mResearchDelay++;
        }
        if (inUnit == NULL || !theInSet)
            thePlayer->MoveQueuedImprovements(theFrom, theTo);
        bool theSetBehavior;
        if (inID == 0x413 || inID == 0x138d)
            theSetBehavior = true;
        else
            theSetBehavior = false;
        EEComplexUnit* theNew = NULL;
        for (std::vector<EEUnit*>::iterator it = theUnits.begin(); it != theUnits.end(); it++) {
            EEUnit* theUnit = *it;
            if (!(theUnit->GetHitPoints() > 0))
                continue;
            if (theUnit->mTileNode == NULL || theUnit->GetComplexUnit()->mPreviousPart == NULL)
                theNew = EEReplaceUnit(static_cast<EEComplexUnit*>(theUnit), inPlayerIndex, theClass);
            if (theSetBehavior && theNew != NULL)
                theNew->SetBehavior(theNew->GetClass()->mBehavior);
        }
    } else
        theResult = 3;
    return theResult;
}

// FUNCTION: EXE 0x5caea4
// Applies an event's stat change to a unit class or unit set for the player (attack, range, sight, hit points, armor,
// gather rates, terrain ...) or to the player (population bonus, tribute fee); 9 for an unknown stat
long Fn1CAEA4(EEEventEffect* inEffect, long inPlayerIndex, EEPlayer* inPlayer)
{
    EEPlayer* thePlayer = inPlayer;
    if (thePlayer == NULL)
        thePlayer = gWorld.mPlayers[inPlayerIndex];
    std::vector<EEUnitClass*> theClasses;
    EEUnitClass* theClass = NULL;
    long theResult = 0;
    if (thePlayer != NULL) {
        if (inEffect->mAttribute == 23)
            thePlayer->mPopulationLimitBonus += (long)inEffect->mOffset;
        else if (inEffect->mAttribute == 38)
            thePlayer->mTributeFee = inEffect->mFactor;
        else if (inEffect->mAttribute == 39)
            thePlayer->mBB44 = true;
        else {
            if (inEffect->mUnitClass != -1) {
                theClass = gUnitClassTable[inEffect->mUnitClass];
                theClasses.push_back(theClass);
            }
            if (theClass == NULL && inEffect->mUnitSet != -1)
                EEUnitClass::GetClassesInSet(inEffect->mUnitSet, theClasses);
        }
        for (unsigned long i = 0; i < theClasses.size(); i++) {
            theClass = theClasses[i];
            EEUnitClassUpgrade* theUpgrade = thePlayer->mUnitClassUpgrades[theClass->GetUnitType()];
            switch (inEffect->mAttribute) {
            case 1:
                theUpgrade->mAttack = EEApplyEventEffect((float)theClass->GetAttack(), theUpgrade->GetAttack(), inEffect);
                break;
            case 2:
                theUpgrade->mReloadTime = EEApplyEventEffect(theClass->mReloadTime, theUpgrade->mReloadTime, inEffect);
                break;
            case 3:
                theUpgrade->SetRange(EEApplyEventEffect(theClass->mRange, theUpgrade->GetRange(), inEffect));
                break;
            case 4: {
                for (EEUnit* theUnit = thePlayer->mFirstUnit; theUnit != NULL;
                     theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
                    if (theUnit->GetClass()->GetUnitType() == theClass->mUnitType)
                        theUnit->SightOffHere();
                }
                theUpgrade->mLineOfSight = EEApplyEventEffect(theClass->mLineOfSight, theUpgrade->GetLineOfSight(), inEffect);
                for (EEUnit* theNext = thePlayer->mFirstUnit; theNext != NULL;
                     theNext = static_cast<EEUnit*>(theNext->mNext)) {
                    if (theNext->GetClass()->GetUnitType() == theClass->mUnitType)
                        theNext->SightOnHere();
                }
                break;
            }
            case 5:
                theUpgrade->mAreaEffect = EEApplyEventEffect(theClass->mAreaEffect, theUpgrade->mAreaEffect, inEffect);
                break;
            case 6: {
                long theOldMaximum = theUpgrade->mMaxHitPoints;
                long theBaseMaximum = (long)((gGameVariants[gWorld.mGameOptions.GetVariant()]->GetFactor(
                    theUpgrade->mUnitClass->mUnitType) + 1.0f) * theClass->mHitPoints);
                long theMaximum = EEApplyEventEffect(theBaseMaximum, theOldMaximum, inEffect);
                if (theMaximum <= 1) theMaximum = 1;
                theUpgrade->mMaxHitPoints = theMaximum;
                if (theOldMaximum != 0 && !gLoadingSavedGame) {
                    for (EEUnit* theUnit = thePlayer->mFirstUnit; theUnit != NULL;
                         theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
                        if (theUnit->GetClass()->GetUnitType() != theClass->mUnitType || !(theUnit->GetHitPoints() > 0) ||
                            (theUnit->GetComplexUnit() != NULL && theUnit->mComplexUnit->mIsBuilt != true))
                            continue;
                        float theAmount = (float)UMCeil((float)theUnit->GetHitPoints() / theOldMaximum * theUpgrade->mMaxHitPoints);
                        if (theUnit->GetHitPoints() > 0)
                            theUnit->SetHitPoints((long)theAmount);
                    }
                    for (unsigned long j = 0; j < thePlayer->GetNumPendingUnits(); j++) {
                        EEUnit* theUnit = static_cast<EEUnit*>(thePlayer->GetPendingUnit(j));
                        if (theUnit->mUnitClass->mUnitType != theClass->mUnitType || !(theUnit->GetHitPoints() > 0))
                            continue;
                        float theAmount = (float)UMCeil((float)theUnit->GetHitPoints() / theOldMaximum * theUpgrade->mMaxHitPoints);
                        if (theUnit->GetHitPoints() > 0 &&
                            (theUnit->mComplexUnit == NULL || theUnit->mComplexUnit->mIsBuilt == true))
                            theUnit->SetHitPoints((long)theAmount);
                    }
                }
                break;
            }
            case 9:
                theUpgrade->SetMaxVelocity(EEApplyEventEffect(theClass->mMaxVelocity, theUpgrade->mMaxVelocity, inEffect));
                break;
            case 10: {
                long theOldMaximum = theUpgrade->mFlightTime;
                theUpgrade->mFlightTime = EEApplyEventEffect(theClass->mFlightTime, theUpgrade->mFlightTime, inEffect);
                if (theOldMaximum != 0 && !gLoadingSavedGame) {
                    for (EEUnit* theUnit = thePlayer->mFirstUnit; theUnit != NULL;
                         theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
                        if (theUnit->mUnitClass->mUnitType != theClass->mUnitType || !(theUnit->GetHitPoints() > 0) ||
                            theUnit->mComplexUnit == NULL)
                            continue;
                        float theAmount = (float)UMCeil((float)theUnit->mComplexUnit->mFlightTime / theOldMaximum * theUpgrade->mFlightTime);
                        long theValue = (long)theAmount;
                        if (theUnit->mComplexUnit->mFlightTime > 0)
                            theUnit->mComplexUnit->SetFlightTime(theValue);
                    }
                    for (unsigned long j = 0; j < thePlayer->GetNumPendingUnits(); j++) {
                        EEUnit* theUnit = static_cast<EEUnit*>(thePlayer->GetPendingUnit(j));
                        if (theUnit->mUnitClass->mUnitType != theClass->mUnitType || !(theUnit->GetHitPoints() > 0) ||
                            theUnit->mComplexUnit == NULL)
                            continue;
                        float theAmount = (float)UMCeil((float)theUnit->mComplexUnit->mFlightTime / theOldMaximum * theUpgrade->mFlightTime);
                        long theValue = (long)theAmount;
                        if (theUnit->mComplexUnit->mFlightTime > 0)
                            theUnit->mComplexUnit->SetFlightTime(theValue);
                    }
                }
                break;
            }
            case 11:
                theUpgrade->mLeadership = EEApplyEventEffect((float)theClass->mLeadership, theUpgrade->mLeadership, inEffect);
                break;
            case 16:
                theUpgrade->mArmor[0] = EEApplyEventEffect((float)theClass->mArmor[0], theUpgrade->mArmor[0], inEffect);
                break;
            case 17:
                theUpgrade->mArmor[2] = EEApplyEventEffect((float)theClass->mArmor[2], theUpgrade->mArmor[2], inEffect);
                break;
            case 18:
                theUpgrade->mArmor[1] = EEApplyEventEffect((float)theClass->mArmor[1], theUpgrade->mArmor[1], inEffect);
                break;
            case 19:
                theUpgrade->mArmor[3] = EEApplyEventEffect((float)theClass->mArmor[3], theUpgrade->mArmor[3], inEffect);
                break;
            case 20:
                theUpgrade->mArmor[5] = EEApplyEventEffect((float)theClass->mArmor[5], theUpgrade->mArmor[5], inEffect);
                break;
            case 21:
                theUpgrade->mArmor[4] = EEApplyEventEffect((float)theClass->mArmor[4], theUpgrade->mArmor[4], inEffect);
                break;
            case 22: case 24: case 25: case 34: case 35:
                break;
            case 26:
                Fn1CB964(inEffect, 0, thePlayer, theClass, theUpgrade);
                break;
            case 27:
                Fn1CB964(inEffect, 1, thePlayer, theClass, theUpgrade);
                break;
            case 28:
                Fn1CB964(inEffect, 2, thePlayer, theClass, theUpgrade);
                break;
            case 29:
                Fn1CB964(inEffect, 3, thePlayer, theClass, theUpgrade);
                break;
            case 31:
                Fn1CB964(inEffect, 4, thePlayer, theClass, theUpgrade);
                break;
            case 32:
                Fn1CB964(inEffect, 5, thePlayer, theClass, theUpgrade);
                break;
            case 33:
                Fn1CB964(inEffect, 6, thePlayer, theClass, theUpgrade);
                break;
            case 36:
                theUpgrade->mConvertsPriests = true;
                break;
            case 37:
                theUpgrade->mConvertsBuildings = true;
                break;
            case 40:
                theResult = Fn1CA962(inEffect, inPlayerIndex, inPlayer, theClass, theUpgrade);
                break;
            case 41:
                theUpgrade->mPowerRegeneration = EEApplyEventEffect(theClass->mPowerRegeneration, theUpgrade->mPowerRegeneration, inEffect);
                break;
            case 42:
                if (inEffect->mTerrainType >= 0 && inEffect->mTerrainType < 17) {
                    if (theUpgrade != NULL) {
                        if (inEffect->mValue == 0.0f) {
                            if ((theUpgrade->mTerrainMask & (1UL << inEffect->mTerrainType)) != 0)
                                theUpgrade->mTerrainMask &= ~(1UL << inEffect->mTerrainType);
                        } else if ((theUpgrade->mTerrainMask & (1UL << inEffect->mTerrainType)) == 0)
                            theUpgrade->mTerrainMask |= (1UL << inEffect->mTerrainType);
                    }
                    EEUnitSet* theSet = gUnitSetManager.GetSet(inEffect->mUnitSet);
                    if (theSet != NULL) {
                        EEUnitSetFunctorModifyPathfindingBehavior theFunctor(thePlayer, inEffect->mTerrainType, inEffect->mValue == 1.0f);
                        theSet->ForEach(&theFunctor);
                    }
                }
                break;
            case 43:
                theUpgrade->mRegeneration = EEApplyEventEffect(theClass->mRegeneration, theUpgrade->mRegeneration, inEffect);
                break;
            case 44:
                theUpgrade->mPower = EEApplyEventEffect(theClass->mPower, theUpgrade->mPower, inEffect);
                break;
            case 45: {
                UWideString theName;
                gResource->GetString(UMFloatToLong(inEffect->mValue), theName);
                theUpgrade->SetName(theName);
                break;
            }
            default:
                theResult = 9;
                break;
            }
        }
    } else
        theResult = 9;
    return theResult;
}


// FUNCTION: EXE 0x5cb83b
// Allows or forbids the terrain type in the player's upgrade of a unit type; always continues the set walk
bool EEUnitSetFunctorModifyPathfindingBehavior::Execute(long inUnitType)
{
    if ((unsigned long)inUnitType < 0x367) {
        EEUnitClassUpgrade* theUpgrade = this->mPlayer->GetUpgrade(inUnitType);
        if (theUpgrade != NULL) {
            if (this->mEnabled) {
                if ((theUpgrade->mTerrainMask & (1UL << this->mType)) == 0)
                    theUpgrade->mTerrainMask |= (1UL << this->mType);
            } else if ((theUpgrade->mTerrainMask & (1UL << this->mType)) != 0)
                theUpgrade->mTerrainMask &= ~(1UL << this->mType);
        }
    }
    return true;
}


// FUNCTION: EXE 0x5cb893
// Plays the event's sound; returns 9 if it does not exist
long PlayEventSound(EEEventEffect* inEffect)
{
    EESoundObject* theSound = EESoundObject::Get(inEffect->mSoundID);
    long theResult = 9;
    if (theSound != NULL) {
        theSound->Play();
        theResult = 0;
    }
    return theResult;
}

// FUNCTION: EXE 0x5cb8b1
// Researches the event's technology for the player at once; returns 9 without a player
long ResearchEventTechnology(EEEventEffect* inEffect, long inPlayerIndex, EEPlayer* inPlayer)
{
    long theResult = 0;
    if (inPlayer == NULL)
        inPlayer = gWorld.mPlayers[inPlayerIndex];
    if (inPlayer != NULL) {
        EETechnology* theTechnology = inPlayer->GetTechTree()->Get(inEffect->mTechnology);
        if (theTechnology != NULL)
            theResult = theTechnology->ResearchNode(NULL);
    } else
        theResult = 9;
    return theResult;
}

// FUNCTION: EXE 0x5cb8f6
// Makes the event's technology obsolete for the player; returns 9 without a player
long Fn1CB8F6(EEEventEffect* inEffect, long inPlayerIndex, EEPlayer* inPlayer)
{
    if (inPlayer == NULL)
        inPlayer = gWorld.mPlayers[inPlayerIndex];
    long theResult = 0;
    if (inPlayer != NULL)
        inPlayer->mTechnologies->MakeObsolete(inEffect->mTechnology);
    else
        theResult = 9;
    return theResult;
}

// FUNCTION: EXE 0x5cb92b
// Sets a prerequisite of the event's technology for the player; returns 9 without a player
long SetEventPrerequisite(EEEventEffect* inEffect, long inPlayerIndex, long inPrerequisite, EEPlayer* inPlayer)
{
    if (inPlayer == NULL)
        inPlayer = gWorld.mPlayers[inPlayerIndex];
    long theResult = 0;
    if (inPlayer != NULL)
        inPlayer->mTechnologies->SetPrerequisite(inEffect->mTechnology, inPrerequisite);
    else
        theResult = 9;
    return theResult;
}

// FUNCTION: EXE 0x5cb964
// Changes one gather rate of the unit type's upgrade by the event: offset, factor of the class rate, or set value
void Fn1CB964(EEEventEffect* inEffect, long inIndex, EEPlayer* inPlayer,
              EEUnitClass* inClass, EEUnitClassUpgrade* inUpgrade)
{
    if (inUpgrade == NULL)
        return;
    if (inEffect->mOffset != 0.0f)
        inUpgrade->mGatherTimes[inIndex] += (long)inEffect->mOffset;
    else if (inEffect->mFactor != 0.0f)
        inUpgrade->mGatherTimes[inIndex] += (long)(inClass->mGatherTimes[inIndex] * inEffect->mFactor);
    else if (inEffect->mValue != 0.0f)
        inUpgrade->mGatherTimes[inIndex] = (long)inEffect->mValue;
}

// TEMPLATE: EXE 0x5cb9e0
// ?push_back@?$vector@PAVEEUnitClass@@V?$allocator@PAVEEUnitClass@@@std@@@std@@QAEXABQAVEEUnitClass@@@Z

// FUNCTION: EXE 0x5cba0c
// Returns the button object with the given index, or NULL when out of range
EEButtonObject* EEButtonObject::Get(unsigned long inIndex)
{
    if (inIndex < EEButtonObject::sTable.size())
        return EEButtonObject::sTable[inIndex];
    return NULL;
}

long EEEventKind7(EEEventEffect* inEffect, long inPlayerIndex, EEPlayer* inPlayer)
{
    return 0;
}
