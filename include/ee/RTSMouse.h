// C:\EEX\RTS Engine\Graphics\RTSMouse.h (RECONSTRUCTION; file name inferred). RTTI RTSMouse
// (vtable 0x441c74); RTSMouse.cpp: ctor 0x11ab85 (its asserts name mRenderLock/mInputLock), dtor 0x11acda.
// sizeof 0xa8. Slots named by offset; member names provisional unless named.
// Moved out of ee/EEMouse.h (R5a.6): the RTS layer's mouse without the game's header (EEMouse.h includes it).
#ifndef EE_RTSMOUSE_H
#define EE_RTSMOUSE_H

#include "lle/GERectangle.h"           // RTSMouse::mBounds

class GEScreen;
class GESurface;
class TSCriticalSection;

struct EeGfxMouseFrame;                      // ee_gfx mouse_frame.rs

class RTSMouse {
public:
    RTSMouse();
    virtual ~RTSMouse();                    // +0x00 (??_G 0x11acbe)
    virtual long Load(GEScreen* inScreen);        // +0x04 0x11b02b (calls inScreen's GetCurrentMode; EEGraphics passes the game screen; returns the load result)
    virtual long Release();                 // +0x08 0x11b373 (returns 0)
    // 0x11af66: finds inFile on the texture path and loads it into *ioSurface on inScreen (EEMouse::Load passes
    // its 14 pointer files; provisional name)
    long LoadPointer(const char* inFile, GEScreen* inScreen, GESurface** ioSurface);
    void Update();                          // 0x11b3cc (under mInputLock after ISMouse::RefreshXY; provisional name)
    // drawn by RTSGraphics::Render 0x11a71c in this order (provisional names)
    void BeginFrame();
    void SaveUnder(GESurface* inSurface, const GE2DPoint& inOrigin);
    void Draw(GESurface* inSurface, const GE2DPoint& inOrigin);
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
    // +0x14: the bounds the position is clamped to (SetBounds; GERectangle's operator= order top/left/bottom/right:
    // +0x20/+0x18/+0x14/+0x1c) (provisional name)
    GERectangle mBounds;
    // port (R4.4, twenty-sixth round): the frame's state, ee_gfx mouse_frame.rs's: the drawn position (+0x0c), the two
    // save-under records (+0x24/+0x38, each a rectangle and a 32x32 surface; BeginFrame swaps the current and
    // previous, +0x80/+0x84), the ten pointer pictures (+0x4c; EEMouse's fourteen, +0xa8, after them), the frame's
    // pointer (+0x74), the in-frame and just-loaded flags (+0xa1, +0xa3)
    EeGfxMouseFrame* mFrame;
    TSCriticalSection* mRenderLock;         // +0x78
    TSCriticalSection* mInputLock;          // +0x7c
    float mSensitivity;                     // +0x88
    unsigned long mDesktopHeight;           // +0x8c (the desktop height; unsigned: Update divides by it through a zero-extended qword temp)
    unsigned long mDesktopWidth;            // +0x90 (the desktop width; unsigned, likewise)
    long mWheel;                            // +0x94
    volatile long mPointer;                 // +0x98 (volatile: EEMSavedGroupSelect stores `mov dword ptr [x], 0`)
    volatile long mOverlay;                 // +0x9c (= 8; volatile like mPointer: BeginFrame keeps the push next to the call, 0x11b240)
    volatile bool mShiftMode;               // +0xa0 (volatile: `mov byte ptr [x], 0`, 0xbe1b7) set with the local player's shift mode (0xbe076; provisional name)
    bool mBA2;                              // +0xa2
    bool mIsFrozen;                         // +0xa4
};

// New (R5a.6): the mouse the input side updates, the game's gMouse (defined in Empire Earth.cpp), so that the RTS
// layer does not include the game's mouse header for it; RTSMousePointer's thread reads it at each pass, as it read
// gMouse.
RTSMouse* RTSGetInputMouse();

#endif
