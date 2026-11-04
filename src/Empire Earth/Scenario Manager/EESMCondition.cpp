// C:\EEX\Empire Earth\Scenario Manager\EESMCondition.cpp (file name inferred: no __FILE__): 0x1ba89f..0x1bb5a5,
// one scenario trigger condition. Unit flags: the exe module set.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/UWideString.h"
#include "ee/EESMCondition.h"
#include <vector>
#include <mbstring.h>
#include "lle/UResourceFile.h"
#include "ee/EmpireEarth.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEScenarioManager.h"
#include "ee/EESMObject.h"
#include "ee/EESMTrigger.h"
#include "ee/EEPhysics.h"
#include "ee/EEComputerPlayer.h"

static const float kSMCHoursPerDay = -24.0f;    // 0x44605c (Evaluate's time of day; unit .rdata; provisional name)

// FUNCTION: EXE 0x5ba89f
// Creates a player-attribute condition with default players, attribute and operator, and no trigger or time tests
EESMCondition::EESMCondition()
{
    this->SetName("");
    this->SetText("");
    this->mType = kSMConditionPlayerAttribute;
    this->mObjectIndex = 0;
    this->mPlayer = 7;
    this->mAttribute = 2;
    this->mOperator = 2;
    this->mAmount = 0;
    this->mOtherPlayer = 8;
    this->mOtherAttribute = 2;
    this->mTechnology = kTechEpochPrehistoric;
    this->mTrigger = 0;
    this->mIsTriggerOn = false;
    this->mHasTriggerFired = false;
    this->mUsesPlayerAttribute = false;
    this->mMaxTime = 0;
    this->mMinTime = 0;
    this->mHour = 0;
    this->mUsesDifficulty = false;
    this->mUsesElapsedTime = false;
    this->mUsesHour = false;
    this->mDifficulty = 1;
    this->mDiplomacy = 0;
    this->mID = 0;
}

// FUNCTION: EXE 0x5ba976
// Destroys the condition (members only)
EESMCondition::~EESMCondition()
{
}

// FUNCTION: EXE 0x5ba998
// Returns whether the value compares to the target under the operator (<, <=, ==, >=, >); false for any other
bool EESMCondition::Compare(unsigned long inValue, unsigned long inOperator, unsigned long inTarget)
{
    bool theResult = false;
    switch (inOperator) {
    case kSMCompareLessThan:
        theResult = inValue < inTarget;
        break;
    case kSMCompareLessOrEqual:
        theResult = inValue <= inTarget;
        break;
    case kSMCompareEqual:
        theResult = inValue == inTarget;
        break;
    case kSMCompareGreaterOrEqual:
        theResult = inValue >= inTarget;
        break;
    case kSMCompareGreater:
        theResult = inValue > inTarget;
        break;
    }
    return theResult;
}

// FUNCTION: EXE 0x5ba9e7
// Copies every setting of another condition, including its name, texts and ID
void EESMCondition::Copy(const EESMCondition& inCondition)
{
    this->mName = inCondition.mName;
    this->mText = inCondition.mText;
    this->mChatText = inCondition.mChatText;
    this->mType = inCondition.mType;
    this->mObjectIndex = inCondition.mObjectIndex;
    this->mPlayer = inCondition.mPlayer;
    this->mAttribute = inCondition.mAttribute;
    this->mOperator = inCondition.mOperator;
    this->mAmount = inCondition.mAmount;
    this->mOtherPlayer = inCondition.mOtherPlayer;
    this->mOtherAttribute = inCondition.mOtherAttribute;
    this->mTechnology = inCondition.mTechnology;
    this->mTrigger = inCondition.mTrigger;
    this->mIsTriggerOn = inCondition.mIsTriggerOn;
    this->mHasTriggerFired = inCondition.mHasTriggerFired;
    this->mUsesPlayerAttribute = inCondition.mUsesPlayerAttribute;
    this->mMaxTime = inCondition.mMaxTime;
    this->mMinTime = inCondition.mMinTime;
    this->mHour = inCondition.mHour;
    this->mUsesDifficulty = inCondition.mUsesDifficulty;
    this->mUsesElapsedTime = inCondition.mUsesElapsedTime;
    this->mUsesHour = inCondition.mUsesHour;
    this->mDifficulty = inCondition.mDifficulty;
    this->mDiplomacy = inCondition.mDiplomacy;
    this->mID = inCondition.mID;
}

// FUNCTION: EXE 0x5baabf
// Returns whether the condition holds: object exists, player attribute/technology/diplomacy for any or all chosen
// players, trigger on/fired, elapsed time/hour/difficulty, or the last chat text containing a phrase
// NEAR: the player loops' flag shapes are a first decode (the diplomacy all/all test matches the original's
// i == 0 && j == 0 seed at 0x5bac3b)
bool EESMCondition::Evaluate()
{
    std::vector<long> thePlayers;
    std::vector<long> theOthers;
    EESMObject* theObject = gWorld.mScenarioManager->GetScenarioObject(this->mObjectIndex);
    EESMTrigger* theTrigger = gWorld.mScenarioManager->GetTrigger(this->mTrigger);
    bool theResult = false;
    switch (this->GetType()) {
    case kSMConditionAlwaysTrue:
        theResult = true;
        break;
    case kSMConditionObject:
        if (theObject != NULL)
            theResult = theObject->Evaluate(0);
        else
            theResult = false;
        break;
    case kSMConditionPlayerAttribute:
    case kSMConditionPlayerTechnology:
    case kSMConditionPlayerDiplomacy: {
        bool theAny = gWorld.mScenarioManager->ChoosePlayer(this->mPlayer, &thePlayers, false) == -1;
        theResult = false;
        for (unsigned char i = 0; i < thePlayers.size(); i++) {
            bool thePlayerResult = false;
            if (theAny && theResult)
                break;
            if (this->GetType() == kSMConditionPlayerDiplomacy) {
                EEPlayer* thePlayer = gWorld.mPlayers[thePlayers[i]];
                if (thePlayer != NULL) {
                    bool theAnyOther = gWorld.mScenarioManager->ChoosePlayer(this->mOtherPlayer, &theOthers, false) == -1;
                    for (unsigned char j = 0; j < theOthers.size(); j++) {
                        if (theAnyOther) {
                            thePlayerResult = thePlayer->mEnemies[theOthers[j]] == (long)this->mDiplomacy;
                            if (thePlayerResult)
                                break;
                        } else {
                            if ((thePlayerResult || j == 0)
                                && thePlayer->mEnemies[theOthers[j]] == (long)this->mDiplomacy) {
                                thePlayerResult = true;
                                continue;
                            }
                            thePlayerResult = false;
                        }
                    }
                }
            } else if (this->GetType() == kSMConditionPlayerAttribute) {
                unsigned long theValue = this->GetPlayerValue(thePlayers[i], this->mAttribute);
                if (this->mUsesPlayerAttribute == true) {
                    long theOther = gWorld.GetScenarioManager()->ChoosePlayer(this->mOtherPlayer, &theOthers, false);
                    thePlayerResult = false;
                    for (unsigned char j = 0; j < theOthers.size(); j++) {
                        unsigned long theOtherValue = this->GetPlayerValue(theOthers[j], this->mOtherAttribute);
                        if (theOther == -1) {
                            thePlayerResult = this->Compare(theValue, this->mOperator, theOtherValue);
                            if (thePlayerResult)
                                break;
                        } else if ((!thePlayerResult && j != 0) || !this->Compare(theValue, this->mOperator, theOtherValue))
                            thePlayerResult = false;
                        else
                            thePlayerResult = true;
                    }
                } else
                    thePlayerResult = this->Compare(theValue, this->mOperator, this->mAmount);
            } else if (this->GetType() == kSMConditionPlayerTechnology) {
                EEPlayer* thePlayer = gWorld.mPlayers[thePlayers[i]];
                if (thePlayer != NULL) {
                    long theTech = this->mTechnology;
                    if (theTech < kTechEpochAll)
                        thePlayerResult = theTech <= thePlayer->GetTechTree()->mCurrentEpoch;
                    else {
                        EETechTreeNode* theNode = thePlayer->GetTechTree()->Get(theTech);
                        if (theNode != NULL && (theNode->mData->mMakesUnit || theNode->mIsResearched)) {
                            bool theResearched = theNode->mIsResearched;
                            theNode->mIsResearched = false;
                            thePlayerResult = theNode->GetIsAvailable();
                            theNode->mIsResearched = theResearched;
                        }
                    }
                }
            }
            if (theAny) {
                theResult = thePlayerResult;
                if (thePlayerResult)
                    break;
            } else {
                if ((theResult || i == 0) && thePlayerResult)
                    theResult = true;
                else
                    theResult = false;
            }
        }
        break;
    }
    case kSMConditionTriggerAttribute:
        theResult = true;
        // a missing trigger has neither fired nor is on (it used to be read through NULL)
        if (this->mHasTriggerFired)
            theResult = theTrigger != NULL && theTrigger->mHasFired;
        if (this->mIsTriggerOn)
            theResult = theTrigger != NULL && theTrigger->mIsOn;
        break;
    case kSMConditionGameAttribute: {
        bool theTimeResult = true;
        if (this->mUsesElapsedTime) {
            float theTime = gWorld.mScenarioManager->mTime * 0.001f;
            if (theTime < (float)this->mMinTime || (float)this->mMaxTime < theTime)
                theResult = false;
            else
                theResult = true;
            theTimeResult = theResult;
        }
        if (this->mUsesHour && theTimeResult) {
            unsigned long theHour = (long)(gWorld.mTimeOfDay * kSMCHoursPerDay);
            theTimeResult = !((30 - theHour) % 24 < this->mHour);
            theResult = theTimeResult;
        }
        if (this->mUsesDifficulty && theTimeResult)
            theResult = gDifficultyLevel == (long)this->mDifficulty;
        break;
    }
    case kSMConditionChatMessageContains: {
        UWideString theText(this->mChatText);
        UWideString theChat(gWorld.mScenarioManager->mLastChatText);
        _mbslwr((unsigned char*)(const char*)theText);
        _mbslwr((unsigned char*)(const char*)theChat);
        theResult = theChat.Find(theText, 0) != theChat.GetLength();
        break;
    }
    }
    return theResult;
}

// FUNCTION: EXE 0x5baf78
// Returns a player's value for a condition: a resource (any: a random one), population, units, difficulty, scenario
// variable, tribute sent/received, statistic or mining limit; 0 without a player
unsigned long EESMCondition::GetPlayerValue(long inPlayer, unsigned long inType)
{
    EEPlayer* thePlayer = gWorld.mPlayers[inPlayer];
    unsigned long theValue = 0;
    if (thePlayer == NULL)
        return theValue;
    switch (inType) {
    case kSMPlayerValueAnyResource:
        theValue = this->GetPlayerValue(inPlayer, gPhysics->GetRandom(2L, 6L));
        break;
    case kSMPlayerValueAllResources:
        theValue = thePlayer->mResourceCaps[kResourceIron] + thePlayer->mResourceCaps[kResourceGold] + thePlayer->mResourceCaps[kResourceStone]
                   + thePlayer->mResourceCaps[kResourceWood] + thePlayer->mResourceCaps[kResourceFood];
        break;
    case kSMPlayerValueStone:
        theValue = thePlayer->mResourceCaps[kResourceStone];
        break;
    case kSMPlayerValueGold:
        theValue = thePlayer->mResourceCaps[kResourceGold];
        break;
    case kSMPlayerValueWood:
        theValue = thePlayer->mResourceCaps[kResourceWood];
        break;
    case kSMPlayerValueFood:
        theValue = thePlayer->mResourceCaps[kResourceFood];
        break;
    case kSMPlayerValueIron:
        theValue = thePlayer->mResourceCaps[kResourceIron];
        break;
    case kSMPlayerValueCurrentPop:
        theValue = thePlayer->GetTotalPopulation(false, false);
        break;
    case kSMPlayerValuePopCap:
        theValue = thePlayer->GetPopulationLimit(0);
        break;
    case kSMPlayerValueUnitCount:
        theValue = thePlayer->mUnitCount;
        break;
    case kSMPlayerValueDifficultyLevel:
        if (thePlayer->mComputerPlayer != NULL)
            theValue = thePlayer->mComputerPlayer->mDifficulty;
        break;
    case kSMPlayerValueVariable0:
    case kSMPlayerValueVariable1:
    case kSMPlayerValueVariable2:
    case kSMPlayerValueVariable3:
    case kSMPlayerValueVariable4:
    case kSMPlayerValueVariable5:
    case kSMPlayerValueVariable6:
    case kSMPlayerValueVariable7:
    case kSMPlayerValueVariable8:
    case kSMPlayerValueVariable9:
        theValue = thePlayer->mScenarioVariables[inType - 11];
        break;
    case kSMPlayerValueStoneSent:
        theValue = thePlayer->mTributes[2];
        break;
    case kSMPlayerValueStoneReceived:
        theValue = thePlayer->mTributes[7];
        break;
    case kSMPlayerValueGoldSent:
        theValue = thePlayer->mTributes[3];
        break;
    case kSMPlayerValueGoldReceived:
        theValue = thePlayer->mTributes[8];
        break;
    case kSMPlayerValueWoodSent:
        theValue = thePlayer->mTributes[1];
        break;
    case kSMPlayerValueWoodReceived:
        theValue = thePlayer->mTributes[6];
        break;
    case kSMPlayerValueFoodSent:
        theValue = thePlayer->mTributes[0];
        break;
    case kSMPlayerValueFoodReceived:
        theValue = thePlayer->mTributes[5];
        break;
    case kSMPlayerValueIronSent:
        theValue = thePlayer->mTributes[4];
        break;
    case kSMPlayerValueIronReceived:
        theValue = thePlayer->mTributes[9];
        break;
    default:
        if (inType - 30 < 31)
            theValue = thePlayer->mSavedGroups.GetLastValue(inType - 30);
        else if (inType == kSMPlayerValueMiningLimit)
            theValue = thePlayer->mMiningLimit;
        break;
    }
    return theValue;
}

// FUNCTION: EXE 0x5bb154
// Reads the condition (diplomacy above 1 becomes 1); returns the first error, ignoring the chat text's
long EESMCondition::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    long theResult = this->mName.Load(inFile);
    if (theResult == kErrorOK)
        theResult = this->mText.Load(inFile);
    if (theResult == kErrorOK) {
        this->mChatText.Load(inFile);
        theResult = inFile.Read(this->mType);
    }
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mObjectIndex);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mPlayer);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mAttribute);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mOperator);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mAmount);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mOtherPlayer);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mOtherAttribute);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mTechnology);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mTrigger);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mIsTriggerOn);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mHasTriggerFired);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mUsesPlayerAttribute);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mMaxTime);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mMinTime);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mHour);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mUsesDifficulty);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mUsesElapsedTime);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mUsesHour);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mDifficulty);
    if (theResult == kErrorOK) {
        theResult = inFile.Read(this->mDiplomacy);
        if (this->mDiplomacy >= 2)
            this->mDiplomacy = 1;
    }
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mID);
    return theResult;
}

// FUNCTION: EXE 0x5bb326
// Updates the condition's object index after objects inOld/inNew swap, or after inOld is deleted (inNew -1)
void EESMCondition::RenumberObjects(unsigned long inOldIndex, unsigned long inNewIndex)
{
    unsigned long theIndex = this->mObjectIndex;
    if (theIndex == inOldIndex) {
        this->mObjectIndex = (inNewIndex != 0xffffffff) ? inNewIndex : 0;
    } else if (theIndex == inNewIndex) {
        this->mObjectIndex = inOldIndex;
    } else if (inNewIndex == 0xffffffff && theIndex > inOldIndex) {
        this->mObjectIndex = theIndex - 1;
    }
}

// FUNCTION: EXE 0x5bb35c
// Updates the condition's trigger index after triggers inOld/inNew swap, or after inOld is deleted (inNew -1)
void EESMCondition::RenumberTriggers(unsigned long inOldIndex, unsigned long inNewIndex)
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

// FUNCTION: EXE 0x5bb392
// Maps the condition's object index through the ID map
void EESMCondition::RemapObjects(EESMIDMap& inMap)
{
    EERemapID(this->mObjectIndex, inMap);
}

// FUNCTION: EXE 0x5bb3a4
// Maps the condition's trigger index through the ID map
void EESMCondition::RemapTriggers(EESMIDMap& inMap)
{
    EERemapID(this->mTrigger, inMap);
}

// FUNCTION: EXE 0x5bb3b6
// Writes the condition; returns the first error
long EESMCondition::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    long theResult = this->mName.Save(outFile);
    if (theResult == kErrorOK) {
        // The text and chat text results are checked like every other field (they were ignored).
        theResult = this->mText.Save(outFile);
        if (theResult == kErrorOK)
            theResult = this->mChatText.Save(outFile);
        if (theResult == kErrorOK)
            theResult = outFile.Write(this->mType);
    }
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mObjectIndex);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mPlayer);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mAttribute);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mOperator);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mAmount);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mOtherPlayer);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mOtherAttribute);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mTechnology);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mTrigger);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mIsTriggerOn);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mHasTriggerFired);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mUsesPlayerAttribute);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mMaxTime);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mMinTime);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mHour);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mUsesDifficulty);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mUsesElapsedTime);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mUsesHour);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mDifficulty);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mDiplomacy);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mID);
    return theResult;
}
