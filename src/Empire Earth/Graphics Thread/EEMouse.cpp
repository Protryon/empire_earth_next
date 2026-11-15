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

// FUNCTION: EXE 0x50a9b0
// Creates the game's mouse with none of its 14 extra pointer images loaded
EEMouse::EEMouse()
{
    for (long i = 0; i < 14; i++)
        this->mExtraPointerSurfaces[i] = NULL;
}

// FUNCTION: EXE 0x50a9d1
// Returns the image of a mouse pointer: the game's extra pointers for 10..23, else the engine's
GESurface* EEMouse::GetPointerSurface(unsigned long inIndex)
{
    if (inIndex >= 10 && inIndex <= 23)
        return this->mExtraPointerSurfaces[inIndex - 10];
    return RTSMouse::GetPointerSurface(inIndex);
}

// FUNCTION: EXE 0x50a9f1
// Loads the 14 extra pointer images (convert, transport, calamity, rally, heal, patrol, garrison, flare,
// attack ground...), then the engine's pointers; returns the first load error
long EEMouse::Load(GEScreen* inScreen)
{
    long theResult = kErrorOK;
    for (long i = 0; i < 14 && theResult == kErrorOK; i++)
        theResult = this->LoadPointer(sPointerFiles[i], inScreen, &this->mExtraPointerSurfaces[i]);
    if (theResult == kErrorOK)
        theResult = RTSMouse::Load(inScreen);
    return theResult;
}

// FUNCTION: EXE 0x50aa38
// Releases the extra pointer images, then the engine's; returns the engine's result
long EEMouse::Release()
{
    for (long i = 0; i < 14; i++) {
        if (this->mExtraPointerSurfaces[i] != NULL) {
            this->mExtraPointerSurfaces[i]->Dereference();
            this->mExtraPointerSurfaces[i] = NULL;
        }
    }
    return RTSMouse::Release();
}

// SYNTHETIC: EXE 0x50aa66
// (the scalar deleting destructor, with EEMouse's inline dtor)
