// C:\EEX\Empire Earth\Empire Earth\EECheatCodes.cpp (name from RTTI EECheatCodes; no __FILE__). The cheat codes:
// the table, the chat line's parse, the lookup, the code list and the dispatch are ee_app cheats.rs (RUST_PLAN R6
// "app and world"); this unit is their host: the strings, the chat log, the camera and each cheat's world walk.
#include <windows.h>
#include <string.h>
#include <string>
#include <vector>
#include <list>
#include <map>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GE3DLine.h"
#include "lle/GETransformation.h"
#include "lle/GEViewport.h"
#include "lle/GETerrainMesh.h"
#include "lle/GEPhysicalModel.h"
#include "lle/UResourceFile.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"
#include "lle/NESynchronousServer.h"
#include "lle/NESynchronousClient.h"

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
#include "ee/EEAppCapture.h"
#include "rust/ee_app.h"

// New (port): the Rust table's host (ee_app cheats_ffi.rs)
struct EECheatHostContext {
    UWideString mString;                    // STRING's answer
};

// Kills every unit of a player (emptying its resource units); not all: the local player's selected and viewed
// units are spared
static void EECheatKill(unsigned long inPlayer, bool inAll)
{
    for (EEUnit* theUnit = gWorld.mPlayers[inPlayer]->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
        EEComplexUnit* theComplexUnit = theUnit->mComplexUnit;
        if (!inAll && gWorld.GetCurrentPlayer()->mViewedUnit == theUnit)
            continue;
        if (theComplexUnit != NULL) {
            if (inAll || !gWorld.GetCurrentPlayer()->mActiveGroup->Contains(theComplexUnit))
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

// One cheat's work for a player (below the player count)
static void EECheatAction(uint32_t inAction, unsigned long inPlayer, long inA, long inB)
{
    EEPlayer* thePlayer = gWorld.mPlayers[inPlayer];
    switch (inAction) {
    case EE_APP_CHEAT_KILL:
        EECheatKill(inPlayer, inA != 0);
        break;
    case EE_APP_CHEAT_EXPLORE:
        gWorld.mMap->ExploreUnseen(thePlayer);
        break;
    case EE_APP_CHEAT_MAKE_VISIBLE:
        // the message that makes the whole world visible, from the local player only
        if (thePlayer == gWorld.GetCurrentPlayer()) {
            EEMRecord* theMessage = new EEMRecord(kMessageMakeWorldVisible);
            theMessage->SetOriginatingSlot(thePlayer->mSlot);
            gCommunications->ProcessLocalUserInput(theMessage, kSlotNone);
        }
        break;
    case EE_APP_CHEAT_RESEARCH_ALL:
        // every improvement the player can still get, to its highest level; the queued ones dropped
        for (long i = 0; i < EEUnitClassCount(); i++) {
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
        break;
    case EE_APP_CHEAT_ADVANCE_EPOCH:
        // a valid later epoch above the prehistoric one
        if (inA < EEEpochCount() && inA > kTechEpochPrehistoric && inA > thePlayer->mTechnologies->mCurrentEpoch)
            thePlayer->SetEpoch(inA);
        break;
    case EE_APP_CHEAT_RESTORE_POWER:
        for (EEUnit* theUnit = thePlayer->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
            EEComplexUnit* theComplexUnit = theUnit->mComplexUnit;
            if (theUnit->mHitPoints > 0 && theComplexUnit != NULL && theComplexUnit->mPowerRegenTimer > 0 &&
                theComplexUnit->mPower != theComplexUnit->GetStats()->mPower)
                theComplexUnit->mPower = theComplexUnit->mStats->mPower;
        }
        break;
    case EE_APP_CHEAT_HEAL:
        for (EEUnit* theUnit = thePlayer->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
            if (theUnit->mHitPoints > 0)
                theUnit->SetHitPoints(theUnit->mStats->mMaxHitPoints);
        }
        break;
    case EE_APP_CHEAT_REFUEL:
        // living aircraft refuelled; grounded ones commandable again
        for (EEUnit* theUnit = thePlayer->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
            EEComplexUnit* theComplexUnit = theUnit->mComplexUnit;
            if (!(theUnit->mHitPoints > 0) || theComplexUnit == NULL || !(theComplexUnit->mStats->mFlightTime > 0))
                continue;
            if (theComplexUnit->mFlightTime == 0 && theComplexUnit->mIsCommandable != 0 && theComplexUnit->mIsCommandable != 1)
                theComplexUnit->mIsCommandable = 1;
            if (theComplexUnit->mFlightTime != theComplexUnit->mStats->mFlightTime)
                theComplexUnit->mFlightTime = theComplexUnit->mStats->mFlightTime;
        }
        break;
    case EE_APP_CHEAT_TOGGLE_INSTANT_BUILD:
        thePlayer->mInstantBuild = !thePlayer->mInstantBuild;
        break;
    case EE_APP_CHEAT_END_GAME:
        // a victory for the local player (inA) or a defeat
        gPhysics->EndGame(inA != 0 ? thePlayer == gWorld.GetCurrentPlayer() : thePlayer != gWorld.GetCurrentPlayer());
        break;
    case EE_APP_CHEAT_ADD_RESOURCE:
        thePlayer->AddResource(inA, inB);
        break;
    case EE_APP_CHEAT_CLEAR_RESOURCE:
        thePlayer->mResourceCaps[inA] = 0;
        break;
    }
}

static void EECheatHostOp(void* inContext, int32_t inOp, EeAppCheatIo* io)
{
    EECheatHostContext& theContext = *static_cast<EECheatHostContext*>(inContext);
    switch (inOp) {
    case EE_APP_CHEAT_OP_STRING:
        gResource->GetString(io->id, theContext.mString);
        io->text = reinterpret_cast<const uint8_t*>((const char*)theContext.mString);
        io->len = strlen((const char*)theContext.mString);
        break;
    case EE_APP_CHEAT_OP_MULTIPLAYER:
        io->flag = gCommunications->GetIsMultiplayerSession();
        break;
    case EE_APP_CHEAT_OP_CHEATS_ENABLED:
        io->flag = gWorld.mCheatsEnabled;
        break;
    case EE_APP_CHEAT_OP_PLAYERS:
        io->n = gWorld.mNumPlayers;
        break;
    case EE_APP_CHEAT_OP_CHAT: {
        UWideString theLine(std::string(reinterpret_cast<const char*>(io->text), io->len).c_str());
        gUserInterface->AddChatLogEntry(theLine, 0);
        EEAppCaptureCheatList(theLine);   // CHLS (TEST ONLY)
        break;
    }
    case EE_APP_CHEAT_OP_GOTO: {
        // the camera to the map's closest tile of the point
        GE3DPoint thePoint;
        GE3DPoint theLocation;
        long theX;
        long theY;
        thePoint.SetX(io->x);
        thePoint.SetY(io->y);
        if (gWorld.mMap != NULL && gWorld.mMap->GetClosestTile(&thePoint, &theX, &theY) != NULL) {
            gWorld.mCameraControl.GetLocation(gWorld.mCameraControl.mFraction, theLocation);
            theLocation.mX = (float)theX;
            theLocation.mY = (float)theY;
            gWorld.mCameraControl.SetLocation(theLocation, true, true);
        }
        break;
    }
    case EE_APP_CHEAT_OP_FIND: {
        // the camera to the unit's tile
        RTSUnit* theUnit = gWorld.GetUnit(io->n);
        if (theUnit != NULL) {
            GE3DPoint theLocation;
            gWorld.mCameraControl.GetLocation(gWorld.mCameraControl.mFraction, theLocation);
            theLocation.SetX((float)theUnit->mTileX);
            theLocation.SetY((float)theUnit->mTileY);
            gWorld.mCameraControl.SetLocation(theLocation, true, true);
        }
        break;
    }
    case EE_APP_CHEAT_OP_ACTION:
        EECheatAction(io->id, io->n, io->a, io->b);
        break;
    }
}

// Creates the cheat code table from the string resources
EECheatCodes::EECheatCodes()
{
    EECheatHostContext theContext;
    this->mRust = ee_app_cheats_new(&EECheatHostOp, &theContext);
}

// Destroys the cheat code table
EECheatCodes::~EECheatCodes()
{
    ee_app_cheats_free(this->mRust);
}

// Runs a cheat code for a player (when cheats are enabled): resources, reveal/explore, epochs, instant build,
// research, healing, power, refuel, win/lose, kill units, or the code list
void EECheatCodes::ApplyCheatCode(long inCode, long inPlayerID, long inParameter)
{
    EECheatHostContext theContext;
    ee_app_cheats_apply(this->mRust, &EECheatHostOp, &theContext, inCode, inPlayerID, inParameter);
}

// Parses a chat line: "goto x y" and "find [player] id" move the camera; otherwise strips a trailing number as
// the parameter and looks the rest up as a cheat code
void EECheatCodes::ParseCheatCode(const UWideString& inText, long& outCode, long& outParameter)
{
    EECheatHostContext theContext;
    const char* theText = (const char*)inText;
    ee_app_cheats_parse(this->mRust, &EECheatHostOp, &theContext, reinterpret_cast<const uint8_t*>(theText),
                        strlen(theText), &outCode, &outParameter);
}
