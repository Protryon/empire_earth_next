// C:\EEX\Empire Earth\Empire Earth\EEGameUtilities.cpp (file name inferred: no __FILE__, no RTTI of its
// own). 0x1482e4..0x1497f4: RTSWorld.cpp and the separate __ftol object end at 0x1482e4, EEPCalamityHelper.cpp starts
// at 0x1497f4. Game-wide helpers: the display-device list gDisplayRasterizers (its `$E` chain opens the unit; Empire
// Earth.cpp fills the settings from it), the local player's sound, the checksum report, the device enumeration, a
// graphic object's frame sound and the camera-space point helper; then the vector COMDATs of the display structs.
// The .rdata 4.0f at 0x4421fc (the sound falloff) and the ESChecksum vtable copy 0x442204 are this unit's.
// Unit flags: the exe module set.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/UWideString.h"
#include "lle/FSFile.h"
#include <vector>
#include <algorithm>
#include <mbstring.h>
#include <stdlib.h>
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
#include "lle/ESChecksum.h"
#include "ee/EEMedia.h"
#include "ee/EESoundObject.h"
#include "ee/EEGraphicObject.h"
#include "ee/EEDisplayDevices.h"

extern EEWorld gWorld;
extern EEUIManager* gUIManager;
extern GETransformation gRenderCameraInverse;  // 0x4e8ff8: the camera transformation the 3D points are mapped through (provisional name)

// the falloff of a graphic object's frame sound (own .rdata copy 0x4421fc: a named const, as kCalamitySoundFalloff)
static const float kGraphicSoundFalloff = 4.0f;

// chain at 0x5482e4 (wrapper)
//   object 0x5319f8  ctor 0x1482ee, registrar 0x148306, atexit dtor 0x148312
std::vector<EEDisplayRasterizer> gDisplayRasterizers;
// SYNTHETIC: EXE 0x5482e4   (wrapper)
// SYNTHETIC: EXE 0x5482ee   (ctor)
// SYNTHETIC: EXE 0x548306   (registrar)
// SYNTHETIC: EXE 0x548312   (atexit dtor)

// FUNCTION: EXE 0x54831c
// Plays a sound when the player is the local player; returns the sound instance, or -1 when nothing played
unsigned long EEPlayForLocalPlayer(EEPlayer* inPlayer, EESoundObject* inSound)
{
    unsigned long theInstance = -1;
    if (inPlayer == gWorld.mPlayers[gWorld.mCurrentPlayer] && inSound != NULL)
        theInstance = gSoundManager->Play(inSound->GetSampleID(), inSound->GetVolume(), 0.0f, 1.0f, false, NULL);
    return theInstance;
}

// FUNCTION: EXE 0x548362
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
        sprintf(theTotal, " Game state checksum: %u ", theChecksum->GetTotalChecksum());
        theText += (const char*)theTotal;
        if (inChecksum->Compare(*theChecksum, theErrors)) {
            theText += " (checksum OK)";
        } else if (theChecksum->GetIsVerboseMode()) {
            theText += " (wrong checksum, see error log)";
            theDebug += (const char*)theText;
            theDebug += " (wrong checksum, see following errors)\r\n";
            ESDebugOut(theDebug);
            for (std::vector<UString>::iterator i = theErrors.begin(); i != theErrors.end(); i++)
                ESDebugOut(*i);
            ESDebugOut("End checksum errors");
        } else {
            theText += " (wrong checksum, game saved in release so extended info not available)";
            theDebug += (const char*)theText;
            ESDebugOut(theDebug);
        }
        delete theChecksum;
    }
    if (gUIManager != NULL)
        gUIManager->GetMainGameForm()->SetErrorString(theText, 0x1c);
}

// FUNCTION: EXE 0x548514
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
                        sprintf(theMode.mName, "%u x %u x %u Bit", theVideoMode.mWidth, theVideoMode.mHeight,
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

// FUNCTION: EXE 0x5487a3
// Starts, restarts or updates the sound of a graphic object's animation frame at a location; returns the
// referenced sound instance now playing (the given one when nothing changes)
SESMInstanceNode* Fn1487A3(const GE3DPoint& inLocation, EEGraphicObject* inGraphicObject, long inAnimation,
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
        else if (inFrame == inGraphicObject->mSound2Frames[inAnimation])
            theSound = inGraphicObject->mSounds2[inAnimation];
        else
            return theResult;
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

// FUNCTION: EXE 0x548992
// Maps a world point into camera space, returning it divided by its depth, and the depth
// inPoint in camera space (outScreen, divided by the depth) and its depth
void EEGetCameraPoint(const GE3DPoint& inPoint, GE3DPoint& outScreen, float& outDepth)
{
    GE3DPoint theCamera;
    GE3DPoint theNormalized;
    float theDepth;
    gRenderCameraInverse.Apply(inPoint, theCamera);
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



// TEMPLATE: EXE 0x548a29
// ??0?$vector@UEEDisplayScreen@@V?$allocator@UEEDisplayScreen@@@std@@@std@@QAE@XZ
// TEMPLATE: EXE 0x548a3f
// ?push_back@?$vector@UEEDisplayMode@@V?$allocator@UEEDisplayMode@@@std@@@std@@QAEXABUEEDisplayMode@@@Z
// TEMPLATE: EXE 0x548a6b
// ?push_back@?$vector@UEEDisplayScreen@@V?$allocator@UEEDisplayScreen@@@std@@@std@@QAEXABUEEDisplayScreen@@@Z
// TEMPLATE: EXE 0x548a97
// ?push_back@?$vector@UEETileCoordinate@@V?$allocator@UEETileCoordinate@@@std@@@std@@QAEXABUEETileCoordinate@@@Z
// TEMPLATE: EXE 0x548ac3
// ?insert@?$vector@UEEDisplayMode@@V?$allocator@UEEDisplayMode@@@std@@@std@@QAE?AV?$_Ptrit@UEEDisplayMode@@HPAU1@AAU1@PAU1@AAU1@@2@V32@ABUEEDisplayMode@@@Z
// TEMPLATE: EXE 0x548b1a
// ?insert@?$vector@UEEDisplayScreen@@V?$allocator@UEEDisplayScreen@@@std@@@std@@QAE?AV?$_Ptrit@UEEDisplayScreen@@HPAU1@AAU1@PAU1@AAU1@@2@V32@ABUEEDisplayScreen@@@Z
// TEMPLATE: EXE 0x548b74
// ?insert@?$vector@UEEDisplayRasterizer@@V?$allocator@UEEDisplayRasterizer@@@std@@@std@@QAE?AV?$_Ptrit@UEEDisplayRasterizer@@HPAU1@AAU1@PAU1@AAU1@@2@V32@ABUEEDisplayRasterizer@@@Z
// TEMPLATE: EXE 0x548bce
// ?_Tidy@?$vector@UEEDisplayRasterizer@@V?$allocator@UEEDisplayRasterizer@@@std@@@std@@IAEXXZ
// TEMPLATE: EXE 0x548c06
// ?insert@?$vector@UEEDisplayMode@@V?$allocator@UEEDisplayMode@@@std@@@std@@QAEXV?$_Ptrit@UEEDisplayMode@@HPAU1@AAU1@PAU1@AAU1@@2@IABUEEDisplayMode@@@Z
// TEMPLATE: EXE 0x548ef5
// ?insert@?$vector@UEEDisplayScreen@@V?$allocator@UEEDisplayScreen@@@std@@@std@@QAEXV?$_Ptrit@UEEDisplayScreen@@HPAU1@AAU1@PAU1@AAU1@@2@IABUEEDisplayScreen@@@Z
// TEMPLATE: EXE 0x5491d7
// ??_GEEDisplayScreen@@QAEPAXI@Z
// TEMPLATE: EXE 0x5491ec
// ??G?$_Ptrit@VEECPUnitRequirement@@HPAV1@AAV1@PAV1@AAV1@@std@@QBE?AV01@H@Z
// TEMPLATE: EXE 0x549203
// ??_GESChecksum@@UAEPAXI@Z
// TEMPLATE: EXE 0x549229
// ?insert@?$vector@UEEDisplayRasterizer@@V?$allocator@UEEDisplayRasterizer@@@std@@@std@@QAEXV?$_Ptrit@UEEDisplayRasterizer@@HPAU1@AAU1@PAU1@AAU1@@2@IABUEEDisplayRasterizer@@@Z
// TEMPLATE: EXE 0x54950b
// ??_GEEDisplayRasterizer@@QAEPAXI@Z
// TEMPLATE: EXE 0x549520
// ??H?$_Ptrit@VEECPUnitRequirement@@HPAV1@AAV1@PAV1@AAV1@@std@@QBE?AV01@H@Z
// TEMPLATE: EXE 0x549535
// ??0EEDisplayRasterizer@@QAE@ABU0@@Z (it copies a vector of 0x28-byte EEDisplayScreen: 0x149598; EEDisplayScreen's own is 0x14785b)
// TEMPLATE: EXE 0x54955d
// ?_Ufill@?$vector@UEEDisplayMode@@V?$allocator@UEEDisplayMode@@@std@@@std@@IAEPAUEEDisplayMode@@PAU3@IABU3@@Z
// TEMPLATE: EXE 0x549584
// ??G?$_Ptrit@UEEPGSPlayerReport@@HPAU1@AAU1@PAU1@AAU1@@std@@QBE?AV01@H@Z
// TEMPLATE: EXE 0x549598
// ??0?$vector@UEEDisplayScreen@@V?$allocator@UEEDisplayScreen@@@std@@@std@@QAE@ABV01@@Z
// TEMPLATE: EXE 0x5495e2
// ?_Ufill@?$vector@UEEDisplayScreen@@V?$allocator@UEEDisplayScreen@@@std@@@std@@IAEPAUEEDisplayScreen@@PAU3@IABU3@@Z
// TEMPLATE: EXE 0x54960d
// ?_Ufill@?$vector@UEEDisplayRasterizer@@V?$allocator@UEEDisplayRasterizer@@@std@@@std@@IAEPAUEEDisplayRasterizer@@PAU3@IABU3@@Z
// TEMPLATE: EXE 0x549638
// ?_Ucopy@?$vector@UEEDisplayMode@@V?$allocator@UEEDisplayMode@@@std@@@std@@IAEPAUEEDisplayMode@@V?$_Ptrit@UEEDisplayMode@@HPAU1@AAU1@PAU1@AAU1@@2@0PAU3@@Z
// TEMPLATE: EXE 0x549660
// ?fill@std@@YAXV?$_Ptrit@UEEDisplayMode@@HPAU1@AAU1@PAU1@AAU1@@1@0ABUEEDisplayMode@@@Z
// TEMPLATE: EXE 0x54969b
// ?copy_backward@std@@YA?AV?$_Ptrit@UEEDisplayMode@@HPAU1@AAU1@PAU1@AAU1@@1@V21@00@Z
// TEMPLATE: EXE 0x5496e6
// ?_Ucopy@?$vector@UEEDisplayScreen@@V?$allocator@UEEDisplayScreen@@@std@@@std@@IAEPAUEEDisplayScreen@@V?$_Ptrit@UEEDisplayScreen@@HPAU1@AAU1@PAU1@AAU1@@2@0PAU3@@Z
// TEMPLATE: EXE 0x549712
// ?fill@std@@YAXV?$_Ptrit@UEEDisplayScreen@@HPAU1@AAU1@PAU1@AAU1@@1@0ABUEEDisplayScreen@@@Z
// TEMPLATE: EXE 0x54972f
// ?copy_backward@std@@YA?AV?$_Ptrit@UEEDisplayScreen@@HPAU1@AAU1@PAU1@AAU1@@1@V21@00@Z
// TEMPLATE: EXE 0x54975b
// ?_Ucopy@?$vector@UEEDisplayRasterizer@@V?$allocator@UEEDisplayRasterizer@@@std@@@std@@IAEPAUEEDisplayRasterizer@@V?$_Ptrit@UEEDisplayRasterizer@@HPAU1@AAU1@PAU1@AAU1@@2@0PAU3@@Z
// TEMPLATE: EXE 0x549787
// ?fill@std@@YAXV?$_Ptrit@UEEDisplayRasterizer@@HPAU1@AAU1@PAU1@AAU1@@1@0ABUEEDisplayRasterizer@@@Z
// TEMPLATE: EXE 0x5497a4
// ?copy_backward@std@@YA?AV?$_Ptrit@UEEDisplayRasterizer@@HPAU1@AAU1@PAU1@AAU1@@1@V21@00@Z
// TEMPLATE: EXE 0x5497d0
// ?_Construct@std@@YAXPAUEEDisplayScreen@@ABU2@@Z
// TEMPLATE: EXE 0x5497e2
// ?_Construct@std@@YAXPAUEEDisplayRasterizer@@ABU2@@Z
