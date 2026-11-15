// Painting and layout of the Windows-style scrollbar, combo box and scroll container (file name inferred)
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
#include "ee/wonapi/WONLobbyMSExtraConfig.h"
#include "ee/wonapi/WONGUIDraw.h"
#include "ee/wonapi/WONSkinLayout.h"

namespace WONAPI {
// Fills the scrollbar track when it needs repainting (unless transparent), then paints the buttons and thumb
void MSScrollbar::Paint(Graphics& g)
{
    WONMsPaint(g,WONMsPaintOf(EeWonMsPaintKind::Scrollbar,0,0,(this->GetScrollRect()->mInvalidState!=0?EE_WON_MSPAINT_INVALID:0)|
                              (this->mIsTransparent?EE_WON_MSPAINT_TRANSPARENT:0),this->GetScrollRect()->Left(),
                              this->GetScrollRect()->Top(),this->GetScrollRect()->Width(),this->GetScrollRect()->Height()));
    Container::Paint(g);
}

// Paints the combo box and, if framed, a sunken 3D frame around it
void MSComboBox::Paint(Graphics& g)
{
    ComboBox::Paint(g);
    WONMsPaint(g,WONMsPaintOf(EeWonMsPaintKind::ComboBox,this->Width(),this->Height(),this->mDrawFrame?EE_WON_MSPAINT_DRAW_FRAME:0));
}

// Returns the combo box's preferred size, 100 x 22 (the linker folds this body into the identical one at 0x006ffb00)
void MSComboBox::GetDesiredSize(int& width,int& height)
{
    width=100;
    height=22;
}

// Lays out the combo box: the arrow button (if visible) at the right inside the frame, the edit box filling the rest
// of the width and the full height (skinlay.rs)
void MSComboBox::Virtual08()
{
    ComboBox::Virtual08();
    WONSkinLayout(this,EeWonSkinLayKind::MsCombo);
}

// Lays out the scroll container: visible scrollbars along the right and bottom edges inside the 3D frame, the scroll
// area in the rest below the header (skinlay.rs)
void MSScrollContainer::Virtual08()
{
    Container::Virtual08();
    WONSkinLayout(this,EeWonSkinLayKind::MsScroll);
}

// Paints the scroll container: fills the corner between two visible scrollbars and draws a 3D or
// dark one-pixel frame
void MSScrollContainer::Paint(Graphics& g)
{
    Container::Paint(g);
    Scrollbar* v=this->GetScrollbar(true);
    Scrollbar* h=this->GetScrollbar(false);
    uint32_t f=(v->IsVisible()?EE_WON_MSPAINT_VERT:0)|(h->IsVisible()?EE_WON_MSPAINT_HORZ:0)|(this->mTransparent?EE_WON_MSPAINT_TRANSPARENT:0)|
               (this->mDrawFrame?EE_WON_MSPAINT_DRAW_FRAME:0)|(this->mDraw3DFrame?EE_WON_MSPAINT_DRAW_3D:0);
    WONMsPaint(g,WONMsPaintOf(EeWonMsPaintKind::ScrollContainer,this->Width(),this->Height(),f,v->Left(),v->Top(),v->Width(),v->Height(),h->Height()));
}
}
