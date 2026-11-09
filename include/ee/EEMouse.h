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

#include "ee/RTSMouse.h"            // RTSMouse, RTSMouseSaveUnder (the RTS layer's header since R5a.6)

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
