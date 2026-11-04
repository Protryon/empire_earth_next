// C:\EEX\Empire Earth\Empire Earth\EECheatCodes.cpp (name from RTTI EECheatCodes; no __FILE__). 0x12a54a..0x12c3eb:
// the cheat codes (ctor, ??_G, dtor, the table, the per-cheat actions, the chat parser), EEMMakeWorldVisible's
// default ctor, then the two maps' COMDATs. EECameraControl.cpp ends at 0x12a54a, EECivilization.cpp starts at
// 0x12c3eb. (provisional names)
#include <windows.h>
#include <mbstring.h>
#include <algorithm>
#include <string>
#include <list>
#include <map>
#include <vector>
#include <stdlib.h>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "GEWindow.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/GEBitmap.h"
#include "lle/FSFileSpec.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "lle/TMPoll.h"
#include "lle/TSCriticalSection.h"
#include "lle/TSScheduler.h"
#include "lle/SESample.h"
#include "lle/SESMInstanceNode.h"
#include "lle/SESoundManager.h"
#include "lle/Input System.h"
#include "lle/ISKeyboard.h"
#include "lle/ISMouse.h"
#include "lle/UIControl.h"
#include "lle/UIEvent.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIBackground.h"
#include "lle/UIFontTable.h"
#include "lle/UIHotKey.h"
#include "lle/UIIMECandidate.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UIButton.h"
#include "lle/UIBlinkingButton.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"
#include "lle/NESynchronousServer.h"
#include "lle/NESynchronousClient.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/UIFormManager.h"
#include "lle/UResourceFile.h"

#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GETransformation.h"
#include "lle/GETerrainMesh.h"
#include "lle/GE3DLine.h"
#include "lle/GEPhysicalModel.h"
#include "ee/EmpireEarth.h"
#include "ee/EEUserInterface.h"
#include "ee/RTSWorld.h"
#include "ee/EECameraControl.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEUnit.h"
#include "ee/EEPlayer.h"
#include "ee/EEPhysics.h"
#include "ee/EEMessage.h"
#include "ee/EECommunications.h"
#include "ee/EECheatCodes.h"
#include "ee/EEStringIDs.h"
extern char gU4E5B98[];                     // 0x4e5b98 (pooled "")

// FUNCTION: EXE 0x52a54a
// Creates the cheat code table from the string resources
EECheatCodes::EECheatCodes()
{
    this->Init();
}

// Compiler-generated: the ??_G of the virtual dtor:
// TEMPLATE: EXE 0x52a589

// FUNCTION: EXE 0x52a5a5
// Destroys the cheat code table
EECheatCodes::~EECheatCodes()
{
    this->mCodes.clear();
}

// FUNCTION: EXE 0x52a5cb
// Adds a cheat code (lower-cased text) with its number and whether it is single-player only
void EECheatCodes::AddCheatCode(const UWideString& inText, long inCode, bool inEnabled)
{
    std::pair<UWideString, long> theEntry;
    UWideString theText;
    theText = inText;
    _mbslwr((unsigned char*)(const char*)theText);
    theEntry.first = theText;
    theEntry.second = inCode;
    this->mCodes.insert(theEntry);
    this->mIsSinglePlayerOnly.insert(std::pair<const long, bool>(inCode, inEnabled));
}

// FUNCTION: EXE 0x52a669
// Gives a player an amount of a resource
void EECheatCodes::AddResource(long inResource, unsigned long inPlayer, long inAmount)
{
    if (inPlayer < gWorld.mNumPlayers)   // players are 0..mNumPlayers - 1 (the test was <=)
        gWorld.mPlayers[inPlayer]->AddResource(inResource, inAmount);
}

// FUNCTION: EXE 0x52a68c
// Lists the cheat codes in the chat log, five per line (the third line repeats the second line's first code and
// the fourth line repeats the third line's last one)
void EECheatCodes::ListCheatCodes()
{
    UWideString theA;
    UWideString theB;
    UWideString theC;
    UWideString theD;
    UWideString theE;
    UWideString theLine;
    UWideString theTitle;
    gResource->GetString(IDS_INGAME_CHEAT, theTitle);
    gResource->GetString(IDS_HEADSHOT, theA);
    gResource->GetString(IDS_SOMEBODY_SET_UP_US_BOMB, theB);
    gResource->GetString(IDS_COLUMBUS, theC);
    gResource->GetString(IDS_ASUS_DRIVERS, theD);
    gResource->GetString(IDS_ATM, theE);
    theLine = theA + "     " + theB + "     " + theC + "     " + theD + "     " + theE;
    gUserInterface->AddChatLogEntry(theTitle + theLine, 0);
    gResource->GetString(IDS_BOSTON_FOOD_SUCKS, theA);
    gResource->GetString(IDS_YOU_SAID_WOOD, theB);
    gResource->GetString(IDS_ATM, theC);
    gResource->GetString(IDS_ROCK_ROLL, theD);
    gResource->GetString(IDS_CREATINE, theE);
    theLine = theA + "     " + theB + "     " + theC + "     " + theD + "     " + theE;
    gUserInterface->AddChatLogEntry(theTitle + theLine, 0);
    gResource->GetString(IDS_ALL_YOUR_BASE_ARE_BELONG_TO_US, theB);
    gResource->GetString(IDS_BAM, theC);
    gResource->GetString(IDS_BRAINSTORM, theD);
    gResource->GetString(IDS_BOSTON_RENT, theE);
    theLine = theA + "     " + theB + "     " + theC + "     " + theD + "     " + theE;
    gUserInterface->AddChatLogEntry(theTitle + theLine, 0);
    gResource->GetString(IDS_UH_SMOKE, theA);
    gResource->GetString(IDS_SLIMFAST, theB);
    gResource->GetString(IDS_MINE_YOUR_OWN_BUSINESS, theC);
    gResource->GetString(IDS_GIRLYMAN, theD);
    theLine = theA + "     " + theB + "     " + theC + "     " + theD + "     " + theE;
    gUserInterface->AddChatLogEntry(theTitle + theLine, 0);
    gResource->GetString(IDS_BIG_DIG, theA);
    gResource->GetString(IDS_MY_NAME_IS_METHOS, theB);
    gResource->GetString(IDS_QUOTABLE_PATELLA, theC);
    gResource->GetString(IDS_DISPLAY_CHEATS, theD);
    theE = gU4E5B98;
    theLine = theA + "     " + theB + "     " + theC + "     " + theD + "     " + theE;
    gUserInterface->AddChatLogEntry(theTitle + theLine, 0);
    gResource->GetString(IDS_I_HAVE_POWER, theA);
    gResource->GetString(IDS_COFFEE_TRAIN, theB);
    gResource->GetString(IDS_AHHHCOOL, theC);
    gResource->GetString(IDS_FRIENDLY_SKIES, theD);
    theE = gU4E5B98;
    theLine = theA + "     " + theB + "     " + theC + "     " + theD + "     " + theE;
    gUserInterface->AddChatLogEntry(theTitle + theLine, 0);
}

// FUNCTION: EXE 0x52aef8
// Runs a cheat code for a player (when cheats are enabled): resources, reveal/explore, epochs, instant build,
// research, healing, power, refuel, win/lose, kill units, or the code list
void EECheatCodes::ApplyCheatCode(long inCode, long inPlayerID, long inParameter)
{
    if (!gWorld.mCheatsEnabled)
        return;
    switch (inCode) {
    case 1:
        this->RevealMap(inParameter);
        break;
    case 2:
        this->SetPhysicsFlagForPlayer(inPlayerID);
        break;
    case 3:
        this->ExploreMap(inPlayerID);
        break;
    case 4:
        this->MakeWorldVisible(inPlayerID);
        break;
    case 5:
        this->AddResource(kResourceGold, inPlayerID, 1000);
        break;
    case 7:
        this->AddResource(kResourceWood, inPlayerID, 1000);
        break;
    case 6:
        this->AddResource(kResourceFood, inPlayerID, 1000);
        break;
    case 8:
        this->AddResource(kResourceStone, inPlayerID, 1000);
        break;
    case 9:
        this->AddResource(kResourceIron, inPlayerID, 1000);
        break;
    case 0xb:
        this->AddResource(kResourceStone, inPlayerID, 100000);
        this->AddResource(kResourceGold, inPlayerID, 100000);
        this->AddResource(kResourceWood, inPlayerID, 100000);
        this->AddResource(kResourceFood, inPlayerID, 100000);
        this->AddResource(kResourceIron, inPlayerID, 100000);
        break;
    case 0xc:
        this->AdvanceEpoch(inParameter - 1, inPlayerID);
        break;
    case 0xd:
        this->ToggleInstantBuild(inPlayerID);
        break;
    case 0xe:
        this->ClearResource(kResourceGold, inPlayerID);
        break;
    case 0xf:
        this->ClearResource(kResourceWood, inPlayerID);
        break;
    case 0x10:
        this->ClearResource(kResourceFood, inPlayerID);
        break;
    case 0x11:
        this->ClearResource(kResourceStone, inPlayerID);
        break;
    case 0x12:
        this->ClearResource(kResourceIron, inPlayerID);
        break;
    case 0x14:
        this->ClearResource(kResourceStone, inPlayerID);
        this->ClearResource(kResourceGold, inPlayerID);
        this->ClearResource(kResourceWood, inPlayerID);
        this->ClearResource(kResourceFood, inPlayerID);
        this->ClearResource(kResourceIron, inPlayerID);
        break;
    case 0x15:
        this->AddResource(kResourceStone, inPlayerID, 100000);
        this->AddResource(kResourceGold, inPlayerID, 100000);
        this->AddResource(kResourceWood, inPlayerID, 100000);
        this->AddResource(kResourceFood, inPlayerID, 100000);
        this->AddResource(kResourceIron, inPlayerID, 100000);
        this->ExploreMap(inPlayerID);
        this->MakeWorldVisible(inPlayerID);
        this->ToggleInstantBuild(inPlayerID);
        this->ResearchAll(inPlayerID);
        break;
    case 0x16:
        this->ResearchAll(inPlayerID);
        break;
    case 0x17:
        this->ListCheatCodes();
        break;
    case 0x1a:
        this->ClearPhysicsFlagForPlayer(inPlayerID);
        break;
    case 0x18:
        this->RestorePower(inPlayerID);
        break;
    case 0x19:
        this->HealUnits(inPlayerID);
        break;
    case 0x1b:
        this->Refuel(inPlayerID);
        break;
    }
}

// FUNCTION: EXE 0x52b169
// Fills the cheat code table with every cheat's text from the string resources and its number
void EECheatCodes::Init()
{
    UWideString theText;
    gResource->GetString(IDS_HEADSHOT, theText);
    this->AddCheatCode(theText, 1, true);
    gResource->GetString(IDS_SOMEBODY_SET_UP_US_BOMB, theText);
    this->AddCheatCode(theText, 2, true);
    gResource->GetString(IDS_COLUMBUS, theText);
    this->AddCheatCode(theText, 0x3, false);
    gResource->GetString(IDS_ASUS_DRIVERS, theText);
    this->AddCheatCode(theText, 0x4, false);
    gResource->GetString(IDS_ATM, theText);
    this->AddCheatCode(theText, 0x5, false);
    gResource->GetString(IDS_BOSTON_FOOD_SUCKS, theText);
    this->AddCheatCode(theText, 0x6, false);
    gResource->GetString(IDS_YOU_SAID_WOOD, theText);
    this->AddCheatCode(theText, 0x7, false);
    gResource->GetString(IDS_ATM, theText);
    this->AddCheatCode(theText, 0x5, false);
    gResource->GetString(IDS_ROCK_ROLL, theText);
    this->AddCheatCode(theText, 0x8, false);
    gResource->GetString(IDS_CREATINE, theText);
    this->AddCheatCode(theText, 0x9, false);
    gResource->GetString(IDS_ALL_YOUR_BASE_ARE_BELONG_TO_US, theText);
    this->AddCheatCode(theText, 0xb, false);
    gResource->GetString(IDS_BAM, theText);
    this->AddCheatCode(theText, 0xc, false);
    gResource->GetString(IDS_BRAINSTORM, theText);
    this->AddCheatCode(theText, 0xd, false);
    gResource->GetString(IDS_BOSTON_RENT, theText);
    this->AddCheatCode(theText, 0xe, false);
    gResource->GetString(IDS_UH_SMOKE, theText);
    this->AddCheatCode(theText, 0xf, false);
    gResource->GetString(IDS_SLIMFAST, theText);
    this->AddCheatCode(theText, 0x10, false);
    gResource->GetString(IDS_MINE_YOUR_OWN_BUSINESS, theText);
    this->AddCheatCode(theText, 0x11, false);
    gResource->GetString(IDS_GIRLYMAN, theText);
    this->AddCheatCode(theText, 0x12, false);
    gResource->GetString(IDS_BIG_DIG, theText);
    this->AddCheatCode(theText, 0x14, false);
    gResource->GetString(IDS_MY_NAME_IS_METHOS, theText);
    this->AddCheatCode(theText, 0x15, false);
    gResource->GetString(IDS_QUOTABLE_PATELLA, theText);
    this->AddCheatCode(theText, 0x16, false);
    gResource->GetString(IDS_DISPLAY_CHEATS, theText);
    this->AddCheatCode(theText, 0x17, false);
    gResource->GetString(IDS_I_HAVE_POWER, theText);
    this->AddCheatCode(theText, 0x18, false);
    gResource->GetString(IDS_COFFEE_TRAIN, theText);
    this->AddCheatCode(theText, 0x19, false);
    gResource->GetString(IDS_AHHHCOOL, theText);
    this->AddCheatCode(theText, 0x1a, false);
    gResource->GetString(IDS_FRIENDLY_SKIES, theText);
    this->AddCheatCode(theText, 0x1b, false);
}

// FUNCTION: EXE 0x52b4bb
// Kills every unit of a player (emptying its resource units); a player number plus 10 spares the local player's
// selected and viewed units
void EECheatCodes::RevealMap(unsigned long inPlayer)
{
    unsigned long theCount = gWorld.mNumPlayers;
    bool theAll = true;
    if (inPlayer >= theCount) {
        inPlayer -= 10;
        theAll = false;
        if (inPlayer >= theCount)
            return;
    }
    for (EEUnit* theUnit = gWorld.mPlayers[inPlayer]->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
        EEComplexUnit* theComplexUnit = theUnit->mComplexUnit;
        if (!theAll && gWorld.GetCurrentPlayer()->mViewedUnit == theUnit)
            continue;
        if (theComplexUnit != NULL) {
            if (theAll || !gWorld.GetCurrentPlayer()->mActiveGroup->Contains(theComplexUnit))
                theComplexUnit->Kill(0, 0, 0);
        } else {
            if (theUnit->mResourceAmount != 0)
                theUnit->mResourceAmount = 0;
            theUnit->Kill(0, 0, 0);
            if (theUnit->mUnitClass->mCategory == kUnitCategoryResource)
                theUnit->mPlayer->KillUnit(theUnit, false);
        }
    }
}

// FUNCTION: EXE 0x52b575
// Returns the cheat number for a lowered chat text, or 0 when unknown or single-player only in a multiplayer game
long EECheatCodes::LookUp(const UWideString& inText)
{
    long theCode;
    std::map<UWideString, long>::iterator theIterator = this->mCodes.find(inText);
    theCode = theIterator != this->mCodes.end() ? theIterator->second : 0;
    if (theCode != 0) {
        std::map<long, bool>::iterator theFlag = this->mIsSinglePlayerOnly.find(theCode);
        if ((theFlag == this->mIsSinglePlayerOnly.end() || theFlag->second != 0) && gCommunications->GetIsMultiplayerSession())
            theCode = 0;
    }
    return theCode;
}

// FUNCTION: EXE 0x52b5d6
// Parses a chat line: "goto x y" and "find [player] id" move the camera; otherwise strips a trailing number as
// the parameter and looks the rest up as a cheat code
void EECheatCodes::ParseCheatCode(const UWideString& inText, long& outCode, long& outParameter)
{
    std::vector<UWideString> theTokens;
    UWideString theSeparators(" ");
    UWideString theText;
    GE3DPoint theLocation;
    GE3DPoint thePoint;
    long theX;
    long theY;
    theText = inText;
    if (theText.Find("goto", 0) == 0) {
        theText.Tokenize(theSeparators, theTokens);
        if (theTokens.size() == 3) {
            thePoint.SetX((float)atof(theTokens[1]));
            thePoint.SetY((float)atof(theTokens[2]));
            if (gWorld.mMap != NULL && gWorld.mMap->GetClosestTile(&thePoint, &theX, &theY) != NULL) {
                gWorld.mCameraControl.GetLocation(gWorld.mCameraControl.mFraction, theLocation);
                theLocation.SetX((float)theX);
                theLocation.SetY((float)theY);
                gWorld.mCameraControl.SetLocation(theLocation, true, true);
            }
        }
        outCode = 0;
    } else if (theText.Find("find", 0) == 0) {
        theText.Tokenize(theSeparators, theTokens);
        if (theTokens.size() >= 2) {
            unsigned long theID = (unsigned long)-1;
            if (theTokens.size() == 2)
                theID = atoi(theTokens[1]);
            else if (theTokens.size() == 3) {
                long thePlayer = atoi(theTokens[1]);
                theID = (thePlayer << 24) + atoi(theTokens[2]);
            }
            if (theID != (unsigned long)-1) {
                RTSUnit* theUnit = gWorld.GetUnit(theID);
                if (theUnit != NULL) {
                    gWorld.mCameraControl.GetLocation(gWorld.mCameraControl.mFraction, theLocation);
                    theLocation.SetX((float)theUnit->mTileX);
                    theLocation.SetY((float)theUnit->mTileY);
                    gWorld.mCameraControl.SetLocation(theLocation, true, true);
                }
            }
        }
        outCode = 0;
    } else {
        long theLength = theText.GetLength();
        if (theLength > 1) {
            unsigned char* theLast = _mbsninc(reinterpret_cast<const unsigned char*>((const char*)theText), theLength - 2);
            outParameter = atoi(reinterpret_cast<const char*>(theLast));
            if (outParameter != 0 || _mbscmp(theLast, reinterpret_cast<const unsigned char*>((const char*)UWideString(" 0"))) == 0) {
                if ((unsigned long)outParameter > 9 && (const char*)theText != reinterpret_cast<const char*>(theLast))
                    theLast = _mbsdec(reinterpret_cast<const unsigned char*>((const char*)theText), theLast);
                strcpy(reinterpret_cast<char*>(theLast), gU4E5B98);
            }
        }
        _mbslwr((unsigned char*)(const char*)theText);
        outCode = this->LookUp(theText);
    }
}

// FUNCTION: EXE 0x52b8cc
// Explores the whole map for a player
void EECheatCodes::ExploreMap(unsigned long inPlayer)
{
    if (inPlayer < gWorld.mNumPlayers)   // players are 0..mNumPlayers - 1 (the test was <=)
        gWorld.mMap->ExploreUnseen(gWorld.mPlayers[inPlayer]);
}

// FUNCTION: EXE 0x52b8ed
// Refuels every living aircraft of a player, making grounded ones commandable again
void EECheatCodes::Refuel(unsigned long inPlayer)
{
    if (!(inPlayer < gWorld.mNumPlayers))   // players are 0..mNumPlayers - 1 (the test was <=)
        return;
    for (EEUnit* theUnit = gWorld.mPlayers[inPlayer]->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
        EEComplexUnit* theComplexUnit = theUnit->mComplexUnit;
        if (!(theUnit->mHitPoints > 0) || theComplexUnit == NULL || !(theComplexUnit->mStats->mFlightTime > 0))
            continue;
        if (theComplexUnit->mFlightTime == 0 && theComplexUnit->mIsCommandable != 0 && theComplexUnit->mIsCommandable != 1)
            theComplexUnit->mIsCommandable = 1;
        if (theComplexUnit->mFlightTime != theComplexUnit->mStats->mFlightTime)
            theComplexUnit->mFlightTime = theComplexUnit->mStats->mFlightTime;
    }
}

// FUNCTION: EXE 0x52b965
// Heals every living unit of a player to full hit points
void EECheatCodes::HealUnits(unsigned long inPlayer)
{
    if (!(inPlayer < gWorld.mNumPlayers))   // players are 0..mNumPlayers - 1 (the test was <=)
        return;
    for (EEUnit* theUnit = gWorld.mPlayers[inPlayer]->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
        if (theUnit->mHitPoints > 0)
            theUnit->SetHitPoints(theUnit->mStats->mMaxHitPoints);
    }
}

// FUNCTION: EXE 0x52b9a2
// Restores the power of a player's living units that have power
void EECheatCodes::RestorePower(unsigned long inPlayer)
{
    if (!(inPlayer < gWorld.mNumPlayers))   // players are 0..mNumPlayers - 1 (the test was <=)
        return;
    for (EEUnit* theUnit = gWorld.mPlayers[inPlayer]->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
        EEComplexUnit* theComplexUnit = theUnit->mComplexUnit;
        if (theUnit->mHitPoints > 0 && theComplexUnit != NULL && theComplexUnit->mPowerRegenTimer > 0) {
            if (theComplexUnit->mPower != theComplexUnit->GetStats()->mPower)
                theComplexUnit->mPower = theComplexUnit->mStats->mPower;
        }
    }
}

// FUNCTION: EXE 0x52b9f2
// Sends the message that makes the whole world visible, if the player is the local player
void EECheatCodes::MakeWorldVisible(unsigned long inPlayer)
{
    if (!(inPlayer < gWorld.mNumPlayers))   // players are 0..mNumPlayers - 1 (the test was <=)
        return;
    EEPlayer* thePlayer = gWorld.mPlayers[inPlayer];
    if (thePlayer != gWorld.GetCurrentPlayer())
        return;
    EEMMakeWorldVisible* theMessage = new EEMMakeWorldVisible;
    theMessage->SetOriginatingSlot(thePlayer->mSlot);
    gCommunications->ProcessLocalUserInput(theMessage, kSlotNone);
}

// FUNCTION: EXE 0x52ba49
// Creates the empty make-world-visible message
EEMMakeWorldVisible::EEMMakeWorldVisible()
    : EEMessage(kMessageMakeWorldVisible, false)
{
}

// FUNCTION: EXE 0x52ba6d
// Researches every improvement a player can still get, to its highest level, and drops the queued improvements
void EECheatCodes::ResearchAll(unsigned long inPlayer)
{
    if (!(inPlayer < gWorld.mNumPlayers))   // players are 0..mNumPlayers - 1 (the test was <=)
        return;
    EEPlayer* thePlayer = gWorld.mPlayers[inPlayer];
    for (long i = 0; i < 0x366; i++) {
        EEUnitClass* theClass = gUnitClassTable[i];
        if (theClass == NULL)
            continue;
        EEUnitClassUpgrade* theUpgrade = thePlayer->mUnitClassUpgrades[i];
        if (theUpgrade != NULL) {
            for (unsigned long j = 0; j < 0x10; j++) {
                if (!thePlayer->CanImprove(i, j, NULL))
                    continue;
                long theMax = theClass->mImprovements->mMaxLevel;
                for (long n = theMax - theUpgrade->mImprovement.GetLevel(j) + 1; n != 0; n--)
                    thePlayer->ApplyImprovement(i, j, false);
            }
        }
        thePlayer->EraseQueuedImprovements(i);
    }
}

// FUNCTION: EXE 0x52bb21
// Advances a player to a later epoch (valid epochs above the prehistoric one only)
void EECheatCodes::AdvanceEpoch(long inEpoch, unsigned long inPlayer)
{
    if (!(inPlayer < gWorld.mNumPlayers))   // players are 0..mNumPlayers - 1 (the test was <=)
        return;
    EEPlayer* thePlayer = gWorld.mPlayers[inPlayer];
    if (inEpoch < kTechEpochAll && inEpoch > kTechEpochPrehistoric && inEpoch > thePlayer->mTechnologies->mCurrentEpoch)
        thePlayer->SetEpoch(inEpoch);
}

// FUNCTION: EXE 0x52bb58
// Toggles instant building for a player
void EECheatCodes::ToggleInstantBuild(unsigned long inPlayer)
{
    if (inPlayer < gWorld.mNumPlayers)   // players are 0..mNumPlayers - 1 (the test was <=)
        gWorld.mPlayers[inPlayer]->mInstantBuild = !gWorld.mPlayers[inPlayer]->mInstantBuild;
}

// FUNCTION: EXE 0x52bb7b
// Ends the game, as a defeat when the player is the local player (else a victory)
void EECheatCodes::ClearPhysicsFlagForPlayer(unsigned long inPlayer)
{
    if (inPlayer < gWorld.mNumPlayers)   // players are 0..mNumPlayers - 1 (the test was <=)
        gPhysics->EndGame(gWorld.mPlayers[inPlayer] != gWorld.GetCurrentPlayer());
}

// FUNCTION: EXE 0x52bbad
// Ends the game, as a victory when the player is the local player (else a defeat)
void EECheatCodes::SetPhysicsFlagForPlayer(unsigned long inPlayer)
{
    if (inPlayer < gWorld.mNumPlayers)   // players are 0..mNumPlayers - 1 (the test was <=)
        gPhysics->EndGame(gWorld.mPlayers[inPlayer] == gWorld.GetCurrentPlayer());
}

// FUNCTION: EXE 0x52bbdf
// Sets a player's stock of a resource to zero
void EECheatCodes::ClearResource(long inResource, unsigned long inPlayer)
{
    if (inPlayer < gWorld.mNumPlayers)   // players are 0..mNumPlayers - 1 (the test was <=)
        gWorld.mPlayers[inPlayer]->mResourceCaps[inResource] = 0;
}

// The two maps' COMDATs (map<UWideString, long> clear/find/insert/_Erase/..., map<long, bool>'s):
// TEMPLATE: EXE 0x52bc01
// TEMPLATE: EXE 0x52bc2c
// TEMPLATE: EXE 0x52bcfc
// TEMPLATE: EXE 0x52bd10
// TEMPLATE: EXE 0x52bd54
// TEMPLATE: EXE 0x52bd7f
// TEMPLATE: EXE 0x52bdf0
// TEMPLATE: EXE 0x52bf84
// TEMPLATE: EXE 0x52bff5
// TEMPLATE: EXE 0x52c2ac
// TEMPLATE: EXE 0x52c2e7
// TEMPLATE: EXE 0x52c31d
// TEMPLATE: EXE 0x52c361
// TEMPLATE: EXE 0x52c393
// TEMPLATE: EXE 0x52c3cb
