// EEMouse.h (RECONSTRUCTION; file name inferred). The game's mouse (RTTI EEMouse > RTSMouse; vtable 0x441320,
// sizeof 0xe0, ctor 0x10a9b0), created at startup into gMouse (0x531924, 0x1423d1; provisional name). Only what the
// decompiled units read. Requires lle/GE2DPoint.h (and lle/GERectangle.h, included).
#ifndef EE_EEMOUSE_H
#define EE_EEMOUSE_H

#include "lle/GERectangle.h"           // RTSMouse::mBounds

class GEScreen;
class GESurface;
class TSCriticalSection;
class UReferenceCounter;

// C:\EEX\RTS Engine\Graphics\RTSMouse.h (RECONSTRUCTION; file name inferred). RTTI RTSMouse
// (vtable 0x441c74); RTSMouse.cpp: ctor 0x11ab85 (its asserts name mRenderLock/mInputLock), dtor 0x11acda.
// sizeof 0xa8. Slots named by offset; member names provisional unless named.
// The area under the pointer, saved before it is drawn (0x14 bytes; RTSMouse +0x24/+0x38; Virtual04 0x11b02b creates
// mSurface on the screen, SaveUnder copies the screen into it, RestoreUnder copies it back) (provisional name)
struct RTSMouseSaveUnder {
    GERectangle mRect;                      // +0x00
    GESurface* mSurface;                    // +0x10
};

class RTSMouse {
public:
    RTSMouse();                             // 0x11ab85
    virtual ~RTSMouse();                    // +0x00 (??_G 0x11acbe)
    virtual long Load(GEScreen* inScreen);        // +0x04 0x11b02b (calls inScreen's GetCurrentMode; EEGraphics passes the game screen; returns the load result)
    virtual long Release();                 // +0x08 0x11b373 (returns 0)
    virtual GESurface* GetPointerSurface(unsigned long inIndex);  // +0x0c 0x11af52: mPointerSurfaces[inIndex] below 10, else NULL
    // 0x11af66: finds inFile on the texture path and loads it into *ioSurface on inScreen (EEMouse::Load passes
    // its 14 pointer files; provisional name)
    long LoadPointer(const char* inFile, GEScreen* inScreen, GESurface** ioSurface);
    void Update();                          // 0x11b3cc (under mInputLock after ISMouse::RefreshXY; provisional name)
    // drawn by RTSGraphics::Render 0x11a71c in this order (provisional names)
    void BeginFrame();                      // 0x11b1dd
    void SaveUnder(GESurface* inSurface, const GE2DPoint& inOrigin);  // 0x11acf9
    void Draw(GESurface* inSurface, const GE2DPoint& inOrigin);       // 0x11b131
    long RestoreUnder(GESurface* inSurface, const GE2DPoint& inOrigin, bool inIsWindowed);  // 0x11ae5d (the original returns the CopyTo result, 0 on the first call: re-mangle names.txt 11ae5d with any retype)
    void EndFrame();                        // 0x10a94c: releases +0x74, clears +0xa1, leaves mRenderLock
    // inline: the game settings store the mouse sensitivity value first, then load gMouse (0x1d6c80, 0x1d88eb;
    // provisional name)
    void SetSensitivity(float inSensitivity) { this->mSensitivity = inSensitivity; }
    // inline (out-of-line COMDAT 0x140976 after its first user in Empire Earth.cpp 0x140306; 10 callers in 8 units)
    void SetBounds(const GERectangle& inBounds) { this->mBounds = inBounds; this->ClampPosition(); }
    // inline: Open 0x266038 loads gMouse once as `this` for the store and ClampPosition
    void SetPosition(const GE2DPoint& inPosition) { this->mPosition = inPosition; this->ClampPosition(); }
    void ClampPosition();                   // 0x11b31c: mPosition into mBounds (provisional name)

    GE2DPoint mPosition;                    // +0x04 (provisional name; the unit calamities pick the unit under it)
    GE2DPoint mDrawnPosition;               // +0x0c (the ctor zeroes it right after mPosition; BeginFrame copies mPosition into it, SaveUnder saves under it) (provisional name)
    // +0x14: the bounds the position is clamped to (SetBounds; GERectangle's operator= order top/left/bottom/right:
    // +0x20/+0x18/+0x14/+0x1c) (provisional name)
    GERectangle mBounds;
    // +0x24/+0x38: the two save-under records (the ctor zeroes both rectangles and points mCurrentSaveUnder /
    // mPreviousSaveUnder at them; BeginFrame swaps the pointers each frame)
    RTSMouseSaveUnder mSaveUnder24;         // +0x24
    RTSMouseSaveUnder mSaveUnder38;         // +0x38
    GESurface* mPointerSurfaces[10];        // +0x4c (zeroed with one rep stosd; Virtual04 loads the pointer files into them, Virtual08 dereferences them)
    GESurface* mCurrentSurface;             // +0x74 (EndFrame dereferences and clears it; the current pointer surface: BeginFrame = Virtual0C(..), GetWidth/GetHeight/CopyTo)
    TSCriticalSection* mRenderLock;         // +0x78
    TSCriticalSection* mInputLock;          // +0x7c
    RTSMouseSaveUnder* mCurrentSaveUnder;   // +0x80 (= &mSaveUnder24)
    RTSMouseSaveUnder* mPreviousSaveUnder;  // +0x84 (= &mSaveUnder38)
    float mSensitivity;                     // +0x88
    unsigned long mDesktopHeight;           // +0x8c (the desktop height; unsigned: Update divides by it through a zero-extended qword temp)
    unsigned long mDesktopWidth;            // +0x90 (the desktop width; unsigned, likewise)
    long mWheel;                            // +0x94
    volatile long mPointer;                 // +0x98 (volatile: EEMSavedGroupSelect stores `mov dword ptr [x], 0`)
    volatile long mOverlay;                 // +0x9c (= 8; volatile like mPointer: BeginFrame keeps the push next to the call, 0x11b240)
    volatile bool mShiftMode;               // +0xa0 (volatile: `mov byte ptr [x], 0`, 0xbe1b7) set with the local player's shift mode (0xbe076; provisional name)
    bool mInFrame;                          // +0xa1
    bool mBA2;                              // +0xa2
    bool mIsJustLoaded;                     // +0xa3 (= true)
    bool mIsFrozen;                         // +0xa4
};

class EEMouse : public RTSMouse {
public:
    EEMouse();                              // 0x10a9b0: RTSMouse(), the vptr, +0xa8..+0xe0 zeroed
    // inline: no out-of-line dtor in EEMouse.cpp's range; ??_G 0x10aa66 stores the vptr and calls ~RTSMouse
    virtual ~EEMouse() {}                   // +0x00 (??_G 0x10aa66)
    virtual long Load(GEScreen* inScreen);        // +0x04 0x10a9f1 (loads the 14 extra pointers, then RTSMouse's)
    virtual long Release();                 // +0x08 0x10aa38 (dereferences them)
    virtual GESurface* GetPointerSurface(unsigned long inIndex);  // +0x0c 0x10a9d1 (10..23: mLA8[inIndex - 10])

    GESurface* mExtraPointerSurfaces[14];   // +0xa8 the extra pointers (zeroed with one rep stosd of 14; LoadPointer loads them)
};

extern EEMouse* gMouse;                     // 0x531924

#endif
