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
#include "ee/EEEffect.h"
#include "ee/EEEffectManager.h"


// the falloff of a graphic object's frame sound (own .rdata copy 0x4421fc: a named const, as kCalamitySoundFalloff)
static const float kGraphicSoundFalloff = 4.0f;

// chain at 0x5482e4 (wrapper)
//   object 0x5319f8  ctor 0x1482ee, registrar 0x148306, atexit dtor 0x148312
std::vector<EEDisplayRasterizer> gDisplayRasterizers;

// (the bodies: ee_game gamecpp utilities.rs)

// Plays a sound when the player is the local player; returns the sound instance, or -1 when nothing played
unsigned long EEPlayForLocalPlayer(EEPlayer* inPlayer, EESoundObject* inSound)
{
    return ee_gamecpp_play_for_local(inPlayer, inSound);
}

// Shows a load or sync failure message in the main game form; with a checksum, appends the game state's checksum
// and whether it matches (logging the differences in verbose mode), taking the world's read lock when asked
void EEReportLoadFailure(const UString& inMessage, bool inLock, ESChecksum* inChecksum)
{
    ee_gamecpp_report_load_failure((const char*)inMessage, inLock, inChecksum);
}

// Starts, restarts or updates the sound of a graphic object's animation frame at a location; returns the
// referenced sound instance now playing (the given one when nothing changes)
SESMInstanceNode* PlayAnimationSound(const GE3DPoint& inLocation, EEGraphicObject* inGraphicObject, long inAnimation,
                           SESMInstanceNode* inSound, unsigned long inFrame, bool inStart)
{
    return (SESMInstanceNode*)ee_gamecpp_play_animation_sound(&inLocation, inGraphicObject, inAnimation, inSound, inFrame, inStart);
}

// Maps a world point into camera space, returning it divided by its depth, and the depth
void EEGetCameraPoint(const GE3DPoint& inPoint, GE3DPoint& outScreen, float& outDepth)
{
    ee_gamecpp_camera_point(&inPoint, &outScreen, &outDepth);
}

// ---- the game's helpers the actions, the ghosts and the data loading thread use (they were
// EEWonNetAvailableGameFormManager.cpp's: in the UI's unit they put the actions and the ghost units in one
// dependency cycle with the UI; tools/cpp_deps.py, PORT_LEARNINGS "Cycle cuts, round 3")

// Shows the command-confirmation click effect at the ground point under the cursor
void EEOrderAtCursor(const GE2DPoint& inPoint, EEUnit* inUnit)
{
    ee_gamecpp_order_at_cursor(&inPoint, inUnit);
}

// Creates the local-only click confirmation effect on a unit or on the ground
void EEMakeCalamityEffect(const GE3DPoint& inLocation, EEUnit* inUnit)
{
    ee_gamecpp_make_click_effect(&inLocation, inUnit);
}

// Returns the local player's first selected unit; false when nothing is selected
bool EEGetFirstSelectedUnit(EEComplexUnit*& outUnit)
{
    return ee_gamecpp_first_selected_unit((void**)&outUnit);
}

// Finds the tile under a screen point (picking the visible tile first); false when only the plain map lookup hit
bool EEPointUnitOrTileXY(const GE2DPoint& inPoint, EETile*& outTile, long& outX, long& outY, bool inFlag)
{
    return ee_gamecpp_point_tile_xy(&inPoint, (void**)&outTile, &outX, &outY, inFlag);
}

// Finds the ground point under a screen point; without a pick it uses the tile centre at terrain height and
// returns false
bool EEPointUnitOrTile(const GE2DPoint& inPoint, EETile*& outTile, GE3DPoint& outPoint, bool inFlag)
{
    return ee_gamecpp_point_unit_or_tile(&inPoint, (void**)&outTile, &outPoint, inFlag);
}

// NEAR: the original pushes mCurrentPlayer as a dword: ExecuteRemovePlayer's first parameter is wider than
// the header's unsigned char (NESynchronousCommunications/EEServer/EEClient; header item)
// Leaves the game: a client removes itself (server lost) or notifies the server; the server posts the removal
// of all players when the game is open, running or paused
void EEExitGame()
{
    ee_gamecpp_exit_game();
}

// ------------------------------------------------------------------------- round 10, lane D: the Empire Earth directory's managers and utilities

int64_t EECore10OpD2(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);   // EEEMStorm.cpp
double EECore10OpD7F(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);   // EECameraControl.cpp

static_assert(kGraphicEffectCommandConfirmationMouseClickNoUnit == 0xd1 && kGraphicEffectCommandConfirmationMouseClickUnit == 0xd3
                  && kServerStateOpen == 0 && kServerStateRunning == 3 && kServerStatePaused == 4
                  && sizeof(GEVideoMode) == 5 * sizeof(unsigned long),
              "gamecpp utilities.rs");

#define P64(x) ((int64_t)(intptr_t)(x))
#define RZ ((GERasterizer*)p)
#define SCR ((GEScreen*)p)
#define GO ((EEGraphicObject*)p)
#define NODE ((SESMInstanceNode*)p)
#define CS ((ESChecksum*)p)
#define ERRORS ((std::vector<UString>*)p)
#define SP(i) (*(const GE2DPoint*)a[i])
#define PT(i) (*(GE3DPoint*)a[i])

// the C++ calls and members of lane D's bodies in Rust (gamecpp utilities.rs; EeGameCpp's round 10 lane D section):
// one statement each; the other files' cases follow (EEEMStorm.cpp, EEEnvironmentManager.cpp, EECheatCodes.cpp,
// EECivilization.cpp, EEVCWonder.cpp, EECameraControl.cpp)
int64_t EECore10OpD(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    case O::DuLocalPlayer: return P64(gWorld.mPlayers[gWorld.mCurrentPlayer]);
    case O::DuPlaySound: return gSoundManager->Play(((EESoundObject*)p)->GetSampleID(), ((EESoundObject*)p)->GetVolume(), 0.0f, 1.0f, false, NULL);
    // the failure report
    case O::DuWorldReadLock: gWorld.mLock->ReadLock();
        break;
    case O::DuWorldReadUnlock: gWorld.mLock->ReadUnlock();
        break;
    case O::DuNewChecksum: return P64(new ESChecksum(CS->GetIsVerboseMode()));
    case O::DuWorldChecksum: gWorld.GetChecksum(*CS);
        break;
    case O::DuChecksumTotal: return CS->GetTotalChecksum();
    case O::DuChecksumCompare: return CS->Compare(*(ESChecksum*)a[0], *(std::vector<UString>*)a[1]);
    case O::DuChecksumVerbose: return CS->GetIsVerboseMode();
    case O::DuChecksumDelete: delete CS;
        break;
    case O::DuErrorsNew: return P64(new std::vector<UString>);
    case O::DuErrorsDelete: delete ERRORS;
        break;
    case O::DuErrorCount: return (int64_t)ERRORS->size();
    case O::DuErrorAt: return P64((const char*)(*ERRORS)[a[0]]);
    case O::DuDebugOut: ESDebugOut("%s", (const char*)a[0]);
        break;
    case O::DuSetErrorString: gUIManager->GetMainGameForm()->SetErrorString(UWideString((const char*)a[0]), 0x1c);
        break;
    // the display devices
    case O::DuPixelFormatA8R8G8B8: return kPixelFormatA8R8G8B8;
    case O::DuActiveRasterizer: return P64(GERasterizer::GetActiveRasterizer());
    case O::DuDevicesClear: gDisplayRasterizers.clear();
        break;
    case O::DuRasterizerCount: return GERasterizer::GetRasterizerCount();
    case O::DuRasterizerAt: return P64(GERasterizer::GetRasterizer((unsigned long)a[0]));
    case O::DuActivate: return RZ->Activate();
    case O::DuDeactivate: RZ->Deactivate();
        break;
    case O::DuPushRasterizer: gDisplayRasterizers.push_back(EEDisplayRasterizer{UWideString(RZ->GetName()), {}, (unsigned long)a[0]});
        break;
    case O::DuScreenCount: return RZ->GetScreenCount();
    case O::DuScreenAt: return P64(RZ->GetScreen((unsigned long)a[0]));
    case O::DuPushScreen: gDisplayRasterizers.back().mScreens.push_back(EEDisplayScreen{UWideString(SCR->GetName()), {}, (unsigned long)a[0]});
        break;
    case O::DuVideoMemory: return RZ->GetTotalVideoMemory((unsigned long)a[0], *(unsigned long*)a[1]);
    case O::DuModeCount: return SCR->GetModeCount();
    case O::DuModeAt: *(GEVideoMode*)a[1] = SCR->GetMode((unsigned long)a[0]);
        break;
    case O::Du32Bit: return SCR->Get32BitDeviceSupport();
    case O::DuPushMode: gDisplayRasterizers.back().mScreens.back().mModes.push_back(EEDisplayMode{UWideString((const char*)a[0]), *(const GEVideoMode*)a[1], (unsigned long)a[2]});
        break;
    // a graphic object's frame sound
    case O::DuSound1Frame: return GO->mSound1Frames[a[0]];
    case O::DuSound2Frame: return GO->mSound2Frames[a[0]];
    case O::DuSound1: return P64(GO->mSounds1[a[0]]);
    case O::DuSound2: return P64(GO->mSounds2[a[0]]);
    case O::DuSoundSample: return ((EESoundObject*)p)->GetSampleID();
    case O::DuNodeSample: return gSoundManager->GetSampleID(NODE);
    case O::DuNodeReference: NODE->Reference();
        break;
    case O::DuNodeLooping: return gSoundManager->GetIsLooping(NODE);
    case O::DuNodeSetParameters: gSoundManager->SetSoundParameters(NODE, f[0], f[1], f[2]);
        break;
    case O::DuNodeStop: gSoundManager->Stop(NODE, a[0] != 0);
        break;
    case O::DuSoundParameters: EEComputeSoundParameters(f[0], PT(0), *(float*)a[1], *(float*)a[2], *(float*)a[3], kGraphicSoundFalloff);
        break;
    // the camera point, the click effect, the screen point's tile
    case O::DuCameraInverseApply: gU4E8F08.GetCameraInverse().Apply(*(GE3DPoint*)p, PT(0));
        break;
    case O::DuCameraNormalize: gOrthographicView ? gU4E8F08.NormalizeOrthographicCameraPoint(PT(0), PT(1), *(float*)a[2]) : gU4E8F08.NormalizePerspectiveCameraPoint(PT(0), PT(1), *(float*)a[2]);
        break;
    case O::DuCxPosition: ((EEComplexUnit*)p)->GetPosition(PT(0));
        break;
    case O::DuSetLocal: ((EEEffect*)p)->SetLocalTo(::gWorld.GetCurrentPlayer()->mPlayerID);
        break;
    case O::DuSelectedGroup: return P64(::gWorld.GetCurrentPlayer()->mSelectedGroup);
    case O::DuGroupCount: return ((EESelectedGroup*)p)->GetNumUnits();
    case O::DuGroupUnit: return P64(((EESelectedGroup*)p)->GetUnit((unsigned long)a[0]));
    case O::DuPickScreenTile: return P64(::gWorld.mMap->PickScreenTile(SP(0), *(long*)a[1], *(long*)a[2], a[3] != 0));
    case O::DuTileAt: return P64(::gWorld.mMap->GetTileAt(SP(0), *(long*)a[1], *(long*)a[2]));
    case O::DuLocationAt: return P64(::gWorld.mMap->GetLocationAt(SP(0), PT(1), a[2] != 0));
    // leaving the game
    case O::DuIsClient: return gCommunications->IsClient() == true;
    case O::DuClientConnected: return static_cast<EEClient*>(gCommunications)->GetConnection() != NULL;
    case O::DuServerLost: return static_cast<EEClient*>(gCommunications)->mIsServerLost;
    case O::DuRemoveSelf: gCommunications->ExecuteRemovePlayer(::gWorld.mCurrentPlayer, false, false, true);
        break;
    case O::DuNotifyDisconnect: gCommunications->ProcessLocalUserInput(new NEMRecord(kNetMessageClientDisconnectionNotification, {true}), kSlotNone);
        break;
    case O::DuServerState: return static_cast<NESynchronousServer*>(gCommunications)->GetGameState();
    case O::DuNewRemoveAll: return P64(new EEARemovePlayer(0xfe, true));
    case O::DuPostAction: gUserInterface->PostAction((EEAction*)p);
        break;
    case O::DuActionDereference: ((EEAction*)p)->Dereference();
        break;
    default: return EECore10OpD2(inOp, p, a, f);
    }
    return 0;
}

double EECore10OpDF(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    case O::DuMapHeight: return ::gWorld.mMap->GetHeight(PT(0));
    default: return EECore10OpD7F(inOp, p, a, f);
    }
}
