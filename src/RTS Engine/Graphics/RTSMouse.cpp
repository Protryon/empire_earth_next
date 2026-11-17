// C:\EEX\RTS Engine\Graphics\RTSMouse.cpp (__FILE__ 0x4beb70). The mouse (RTTI RTSMouse, vtable 0x441c74):
// 0x11ab55..0x11b54e (the transparency colour's init chain, the ctor .. Update; RTSMousePointer.cpp opens the next
// unit at 0x11b54e). Unit flags: the exe's module set.
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

#include "lle/LowLevelEngine.h"
#include "platform/PlatWindow.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureManager.h"
#include "lle/GEScreen.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "lle/FSFileSpec.h"
#include "lle/TSCriticalSection.h"
#include "lle/TSScheduler.h"
#include "lle/Input System.h"
#include "lle/ISMouse.h"
#include "lle/ISEntity.h"

#include "ee/RTSMouse.h"
#include "lle/GERenderState.h"
#include "rust/base/math.h"   // ee_gfx mouse.rs (ClampPosition, Update's move), mouse_frame.rs (the frame)
#include "rust/render/gfx.h"
#include "rust/base/math.h"
#include "rust/render/gfx.h"
// the GE value classes as their #[repr(C)] mirrors (the same storage)
static_assert(sizeof(GE2DPoint) == sizeof(EeGe2DPoint) && sizeof(GERectangle) == sizeof(EeGeRectangle), "GE mirrors");

// 0x4be9f8: the pointer textures, by pointer index (Virtual0C); the last slot repeats the blank pointer. PNG files
// (the original's were Targa files, .tga; RUST_PLAN D9)
static const char* sPointerFiles[10] = {
    "Textures\\Normal Pointer.png",
    "Textures\\Attack Pointer.png",
    "Textures\\Invalid Target Pointer.png",
    "Textures\\Target Too Close Pointer.png",
    "Textures\\Gather Pointer.png",
    "Textures\\Build Pointer.png",
    "Textures\\Blank Pointer.png",
    "Textures\\No LOS.png",
    "Textures\\Plus Pointer.png",
    "Textures\\Blank Pointer.png",
};
// 0x4bea20 / 0x4bea24: the smallest raw mouse movement seen so far (Update scales by it)
static long sMinXValue = 100;
static long sMinYValue = 100;

// 0x5297a8: the pointer textures' transparent colour (blue)
static GEColor sTransparencyColor(0.0f, 0.0f, 1.0f);

// The frame's surface calls (ee_gfx mouse_frame.rs)
static uint32_t EEMouseWidth(void* inSurface) { return (uint32_t)static_cast<GESurface*>(inSurface)->GetWidth(); }
static uint32_t EEMouseHeight(void* inSurface) { return (uint32_t)static_cast<GESurface*>(inSurface)->GetHeight(); }
static int32_t EEMouseCopy(void* inFrom, void* inTo, const EeGeRectangle* inSource, const EeGeRectangle* inDestination,
                           bool inTransparent)
{
    GERectangle theSource = *reinterpret_cast<const GERectangle*>(inSource);   // CopyTo takes non-const references
    GERectangle theDestination = *reinterpret_cast<const GERectangle*>(inDestination);
    return (int32_t)static_cast<GESurface*>(inFrom)->CopyTo(static_cast<GESurface*>(inTo), theSource, theDestination,
                                                            inTransparent);
}
static void EEMouseReference(void* inSurface) { static_cast<GESurface*>(inSurface)->Reference(); }
static void EEMouseDereference(void* inSurface) { static_cast<GESurface*>(inSurface)->Dereference(); }
static const EeGfxMouseSurfaces kMouseSurfaces = {EEMouseWidth, EEMouseHeight, EEMouseCopy, EEMouseReference,
                                                  EEMouseDereference};

// One of the frame's calls (ee_gfx_mouse_frame_call's ops)
long RTSMouseFrameCall(RTSMouse* inMouse, uint32_t inOp, GESurface* inSurface, const GE2DPoint* inOrigin, bool inFlag)
{
    return ee_gfx_mouse_frame_call(inMouse->mFrame, &kMouseSurfaces, inOp, inSurface,
                                   reinterpret_cast<const EeGe2DPoint*>(inOrigin), (uint32_t)inMouse->mPointer,
                                   (uint32_t)inMouse->mOverlay, inFlag);
}

// Pointer slot inIndex (0..23) or save-under record 24 + k's surface, filled in place by the loads
GESurface** RTSMouseSlot(RTSMouse* inMouse, unsigned long inIndex)
{
    return reinterpret_cast<GESurface**>(ee_gfx_mouse_frame_slot(inMouse->mFrame, (uint32_t)inIndex));
}

// Creates the mouse with its render and input locks and the desktop size: normal pointer, plus overlay, no surfaces
RTSMouse::RTSMouse()
    : mFrame(ee_gfx_mouse_frame_new())
{
    ESVERIFY_AT(TSScheduler::CreateCriticalSection(this->mRenderLock) == keseOK, 68);
    ESVERIFY_AT(TSScheduler::CreateCriticalSection(this->mInputLock) == keseOK, 69);
    int32_t theDesktopWidth = 0, theDesktopHeight = 0;
    PlatWindowGetDisplaySize(NULL, &theDesktopWidth, &theDesktopHeight);   // the desktop window's size
    this->mSensitivity = 1.0f;
    this->mDesktopHeight = theDesktopHeight;
    this->mDesktopWidth = theDesktopWidth;
    this->mWheel = 0;
    this->mPointer = 0;
    this->mOverlay = 8;
    this->mIsFrozen = false;
    this->mBA2 = false;
    this->mShiftMode = false;
}

// Destroys the mouse's input and render locks
RTSMouse::~RTSMouse()
{
    TSScheduler::DestroyCriticalSection(this->mInputLock);
    TSScheduler::DestroyCriticalSection(this->mRenderLock);
    ee_gfx_mouse_frame_free(this->mFrame);
}

// Saves the screen under the pointer (at the frame's drawn position, clipped to the surface) into the current
// save-under record
void RTSMouse::SaveUnder(GESurface* inSurface, const GE2DPoint& inOrigin)
{
    RTSMouseFrameCall(this, 1, inSurface, &inOrigin, false);
}

// Puts back the screen saved under the pointer from the previous record (windowed: the current one, unshifted);
// the first call after a load only clears the just-loaded flag; returns the copy's result
long RTSMouse::RestoreUnder(GESurface* inSurface, const GE2DPoint& inOrigin, bool inIsWindowed)
{
    return RTSMouseFrameCall(this, 3, inSurface, &inOrigin, inIsWindowed);
}

// Returns the picture of pointer inIndex (EEMouse's 10..23 too; NULL past them)
GESurface* RTSMouse::GetPointerSurface(unsigned long inIndex)
{
    return static_cast<GESurface*>(ee_gfx_mouse_frame_pointer(this->mFrame, (uint32_t)inIndex));
}

// port: a pointer from the texture pack (GETextureManager::SetTexturePack): its file is 32x32 << the pack's scale;
// the surface holds it at its pixel size (halved for the pack's extra MIP drop: nearest, so the blue colour key stays
// exact) and keeps 32x32 as its logical size (GESurface::GetLogicalShift), which the save-under and the drawing use.
// -> kErrorOK with *ioSurface replaced; an error (and *ioSurface untouched) when the pack has no such pointer.
static long LoadPackPointer(const char* inFile, GEScreen* inScreen, const GESurfaceDescription& inDescription,
                            GESurface** ioSurface)
{
    UString thePackName;
    FSFileSpec* theFile = NULL;
    long theShift = GETextureManager::GetTexturePackLogicalShift();
    long theDrop = GETextureManager::GetTexturePackMipLevelsToDrop();
    if (theShift <= 0 || !GETextureManager::GetTexturePackName(UString(inFile), thePackName)
        || inScreen->GetTextureManager()->GetTexturePath()->FindFile(thePackName, theFile) != kErrorOK
        || theFile == NULL)
        return kErrorNotFound;
    unsigned long theWidth = 0, theHeight = 0, theDepth = 0;
    // open across both reads (an in-memory file reopened keeps its position): the header, then the whole file
    long theResult = theFile->Open(kFileAccessRead);
    if (theResult == kErrorOK)
        theResult = GESurface::GetImageFileAttributes(*theFile, kImageFilePNG, theWidth, theHeight, theDepth);
    if (theResult == kErrorOK)
        theResult = theFile->SetPosition(0, kFileOriginBegin, true);
    unsigned long theSize = inDescription.mWidth << (theShift + theDrop);
    if (theResult == kErrorOK && (theWidth != theSize || theHeight != theSize))
        theResult = kErrorFailed;   // not a 32x32 pointer at the pack's scale
    GESurfaceDescription theDescription = inDescription;
    theDescription.mWidth = theWidth;
    theDescription.mHeight = theHeight;
    GESurface* theFull = NULL;
    if (theResult == kErrorOK)
        theResult = inScreen->CreateSurface(theDescription, &theFull);
    if (theResult == kErrorOK)
        theResult = theFull->Load(*theFile, kImageFilePNG);
    theFile->Close();
    delete theFile;
    GESurface* theSurface = theFull;
    if (theResult == kErrorOK && theDrop > 0) {
        theDescription.mWidth = theWidth >> theDrop;
        theDescription.mHeight = theHeight >> theDrop;
        theSurface = NULL;
        theResult = inScreen->CreateSurface(theDescription, &theSurface);
        if (theResult == kErrorOK) {
            GERectangle theSource(0.0f, 0.0f, (float)theWidth, (float)theHeight);
            GERectangle theDestination(0.0f, 0.0f, (float)theDescription.mWidth, (float)theDescription.mHeight);
            theResult = theFull->CopyTo(theSurface, theSource, theDestination, false);
        }
        theFull->Dereference();
    }
    if (theResult != kErrorOK) {
        if (getenv("EE_TEXTURE_PACK_LOG"))
            fprintf(stderr, "texture pack: %s not used (%lux%lu, error %ld)\n", (const char*)thePackName, theWidth,
                    theHeight, theResult);
        if (theSurface)
            theSurface->Dereference();
        return theResult;
    }
    theSurface->SetLogicalShift(theShift);
    theSurface->SetTransparencyColor(sTransparencyColor);
    if (*ioSurface)
        (*ioSurface)->Dereference();
    *ioSurface = theSurface;
    if (getenv("EE_TEXTURE_PACK_LOG"))
        fprintf(stderr, "texture pack: %s logical %lux%lu pixels %lux%lu (pointer)\n", (const char*)thePackName,
                theSurface->GetWidth(), theSurface->GetHeight(), theSurface->GetPixelWidth(),
                theSurface->GetPixelHeight());
    return kErrorOK;
}

// Loads a 32x32 pointer picture (PNG) from the texture path into a surface (created if needed), blue transparent;
// returns the first error of finding, loading and closing the file
long RTSMouse::LoadPointer(const char* inFile, GEScreen* inScreen, GESurface** ioSurface)
{
    GEVideoMode theMode = inScreen->GetCurrentMode();
    GESurfaceDescription theDescription;
    FSFileSpec* theFile = NULL;
    theDescription.mType = kSurfaceOffscreen;
    theDescription.mPixelFormat = theMode.mPixelFormat;
    theDescription.mSurfaceVolatility = 1;
    theDescription.mWidth = 0x20;
    theDescription.mHeight = 0x20;
    theDescription.m14 = 0;
    theDescription.mMipLevelCount = 0;
    theDescription.mPixelDepth = theMode.mBitCount;
    theDescription.mIs3DRenderable = false;
    theDescription.mIsTransparencyEnabled = true;
    // port: the texture pack's pointer when it has one (docs/DATA_FORMATS.md "Texture packs")
    if (LoadPackPointer(inFile, inScreen, theDescription, ioSurface) == kErrorOK)
        return kErrorOK;
    if (*ioSurface != NULL && (*ioSurface)->GetLogicalShift() != 0) {   // was a pack pointer: back to 32x32 pixels
        (*ioSurface)->Dereference();
        *ioSurface = NULL;
    }
    if (*ioSurface == NULL)
        inScreen->CreateSurface(theDescription, ioSurface);
    long theResult = inScreen->GetTextureManager()->GetTexturePath()->FindFile(UString(inFile), theFile);
    // a missing pointer texture fails instead of being loaded through NULL
    if (theFile == NULL)
        return theResult != kErrorOK ? theResult : kErrorNotFound;
    // the load's result counts too (only the close's used to be returned)
    theResult = (*ioSurface)->Load(*theFile, kImageFilePNG);
    long theCloseResult = theFile->Close();
    if (theResult == kErrorOK)
        theResult = theCloseResult;
    delete theFile;
    (*ioSurface)->SetTransparencyColor(sTransparencyColor);
    return theResult;
}

// Loads the 10 pointer textures and creates the two 32x32 save-under surfaces on a screen; returns the first error
long RTSMouse::Load(GEScreen* inScreen)
{
    GEVideoMode theMode = inScreen->GetCurrentMode();
    GESurfaceDescription theDescription;
    long theResult = kErrorOK;
    for (unsigned long i = 0; i < 10 && theResult == kErrorOK; i++)
        theResult = this->LoadPointer(sPointerFiles[i], inScreen, RTSMouseSlot(this, i));
    if (theResult == kErrorOK) {
        theDescription.mIs3DRenderable = false;
        theDescription.mIsTransparencyEnabled = false;
        theDescription.mWidth = 0x20;
        theDescription.mHeight = 0x20;
        theDescription.mPixelFormat = theMode.mPixelFormat;
        theDescription.mPixelDepth = theMode.mBitCount;
        theDescription.mType = kSurfaceOffscreen;
        theDescription.mSurfaceVolatility = 0;
        theDescription.m14 = 0;
        theDescription.mMipLevelCount = 0;
        if (*RTSMouseSlot(this, 24) == NULL) {
            theResult = inScreen->CreateSurface(theDescription, RTSMouseSlot(this, 24));

            ESVERIFY_AT(theResult == keseOK, 349);
        }
        if (*RTSMouseSlot(this, 25) == NULL) {
            theResult = inScreen->CreateSurface(theDescription, RTSMouseSlot(this, 25));

            ESVERIFY_AT(theResult == keseOK, 356);
        }
    }
    RTSMouseFrameCall(this, 7, NULL, NULL, false);   // the next restore only clears the flag
    return theResult;
}

// Draws the frame's pointer transparently over the current save-under rectangle, shifted by inOrigin
void RTSMouse::Draw(GESurface* inSurface, const GE2DPoint& inOrigin)
{
    RTSMouseFrameCall(this, 2, inSurface, &inOrigin, false);
}

// Starts a pointer frame: enters the render lock (EndFrame leaves it), takes the position and pointer (in shift mode
// the pointer with the overlay drawn over it, composed in spare slot 9) and swaps the save-under records
void RTSMouse::BeginFrame()
{
    this->mRenderLock->Enter();
    GE2DPoint thePosition = this->mPosition;
    RTSMouseFrameCall(this, 0, NULL, &thePosition, this->mShiftMode);
}

// Ends the mouse cursor's frame, releasing its surface and the render lock
void RTSMouse::EndFrame()
{
    RTSMouseFrameCall(this, 4, NULL, NULL, false);
    this->mRenderLock->Leave();
}

// Keeps the pointer position inside the bounds (the right and bottom edges excluded): ee_gfx mouse.rs
void RTSMouse::ClampPosition()
{
    ee_gfx_mouse_clamp(reinterpret_cast<EeGe2DPoint*>(&this->mPosition),
                       reinterpret_cast<const EeGeRectangle*>(&this->mBounds));
}

// Releases the pointer textures, both save-under surfaces and the frame's pointer; returns 0
long RTSMouse::Release()
{
    RTSMouseFrameCall(this, 6, NULL, NULL, false);
    return 0;
}

// Moves the pointer by the raw mouse axes (scaled by bounds over desktop, sensitivity and the smallest move seen;
// the arithmetic is ee_gfx mouse.rs) unless frozen, clamps it, and reads the wheel (0 without one)
void RTSMouse::Update()
{
    ISEntity* theAxisX;
    ISEntity* theAxisY;
    ISEntity* theWheel = NULL;
    gISMouse->GetEntity(kMouseXAxis, theAxisX);
    gISMouse->GetEntity(kMouseYAxis, theAxisY);
    gISMouse->GetEntity(kMouseZAxis, theWheel);
    if (!this->mIsFrozen) {
        // the move's arithmetic: ee_gfx mouse.rs (the axes read here, x first; the smallest moves are this unit's
        // statics, 32-bit there: a mouse axis never reaches past it)
        EeGfxMouseInput theInput;
        theInput.position = *reinterpret_cast<const EeGe2DPoint*>(&this->mPosition);
        theInput.bounds = *reinterpret_cast<const EeGeRectangle*>(&this->mBounds);
        theInput.desktop_width = (uint32_t)this->mDesktopWidth;
        theInput.desktop_height = (uint32_t)this->mDesktopHeight;
        theInput.sensitivity = this->mSensitivity;
        theInput.move_x = (int32_t)theAxisX->GetNormalizedValue();
        theInput.move_y = (int32_t)theAxisY->GetNormalizedValue();
        int32_t theMinX = (int32_t)sMinXValue, theMinY = (int32_t)sMinYValue;
        EeGfxMouseMove theMove = ee_gfx_mouse_update_move(&theInput, &theMinX, &theMinY);
        sMinXValue = theMinX;
        sMinYValue = theMinY;
        if (theMove.min_x_changed)
            ESDebugOut("!MinXValue now %ld\n", sMinXValue);
        if (theMove.min_y_changed)
            ESDebugOut("!MinYValue now %ld\n", sMinYValue);
        if (theMove.moved)
            this->mPosition = *reinterpret_cast<const GE2DPoint*>(&theMove.position);
    }
    if (theWheel != NULL)
        this->mWheel = theWheel->GetValue();
    else
        this->mWheel = 0;
}
