// C:\EEX\Empire Earth\Graphics Thread\EEMouse.cpp (file name inferred: no __FILE__). The game's mouse (RTTI EEMouse >
// RTSMouse, vtable 0x441320): 0x10a9b0..0x10aa88 (the ctor, the three slots, ??_G with the inline dtor). The pointer
// file table 0x4be124 is this unit's .data. Unit flags: the exe's module set.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GESurface.h"

#include "ee/EEMouse.h"

// the extra mouse pointers, loaded in this order into mLA8 (0x4be124: 14 pointers in .data) (provisional name)
static const char* sPointerFiles[14] = {
    "Textures\\Convert Pointer.png",
    "Textures\\Board Transport Pointer.png",
    "Textures\\Leave Transport Pointer.png",
    "Textures\\Calamity Pointer.png",
    "Textures\\Invalid Calamity Pointer.png",
    "Textures\\Rally Point Pointer.png",
    "Textures\\Protected Conversion Pointer.png",
    "Textures\\Protected Calamity Pointer.png",
    "Textures\\Heal.png",
    "Textures\\Invalid Heal.png",
    "Textures\\Patrol Pointer.png",
    "Textures\\Garrison Pointer.png",
    "Textures\\Flare Pointer.png",
    "Textures\\Attack Ground Pointer.png",
};

// Creates the game's mouse with none of its 14 extra pointer images loaded (the frame's slots 10..23)
EEMouse::EEMouse()
{
}

// The frame's slots and calls (RTSMouse.cpp)
GESurface** RTSMouseSlot(RTSMouse* inMouse, unsigned long inIndex);
long RTSMouseFrameCall(RTSMouse* inMouse, uint32_t inOp, GESurface* inSurface, const GE2DPoint* inOrigin, bool inFlag);

// Loads the 14 extra pointer images (convert, transport, calamity, rally, heal, patrol, garrison, flare,
// attack ground...), then the engine's pointers; returns the first load error
long EEMouse::Load(GEScreen* inScreen)
{
    long theResult = kErrorOK;
    for (long i = 0; i < 14 && theResult == kErrorOK; i++)
        theResult = this->LoadPointer(sPointerFiles[i], inScreen, RTSMouseSlot(this, 10 + i));
    if (theResult == kErrorOK)
        theResult = RTSMouse::Load(inScreen);
    return theResult;
}

// Releases the extra pointer images, then the engine's; returns the engine's result
long EEMouse::Release()
{
    RTSMouseFrameCall(this, 5, NULL, NULL, false);
    return RTSMouse::Release();
}
