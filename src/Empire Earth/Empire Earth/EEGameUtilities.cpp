// C:\EEX\Empire Earth\Empire Earth\EEGameUtilities.cpp (file name inferred: no __FILE__, no RTTI of its
// own). 0x1482e4..0x1497f4: RTSWorld.cpp and the separate __ftol object end at 0x1482e4, EEPCalamityHelper.cpp starts
// at 0x1497f4. Game-wide helpers: the display-device list gDisplayRasterizers (its `$E` chain opens the unit; Empire
// Earth.cpp fills the settings from it), the local player's sound, the checksum report, the device enumeration, a
// graphic object's frame sound and the camera-space point helper; then the vector COMDATs of the display structs.
// The .rdata 4.0f at 0x4421fc (the sound falloff) and the ESChecksum vtable copy 0x442204 are this unit's.
// Unit flags: the exe module set.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "rust/game/gamecpp.h"   // round 10 (ee_game gamecpp)
#include <vector>
#include <algorithm>
#include <mbstring.h>
#include <stdlib.h>
#include "ee/EmpireEarth.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "lle/SESoundManager.h"
#include "ee/EEUserInterface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GEBitmap.h"
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
#include "ee/EEMedia.h"
#include "ee/EESoundObject.h"
#include "ee/EEGraphicObject.h"
#include "ee/EEDisplayDevices.h"
#include "lle/GETerrainMesh.h"
#include "lle/NESynchronousServer.h"
#include "lle/NESynchronousClient.h"
#include "ee/EEClient.h"
#include "ee/EEMap.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEARust.h"

extern EEWorld gWorld;
extern EEUIManager* gUIManager;

// the falloff of a graphic object's frame sound (own .rdata copy 0x4421fc: a named const, as kCalamitySoundFalloff)
static const float kGraphicSoundFalloff = 4.0f;

// chain at 0x5482e4 (wrapper)
//   object 0x5319f8  ctor 0x1482ee, registrar 0x148306, atexit dtor 0x148312
std::vector<EEDisplayRasterizer> gDisplayRasterizers;

// Plays a sound when the player is the local player; returns the sound instance, or -1 when nothing played
unsigned long EEPlayForLocalPlayer(EEPlayer* inPlayer, EESoundObject* inSound)
{
    unsigned long theInstance = -1;
    if (inPlayer == gWorld.mPlayers[gWorld.mCurrentPlayer] && inSound != NULL)
        theInstance = gSoundManager->Play(inSound->GetSampleID(), inSound->GetVolume(), 0.0f, 1.0f, false, NULL);
    return theInstance;
}

// Shows a load or sync failure message in the main game form; with a checksum, appends the game state's checksum
// and whether it matches (logging the differences in verbose mode), taking the world's read lock when asked
void EEReportLoadFailure(const UString& inMessage, bool inLock, ESChecksum* inChecksum)
{
    UWideString theText;
    UString theTotal;
    UString theDebug;
    UString theUnused;
    std::vector<UString> theErrors;
    theText = (const char*)inMessage;
    if (inChecksum != NULL) {
        if (inLock)
            gWorld.mLock->ReadLock();
        ESChecksum* theChecksum = new ESChecksum(inChecksum->GetIsVerboseMode());
        gWorld.GetChecksum(*theChecksum);
        if (inLock)
            gWorld.mLock->ReadUnlock();
        sprintf(theTotal, " Game state checksum: %lu ", theChecksum->GetTotalChecksum());
        theText += (const char*)theTotal;
        if (inChecksum->Compare(*theChecksum, theErrors)) {
            theText += " (checksum OK)";
        } else if (theChecksum->GetIsVerboseMode()) {
            theText += " (wrong checksum, see error log)";
            theDebug += (const char*)theText;
            theDebug += " (wrong checksum, see following errors)\r\n";
            ESDebugOut("%s", (const char*)theDebug);
            for (std::vector<UString>::iterator i = theErrors.begin(); i != theErrors.end(); i++)
                ESDebugOut("%s", (const char*)*i);
            ESDebugOut("End checksum errors");
        } else {
            theText += " (wrong checksum, game saved in release so extended info not available)";
            theDebug += (const char*)theText;
            ESDebugOut("%s", (const char*)theDebug);
        }
        delete theChecksum;
    }
    if (gUIManager != NULL)
        gUIManager->GetMainGameForm()->SetErrorString(theText, 0x1c);
}

// Lists the display devices: every rasterizer that activates, its screens, and their 16-bit (and supported 32-bit)
// modes of at least 640x480 that fit three buffers in video memory (at least 0x3e8000 bytes assumed)
void EEEnumerateDisplayDevices()
{
    EEDisplayRasterizer theRasterizer;
    EEDisplayScreen theScreen;
    EEDisplayMode theMode;
    GEVideoMode theLast;
    unsigned long theMemory;
    theLast.mWidth = 0;
    theLast.mHeight = 0;
    theLast.mBitCount = 0;
    theLast.mPixelFormat = kPixelFormatA8R8G8B8;
    if (GERasterizer::GetActiveRasterizer() != NULL)
        return;
    gDisplayRasterizers.clear();
    for (unsigned long i = 0; i < GERasterizer::GetRasterizerCount(); i++) {
        GERasterizer* theDevice = GERasterizer::GetRasterizer(i);
        if (theDevice->Activate() != 0)
            continue;
        theRasterizer.mIndex = i;
        theRasterizer.mName.CopyString(theDevice->GetName(), theDevice->GetName().GetLength());
        gDisplayRasterizers.push_back(theRasterizer);
        for (unsigned long j = 0; j < theDevice->GetScreenCount(); j++) {
            GEScreen* theDisplay = theDevice->GetScreen(j);
            theScreen.mIndex = j;
            theScreen.mName.CopyString(theDisplay->GetName(), theDisplay->GetName().GetLength());
            gDisplayRasterizers.back().mScreens.push_back(theScreen);
            bool theKnown = theDevice->GetTotalVideoMemory(j, theMemory) == 0;
            if (theMemory < 0x3e8000)
                theMemory = 0x3e8000;
            for (unsigned long k = 0; k < theDisplay->GetModeCount(); k++) {
                GEVideoMode theVideoMode = theDisplay->GetMode(k);
                if ((theVideoMode.mBitCount == 16
                     || (theVideoMode.mBitCount == 32 && theDisplay->Get32BitDeviceSupport()))
                    && theVideoMode.mWidth >= 640 && theVideoMode.mHeight >= 480
                    && (theVideoMode.mBitCount != theLast.mBitCount || theVideoMode.mHeight != theLast.mHeight
                        || theVideoMode.mWidth != theLast.mWidth)) {
                    bool theFits = theKnown
                        ? theMemory >= theVideoMode.mBitCount / 8 * theVideoMode.mHeight * theVideoMode.mWidth * 3
                        : true;
                    if (theFits) {
                        sprintf(theMode.mName, "%lu x %lu x %lu Bit", theVideoMode.mWidth, theVideoMode.mHeight,
                                theVideoMode.mBitCount);
                        theMode.mMode = theVideoMode;
                        theMode.mIndex = k;
                        gDisplayRasterizers.back().mScreens.back().mModes.push_back(theMode);
                    }
                }
                theLast = theVideoMode;
            }
        }
        theDevice->Deactivate();
    }
}

// Starts, restarts or updates the sound of a graphic object's animation frame at a location; returns the
// referenced sound instance now playing (the given one when nothing changes)
SESMInstanceNode* PlayAnimationSound(const GE3DPoint& inLocation, EEGraphicObject* inGraphicObject, long inAnimation,
                           SESMInstanceNode* inSound, unsigned long inFrame, bool inStart)
{
    SESMInstanceNode* theResult = inSound;
    if (inSound != NULL)
        inSound->Reference();
    float theVolume;
    float thePan;
    float thePitch;
    if (inStart) {
        EESoundObject* theSound;
        if (inFrame == inGraphicObject->mSound1Frames[inAnimation])
            theSound = inGraphicObject->mSounds1[inAnimation];
        else if (inFrame != inGraphicObject->mSound2Frames[inAnimation])
            return theResult;
        else
            theSound = inGraphicObject->mSounds2[inAnimation];
        if (theSound == NULL)
            return theResult;
        if (theSound->GetSampleID() == gSoundManager->GetSampleID(inSound)) {
            EEComputeSoundParameters(theSound->GetVolume(), inLocation, theVolume, thePan, thePitch, kGraphicSoundFalloff);
            if (gSoundManager->GetIsLooping(inSound)) {
                gSoundManager->SetSoundParameters(inSound, theVolume, thePan, thePitch);
                return theResult;
            }
            if (inSound != NULL)
                inSound->Dereference();
            theResult = gSoundManager->PlayAndReturnReferencedNode(theSound->GetSampleID(), theVolume, thePan, thePitch,
                                                                   theSound->GetLooping(), NULL);
        } else {
            gSoundManager->Stop(inSound, !gSoundManager->GetIsLooping(inSound));
            // drop the reference taken above, as the same-sample branch does (the old sound node used to leak)
            if (inSound != NULL)
                inSound->Dereference();
            EEComputeSoundParameters(theSound->GetVolume(), inLocation, theVolume, thePan, thePitch, kGraphicSoundFalloff);
            theResult = gSoundManager->PlayAndReturnReferencedNode(theSound->GetSampleID(), theVolume, thePan, thePitch,
                                                                   theSound->GetLooping(), NULL);
        }
    } else if (inSound != NULL) {
        unsigned long theSampleID = gSoundManager->GetSampleID(inSound);
        EESoundObject* theSound = inGraphicObject->mSounds1[inAnimation];
        if (theSound == NULL || theSampleID != theSound->GetSampleID()) {
            theSound = inGraphicObject->mSounds2[inAnimation];
            if (theSound == NULL || theSampleID != theSound->GetSampleID())
                theSound = NULL;
        }
        if (theSound != NULL) {
            EEComputeSoundParameters(theSound->GetVolume(), inLocation, theVolume, thePan, thePitch, kGraphicSoundFalloff);
            gSoundManager->SetSoundParameters(inSound, theVolume, thePan, thePitch);
        }
    }
    return theResult;
}

// Maps a world point into camera space, returning it divided by its depth, and the depth
// inPoint in camera space (outScreen, divided by the depth) and its depth
void EEGetCameraPoint(const GE3DPoint& inPoint, GE3DPoint& outScreen, float& outDepth)
{
    GE3DPoint theCamera;
    GE3DPoint theNormalized;
    float theDepth;
    gU4E8F08.GetCameraInverse().Apply(inPoint, theCamera);
    if (!gOrthographicView)
        gU4E8F08.NormalizePerspectiveCameraPoint(theCamera, theNormalized, theDepth);
    else
        gU4E8F08.NormalizeOrthographicCameraPoint(theCamera, theNormalized, theDepth);
    float theScale = theDepth != 0.0f ? 1.0f / theDepth : 1.0f;
    outScreen.mX = theNormalized.mX * theScale;
    outScreen.mY = theNormalized.mY * theScale;
    outScreen.mZ = theNormalized.mZ * theScale;
    outDepth = theDepth;
}

// ---- the game's helpers the actions, the ghosts and the data loading thread use (they were
// EEWonNetAvailableGameFormManager.cpp's: in the UI's unit they put the actions and the ghost units in one
// dependency cycle with the UI; tools/cpp_deps.py, PORT_LEARNINGS "Cycle cuts, round 3")

// Shows the command-confirmation click effect at the ground point under the cursor
void EEOrderAtCursor(const GE2DPoint& inPoint, EEUnit* inUnit)
{
    GE3DPoint thePoint(0.0f, 0.0f, 0.0f);
    EETile* theTile;
    EEPointUnitOrTile(inPoint, theTile, thePoint, true);
    EEMakeCalamityEffect(thePoint, inUnit);
}

// Creates the local-only click confirmation effect on a unit or on the ground
void EEMakeCalamityEffect(const GE3DPoint& inLocation, EEUnit* inUnit)
{
    GE3DPoint theLocation = inLocation;
    EEGraphicEffect* theEffect;
    if (inUnit == NULL) {
        theEffect = EEGraphicEffect::Get(kGraphicEffectCommandConfirmationMouseClickNoUnit);
        theLocation.SetZ(::gWorld.mMap->mTerrainMesh.GetMeshZ(inLocation.GetX(), inLocation.GetY()));
    } else {
        theEffect = EEGraphicEffect::Get(kGraphicEffectCommandConfirmationMouseClickUnit);
        if (inUnit->mTileNode != NULL)
            inUnit->mComplexUnit->GetPosition(theLocation);
        else
            inUnit->GetPosition(theLocation);
    }
    EEEffectHandle theHandle;
    gEffectManager->CreateEffect(inUnit, theLocation, theEffect, &theHandle, 0);
    EEEffect* theFlare = gEffectManager->GetEffect(theHandle);
    unsigned long thePlayerID = ::gWorld.GetCurrentPlayer()->mPlayerID;
    theFlare->SetLocalTo(thePlayerID);
}

// Returns the local player's first selected unit; false when nothing is selected
bool EEGetFirstSelectedUnit(EEComplexUnit*& outUnit)
{
    EEGroup* theGroup = ::gWorld.GetCurrentPlayer()->mSelectedGroup;
    bool theResult = false;
    if (theGroup->GetNumUnits() != 0) {
        outUnit = theGroup->GetUnit(0);
        theResult = true;
    }
    return theResult;
}

// Finds the tile under a screen point (picking the visible tile first); false when only the plain map lookup hit
bool EEPointUnitOrTileXY(const GE2DPoint& inPoint, EETile*& outTile, long& outX, long& outY, bool inFlag)
{
    outTile = ::gWorld.mMap->PickScreenTile(inPoint, outX, outY, inFlag);
    if (outTile != NULL)
        return true;
    outTile = ::gWorld.mMap->GetTileAt(inPoint, outX, outY);
    return false;
}

// NEAR: the original stores the tile centre's y to a frame float (inPoint's dead slot) and reloads it for
// GetMeshZ (fstp/fld); ours keeps it on the FP stack (fst)
// Finds the ground point under a screen point; without a pick it uses the tile centre at terrain height and
// returns false
bool EEPointUnitOrTile(const GE2DPoint& inPoint, EETile*& outTile, GE3DPoint& outPoint, bool inFlag)
{
    outTile = ::gWorld.mMap->GetLocationAt(inPoint, outPoint, inFlag);
    if (outTile != NULL)
        return true;
    long theX;
    long theY;
    outTile = ::gWorld.mMap->GetTileAt(inPoint, theX, theY);
    outPoint.SetX(theX + 0.5f);
    outPoint.SetY(theY + 0.5f);
    outPoint.SetZ(::gWorld.mMap->GetHeight(outPoint));
    return false;
}

// NEAR: the original pushes mCurrentPlayer as a dword: ExecuteRemovePlayer's first parameter is wider than
// the header's unsigned char (NESynchronousCommunications/EEServer/EEClient; header item)
// Leaves the game: a client removes itself (server lost) or notifies the server; the server posts the removal
// of all players when the game is open, running or paused
void EEExitGame()
{
    if (gCommunications->IsClient() == true) {
        EEClient* theClient = static_cast<EEClient*>(gCommunications);
        if (theClient->GetConnection() != NULL) {
            if (theClient->mIsServerLost)
                gCommunications->ExecuteRemovePlayer(::gWorld.mCurrentPlayer, false, false, true);
            else
                gCommunications->ProcessLocalUserInput(new NEMRecord(kNetMessageClientDisconnectionNotification, {true}), kSlotNone);
        }
    } else {
        unsigned char theState = static_cast<NESynchronousServer*>(gCommunications)->GetGameState();
        if (theState == kServerStateOpen || theState == kServerStateRunning || theState == kServerStatePaused) {
            EEAction* theAction = new EEARemovePlayer(0xfe, true);
            gUserInterface->PostAction(theAction);
            theAction->Dereference();
        }
    }
}

// ------------------------------------------------------------------------- round 10, lane D: the Empire Earth directory's managers and utilities

int64_t EECore10OpE(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);
double EECore10OpEF(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);

// the C++ calls and members of lane D's bodies in Rust (gamecpp; EeGameCpp's round 10 lane D section): one
// statement each
int64_t EECore10OpD(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    return EECore10OpE(inOp, p, a, f);
}

double EECore10OpDF(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    return EECore10OpEF(inOp, p, a, f);
}
