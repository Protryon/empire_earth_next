// EEMouse.h (RECONSTRUCTION; file name inferred). The game's mouse (RTTI EEMouse > RTSMouse; vtable 0x441320,
// sizeof 0xe0, ctor 0x10a9b0), created at startup into gMouse (0x531924, 0x1423d1; provisional name). Only what the
// decompiled units read. Requires lle/GE2DPoint.h (and lle/GERectangle.h, included).
#ifndef EE_EEMOUSE_H
#define EE_EEMOUSE_H


class GEScreen;
class GESurface;
class TSCriticalSection;
class UReferenceCounter;

#include "ee/RTSMouse.h"            // RTSMouse, RTSMouseSaveUnder (the RTS layer's header since R5a.6)
#include "rust/game/gamecpp.h"

class EEMouse : public RTSMouse {
public:
    EEMouse();                              // 0x10a9b0: RTSMouse(), the vptr, +0xa8..+0xe0 zeroed
    // no out-of-line dtor in EEMouse.cpp's range in the original (??_G 0x10aa66 stored the vptr and called ~RTSMouse);
    // port: it drops the members' registration for the Rust readers (ee_gfx input_views.rs)
    // +0x00 (??_G 0x10aa66)
    virtual ~EEMouse() { ee_gfx_input_mouse_set(NULL); }
    // Loads the 14 extra pointer images (convert, transport, calamity, rally, heal, patrol, garrison, flare,
    // attack ground...), then the engine's pointers; returns the first load error (gamecpp ro_views.rs)
    // +0x04 0x10a9f1 (loads the 14 extra pointers, then RTSMouse's)
    virtual long Load(GEScreen* inScreen) { return ee_gamecpp_mo_load(this, inScreen); }
    // Releases the extra pointer images, then the engine's; returns the engine's result (gamecpp ro_views.rs)
    // +0x08 0x10aa38 (dereferences them)
    virtual long Release() { return ee_gamecpp_mo_release(this); }
    // +0x0c 0x10a9d1 (10..23: its own pointers) is RTSMouse's since R4.4's twenty-sixth round: the pointers are ee_gfx
    // mouse_frame.rs's slots, EEMouse's fourteen (+0xa8 in the original) in 10..23
};

extern EEMouse* gMouse;

#endif
