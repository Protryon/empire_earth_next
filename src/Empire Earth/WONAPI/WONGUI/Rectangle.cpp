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
void WONRectangle::DoIntersect(const WONRectangle& other)
{
    int right=this->x+this->width;
    int bottom=this->y+this->height;
    int otherRight=other.x+other.width;
    int otherBottom=other.y+other.height;
    if(other.x>this->x) this->x=other.x;
    if(other.y>this->y) this->y=other.y;
    if(otherRight<right) right=otherRight;
    if(otherBottom<bottom) bottom=otherBottom;
    this->width=right-this->x;
    this->height=bottom-this->y;
    if(this->width<0) this->width=0;
    if(this->height<0) this->height=0;
}
// FUNCTION: WON 0x007493b0
// Grows the rectangle to cover another as well; an empty rectangle becomes a copy of the other
void WONRectangle::DoUnion(const WONRectangle& other)
{
    if(this->width==0 || this->height==0)
    {
        this->x=other.x; this->y=other.y; this->width=other.width; this->height=other.height;
        return;
    }
    int right=this->x+this->width;
    int bottom=this->y+this->height;
    int otherRight=other.x+other.width;
    int otherBottom=other.y+other.height;
    if(other.x<this->x) this->x=other.x;
    if(other.y<this->y) this->y=other.y;
    if(otherRight>right) right=otherRight;
    if(otherBottom>bottom) bottom=otherBottom;
    this->width=right-this->x;
    this->height=bottom-this->y;
}
// FUNCTION: WON 0x00749450
// Returns whether two rectangles overlap (touching this one's right/bottom edge counts)
bool WONRectangle::Intersects(const WONRectangle& other) const
{
    return this->x+this->width>=other.x && this->x<other.x+other.width &&
           this->y+this->height>=other.y && this->y<other.y+other.height;
}
}
