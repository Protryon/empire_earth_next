// Paint methods of the Windows-style header button, close button, scroll thumb and label (R6 WON round 36: the
// Rust plans of mspaint.rs)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyMSRemainingPainters.h"
#include "ee/wonapi/WONGUIDraw.h"

namespace WONAPI {
// FUNCTION: WON 0x006ff000
// Paints a list header button and, when its column is sorted, a small up or down triangle after the text (shifted
// by one pixel while pressed)
void MSHeaderButton::Paint(Graphics& g)
{
    MSButton::Paint(g);
    static_assert(ListSortType_Ascending==0 && ListSortType_Descending==1 && ListSortType_None==2,"mspaint.rs SORT_*");
    WONMsPaint(g,WONMsPaintOf(EeWonMsPaintKind::HeaderSort,this->Width(),this->Height(),this->IsPressed()?EE_WON_MSPAINT_PRESSED:0,
                              (int)MultiListHeader::GetSortTypeForButton(this),this->mText->GetWidth(),this->GetLeftTextPad()));
}

// FUNCTION: WON 0x006ff2a0
// Paints a close button: face background, a raised or sunken 3D frame and the X, shifted while pressed
void MSCloseButton::Paint(Graphics& g)
{
    Button::Paint(g);
    WONMsPaint(g,WONMsPaintOf(EeWonMsPaintKind::CloseButton,this->Width(),this->Height(),this->IsPressed()?EE_WON_MSPAINT_PRESSED:0));
}

// FUNCTION: WON 0x006ff360
// Paints a scrollbar thumb: face background with a raised frame
void MSScrollThumb::Paint(Graphics& g)
{
    Component::Paint(g);
    WONMsPaint(g,WONMsPaintOf(EeWonMsPaintKind::ScrollThumb,this->Width(),this->Height()));
}

// FUNCTION: WON 0x006ff7d0
// Paints a label: face background unless transparent, then the text
void MSLabel::Paint(Graphics& g)
{
    MSButtonBase::Paint(g);
    WONMsPaint(g,WONMsPaintOf(EeWonMsPaintKind::Label,this->Width(),this->Height(),this->IsTransparent()?EE_WON_MSPAINT_TRANSPARENT:0),this);
}
}
