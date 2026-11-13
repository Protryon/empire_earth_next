// The percentage layout's parameters (file name inferred). The layouts' Execute bodies, DoCalc and SameSizeLayout::Add
// are the Rust instructions (rust/crates/ee_won/src/layoutobj.rs, R6 WON round 31).
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
#include "ee/wonapi/WONLobbyLayout.h"
#include "rust/ee_won.h"
namespace WONAPI {
// FUNCTION: WON 0x00737370
// Sets the four anchor components; a missing edge takes the opposite edge on the same axis, then the last
// anchor supplied
void PercentLayoutParam::SetBorders(Component* inLeft, Component* inTop, Component* inRight, Component* inBottom)
{
    Component* in[4] = { inLeft, inTop, inRight, inBottom };
    int anIndex[4];
    ee_won_gui_percent_borders((inLeft!=NULL?1u:0u)|(inTop!=NULL?2u:0u)|(inRight!=NULL?4u:0u)|(inBottom!=NULL?8u:0u),
                               anIndex);
    this->left=anIndex[0]<0?NULL:in[anIndex[0]];
    this->top=anIndex[1]<0?NULL:in[anIndex[1]];
    this->right=anIndex[2]<0?NULL:in[anIndex[2]];
    this->bottom=anIndex[3]<0?NULL:in[anIndex[3]];
}
}
#include "ee/wonapi/WONPercentLayout.h"

namespace WONAPI {
// FUNCTION: WON 0x007372c0
// Sets the anchors, percentages and padding of a percentage layout
void PercentLayoutParam::Init(Component *left,Component *top,Component *right,Component *bottom,int px,int py,int pw,int ph,int lp,int tp,int rp,int bp)
{
    this->SetBorders(left,top,right,bottom);
    this->SetPercentages(px,py,pw,ph);
    this->SetPadding(lp,tp,rp,bp);
}
// FUNCTION: WON 0x007373f0
// Sets the position and size percentages (negative = leave that coordinate alone)
void PercentLayoutParam::SetPercentages(int thePercentX,int thePercentY,int thePercentWidth,int thePercentHeight)
{
    this->px=thePercentX;
    this->py=thePercentY;
    this->pw=thePercentWidth;
    this->ph=thePercentHeight;
}
// FUNCTION: WON 0x00737410
// Sets the padding applied inside each anchor edge
void PercentLayoutParam::SetPadding(int theLeftPad,int theTopPad,int theRightPad,int theBottomPad)
{
    this->lp=theLeftPad;
    this->tp=theTopPad;
    this->rp=theRightPad;
    this->bp=theBottomPad;
}
}

namespace WONAPI {
// FUNCTION: WON 0x00737320
// Creates percentage layout parameters from four anchors, percentages and padding
PercentLayoutParam::PercentLayoutParam(Component *left, Component *top, Component *right, Component *bottom, int px, int py, int pw, int ph, int lp, int tp, int rp, int bp)
{
	this->Init(left,top,right,bottom,px,py,pw,ph,lp,tp,rp,bp);
}
}
