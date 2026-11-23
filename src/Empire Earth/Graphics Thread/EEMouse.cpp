// C:\EEX\Empire Earth\Graphics Thread\EEMouse.cpp (file name inferred: no __FILE__). The game's mouse (RTTI EEMouse >
// RTSMouse, vtable 0x441320): 0x10a9b0..0x10aa88 (the ctor, the three slots, ??_G with the inline dtor). The pointer
// file table 0x4be124 is this unit's .data. Unit flags: the exe's module set.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GESurface.h"

#include "ee/EEMouse.h"
#include "rust/render/gfx.h"
#include "rust/game/gamecpp.h"

// the extra mouse pointers' file table (0x4be124: 14 pointers in .data): gamecpp ro_views.rs

// Creates the game's mouse with none of its 14 extra pointer images loaded (the frame's slots 10..23); port: its
// members are registered for the Rust readers (ee_gfx input_views.rs: the UI's)
EEMouse::EEMouse()
{
    EeGfxMouseView theView = {reinterpret_cast<EeGe2DPoint*>(&this->mPosition), reinterpret_cast<EeGeRectangle*>(&this->mBounds),
                              &this->mWheel, const_cast<long*>(&this->mPointer), const_cast<long*>(&this->mOverlay),
                              &this->mIsFrozen, const_cast<bool*>(&this->mShiftMode), &this->mSensitivity};
    ee_gfx_input_mouse_set(&theView);
}

EEMouse::~EEMouse()
{
    ee_gfx_input_mouse_set(NULL);
}

// The frame's slots and calls (RTSMouse.cpp)
GESurface** RTSMouseSlot(RTSMouse* inMouse, unsigned long inIndex);
long RTSMouseFrameCall(RTSMouse* inMouse, uint32_t inOp, GESurface* inSurface, const GE2DPoint* inOrigin, bool inFlag);

// Loads the 14 extra pointer images (convert, transport, calamity, rally, heal, patrol, garrison, flare,
// attack ground...), then the engine's pointers; returns the first load error (gamecpp ro_views.rs)
long EEMouse::Load(GEScreen* inScreen)
{
    return ee_gamecpp_mo_load(this, inScreen);
}

// Releases the extra pointer images, then the engine's; returns the engine's result (gamecpp ro_views.rs)
long EEMouse::Release()
{
    return ee_gamecpp_mo_release(this);
}

// ------------------------------------------------------------------------- round 10, lane A: the mouse's C++ (gamecpp ro_views.rs)

int64_t EECore10OpB(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);
double EECore10OpBF(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);

#define MO_MOUSE ((EEMouse*)p)

// the C++ calls of gamecpp ro_views.rs' mouse (EERenderSnapshot.cpp's EERoViewsOp default; the lane's last switch,
// whose default is lane B's link): one statement each
int64_t EEMouseOp(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    case O::MoLoadPointer: return MO_MOUSE->LoadPointer((const char*)a[0], (GEScreen*)a[1], RTSMouseSlot(MO_MOUSE, 10 + (unsigned long)a[2]));
    case O::MoRtsLoad: return MO_MOUSE->RTSMouse::Load((GEScreen*)a[0]);
    case O::MoReleaseExtra: return RTSMouseFrameCall(MO_MOUSE, 5, NULL, NULL, false);
    case O::MoRtsRelease: return MO_MOUSE->RTSMouse::Release();
    default: return EECore10OpB(inOp, p, a, f);
    }
}

double EEMouseOpF(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    return EECore10OpBF(inOp, p, a, f);
}
