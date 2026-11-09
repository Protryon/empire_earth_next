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
#include "lle/FSFileSpec.h"
#include "ee/EEEvent.h"
#include "ee/EEGameVariant.h"
#include "lle/UResourceFile.h"
#include "ee/EEUnitSetManager.h"
#include "ee/EETechCapture.h"
#include "ee/EETechRust.h"

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
std::vector<EEEventEffect> gEventEffects;
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

std::vector<std::unique_ptr<EEEvent>> EEEvent::sEvents;

// FUNCTION: EXE 0x5ca591
// Returns an event effect applied to a float stat: base + offset, value * factor + base, or the set value (else 0)
float EEApplyEventEffect(float inValue, float inBase, EEEventEffect* inEffect)
{
    EeTechEffectValues theValues = EETechEffectView(inEffect);
    return ee_tech_effect_f32(inValue, inBase, &theValues);
}

// FUNCTION: EXE 0x5ca5df
// Returns an event effect applied to an integer stat: base + offset, value * factor + base, or the set value
long EEApplyEventEffect(long inValue, long inBase, EEEventEffect* inEffect)
{
    EeTechEffectValues theValues = EETechEffectView(inEffect);
    return ee_tech_effect_i32((int32_t)inValue, (int32_t)inBase, &theValues);
}

// FUNCTION: EXE 0x5ca63b
// Loads every event effect and every event with its effect list (as many as the files hold: the original read
// exactly 2000 effects and 1500 events); returns the last read's result
long EEEvent::LoadEvents(FSFileSpec& inEvents, FSFileSpec& inEffects)
{
    unsigned long theRead = 0;
    long theResult = kErrorOK;
    char theName[100];
    inEvents.Open(kFileAccessRead);
    inEffects.Open(kFileAccessRead);
    unsigned long theLength = 0;
    inEffects.GetLength(theLength);
    gEventEffects.assign(theLength / sizeof(EEEventEffect), EEEventEffect());
    if (!gEventEffects.empty())
        inEffects.Read((char*)&gEventEffects[0], (unsigned long)(gEventEffects.size() * sizeof(EEEventEffect)), theRead);
    inEvents.GetLength(theLength);
    EEEvent::sEvents.clear();
    unsigned long thePosition = 0;
    while (thePosition + 108 <= theLength) {
        EEEvent* theEvent = new EEEvent;
        EEEvent::sEvents.emplace_back(theEvent);
        inEvents.Read(theName, 100, theRead);
        inEvents.Read(theEvent->mID);
        theResult = inEvents.Read(theEvent->mNumEffects);
        thePosition += 108 + 4 * theEvent->mNumEffects;
        if (theEvent->mNumEffects > 0) {
            theEvent->mEffects = new int32_t[theEvent->mNumEffects];
            theResult = inEvents.Read(reinterpret_cast<char*>(theEvent->mEffects), theEvent->mNumEffects * 4, theRead);
        } else
            theEvent->mEffects = NULL;
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
    // every dbevents row is an event (the original: 0..1499; event 1500 used to read one entry past it)
    if (inEvent >= 0 && (unsigned long)inEvent < EEEvent::sEvents.size()) {
        EEEvent* theEvent = EEEvent::sEvents[inEvent].get();
        if (theEvent != NULL && theEvent->mEffects != NULL) {
            for (unsigned long i = 0; i < theEvent->mNumEffects; i++)
                theResult = EEEvent::RunEffect(theEvent->mEffects[i], thePlayerIndex, inID, inPlayer, inUnit);
        }
    }
    return theResult;
}

long SetEventButton(EEEventEffect* inEffect, long inPlayerIndex, EEPlayer* inPlayer);
long ApplyEventAttribute(EEEventEffect* inEffect, long inPlayerIndex, EEPlayer* inPlayer);
void ApplyEventGatherRate(EEEventEffect* inEffect, long inIndex, EEPlayer* inPlayer, EEUnitClass* inClass, EEUnitClassUpgrade* inUpgrade);
long SetEventUnitModel(EEEventEffect* inEffect, long inPlayerIndex, long inID, EEPlayer* inPlayer);
long SetEventUnitSound(EEEventEffect* inEffect, long inPlayerIndex, long inID, EEPlayer* inPlayer);
long ConvertEventUnits(EEEventEffect* inEffect, long inPlayerIndex, long inID, EEComplexUnit* inUnit);
long SetEventTechTreeMedia(EEEventEffect* inEffect, long inPlayerIndex, EEPlayer* inPlayer);
long PlayEventSound(EEEventEffect* inEffect);
long ResearchEventTechnology(EEEventEffect* inEffect, long inPlayerIndex, EEPlayer* inPlayer);
long MakeEventTechnologyObsolete(EEEventEffect* inEffect, long inPlayerIndex, EEPlayer* inPlayer);
long SetEventPrerequisite(EEEventEffect* inEffect, long inPlayerIndex, long inPrerequisite, EEPlayer* inPlayer);
// kind 7: an empty handler (`return 0`, ICF-folded into 0xfca98; provisional name, defined at the end of the unit)
long EEEventKind7(EEEventEffect* inEffect, long inPlayerIndex, EEPlayer* inPlayer);

// FUNCTION: EXE 0x5ca7cf
// Runs one event effect by kind (stats, button, model, sounds, research, unit upgrade ...); returns 9 for a bad id or
// while loading a save unless the effect runs on load, 3 for an unknown kind
long EEEvent::RunEffect(unsigned long inEffect, long inPlayerIndex, long inID, EEPlayer* inPlayer, EEComplexUnit* inUnit)
{
    if (EESimCaptureRecord::Active()) {   // TEST ONLY (ee/EETechCapture.h)
        EESimCaptureRecord theRecord(EECaptureTag("EVRN"));
        theRecord.U32(inEffect);
        bool theKnown = inEffect < gEventEffects.size();
        theRecord.I32(theKnown ? gEventEffects[inEffect].mKind : -1);
        theRecord.I32(theKnown ? gEventEffects[inEffect].mAttribute : -1);
        theRecord.I32(inPlayer != NULL ? inPlayer->mPlayerID : inPlayerIndex);
        theRecord.I32(inID);
        theRecord.I32(inUnit != NULL ? (long)inUnit->mUnitID : -1);
        theRecord.U32((gLoadingSavedGame ? 1u : 0u) | (gEventsEnabled ? 2u : 0u));
        long theResult = EEEvent::RunEffectUncaptured(inEffect, inPlayerIndex, inID, inPlayer, inUnit);
        theRecord.I32(theResult);
        theRecord.Commit();
        return theResult;
    }
    return EEEvent::RunEffectUncaptured(inEffect, inPlayerIndex, inID, inPlayer, inUnit);
}

long EEEvent::RunEffectUncaptured(unsigned long inEffect, long inPlayerIndex, long inID, EEPlayer* inPlayer, EEComplexUnit* inUnit)
{
    long theResult = 9;
    long thePlayerIndex = inPlayer != NULL ? inPlayer->mPlayerID : inPlayerIndex;
    bool theKnown = inEffect < gEventEffects.size();
    // which handler runs the effect is the Rust tech logic's (ee_tech effect_handler)
    switch (ee_tech_effect_handler(theKnown, theKnown ? (int32_t)gEventEffects[inEffect].mKind : 0, gLoadingSavedGame,
                                   theKnown ? gEventEffects[inEffect].mRunOnLoad : 0, gEventsEnabled)) {
    case EeTechEffectHandler::Refused:
        theResult = 9;
        break;
    case EeTechEffectHandler::UnknownKind:
        theResult = 3;
        break;
    case EeTechEffectHandler::Nothing:   // kind 22; kind 19 while it may not run
        theResult = 0;
        break;
    case EeTechEffectHandler::Kind7:
        theResult = EEEventKind7(&gEventEffects[inEffect], thePlayerIndex, inPlayer);
        break;
    case EeTechEffectHandler::Attribute:
        theResult = ApplyEventAttribute(&gEventEffects[inEffect], thePlayerIndex, inPlayer);
        break;
    case EeTechEffectHandler::Prerequisite:
        theResult = SetEventPrerequisite(&gEventEffects[inEffect], thePlayerIndex, inID, inPlayer);
        break;
    case EeTechEffectHandler::Obsolete:
        theResult = MakeEventTechnologyObsolete(&gEventEffects[inEffect], thePlayerIndex, inPlayer);
        break;
    case EeTechEffectHandler::UnitModel:
        theResult = SetEventUnitModel(&gEventEffects[inEffect], thePlayerIndex, inID, inPlayer);
        break;
    case EeTechEffectHandler::TechTreeMedia:
        theResult = SetEventTechTreeMedia(&gEventEffects[inEffect], thePlayerIndex, inPlayer);
        break;
    case EeTechEffectHandler::Research:
        theResult = ResearchEventTechnology(&gEventEffects[inEffect], thePlayerIndex, inPlayer);
        break;
    case EeTechEffectHandler::Button:
        theResult = SetEventButton(&gEventEffects[inEffect], thePlayerIndex, inPlayer);
        break;
    case EeTechEffectHandler::Sound:
        theResult = PlayEventSound(&gEventEffects[inEffect]);
        break;
    case EeTechEffectHandler::UnitSound:
        theResult = SetEventUnitSound(&gEventEffects[inEffect], thePlayerIndex, inID, inPlayer);
        break;
    case EeTechEffectHandler::ConvertUnits:
        theResult = ConvertEventUnits(&gEventEffects[inEffect], thePlayerIndex, inID, inUnit);
        break;
    }
    return theResult;
}

// FUNCTION: EXE 0x5ca962
// Sets the unit type's area effect template and rebuilds the area effects of the player's living units of that type
long SetEventAreaEffect(EEEventEffect* inEffect, long inPlayerIndex, EEPlayer* inPlayer,
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
long SetEventButton(EEEventEffect* inEffect, long inPlayerIndex, EEPlayer* inPlayer)
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
long SetEventTechTreeMedia(EEEventEffect* inEffect, long inPlayerIndex, EEPlayer* inPlayer)
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
long SetEventUnitModel(EEEventEffect* inEffect, long inPlayerIndex, long inID, EEPlayer* inPlayer)
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
        if (theUnit->GetComplexUnit() != NULL && theUnit->GetComplexUnit()->GetUnitClass() == theClass &&
            theUpgrade->GetMediaCount() > 0)
            theUnit->GetComplexUnit()->SwapGraphic(theOldModel, theModel);
    }
    return 0;
    return 9;
}

// FUNCTION: EXE 0x5cabbf
// Sets a unit type's acknowledge, creation, death or selection sound when this is its latest such effect; 9 on failure
long SetEventUnitSound(EEEventEffect* inEffect, long inPlayerIndex, long inID, EEPlayer* inPlayer)
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
long ConvertEventUnits(EEEventEffect* inEffect, long inPlayerIndex, long inID, EEComplexUnit* inUnit)
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
long ApplyEventAttribute(EEEventEffect* inEffect, long inPlayerIndex, EEPlayer* inPlayer)
{
    EEPlayer* thePlayer = inPlayer;
    if (thePlayer == NULL)
        thePlayer = gWorld.mPlayers[inPlayerIndex];
    std::vector<EEUnitClass*> theClasses;
    EEUnitClass* theClass = NULL;
    long theResult = 0;
    bool theCapture = EESimCaptureRecord::Active();   // TEST ONLY (ee/EETechCapture.h)
    EESimCaptureRecord theCallRecord(EECaptureTag("EVAT"));
    if (theCapture) {
        theCallRecord.U32(0);
        theCallRecord.I32((long)(inEffect - &gEventEffects[0]));
        theCallRecord.I32(inEffect->mAttribute);
        theCallRecord.I32(inPlayerIndex);
        EETechCapturePlayer(theCallRecord, thePlayer);
    }
    // which stat the attribute changes, and how, are the Rust tech logic's (ee_tech effects.rs); the members stay here
    EeTechEffectValues theValues = EETechEffectView(inEffect);
    EeTechAttribute theOp = ee_tech_attribute((int32_t)inEffect->mAttribute);
    if (thePlayer != NULL) {
        if (theOp.op == EeTechAttributeOp::PopulationBonus)
            thePlayer->mPopulationLimitBonus = ee_tech_population_bonus((int32_t)thePlayer->mPopulationLimitBonus, &theValues);
        else if (theOp.op == EeTechAttributeOp::TributeFee)
            thePlayer->mTributeFee = inEffect->mFactor;
        else if (theOp.op == EeTechAttributeOp::PlayerFlag)
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
            EESimCaptureRecord theClassRecord(EECaptureTag("EVAT"));
            if (theCapture) {
                theClassRecord.U32(1);
                theClassRecord.I32((long)(inEffect - &gEventEffects[0]));
                theClassRecord.I32(theClass->GetUnitType());
                EETechCaptureUpgrade(theClassRecord, theUpgrade);
            }
            switch (theOp.op) {
            case EeTechAttributeOp::Attack:
                theUpgrade->mAttack = ee_tech_effect_f32((float)theClass->GetAttack(), theUpgrade->GetAttack(), &theValues);
                break;
            case EeTechAttributeOp::ReloadTime:
                theUpgrade->mReloadTime = ee_tech_effect_i32((int32_t)theClass->mReloadTime, (int32_t)theUpgrade->mReloadTime, &theValues);
                break;
            case EeTechAttributeOp::Range:
                theUpgrade->SetRange(ee_tech_effect_f32(theClass->mRange, theUpgrade->GetRange(), &theValues));
                break;
            case EeTechAttributeOp::LineOfSight: {
                for (EEUnit* theUnit = thePlayer->mFirstUnit; theUnit != NULL;
                     theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
                    if (theUnit->GetClass()->GetUnitType() == theClass->mUnitType)
                        theUnit->SightOffHere();
                }
                theUpgrade->mLineOfSight = ee_tech_effect_f32(theClass->mLineOfSight, theUpgrade->GetLineOfSight(), &theValues);
                for (EEUnit* theNext = thePlayer->mFirstUnit; theNext != NULL;
                     theNext = static_cast<EEUnit*>(theNext->mNext)) {
                    if (theNext->GetClass()->GetUnitType() == theClass->mUnitType)
                        theNext->SightOnHere();
                }
                break;
            }
            case EeTechAttributeOp::AreaEffect:
                theUpgrade->mAreaEffect = ee_tech_effect_f32(theClass->mAreaEffect, theUpgrade->mAreaEffect, &theValues);
                break;
            case EeTechAttributeOp::MaxHitPoints: {
                unsigned long theOldMaximum = theUpgrade->mMaxHitPoints;
                theUpgrade->mMaxHitPoints = ee_tech_max_hit_points((int32_t)theClass->mHitPoints,
                    gGameVariants[gWorld.mGameOptions.GetVariant()]->GetFactor(theUpgrade->mUnitClass->mUnitType),
                    (uint32_t)theOldMaximum, &theValues);
                if (theOldMaximum != 0 && !gLoadingSavedGame) {
                    for (EEUnit* theUnit = thePlayer->mFirstUnit; theUnit != NULL;
                         theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
                        if (theUnit->GetClass()->GetUnitType() != theClass->mUnitType || !(theUnit->GetHitPoints() > 0) ||
                            (theUnit->GetComplexUnit() != NULL && theUnit->mComplexUnit->mIsBuilt != true))
                            continue;
                        float theAmount = ee_tech_rescale((int32_t)theUnit->GetHitPoints(), (uint32_t)theOldMaximum,
                                                          (int32_t)theUpgrade->mMaxHitPoints);
                        if (theCapture)   // TEST ONLY (ee/EETechCapture.h)
                            EETechCaptureRescale(0, theUnit->mUnitID, theUnit->GetHitPoints(), theOldMaximum,
                                                 theUpgrade->mMaxHitPoints, theAmount);
                        if (theUnit->GetHitPoints() > 0)
                            theUnit->SetHitPoints((long)theAmount);
                    }
                    for (unsigned long j = 0; j < thePlayer->GetNumPendingUnits(); j++) {
                        EEUnit* theUnit = static_cast<EEUnit*>(thePlayer->GetPendingUnit(j));
                        if (theUnit->mUnitClass->mUnitType != theClass->mUnitType || !(theUnit->GetHitPoints() > 0))
                            continue;
                        float theAmount = ee_tech_rescale((int32_t)theUnit->GetHitPoints(), (uint32_t)theOldMaximum,
                                                          (int32_t)theUpgrade->mMaxHitPoints);
                        if (theCapture)   // TEST ONLY (ee/EETechCapture.h)
                            EETechCaptureRescale(0, theUnit->mUnitID, theUnit->GetHitPoints(), theOldMaximum,
                                                 theUpgrade->mMaxHitPoints, theAmount);
                        if (theUnit->GetHitPoints() > 0 &&
                            (theUnit->mComplexUnit == NULL || theUnit->mComplexUnit->mIsBuilt == true))
                            theUnit->SetHitPoints((long)theAmount);
                    }
                }
                break;
            }
            case EeTechAttributeOp::MaxVelocity:
                theUpgrade->SetMaxVelocity(ee_tech_effect_f32(theClass->mMaxVelocity, theUpgrade->mMaxVelocity, &theValues));
                break;
            case EeTechAttributeOp::FlightTime: {
                unsigned long theOldMaximum = theUpgrade->mFlightTime;
                theUpgrade->mFlightTime = ee_tech_effect_i32((int32_t)theClass->mFlightTime, (int32_t)theUpgrade->mFlightTime, &theValues);
                if (theOldMaximum != 0 && !gLoadingSavedGame) {
                    for (EEUnit* theUnit = thePlayer->mFirstUnit; theUnit != NULL;
                         theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
                        if (theUnit->mUnitClass->mUnitType != theClass->mUnitType || !(theUnit->GetHitPoints() > 0) ||
                            theUnit->mComplexUnit == NULL)
                            continue;
                        float theAmount = ee_tech_rescale((int32_t)theUnit->mComplexUnit->mFlightTime, (uint32_t)theOldMaximum,
                                                          (int32_t)theUpgrade->mFlightTime);
                        if (theCapture)   // TEST ONLY (ee/EETechCapture.h)
                            EETechCaptureRescale(1, theUnit->mUnitID, theUnit->mComplexUnit->mFlightTime, theOldMaximum,
                                                 theUpgrade->mFlightTime, theAmount);
                        long theValue = (long)theAmount;
                        if (theUnit->mComplexUnit->mFlightTime > 0)
                            theUnit->mComplexUnit->SetFlightTime(theValue);
                    }
                    for (unsigned long j = 0; j < thePlayer->GetNumPendingUnits(); j++) {
                        EEUnit* theUnit = static_cast<EEUnit*>(thePlayer->GetPendingUnit(j));
                        if (theUnit->mUnitClass->mUnitType != theClass->mUnitType || !(theUnit->GetHitPoints() > 0) ||
                            theUnit->mComplexUnit == NULL)
                            continue;
                        float theAmount = ee_tech_rescale((int32_t)theUnit->mComplexUnit->mFlightTime, (uint32_t)theOldMaximum,
                                                          (int32_t)theUpgrade->mFlightTime);
                        if (theCapture)   // TEST ONLY (ee/EETechCapture.h)
                            EETechCaptureRescale(1, theUnit->mUnitID, theUnit->mComplexUnit->mFlightTime, theOldMaximum,
                                                 theUpgrade->mFlightTime, theAmount);
                        long theValue = (long)theAmount;
                        if (theUnit->mComplexUnit->mFlightTime > 0)
                            theUnit->mComplexUnit->SetFlightTime(theValue);
                    }
                }
                break;
            }
            case EeTechAttributeOp::Leadership:
                theUpgrade->mLeadership = ee_tech_effect_f32((float)theClass->mLeadership, theUpgrade->mLeadership, &theValues);
                break;
            case EeTechAttributeOp::Armor:
                theUpgrade->mArmor[theOp.index] =
                    ee_tech_effect_f32((float)theClass->mArmor[theOp.index], theUpgrade->mArmor[theOp.index], &theValues);
                break;
            case EeTechAttributeOp::Nothing:
                break;
            case EeTechAttributeOp::GatherTime:
                ApplyEventGatherRate(inEffect, theOp.index, thePlayer, theClass, theUpgrade);
                break;
            case EeTechAttributeOp::ConvertsPriests:
                theUpgrade->mConvertsPriests = true;
                break;
            case EeTechAttributeOp::ConvertsBuildings:
                theUpgrade->mConvertsBuildings = true;
                break;
            case EeTechAttributeOp::AreaEffectTemplate:
                theResult = SetEventAreaEffect(inEffect, inPlayerIndex, inPlayer, theClass, theUpgrade);
                break;
            case EeTechAttributeOp::PowerRegeneration:
                theUpgrade->mPowerRegeneration =
                    ee_tech_effect_i32((int32_t)theClass->mPowerRegeneration, (int32_t)theUpgrade->mPowerRegeneration, &theValues);
                break;
            case EeTechAttributeOp::Terrain:
                if (ee_tech_terrain_type_valid((int32_t)inEffect->mTerrainType)) {
                    if (theUpgrade != NULL)
                        theUpgrade->mTerrainMask = ee_tech_terrain_mask((uint32_t)theUpgrade->mTerrainMask,
                                                                        (int32_t)inEffect->mTerrainType, inEffect->mValue != 0.0f);
                    EEUnitSet* theSet = gUnitSetManager.GetSet(inEffect->mUnitSet);
                    if (theSet != NULL) {
                        EEUnitSetFunctorModifyPathfindingBehavior theFunctor(thePlayer, inEffect->mTerrainType, inEffect->mValue == 1.0f);
                        theSet->ForEach(&theFunctor);
                    }
                }
                break;
            case EeTechAttributeOp::Regeneration:
                theUpgrade->mRegeneration = ee_tech_effect_i32((int32_t)theClass->mRegeneration, (int32_t)theUpgrade->mRegeneration, &theValues);
                break;
            case EeTechAttributeOp::Power:
                theUpgrade->mPower = ee_tech_effect_i32((int32_t)theClass->mPower, (int32_t)theUpgrade->mPower, &theValues);
                break;
            case EeTechAttributeOp::Name: {
                UWideString theName;
                gResource->GetString(UMFloatToLong(inEffect->mValue), theName);
                theUpgrade->SetName(theName);
                break;
            }
            default:   // an unknown attribute (and the player's, which build no class list)
                theResult = 9;
                break;
            }
            if (theCapture) {
                theClassRecord.I32(theResult);
                EETechCaptureUpgrade(theClassRecord, theUpgrade);
                theClassRecord.Commit();
            }
        }
    } else
        theResult = 9;
    if (theCapture) {
        theCallRecord.U32((unsigned int)theClasses.size());
        theCallRecord.I32(theResult);
        EETechCapturePlayer(theCallRecord, thePlayer);
        theCallRecord.Commit();
    }
    return theResult;
}


// FUNCTION: EXE 0x5cb83b
// Allows or forbids the terrain type in the player's upgrade of a unit type; always continues the set walk
bool EEUnitSetFunctorModifyPathfindingBehavior::Execute(long inUnitType)
{
    if ((unsigned long)inUnitType < EEUnitClassCount()) {
        EEUnitClassUpgrade* theUpgrade = this->mPlayer->GetUpgrade(inUnitType);
        if (theUpgrade != NULL)
            theUpgrade->mTerrainMask = ee_tech_terrain_mask((uint32_t)theUpgrade->mTerrainMask, (int32_t)this->mType, this->mEnabled);
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
long MakeEventTechnologyObsolete(EEEventEffect* inEffect, long inPlayerIndex, EEPlayer* inPlayer)
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
void ApplyEventGatherRate(EEEventEffect* inEffect, long inIndex, EEPlayer* inPlayer,
              EEUnitClass* inClass, EEUnitClassUpgrade* inUpgrade)
{
    if (inUpgrade == NULL)
        return;
    EeTechEffectValues theValues = EETechEffectView(inEffect);
    inUpgrade->mGatherTimes[inIndex] =
        ee_tech_gather_time((int32_t)inUpgrade->mGatherTimes[inIndex], (int32_t)inClass->mGatherTimes[inIndex], &theValues);
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
