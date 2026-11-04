// C:\EEX\RTS Engine\Graphics\RTSMouse.cpp (__FILE__ 0x4beb70). The mouse (RTTI RTSMouse, vtable 0x441c74):
// 0x11ab55..0x11b54e (the transparency colour's init chain, the ctor .. Update; RTSMousePointer.cpp opens the next
// unit at 0x11b54e). Unit flags: the exe's module set.
#include <windows.h>

#include "lle/LowLevelEngine.h"
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

#include "ee/EEMouse.h"
#include "lle/GERenderState.h"
// 0x4be9f8: the pointer textures, by pointer index (Virtual0C); the last slot repeats the blank pointer
static const char* sPointerFiles[10] = {
    "Textures\\Normal Pointer.tga",
    "Textures\\Attack Pointer.tga",
    "Textures\\Invalid Target Pointer.tga",
    "Textures\\Target Too Close Pointer.tga",
    "Textures\\Gather Pointer.tga",
    "Textures\\Build Pointer.tga",
    "Textures\\Blank Pointer.tga",
    "Textures\\No LOS.tga",
    "Textures\\Plus Pointer.tga",
    "Textures\\Blank Pointer.tga",
};
// 0x4bea20 / 0x4bea24: the smallest raw mouse movement seen so far (Update scales by it)
static long sMinXValue = 100;
static long sMinYValue = 100;

// 0x5297a8: the pointer textures' transparent colour (blue)
static GEColor sTransparencyColor(0.0f, 0.0f, 1.0f);
// SYNTHETIC: EXE 0x51ab55   (wrapper)
// SYNTHETIC: EXE 0x51ab5f   (ctor)
// SYNTHETIC: EXE 0x51ab78   (registrar)
// SYNTHETIC: EXE 0x51ab84   (dtor)

// FUNCTION: EXE 0x51ab85
// Creates the mouse with its render and input locks and the desktop size: normal pointer, plus overlay, no surfaces
RTSMouse::RTSMouse()
{
    ESVERIFY_AT(TSScheduler::CreateCriticalSection(this->mRenderLock) == keseOK, 68);
    ESVERIFY_AT(TSScheduler::CreateCriticalSection(this->mInputLock) == keseOK, 69);
    RECT theDesktop;
    GetWindowRect(GetDesktopWindow(), &theDesktop);
    this->mSensitivity = 1.0f;
    this->mCurrentSaveUnder = &this->mSaveUnder24;
    this->mDesktopHeight = theDesktop.bottom - theDesktop.top;
    this->mDesktopWidth = theDesktop.right - theDesktop.left;
    this->mPreviousSaveUnder = &this->mSaveUnder38;
    this->mWheel = 0;
    this->mPointer = 0;
    this->mOverlay = 8;
    this->mIsFrozen = false;
    this->mInFrame = false;
    this->mBA2 = false;
    this->mIsJustLoaded = true;
    this->mShiftMode = false;
    this->mCurrentSurface = NULL;
    this->mSaveUnder24.mSurface = NULL;
    this->mSaveUnder38.mSurface = NULL;
    for (unsigned long i = 0; i < 10; i++)
        this->mPointerSurfaces[i] = NULL;
}

// SYNTHETIC: EXE 0x51acbe
// (the scalar deleting destructor)

// FUNCTION: EXE 0x51acda
// Destroys the mouse's input and render locks
RTSMouse::~RTSMouse()
{
    TSScheduler::DestroyCriticalSection(this->mInputLock);
    TSScheduler::DestroyCriticalSection(this->mRenderLock);
}

// FUNCTION: EXE 0x51acf9
// Saves the screen under the pointer (at the frame's drawn position, clipped to the surface) into the current
// save-under record
void RTSMouse::SaveUnder(GESurface* inSurface, const GE2DPoint& inOrigin)
{
    GERectangle theSource;
    GERectangle theDestination;
    long theX = UMFloor(this->mDrawnPosition.GetX());
    long theY = UMFloor(this->mDrawnPosition.GetY());
    long theWidth = this->mCurrentSurface->GetWidth();
    long theHeight = this->mCurrentSurface->GetHeight();
    if (theX + theWidth > (long)inSurface->GetWidth())
        theWidth = inSurface->GetWidth() - theX;
    if (theY + theHeight > (long)inSurface->GetHeight())
        theHeight = inSurface->GetHeight() - theY;
    theDestination.SetTop(0.0f);
    theDestination.SetLeft(0.0f);
    theDestination.SetBottom(theHeight);
    theDestination.SetRight(theWidth);
    theSource.SetTop(inOrigin.GetY() + theY);
    theSource.SetLeft(inOrigin.GetX() + theX);
    theSource.SetBottom(theSource.GetTop() + theHeight);
    theSource.SetRight(theSource.GetLeft() + theWidth);
    this->mCurrentSaveUnder->mRect = GERectangle(theX, theY, theX + theWidth, theY + theHeight);
    inSurface->CopyTo(this->mCurrentSaveUnder->mSurface, theSource, theDestination, false);
}

// FUNCTION: EXE 0x51ae5d
// Puts back the screen saved under the pointer from the previous record (windowed: the current one, unshifted);
// the first call after a load only clears the just-loaded flag; returns the copy's result
long RTSMouse::RestoreUnder(GESurface* inSurface, const GE2DPoint& inOrigin, bool inIsWindowed)
{
    GERectangle theSource;
    GERectangle theDestination;
    if (!this->mIsJustLoaded) {
        if (inIsWindowed) {
            this->mPreviousSaveUnder = this->mCurrentSaveUnder;
            theDestination = this->mPreviousSaveUnder->mRect;
        } else {
            theDestination = this->mPreviousSaveUnder->mRect;
            theDestination.SetLeft(theDestination.GetLeft() + inOrigin.GetX());
            theDestination.SetTop(theDestination.GetTop() + inOrigin.GetY());
            theDestination.SetRight(theDestination.GetRight() + inOrigin.GetX());
            theDestination.SetBottom(theDestination.GetBottom() + inOrigin.GetY());
        }
        theSource.SetTop(0.0f);
        theSource.SetLeft(0.0f);
        theSource.SetBottom(theDestination.GetBottom() - theDestination.GetTop());
        theSource.SetRight(theDestination.GetRight() - theDestination.GetLeft());
        return this->mPreviousSaveUnder->mSurface->CopyTo(inSurface, theSource, theDestination, false);
    }
    this->mIsJustLoaded = false;
    return 0;
}

// FUNCTION: EXE 0x51af52
// Returns the texture of pointer inIndex (NULL past the 10 pointers)
GESurface* RTSMouse::GetPointerSurface(unsigned long inIndex)
{
    return inIndex < 10 ? this->mPointerSurfaces[inIndex] : NULL;
}

// FUNCTION: EXE 0x51af66
// Loads a 32x32 TGA pointer texture from the texture path into a surface (created if needed), blue transparent;
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
    if (*ioSurface == NULL)
        inScreen->CreateSurface(theDescription, ioSurface);
    long theResult = inScreen->GetTextureManager()->GetTexturePath()->FindFile(UString(inFile), theFile);
    // a missing pointer texture fails instead of being loaded through NULL
    if (theFile == NULL)
        return theResult != kErrorOK ? theResult : kErrorNotFound;
    // the load's result counts too (only the close's used to be returned)
    theResult = (*ioSurface)->Load(*theFile, kImageFileTGA);
    long theCloseResult = theFile->Close();
    if (theResult == kErrorOK)
        theResult = theCloseResult;
    delete theFile;
    (*ioSurface)->SetTransparencyColor(sTransparencyColor);
    return theResult;
}

// FUNCTION: EXE 0x51b02b
// Loads the 10 pointer textures and creates the two 32x32 save-under surfaces on a screen; returns the first error
long RTSMouse::Load(GEScreen* inScreen)
{
    GEVideoMode theMode = inScreen->GetCurrentMode();
    GESurfaceDescription theDescription;
    long theResult = kErrorOK;
    for (unsigned long i = 0; i < 10 && theResult == kErrorOK; i++)
        theResult = this->LoadPointer(sPointerFiles[i], inScreen, &this->mPointerSurfaces[i]);
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
        if (this->mSaveUnder24.mSurface == NULL) {
            theResult = inScreen->CreateSurface(theDescription, &this->mSaveUnder24.mSurface);

            ESVERIFY_AT(theResult == keseOK, 349);
        }
        if (this->mSaveUnder38.mSurface == NULL) {
            theResult = inScreen->CreateSurface(theDescription, &this->mSaveUnder38.mSurface);

            ESVERIFY_AT(theResult == keseOK, 356);
        }
    }
    this->mIsJustLoaded = true;
    return theResult;
}

// FUNCTION: EXE 0x51b131
// Draws the frame's pointer transparently over the current save-under rectangle, shifted by inOrigin
void RTSMouse::Draw(GESurface* inSurface, const GE2DPoint& inOrigin)
{
    GERectangle theSource;
    GERectangle theDestination;
    theDestination = this->mCurrentSaveUnder->mRect;
    theDestination.SetLeft(theDestination.GetLeft() + inOrigin.GetX());
    theDestination.SetTop(theDestination.GetTop() + inOrigin.GetY());
    theDestination.SetRight(theDestination.GetRight() + inOrigin.GetX());
    theDestination.SetBottom(theDestination.GetBottom() + inOrigin.GetY());
    theSource.SetTop(0.0f);
    theSource.SetLeft(0.0f);
    theSource.SetBottom(theDestination.GetBottom() - theDestination.GetTop());
    theSource.SetRight(theDestination.GetRight() - theDestination.GetLeft());
    this->mCurrentSurface->CopyTo(inSurface, theSource, theDestination, true);
}

// FUNCTION: EXE 0x51b1dd
// Starts a pointer frame: enters the render lock (EndFrame leaves it), takes the position and pointer (in shift mode
// the pointer with the overlay drawn over it, composed in spare slot 9) and swaps the save-under records
void RTSMouse::BeginFrame()
{
    this->mRenderLock->Enter();
    this->mInFrame = true;
    this->mDrawnPosition = this->mPosition;
    this->mCurrentSurface = this->GetPointerSurface(this->mPointer);
    this->mCurrentSurface->Reference();
    if (this->mShiftMode) {
        GESurface* thePlus = this->GetPointerSurface(9);
        GESurface* thePointer = this->GetPointerSurface(this->mPointer);
        GESurface* theOverlay = this->GetPointerSurface(this->mOverlay);
        float theWidth = (float)this->mCurrentSurface->GetWidth();
        float theHeight = (float)this->mCurrentSurface->GetHeight();
        GERectangle theDestination;
        GERectangle theSource;
        theDestination.SetLeft(0.0f);
        theDestination.SetTop(0.0f);
        theDestination.SetRight(theWidth);
        theDestination.SetBottom(theHeight);
        theSource.SetLeft(0.0f);
        theSource.SetTop(0.0f);
        theSource.SetRight(theWidth);
        theSource.SetBottom(theHeight);
        if (thePointer->CopyTo(thePlus, theSource, theDestination, false) == kErrorOK) {
            theSource.SetLeft(0.0f);
            theSource.SetTop(0.0f);
            theSource.SetRight(theWidth);
            theSource.SetBottom(theHeight);
            theDestination.SetLeft(0.0f);
            theDestination.SetTop(0.0f);
            theDestination.SetRight(theWidth);
            theDestination.SetBottom(theHeight);
            if (theOverlay->CopyTo(thePlus, theSource, theDestination, true) == kErrorOK) {
                this->mCurrentSurface->Dereference();
                this->mCurrentSurface = thePlus;
                thePlus->Reference();
            }
        }
    }
    RTSMouseSaveUnder* theSaveUnder = this->mCurrentSaveUnder;
    this->mCurrentSaveUnder = this->mPreviousSaveUnder;
    this->mPreviousSaveUnder = theSaveUnder;
}

// FUNCTION: EXE 0x51b31c
// Keeps the pointer position inside the bounds (the right and bottom edges excluded)
void RTSMouse::ClampPosition()
{
    if (this->mPosition.GetX() < this->mBounds.GetLeft())
        this->mPosition.SetX(this->mBounds.GetLeft());
    else if (this->mPosition.GetX() >= this->mBounds.GetRight())
        this->mPosition.SetX(this->mBounds.GetRight() - 1.0f);
    if (this->mPosition.GetY() < this->mBounds.GetTop())
        this->mPosition.SetY(this->mBounds.GetTop());
    else if (this->mPosition.GetY() >= this->mBounds.GetBottom())
        this->mPosition.SetY(this->mBounds.GetBottom() - 1.0f);
}

// FUNCTION: EXE 0x51b373
// Releases the pointer textures, both save-under surfaces and the frame's pointer; returns 0
long RTSMouse::Release()
{
    for (unsigned long i = 0; i < 10; i++) {
        if (this->mPointerSurfaces[i] != NULL) {
            this->mPointerSurfaces[i]->Dereference();
            this->mPointerSurfaces[i] = NULL;
        }
    }
    if (this->mSaveUnder24.mSurface != NULL) {
        this->mSaveUnder24.mSurface->Dereference();
        this->mSaveUnder24.mSurface = NULL;
    }
    if (this->mSaveUnder38.mSurface != NULL) {
        this->mSaveUnder38.mSurface->Dereference();
        this->mSaveUnder38.mSurface = NULL;
    }
    if (this->mCurrentSurface != NULL) {
        this->mCurrentSurface->Dereference();
        this->mCurrentSurface = NULL;
    }
    return 0;
}

// FUNCTION: EXE 0x51b3cc
// Moves the pointer by the raw mouse axes (scaled by bounds over desktop, sensitivity and the smallest move seen)
// unless frozen, clamps it, and reads the wheel (0 without one)
void RTSMouse::Update()
{
    float theDeltaX = 0.0f;
    float theDeltaY = 0.0f;
    float theScaleX = (this->mBounds.GetRight() - this->mBounds.GetLeft()) / this->mDesktopWidth * this->mSensitivity;
    float theScaleY = (this->mBounds.GetBottom() - this->mBounds.GetTop()) / this->mDesktopHeight * this->mSensitivity;
    ISEntity* theAxisX;
    ISEntity* theAxisY;
    ISEntity* theWheel = NULL;
    gISMouse->GetEntity(kMouseXAxis, theAxisX);
    gISMouse->GetEntity(kMouseYAxis, theAxisY);
    gISMouse->GetEntity(kMouseZAxis, theWheel);
    if (!this->mIsFrozen) {
        long theMoveX = theAxisX->GetNormalizedValue();
        long theMoveY = theAxisY->GetNormalizedValue();
        if (theMoveX != 0 || theMoveY != 0) {
            if (theMoveX != 0) {
                if ((theMoveX < 0 ? -theMoveX : theMoveX) < sMinXValue) {
                    sMinXValue = theMoveX < 0 ? -theMoveX : theMoveX;
                    ESDebugOut("!MinXValue now %d\n", sMinXValue);
                }
                theDeltaX = (theScaleX + 0.0625f) * (float)theMoveX * 4.0f / sMinXValue;
            }
            if (theMoveY != 0) {
                if ((theMoveY < 0 ? -theMoveY : theMoveY) < sMinYValue) {
                    sMinYValue = theMoveY < 0 ? -theMoveY : theMoveY;
                    ESDebugOut("!MinYValue now %d\n", sMinYValue);
                }
                theDeltaY = (theScaleY + 0.0625f) * (float)theMoveY * 4.0f / sMinYValue;
            }
            this->mPosition.SetX(theDeltaX + this->mPosition.mX);
            this->mPosition.SetY(theDeltaY + this->mPosition.mY);
            this->ClampPosition();
        }
    }
    if (theWheel != NULL)
        this->mWheel = theWheel->GetValue();
    else
        this->mWheel = 0;
}
