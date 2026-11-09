// C:\EEX\Low-Level Engine\Graphics Engine\DX7 Display Plug-ins\DX7HRTnLDisplay\DX7TnLRasterizer.cpp (folder and file
// name inferred: no __FILE__ in DX7HRTnLDisplay.dll names it; it sorts after Common\DX7Surface.cpp in link order)
// Unit flags (hypothesis, docs/CALIBRATION.md "dx7hr"): -O1 -G6 -MD -GR
//
// DX7HRTnLDisplay.dll 0x1000fd16..0x10010512: the U2D key init chains, the TnL device enumeration callback, then
// DX7TnLRasterizer's ctor and dtor and its members in alphabetical order, then the template-static init chain head
// and its atexit registrars.
#include <windows.h>
#include "gx/GX.h"
#include <float.h>

#include "dx7/DX7Rasterizer.h"
#include "dx7/DX7TnLRasterizer.h"
#include "dx7/DX7Surface.h"
#include "dx7/DX7Screen.h"
#include "rust/ee_rust.h"

// The U2D key init chains:
// SYNTHETIC: DX7HRTNL 0x1000fd16
// SYNTHETIC: DX7HRTNL 0x1000fd20
// SYNTHETIC: DX7HRTNL 0x1000fd31
// SYNTHETIC: DX7HRTNL 0x1000fd3d
// SYNTHETIC: DX7HRTNL 0x1000fd48
// SYNTHETIC: DX7HRTNL 0x1000fd52
// SYNTHETIC: DX7HRTNL 0x1000fd63
// SYNTHETIC: DX7HRTNL 0x1000fd6f
// SYNTHETIC: DX7HRTNL 0x1000fd7a
// SYNTHETIC: DX7HRTNL 0x1000fd84
// SYNTHETIC: DX7HRTNL 0x1000fd95
// SYNTHETIC: DX7HRTNL 0x1000fda1
// SYNTHETIC: DX7HRTNL 0x1000fdac
// SYNTHETIC: DX7HRTNL 0x1000fdb6
// SYNTHETIC: DX7HRTNL 0x1000fdc7
// SYNTHETIC: DX7HRTNL 0x1000fdd3
// SYNTHETIC: DX7HRTNL 0x1000fdde
// SYNTHETIC: DX7HRTNL 0x1000fde8
// SYNTHETIC: DX7HRTNL 0x1000fdf9
// SYNTHETIC: DX7HRTNL 0x1000fe05
// SYNTHETIC: DX7HRTNL 0x1000fe10
// SYNTHETIC: DX7HRTNL 0x1000fe1a
// SYNTHETIC: DX7HRTNL 0x1000fe2b
// SYNTHETIC: DX7HRTNL 0x1000fe37
// SYNTHETIC: DX7HRTNL 0x1000fe42
// SYNTHETIC: DX7HRTNL 0x1000fe4c
// SYNTHETIC: DX7HRTNL 0x1000fe5d
// SYNTHETIC: DX7HRTNL 0x1000fe69

// FUNCTION: DX7HRTNL 0x1000feba
// Creates the T&L rasterizer under the given name
DX7TnLRasterizer::DX7TnLRasterizer(const UString& inName) : DX7Rasterizer(inName)
{
}

// FUNCTION: DX7HRTNL 0x1000fed2
// Destroys the T&L rasterizer (base members only)
DX7TnLRasterizer::~DX7TnLRasterizer()
{
}

// FUNCTION: DX7HRTNL 0x1000fedd
// Creates the T&L HAL device on the back buffer, sets the viewport, fixed render states and vertex buffer, then
// resets the cached states (rust/crates/ee_dx7/src/raster.rs); on a CreateDevice failure the GXDirect3D interface is released
long DX7TnLRasterizer::ActivateTheScreen(unsigned long inScreen, GESurface* inPrimary, GESurface* inSecondary,
                                         GETextureManager* inManager, GEWindow* inWindow)
{
    DX7Surface* theTarget = dynamic_cast<DX7Surface*>(inSecondary);
    return ee_dx7r_activate_the_screen(reinterpret_cast<EeDx7ScreenState*>(this->mDX7ScreenStates[inScreen]),
                                       static_cast<DX7Screen*>(this->mDX7ScreenStates[inScreen]->mScreen)->mDirectDraw,
                                       (GXSurface*)*theTarget, theTarget->GetWidth(), theTarget->GetHeight(), true);
}

// FUNCTION: DX7HRTNL 0x10010391
// Creates the screen's write-only vertex buffer (2400 vertices: position, normal, colour, one texture)
long DX7TnLRasterizer::CreateVertexBuffers(unsigned long inScreen)
{
    return ee_dx7r_create_vertex_buffers(reinterpret_cast<EeDx7ScreenState*>(this->mDX7ScreenStates[inScreen]), true);
}

// FUNCTION: DX7HRTNL 0x10010410
// Tells whether a screen's DirectDraw object offers a device with hardware T&L and rasterization
bool DX7TnLRasterizer::GetIsScreenValid(DX7Screen* inScreen)
{
    return ee_dx7r_is_screen_valid((GXDirectDraw*)*inScreen, true);
}

// The template-static init chain head and its atexit registrars:
// SYNTHETIC: DX7HRTNL 0x100104ba
// SYNTHETIC: DX7HRTNL 0x100104e2
// SYNTHETIC: DX7HRTNL 0x100104ee
// SYNTHETIC: DX7HRTNL 0x100104fa
// SYNTHETIC: DX7HRTNL 0x10010506
