// C:\EEX\Empire Earth\Scenario Manager\EESMEffect.cpp (file name inferred: no __FILE__): the scenario trigger
// effects, 0x1bb5a5..0x1c1ca9 plus the inline and template COMDATs after them. Unit flags: the exe module set.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/UWideString.h"
#include "ee/EESMDialogMessage.h"
#include "lle/FSFile.h"
#include <vector>
#include "ee/EEAreaUtilities.h"
#include "ee/EEScenarioManager.h"
#include "ee/EESMCondition.h"
#include <algorithm>
#include <mbstring.h>
#include <stdlib.h>
#include "ee/EESMEffect.h"
#include "ee/EESMObject.h"
#include "ee/EESMArea.h"
#include "ee/EESMTrigger.h"
#include "lle/USingleton.h"
#include "ee/EECalamityManager.h"   // (also defines DeleteVectorContents: ee/EEDeleteVectorContents.h)
#include "lle/UResourceFile.h"
#include "ee/EmpireEarth.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEUCAirplane.h"
#include "ee/EEUGLocation.h"
#include "ee/EEUCBuilding.h"
#include "ee/EEUCGate.h"
#include "ee/EEPhysics.h"
#include "ee/EEAIRandom.h"
#include "ee/EEDebugTrace.h"
#include "LostDebug.h"
#include "lle/SESoundManager.h"
#include "lle/FSFileSpec.h"
#include "GEWindow.h"
#include "ee/EECameraControl.h"
#include "ee/EEGraphics.h"
#include "ee/EEMouse.h"
#include "ee/EEUserInterface.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GESurface.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "lle/GEBitmap.h"
#include "lle/GEVertexArray.h"
#include "lle/GERhombus.h"
#include "lle/ISEntity.h"
#include "lle/UIEntity.h"
#include "lle/UIControl.h"
#include "lle/UIEvent.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIBackground.h"
#include "lle/UIFontTable.h"
#include "lle/Input System.h"
#include "lle/ISKeyboard.h"
#include "lle/UIHotKey.h"
#include "lle/UIIMECandidate.h"
#include "lle/UIForm.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GERasterizer.h"
#include "lle/UIManager.h"
#include "ee/EEUIManager.h"
#include "ee/EEUIFMainGame.h"
#include "ee/EEGameSound.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"
#include "ee/EECommunications.h"
#include "ee/EECPBehavior.h"
#include "ee/EEEffectKindIDs.h"

// FUNCTION: EXE 0x5bb5a5
// Creates a scenario effect with the default settings (see Reset)
EESMEffect::EESMEffect()
{
    this->Reset();
}

// FUNCTION: EXE 0x5bb609
// Destroys the effect (members only)
EESMEffect::~EESMEffect()
{
}

// FUNCTION: EXE 0x5bb65a
// Resets every effect setting to its default: an object Create effect, 20 s duration, empty texts and object list
void EESMEffect::Reset()
{
    this->mName = "";
    this->mText = "";
    this->mDuration = 0x14;
    this->mMessage = "";
    this->mS64 = "";
    this->mS78 = "";
    this->mNewName = "";
    this->mFileName = "";
    this->mDescription = "";
    this->mPlayerAttribute = 0;
    this->mDiplomacy = 0;
    this->mMessagePlayer = 7;
    this->mNewOwner = 7;
    this->mPlayer = 7;
    this->mDiplomacyPlayer = 8;
    this->mMediaAction = kSMMediaActionPlaySound;
    this->mULF4 = 0;
    this->mPlayerAction = kSMPlayerActionVictory;
    this->mTriggerState = kSMTriggerStateOn;
    this->mType = kSMEffectObject;
    this->mObjectAction = kSMObjectActionCreate;
    this->mTechnology = kTechEpochPrehistoric;
    this->mPlacedEffect = 0;
    this->mPlaceArea = 0;
    this->mPlaceObject = 0;
    this->mDaylightHour = 0xf;
    this->mCycleTime = 0;
    this->mGameAction = kSMGameActionDaylightHour;
    this->mTextObject = kSMMediaTextHints;
    this->mAttribute = kSMUnitAttributeHitPoints;
    this->mAttributePlayer = -1;
    this->mAttributeUnitType = 0;
    this->mAttributeUnit = -1;
    this->mAttributeMode = 0;
    this->mAttributeAmount = 0;
    this->mObject = 0;
    this->mStance = kSMStanceExplore;
    this->mTaskAction = kSMTaskActionNormal;
    this->mTaskArea = 0;
    this->mTaskObject = 0;
    this->mCivPoints = 1;
    this->mPlayerAttributeMode = 0;
    this->mPlayerAttributeAmount = 0;
    this->mLocationArea = 0;
    this->mLocationObject = 0;
    this->mTrigger = 0;
    this->mB18D = false;
    this->mIsClearText = true;
    this->mUsesLocationArea = true;
    this->mUsesPercent = false;
    this->mIsAddTechnology = true;
    this->mUsesPlaceArea = true;
    this->mFloatValue = 0.0f;
    this->mB193 = false;
    this->mAIAction = kSMAIActionModifyAIMilitaryTarget;
    this->mTargetArea = 0;
    this->mAIAttribute = kCPBehaviorNone;
    this->mIsOn = false;
    this->mIntValue1 = 0;
    this->mIntValue2 = 0;
    this->mObjects.clear();
    this->mULD0 = 8;
    this->mTargetObject = 0;
    this->mAIToggle = kSMAIToggleActive;
    this->mUsesTargetArea = false;
    this->mIsAttack = false;
    this->mUsesFavorites = false;
    this->mID = 0;
    this->mIsAddTarget = true;
}

#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEAIContinent.h"
#include "ee/EEAIRectangleArea.h"
#include "ee/EECPCombatManager.h"
#include "ee/EETown.h"
#include "ee/EECPUnitRequirement.h"
#include "ee/EECPCombatStrategy.h"
#include "ee/EECPAttackCommander.h"
#include "ee/EECPIntelligence.h"

// FUNCTION: EXE 0x5bb82f
// Has each chosen computer player raise a military group of the target object's unit kind (random min..max count)
// for its first town, else the largest continent, aimed at a random enemy
void EESMEffect::BuildObject()
{
    std::vector<long> theUnitIDs;
    std::vector<long> thePlayers;
    EECPCombatStrategy theStrategy;
    EECPUnitRequirement theRequirement;
    EECPILocation theLocation;
    EECPAGTask theTask;
    EEAIContinent* theContinent = NULL;
    EETown* theTown = NULL;
    bool theAnyCount = false;
    if (gWorld.mScenarioManager != NULL) {
        gWorld.mScenarioManager->ChoosePlayer(this->mPlayer, &thePlayers, true);
        for (long i = (long)thePlayers.size() - 1; i >= 0; --i) {
            EEComputerPlayer* thePlayer = dynamic_cast<EEComputerPlayer*>(gWorld.GetPlayer(*(thePlayers.begin() + i)));
            if (thePlayer == NULL)
                continue;
            // per player: no town, continent or "any count" carried over from the previous player
            theTown = NULL;
            theContinent = NULL;
            theAnyCount = false;
            if (thePlayer->mTowns.size() != 0)
                theTown = thePlayer->mTowns[0];
            if (theTown == NULL) {
                theContinent = gContinentManager.GetContinentByID((unsigned short)gContinentManager.GetLargestContinent(kContinentCategoryLand));
                if (theContinent == NULL) {
                    theContinent = gContinentManager.GetContinentByID((unsigned short)gContinentManager.GetLargestContinent(kContinentCategoryNone));
                    if (theContinent == NULL)
                        continue;
                }
            }
            EECPCombatManager* theCombat = thePlayer->mCombatManager;
            if (theTown != NULL) {
                theLocation.mSerial = theTown->mSerial;
                theLocation.mType = kAIAreaTown;
            } else {
                theLocation.mSerial = theContinent->mSerial;
                theLocation.mType = kAIAreaContinent;
            }
            theLocation.ResetToArea();
            theTask.mLocation = theLocation;
            EEPlayer* theTarget = theCombat->mIntelligence->GetRandomEnemy(thePlayer);
            theTask.mPlayer = theTarget != NULL ? gWorld.GetPlayer(theTarget->mPlayerID) : gWorld.GetPlayer(0);
            theTask.mType = kAGTaskLocation;
            theTask.mIsActive = true;
            theStrategy.Reset();
            theStrategy.mKind = 1;
            theStrategy.mAddFavorites = false;
            theRequirement.Clear();
            EESMObject* theObject = gWorld.mScenarioManager->GetScenarioObject(this->mTargetObject);
            if (theObject == NULL)
                continue;
            if (theObject->GetMinCount() == 0 && theObject->GetMaxCount() == 0) {
                theAnyCount = true;
                theStrategy.mNumUnits = 0;
                theStrategy.mNumRemaining = 0;
            } else {
                unsigned long theLow = min(theObject->GetMinCount(), theObject->GetMaxCount());
                unsigned long theHigh = max(theObject->GetMinCount(), theObject->GetMaxCount());
                long theCount = gPhysics->GetRandom((long)theLow, (long)theHigh);
                theRequirement.mNumLeft = theCount;
                theRequirement.mNumWanted = theCount;
                if (!theAnyCount) {
                    theStrategy.mNumUnits += theCount;
                    theStrategy.mNumRemaining = theStrategy.mNumUnits;
                }
            }
            bool theAddRequirement = true;
            if (theObject->mFamily == 0) {
                EEUnitClass* theClass = gUnitClassTable[theObject->mClass];
                if (theClass != NULL && theClass->mIsScenarioSelectable)
                    theRequirement.mUnitClass = theObject->mClass;
                else
                    theAddRequirement = false;
            } else if (theObject->mFamily < 0x10)
                theRequirement.mUnitClass = thePlayer->mFavoriteLandType;
            else if (theObject->mFamily < 0x4b)
                theRequirement.mFamily = theObject->mFamily - 0xf;
            else if (theObject->mFamily <= 0x74)
                theRequirement.mUnitKind = theObject->mFamily - 0x4a;
            else
                theAddRequirement = false;
            if (theAddRequirement) {
                theRequirement.mKind = kRequirementAlreadyDefined;
                theStrategy.mRequirements.push_back(theRequirement);
            }
            if (theCombat->IsValidTask(theTask, theStrategy, theUnitIDs))
                theCombat->mAttackCommander->CreateGroupFromStrategy(theTask, theStrategy, theUnitIDs);
        }
    }
}

// FUNCTION: EXE 0x5bbba1
// Has each chosen computer player form a military group from the listed objects' units (existing ones join, the rest
// are requested), aimed at the target area or the target object's unit; an attack or a defence group per mIsAttack
void EESMEffect::CreateAIMilitaryGroup()
{
    std::vector<long> theUnitIDs;
    std::vector<long> thePlayers;
    EECPCombatStrategy theStrategy;
    EECPUnitRequirement theRequirement;
    EECPILocation theLocation;
    EECPAGTask theTask;
    EEPlayer* theTargetPlayer = NULL;
    unsigned long theAreaSerial = 0;
    unsigned long theUnitID = 0;
    long theTotalUnits = 0;
    bool theAnyCount = false;
    bool theContinentArea = false;
    if (gWorld.mScenarioManager != NULL) {
        gWorld.mScenarioManager->ChoosePlayer(this->mPlayer, &thePlayers, true);
        for (long i = (long)thePlayers.size() - 1; i >= 0; --i) {
            EEComputerPlayer* thePlayer = dynamic_cast<EEComputerPlayer*>(gWorld.GetPlayer(thePlayers[i]));
            if (thePlayer == NULL)
                continue;
            EECPCombatManager* theCombat = thePlayer->mCombatManager;
            theAnyCount = false;
            theTask.mMode = kAGModeCombat;
            if (this->mUsesTargetArea) {
                EESMArea* theArea = gWorld.mScenarioManager->GetArea(this->mTargetArea);
                if (theArea != NULL && !theArea->IsUnset() && !theArea->IsOffMap()) {
                    for (long j = (long)gContinentManager.mRectangleAreas.size() - 1; j >= 0; --j) {
                        EEAIRectangleArea* theRectangle = gContinentManager.mRectangleAreas[j];
                        if (theRectangle != NULL && theRectangle->mScenarioAreaID == theArea->mID) {
                            theAreaSerial = theRectangle->mSerial;
                            break;
                        }
                    }
                    if (theAreaSerial == 0 && theArea->mAnchor.mX >= 0 && theArea->mAnchor.mY >= 0) {
                        EEAIContinent* theContinent = gContinentManager.GetContinentAt(theArea->mAnchor.mX, theArea->mAnchor.mY);
                        if (theContinent != NULL) {
                            theAreaSerial = theContinent->mSerial;
                            theContinentArea = true;
                        }
                    }
                }
            }
            if (!this->mUsesTargetArea) {
                EESMObject* theObject = gWorld.mScenarioManager->GetScenarioObject(this->mTargetObject);
                if (theObject != NULL && theObject->mUnits.size() != 0) {
                    theUnitID = *theObject->mUnits.begin();
                    for (long k = (long)theObject->mUnits.size() - 1; k >= 0; --k) {
                        EEComplexUnit* theUnit = dynamic_cast<EEComplexUnit*>(gWorld.GetUnit(theObject->mUnits[k]));
                        if (theUnit != NULL && theUnit->GetHitPoints() > 0) {
                            theUnitID = theUnit->GetUnitID();
                            theTask.SetUnit(theUnit);
                        }
                    }
                }
            }
            if (theAreaSerial != 0) {
                theLocation.mSerial = theAreaSerial;
                theLocation.mType = theContinentArea ? 1 : 8;
                theLocation.ResetToArea();
                theTask.mLocation = theLocation;
                theTargetPlayer = theCombat->mIntelligence->GetRandomEnemy(thePlayer);
            } else if (theUnitID != 0) {
                EEComplexUnit* theUnit = dynamic_cast<EEComplexUnit*>(gWorld.GetUnit(theUnitID));
                if (theUnit != NULL) {
                    theTargetPlayer = theUnit->mPlayer;
                    if (theUnit->mContainer != NULL)
                        theLocation.mTile = EETileLocation(theUnit->mContainer->GetTileX(), theUnit->mContainer->GetTileY());
                    else
                        theLocation.mTile = EETileLocation(theUnit->GetTileX(), theUnit->GetTileY());
                    theTask.mLocation = theLocation;
                } else
                    theTargetPlayer = theCombat->mIntelligence->GetRandomEnemy(thePlayer);
            }
            theTask.mPlayer = theTargetPlayer != NULL && theTargetPlayer != thePlayer
                           ? gWorld.GetPlayer(theTargetPlayer->mPlayerID) : gWorld.GetPlayer(0);
            theTask.mIsActive = true;
            theTask.mType = kAGTaskLocation;
            if (this->mIsAttack && theUnitID == 0)
                theTask.mType = kAGTaskTerritory;
            else if (this->mIsAttack && theTask.mUnit != NULL && theTask.mUnit->mPlayer != thePlayer
                     && thePlayer->mEnemies[theTask.mUnit->mPlayer->mPlayerID] != 0)
                theTask.mType = kAGTaskUnit;
            theStrategy.Reset();
            theStrategy.mKind = 5 - (this->mIsAttack != false);
            theStrategy.mAddFavorites = this->mUsesFavorites;
            for (long n = (long)this->mObjects.size() - 1; n >= 0; --n) {
                theRequirement.Clear();
                EESMObject* theObject = gWorld.mScenarioManager->GetScenarioObject(this->mObjects[n]);
                if (theObject == NULL)
                    continue;
                if (theObject->Evaluate(false) && theObject->mUnits.size() != 0) {
                    theTotalUnits += theObject->mUnits.size();
                    for (unsigned long u = 0; u < theObject->mUnits.size(); u++)
                        theUnitIDs.push_back(theObject->mUnits[u]);
                    continue;
                }
                if (theObject->mMinCount == 0 && theObject->mMaxCount == 0) {
                    theAnyCount = true;
                    theStrategy.mNumUnits = 0;
                    theStrategy.mNumRemaining = 0;
                } else {
                    unsigned long theLow = min(theObject->mMinCount, theObject->mMaxCount);
                    unsigned long theHigh = max(theObject->mMinCount, theObject->mMaxCount);
                    long theCount = gPhysics->GetRandom((long)theLow, (long)theHigh);
                    theRequirement.mNumLeft = theCount;
                    theRequirement.mNumWanted = theCount;
                    if (!theAnyCount) {
                        theStrategy.mNumUnits += theCount;
                        theStrategy.mNumRemaining = theStrategy.mNumUnits;
                    }
                }
                bool theAddRequirement = true;
                if (theObject->mFamily == 0) {
                    EEUnitClass* theClass = gUnitClassTable[theObject->mClass];
                    if (theClass != NULL && theClass->mIsScenarioSelectable)
                        theRequirement.mUnitClass = theObject->mClass;
                    else
                        theAddRequirement = false;
                } else if (theObject->mFamily < 0x10)
                    theRequirement.mUnitClass = thePlayer->mFavoriteLandType;
                else if (theObject->mFamily < 0x4b)
                    theRequirement.mFamily = theObject->mFamily - 0xf;
                else if (theObject->mFamily <= 0x74)
                    theRequirement.mUnitKind = theObject->mFamily - 0x4a;
                else
                    theAddRequirement = false;
                if (theAddRequirement) {
                    theRequirement.mKind = kRequirementAlreadyDefined;
                    theStrategy.mRequirements.push_back(theRequirement);
                }
            }
            for (long v = (long)theUnitIDs.size() - 1; v >= 0; --v) {
                EEUnit* theUnit = gWorld.GetUnit(theUnitIDs[v]);
                if (theUnit == NULL || theUnit->mComplexUnit == NULL || theUnit->mPlayer != thePlayer
                    || theUnit->GetHitPoints() < 1 || !theCombat->IsMilitaryUnit(theUnit->mComplexUnit))
                    theUnitIDs.erase(theUnitIDs.begin() + v);
                else if (theUnit->mComplexUnit->mIsDetachedFromAI)
                    theUnit->mComplexUnit->mIsDetachedFromAI = false;
            }
            if (theCombat->IsValidTask(theTask, theStrategy, theUnitIDs))
                theCombat->mAttackCommander->CreateGroupFromStrategy(theTask, theStrategy, theUnitIDs);
        }
    }
}

// FUNCTION: EXE 0x5bc333
// Adds or drops an attack or defend target (the target area's AI area or continent, or the target object's living unit)
// for each chosen computer player; a new target must pass the combat manager's IsValidTarget
void EESMEffect::ModifyAIMilitaryTarget()
{
    std::vector<unsigned long> theUnused;
    std::vector<long> thePlayers;
    unsigned long theSerial = 0;
    if (gWorld.mScenarioManager == NULL)
        return;
    gWorld.mScenarioManager->ChoosePlayer(this->mPlayer, &thePlayers, true);
    for (long i = (long)thePlayers.size() - 1; i >= 0; --i) {
        EEComputerPlayer* thePlayer = dynamic_cast<EEComputerPlayer*>(gWorld.GetPlayer(thePlayers[i]));
        if (thePlayer == NULL)
            continue;
        EESMArea* theArea = gWorld.mScenarioManager->GetArea(this->mTargetArea);
        EECPCombatManager* theCombat = thePlayer->mCombatManager;
        if (theArea != NULL && !theArea->IsUnset() && !theArea->IsOffMap()) {
            for (long j = (long)gContinentManager.mRectangleAreas.size() - 1; j >= 0; --j) {
                if (gContinentManager.mRectangleAreas[j] != NULL
                    && (gContinentManager.mRectangleAreas[j])->mScenarioAreaID == theArea->mID) {
                    theSerial = (gContinentManager.mRectangleAreas[j])->mSerial;
                    break;
                }
            }
            if (theSerial == 0 && theArea->mAnchor.mX >= 0 && theArea->mAnchor.mY >= 0) {
                EEAIContinent* theContinent = gContinentManager.GetContinentAt(theArea->mAnchor.mX, theArea->mAnchor.mY);
                if (theContinent != NULL)
                    theSerial = theContinent->mSerial;
            }
        }
        unsigned long theUnitID = (unsigned long)-1;
        EESMObject* theObject = gWorld.mScenarioManager->GetScenarioObject(this->mTargetObject);
        if (theObject != NULL && theObject->mUnits.size() > 0) {
            theUnitID = *theObject->mUnits.begin();
            for (long k = (long)theObject->mUnits.size() - 1; k >= 0; --k) {
                EEComplexUnit* theUnit = dynamic_cast<EEComplexUnit*>(gWorld.GetUnit(theObject->mUnits[k]));
                if (theUnit != NULL && theUnit->GetHitPoints() > 0)
                    theUnitID = theUnit->GetUnitID();
            }
        }
        if (this->mIsAddTarget && !theCombat->IsValidTarget(theSerial, theUnitID, this->GetIsAttack()))
            continue;
        if (this->mIsAttack) {
            if (this->mIsAddTarget)
                theCombat->AddAttackTask(theUnitID, theSerial);
            else if (theUnitID == (unsigned long)-1)
                theCombat->DropAreaTask(theSerial, false);
            else
                theCombat->DropUnitTask(theUnitID, true);
        } else {
            if (!this->mIsAddTarget) {
                if (theUnitID == (unsigned long)-1)
                    theCombat->DropAreaTask(theSerial, false);
                else
                    theCombat->DropUnitTask(theUnitID, true);
            } else
                theCombat->AddDefendTask(theUnitID, theSerial);
        }
    }
}

// FUNCTION: EXE 0x5bc60f
// Copies another effect's settings, texts and object list
void EESMEffect::Copy(const EESMEffect& inEffect)
{
    this->mName = inEffect.mName;
    this->mText = inEffect.mText;
    this->mMessage = inEffect.mMessage;
    this->mS64 = inEffect.mS64;
    this->mS78 = inEffect.mS78;
    this->mNewName = inEffect.mNewName;
    this->mFileName = inEffect.mFileName;
    this->mDescription = inEffect.mDescription;
    this->mPlayerAttribute = inEffect.mPlayerAttribute;
    this->mDiplomacy = inEffect.mDiplomacy;
    this->mDuration = inEffect.mDuration;
    this->mMessagePlayer = inEffect.mMessagePlayer;
    this->mNewOwner = inEffect.mNewOwner;
    this->mPlayer = inEffect.mPlayer;
    this->mDiplomacyPlayer = inEffect.mDiplomacyPlayer;
    this->mMediaAction = inEffect.mMediaAction;
    this->mULF4 = inEffect.mULF4;
    this->mPlayerAction = inEffect.mPlayerAction;
    this->mTriggerState = inEffect.mTriggerState;
    this->mType = inEffect.mType;
    this->mObjectAction = inEffect.mObjectAction;
    this->mTechnology = inEffect.mTechnology;
    this->mPlacedEffect = inEffect.mPlacedEffect;
    this->mPlaceArea = inEffect.mPlaceArea;
    this->mPlaceObject = inEffect.mPlaceObject;
    this->mDaylightHour = inEffect.mDaylightHour;
    this->mCycleTime = inEffect.mCycleTime;
    this->mGameAction = inEffect.mGameAction;
    this->mTextObject = inEffect.mTextObject;
    this->mAttribute = inEffect.mAttribute;
    this->mAttributeUnitType = inEffect.mAttributeUnitType;
    this->mAttributeUnit = inEffect.mAttributeUnit;
    this->mAttributePlayer = inEffect.mAttributePlayer;
    this->mAttributeMode = inEffect.mAttributeMode;
    this->mAttributeAmount = inEffect.mAttributeAmount;
    this->mObject = inEffect.mObject;
    this->mStance = inEffect.mStance;
    this->mTaskAction = inEffect.mTaskAction;
    this->mTaskArea = inEffect.mTaskArea;
    this->mTaskObject = inEffect.mTaskObject;
    this->mCivPoints = inEffect.mCivPoints;
    this->mPlayerAttributeMode = inEffect.mPlayerAttributeMode;
    this->mPlayerAttributeAmount = inEffect.mPlayerAttributeAmount;
    this->mLocationArea = inEffect.mLocationArea;
    this->mLocationObject = inEffect.mLocationObject;
    this->mTrigger = inEffect.mTrigger;
    this->mB18D = inEffect.mB18D;
    this->mIsClearText = inEffect.mIsClearText;
    this->mUsesLocationArea = inEffect.mUsesLocationArea;
    this->mUsesPercent = inEffect.mUsesPercent;
    this->mIsAddTechnology = inEffect.mIsAddTechnology;
    this->mUsesPlaceArea = inEffect.mUsesPlaceArea;
    this->mB193 = inEffect.mB193;
    this->mAIAction = inEffect.mAIAction;
    this->mTargetArea = inEffect.mTargetArea;
    this->mAIAttribute = inEffect.mAIAttribute;
    this->mIsOn = inEffect.mIsOn;
    this->mFloatValue = inEffect.GetFloatValue();
    this->mIntValue1 = inEffect.mIntValue1;
    this->mIntValue2 = inEffect.mIntValue2;
    this->mObjects = inEffect.mObjects;
    this->mULD0 = inEffect.mULD0;
    this->mTargetObject = inEffect.mTargetObject;
    this->mAIToggle = inEffect.mAIToggle;
    this->mUsesTargetArea = inEffect.mUsesTargetArea;
    this->mIsAddTarget = inEffect.mIsAddTarget;
    this->mIsAttack = inEffect.mIsAttack;
    this->mUsesFavorites = inEffect.mUsesFavorites;
    this->mID = inEffect.mID;
}

// FUNCTION: EXE 0x5bc942
// Gives the scenario object's living units the effect's new name (with no units, the class upgrade of a picked type
// for each owner player); then refreshes the main game panel
void EESMEffect::SetClassName()
{
    std::vector<long> thePlayers;
    EESMObject* theObject = gWorld.mScenarioManager->GetScenarioObject(this->mObject);
    if (theObject == NULL)
        return;
    if (theObject->GetIsSelectOnMap() || theObject->GetInAreaCheck() != 0 || theObject->GetHasAttributeCheck() != 0
        || theObject->GetNearCheck() != 0 || theObject->GetSelectedByCheck() != 0 || theObject->GetHasStateCheck() != 0
        || theObject->GetVisibleByCheck() != 0 || theObject->GetParent() != 0)
        theObject->Evaluate(0);
    unsigned long theNumUnits = theObject->mUnits.size();
    if (theNumUnits > 0) {
        for (unsigned long i = 0; i < theNumUnits; ++i) {
            EEUnit* theUnit = gWorld.GetUnit(theObject->GetUnitAt(i));
            if (theUnit != NULL && theUnit->GetHitPoints() > 0)
                theUnit->GetStats()->SetName(this->GetNewName());
        }
    } else {
        gWorld.mScenarioManager->ChoosePlayer(theObject->mOwner, &thePlayers, true);
        for (unsigned long i = 0; i < thePlayers.size(); ++i) {
            long theUnitClass = theObject->PickUnitType();
            if (theUnitClass >= 0) {
                EEUnitClass* theClass = gUnitClassTable[theUnitClass];
                gWorld.GetPlayer(thePlayers[i])->GetUpgrade(theClass->GetUnitType())->SetName(this->GetNewName());
            }
        }
    }
    if (gUIManager->GetMainGameForm() != NULL)
        gUIManager->GetMainGameForm()->mRefreshGroup = true;
}

// FUNCTION: EXE 0x5bcae9
// Returns the player's upgrade record for a unit type (EEPlayer::GetUpgrade)
// Inline EEPlayer::GetUpgrade(long), emitted by SetClassName; existing mUnitClassUpgrades vector at +0x884.

// FUNCTION: EXE 0x5bcaf9
// Returns the unit class's unit type (EEUnitClass::GetUnitType)
// Inline EEUnitClass::GetUnitType() const, emitted by SetClassName; mUnitType at +0x1bc.

// FUNCTION: EXE 0x5bcb00
// Sets the upgrade record's display name (EEUnitClassUpgrade::SetName)
// Inline EEUnitClassUpgrade::SetName(UWideString), emitted by SetClassName; by-value argument, mName at +0x4c.

// FUNCTION: EXE 0x5bcb1b
// Returns the unit's upgrade record (EEUnit::GetStats)
// Inline EEUnit::GetStats(), emitted by SetClassName; mStats at +0x34.

// FUNCTION: EXE 0x5bcb1f
// Sets, raises or lowers a player value: a resource, the population cap (also queued as a pending effect), difficulty,
// a scenario variable, a tribute or other statistic or the mining limit; any/all resources recurse per resource
void EESMEffect::SetPlayerAttribute(long inPlayer, unsigned long inAttribute, long inMode, long inAmount)
{
    EESMEffect theEffect;
    long theAmount = inAmount;
    if (inMode == kSMValueModeToDecreaseBy)
        theAmount = -theAmount;
    EEPlayer* thePlayer = gWorld.mPlayers[inPlayer];
    if (thePlayer == NULL)
        return;
    switch (inAttribute) {
    case kSMPlayerValueAnyResource:
        this->SetPlayerAttribute(inPlayer, gPhysics->GetRandom(2L, 6L), inMode, inAmount);
        break;
    case kSMPlayerValueAllResources: {
        for (unsigned long i = 2; i < 7; i++)
            this->SetPlayerAttribute(inPlayer, i, inMode, inAmount);
        break;
    }
    case kSMPlayerValueStone:
        if (inMode == kSMValueModeToExactly) thePlayer->mResourceCaps[kResourceStone] = theAmount;
        else thePlayer->mResourceCaps[kResourceStone] += theAmount;
        break;
    case kSMPlayerValueGold:
        if (inMode == kSMValueModeToExactly) thePlayer->mResourceCaps[kResourceGold] = theAmount;
        else thePlayer->mResourceCaps[kResourceGold] += theAmount;
        break;
    case kSMPlayerValueWood:
        if (inMode == kSMValueModeToExactly) thePlayer->mResourceCaps[kResourceWood] = theAmount;
        else thePlayer->mResourceCaps[kResourceWood] += theAmount;
        break;
    case kSMPlayerValueFood:
        if (inMode == kSMValueModeToExactly) thePlayer->mResourceCaps[kResourceFood] = theAmount;
        else thePlayer->mResourceCaps[kResourceFood] += theAmount;
        break;
    case kSMPlayerValueIron:
        if (inMode == kSMValueModeToExactly) thePlayer->mResourceCaps[kResourceIron] = theAmount;
        else thePlayer->mResourceCaps[kResourceIron] += theAmount;
        break;
    case kSMPlayerValueCurrentPop:
    case kSMPlayerValueUnitCount:
        break;
    case kSMPlayerValuePopCap: {
        long theLimit;
        if (inMode == kSMValueModeToExactly)
            theLimit = theAmount - gWorld.mPlayerPopulationLimit;
        else
            theLimit = thePlayer->mShareBonus + theAmount;
        thePlayer->mShareBonus = theLimit;
        theEffect.Copy(*this);
        theEffect.mPlayer = thePlayer->mPlayerID + 6;
        theEffect.mPlayerAttributeMode = 0;
        theEffect.mPlayerAttributeAmount = gWorld.mPlayerPopulationLimit + theLimit;
        gWorld.mScenarioManager->AddV134Effect(theEffect);
        break;
    }
    case kSMPlayerValueDifficultyLevel:
        if (thePlayer->mComputerPlayer != NULL) {
            if (inMode == kSMValueModeToExactly) {
                if (theAmount >= 3) theAmount = 2;
                thePlayer->mComputerPlayer->mDifficulty = theAmount;
            } else {
                theAmount += thePlayer->mComputerPlayer->mDifficulty;
                if (theAmount >= 3) theAmount = 2;
                if (theAmount < 0) theAmount = 0;
                thePlayer->mComputerPlayer->mDifficulty = theAmount;
            }
        }
        break;
    case kSMPlayerValueVariable0: case kSMPlayerValueVariable1: case kSMPlayerValueVariable2: case kSMPlayerValueVariable3: case kSMPlayerValueVariable4:
    case kSMPlayerValueVariable5: case kSMPlayerValueVariable6: case kSMPlayerValueVariable7: case kSMPlayerValueVariable8: case kSMPlayerValueVariable9:
        if (inMode == kSMValueModeToExactly)
            thePlayer->mScenarioVariables[inAttribute - 11] = theAmount;
        else
            thePlayer->mScenarioVariables[inAttribute - 11] += theAmount;
        break;
    case kSMPlayerValueStoneSent:
        if (inMode == kSMValueModeToExactly) { thePlayer->mSavedGroups.Reset(0x11, theAmount - (long)thePlayer->mTributes[2]); thePlayer->mTributes[2] = theAmount; }
        else thePlayer->AddTribute(0, 4, theAmount);
        break;
    case kSMPlayerValueStoneReceived:
        if (inMode == kSMValueModeToExactly) { thePlayer->mSavedGroups.Reset(0x12, theAmount - (long)thePlayer->mTributes[7]); thePlayer->mTributes[7] = theAmount; }
        else thePlayer->AddTribute(1, 4, theAmount);
        break;
    case kSMPlayerValueGoldSent:
        if (inMode == kSMValueModeToExactly) { thePlayer->mSavedGroups.Reset(0x11, theAmount - (long)thePlayer->mTributes[3]); thePlayer->mTributes[3] = theAmount; }
        else thePlayer->AddTribute(0, 5, theAmount);
        break;
    case kSMPlayerValueGoldReceived:
        if (inMode == kSMValueModeToExactly) { thePlayer->mSavedGroups.Reset(0x12, theAmount - (long)thePlayer->mTributes[8]); thePlayer->mTributes[8] = theAmount; }
        else thePlayer->AddTribute(1, 5, theAmount);
        break;
    case kSMPlayerValueWoodSent:
        if (inMode == kSMValueModeToExactly) { thePlayer->mSavedGroups.Reset(0x11, theAmount - (long)thePlayer->mTributes[1]); thePlayer->mTributes[1] = theAmount; }
        else thePlayer->AddTribute(0, 3, theAmount);
        break;
    case kSMPlayerValueWoodReceived:
        if (inMode == kSMValueModeToExactly) { thePlayer->mSavedGroups.Reset(0x12, theAmount - (long)thePlayer->mTributes[6]); thePlayer->mTributes[6] = theAmount; }
        else thePlayer->AddTribute(1, 3, theAmount);
        break;
    case kSMPlayerValueFoodSent:
        if (inMode == kSMValueModeToExactly) { thePlayer->mSavedGroups.Reset(0x11, theAmount - (long)thePlayer->mTributes[0]); thePlayer->mTributes[0] = theAmount; }
        else thePlayer->AddTribute(0, 2, theAmount);
        break;
    case kSMPlayerValueFoodReceived:
        if (inMode == kSMValueModeToExactly) { thePlayer->mSavedGroups.Reset(0x12, theAmount - (long)thePlayer->mTributes[5]); thePlayer->mTributes[5] = theAmount; }
        else thePlayer->AddTribute(1, 2, theAmount);
        break;
    case kSMPlayerValueIronSent:
        if (inMode == kSMValueModeToExactly) { thePlayer->mSavedGroups.Reset(0x11, theAmount - (long)thePlayer->mTributes[4]); thePlayer->mTributes[4] = theAmount; }
        else thePlayer->AddTribute(0, 6, theAmount);
        break;
    case kSMPlayerValueIronReceived:
        if (inMode == kSMValueModeToExactly) { thePlayer->mSavedGroups.Reset(0x12, theAmount - (long)thePlayer->mTributes[9]); thePlayer->mTributes[9] = theAmount; }
        else thePlayer->AddTribute(1, 6, theAmount);
        break;
    default:
        if (inAttribute - 30 < 31) {
            if (inMode == kSMValueModeToExactly)
                thePlayer->mSavedGroups.Set(inAttribute - 30, theAmount);
            else
                thePlayer->mSavedGroups.Reset(inAttribute - 30, theAmount);
        } else if (inAttribute - 61 == 0) {
            if (inMode == kSMValueModeToExactly) thePlayer->mMiningLimit = theAmount;
            else thePlayer->mMiningLimit += theAmount;
        }
        break;
    }
}


struct EESMUnitPriorityLess {
    bool operator()(EEUnit* inFirst, EEUnit* inSecond) const;
};

// Original no-/Og: frame0x38 and memory-resident inline getter temporaries. The pragma alone
// makes this natural comparator exact200/200B; normal-Og version is73B (A/B proof).
#pragma optimize("g", off)
// FUNCTION: EXE 0x5bce99
// Orders units for SetUnitsOwner: non-farms before farms, otherwise the higher unit id first
bool EESMUnitPriorityLess::operator()(EEUnit* inFirst, EEUnit* inSecond) const
{
    bool theResult;
    if (inFirst->GetUnitClass()->GetUnitType() == kUnitTypeFarm && inSecond->GetUnitClass()->GetUnitType() == kUnitTypeFarm)
        theResult = inFirst->GetUnitID() > inSecond->GetUnitID();
    else if (inFirst->GetUnitClass()->GetUnitType() == kUnitTypeFarm)
        theResult = false;
    else if (inSecond->GetUnitClass()->GetUnitType() == kUnitTypeFarm)
        theResult = true;
    else
        theResult = inFirst->GetUnitID() > inSecond->GetUnitID();
    return theResult;
}


#pragma optimize("g", on)


// FUNCTION: EXE 0x5bcf61
// Converts the scenario object's living units (farms last, newest first) to a player picked from the new-owner setting
// for each unit; nature (player 0) is never picked
void EESMEffect::SetUnitsOwner()
{
    std::vector<EEUnit*> theUnits;
    EEUnit* theUnit = NULL;
    EESMObject* theObject = gWorld.mScenarioManager->GetScenarioObject(this->mObject);
    theObject->Evaluate(0);
    unsigned long theCount = theObject->mUnits.size();
    for (unsigned long i = 0; i < theCount; i++) {
        theUnit = gWorld.GetUnit(theObject->mUnits[i]);
        if (theUnit != NULL && theUnit->GetHitPoints() > 0)
            theUnits.push_back(theUnit);
    }
    std::sort(theUnits.begin(), theUnits.end(), EESMUnitPriorityLess());
    theCount = theUnits.size();
    for (unsigned long j = 0; j < theCount; j++) {
        theUnit = theUnits[j];
        if (theUnit == NULL || !(theUnit->GetHitPoints() > 0))
            continue;
        long thePlayer = gWorld.mScenarioManager->ChoosePlayer(this->mNewOwner, NULL, true);
        if (thePlayer != -1 && thePlayer != 0)
            EEConvertUnit(static_cast<EEComplexUnit*>(theUnit), thePlayer, 0);
    }
}


#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "ee/EEMap.h"
#include "ee/EEUUUnitFunctor.h"
#include "ee/EEUnitUtilities.h"
#include "ee/EERandomMapClass.h"
#include "ee/EEMedia.h"
#include "ee/EESoundObject.h"

void EEChangeUnit(unsigned long, bool, EEUnit*, long, unsigned long, bool, long);

// FUNCTION: EXE 0x5bd099
// Creates units for the scenario object (a random min..max count per chosen owner) or copies of its existing ones,
// placed by the object's rules and finished; a newly created set becomes the object's units
void EESMEffect::CreateUnits()
{
    std::vector<EEUnit*> theWallPieces;
    std::vector<long> thePlayers;
    std::vector<unsigned long> theCreatedUnits;
    GE2DPoint theLocation;
    GEVector theDirection;
    EESMObject* theObject = gWorld.mScenarioManager->GetScenarioObject(this->mObject);
    bool theCreate = false;
    EEMap* theMap = gWorld.mMap;
    if (theObject == NULL)
        return;
    unsigned long theCount;
    if (!theObject->mIsSelectOnMap && (theObject->mUnits.size() <= 0 || theObject->mIsDynamic)) {
        theCreatedUnits.clear();
        theCount = gPhysics->GetRandom((long)theObject->GetMinCount(),(long)theObject->GetMaxCount());
        theCreate = true;
    } else
        theCount = theObject->mUnits.size();
    for (unsigned long i = 0; i < theCount; ++i) {
        thePlayers.clear();
        if (theCreate == true)
            gWorld.mScenarioManager->ChoosePlayer(theObject->GetOwner(),&thePlayers,true);
        else {
            for (unsigned long j = 0; j < theCount; ++j) {
                if (gWorld.GetUnit(theObject->mUnits[j]) == NULL)
                    continue;
                long thePlayerID = gWorld.GetUnit(theObject->mUnits[j])->mPlayer->mPlayerID;
                thePlayers.push_back(thePlayerID);
                break;
            }
        }
        for (unsigned long p = 0; p < thePlayers.size(); ++p) {
            long thePlayerID = thePlayers[p];
            EEPlayer* thePlayer = gWorld.GetPlayer(thePlayerID);
            if (thePlayer == NULL)
                continue;
            EEUnitClass* theClass;
            long theUnitType;
            if (theCreate == true) {
                theUnitType = theObject->PickUnitType();
                if (theUnitType < 0)
                    continue;
                theClass = gUnitClassTable[theUnitType];
            } else {
                EEUnit* theUnit = gWorld.GetUnit(theObject->mUnits[i]);
                if (theUnit == NULL || theUnit->GetHitPoints() <= 0)
                    continue;
                theClass = theUnit->GetUnitClass();
                theUnitType = theClass->GetUnitType();
            }
            if (theClass == NULL || !theObject->FindPlacement(theUnitType,theLocation,true))
                continue;
            EETechTreeNode* theNode = thePlayer->GetTechTree()->Get(theClass->GetTechnology());
            EEUnit* theUnit;
            if (theNode != NULL && thePlayer->GetTechTree()->Get(theClass->GetTechnology())->mData->mKind == 6) {
                theLocation.mX += 0.5f;
                theLocation.mY += 0.5f;
                EEPlanWall(theLocation, theLocation, theClass, thePlayer, false, 1, theWallPieces);
                theUnit = *(theWallPieces.begin());
                theWallPieces.clear();
            } else {
                theClass->IsLandBuilding();
                theUnit = theClass->CreateUnit((long)theLocation.GetX(),(long)theLocation.GetY(),thePlayerID,false,NULL);
            }
            if (theUnit->mTileNode != NULL)
                theUnit->GetComplexUnit()->SetBuilt(1,0,1);
            if ((theUnit->GetUnitClass()->mDomains & kMovementAir) != 0) {
                theUnit->GetComplexUnit()->SetFlightTime(theUnit->GetStats()->mFlightTime);
                theUnit->GetComplexUnit()->SetAmmunition(theUnit->GetUnitClass()->mAmmunition);
                theDirection.mX = (float)theMap->mSize * 0.5f - theLocation.GetX();
                theDirection.mY = theLocation.GetX() - theLocation.GetY();
                theDirection.mZ = 0.0f;
                theDirection.Normalize();
                float theHeading = UMArcTan2(-theDirection.GetX(),theDirection.GetY());
                GETransformation theTransformation(false);
                theTransformation = theUnit->GetComplexUnit()->mPhysicalModel.GetLocation();
                theTransformation.SetOrientationYPR(theHeading,0.0f,0.0f);
                theUnit->GetComplexUnit()->SetTransformation(theTransformation,true);
            }
            if (theUnit->mTileNode != NULL && theUnit->GetComplexUnit()->GetStats()->mPower > 0)
                theUnit->GetComplexUnit()->SetPower(theUnit->GetStats()->mPower);
            thePlayer->AddPendingUnits(false,true);
            gWorld.FlushQueuedPlacements();
            gWorld.mMap->PlaceUnitOnGround(theUnit);
            if (theObject->mHasAttributeCheck != 0)
                EEChangeUnit(theObject->mAttribute,theObject->mIsAttributePercent,theUnit,theObject->mAttributeMin,
                            theObject->mAttributeMax,theObject->mHasAttributeCheck == 2,kSMAttributeModeExactly);
            if (theCreate == true) {
                unsigned long theID = theUnit->GetUnitID();
                theCreatedUnits.push_back(theID);
            }
            EESoundObject* theSound = theUnit->GetStats()->mCreationSound;
            if (theSound != NULL && theUnit->GetPlayer()->mPlayerID == gWorld.mCurrentPlayer)
                gSoundManager->Play(theSound->mSampleID,theSound->mVolume,0.0f,1.0f,false,NULL);
        }
    }
    if (theCreate == true && theCreatedUnits.size() > 0)
        theObject->SetUnits(theCreatedUnits);
}


#include "ee/EEGraphicEffect.h"
#include "ee/EEEffectManager.h"
#include "ee/EEEnvironmentManager.h"
#include "ee/EEMiniMap.h"
#include "lle/UIFormManager.h"
#include "ee/EEUISCIFormManager.h"
#include "ee/EECalamityClass.h"

extern bool gInCinematic;                   // 0x5318e0 (EECameraControl.cpp's free-camera flag; provisional name)
extern char gU4E5B98[];                     // 0x4e5b98 (pooled "")

// Plain (non-folded) .rdata floats of this unit in the original (0x44608c, 0x446090, 0x446098), each read by one
// function; the pooled __real copies of the same values are separate (provisional names)
static const float kSMEHourFraction = 1.0f / 24.0f;
static const float kSMEOneDegree = 0.017453292f;
static const float kSMEZoomStep = 0.02f;

// FUNCTION: EXE 0x5bd5eb
// Runs the effect: an object action, a player action (victory, diplomacy, tech tree, camera, flare...), trigger state,
// media (sound, text, dialogue, chat), a game action (time of day, calamity, graphic effect, cinematic) or an AI action
void EESMEffect::Execute(bool inIsCinematic)
{
    unsigned long theSample;
    std::vector<EEUnit*> theUnits;
    EESMDialogMessage theMessage;
    GE3DPoint thePoint;
    U2DPoint<long> theTile;
    GEColor theColor;
    FSFileSpec theSpec;
    UString theString1;
    UString theString2;
    UString thePath;
    UWideString theMediaText;
    UWideString theName;
    UWideString theText;
    std::vector<U2DPoint<long> > theTiles;
    EEScenarioManager* theManager = gWorld.mScenarioManager;
    EESMObject* theUnitObject = theManager->GetScenarioObject(this->mPlaceObject);
    EESMObject* theObject = theManager->GetScenarioObject(this->mObject);
    long theType = 0;
    EESMArea* theArea = theManager->GetArea(this->mPlaceArea);
    std::vector<long> thePlayers;
    std::vector<long> theOthers;
    unsigned long i;
    switch (this->mType) {
    case kSMEffectObject:
        switch (this->mObjectAction) {
        case kSMObjectActionCreate:
            this->CreateUnits();
            break;
        case kSMObjectActionFreeze:
            this->FreezeUnits(true);
            break;
        case kSMObjectActionUnfreeze:
            this->FreezeUnits(false);
            break;
        case kSMObjectActionKill:
            this->KillUnits(true);
            break;
        case kSMObjectActionRemove:
            this->KillUnits(false);
            break;
        case kSMObjectActionTaskObject:
            this->TaskUnitsToObject();
            break;
        case kSMObjectActionTaskArea:
            this->TaskUnitsToArea();
            break;
        case kSMObjectActionStance:
            this->SetUnitsStance();
            break;
        case kSMObjectActionOwner:
            this->SetUnitsOwner();
            break;
        case kSMObjectActionUnitAttribute:
        case kSMObjectActionClassAttribute:
            this->ChangeAttribute();
            break;
        case kSMObjectActionClassName:
            this->SetClassName();
            break;
        case kSMObjectActionFaceObject:
            this->FaceObject();
            break;
        }
        break;
    case kSMEffectPlayer:
        theManager->ChoosePlayer(this->mPlayer, &thePlayers, true);
        for (i = 0; i < thePlayers.size(); i++) {
            switch (this->mPlayerAction) {
            case kSMPlayerActionScriptCamera:
                if (gWorld.mCurrentPlayer == thePlayers[i])
                    this->ScriptCamera();
                break;
            case kSMPlayerActionVictory:
            case kSMPlayerActionDefeat:
                if (thePlayers[i] != 0 && gWorld.GetPlayer(thePlayers[i]) != NULL) {
                    if (!gWorld.GetPlayer(thePlayers[i])->mObserverMode) {
                        gWorld.GetPlayer(thePlayers[i])->SetGameResult(this->GetPlayerAction() == kSMPlayerActionVictory ? 1 : 2);
                        gWorld.RemovePlayerFromGame(this->mPlayerAction == kSMPlayerActionVictory, thePlayers[i]);
                    }
                    if (gWorld.GetPlayer(thePlayers[i])->mComputerPlayer != NULL)
                        gWorld.GetPlayer(thePlayers[i])->mComputerPlayer->SetActive(false);
                }
                break;
            case kSMPlayerActionDiplomacy: {
                EEPlayer* thePlayer = gWorld.GetPlayer(thePlayers[i]);
                if (thePlayer != NULL) {
                    theManager->ChoosePlayer(this->mDiplomacyPlayer, &theOthers, true);
                    for (unsigned long j = 0; j < theOthers.size(); j++) {
                        if (theOthers[j] == thePlayers[i])
                            continue;
                        gWorld.SetDiplomaticStance(thePlayer->mPlayerID, theOthers[j], this->mDiplomacy);
                        if (this->mDiplomacy != 1)
                            thePlayer->NoticeTargetPlayer(gWorld.GetPlayer(theOthers[j]), false);
                    }
                }
                break;
            }
            case kSMPlayerActionFadeIn:
                if (thePlayers[i] == gWorld.mCurrentPlayer)
                    gGraphics->PostFadeIn(3000);
                break;
            case kSMPlayerActionFadeOut:
                if (gInCinematic && thePlayers[i] == gWorld.mCurrentPlayer)
                    gGraphics->PostFadeOut(3000);
                break;
            case kSMPlayerActionSetAttributeLevel:
                this->SetPlayerAttribute(thePlayers[i], this->mPlayerAttribute, this->mPlayerAttributeMode, this->mPlayerAttributeAmount);
                break;
            case kSMPlayerActionModifyTechTree: {
                EEPlayer* thePlayer = gWorld.GetPlayer(thePlayers[i]);
                if (thePlayer != NULL) {
                    if (this->mIsAddTechnology) {
                        if (this->mTechnology < kTechEpochAll) {
                            if (thePlayer->GetTechTree()->mEndEpoch < this->mTechnology)
                                thePlayer->GetTechTree()->ResetEpochRange(thePlayer->GetTechTree()->mStartEpoch, this->mTechnology);
                            thePlayer->GetTechTree()->Get(this->mTechnology)->MakeAvailable();
                        } else {
                            EETechTreeNode* theNode = thePlayer->GetTechTree()->Get(this->mTechnology);
                            theNode->MakeAvailable();
                            theNode->mEpoch = 0;
                            theNode->mObsoleteEpoch = kTechEpochAll;
                            theNode->SetScenEditorObsoleted(false, false);
                            theNode->SetObsolete(false, false);
                            if (theNode->NeedsUnitMedia() == true && !theNode->mIsUnitMediaLoaded) {
                                thePlayer->SetUnitClassMedia(theNode->mData->mUnitClass, 0x7f);
                                theNode->mIsUnitMediaLoaded = true;
                            }
                            if (theNode->mData->mKind == 4)
                                thePlayer->GetTechTree()->mEpochsBeforeWonders = 0;
                            thePlayer->GetTechTree()->Research(this->mTechnology, NULL);
                        }
                    } else {
                        if (thePlayer->GetTechTree()->Get(this->mTechnology)->mData->mMakesUnit) {
                            EEUnitClass* theClass = gUnitClassTable[thePlayer->GetTechTree()->Get(this->mTechnology)->mData->mUnitClass];
                            if (theClass != NULL)
                                EEReplaceQueuedTechnology(thePlayer, theClass, 100000);
                        }
                        thePlayer->GetTechTree()->MakeObsolete(this->mTechnology);
                    }
                    theManager->AddV134Effect(*this);
                }
                break;
            }
            case kSMPlayerActionVisibilityOn: {
                EEPlayer* thePlayer = gWorld.GetPlayer(thePlayers[i]);
                if (thePlayer != NULL) {
                    thePlayer->mSeesAll = true;
                    if (thePlayer == gWorld.GetPlayer(gWorld.mCurrentPlayer))
                        gMiniMap->Redraw();
                }
                break;
            }
            case kSMPlayerActionVisibilityOff: {
                EEPlayer* thePlayer = gWorld.GetPlayer(thePlayers[i]);
                if (thePlayer != NULL) {
                    thePlayer->mSeesAll = false;
                    if (thePlayer == gWorld.GetPlayer(gWorld.mCurrentPlayer))
                        gMiniMap->Redraw();
                }
                break;
            }
            case kSMPlayerActionAwardCivPoints: {
                EEPlayer* thePlayer = gWorld.GetPlayer(thePlayers[i]);
                if (thePlayer != NULL) {
                    thePlayer->mCivilization.AddPoints(this->mCivPoints);
                    EESetCivBuilderButtonBlinking(true);
                    EESoundObject* theSound = EESoundObject::Get(kSoundTrumpets);
                    if (theSound != NULL)
                        theSound->Play();
                }
                break;
            }
            case kSMPlayerActionMiniMapFlare: {
                EEPlayer* thePlayer = gWorld.GetPlayer(thePlayers[i]);
                if (thePlayer != NULL) {
                    RTSTile* theMapTile;
                    if (this->mUsesLocationArea) {
                        EESMArea* theEventArea = theManager->GetArea(this->mLocationArea);
                        if (theEventArea->IsOffMap() || theEventArea->IsUnset())
                            break;
                        theTile = theEventArea->GetCenterTile();
                        theMapTile = gWorld.mMap->GetTile(theTile.GetX(), theTile.GetY());
                    } else {
                        EESMObject* theEventObject = theManager->GetScenarioObject(this->mLocationObject);
                        theEventObject->Evaluate(false);
                        EEUnit* theUnit = NULL;
                        // own index: i is the loop over the chosen players
                        for (unsigned long theIndex = 0; theUnit == NULL && theIndex < theEventObject->GetUnitCount(); theIndex++)
                            theUnit = gWorld.GetUnit(theEventObject->mUnits[theIndex]);
                        if (theUnit == NULL)
                            break;
                        if (theUnit->mComplexUnit != NULL && theUnit->mComplexUnit->mContainer == NULL) {
                            theTile.mX = theUnit->mTileX;
                            theTile.mY = theUnit->mTileY;
                            theMapTile = theUnit->mTile;
                        } else {
                            theTile.mX = theUnit->mComplexUnit->mContainer->mTileX;
                            theTile.mY = theUnit->mComplexUnit->mContainer->mTileY;
                            theMapTile = theUnit->mComplexUnit->mContainer->mTile;
                        }
                    }
                    if (theMapTile != NULL)
                        thePlayer->mEvents.AddEvent(theTile, 3, thePlayer, -1, true);
                }
                break;
            }
            }
        }
        break;
    case kSMEffectTrigger: {
        EESMTrigger* theTrigger = theManager->GetTrigger(this->mTrigger);
        switch (this->mTriggerState) {
        case kSMTriggerStateOn:
            theTrigger->mConditionStartTime = 0;
            theTrigger->mIsOn = true;
            break;
        case kSMTriggerStateOff:
            theTrigger->mIsOn = false;
            break;
        case kSMTriggerStateFired:
            theTrigger->mHasFired = true;
            break;
        case kSMTriggerStateNotFired:
            theTrigger->mHasFired = false;
            break;
        }
        break;
    }
    case kSMEffectMedia:
        switch (this->mMediaAction) {
        case kSMMediaActionPlaySound: {
            if (gWorld.mGameOptions.mCampaignName.GetLength() > 0 && gWorld.mGameOptions.mCampaignChecksum > 0)
                thePath = gU52A280;
            else
                thePath = gU4E5B98;
            unsigned long theInstance;
            EELoadSound("Sounds\\" + thePath + UString(this->mFileName), 0x31, theSample, theInstance, 0);
            if (theInstance != -1)
                gSoundManager->RegisterOneShotSound(theSample, theInstance);
            break;
        }
        case kSMMediaActionChangeText: {
            EEUISCIFormManager* theForm = EEUISCIFormManager::Get();
            theMediaText = "\n";
            theMediaText += this->mDescription;
            switch (this->mTextObject) {
            case kSMMediaTextHints:
                if (this->mIsClearText == true)
                    theManager->mMediaStrings.mHints = UWideString(gU4E5B98);
                else
                    theManager->mMediaStrings.mHints = theManager->mMediaStrings.mHints + theMediaText;
                break;
            case kSMMediaTextInstructions:
                if (this->mIsClearText == true)
                    theManager->mMediaStrings.mInstructions = UWideString(gU4E5B98);
                else
                    theManager->mMediaStrings.mInstructions = theManager->mMediaStrings.mInstructions + theMediaText;
                break;
            case kSMMediaTextHistory:
                if (this->mIsClearText == true)
                    theManager->mMediaStrings.mHistory = UWideString(gU4E5B98);
                else
                    theManager->mMediaStrings.mHistory = theManager->mMediaStrings.mHistory + theMediaText;
                break;
            }
            EESoundObject* theSound = EESoundObject::Get(kSoundTrumpets);
            if (theSound != NULL)
                theSound->Play();
            theForm->SetBlinking(true);
            break;
        }
        case kSMMediaActionSendDialogue:
            this->ExpandTokens(this->mMessage, theText);
            theManager->ChoosePlayer(this->mMessagePlayer, &thePlayers, true);
            for (i = 0; i < thePlayers.size(); i++) {
                if (thePlayers[i] == gWorld.mCurrentPlayer) {
                    theMessage.mText = theName;
                    theMessage.mText += theText;
                    theMessage.mDuration = this->mDuration * 1000;
                    theMessage.mIsCinematic = inIsCinematic;
                    theObject->Evaluate(false);
                    if (theObject->mUnits.size() > 0 && gWorld.GetUnit(theObject->mUnits[0]) != NULL) {
                        EEUnit* theUnit = gWorld.GetUnit(theObject->mUnits[0]);
                        if (theUnit != NULL) {
                            theMessage.mUnitType = theUnit->mUnitClass->mUnitType;
                            theMessage.mScrollSpeed = this->mIntValue1;
                            theMessage.mUnitID = theUnit->mUnitID;
                        }
                    } else
                        theMessage.mUnitType = 0;
                    if (gWorld.mGameOptions.mCampaignName.GetLength() > 0 && gWorld.mGameOptions.mCampaignChecksum > 0)
                        thePath = gU52A280;
                    else
                        thePath = gU4E5B98;
                    theMessage.mSound = (const char*)(thePath + (const char*)this->mFileName);
                    gWorld.mScenarioManager->AddMessage(theMessage);
                } else
                    gUIManager->DisplayMessage(theMediaText, 1, NULL, NULL, true);
            }
            break;
        case kSMMediaActionSendChatMessage:
            this->ExpandTokens(this->mMessage, theText);
            theManager->ChoosePlayer(this->mMessagePlayer, &thePlayers, true);
            for (i = 0; i < thePlayers.size(); i++) {
                if (thePlayers[i] != gWorld.mCurrentPlayer)
                    continue;
                if (this->mDuration > 0)
                    gUserInterface->AddChatLogEntry(theColor, theName, theText, this->mDuration * 1000, 1);
                else {
                    // A chat message without a duration is shown in a message box: with its text (the box showed
                    // theMediaText, which only the Change Text action fills, so it was empty).
                    UWideString theChat = theName;
                    theChat += theText;
                    gUIManager->DisplayMessage(theChat, 1, NULL, NULL, true);
                }
            }
            break;
        }
        break;
    case kSMEffectGame:
        switch (this->mGameAction) {
        case kSMGameActionDaylightHour:
            gWorld.SetTimeOfDay((float)((this->mDaylightHour + 18) % 24) * kSMEHourFraction);
            break;
        case kSMGameActionDayPerNightCycleTime:
            if (this->mCycleTime > 0)
                gPhysics->mTimeOfDayRuns = true;
            else
                gPhysics->mTimeOfDayRuns = false;
            break;
        case kSMGameActionCalamity: {
            unsigned long theClassEnum = this->mPlacedEffect;
            if (theClassEnum == 0)
                theClassEnum = gPhysics->GetRandom(1L, 0x21L);
            EECalamityClass* theClass = USingleton<EECalamityManager>::GetInstance()->GetCalamityClass(
                (EECalamityEnum)theClassEnum);
            if (theClass == NULL)
                break;
            if (!USingleton<EECalamityManager>::GetInstance()->GetCalamityType((EECalamityEnum)theClassEnum, theType))
                break;
            switch (theType) {
            case 1:
                if (theClass->CanCastAt(NULL, thePoint, 0, 0, true, NULL) == 0)
                    USingleton<EECalamityManager>::GetInstance()->CreateGlobalCalamity(theClassEnum, 0, 0);
                break;
            case 2:
                if (this->mUsesPlaceArea && theArea != NULL) {
                    if (!theArea->IsUnset() && !theArea->IsOffMap()) {
                        theArea->GetTiles(theTiles);
                        EEAIRandomShuffle(theTiles.begin(), theTiles.end());
                        if (theTiles.size() > 0
                            && theClass->CanCastAt(NULL, thePoint, theTiles[0].GetX(), theTiles[0].GetY(), true, NULL) == 0)
                            USingleton<EECalamityManager>::GetInstance()->CreateLocationCalamity(theClassEnum, -1,
                                theTiles[0].GetX(), theTiles[0].GetY(), 0);
                    }
                } else if (!this->mUsesPlaceArea && theUnitObject != NULL && theUnitObject->Evaluate(false)) {
                    for (i = 0; i < theUnitObject->mUnits.size(); i++) {
                        EEUnit* theUnit = gWorld.GetUnit(theUnitObject->GetUnitAt(i));
                        if (theUnit != NULL
                            && theClass->CanCastAt(NULL, thePoint, theUnit->mTileX, theUnit->mTileY, true, theUnit) == 0)
                            USingleton<EECalamityManager>::GetInstance()->CreateLocationCalamity(theClassEnum, -1,
                                theUnit->mTileX, theUnit->mTileY, 0);
                    }
                }
                break;
            case 3:
                if (theUnitObject != NULL && !this->mUsesPlaceArea && theUnitObject->Evaluate(false)) {
                    for (i = 0; i < theUnitObject->mUnits.size(); i++) {
                        EEUnit* theUnit = gWorld.GetUnit(theUnitObject->mUnits[i]);
                        if (theUnit != NULL
                            && theClass->CanCastAt(NULL, thePoint, theUnit->mTileX, theUnit->mTileY, true, theUnit) == 0)
                            USingleton<EECalamityManager>::GetInstance()->CreateUnitCalamity(theClassEnum, -1,
                                theUnitObject->mUnits[i], 0);
                    }
                }
                if (this->mUsesPlaceArea && theArea != NULL && !theArea->IsUnset() && !theArea->IsOffMap()) {
                    theUnits.clear();
                    theArea->GetTiles(theTiles);
                    for (i = 0; i < theTiles.size(); i++) {
                        EEMapTile* theMapTile = gWorld.mMap->GetTile(theTiles[i].GetX(), theTiles[i].GetY());
                        if (theMapTile == NULL)
                            continue;
                        for (RTSTileUnitNode* theNode = theMapTile->mUnits; theNode != NULL; theNode = theNode->mNext) {
                            EEUnit* theUnit = theNode->mUnit;
                            if (theUnit == NULL || std::find(theUnits.begin(), theUnits.end(), theUnit) != theUnits.end()
                                || theClass->CanCastAt(NULL, thePoint, theTiles[0].GetX(), theTiles[0].GetY(), true, theUnit) != 0)
                                continue;
                            USingleton<EECalamityManager>::GetInstance()->CreateUnitCalamity(theClassEnum, -1,
                                theUnit->GetUnitID(), 0);
                            theUnits.push_back(theUnit);
                        }
                    }
                }
                break;
            }
            break;
        }
        case kSMGameActionGraphicEffect: {
            EEGraphicEffect* theEffect = EEGraphicEffect::Get(this->mPlacedEffect);
            theType = this->mUsesPlaceArea ? 2 : 3;
            if (theEffect == NULL)
                break;
            theEffect->Load(false);
            switch (theType) {
            case 2:
                if (this->mUsesPlaceArea && theArea != NULL && !theArea->IsUnset() && !theArea->IsOffMap()) {
                    theArea->GetTiles(theTiles);
                    if (theEffect->mKind == kEffect3DRain || theEffect->mKind == kEffect3DSnow)
                        gEffectManager->CreateWeatherEffect(theTiles, theEffect, NULL);
                    else {
                        EEAIRandomShuffle(theTiles.begin(), theTiles.end());
                        if (theTiles.size() > 0 && gWorld.mMap->GetTile(theTiles[0].GetX(), theTiles[0].GetY()) != NULL) {
                            thePoint.mX = (float)theTiles[0].GetX() + 0.5f;
                            thePoint.mY = (float)theTiles[0].GetY() + 0.5f;
                            thePoint.mZ = gWorld.mMap->GetTile(theTiles[0].GetX(), theTiles[0].GetY())->GetHeight();
                            long theTerrain = gWorld.mMap->GetTile(theTiles[0].GetX(), theTiles[0].GetY())->mTerrainType;
                            if (theTerrain == kTerrainDeepWater || theTerrain == kTerrainShallowWater)
                                thePoint.mZ = 0.0f;
                            gEffectManager->CreatePlacedEffect(thePoint, theEffect, NULL);
                        }
                    }
                }
                break;
            case 3:
                if (theUnitObject != NULL && !this->mUsesPlaceArea && theUnitObject->Evaluate(false)) {
                    for (i = 0; i < theUnitObject->mUnits.size(); i++) {
                        EEUnit* theUnit = gWorld.GetUnit(theUnitObject->mUnits[i]);
                        if (theUnit == NULL)
                            continue;
                        if ((theEffect->mKind == kEffect3DTracers || theEffect->mKind == kEffect3DShells || theEffect->mKind == kEffect2DWinterBreath
                             || theEffect->mKind == kEffect2DSmokeMuzzleBlast)
                            && theUnit->mComplexUnit != NULL && i + 1 < theUnitObject->mUnits.size()
                            && gWorld.GetUnit(theUnitObject->mUnits[i + 1]) != NULL)
                            gEffectManager->CreateEffectBetweenUnits(theUnit, gWorld.GetUnit(theUnitObject->mUnits[i + 1]),
                                                                     theEffect, NULL);
                        else if (theEffect->mKind == kEffect3DMarchingLine && i + 1 < theUnitObject->mUnits.size()
                                 && gWorld.GetUnit(theUnitObject->mUnits[i + 1]) != NULL)
                            gEffectManager->CreateEffectBetweenUnits(theUnit, gWorld.GetUnit(theUnitObject->mUnits[i + 1]),
                                                                     theEffect, NULL);
                        else
                            gEffectManager->CreateUnitEffect(theUnit, theEffect, NULL, false, 0, 0);
                    }
                }
                break;
            }
            theEffect->Unload();
            break;
        }
        case kSMGameActionStartCinematicMode: {
            theManager->StartCinematic();
            // the original rounds x * 0.1f and then adds 0.1f (fmul; fadd); the one-expression form is
            // refactored by VC6 into (x + 1.0f) * 0.1f, one rounding fewer
            float theSpeed = (float)this->mDaylightHour * 0.1f;
            theSpeed += 0.1f;
            gWorld.SetGameSpeed(theSpeed);
            break;
        }
        case kSMGameActionEndCinematicMode:
            if (!theManager->mIsShowingDialog && theManager->GetNumMessages() == 0)
                theManager->EndCinematic();
            else
                theManager->SetTimedEffectHasRun(this, false);
            break;
        case kSMGameActionAmbientWeather:
            if (gWorld.mEnvironmentManager != NULL)
                gWorld.mEnvironmentManager->SetEnabled(this->mIsOn);
            break;
        }
        break;
    case kSMEffectAI:
        switch (this->mAIAction) {
        case kSMAIActionModifyAIMilitaryTarget:
            this->ModifyAIMilitaryTarget();
            break;
        case kSMAIActionCreateAIMilitaryGroup:
            this->CreateAIMilitaryGroup();
            break;
        case kSMAIActionBuildObject:
            this->BuildObject();
            break;
        case kSMAIActionMaintainObject:
            this->MaintainObject();
            break;
        case kSMAIActionSetAIToggle:
            this->SetAIToggle();
            break;
        case kSMAIActionSetAIVariable:
            this->SetAIVariable();
            break;
        case kSMAIActionSetFavoriteUnits:
            this->SetFavoriteUnits();
            break;
        }
        break;
    }
}

// Does nothing (MaintainObject; empty: ICF-folded into 0xe123d with the other empty members)
void EESMEffect::MaintainObject()
{
}

// FUNCTION: EXE 0x5bef17
// Copies a message text, replacing each /#OA, /#PA or /#OC tag and its 4-digit id with an object's attribute value,
// a player condition's current value or an object's unit count
void EESMEffect::ExpandTokens(const UWideString& inText, UWideString& outText)
{
    std::vector<UWideString> theMarkers;
    std::vector<long> thePlayers;
    UWideString theNumber;
    UWideString theReplacement;
    EEScenarioManager* theManager = gWorld.mScenarioManager;
    theMarkers.push_back("/#OA");
    theMarkers.push_back("/#PA");
    theMarkers.push_back("/#OC");
    outText = inText;
    unsigned long theMarker = 0;
    while (theMarker < 5) {
        if (theMarkers.size() > theMarker && (theMarkers[theMarker]).GetLength() > 0) {
            if (outText.Find(theMarkers[theMarker], 0) < outText.GetLength() - 4) {
                unsigned long thePosition = outText.Find(theMarkers[theMarker], 0) +
                    (theMarkers[theMarker]).GetLength();
                theNumber = outText.GetIndexedSubstring(thePosition, thePosition + 4);
                long theIndex = atoi(theNumber);
                theReplacement.Clear();
                switch (theMarker) {
                case 0: {
                    EESMObject* theObject = theManager->FindObjectByID(theIndex);
                    if (theObject != NULL && theObject->mHasAttributeCheck != 0) {
                        theObject->Evaluate(0);
                        if (theObject->mUnits.size() > 0) {
                            long theValue = theObject->GetUnitAttribute(theObject->GetAttribute(), theObject->GetIsAttributePercent(),
                                gWorld.GetUnit(theObject->GetUnitAt(0)), 0, 0);
                            sprintf(theReplacement, "%d", theValue);
                        }
                    }
                    break;
                }
                case 1: {
                    EESMCondition* theCondition = theManager->FindConditionByID(theIndex);
                    if (theCondition != NULL && theCondition->mType == kSMConditionPlayerAttribute) {
                        theManager->ChoosePlayer(theCondition->mPlayer, &thePlayers, true);
                        if (thePlayers.size() > 0) {
                            long theValue = theCondition->GetPlayerValue(thePlayers.front(), theCondition->mAttribute);
                            sprintf(theReplacement, "%d", theValue);
                        }
                    }
                    break;
                }
                case 2: {
                    EESMObject* theObject = theManager->FindObjectByID(theIndex);
                    if (theObject != NULL) {
                        theObject->Evaluate(0);
                        sprintf(theReplacement, "%d", theObject->mUnits.size());
                    }
                    break;
                }
                }
                outText = outText.GetIndexedSubstring(0, thePosition - theMarkers[theMarker].GetLength() - 1) +
                    theReplacement + outText.GetIndexedSubstring(thePosition + 4, outText.GetLength());
                continue;
            }
        }
        ++theMarker;
    }
}


// FUNCTION: EXE 0x5bf2b3
// Follows an area move in the effect's area references (old and new index swap); on a delete (new index -1) references
// to it become 0 and later ones shift down
void EESMEffect::RenumberAreas(unsigned long inOldIndex, unsigned long inNewIndex)
{
    unsigned long theIndex;
    theIndex = this->mTaskArea;
    if (theIndex == inOldIndex) {
        this->mTaskArea = (inNewIndex != 0xffffffff) ? inNewIndex : 0;
    } else if (theIndex == inNewIndex) {
        this->mTaskArea = inOldIndex;
    } else if (inNewIndex == 0xffffffff && theIndex > inOldIndex) {
        this->mTaskArea = theIndex - 1;
    }
    theIndex = this->mTargetArea;
    if (theIndex == inOldIndex) {
        this->mTargetArea = (inNewIndex != 0xffffffff) ? inNewIndex : 0;
    } else if (theIndex == inNewIndex) {
        this->mTargetArea = inOldIndex;
    } else if (inNewIndex == 0xffffffff && theIndex > inOldIndex) {
        this->mTargetArea = theIndex - 1;
    }
    theIndex = this->mPlaceArea;
    if (theIndex == inOldIndex) {
        this->mPlaceArea = (inNewIndex != 0xffffffff) ? inNewIndex : 0;
    } else if (theIndex == inNewIndex) {
        this->mPlaceArea = inOldIndex;
    } else if (inNewIndex == 0xffffffff && theIndex > inOldIndex) {
        this->mPlaceArea = theIndex - 1;
    }
    theIndex = this->mLocationArea;
    if (theIndex == inOldIndex) {
        this->mLocationArea = (inNewIndex != 0xffffffff) ? inNewIndex : 0;
    } else if (theIndex == inNewIndex) {
        this->mLocationArea = inOldIndex;
    } else if (inNewIndex == 0xffffffff && theIndex > inOldIndex) {
        this->mLocationArea = theIndex - 1;
    }
}

// FUNCTION: EXE 0x5bf384
// Follows an object move or delete in the effect's object references and object list, like RenumberAreas
void EESMEffect::RenumberObjects(unsigned long inOldIndex, unsigned long inNewIndex)
{
    unsigned long theIndex;
    theIndex = this->mObject;
    if (theIndex == inOldIndex) {
        this->mObject = (inNewIndex != 0xffffffff) ? inNewIndex : 0;
    } else if (theIndex == inNewIndex) {
        this->mObject = inOldIndex;
    } else if (inNewIndex == 0xffffffff && theIndex > inOldIndex) {
        this->mObject = theIndex - 1;
    }
    theIndex = this->mLocationObject;
    if (theIndex == inOldIndex) {
        this->mLocationObject = (inNewIndex != 0xffffffff) ? inNewIndex : 0;
    } else if (theIndex == inNewIndex) {
        this->mLocationObject = inOldIndex;
    } else if (inNewIndex == 0xffffffff && theIndex > inOldIndex) {
        this->mLocationObject = theIndex - 1;
    }
    theIndex = this->mTaskObject;
    if (theIndex == inOldIndex) {
        this->mTaskObject = (inNewIndex != 0xffffffff) ? inNewIndex : 0;
    } else if (theIndex == inNewIndex) {
        this->mTaskObject = inOldIndex;
    } else if (inNewIndex == 0xffffffff && theIndex > inOldIndex) {
        this->mTaskObject = theIndex - 1;
    }
    theIndex = this->mPlaceObject;
    if (theIndex == inOldIndex) {
        this->mPlaceObject = (inNewIndex != 0xffffffff) ? inNewIndex : 0;
    } else if (theIndex == inNewIndex) {
        this->mPlaceObject = inOldIndex;
    } else if (inNewIndex == 0xffffffff && theIndex > inOldIndex) {
        this->mPlaceObject = theIndex - 1;
    }
    theIndex = this->mTargetObject;
    if (theIndex == inOldIndex) {
        this->mTargetObject = (inNewIndex != 0xffffffff) ? inNewIndex : 0;
    } else if (theIndex == inNewIndex) {
        this->mTargetObject = inOldIndex;
    } else if (inNewIndex == 0xffffffff && theIndex > inOldIndex) {
        this->mTargetObject = theIndex - 1;
    }
    for (unsigned long i = 0; i < this->mObjects.size(); i++) {
        unsigned long& theID = this->mObjects[i];
        theIndex = theID;
        if (theIndex == inOldIndex) {
            theID = (inNewIndex != 0xffffffff) ? inNewIndex : 0;
        } else if (theIndex == inNewIndex) {
            theID = inOldIndex;
        } else if (inNewIndex == 0xffffffff && theIndex > inOldIndex) {
            theID = theIndex - 1;
        }
    }
}

// FUNCTION: EXE 0x5bf4c9
// Follows a trigger move or delete in the effect's trigger reference, like RenumberAreas
void EESMEffect::RenumberTriggers(unsigned long inOldIndex, unsigned long inNewIndex)
{
    unsigned long theIndex = this->mTrigger;
    if (theIndex == inOldIndex) {
        this->mTrigger = (inNewIndex != 0xffffffff) ? inNewIndex : 0;
    } else if (theIndex == inNewIndex) {
        this->mTrigger = inOldIndex;
    } else if (inNewIndex == 0xffffffff && theIndex > inOldIndex) {
        this->mTrigger = theIndex - 1;
    }
}

// FUNCTION: EXE 0x5bf508
// Maps the effect's area references through an old-to-new id map
void EESMEffect::RemapAreas(EESMIDMap& inMap)
{
    EERemapID(this->mTaskArea, inMap);
    EERemapID(this->mTargetArea, inMap);
    EERemapID(this->mPlaceArea, inMap);
    EERemapID(this->mLocationArea, inMap);
}

// FUNCTION: EXE 0x5bf54c
// Maps the effect's object references and object list through an old-to-new id map
void EESMEffect::RemapObjects(EESMIDMap& inMap)
{
    EERemapID(this->mObject, inMap);
    EERemapID(this->mLocationObject, inMap);
    EERemapID(this->mTaskObject, inMap);
    EERemapID(this->mPlaceObject, inMap);
    EERemapID(this->mTargetObject, inMap);
    for (unsigned long i = 0; i < this->mObjects.size(); i++)
        EERemapID(this->mObjects[i], inMap);
}

// FUNCTION: EXE 0x5bf5c3
// Maps the effect's trigger reference through an old-to-new id map
void EESMEffect::RemapTriggers(EESMIDMap& inMap)
{
    EERemapID(this->mTrigger, inMap);
}

// FUNCTION: EXE 0x5bf5d8
// Turns the scenario object's idle living units to face mAttributeAmount degrees, at once (mUsesPercent) or with a turn
// goal; units already within one degree are left alone
void EESMEffect::FaceObject()
{
    EESMObject* theObject = gWorld.GetScenarioManager()->GetScenarioObject(this->mObject);
    theObject->Evaluate(0);
    unsigned long theCount = theObject->mUnits.size();
    for (unsigned long i = 0; i < theCount; i++) {
        EEComplexUnit* theUnit = dynamic_cast<EEComplexUnit*>(gWorld.GetUnit(theObject->mUnits[i]));
        if (theUnit == NULL || !(theUnit->GetHitPoints() > 0) ||
            (theUnit->GetGoal() != NULL && theUnit->GetCommandType() != kActionIdle) || !(theUnit->GetMaxAngularVelocity() > 0.0f))
            continue;
        GETransformation theLocation(false);
        theLocation = theUnit->mPhysicalModel.GetLocation();
        float theYaw = this->mAttributeAmount * 0.017453292f;
        if (!(UMAbs(theYaw - UMArcTan2(theLocation.GetElement(1, 0), theLocation.GetElement(0, 0))) >= kSMEOneDegree))
            continue;
        if (this->mUsesPercent) {
            theLocation.SetOrientationYPR(theYaw, 0.0f, 0.0f);
            theUnit->SetTransformation(theLocation, true);
            GE3DPoint thePosition(theLocation.GetX(), theLocation.GetY(), theLocation.GetZ());
            theUnit->PlaceOnGround(thePosition);
        } else {
            theUnit->ClearGoals(0);
            theUnit->SetGoal(new EEUGLocation(&GE3DPoint(theUnit->mPhysicalModel.GetLocation()),
                true, true, true, theYaw, false, false), 0, 0);
            theUnit->StartAction(kActionPrepareToMove);
            theUnit->GetUnitClass()->ActionChanged(theUnit);
        }
    }
}


// FUNCTION: EXE 0x5bf7f9
// Freezes the scenario object's living units (stopped, aircraft circle, gates close; disabled, not commandable or
// deletable) or unfreezes them
void EESMEffect::FreezeUnits(bool inFlag)
{
    EEScenarioManager* theManager = gWorld.mScenarioManager;
    unsigned long theObjectIndex = this->mObject;
    if (!(theObjectIndex < theManager->GetNumObjects()))
        return;
    EESMObject* theObject = theManager->mObjects[theObjectIndex];
    if (theObject == NULL)
        return;
    if (!theObject->mIsSelectOnMap && (theObject->mIsDynamic == true || theObject->mUnits.size() == 0))
        theObject->Evaluate(0);
    unsigned long theNumUnits = theObject->mUnits.size();
    for (unsigned long i = 0; i < theNumUnits; ++i) {
        EEComplexUnit* theUnit = dynamic_cast<EEComplexUnit*>(gWorld.GetUnit(theObject->mUnits[i]));
        if (theUnit == NULL || !(theUnit->GetHitPoints() > 0))
            continue;
        if (inFlag == true) {
            theUnit->ClearGoals(0);
            EEUnitClass* theClass = theUnit->GetUnitClass();
            if ((theClass->GetCategory() == kUnitCategoryAirplane || theClass->GetCategory() == kUnitCategoryPlanetSpaceship)
                && theUnit->GetCommandType() != kActionIdle) {
                theUnit->StartAction(kActionCircle);
                theUnit->GetUnitClass()->ActionChanged(theUnit);
            } else if (theClass->GetCategory() == kUnitCategoryGate) {
                EEUCGate* theGate = dynamic_cast<EEUCGate*>(theClass);
                theUnit->StartAction(kActionClosing);
                theUnit->GetUnitClass()->ActionChanged(theUnit);
            } else if (!theClass->mIsWall) {
                theUnit->StartAction(kActionIdle);
                theUnit->GetUnitClass()->ActionChanged(theUnit);
            }
            theUnit->SetIsDeletable(false);
        } else {
            theUnit->SetIsDeletable(theUnit->GetUnitClass()->mIsDeletable);
        }
        theUnit->SetDisabled(inFlag, 0);
        theUnit->SetIsCommandable(!inFlag);
    }
}

// FUNCTION: EXE 0x5bf9b7
// Kills (inFlag) or removes the scenario object's living units; off-map units are dropped from their player directly
void EESMEffect::KillUnits(bool inFlag)
{
    EEScenarioManager* theManager = gWorld.mScenarioManager;
    EESMObject* theObject = theManager->GetScenarioObject(this->mObject);
    if (theObject == NULL)
        return;
    if (!theObject->mIsSelectOnMap && (theObject->mIsDynamic == true || theObject->mUnits.size() == 0))
        theObject->Evaluate(0);
    unsigned long theNumUnits = theObject->mUnits.size();
    for (unsigned long i = 0; i < theNumUnits; ++i) {
        EEUnit* theUnit = gWorld.GetUnit(theObject->GetUnitAt(i));
        if (theUnit == NULL || !(theUnit->GetHitPoints() > 0))
            continue;
        theUnit->Kill(inFlag, false, false);
        if (theUnit->mTileNode == NULL)
            theUnit->GetPlayer()->KillUnit(theUnit, false);
    }
}

// FUNCTION: EXE 0x5bfa67
// Reads the effect from a scenario file, renumbering player actions and unit attributes of older files; returns the
// first read error
long EESMEffect::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    UWideString theString;
    unsigned long theCount;
    long theResult = this->mName.Load(inFile);
    if (theResult == kErrorOK)
        theResult = this->mText.Load(inFile);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mDuration);
    if (theResult == kErrorOK)
        theResult = this->mMessage.Load(inFile);
    if (theResult == kErrorOK)
        theResult = this->mS64.Load(inFile);
    if (theResult == kErrorOK)
        theResult = this->mS78.Load(inFile);
    if (theResult == kErrorOK)
        theResult = this->mNewName.Load(inFile);
    if (theResult == kErrorOK)
        theResult = this->mFileName.Load(inFile);
    if (theResult == kErrorOK)
        theResult = this->mDescription.Load(inFile);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mPlayerAttribute);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mDiplomacy);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mMessagePlayer);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mNewOwner);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mPlayer);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mDiplomacyPlayer);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mMediaAction);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mULF4);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mPlayerAction);
    if (this->mPlayerAction >= kSMPlayerActionTotal)
        this->mPlayerAction -= 2;
    if (this->mPlayerAction >= kSMPlayerActionTotal)
        this->mPlayerAction = kSMPlayerActionVictory;
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mTriggerState);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mType);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mObjectAction);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mTechnology);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mPlacedEffect);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mPlaceArea);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mPlaceObject);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mDaylightHour);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mCycleTime);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mGameAction);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mTextObject);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mAttribute);
    if (theResult == kErrorOK && inMajorVersion <= 0 && inMinorVersion < 0xcd && this->mAttribute > kSMUnitAttributeConstructionComplete)
        this->mAttribute += 3;
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mAttributePlayer);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mAttributeUnitType);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mAttributeUnit);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mAttributeMode);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mAttributeAmount);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mObject);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mStance);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mTaskAction);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mTaskArea);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mTaskObject);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mCivPoints);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mPlayerAttributeMode);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mPlayerAttributeAmount);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mLocationArea);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mLocationObject);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mTrigger);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mB18D);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mIsClearText);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mUsesLocationArea);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mUsesPercent);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mIsAddTechnology);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mUsesPlaceArea);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mB193);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mAIAction);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mTargetArea);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mAIAttribute);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mIsOn);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mFloatValue);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mIntValue1);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mIntValue2);
    if (theResult == kErrorOK) {
        theResult = inFile.Read(theCount);
        if (theResult == kErrorOK) {
            this->mObjects.clear();
            for (unsigned long i = 0; i < theCount; i++) {
                unsigned long theValue;
                theResult = inFile.Read(theValue);
                if (theResult != kErrorOK)
                    break;
                this->mObjects.push_back(theValue);
            }
        }
    }
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mULD0);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mTargetObject);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mAIToggle);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mUsesTargetArea);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mIsAddTarget);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mID);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mIsAttack);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mUsesFavorites);
    return theResult;
}

// FUNCTION: EXE 0x5c0134
// Writes the effect to a scenario file (civ points from minor version 0x2a, mB193 from 0x14); returns the first write
// error
long EESMEffect::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    unsigned long theCount;
    long theResult = this->mName.Save(outFile);
    if (theResult == kErrorOK)
        theResult = this->mText.Save(outFile);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mDuration);
    if (theResult == kErrorOK)
        theResult = this->mMessage.Save(outFile);
    if (theResult == kErrorOK)
        theResult = this->mS64.Save(outFile);
    if (theResult == kErrorOK)
        theResult = this->mS78.Save(outFile);
    if (theResult == kErrorOK)
        theResult = this->mNewName.Save(outFile);
    if (theResult == kErrorOK)
        theResult = this->mFileName.Save(outFile);
    if (theResult == kErrorOK)
        theResult = this->mDescription.Save(outFile);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mPlayerAttribute);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mDiplomacy);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mMessagePlayer);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mNewOwner);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mPlayer);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mDiplomacyPlayer);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mMediaAction);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mULF4);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mPlayerAction);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mTriggerState);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mType);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mObjectAction);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mTechnology);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mPlacedEffect);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mPlaceArea);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mPlaceObject);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mDaylightHour);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mCycleTime);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mGameAction);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mTextObject);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mAttribute);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mAttributePlayer);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mAttributeUnitType);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mAttributeUnit);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mAttributeMode);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mAttributeAmount);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mObject);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mStance);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mTaskAction);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mTaskArea);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mTaskObject);
    if (theResult == kErrorOK && (inMajorVersion > 0 || inMinorVersion >= 0x2a))
        theResult = outFile.Write(this->mCivPoints);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mPlayerAttributeMode);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mPlayerAttributeAmount);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mLocationArea);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mLocationObject);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mTrigger);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mB18D);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mIsClearText);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mUsesLocationArea);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mUsesPercent);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mIsAddTechnology);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mUsesPlaceArea);
    if (theResult == kErrorOK && (inMajorVersion > 0 || inMinorVersion >= 0x14))
        theResult = outFile.Write(this->mB193);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mAIAction);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mTargetArea);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mAIAttribute);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mIsOn);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mFloatValue);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mIntValue1);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mIntValue2);
    if (theResult == kErrorOK) {
        theCount = this->mObjects.size();
        theResult = outFile.Write(theCount);
        if (theResult == kErrorOK) {
            for (unsigned long i = 0; i < theCount; i++) {
                theResult = outFile.Write(this->mObjects[i]);
                if (theResult != kErrorOK)
                    break;
            }
        }
    }
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mULD0);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mTargetObject);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mAIToggle);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mUsesTargetArea);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mIsAddTarget);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mID);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mIsAttack);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mUsesFavorites);
    return theResult;
}

// FUNCTION: EXE 0x5c07c4
// Starts a scripted camera move: along an object's camera-object units, else from the start area or unit to
// the end area or unit over mIntValue1, at zoom level mIntValue2
void EESMEffect::ScriptCamera()
{
    U2DPoint<long> theTile;
    std::vector<unsigned long> theUnits;
    EESMObject* theStartObject = gWorld.mScenarioManager->GetScenarioObject(this->mTargetObject);
    EESMObject* theEndObject = gWorld.mScenarioManager->GetScenarioObject(this->mLocationObject);
    EESMArea* theStartArea = gWorld.mScenarioManager->GetArea(this->mTargetArea);
    EESMArea* theEndArea = gWorld.mScenarioManager->GetArea(this->mLocationArea);
    EEUnit* theEndUnit = NULL;
    EEUnit* theStartUnit = NULL;
    GE3DPoint theEndPoint(-10000.0f,0.0f,0.0f);
    GE3DPoint theStartPoint(-10000.0f,0.0f,0.0f);
    long theDuration = this->mIntValue1;
    long theZoom = this->mIntValue2;
    bool theStartIsArea = this->mUsesTargetArea;
    bool theEndFlag = this->mIsAttack;
    bool theStartFlag = this->mUsesFavorites;
    EECameraControl& theCamera = gWorld.mCameraControl;
    theCamera.ResetPath();
    if (!theStartIsArea && theStartObject != NULL && this->mTargetObject != 0 && !this->mUsesLocationArea
        && theEndObject != NULL && this->mLocationObject != 0 && this->mLocationObject == this->mTargetObject) {
        bool theAllCameraPoints = true;
        theEndObject->Evaluate(false);
        theEndObject->GetUnits(theUnits);
        for (unsigned long i = 0; i < theUnits.size(); ++i) {
            EEUnit* theUnit = gWorld.GetUnit(theUnits[i]);
            if (theUnit == NULL || theUnit->mUnitClass->mUnitType != kUnitTypeCameraObject)
                theAllCameraPoints = false;
        }
        if (theAllCameraPoints) {
            theCamera.mTurnYaw = 0.0f;
            if (theZoom != 0) {
                float theLevel = (float)(theZoom - 1) * kSMEZoomStep - 1.0f;
                theCamera.SetMode(3);
                theCamera.SetZoom(theLevel > 0.0f ? theLevel : 0.0f);
            }
            for (unsigned long j = 0; j < theUnits.size(); ++j) {
                long theStepDuration = theUnits.size() == 1 || j != 0 ? theDuration : 0;
                // These camera APIs consume the original base-unit pointer, including its optional complex-unit link.
                EEUnit* theUnit = gWorld.GetUnit(theUnits[j]);
                EECameraKey theKey;
                theKey.mOffset = theCamera.mEndOffset;
                theCamera.GetUnitPoint(theUnit,theKey.mFocus);
                theCamera.GetUnitTurn(theUnit,theKey.mT40);
                theKey.mWait = 0.0f;
                theKey.mChanged = theStepDuration == 0;
                theKey.mRate = theStepDuration == 0 ? 0.0f : 1.0f / (float)theStepDuration;
                theCamera.AddKey(theKey);
            }
            theCamera.StartPath();
            return;
        }
    }
    if (this->mUsesLocationArea) {
        if (!theEndArea->IsUnset() && !theEndArea->IsOffMap()) {
            theTile = theEndArea->GetCenterTile();
            EEMapTile* theMapTile = gWorld.mMap->GetTile(theTile.mX,theTile.mY);
            if (theMapTile != NULL) {
                theEndPoint.mX = (float)theTile.mX + 0.5f;
                theEndPoint.mY = (float)theTile.mY + 0.5f;
                theEndPoint.mZ = theMapTile->mHeight;
            }
        }
    } else if (theEndObject != NULL && this->mLocationObject != 0) {
        theEndObject->Evaluate(false);
        if (theEndObject->mUnits.size() != 0)
            theEndUnit = gWorld.GetUnit(*(theEndObject->mUnits.begin()));
    }
    if (!theStartIsArea) {
        if (theStartObject != NULL && this->mTargetObject != 0) {
            theStartObject->Evaluate(false);
            if (theStartObject->mUnits.size() != 0)
                theStartUnit = gWorld.GetUnit(*(theStartObject->mUnits.begin()));
        }
    } else if (!theStartArea->IsUnset() && !theStartArea->IsOffMap()) {
        theTile = theStartArea->GetCenterTile();
        EEMapTile* theMapTile = gWorld.mMap->GetTile(theTile.mX,theTile.mY);
        if (theMapTile != NULL) {
            theStartPoint.mX = (float)theTile.mX + 0.5f;
            theStartPoint.mY = (float)theTile.mY + 0.5f;
            theStartPoint.mZ = theMapTile->mHeight;
        }
    }
    float theLevel;
    if (theZoom == 0)
        theLevel = -1.1f;
    else {
        --theZoom;
        theLevel = (float)theZoom * kSMEZoomStep - 1.0f;
        theCamera.SetMode(3);
    }
    theCamera.MoveTo(theEndUnit,theEndPoint,theEndFlag,(float)theDuration,
                        theStartUnit,theStartPoint,theStartFlag,theLevel,0.0f,0.0f);
}


#include "ee/EECPCivilizationManager.h"

// EEComputerPlayer's scenario tuning setters (declared in ee/EEComputerPlayer.h): store the value, then mirror it
// into the civilization manager's map under the unit type. The local manager pointer makes all 34 exact (the
// original reloads ecx from +0xbc4 after the store). Emitted after SetAIVariable (0x1c1ca9..0x1c24c8).
inline void EEComputerPlayer::SetAntiAirMax(long inValue)
{
    this->mAntiAirMax = inValue;
    EECPCivilizationManager* theManager = this->mCivilizationManager;
    if (theManager != NULL)
        theManager->mMaxCounts[kUnitTypeAA10Gun88mm] = inValue;
}
inline void EEComputerPlayer::SetAirportMax(long inValue)
{
    this->mAirportMax = inValue;
    EECPCivilizationManager* theManager = this->mCivilizationManager;
    if (theManager != NULL)
        theManager->mMaxCounts[kUnitTypeAirport] = inValue;
}
inline void EEComputerPlayer::SetArcheryRangeMax(long inValue)
{
    this->mArcheryRangeMax = inValue;
    EECPCivilizationManager* theManager = this->mCivilizationManager;
    if (theManager != NULL)
        theManager->mMaxCounts[kUnitTypeArcheryRange] = inValue;
}
inline void EEComputerPlayer::SetBarracksMax(long inValue)
{
    this->mBarracksMax = inValue;
    EECPCivilizationManager* theManager = this->mCivilizationManager;
    if (theManager != NULL)
        theManager->mMaxCounts[kUnitTypeBarracks] = inValue;
}
inline void EEComputerPlayer::SetDockMax(long inValue)
{
    this->mDockMax = inValue;
    EECPCivilizationManager* theManager = this->mCivilizationManager;
    if (theManager != NULL)
        theManager->mMaxCounts[kUnitTypeDock] = inValue;
}
inline void EEComputerPlayer::SetGranaryMax(long inValue)
{
    this->mGranaryMax = inValue;
    EECPCivilizationManager* theManager = this->mCivilizationManager;
    if (theManager != NULL)
        theManager->mMaxCounts[kUnitTypeFarm] = inValue;
}
inline void EEComputerPlayer::SetHospitalMax(long inValue)
{
    this->mHospitalMax = inValue;
    EECPCivilizationManager* theManager = this->mCivilizationManager;
    if (theManager != NULL)
        theManager->mMaxCounts[kUnitTypeHospital] = inValue;
}
inline void EEComputerPlayer::SetCyberFactoryMax(long inValue)
{
    this->mCyberFactoryMax = inValue;
    EECPCivilizationManager* theManager = this->mCivilizationManager;
    if (theManager != NULL)
        theManager->mMaxCounts[kUnitTypeCyberFactory] = inValue;
}
inline void EEComputerPlayer::SetCyberLabMax(long inValue)
{
    this->mCyberLabMax = inValue;
    EECPCivilizationManager* theManager = this->mCivilizationManager;
    if (theManager != NULL)
        theManager->mMaxCounts[kUnitTypeCyberLaboratory] = inValue;
}
inline void EEComputerPlayer::SetNavalYardMax(long inValue)
{
    this->mNavalYardMax = inValue;
    EECPCivilizationManager* theManager = this->mCivilizationManager;
    if (theManager != NULL)
        theManager->mMaxCounts[kUnitTypeNavyYard] = inValue;
}
inline void EEComputerPlayer::SetSiegeFactoryMax(long inValue)
{
    this->mSiegeFactoryMax = inValue;
    EECPCivilizationManager* theManager = this->mCivilizationManager;
    if (theManager != NULL)
        theManager->mMaxCounts[kUnitTypeSiegeFactory] = inValue;
}
inline void EEComputerPlayer::SetSpaceDockMax(long inValue)
{
    this->mSpaceDockMax = inValue;
    EECPCivilizationManager* theManager = this->mCivilizationManager;
    if (theManager != NULL)
        theManager->mMaxCounts[kUnitTypeSpaceDock] = inValue;
}
inline void EEComputerPlayer::SetStableMax(long inValue)
{
    this->mStableMax = inValue;
    EECPCivilizationManager* theManager = this->mCivilizationManager;
    if (theManager != NULL)
        theManager->mMaxCounts[kUnitTypeStable] = inValue;
}
inline void EEComputerPlayer::SetTankFactoryMax(long inValue)
{
    this->mTankFactoryMax = inValue;
    EECPCivilizationManager* theManager = this->mCivilizationManager;
    if (theManager != NULL)
        theManager->mMaxCounts[kUnitTypeTankFactory] = inValue;
}
inline void EEComputerPlayer::SetTempleMax(long inValue)
{
    this->mTempleMax = inValue;
    EECPCivilizationManager* theManager = this->mCivilizationManager;
    if (theManager != NULL)
        theManager->mMaxCounts[kUnitTypeTemple] = inValue;
}
inline void EEComputerPlayer::SetTowerMax(long inValue)
{
    this->mTowerMax = inValue;
    EECPCivilizationManager* theManager = this->mCivilizationManager;
    if (theManager != NULL)
        theManager->mMaxCounts[kUnitTypeGuardTowerPaleo] = inValue;
}
inline void EEComputerPlayer::SetUniversityMax(long inValue)
{
    this->mUniversityMax = inValue;
    EECPCivilizationManager* theManager = this->mCivilizationManager;
    if (theManager != NULL)
        theManager->mMaxCounts[kUnitTypeUniversity] = inValue;
}
inline void EEComputerPlayer::SetUnitsPerAntiAirBuilding(long inValue)
{
    this->mUnitsPerAntiAirBuilding = inValue;
    EECPCivilizationManager* theManager = this->mCivilizationManager;
    if (theManager != NULL)
        theManager->mUnitsPer[kUnitTypeAA10Gun88mm] = inValue;
}
inline void EEComputerPlayer::SetUnitsPerAirport(long inValue)
{
    this->mUnitsPerAirport = inValue;
    EECPCivilizationManager* theManager = this->mCivilizationManager;
    if (theManager != NULL)
        theManager->mUnitsPer[kUnitTypeAirport] = inValue;
}
inline void EEComputerPlayer::SetUnitsPerArcheryRange(long inValue)
{
    this->mUnitsPerArcheryRange = inValue;
    EECPCivilizationManager* theManager = this->mCivilizationManager;
    if (theManager != NULL)
        theManager->mUnitsPer[kUnitTypeArcheryRange] = inValue;
}
inline void EEComputerPlayer::SetUnitsPerBarracks(long inValue)
{
    this->mUnitsPerBarracks = inValue;
    EECPCivilizationManager* theManager = this->mCivilizationManager;
    if (theManager != NULL)
        theManager->mUnitsPer[kUnitTypeBarracks] = inValue;
}
inline void EEComputerPlayer::SetUnitsPerDock(long inValue)
{
    this->mUnitsPerDock = inValue;
    EECPCivilizationManager* theManager = this->mCivilizationManager;
    if (theManager != NULL)
        theManager->mUnitsPer[kUnitTypeDock] = inValue;
}
inline void EEComputerPlayer::SetUnitsPerGranary(long inValue)
{
    this->mUnitsPerGranary = inValue;
    EECPCivilizationManager* theManager = this->mCivilizationManager;
    if (theManager != NULL)
        theManager->mUnitsPer[kUnitTypeFarm] = inValue;
}
inline void EEComputerPlayer::SetUnitsPerHospital(long inValue)
{
    this->mUnitsPerHospital = inValue;
    EECPCivilizationManager* theManager = this->mCivilizationManager;
    if (theManager != NULL)
        theManager->mUnitsPer[kUnitTypeHospital] = inValue;
}
inline void EEComputerPlayer::SetUnitsPerCyberFactory(long inValue)
{
    this->mUnitsPerCyberFactory = inValue;
    EECPCivilizationManager* theManager = this->mCivilizationManager;
    if (theManager != NULL)
        theManager->mUnitsPer[kUnitTypeCyberFactory] = inValue;
}
inline void EEComputerPlayer::SetUnitsPerCyberLab(long inValue)
{
    this->mUnitsPerCyberLab = inValue;
    EECPCivilizationManager* theManager = this->mCivilizationManager;
    if (theManager != NULL)
        theManager->mUnitsPer[kUnitTypeCyberLaboratory] = inValue;
}
inline void EEComputerPlayer::SetUnitsPerNavalYard(long inValue)
{
    this->mUnitsPerNavalYard = inValue;
    EECPCivilizationManager* theManager = this->mCivilizationManager;
    if (theManager != NULL)
        theManager->mUnitsPer[kUnitTypeNavyYard] = inValue;
}
inline void EEComputerPlayer::SetUnitsPerSiegeFactory(long inValue)
{
    this->mUnitsPerSiegeFactory = inValue;
    EECPCivilizationManager* theManager = this->mCivilizationManager;
    if (theManager != NULL)
        theManager->mUnitsPer[kUnitTypeSiegeFactory] = inValue;
}
inline void EEComputerPlayer::SetUnitsPerSpaceDock(long inValue)
{
    this->mUnitsPerSpaceDock = inValue;
    EECPCivilizationManager* theManager = this->mCivilizationManager;
    if (theManager != NULL)
        theManager->mUnitsPer[kUnitTypeSpaceDock] = inValue;
}
inline void EEComputerPlayer::SetUnitsPerStable(long inValue)
{
    this->mUnitsPerStable = inValue;
    EECPCivilizationManager* theManager = this->mCivilizationManager;
    if (theManager != NULL)
        theManager->mUnitsPer[kUnitTypeStable] = inValue;
}
inline void EEComputerPlayer::SetUnitsPerTankFactory(long inValue)
{
    this->mUnitsPerTankFactory = inValue;
    EECPCivilizationManager* theManager = this->mCivilizationManager;
    if (theManager != NULL)
        theManager->mUnitsPer[kUnitTypeTankFactory] = inValue;
}
inline void EEComputerPlayer::SetUnitsPerTemple(long inValue)
{
    this->mUnitsPerTemple = inValue;
    EECPCivilizationManager* theManager = this->mCivilizationManager;
    if (theManager != NULL)
        theManager->mUnitsPer[kUnitTypeTemple] = inValue;
}
inline void EEComputerPlayer::SetUnitsPerTower(long inValue)
{
    this->mUnitsPerTower = inValue;
    EECPCivilizationManager* theManager = this->mCivilizationManager;
    if (theManager != NULL)
        theManager->mUnitsPer[kUnitTypeGuardTowerPaleo] = inValue;
}
inline void EEComputerPlayer::SetUnitsPerUniversity(long inValue)
{
    this->mUnitsPerUniversity = inValue;
    EECPCivilizationManager* theManager = this->mCivilizationManager;
    if (theManager != NULL)
        theManager->mUnitsPer[kUnitTypeUniversity] = inValue;
}

// FUNCTION: EXE 0x5c0cc9
// Sets one AI tuning value (mAIAttribute) on each chosen computer player from mIntValue1/2 and mFloatValue; most
// building-count settings are also queued as pending effects
// Calls the 34 EEComputerPlayer inline setters (COMDATs 0x1c1ca9..0x1c24c8, emitted after it)
void EESMEffect::SetAIVariable()
{
    std::vector<long> thePlayers;
    EESMEffect theEffect;
    bool theDeferred = false;
    gWorld.mScenarioManager->ChoosePlayer(this->mPlayer, &thePlayers, true);
    for (unsigned long i = 0; i < thePlayers.size(); ++i) {
        EEPlayer* theBasePlayer = gWorld.GetPlayer(thePlayers[i]);
        if (theBasePlayer == NULL || theBasePlayer->IsHuman())
            continue;
        // Original dispatch uses the computer-player-only tuning and manager members after its player-kind test.
        EEComputerPlayer* thePlayer = static_cast<EEComputerPlayer*>(theBasePlayer);
        long theValue;
        switch(this->mAIAttribute) {
        case kCPBehaviorInitialAttackDelay:
            thePlayer->mInitialAttackDelayMax = max(this->mIntValue1, this->mIntValue2);
            thePlayer->mInitialAttackDelayMin = min(this->mIntValue1, this->mIntValue2);
            break;
        case kCPBehaviorAttackDelayModifier:   // as EEComputerPlayer::Init (was mDifficulty = value 1)
            thePlayer->mAttackDelayModifierEasy = this->mIntValue1;
            thePlayer->mAttackDelayModifierHard = this->mIntValue2;
            break;
        case kCPBehaviorTroopsToAssistAllies:
            thePlayer->mAssistAlliesDelayEasy = this->mIntValue1 * 60000;
            thePlayer->mAssistAlliesDelayMedium = this->mIntValue2 * 60000;   // value 2, as Init
            theValue = (long)(this->mFloatValue * 60000.0f);
            thePlayer->mAssistAlliesDelayHard = theValue;
            break;
        case kCPBehaviorAttackFrequency:
            thePlayer->mAttackFrequencyMin = min(this->mIntValue1, this->mIntValue2);
            thePlayer->mAttackFrequencyMax = max(this->mIntValue1, this->mIntValue2);
            thePlayer->SetNextAttackTime(gPhysics->mGameTime);
            break;
        case kCPBehaviorAttackFrequencyModifier:
            thePlayer->mAttackFrequencyModifierEasy = this->mIntValue1;
            thePlayer->mAttackFrequencyModifierHard = this->mIntValue2;
            break;
        case kCPBehaviorNumberOfLandExplorers:
            thePlayer->mNumLandExplorers = this->mIntValue1;
            break;
        case kCPBehaviorNumberOfSeaExplorers:
            thePlayer->mNumSeaExplorers = this->mIntValue1;
            break;
        case kCPBehaviorNumberOfAirExplorers:
            thePlayer->mNumAirExplorers = this->mIntValue1;
            break;
        case kCPBehaviorAttackGroupModifier:
            thePlayer->mAttackGroupModifierEasy = this->mIntValue1;
            thePlayer->mAttackGroupModifierMedium = this->mIntValue2;   // value 2, as Init
            theValue = (long)this->mFloatValue;
            thePlayer->mAttackGroupModifierHard = theValue;
            break;
        case kCPBehaviorCountWeights:
            thePlayer->mMilitaryCountWeight = (float)this->mIntValue1 * 0.01f;
            thePlayer->mWorkerCountWeight = (float)this->mIntValue2 * 0.01f;   // as Init (was mMilitaryCountWeight again)
            break;
        case kCPBehaviorAttackGroupSize:
            thePlayer->mAttackGroupSizeMin = min(this->mIntValue1, this->mIntValue2);
            thePlayer->mAttackGroupSizeMax = max(this->mIntValue1, this->mIntValue2);
            break;
        case kCPBehaviorResourceDropOffDistance:
            thePlayer->mDropOffDistance = this->mIntValue1;
            break;
        case kCPBehaviorResearchFrequency:
            thePlayer->mResearchFrequency = this->mIntValue1;
            break;
        case kCPBehaviorUnitSelectedForImprovements:
            thePlayer->mNumImprovedUnitTypes = this->mIntValue1;
            break;
        case kCPBehaviorUnitAttributesSelectedForImprovements:
            thePlayer->mNumImprovedAttributes = this->mIntValue1;
            break;
        case 0x10:
            thePlayer->mLD7C = min(this->mIntValue1, this->mIntValue2);
            thePlayer->mLD78 = max(this->mIntValue1, this->mIntValue2);
            break;
        case kCPBehaviorInitialAttackDelayPerEpoch:
            thePlayer->mInitialAttackDelayPerEpoch = this->mIntValue1;
            break;
        case kCPBehaviorAttackFrequencyBoundaries:
            thePlayer->mAttackDelayFloor = min(this->mIntValue1, this->mIntValue2);
            thePlayer->mAttackDelayCeiling = max(this->mIntValue1, this->mIntValue2);
            break;
        case kCPBehaviorDefensiveUnitCap:
            thePlayer->mDefensiveUnitCap = this->mFloatValue;
            break;
        case kCPBehaviorAttackFrequencyBoundary:
            thePlayer->mAttackFrequencyFloor = min(this->mIntValue1, this->mIntValue2);
            thePlayer->mAttackFrequencyCeiling = max(this->mIntValue1, this->mIntValue2);
            break;
        case kCPBehaviorAttackFrequencyPerEpoch:
            thePlayer->mAttackFrequencyPerEpoch = this->mFloatValue;
            break;
        case kCPBehaviorResearchFrequencyModifier:
            thePlayer->mResearchFrequencyModifierEasy = this->mIntValue1;
            thePlayer->mResearchFrequencyModifierHard = this->mIntValue2;
            break;
        case kCPBehaviorAttackEnemy:
            thePlayer->mAttackEnemyChance = this->mIntValue1;
            break;
        case kCPBehaviorAttackToClaimResource:
            thePlayer->mAttackToClaimResourceChance = this->mIntValue1;
            break;
        case kCPBehaviorAttackForRevenge:
            thePlayer->mAttackForRevengeChance = this->mIntValue1;
            break;
        case kCPBehaviorEpochFrequency:
            thePlayer->mEpochFrequencyMin = min(this->mIntValue1, this->mIntValue2);
            thePlayer->mEpochFrequencyMax = max(this->mIntValue1, this->mIntValue2);
            break;
        case kCPBehaviorDesiredNumberOfBarracks:
        {
            thePlayer->SetUnitsPerBarracks(this->mIntValue1);
            thePlayer->SetBarracksMax(UMFloatToLong(this->mFloatValue));
            theDeferred = true;
            break;
        }
        case kCPBehaviorDesiredNumberOfArcheryRanges:
        {
            thePlayer->SetUnitsPerArcheryRange(this->mIntValue1);
            thePlayer->SetArcheryRangeMax(UMFloatToLong(this->mFloatValue));
            theDeferred = true;
            break;
        }
        case kCPBehaviorDesiredNumberOfStables:
        {
            thePlayer->SetUnitsPerStable(this->mIntValue1);
            thePlayer->SetStableMax(UMFloatToLong(this->mFloatValue));
            theDeferred = true;
            break;
        }
        case kCPBehaviorDesiredNumberOfDocks:
        {
            thePlayer->SetUnitsPerDock(this->mIntValue1);
            thePlayer->SetDockMax(UMFloatToLong(this->mFloatValue));
            theDeferred = true;
            break;
        }
        case kCPBehaviorDesiredNumberOfSiegeFactories:
        {
            thePlayer->SetUnitsPerSiegeFactory(this->mIntValue1);
            thePlayer->SetSiegeFactoryMax(UMFloatToLong(this->mFloatValue));
            theDeferred = true;
            break;
        }
        case kCPBehaviorDesiredNumberOfNavalYards:
        {
            thePlayer->SetUnitsPerNavalYard(this->mIntValue1);
            thePlayer->SetNavalYardMax(UMFloatToLong(this->mFloatValue));
            theDeferred = true;
            break;
        }
        case kCPBehaviorDesiredNumberOfAirports:
        {
            thePlayer->SetUnitsPerAirport(this->mIntValue1);
            thePlayer->SetAirportMax(UMFloatToLong(this->mFloatValue));
            theDeferred = true;
            break;
        }
        case kCPBehaviorDesiredNumberOfCyberFactories:
        {
            thePlayer->SetUnitsPerCyberFactory(this->mIntValue1);
            thePlayer->SetCyberFactoryMax(UMFloatToLong(this->mFloatValue));
            theDeferred = true;
            break;
        }
        case kCPBehaviorDesiredNumberOfUniversities:
        {
            thePlayer->SetUnitsPerUniversity(this->mIntValue1);
            thePlayer->SetUniversityMax(UMFloatToLong(this->mFloatValue));
            theDeferred = true;
            break;
        }
        case kCPBehaviorDesiredNumberOfTemples:
        {
            thePlayer->SetUnitsPerTemple(this->mIntValue1);
            thePlayer->SetTempleMax(UMFloatToLong(this->mFloatValue));
            theDeferred = true;
            break;
        }
        case kCPBehaviorDesiredNumberOfHospitals:
        {
            thePlayer->SetUnitsPerHospital(this->mIntValue1);
            thePlayer->SetHospitalMax(UMFloatToLong(this->mFloatValue));
            theDeferred = true;
            break;
        }
        case kCPBehaviorDesiredNumberOfGranaries:
        {
            thePlayer->SetUnitsPerGranary(this->mIntValue1);
            thePlayer->SetGranaryMax(UMFloatToLong(this->mFloatValue));
            theDeferred = true;
            break;
        }
        case kCPBehaviorDesiredNumberOfCyberLaboratories:
        {
            thePlayer->SetUnitsPerCyberLab(this->mIntValue1);
            thePlayer->SetCyberLabMax(UMFloatToLong(this->mFloatValue));
            theDeferred = true;
            break;
        }
        case kCPBehaviorDesiredNumberOfSpaceDocks:
        {
            thePlayer->SetUnitsPerSpaceDock(this->mIntValue1);
            thePlayer->SetSpaceDockMax(UMFloatToLong(this->mFloatValue));
            theDeferred = true;
            break;
        }
        case kCPBehaviorNumberOfMilitaryUnitsToTrain:
            thePlayer->mMilitaryPopPercentage = this->mFloatValue;
            break;
        case kCPBehaviorSpecializedUnitPercentage:
            thePlayer->mSpecializedUnitPercentage = this->mFloatValue;
            break;
        case kCPBehaviorMaximumAttackResponseRange:
            // the triggers compare it with squared distances: square it as EECPBehaviorTable::Load squares the
            // database value (the editor slider uses the unsquared range)
            thePlayer->mMaxAttackResponseRange = this->mFloatValue * this->mFloatValue;
            break;
        case kCPBehaviorPriestUsage:
            thePlayer->mPriestMaxPerGroup = this->mIntValue1;
            thePlayer->mPriestChance = (float)this->mIntValue2 * 0.01f;
            thePlayer->mPriestPopPercentage = this->mFloatValue;
            break;
        case kCPBehaviorProphetUsage:
            thePlayer->mProphetMaxPerGroup = this->mIntValue1;
            thePlayer->mProphetChance = (float)this->mIntValue2 * 0.01f;
            thePlayer->mProphetPopPercentage = this->mFloatValue;
            break;
        case kCPBehaviorHeroUsage:
            thePlayer->mHeroMaxPerGroup = this->mIntValue1;
            thePlayer->mHeroChance = (float)this->mIntValue2 * 0.01f;
            thePlayer->mHeroPopPercentage = this->mFloatValue;
            break;
        case kCPBehaviorCarrierUsage:
            thePlayer->mCarrierMaxPerGroup = this->mIntValue1;
            thePlayer->mCarrierChance = (float)this->mIntValue2 * 0.01f;
            thePlayer->mCarrierPopPercentage = this->mFloatValue;
            break;
        case kCPBehaviorRPSSelectionNonIslands:
            thePlayer->mRPSSelectionNonIslands = this->mFloatValue;
            break;
        case kCPBehaviorEnableStockCivilization:
            thePlayer->mEnableStockCivilization = this->mIntValue1;
            break;
        case kCPBehaviorEnableStockResource:
            thePlayer->mEnableStockResource = this->mIntValue1;
            break;
        case kCPBehaviorEnableStockResearch:
            thePlayer->mEnableStockResearch = this->mIntValue1;
            break;
        case kCPBehaviorEnableStockProduction:
            thePlayer->mEnableStockProduction = this->mIntValue1;
            break;
        case kCPBehaviorEnableStockBuild:
            thePlayer->mEnableStockBuild = this->mIntValue1;
            break;
        case 0x3c:
            thePlayer->mLE80 = this->mIntValue1;
            break;
        case kCPBehaviorEnableStockDefense:
            thePlayer->mEnableStockDefense = this->mIntValue1;
            break;
        case kCPBehaviorEnableStockExploration:
            thePlayer->mEnableStockExploration = this->mIntValue1;
            break;
        case kCPBehaviorEnableStockCombat:
            thePlayer->mEnableStockCombat = this->mIntValue1;
            break;
        case kCPBehaviorManagerEpochAdvanceWait:
            thePlayer->mManagerEpochAdvanceWait = this->mIntValue1;
            break;
        case kCPBehaviorManagerAttackWait:
            thePlayer->mManagerAttackWait = this->mIntValue1;
            break;
        case kCPBehaviorMaxEnabledManagers:
            thePlayer->mMaxEnabledManagers = this->mIntValue1;
            break;
        case kCPBehaviorUnitCostReductionMedium:
            thePlayer->mUnitCostMedium = this->mFloatValue;   // a percentage, as Init (the user scales it by 0.01)
            break;
        case kCPBehaviorEnableInterval:
            thePlayer->mEnableInterval = this->mFloatValue;
            break;
        case kCPBehaviorAidThreatThreshold:
            thePlayer->mAidThreatThreshold = this->mFloatValue;
            break;
        case kCPBehaviorGuardChokepointPercentage:
            thePlayer->mGuardChokepointPercentage = this->mFloatValue;
            break;
        case kCPBehaviorGuardContinentPercentage:
            thePlayer->mGuardContinentPercentage = this->mFloatValue;
            break;
        case kCPBehaviorGuardHighGroundPercentage:
            thePlayer->mGuardHighGroundPercentage = this->mFloatValue;
            break;
        case kCPBehaviorGuardTownLandPercentage:
            thePlayer->mGuardTownLandPercentage = this->mFloatValue;
            break;
        case 0x4a:
            thePlayer->mFC44 = this->mFloatValue;
            break;
        case 0x4b:
            thePlayer->mFC50 = this->mFloatValue;
            break;
        case 0x4c:
            thePlayer->mFC58 = this->mFloatValue;
            break;
        case kCPBehaviorGuardChokepointAirPercentage:
            thePlayer->mGuardChokepointAirPercentage = this->mFloatValue;
            break;
        case kCPBehaviorGuardTownPercentage:
            thePlayer->mGuardTownPercentage = this->mFloatValue;
            break;
        case 0x4f:
            thePlayer->mFC38 = this->mFloatValue;
            break;
        case 0x50:
            thePlayer->mFC40 = this->mFloatValue;
            break;
        case 0x51:
            thePlayer->mFC4C = this->mFloatValue;
            break;
        case kCPBehaviorGuardWaterwayPercentage:
            thePlayer->mGuardWaterwayPercentage = this->mFloatValue;
            break;
        case 0x53:
            thePlayer->mFC3C = this->mFloatValue;
            break;
        case 0x54:
            thePlayer->mFC48 = this->mFloatValue;
            break;
        case 0x55:
            thePlayer->mFC54 = this->mFloatValue;
            break;
        case kCPBehaviorNumChokePointsToGuard:
            thePlayer->mNumChokePointsToGuard = this->mIntValue1;
            break;
        case kCPBehaviorDefenseGroupSizeModifierPercentage:
            thePlayer->mDefenseGroupSizeMin = min(this->mIntValue1, this->mIntValue2);
            thePlayer->mDefenseGroupSizeMax = max(this->mIntValue1, this->mIntValue2);
            break;
        case kCPBehaviorDefensiveUnitUpdate:
            thePlayer->mDefensiveUnitUpdateEasy = this->mIntValue1;
            thePlayer->mDefensiveUnitUpdateMedium = this->mIntValue2;
            theValue = (long)this->mFloatValue;
            thePlayer->mDefensiveUnitUpdateHard = theValue;
            break;
        case kCPBehaviorInitialDelayInReplenishmentDefenseUnits:
            thePlayer->mReplenishDelayMin = min(this->mIntValue1, this->mIntValue2);
            thePlayer->mReplenishDelayMax = max(this->mIntValue1, this->mIntValue2);
            break;
        case kCPBehaviorTownGuardPercentage:
            thePlayer->mTownGuardPercentage = this->mFloatValue;
            break;
        case kCPBehaviorComputerVsComputerAggression:
            thePlayer->mComputerAggression = this->mFloatValue;
            break;
        case kCPBehaviorDesiredNumberOfTowers:
        {
            thePlayer->SetUnitsPerTower(this->mIntValue1);
            thePlayer->SetTowerMax(UMFloatToLong(this->mFloatValue));
            break;
        }
        case kCPBehaviorUnitCostReductionEasyAndHard:   // its own values, as Init (was the epoch frequency modifier body)
            thePlayer->mUnitCostReductionEasy = this->mIntValue1;
            thePlayer->mUnitCostReductionHard = this->mIntValue2;
            break;
        case kCPBehaviorEpochFrequencyModifier:
            thePlayer->mEpochFrequencyModifierEasy = this->mIntValue1;
            thePlayer->mEpochFrequencyModifierHard = this->mIntValue2;
            break;
        case kCPBehaviorExpansionTimer:
        {
            thePlayer->mExpansionTimerEasy = this->mIntValue1;
            thePlayer->mExpansionTimerMedium = this->mIntValue2;
            long theRoundedValue = UMFloatToLong(this->mFloatValue);
            thePlayer->mExpansionTimerHard = theRoundedValue;
            break;
        }
        case kCPBehaviorPopulationLimitAdjustment:
        {
            thePlayer->mPopLimitAdjustmentEasy = this->mIntValue1;
            thePlayer->mPopLimitAdjustmentMedium = this->mIntValue2;
            long theRoundedValue = UMFloatToLong(this->mFloatValue);
            thePlayer->mPopLimitAdjustmentHard = theRoundedValue;
            break;
        }
        case kCPBehaviorTownCenterUpgradeTimer:
            thePlayer->mTownCenterUpgradeTimer = this->mIntValue1;
            break;
        case kCPBehaviorCapitolUpgradeTimer:
            thePlayer->mCapitolUpgradeTimer = this->mIntValue1;
            break;
        case kCPBehaviorEpochCatchUp:
            thePlayer->mEpochCatchUpMin = this->mIntValue1;
            thePlayer->mEpochCatchUpMax = this->mIntValue2;
            break;
        case kCPBehaviorEasyMinNumberMilitaryBuildings:
            thePlayer->mMinMilitaryBuildingsEasy = this->mIntValue1;
            break;
        case kCPBehaviorHardMinNumberMilitaryBuildings:
            thePlayer->mMinMilitaryBuildingsHard = this->mIntValue1;
            break;
        case kCPBehaviorDMEarlyAttack:
            thePlayer->mDMEarlyAttackScale = (float)this->mIntValue1 * 0.01f;
            break;
        case kCPBehaviorMediumMinNumberMilitaryBuildings:
            thePlayer->mMinMilitaryBuildingsMedium = this->mIntValue1;
            break;
        case kCPBehaviorDesiredNumberOfTankFactories:
        {
            thePlayer->SetUnitsPerTankFactory(this->mIntValue1);
            thePlayer->SetTankFactoryMax(UMFloatToLong(this->mFloatValue));
            break;
        }
        case kCPBehaviorDesiredNumberOfAntiAirBuildings:
        {
            thePlayer->SetUnitsPerAntiAirBuilding(this->mIntValue1);
            thePlayer->SetAntiAirMax(UMFloatToLong(this->mFloatValue));
            break;
        }
        case kCPBehaviorSpecializedUnitSelectionChance:   // its own value, as Init (was the RPS islands body)
            thePlayer->mSpecializedUnitChance = this->mFloatValue;
            break;
        case kCPBehaviorRPSSelectionIslands:
            thePlayer->mRPSSelectionIslands = this->mFloatValue;
            break;
        case kCPBehaviorNavalUnitWeight:
            thePlayer->mNavalUnitWeight = this->mFloatValue;
            break;
        case kCPBehaviorAirUnitWeight:
            thePlayer->mAirUnitWeight = this->mFloatValue;
            break;
        case kCPBehaviorDesiredNumberOfFlameThrowers:
            thePlayer->mMaxFlameThrowers = this->mIntValue1;
            break;
        case kCPBehaviorTownGuardUpdate:
            thePlayer->mTownGuardUpdateEasy = this->mIntValue1;
            thePlayer->mTownGuardUpdateMedium = this->mIntValue2;
            theValue = (long)this->mFloatValue;
            thePlayer->mTownGuardUpdateHard = theValue;
            break;
        case kCPBehaviorTownGuardUpdateDeathmatch:
            thePlayer->mDMTownGuardUpdateEasy = this->mIntValue1;
            thePlayer->mDMTownGuardUpdateMedium = this->mIntValue2;
            theValue = (long)this->mFloatValue;
            thePlayer->mDMTownGuardUpdateHard = theValue;
            break;
        case kCPBehaviorDefensiveUnitUpdateDeathmatch:
            thePlayer->mDMDefensiveUnitUpdateEasy = this->mIntValue1;
            thePlayer->mDMDefensiveUnitUpdateMedium = this->mIntValue2;
            theValue = (long)this->mFloatValue;
            thePlayer->mDMDefensiveUnitUpdateHard = theValue;
            break;
        case kCPBehaviorMinimumShipMaintenanceTimer:
            thePlayer->mShipMaintenanceTimer = this->mIntValue1;
            break;
        case kCPBehaviorShipsToMaintain:
            thePlayer->mShipsToMaintainEasy = this->mIntValue1;
            thePlayer->mShipsToMaintainMedium = this->mIntValue2;
            theValue = (long)this->mFloatValue;
            thePlayer->mShipsToMaintainHard = theValue;
            break;
        case kCPBehaviorShipsToMaintainDeathmatch:
            thePlayer->mShipsToMaintainDMEasy = this->mIntValue1;
            thePlayer->mShipsToMaintainDMMedium = this->mIntValue2;
            theValue = (long)this->mFloatValue;
            thePlayer->mShipsToMaintainDMHard = theValue;
            break;
        case kCPBehaviorSiegeWeaponPercentage:
            thePlayer->mSiegeWeaponPercentage = this->mFloatValue;
            break;
        case kCPBehaviorBonzaiAttackLevel:
            thePlayer->mBonzaiAttackLevel = this->mIntValue1;
            break;
        case kCPBehaviorIslandExpansionTimer:
            thePlayer->SetIslandExpansionTimer(this->mIntValue1);
            break;
        case kCPBehaviorDMAttackFrequencyModifier:
            thePlayer->SetDMAttackFrequencyModifier(this->mFloatValue);
            break;
        case kCPBehaviorDMInitialAttackModifier:
            thePlayer->SetDMInitialAttackModifier(this->mFloatValue);
        }
        if (!theDeferred)
            continue;
        theEffect.Copy(*this);
        theEffect.mPlayer = thePlayer->mPlayerID + 6;
        gWorld.mScenarioManager->AddV134Effect(theEffect);
    }
}

// FUNCTION: EXE 0x5c1ca9
// Sets the computer player's anti-aircraft building maximum (and the civilization manager's copy)
// Inline EEComputerPlayer::SetAntiAirMax(long), emitted by SetAIVariable (ee/EEComputerPlayer.h).

// FUNCTION: EXE 0x5c1ce8
// Sets the computer player's airport maximum (and the civilization manager's copy)
// Inline EEComputerPlayer::SetAirportMax(long), emitted by SetAIVariable (ee/EEComputerPlayer.h).

// FUNCTION: EXE 0x5c1d27
// Sets the computer player's archery range maximum (and the civilization manager's copy)
// Inline EEComputerPlayer::SetArcheryRangeMax(long), emitted by SetAIVariable (ee/EEComputerPlayer.h).

// FUNCTION: EXE 0x5c1d66
// Sets the computer player's barracks maximum (and the civilization manager's copy)
// Inline EEComputerPlayer::SetBarracksMax(long), emitted by SetAIVariable (ee/EEComputerPlayer.h).

// FUNCTION: EXE 0x5c1da5
// Sets the computer player's dock maximum (and the civilization manager's copy)
// Inline EEComputerPlayer::SetDockMax(long), emitted by SetAIVariable (ee/EEComputerPlayer.h).

// FUNCTION: EXE 0x5c1de4
// Sets the computer player's granary (farm) maximum (and the civilization manager's copy)
// Inline EEComputerPlayer::SetGranaryMax(long), emitted by SetAIVariable (ee/EEComputerPlayer.h).

// FUNCTION: EXE 0x5c1e23
// Sets the computer player's hospital maximum (and the civilization manager's copy)
// Inline EEComputerPlayer::SetHospitalMax(long), emitted by SetAIVariable (ee/EEComputerPlayer.h).

// FUNCTION: EXE 0x5c1e62
// Sets the computer player's cyber factory maximum (and the civilization manager's copy)
// Inline EEComputerPlayer::SetCyberFactoryMax(long), emitted by SetAIVariable (ee/EEComputerPlayer.h).

// FUNCTION: EXE 0x5c1ea1
// Sets the computer player's cyber laboratory maximum (and the civilization manager's copy)
// Inline EEComputerPlayer::SetCyberLabMax(long), emitted by SetAIVariable (ee/EEComputerPlayer.h).

// FUNCTION: EXE 0x5c1ee0
// Sets the computer player's naval yard maximum (and the civilization manager's copy)
// Inline EEComputerPlayer::SetNavalYardMax(long), emitted by SetAIVariable (ee/EEComputerPlayer.h).

// FUNCTION: EXE 0x5c1f1f
// Sets the computer player's siege factory maximum (and the civilization manager's copy)
// Inline EEComputerPlayer::SetSiegeFactoryMax(long), emitted by SetAIVariable (ee/EEComputerPlayer.h).

// FUNCTION: EXE 0x5c1f5e
// Sets the computer player's space dock maximum (and the civilization manager's copy)
// Inline EEComputerPlayer::SetSpaceDockMax(long), emitted by SetAIVariable (ee/EEComputerPlayer.h).

// FUNCTION: EXE 0x5c1f9d
// Sets the computer player's stable maximum (and the civilization manager's copy)
// Inline EEComputerPlayer::SetStableMax(long), emitted by SetAIVariable (ee/EEComputerPlayer.h).

// FUNCTION: EXE 0x5c1fdc
// Sets the computer player's tank factory maximum (and the civilization manager's copy)
// Inline EEComputerPlayer::SetTankFactoryMax(long), emitted by SetAIVariable (ee/EEComputerPlayer.h).

// FUNCTION: EXE 0x5c201b
// Sets the computer player's temple maximum (and the civilization manager's copy)
// Inline EEComputerPlayer::SetTempleMax(long), emitted by SetAIVariable (ee/EEComputerPlayer.h).

// FUNCTION: EXE 0x5c205a
// Sets the computer player's tower maximum (and the civilization manager's copy)
// Inline EEComputerPlayer::SetTowerMax(long), emitted by SetAIVariable (ee/EEComputerPlayer.h).

// FUNCTION: EXE 0x5c2099
// Sets the computer player's university maximum (and the civilization manager's copy)
// Inline EEComputerPlayer::SetUniversityMax(long), emitted by SetAIVariable (ee/EEComputerPlayer.h).

// FUNCTION: EXE 0x5c20d8
// Sets the computer player's units per anti-aircraft building (and the civilization manager's copy)
// Inline EEComputerPlayer::SetUnitsPerAntiAirBuilding(long), emitted by SetAIVariable (ee/EEComputerPlayer.h).

// FUNCTION: EXE 0x5c2117
// Sets the computer player's units per airport (and the civilization manager's copy)
// Inline EEComputerPlayer::SetUnitsPerAirport(long), emitted by SetAIVariable (ee/EEComputerPlayer.h).

// FUNCTION: EXE 0x5c2156
// Sets the computer player's units per archery range (and the civilization manager's copy)
// Inline EEComputerPlayer::SetUnitsPerArcheryRange(long), emitted by SetAIVariable (ee/EEComputerPlayer.h).

// FUNCTION: EXE 0x5c2195
// Sets the computer player's units per barracks (and the civilization manager's copy)
// Inline EEComputerPlayer::SetUnitsPerBarracks(long), emitted by SetAIVariable (ee/EEComputerPlayer.h).

// FUNCTION: EXE 0x5c21d4
// Sets the computer player's units per dock (and the civilization manager's copy)
// Inline EEComputerPlayer::SetUnitsPerDock(long), emitted by SetAIVariable (ee/EEComputerPlayer.h).

// FUNCTION: EXE 0x5c2213
// Sets the computer player's units per granary (farm) (and the civilization manager's copy)
// Inline EEComputerPlayer::SetUnitsPerGranary(long), emitted by SetAIVariable (ee/EEComputerPlayer.h).

// FUNCTION: EXE 0x5c2252
// Sets the computer player's units per hospital (and the civilization manager's copy)
// Inline EEComputerPlayer::SetUnitsPerHospital(long), emitted by SetAIVariable (ee/EEComputerPlayer.h).

// FUNCTION: EXE 0x5c2291
// Sets the computer player's units per cyber factory (and the civilization manager's copy)
// Inline EEComputerPlayer::SetUnitsPerCyberFactory(long), emitted by SetAIVariable (ee/EEComputerPlayer.h).

// FUNCTION: EXE 0x5c22d0
// Sets the computer player's units per cyber laboratory (and the civilization manager's copy)
// Inline EEComputerPlayer::SetUnitsPerCyberLab(long), emitted by SetAIVariable (ee/EEComputerPlayer.h).

// FUNCTION: EXE 0x5c230f
// Sets the computer player's units per naval yard (and the civilization manager's copy)
// Inline EEComputerPlayer::SetUnitsPerNavalYard(long), emitted by SetAIVariable (ee/EEComputerPlayer.h).

// FUNCTION: EXE 0x5c234e
// Sets the computer player's units per siege factory (and the civilization manager's copy)
// Inline EEComputerPlayer::SetUnitsPerSiegeFactory(long), emitted by SetAIVariable (ee/EEComputerPlayer.h).

// FUNCTION: EXE 0x5c238d
// Sets the computer player's units per space dock (and the civilization manager's copy)
// Inline EEComputerPlayer::SetUnitsPerSpaceDock(long), emitted by SetAIVariable (ee/EEComputerPlayer.h).

// FUNCTION: EXE 0x5c23cc
// Sets the computer player's units per stable (and the civilization manager's copy)
// Inline EEComputerPlayer::SetUnitsPerStable(long), emitted by SetAIVariable (ee/EEComputerPlayer.h).

// FUNCTION: EXE 0x5c240b
// Sets the computer player's units per tank factory (and the civilization manager's copy)
// Inline EEComputerPlayer::SetUnitsPerTankFactory(long), emitted by SetAIVariable (ee/EEComputerPlayer.h).

// FUNCTION: EXE 0x5c244a
// Sets the computer player's units per temple (and the civilization manager's copy)
// Inline EEComputerPlayer::SetUnitsPerTemple(long), emitted by SetAIVariable (ee/EEComputerPlayer.h).

// FUNCTION: EXE 0x5c2489
// Sets the computer player's units per tower (and the civilization manager's copy)
// Inline EEComputerPlayer::SetUnitsPerTower(long), emitted by SetAIVariable (ee/EEComputerPlayer.h).

// FUNCTION: EXE 0x5c24c8
// Sets the computer player's units per university (and the civilization manager's copy)
// Inline EEComputerPlayer::SetUnitsPerUniversity(long), emitted by SetAIVariable (ee/EEComputerPlayer.h).

// FUNCTION: EXE 0x5c2507
// Sets each chosen computer player's favourite air, land and sea unit types (mObject, mIntValue1, mIntValue2), each
// only when the class is a moving unit of that kind (air and sea ones must attack)
void EESMEffect::SetFavoriteUnits()
{
    std::vector<long> thePlayers;
    gWorld.mScenarioManager->ChoosePlayer(this->mPlayer, &thePlayers, true);
    for (unsigned long i = 0; i < thePlayers.size(); i++) {
        EEPlayer* theBasePlayer = gWorld.GetPlayer(thePlayers[i]);
        if (theBasePlayer == NULL || theBasePlayer->IsHuman())
            continue;
        EEComputerPlayer* thePlayer = static_cast<EEComputerPlayer*>(theBasePlayer);
        EEUnitClass* theClass = gUnitClassTable[this->mObject];
        if (theClass != NULL && (theClass->GetDomains() & kMovementAir) && !theClass->GetIsProjectile() && !theClass->IsLandBuilding()
            && theClass->GetAttack() > 0 && theClass->GetMaxVelocity() > 0.0f) {
            thePlayer->SetFavoriteAirType(this->mObject);
        }
        theClass = gUnitClassTable[this->mIntValue1];
        if (theClass != NULL && !(theClass->GetDomains() & kMovementSea) && (!(theClass->GetDomains() & kMovementAir) || theClass->FliesOver())
            && !theClass->GetIsProjectile() && !theClass->IsLandBuilding() && theClass->GetMaxVelocity() > 0.0f) {
            switch (theClass->GetCategory()) {
            case kUnitCategoryAmbient: case kUnitCategoryResource: case kUnitCategoryBuilding: case kUnitCategoryAirplane: case kUnitCategoryNaval:
            case kUnitCategoryProjectile: case kUnitCategorySubmergedNaval: case kUnitCategoryBomb: case kUnitCategoryMissile:
            case kUnitCategoryNavalPriest:
            case kUnitCategoryHelicopterTransport: case kUnitCategoryNavalTransport: case kUnitCategoryBuilding19: case kUnitCategoryTorpedo: case kUnitCategoryNavalBuilding: case kUnitCategoryNavalCitizen:
            case kUnitCategoryMiningBuilding: case kUnitCategoryGate:
            case kUnitCategorySpaceship: case kUnitCategorySpaceBuilding: case kUnitCategorySpaceTransport: case kUnitCategoryPlanetSpaceship:
                break;
            default:
                thePlayer->SetFavoriteLandType(this->mIntValue1);
                break;
            }
        }
        long theType = this->GetIntValue2();
        theClass = gUnitClassTable[theType];
        if (theClass != NULL && (theClass->GetDomains() & kMovementSea) && !theClass->GetIsProjectile() && theClass->GetMaxVelocity() > 0.0f
            && theClass->GetAttack() > 0) {
            thePlayer->SetFavoriteSeaType(theType);
        }
    }
}

// FUNCTION: EXE 0x5c26da
// Switches a behaviour of each chosen computer player on or off: active, cheat, attack, defend, build, explore, ignore
// others or gather
void EESMEffect::SetAIToggle()
{
    std::vector<long> thePlayers;
    gWorld.mScenarioManager->ChoosePlayer(this->mPlayer, &thePlayers, true);
    for (unsigned long i = 0; i < thePlayers.size(); i++) {
        EEPlayer* theBasePlayer = gWorld.GetPlayer(thePlayers[i]);
        if (theBasePlayer == NULL || theBasePlayer->IsHuman())
            continue;
        EEComputerPlayer* thePlayer = static_cast<EEComputerPlayer*>(theBasePlayer);
        switch (this->mAIToggle) {
        case kSMAIToggleActive:
            thePlayer->SetActive(this->mIsOn);
            break;
        case kSMAIToggleCheat:
            thePlayer->SetIsCheating(this->mIsOn);
            break;
        case kSMAIToggleAttack:
            thePlayer->SetCanAttack(this->mIsOn);
            break;
        case kSMAIToggleDefend:
            thePlayer->SetCanDefend(this->mIsOn);
            break;
        case kSMAIToggleBuildPerRepair:
            thePlayer->SetCanBuild(this->mIsOn);
            break;
        case kSMAIToggleExplore:
            thePlayer->SetCanExplore(this->mIsOn);
            break;
        case kSMAIToggleIgnore:
            thePlayer->SetIgnoreOthers(this->mIsOn);
            break;
        case kSMAIToggleGather:
            thePlayer->SetCanGather(this->mIsOn);
            break;
        }
    }
}

#include "ee/EEUnitBehavior.h"
#include "ee/EEUGPatrol.h"
#include "ee/EEUGExplore.h"
#include "ee/EEPPatrolGroup.h"

// FUNCTION: EXE 0x5c2811
// Applies a stance to the scenario object's living units: explore, stop, lock or unlock a gate, patrol their town or a
// behaviour (scout, stand ground, guard, harass, cautious, aggressive)
void EESMEffect::SetUnitsStance()
{
    EESMObject* theObject = gWorld.mScenarioManager->GetScenarioObject(this->mObject);
    if (theObject == NULL)
        return;
    theObject->Evaluate(false);
    unsigned long theCount = theObject->mUnits.size();
    if (theCount == 0)
        return;
    for (unsigned long i = 0; i < theCount; i++) {
        EEUnit* theBaseUnit = gWorld.GetUnit(*(theObject->mUnits.begin() + i));
        EEComplexUnit* theUnit = theBaseUnit != NULL ? theBaseUnit->mComplexUnit : NULL;
        if (theUnit == NULL || theUnit->mHitPoints <= 0)
            continue;
        switch (this->mStance) {
        case kSMStanceExplore:
        {
            EEPlayer* thePlayer = theUnit->mPlayer;
            EEActiveGroup* theGroup = new EEActiveGroup(thePlayer);
            theGroup->AddMember(theUnit, true);
            thePlayer->mPendingGroups.push_back(theGroup);
            theGroup->KeepMobileUnits();
            theGroup->ClearGoal();
            theGroup->SetGoal(new EEUGExplore(true), false);
            theGroup->mAction = kActionFindUnexploredArea;
            theGroup->Plan();
            break;
        }
        case kSMStanceIdle:
            theUnit->ClearGoalQueue(0);
            theUnit->StartAction(theUnit->GetUnitClass()->mCategory == kUnitCategoryAirplane || theUnit->GetUnitClass()->mCategory == kUnitCategoryPlanetSpaceship
                                 ? 0x3f0 : 0x1388);
            theUnit->GetUnitClass()->ActionChanged(theUnit);
            break;
        case kSMStanceLocked:
        {
            EEUCGate* theGate = dynamic_cast<EEUCGate*>(theUnit->GetUnitClass());
            if (theUnit->mIsLocked != true)
                theUnit->mIsLocked = true;
            // a single-piece unit (a non-gate, or a gate created by a Create effect) has no second or fourth part
            EEComplexUnit* thePart = theGate->GetSecondPart(theUnit);
            if (thePart != NULL && thePart->mIsLocked != true)
                thePart->mIsLocked = true;
            thePart = theGate->GetFourthPart(theUnit);
            if (thePart != NULL && thePart->mIsLocked != true)
                thePart->mIsLocked = true;
            break;
        }
        case kSMStanceUnlocked:
        {
            EEUCGate* theGate = dynamic_cast<EEUCGate*>(theUnit->GetUnitClass());
            if (theUnit->mIsLocked)
                theUnit->mIsLocked = false;
            // a single-piece unit (a non-gate, or a gate created by a Create effect) has no second or fourth part
            EEComplexUnit* thePart = theGate->GetSecondPart(theUnit);
            if (thePart != NULL && thePart->mIsLocked)
                thePart->mIsLocked = false;
            thePart = theGate->GetFourthPart(theUnit);
            if (thePart != NULL && thePart->mIsLocked)
                thePart->mIsLocked = false;
            break;
        }
        case kSMStanceTownPatrol:
        {
            EETown* theTown = theUnit->FindNearestTown();
            if (theTown == NULL)
                break;
            EEPPatrolGroup* thePatrolGroup = theTown->mPatrolGroup;
            if (thePatrolGroup == NULL)
                break;
            EEComplexUnit* theTarget = thePatrolGroup->GetNearest(theUnit->GetTileX(), theUnit->GetTileY());
            if (theTarget == NULL)
                break;
            EEActiveGroup* theGroup = new EEActiveGroup(theUnit->mPlayer);
            theGroup->AddMember(theUnit, true);
            theUnit->mPlayer->mPendingGroups.push_back(theGroup);
            theGroup->KeepMobileUnits();
            EEUGPatrol* theGoal = new EEUGPatrol(thePatrolGroup, theTarget, true);
            theGroup->ClearGoal();
            theGroup->SetGoal(theGoal, false);
            theGroup->mAction = kActionMoveToNextPatrolPoint;
            theGroup->Plan();
            break;
        }
        case kSMStanceScout:
        case kSMStanceStandGround:
        case kSMStanceGuard:
        case kSMStanceHarass:
        case kSMStanceCautious:
        case kSMStanceAggressive:
        {
            EEUnitBehavior* theBehavior = EEUnitBehavior::sTable[this->mStance - 5];
            if (theUnit->mBehavior != theBehavior)
                theUnit->mBehavior = theBehavior;
            break;
        }
        }
    }
}

void EEModifyUnitAttribute(unsigned long inAttribute, bool inPercentage, EEPlayer* inPlayer,
                EEUnitClassUpgrade* inUpgrade, long inAmount, long inMode);   // EESMObject.cpp 0x1c8871

// FUNCTION: EXE 0x5c2b3e
// Changes an attribute of the scenario object's units, or a class attribute for each (unit type, player) it covers;
// without an object, of the effect's own unit (never as a percentage) or of the player's class upgrade
void EESMEffect::ChangeAttribute()
{
    std::vector<long> thePlayers;
    std::vector<U2DPoint<long> > theTargets;
    U2DPoint<long> theTarget;
    EESMObject* theObject = gWorld.mScenarioManager->GetScenarioObject(this->mObject);
    if (theObject != NULL && this->mAttributeUnit == -1 && this->mAttributeUnitType == 0) {
        if (this->mAttribute < kSMUnitAttributeShockArmorPlus) {
            theObject->Evaluate(false);
            unsigned long theCount = theObject->mUnits.size();
            if (theCount == 0)
                return;
            for (unsigned long i = 0; i < theCount; i++) {
                EEUnit* theUnit = gWorld.GetUnit(theObject->mUnits[i]);
                if (theUnit != NULL && (!theObject->GetIsSelectOnMap() || theUnit->GetHitPoints() > 0 || theUnit->GetUnitClass()->GetIsTree())
                    && this->mAttribute < kSMUnitAttributeShockArmorPlus) {
                    EEChangeUnit(this->mAttribute, this->mUsesPercent, theUnit, this->mAttributeAmount, 0, false, this->mAttributeMode);
                }
            }
        } else {
            if (!theObject->GetIsSelectOnMap() && theObject->GetInAreaCheck() == 0 && theObject->GetHasAttributeCheck() == 0 && theObject->GetNearCheck() == 0
                && theObject->GetSelectedByCheck() == 0 && theObject->GetHasStateCheck() == 0 && theObject->GetVisibleByCheck() == 0 && theObject->GetParent() == 0) {
                gWorld.mScenarioManager->ChoosePlayer(theObject->GetOwner(), &thePlayers, true);
                for (unsigned long i = 0; i < thePlayers.size(); i++) {
                    long theType = theObject->PickUnitType();
                    if (theType < 0)
                        continue;
                    theTarget.SetX(theType);
                    theTarget.SetY(thePlayers[i]);
                    if (std::find(theTargets.begin(), theTargets.end(), theTarget) == theTargets.end())
                        theTargets.push_back(theTarget);
                }
            } else {
                theObject->Evaluate(false);
                unsigned long theCount = theObject->mUnits.size();
                for (unsigned long i = 0; i < theCount; i++) {
                    EEUnit* theUnit = gWorld.GetUnit(theObject->mUnits[i]);
                    if (theUnit == NULL || theUnit->GetHitPoints() <= 0)
                        continue;
                    theTarget.SetX(theUnit->GetUnitClass()->GetUnitType());
                    theTarget.SetY(theUnit->GetPlayer()->GetPlayerID());
                    if (std::find(theTargets.begin(), theTargets.end(), theTarget) == theTargets.end())
                        theTargets.push_back(theTarget);
                }
            }
            for (unsigned long j = 0; j < theTargets.size(); j++) {
                EEPlayer* thePlayer = gWorld.GetPlayer(theTargets[j].GetY());
                EEModifyUnitAttribute(this->mAttribute, this->mUsesPercent, thePlayer, thePlayer->GetUpgrade(theTargets[j].GetX()), this->mAttributeAmount, this->mAttributeMode);
            }
        }
    } else if (this->mAttribute >= kSMUnitAttributeShockArmorPlus) {
        EEUnitClassUpgrade* theUpgrade = this->mAttributePlayer >= 0 && this->mAttributeUnitType != 0 ? gWorld.GetPlayer(this->mAttributePlayer)->GetUpgrade(this->mAttributeUnitType) : NULL;
        EEPlayer* thePlayer = this->mAttributePlayer >= 0 ? gWorld.GetPlayer(this->mAttributePlayer) : NULL;
        EEModifyUnitAttribute(this->mAttribute, this->mUsesPercent, thePlayer, theUpgrade, this->mAttributeAmount, this->mAttributeMode);
    } else {
        EEUnit* theUnit = this->mAttributeUnit != -1 ? gWorld.GetUnit(this->mAttributeUnit) : NULL;
        EEChangeUnit(this->mAttribute, false, theUnit, this->mAttributeAmount, 0, false, this->mAttributeMode);
    }
}

#include "ee/EECalamityClass.h"
#include "ee/EELocationCalamityClass.h"
#include "ee/EEUGCastCalamity.h"
#include "ee/EEUGUnloadTransport.h"
#include "ee/EEAirTrafficControl.h"
#include "ee/EEWUnitsOffMapInfo.h"
#include "ee/EEUnitCalamityClass.h"
#include "ee/EEUCTransport.h"
#include "ee/EEUGGather.h"
#include "ee/EEUGHealUnit.h"
#include "ee/EEUGAttackUnit.h"
#include "ee/EEUGLoadGarrison.h"
#include "ee/EEUGLoadTransport.h"
#include "ee/EEUnitVectorItem.h"

// FUNCTION: EXE 0x5c2f55
// Tasks the scenario object's living units against a second object's units (sources dealt round-robin to targets,
// grouped per player): calamity, gather, attack, heal, garrison, load, repair, drop off, patrol or move, by mTaskAction
// mTaskAction > 6 casts calamity mTaskAction - 6; garrison is 5, load 3; a patrol point is unit type 0xdf. The
// original tested the address of EEUnit::GetComplexUnit instead of calling it, twice, so neither NULL guard worked;
// both now call it.
void EESMEffect::TaskUnitsToObject()
{
    GE3DLine theLine;
    std::vector<EEUnit*> theUnits[16];
    std::vector<std::vector<EEUnit*>*> theBuckets;
    std::vector<EEUnit*> theSources;
    std::vector<EEUnit*> theTargets;
    std::vector<EEComplexUnit*> theLoadUnits;
    std::vector<EEComplexUnit*> theRejected;
    std::vector<long> theNodeIDs;
    std::vector<long> theCalamities;
    EESMObject* theObject = gWorld.mScenarioManager->GetScenarioObject(this->mObject);
    EESMObject* theTargetObject = gWorld.mScenarioManager->GetScenarioObject(this->mTaskObject);
    if (theObject == NULL || theTargetObject == NULL)
        return;
    theObject->Evaluate(false);
    theTargetObject->Evaluate(false);
    unsigned long theCount = theObject->mUnits.size();
    for (unsigned long i = 0; i < theCount; i++) {
        EEUnit* theUnit = gWorld.GetUnit(theObject->mUnits[i]);
        EEComplexUnit* theComplex = theUnit != NULL ? theUnit->GetComplexUnit() : NULL;
        if (theUnit == NULL || theComplex == NULL || theUnit->GetHitPoints() <= 0)
            continue;
        if (theComplex->GetCommandType() == kActionResourcesRotting || theComplex->GetCommandType() == kActionUnitRotting
            || theComplex->GetCommandType() == kActionDead || theComplex->GetCommandType() == kActionResourcesDepleting)
            continue;
        if (theUnit->mTile == NULL
            && (theComplex->mContainer == NULL || theComplex->mContainer->GetUnitClass()->GetUnitType() != kUnitTypeAirport))
            continue;
        if (!theComplex->mIsDisabled && !theComplex->mIsCommandable)
            continue;
        theUnit->GetPlayer()->DetachUnitFromAI(theUnit->GetComplexUnit());
        theComplex->SetDisabled(false, 0);
        theComplex->SetIsCommandable(true);
        theComplex->SetIsDeletable(theComplex->GetUnitClass()->mIsDeletable);
        theSources.push_back(theUnit);
    }
    unsigned long theNumSources = theSources.size();
    unsigned long theNumTargets = theTargetObject->mUnits.size();
    for (unsigned long j = 0; j < theNumTargets; j++) {
        EEUnit* theUnit = gWorld.GetUnit(theTargetObject->mUnits[j]);
        if (theUnit != NULL)
            theTargets.push_back(theUnit);
    }
    if (theNumSources == 0 || theTargets.size() <= 0)
        return;
    unsigned long theNumPairs = theNumSources > theTargets.size() ? theTargets.size() : theNumSources;
    for (unsigned long k = 0; k < theNumPairs; k++) {
        std::vector<EEUnit*>* theBucket = new std::vector<EEUnit*>;
        theBuckets.push_back(theBucket);
    }
    for (unsigned long s = 0; s < theNumSources; s++)
        (theBuckets[s % theNumPairs])->push_back(theSources[s]);
    for (unsigned long t = 0; t < theNumPairs; t++) {
        EEUnit* theTarget = theTargets[t];
        if (theTarget->GetTileX() == -10000 || theTarget->GetTileY() == -10000)
            continue;
        for (long c = 0; c < 16; c++)
            theUnits[c].clear();
        for (unsigned long b = 0; b < (theBuckets[t])->size(); b++) {
            EEUnit* theUnit = *((theBuckets[t])->begin() + b);
            theUnits[gWorld.GetPlayerIndex(theUnit->GetPlayer())].push_back(theUnit);
        }
        for (unsigned long p = 0; p < 16; p++) {
            if (theUnits[p].size() <= 0)
                continue;
            EEPlayer* thePlayer = gWorld.GetPlayer(p);
            if (this->mTaskAction > kSMTaskActionCalamityBase) {
                theCalamities.clear();
                thePlayer->GetTechTree()->GetNodeIDs(theNodeIDs);
                for (unsigned long n = 0; n < theNodeIDs.size(); n++) {
                    EETechTreeNode* theNode = thePlayer->GetTechTree()->Get(theNodeIDs[n]);
                    if (theNode->mData->mCalamity == this->mTaskAction - 6 && theNode->GetIsAvailable())
                        theCalamities.push_back(theNodeIDs[n]);
                }
            }
            EEActiveGroup* theGroup = new EEActiveGroup(thePlayer);
            thePlayer->mPendingGroups.push_back(theGroup);
            unsigned long theNumUnits = theUnits[p].size();
            for (unsigned long u = 0; u < theNumUnits; u++) {
                EEComplexUnit* theComplex = theUnits[p][u]->GetComplexUnit();
                if (theComplex->GetComplexUnit() != NULL && (theComplex->GetUnitClass()->mDomains & kMovementAir)
                    && theComplex->GetCommandType() == kActionIdle) {
                    theComplex->GetComplexUnit()->SetFlightTime(theComplex->GetStats()->mFlightTime);
                    theComplex->GetComplexUnit()->SetAmmunition(theComplex->GetUnitClass()->mAmmunition);
                }
                theGroup->AddMember(theComplex, true);
            }
            EEPlayer* theTargetPlayer = theTarget->GetPlayer();
            EEUnitClass* theTargetClass = theTarget->GetUnitClass();
            EEComplexUnit* theGatherer = theGroup->FindUnitWithFlags(kUnitKindCitizen);
            EEComplexUnit* theCarrier = theGroup->FindUnitWithFlags(kUnitKindNavalCitizen);
            bool theDone = false;
            if (this->mTaskAction == kSMTaskActionAttackMove || this->mTaskAction == kSMTaskActionPatrol)
                goto move;
            if (this->mTaskAction > kSMTaskActionCalamityBase) {
                EECalamityEnum theEnum = (EECalamityEnum)(this->mTaskAction - 6);
                EECalamityClass* theClass = EECalamityManager::GetInstance()->GetCalamityClass(theEnum);
                if (theClass == NULL || theClass->GetTargetKind() != 3)
                    continue;
                EEUnitCalamityClass* theUnitClass = dynamic_cast<EEUnitCalamityClass*>(theClass);
                for (unsigned long u = 0; u < theGroup->GetNumUnits(); u++) {
                    EEComplexUnit* theUnit = theGroup->GetUnit(u);
                    if (theUnitClass == NULL || !theUnitClass->IsValidTarget(theUnit, theTarget))
                        continue;
                    for (unsigned long n = 0; n < theCalamities.size(); n++) {
                        if (thePlayer->GetTechTree()->Get(theCalamities[n])
                                ->Contains(theUnit->GetUnitClass()->GetUnitType())) {
                            EEUGCastCalamity* theGoal = new EEUGCastCalamity(theUnit, theTarget, -10000, -10000,
                                                                             theCalamities[n], true);
                            theUnit->ClearGoals(0);
                            theUnit->SetGoal(theGoal, 0, 0);
                            theUnit->StartAction(kActionPrepareToMove);
                            theUnit->GetUnitClass()->ActionChanged(theUnit);
                            break;
                        }
                    }
                }
                continue;
            }
            if ((theGatherer != NULL
                 && theGatherer->CanGather(theTarget, theTarget->GetTileX(), theTarget->GetTileY(), true, false))
                || (theCarrier != NULL
                    && theCarrier->CanGather(theTarget, theTarget->GetTileX(), theTarget->GetTileY(), true, false))) {
                theGroup->SpreadAroundTile(theTarget->GetTileX(), theTarget->GetTileY());
                theGroup->ClearGoal();
                theGroup->SetGoal(new EEUGGather(theTarget, NULL, true), false);
                theGroup->mAction = kActionGatherResource;
                theGroup->Plan();
                continue;
            }
            if (theTargetPlayer != thePlayer && thePlayer->mEnemies[theTargetPlayer->mPlayerID] != 0
                && theGroup->GetFirstUnit() != NULL && theGroup->GetFirstUnit()->GetUnitClass()->GetCategory() != kUnitCategoryNavalCitizen) {
                if (this->mTaskAction != kSMTaskActionNormal)
                    goto move;
                theGroup->SpreadAroundUnit(theTarget, theTarget->GetTileX(), theTarget->GetTileY());
                theGroup->ClearGoal();
                theGroup->SetGoal(new EEUGAttackUnit(theGroup->GetFirstUnit(), theTarget, true, false), false);
                theGroup->mAction = kActionAttackEnemyUnit;
                theGroup->Plan();
                continue;
            }
            if (theTarget->GetComplexUnit() != NULL && theGroup->GetFirstUnit() != NULL && theTarget->GetComplexUnit() != NULL
                && EECanHeal(theGroup->GetFirstUnit(), theTarget->GetComplexUnit())) {
                for (unsigned long u = 0; u < theGroup->GetNumUnits(); u++) {
                    EEComplexUnit* theUnit = theGroup->GetUnit(u);
                    if (EECanHeal(theUnit, theTarget->GetComplexUnit())) {
                        EEUGHealUnit* theGoal = new EEUGHealUnit(theTarget->GetComplexUnit(), true);
                        theUnit->ClearGoalQueue(0);
                        theUnit->SetGoal(theGoal, 0, 0);
                        theUnit->StartAction(kActionTurnToFaceHealee);
                        theUnit->GetUnitClass()->ActionChanged(theUnit);
                    }
                }
                continue;
            }
            if (theTargetPlayer != thePlayer)
                goto move;
            if (this->mTaskAction == kSMTaskActionGarrison && theTarget->GetComplexUnit() != NULL && theTarget->GetUnitClass()->mGarrisonCapacity > 0) {
                EEComplexUnit* theBuilding = theTarget->GetComplexUnit();
                theGroup->SpreadAroundTile(theTarget->GetTileX(), theTarget->GetTileY());
                theLoadUnits.clear();
                for (unsigned long u = 0; u < theGroup->GetNumUnits(); u++) {
                    EEComplexUnit* theUnit = theGroup->GetUnit(u);
                    theLoadUnits.push_back(theUnit);
                }
                EEUnitGoal* theCurrent = theBuilding->GetGoal();
                if (theLoadUnits.size() <= 0)
                    continue;
                bool theDelete = false;
                EEUGLoadGarrison* theGoal;
                if (theCurrent != NULL && theCurrent->mKind == kGoalLoadGarrison) {
                    theGoal = dynamic_cast<EEUGLoadGarrison*>(theCurrent);
                    theGoal->AddUnits(theLoadUnits);
                } else {
                    theGoal = new EEUGLoadGarrison(theBuilding, theLoadUnits, true);
                    if (theGoal->mIsValid) {
                        theBuilding->ClearGoals(0);
                        theBuilding->SetGoal(theGoal, 0, 0);
                        theBuilding->StartAction(kActionPrepareToMove);
                        theBuilding->GetUnitClass()->ActionChanged(theBuilding);
                    } else
                        theDelete = true;
                }
                if (theGoal->mIsValid) {
                    for (unsigned long n = 0; n < theLoadUnits.size(); n++) {
                        EEComplexUnit* theUnit = theLoadUnits[n];
                        theUnit->ClearGoals(0);
                        theUnit->SetGoal(theGoal->Clone(), 0, 0);
                        theUnit->StartAction(kActionPrepareToMove);
                        theUnit->GetUnitClass()->ActionChanged(theUnit);
                        theDelete = false;
                    }
                }
                if (theDelete && theGoal != NULL)
                    delete theGoal;
                continue;
            }
            EEComplexUnit* theTransport;
            if (this->mTaskAction == kSMTaskActionLoad && (theTransport = theTarget->GetComplexUnit()) != NULL) {
                if (!theTarget->GetUnitClass()->IsTransport())
                    goto move;
                theLoadUnits.clear();
                for (unsigned long u = 0; u < theGroup->GetNumUnits(); u++) {
                    EEComplexUnit* theUnit = theGroup->GetUnit(u);
                    if (theUnit != theTarget)
                        theLoadUnits.push_back(theUnit);
                }
                dynamic_cast<EEUCTransport*>(theTarget->GetUnitClass())->FilterLoadable(&theLoadUnits, theTransport,
                                                                                &theRejected);
                U2DPoint<long> theDestination;
                if (theLoadUnits.size() > 0) {
                    theDone = true;
                    EEUnitGoal* theCurrent = theTransport->GetGoal();
                    EEUGLoadTransport* theGoal;
                    if (theCurrent != NULL && theCurrent->mKind == kGoalLoadTransport) {
                        theGoal = dynamic_cast<EEUGLoadTransport*>(theCurrent);
                        theGoal->AddUnits(theLoadUnits);
                    } else {
                        theGoal = new EEUGLoadTransport(theTransport, theLoadUnits, true);
                        theTransport->ClearGoals(0);
                        theTransport->SetGoal(theGoal, 0, 0);
                        theTransport->StartAction(kActionPrepareToMove);
                        theTransport->GetUnitClass()->ActionChanged(theTransport);
                    }
                    theDestination = theGoal->mUnitTile;
                    for (unsigned long n = 0; n < theLoadUnits.size(); n++) {
                        EEComplexUnit* theUnit = theLoadUnits[n];
                        theUnit->ClearGoals(0);
                        theUnit->SetGoal(theGoal->Clone(), 0, 0);
                        theUnit->StartAction(kActionPrepareToMove);
                        theUnit->GetUnitClass()->ActionChanged(theUnit);
                    }
                } else
                    theDestination.SetXY(theTransport->GetTileX(), theTransport->GetTileY());
                if (theRejected.size() > 0) {
                    theGroup->Finish();
                    for (unsigned long n = 0; n < theRejected.size(); n++)
                        theGroup->AddMember(theRejected[n], true);
                    theGroup->KeepMobileUnits();
                    theGroup->SetGoal(new EEUGLocation(GE2DPoint((float)theDestination.GetX(),
                                                                 (float)theDestination.GetY()),
                                                       true, true, false, 0.0f, false, 0),
                                      false);
                    theGroup->mAction = kActionPrepareToMove;
                    theGroup->Plan();
                }
                if (theDone)
                    continue;
                goto move;
            }
            if (theTargetClass->IsLandBuilding() && theTarget->GetHitPoints() < theTarget->GetStats()->mMaxHitPoints) {
                theGroup->SpreadAroundTile(theTarget->GetTileX(), theTarget->GetTileY());
                unsigned long theNum = theGroup->GetNumUnits();
                for (unsigned long u = 0; u < theNum; u++) {
                    EEComplexUnit* theUnit = theGroup->GetUnit(u);
                    theUnit->OrderRepair(theTarget);
                    theUnit->StartAction(kActionIdle);
                    theUnit->GetUnitClass()->ActionChanged(theUnit);
                }
            } else if (theTargetClass->IsDropSite() || theTargetClass->GetUnitType() == kUnitTypeDock
                       || theTargetClass->GetUnitType() == kUnitTypeNavyYard) {
                theDone = true;
                theGroup->SpreadAroundTile(theTarget->GetTileX(), theTarget->GetTileY());
                theGroup->ClearGoal();
                theGroup->SetGoal(new EEUGGather(NULL, theTarget->GetComplexUnit(), true), false);
                theGroup->mAction = kActionDropoffResource;
                theGroup->Plan();
                if (theGroup->GetNumUnits() == 0)
                    continue;
                EEPlayForLocalPlayer(thePlayer, theGroup->GetFirstUnit()->GetStats()->GetAcknowledgeSound(-1));
                if (theDone)
                    continue;
                goto move;
            } else if (theTargetClass->GetUnitType() == kUnitTypePathPoint) {
                theGroup->KeepMobileUnits();
                EEPPatrolGroup* thePatrolGroup = theTarget->GetComplexUnit()->mPatrolGroup;
                EEUGPatrol* theGoal = new EEUGPatrol(thePatrolGroup, theTarget->GetComplexUnit(), true);
                theGroup->ClearGoal();
                theGroup->SetGoal(theGoal, false);
                theGroup->mAction = kActionMoveToNextPatrolPoint;
                theGroup->Plan();
            } else
                goto move;
            if (theGroup->GetFirstUnit() != NULL)
                EEPlayForLocalPlayer(thePlayer, theGroup->GetFirstUnit()->GetStats()->GetAcknowledgeSound(-1));
            continue;
        move:
            if (theTarget->mTile == NULL)
                continue;
            {
                GE2DPoint thePoint((float)theTarget->GetTileX(), (float)theTarget->GetTileY());
                theGroup->KeepMobileUnits();
                EEComplexUnit* theFirst = theGroup->GetFirstUnit();
                if (theFirst == NULL)
                    continue;
                switch (this->mTaskAction) {
                case kSMTaskActionPatrol: {
                    EEUGPatrol* theGoal = new EEUGPatrol(true);
                    if (theFirst != NULL) {
                        theGoal->mPatrolGroup->AddPoint(GE2DPoint((float)theFirst->GetTileX(), (float)theFirst->GetTileY()), p);
                        theGoal->mPatrolGroup->AddPoint(thePoint, p);
                        theFirst->GetPlayer()->AddPatrolGroup(theGoal->mPatrolGroup);
                    }
                    theGroup->ClearGoal();
                    theGroup->SetGoal(theGoal, false);
                    theGroup->mAction = kActionMoveToNextPatrolPoint;
                    theGroup->Plan();
                    break;
                }
                case kSMTaskActionNormal:
                case kSMTaskActionAttackMove:
                case kSMTaskActionLoad:
                case kSMTaskActionUnload:
                case kSMTaskActionGarrison:
                    if ((theFirst->GetUnitClass()->GetCategory() == kUnitCategoryAirplane || theFirst->GetUnitClass()->GetCategory() == kUnitCategoryPlanetSpaceship)
                        && (theFirst->GetComplexUnit() == NULL
                            || theFirst->GetComplexUnit()->GetCommandType() == kActionIdle)) {
                        for (unsigned long u = 0; u < theGroup->GetNumUnits(); u++) {
                            EEComplexUnit* theUnit = theGroup->GetUnit(u);
                            if (theUnit->mContainer != NULL) {
                                theUnit->ClearGoalQueue(0);
                                theUnit->ClearGoalQueue(1);
                                theUnit->QueueGoal(new EEUGLocation(thePoint, true, this->mTaskAction == kSMTaskActionNormal, false, 0.0f, false, 0), 0);
                                theUnit->QueueGoal(new EEUGLocation(thePoint, true, this->mTaskAction == kSMTaskActionNormal, false, 0.0f, false, 0), 1);
                                theUnit->mGoalQueues[1].AddTime(1);
                            } else if (theUnit->mTile != NULL) {
                                EEUGLocation* theGoal =
                                    new EEUGLocation(thePoint, true, this->mTaskAction == kSMTaskActionNormal, false, 0.0f, false, 0);
                                theUnit->ClearGoals(0);
                                theUnit->SetGoal(theGoal, 0, 0);
                                theUnit->StartAction(kActionTakeoff);
                                theUnit->GetUnitClass()->ActionChanged(theUnit);
                            }
                        }
                    } else {
                        theGroup->ClearGoal();
                        theGroup->SetGoal(new EEUGLocation(thePoint, true, this->mTaskAction == kSMTaskActionNormal, false, 0.0f, false, 0),
                                          false);
                        theGroup->mAction = kActionPrepareToMove;
                        theGroup->Plan();
                    }
                    break;
                }
            }
        }
    }
    DeleteVectorContents(theBuckets);
}


// FUNCTION: EXE 0x5c42d2
// Sends the scenario object's living units to a random tile of the task area (move, patrol, unload or calamity per
// player); off-map units are placed in the area, and with no area set every unit is taken off the map instead
// mTaskAction: 0/2 move, 1 patrol, 4 unload, > 6 cast calamity mTaskAction - 6. EESMArea::IsUnset is true for an
// area with no anchor. The original tested the address of EEUnit::GetComplexUnit instead of calling it, so the
// NULL guard never worked; it now calls it.
void EESMEffect::TaskUnitsToArea()
{
    std::vector<long> theNodeIDs;
    std::vector<long> theCalamities;
    std::vector<U2DPoint<long> > theTiles;
    GE2DPoint theTarget;
    GE2DPoint theLocation;
    GETransformation theTransformation;
    std::vector<EEUnit*> theUnits[16];
    EESMArea* theArea = gWorld.mScenarioManager->GetArea(this->mTaskArea);
    EESMObject* theObject = gWorld.mScenarioManager->GetScenarioObject(this->mObject);
    float theRoll = 0.0f;
    float thePitch = 0.0f;
    if (theObject != NULL && theArea != NULL) {
        bool theUnset = theArea->IsUnset();
        if (!theUnset && !theArea->IsOffMap()) {
            theArea->GetTiles(theTiles);
            // An area with no tiles (e.g. anchored to a unit that is gone) has no destination
            if (theTiles.empty())
                return;
            long theIndex = gPhysics->GetRandom(0L, (long)theTiles.size() - 1);
            theLocation.SetX((float)(theTiles[theIndex]).GetX());
            theLocation.SetY((float)(theTiles[theIndex]).GetY());
            theTarget = theLocation;
            gWorld.mMap->GetTile((long)theTarget.GetX(), (long)theTarget.GetY());
        }
        theObject->Evaluate(false);
        unsigned long theCount = theObject->mUnits.size();
        for (unsigned long i = 0; i < theCount; i++) {
            EEUnit* theUnit = gWorld.GetUnit(theObject->GetUnitAt(i));
            EEComplexUnit* theComplex = theUnit != NULL ? theUnit->GetComplexUnit() : NULL;
            if (theUnit == NULL || theComplex == NULL || theUnit->GetHitPoints() <= 0)
                continue;
            if (theComplex->GetCommandType() == kActionResourcesRotting || theComplex->GetCommandType() == kActionUnitRotting
                || theComplex->GetCommandType() == kActionDead || theComplex->GetCommandType() == kActionResourcesDepleting)
                continue;
            if (!theComplex->mIsDisabled && !theComplex->mIsCommandable)
                continue;
            theUnit->GetPlayer()->DetachUnitFromAI(theComplex);
            theComplex->SetDisabled(false, 0);
            theComplex->SetIsCommandable(true);
            theComplex->SetIsDeletable(theComplex->GetUnitClass()->mIsDeletable);
            if (theUnset) {
                if (!theComplex->GetUnitClass()->IsLandBuilding()) {
                    if (theComplex->GetUnitClass()->IsLandBuilding()) {
                        for (EEComplexUnit* theLinked = theComplex; theLinked != NULL; theLinked = theLinked->mNextPart)
                            theLinked->Set2DPosition(U2DPoint<float>((float)theLinked->GetTileX(), (float)theLinked->GetTileY()));
                    } else {
                        theTransformation = theComplex->mPhysicalModel.GetLocation();
                        theComplex->Set2DPosition(U2DPoint<float>(theTransformation.GetX(), theTransformation.GetY()));
                    }
                    if (theUnit->GetUnitClass()->mAirTrafficType != 0 && theComplex->GetUnitClass()->IsAirBase())
                        theComplex->GetPlayer()->mAirTraffic[theUnit->GetUnitClass()->mAirTrafficType - 1]->RemoveAirport(theComplex);
                    if (theComplex->GetPlayer() == gWorld.GetCurrentPlayer()
                        && theComplex->GetPlayer()->mActiveGroup->Contains(theComplex))
                        gUserInterface->PostMessage424Once();
                    theComplex->TakeOffMap();
                    EEWUnitsOffMapInfo* theInfo = theComplex->GetPlayer()->GetOffMapInfo(theComplex);
                    if (theInfo != NULL)
                        theInfo->mReason = 2;
                } else {
                    theTransformation = theComplex->mPhysicalModel.GetLocation();
                    theComplex->mLastMapPosition = GE3DPoint(theTransformation);
                    theComplex->Set2DPosition(U2DPoint<float>(theTransformation.GetX(), theTransformation.GetY()));
                }
                if (theUnit->GetUnitClass()->mAirTrafficType != 0 && theComplex->GetUnitClass()->IsAirBase())
                    theComplex->GetPlayer()->mAirTraffic[theUnit->GetUnitClass()->mAirTrafficType - 1]->RemoveAirport(theComplex);
                if (theComplex->GetPlayer() == gWorld.GetCurrentPlayer()
                    && theComplex->GetPlayer()->mActiveGroup->Contains(theComplex))
                    gUserInterface->PostMessage424Once();
            } else if ((theUnit->GetTileX() != -10000 && theUnit->GetTileY() != -10000)
                       || ((theUnit->GetUnitClass()->GetCategory() == kUnitCategoryAirplane || theUnit->GetUnitClass()->GetCategory() == kUnitCategoryPlanetSpaceship)
                           && theComplex->mContainer != NULL)) {
                theUnits[gWorld.GetPlayerIndex(theUnit->GetPlayer())].push_back(theUnit);
            } else {
                unsigned long theSaved68 = theObject->mInAreaCheck;
                unsigned long theSaved3C = theObject->mArea;
                unsigned long theSaved6C = theObject->mNearCheck;
                theObject->mInAreaCheck = 1;
                theObject->mArea = this->mTaskArea;
                theObject->mNearCheck = 0;
                if (theArea->IsOffMap()
                    || theObject->FindPlacement(theUnit->GetUnitClass()->GetUnitType(), theLocation, true)) {
                    theTransformation = theComplex->mPhysicalModel.GetLocation();
                    if (theArea->IsOffMap()) {
                        theTransformation.SetX(theComplex->mU0DC.GetX());
                        theTransformation.SetY(theComplex->mU0DC.GetY());
                    } else if (theLocation.GetX() != -10000.0f) {
                        theTransformation.SetX(theLocation.GetX() + 0.5f);
                        theTransformation.SetY(theLocation.GetY() + 0.5f);
                        long theX = (long)theLocation.GetX();
                        EETile* theTile = gWorld.mMap->GetTile(theX, (long)theLocation.GetY());
                        if (theTile != NULL) {
                            if (!(theUnit->GetUnitClass()->mDomains & kMovementAir))
                                theTransformation.mMatrix[2][3] = theTile->GetHeight();
                            else if (theComplex->mPhysicalModel.GetLocation().GetZ() < theTile->GetHeight())
                                theTransformation.mMatrix[2][3] = theUnit->GetUnitClass()->GetCruiseAltitude();
                        }
                    } else {
                        theTransformation.SetX(-10000.0f);
                        theTransformation.SetY(-10000.0f);
                    }
                    if (theTransformation.GetX() != -10000.0f) {
                        GETransformation theGround(false);
                        theGround.MakeIdentity();
                        theGround.SetPosition(GE3DPoint(theTransformation));
                        if (theUnit->GetUnitClass()->mPitchesWithTerrain)
                            thePitch = gWorld.mMap->GetTerrainPitch(theGround);
                        if (theUnit->GetUnitClass()->mRollsWithTerrain)
                            theRoll = gWorld.mMap->GetTerrainRoll(theGround);
                        theTransformation.SetOrientationYPR(
                            UMArcTan2(theComplex->mPhysicalModel.GetLocation().mMatrix[1][0],
                                      theComplex->mPhysicalModel.GetLocation().mMatrix[0][0]),
                            thePitch, theRoll);
                        if (theComplex->mContainer == NULL)
                            theComplex->SetTransformation(theTransformation, true);
                        else
                            theComplex->mContainer->UnloadCargoByID(theComplex->mUnitID, theTransformation);
                        long theSize = theUnit->GetUnitClass()->GetTileSize();
                        if (theSize > 1) {
                            float theOffset = theSize * 0.5f;
                            theTransformation.SetX(theTransformation.GetX() + theOffset);
                            theTransformation.SetY(theTransformation.GetY() + theOffset);
                            theComplex->mPhysicalModel.SetLocation(theTransformation);
                        }
                        if (theUnit->GetUnitClass()->mAirTrafficType != 0 && theComplex->GetUnitClass()->IsAirBase())
                            theComplex->GetPlayer()->mAirTraffic[theUnit->GetUnitClass()->mAirTrafficType - 1]->AddAirport(theComplex);
                        if ((theUnit->GetUnitClass()->GetCategory() == kUnitCategoryAirplane || theUnit->GetUnitClass()->GetCategory() == kUnitCategoryPlanetSpaceship)
                            && (theUnit->GetComplexUnit()->mFlightTime != 0 || theUnit->GetComplexUnit()->mHomeBase != NULL))
                            theComplex->StartAction(kActionCircle);
                        else
                            theComplex->StartAction(kActionIdle);
                        theUnit->GetUnitClass()->ActionChanged(theComplex);
                    }
                }
                theObject->mInAreaCheck = theSaved68;
                theObject->mArea = theSaved3C;
                theObject->mNearCheck = theSaved6C;
            }
        }
        for (long p = 0; p < 16 && !theUnset && !theArea->IsOffMap(); p++) {
            if (theUnits[p].size() <= 0)
                continue;
            EEPlayer* thePlayer = gWorld.GetPlayer(p);
            if (this->mTaskAction > kSMTaskActionCalamityBase) {
                theCalamities.clear();
                thePlayer->GetTechTree()->GetNodeIDs(theNodeIDs);
                for (unsigned long n = 0; n < theNodeIDs.size(); n++) {
                    EETechTreeNode* theNode = thePlayer->GetTechTree()->Get(theNodeIDs[n]);
                    if (theNode->mData->mCalamity == this->mTaskAction - 6 && theNode->GetIsAvailable())
                        theCalamities.push_back(theNodeIDs[n]);
                }
            }
            EEActiveGroup* theGroup = new EEActiveGroup(thePlayer);
            thePlayer->mPendingGroups.push_back(theGroup);
            unsigned long theNumUnits = theUnits[p].size();
            for (unsigned long u = 0; u < theNumUnits; u++)
                theGroup->AddMember(theUnits[p][u]->mComplexUnit, true);
            theGroup->KeepMobileUnits();
            if (theGroup->GetNumUnits() == 0)
                continue;
            EEComplexUnit* theFirst = theGroup->GetFirstUnit();
            switch (this->mTaskAction) {
            case kSMTaskActionNormal:
            case kSMTaskActionAttackMove: {
                if ((theFirst->GetUnitClass()->GetCategory() == kUnitCategoryAirplane || theFirst->GetUnitClass()->GetCategory() == kUnitCategoryPlanetSpaceship)
                    && (theFirst->GetComplexUnit() == NULL || theFirst->GetComplexUnit()->GetCommandType() == kActionIdle)) {
                    for (unsigned long u = 0; u < theGroup->GetNumUnits(); u++) {
                        EEComplexUnit* theUnit = theGroup->GetUnit(u);
                        EEUGLocation* theGoal = new EEUGLocation(theTarget, true, this->mTaskAction == kSMTaskActionNormal, false, 0.0f, false, 0);
                        theUnit->ClearGoals(0);
                        theUnit->SetGoal(theGoal, 0, 0);
                        if (theUnit->mContainer == NULL) {
                            theUnit->StartAction(kActionTakeoff);
                            theUnit->GetUnitClass()->ActionChanged(theUnit);
                        }
                    }
                } else {
                    theGroup->ClearGoal();
                    theGroup->SetGoal(new EEUGLocation(theTarget, true, this->mTaskAction == kSMTaskActionNormal, false, 0.0f, false, 0), false);
                    theGroup->mAction = kActionPrepareToMove;
                    theGroup->Plan();
                }
                break;
            }
            case kSMTaskActionPatrol: {
                EEUGPatrol* theGoal = new EEUGPatrol(true);
                if (theFirst != NULL && theFirst->GetTileX() != -10000 && theFirst->GetTileY() != -10000) {
                    theGoal->mPatrolGroup->AddPoint(GE2DPoint((float)theFirst->GetTileX(), (float)theFirst->GetTileY()), p);
                    theGoal->mPatrolGroup->AddPoint(theTarget, p);
                    theFirst->GetPlayer()->AddPatrolGroup(theGoal->mPatrolGroup);
                }
                theGroup->ClearGoal();
                theGroup->SetGoal(theGoal, false);
                theGroup->mAction = kActionMoveToNextPatrolPoint;
                theGroup->Plan();
                break;
            }
            case kSMTaskActionUnload: {
                for (unsigned long u = 0; u < theGroup->GetNumUnits(); u++) {
                    EEComplexUnit* theUnit = theGroup->GetUnit(u);
                    EEUGUnloadTransport* theGoal =
                        new EEUGUnloadTransport(theUnit, (long)theTarget.GetX(), (long)theTarget.GetY(), true);
                    if (theUnit->GetUnitClass()->IsTransport() && theUnit->GetNumCargoUnits() > 0
                        && theGoal->CanUnloadAtDestination()) {
                        theUnit->ClearGoals(0);
                        theUnit->SetGoal(theGoal, 0, 0);
                    } else {
                        delete theGoal;
                        theUnit->ClearGoals(0);
                        theUnit->SetGoal(new EEUGLocation(theTarget.GetX(), theTarget.GetY(), 0.0f, true, true, false,
                                                          0.0f, false, 0),
                                         0, 0);
                    }
                    theUnit->StartAction(kActionPrepareToMove);
                    theUnit->GetUnitClass()->ActionChanged(theUnit);
                }
                break;
            }
            default:
                if (this->mTaskAction > kSMTaskActionCalamityBase) {
                    EECalamityEnum theEnum = (EECalamityEnum)(this->mTaskAction - 6);
                    EECalamityClass* theClass = EECalamityManager::GetInstance()->GetCalamityClass(theEnum);
                    if (theClass == NULL || theClass->GetTargetKind() != 2)
                        break;
                    EELocationCalamityClass* theLocationClass = dynamic_cast<EELocationCalamityClass*>(theClass);
                    if (theLocationClass == NULL)
                        break;
                    for (unsigned long u = 0; u < theGroup->GetNumUnits(); u++) {
                        EEComplexUnit* theUnit = theGroup->GetUnit(u);
                        if (!theLocationClass->IsValidLocation(theUnit, (long)theTarget.GetX(), (long)theTarget.GetY()))
                            continue;
                        for (unsigned long n = 0; n < theCalamities.size(); n++) {
                            if (!thePlayer->GetTechTree()->Get(theCalamities[n])
                                    ->Contains(theUnit->GetUnitClass()->GetUnitType()))
                                continue;
                            EEUGCastCalamity* theGoal =
                                new EEUGCastCalamity(theUnit, NULL, (long)theTarget.GetX(),
                                                     (long)theTarget.GetY(), theCalamities[n], true);
                            theUnit->ClearGoals(0);
                            theUnit->SetGoal(theGoal, 0, 0);
                            theUnit->StartAction(kActionPrepareToMove);
                            theUnit->GetUnitClass()->ActionChanged(theUnit);
                            break;
                        }
                    }
                }
                break;
            }
        }
    }
    gWorld.UpdateActiveUnits();
}

// TEMPLATE: EXE 0x5c42ce// ?GetComplexUnit@EEUnit@@QBEPAVEEComplexUnit@@XZ (ICF-folded with EECCLeprosy::GetContagionFunctor)
// TEMPLATE: EXE 0x5c5126
// ?push_back@?$vector@PAV?$vector@PAVEEUnit@@V?$allocator@PAVEEUnit@@@std@@@std@@V?$allocator@PAV?$vector@PAVEEUnit@@V?$allocator@PAVEEUnit@@@std@@@std@@@2@@std@@QAEXABQAV?$vector@PAVEEUnit@@V?$allocator@PAVEEUnit@@@std@@@2@@Z
// TEMPLATE: EXE 0x5c5152
// ?GetNumMessages@EEScenarioManager@@QAEKXZ
// TEMPLATE: EXE 0x5c516b
// ?GetPlayer@RTSWorld@@QBEPAVEEPlayer@@K@Z
// TEMPLATE: EXE 0x5c5176
// ?sort@std@@YAXV?$_Ptrit@PAVEEUnit@@HPAPAV1@AAPAV1@PAPAV1@AAPAV1@@1@0UEESMUnitPriorityLess@@@Z
// TEMPLATE: EXE 0x5c5233
// ?_Insertion_sort@std@@YAXV?$_Ptrit@PAVEEUnit@@HPAPAV1@AAPAV1@PAPAV1@AAPAV1@@1@0UEESMUnitPriorityLess@@@Z
// TEMPLATE: EXE 0x5c525c
// ?_Sort@std@@YAXV?$_Ptrit@PAVEEUnit@@HPAPAV1@AAPAV1@PAPAV1@AAPAV1@@1@0HUEESMUnitPriorityLess@@@Z
// TEMPLATE: EXE 0x5c532f
// ?_Unguarded_insertion_sort@std@@YAXV?$_Ptrit@PAVEEUnit@@HPAPAV1@AAPAV1@PAPAV1@AAPAV1@@1@0UEESMUnitPriorityLess@@PAPAVEEUnit@@@Z
// TEMPLATE: EXE 0x5c5360
// ?_Insertion_sort_1@std@@YAXV?$_Ptrit@PAVEEUnit@@HPAPAV1@AAPAV1@PAPAV1@AAPAV1@@1@0UEESMUnitPriorityLess@@PAPAVEEUnit@@@Z
// TEMPLATE: EXE 0x5c53fd
// ?make_heap@std@@YAXV?$_Ptrit@PAVEEUnit@@HPAPAV1@AAPAV1@PAPAV1@AAPAV1@@1@0UEESMUnitPriorityLess@@@Z
// TEMPLATE: EXE 0x5c5437
// ?sort_heap@std@@YAXV?$_Ptrit@PAVEEUnit@@HPAPAV1@AAPAV1@PAPAV1@AAPAV1@@1@0UEESMUnitPriorityLess@@@Z
// TEMPLATE: EXE 0x5c5478
// ?_Unguarded_partition@std@@YA?AV?$_Ptrit@PAVEEUnit@@HPAPAV1@AAPAV1@PAPAV1@AAPAV1@@1@V21@0UEESMUnitPriorityLess@@PAPAVEEUnit@@@Z
// TEMPLATE: EXE 0x5c5598
// ?_Unguarded_insert@std@@YAXV?$_Ptrit@PAVEEUnit@@HPAPAV1@AAPAV1@PAPAV1@AAPAV1@@1@PAVEEUnit@@UEESMUnitPriorityLess@@@Z
// TEMPLATE: EXE 0x5c55cb
// ?_Make_heap@std@@YAXV?$_Ptrit@PAVEEUnit@@HPAPAV1@AAPAV1@PAPAV1@AAPAV1@@1@0UEESMUnitPriorityLess@@PAHPAPAVEEUnit@@@Z
// TEMPLATE: EXE 0x5c560f
// ?pop_heap@std@@YAXV?$_Ptrit@PAVEEUnit@@HPAPAV1@AAPAV1@PAPAV1@AAPAV1@@1@0UEESMUnitPriorityLess@@@Z
// TEMPLATE: EXE 0x5c5638
// ?_Adjust_heap@std@@YAXV?$_Ptrit@PAVEEUnit@@HPAPAV1@AAPAV1@PAPAV1@AAPAV1@@1@HHPAVEEUnit@@UEESMUnitPriorityLess@@@Z
// TEMPLATE: EXE 0x5c56a6
// ?_Pop_heap_0@std@@YAXV?$_Ptrit@PAVEEUnit@@HPAPAV1@AAPAV1@PAPAV1@AAPAV1@@1@0UEESMUnitPriorityLess@@PAPAVEEUnit@@@Z
// TEMPLATE: EXE 0x5c56f9
// ?_Push_heap@std@@YAXV?$_Ptrit@PAVEEUnit@@HPAPAV1@AAPAV1@PAPAV1@AAPAV1@@1@HHPAVEEUnit@@UEESMUnitPriorityLess@@@Z
// TEMPLATE: EXE 0x5c5748
// ?_Pop_heap@std@@YAXV?$_Ptrit@PAVEEUnit@@HPAPAV1@AAPAV1@PAPAV1@AAPAV1@@1@00PAVEEUnit@@UEESMUnitPriorityLess@@PAH@Z
// TEMPLATE: EXE 0x5c577c
// ?GetNewName@EESMEffect@@QBEABVUWideString@@XZ (EESMEffect.h, out of line from SetClassName)
