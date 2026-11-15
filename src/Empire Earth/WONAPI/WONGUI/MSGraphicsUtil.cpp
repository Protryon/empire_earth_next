// Windows-style drawing helpers: 3D frames, arrows, check boxes, radio circles, tabs and the close X (R6 WON round
// 36: each is a Rust plan, rust/crates/ee_won/src/mspaint.rs, run on the Graphics)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONGUIDraw.h"
namespace WONAPI {
// the standard colours and directions mspaint.rs names
static_assert(StandardColor_3DDarkShadow==0 && StandardColor_3DFace==1 && StandardColor_3DHilight==2 &&
              StandardColor_3DShadow==3 && StandardColor_Scrollbar==4 && StandardColor_ButtonText==5 &&
              StandardColor_GrayText==6 && StandardColor_MenuBack==11 && StandardColor_Back==13 &&
              StandardColor_Text==14,"mspaint.rs SYS_*");
static_assert(Direction_Left==0 && Direction_Right==1 && Direction_Up==2 && Direction_Down==3,"mspaint.rs DIR_*");

static void MsDraw(Graphics& g, EeWonMsPaintKind k, int a, int b, int w, int h, bool down, bool checked=false)
{
    WONMsPaint(g,WONMsPaintOf(k,w,h,(down?EE_WON_MSPAINT_PRESSED:0)|(checked?EE_WON_MSPAINT_CHECKED:0),a,b));
}

// Draws a two-pixel 3D frame, raised or sunken (down), at the given position
void LobbyDraw3DFrame(Graphics &g, int x, int y, int width, int height, bool down) { MsDraw(g,EeWonMsPaintKind::Frame3D,x,y,width,height,down); }
// Draws a raised button frame, or a flat shadow-coloured rectangle when down
void LobbyDrawFrame(Graphics &g, int x, int y, int width, int height, bool down) { MsDraw(g,EeWonMsPaintKind::Frame,x,y,width,height,down); }
// Draws a sunken frame inside a dark outline (the down flag is ignored)
void MSGraphicsUtil::DrawFrame3(Graphics &g, int x, int y, int width, int height, bool) { MsDraw(g,EeWonMsPaintKind::Frame3,x,y,width,height,false); }
// Draws a small four-stroke arrow centred in width x height, grey with a highlight when disabled; the x and y
// offsets are ignored (the original shadowed them by the centre locals)
void MSGraphicsUtil::DrawArrow(Graphics &g, Direction theDirection, int, int, int width, int height, bool disabled)
{
    WONMsPaint(g,WONMsPaintOf(EeWonMsPaintKind::Arrow,width,height,disabled?EE_WON_MSPAINT_DISABLED:0,(int)theDirection));
}
// Draws a sunken check box (face colour while pressed, window colour otherwise) with a tick if checked
void MSGraphicsUtil::DrawCheck(Graphics &g, int x, int y, int width, int height, bool down, bool checked) { MsDraw(g,EeWonMsPaintKind::Check,x,y,width,height,down,checked); }
// Draws a sunken radio circle (face colour while pressed, window colour otherwise) with a dot if checked
void MSGraphicsUtil::DrawRadio(Graphics &g, int x, int y, int width, int height, bool down, bool checked) { MsDraw(g,EeWonMsPaintKind::Radio,x,y,width,height,down,checked); }
// Draws a tab with rounded top corners, highlight on the left and top and shadows on the right
void MSGraphicsUtil::DrawTab(Graphics &g, int x, int y, int width, int height) { MsDraw(g,EeWonMsPaintKind::Tab,x,y,width,height,false); }
// Draws the 8 x 8 close X centred in the area, offset by x and y
void MSGraphicsUtil::DrawCloseX(Graphics &g, int x, int y, int width, int height) { MsDraw(g,EeWonMsPaintKind::CloseX,x,y,width,height,false); }
}
