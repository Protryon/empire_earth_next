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

// (the mouse's C++ calls: EERenderSnapshot.cpp's EERoViewsOp)
