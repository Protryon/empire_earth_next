// C:\EEX\RTS Engine\Graphics\RTSMouse.h (RECONSTRUCTION; file name inferred). RTTI RTSMouse
// (vtable 0x441c74); RTSMouse.cpp: ctor 0x11ab85 (its asserts name mRenderLock/mInputLock), dtor 0x11acda.
// sizeof 0xa8. Slots named by offset; member names provisional unless named.
// Moved out of ee/EEMouse.h (R5a.6): the RTS layer's mouse without the game's header (EEMouse.h includes it).
#ifndef EE_RTSMOUSE_H
#define EE_RTSMOUSE_H

#include "rust/game/rts.h"              // EeRtsMouseFields (RTSMouse' plain members)
#include "rust/render/gfx.h"

class GEScreen;
class GESurface;
class TSCriticalSection;

struct EeGfxMouseFrame;                      // ee_gfx mouse_frame.rs

// LLE round 8: the bodies are Rust (ee_game rts mouse.rs); the plain members are its record EeRtsMouseFields, a base
// (mSensitivity, mDesktopHeight, mDesktopWidth, mWheel, mPointer, mOverlay, mShiftMode, mBA2, mIsFrozen).
class RTSMouse : public EeRtsMouseFields {
public:
    RTSMouse() : mFrame(ee_gfx_mouse_frame_new()) { ee_rts_mouse_construct(this); }
    virtual ~RTSMouse();                    // +0x00 (??_G 0x11acbe)
    // +0x04 0x11b02b (calls inScreen's GetCurrentMode; EEGraphics passes the game screen; returns the load result)
    virtual long Load(GEScreen* inScreen) { return ee_rts_mouse_load(this, inScreen); }
    virtual long Release();                 // +0x08 0x11b373 (returns 0)
    // 0x11b3cc (under mInputLock after ISMouse::RefreshXY; provisional name)
    void Update() { ee_rts_mouse_update(this); }
    void SaveUnder(GESurface* inSurface, const GE2DPoint& inOrigin);
    void Draw(GESurface* inSurface, const GE2DPoint& inOrigin);
    long RestoreUnder(GESurface* inSurface, const GE2DPoint& inOrigin, bool inIsWindowed);  // 0x11ae5d (the original returns the CopyTo result, 0 on the first call: re-mangle names.txt 11ae5d with any retype)
    // inline: the game settings store the mouse sensitivity value first, then load gMouse (0x1d6c80, 0x1d88eb;
    // provisional name)
    void SetSensitivity(float inSensitivity) { this->mSensitivity = inSensitivity; }
    // inline (out-of-line COMDAT 0x140976 after its first user in Empire Earth.cpp 0x140306; 10 callers in 8 units)
    void SetBounds(const GERectangle& inBounds) { this->mBounds = inBounds; this->ClampPosition(); }
    // inline: Open 0x266038 loads gMouse once as `this` for the store and ClampPosition
    void SetPosition(const GE2DPoint& inPosition) { this->mPosition = inPosition; this->ClampPosition(); }
    // 0x11b31c: mPosition into mBounds (provisional name)
    void ClampPosition()
        { ee_gfx_mouse_clamp(reinterpret_cast<EeGe2DPoint*>(&this->mPosition), reinterpret_cast<const EeGeRectangle*>(&this->mBounds)); }

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
};

#endif
