// Rectangle constructors, intersection and union (file name inferred)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "rust/ee_won.h"
// The intersection, union and overlap test are ee_won's guilayout module (rust/crates/ee_won/src/guilayout.rs).
namespace WONAPI {
WONRectangle::WONRectangle() { this->x=this->y=this->width=this->height=0; }
WONRectangle::WONRectangle(int inX,int inY,int inWidth,int inHeight)
    :x(inX),y(inY),width(inWidth),height(inHeight) {}
// FUNCTION: WON 0x00749300
// Default constructor above: creates an empty rectangle at the origin
// (??0WONRectangle@WONAPI@@QAE@XZ)

// FUNCTION: WON 0x00749310
// Constructor above: creates a rectangle from position and size
// (??0WONRectangle@WONAPI@@QAE@HHHH@Z)

// FUNCTION: WON 0x00749330
// Shrinks the rectangle to its overlap with another (empty size when they do not overlap)
static EeWonRect ToRust(const WONRectangle& r) { EeWonRect e = { r.x, r.y, r.width, r.height }; return e; }
static void FromRust(WONRectangle& r, const EeWonRect& e) { r.x = e.x; r.y = e.y; r.width = e.width; r.height = e.height; }
void WONRectangle::DoIntersect(const WONRectangle& other)
{
    FromRust(*this, ee_won_gui_rect_intersect(ToRust(*this), ToRust(other)));
}
// FUNCTION: WON 0x007493b0
// Grows the rectangle to cover another as well; an empty rectangle becomes a copy of the other
void WONRectangle::DoUnion(const WONRectangle& other)
{
    FromRust(*this, ee_won_gui_rect_union(ToRust(*this), ToRust(other)));
}
// FUNCTION: WON 0x00749450
// Returns whether two rectangles overlap (touching this one's right/bottom edge counts)
bool WONRectangle::Intersects(const WONRectangle& other) const
{
    return ee_won_gui_rect_intersects(ToRust(*this), ToRust(other));
}
}
