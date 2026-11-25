// C:\EEX\Empire Earth\Graphics Thread\EEROUserInterface.cpp (no __FILE__; name from the RTTI class). The render
// object that draws the UI (vtable 0x441bec): 0x118741..0x118aa2 (the viewport's init chain, the ctor, ??_G, the
// dtor, the four slots); EEUnitPicker.cpp (EEROWater first) starts at 0x118aa2. The three floats after the vtable
// (0x441c00..0x441c08) are this unit's named consts. Unit flags: the exe's module set.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/GEViewport.h"


// the UI's viewport (0x529518) (provisional name)
GEViewport gUIViewport;

// the panel corners of the z-buffer clears (.rdata 0x441c00..) and the unit panel's flag (0x5297a0): gamecpp ro_views.rs,
// ro_units.rs
