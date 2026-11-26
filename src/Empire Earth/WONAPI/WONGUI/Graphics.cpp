// Graphics (R6 WONGUI B round 1): translated drawing, colours and the clip stack are ee_won's gfxobj.rs, the draw
// plans run in gfxrun.rs (round 38), and the Graphics is gfxrun.rs's record, made, attached and freed there (round
// 39). This unit keeps the host (the font's draw, a wrapped text's) and the Windows-style frames.
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <stddef.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONGUIDraw.h"
using namespace WONAPI;
typedef EeWonGfxHook H;
// the Rust Graphics' calls out that are not the surface's nor Rust's (gfxplat.rs, gfxrun.rs): the font's draw (a[]
// its ints, p the text) and a wrapped text's paint
static uint32_t GfxHost(void* inGraphics, EeWonGfxHook inHook, const int32_t* a, const void* p)
{
    Graphics* g = (Graphics*)inGraphics;
    switch (inHook) {
    case H::String: g->GetFont()->DrawString(g->GetContext(), *(const GUIString*)p, (unsigned short)a[0], (unsigned short)a[1], a[2], a[3]); return 0;
    case H::Text: ((WrappedText*)p)->Paint(*g, a[0], a[1]); return 0;
    default: return 0;
    }
}
[[maybe_unused]] static const bool sGfxHost = (ee_won_graphics_register(GfxHost, offsetof(ColorScheme, mColors)), true);

// Windows-style frames (MSGraphicsUtil.cpp folded in, R6 WON GUI round 37): mspaint.rs's plans run on the Graphics
namespace WONAPI {
// the standard colours and directions mspaint.rs names
static_assert(StandardColor_3DDarkShadow==0 && StandardColor_3DFace==1 && StandardColor_3DHilight==2 &&
              StandardColor_3DShadow==3 && StandardColor_Scrollbar==4 && StandardColor_ButtonText==5 &&
              StandardColor_GrayText==6 && StandardColor_MenuBack==11 && StandardColor_Back==13 &&
              StandardColor_Text==14,"mspaint.rs SYS_*");
static_assert(Direction_Left==0 && Direction_Right==1 && Direction_Up==2 && Direction_Down==3,"mspaint.rs DIR_*");
// Draws a two-pixel 3D frame, raised or sunken (down), at the given position
void LobbyDraw3DFrame(Graphics &g, int x, int y, int width, int height, bool down) { WONMsPaint(g,WONMsPaintOf(EeWonMsPaintKind::Frame3D,width,height,down?EE_WON_MSPAINT_PRESSED:0,x,y)); }
// Draws a raised button frame, or a flat shadow-coloured rectangle when down
void LobbyDrawFrame(Graphics &g, int x, int y, int width, int height, bool down) { WONMsPaint(g,WONMsPaintOf(EeWonMsPaintKind::Frame,width,height,down?EE_WON_MSPAINT_PRESSED:0,x,y)); }
}
