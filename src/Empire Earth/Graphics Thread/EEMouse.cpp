// C:\EEX\Empire Earth\Graphics Thread\EEMouse.cpp (file name inferred: no __FILE__). The game's mouse (RTTI EEMouse >
// RTSMouse, vtable 0x441320): 0x10a9b0..0x10aa88 (the ctor, the three slots, ??_G with the inline dtor). The pointer
// file table 0x4be124 is this unit's .data. Unit flags: the exe's module set.
#include <windows.h>

#include "lle/LowLevelEngine.h"

#include "ee/EEMouse.h"

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

// (the mouse's C++ calls: EERenderSnapshot.cpp's EERoViewsOp)
