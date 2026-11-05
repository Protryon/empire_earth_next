// DX7 Display Plug-ins: shared declarations of the renderer plug-ins (DX7HRDisplay.dll, DX7HRTnLDisplay.dll).
// RECONSTRUCTION, provisional file name: the original header layout under C:\EEX\Low-Level Engine\Graphics Engine\
// DX7 Display Plug-ins\Common\ is unknown (no header __FILE__ in either binary). Every DX7 class is exported by the
// plug-in (its copy ctor/operator=/vtable are exports), so the classes are dllexport.
#ifndef DX7_DX7COMMON_H
#define DX7_DX7COMMON_H

#include "gx/GX.h"

// The game (exe view, LLE_EXE_HEADERS) sees DX7Screen only through the rasterizer's screen pointer
// (EELobbyInterface.cpp 0x29754d: FlipToGDISurface through the screen's GXDirectDraw): no export, no import.
#ifdef LLE_EXE_HEADERS
#define DX7_API
#else
#define DX7_API __declspec(dllexport)
#endif

#endif
