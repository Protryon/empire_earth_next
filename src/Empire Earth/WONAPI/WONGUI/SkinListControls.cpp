// Skinned list headers: sort arrow painting, column headers and the header height (file name inferred)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbySkinListControls.h"
#include "ee/wonapi/WONLobbyMSSkinImage.h"
#include "ee/wonapi/WONGUIDraw.h"
#include "ee/wonapi/WONSkinLayout.h"
namespace WONAPI {

// Paints the header button plus a small up or down sort arrow after its text, shifted by one pixel while pressed
void WONHeaderButton::Paint(Graphics &g)
{
	WONButton::Paint(g);

	ListSortType aSortType = MultiListHeader::GetSortTypeForButton(this);
	if(aSortType==ListSortType_None)
		return;

	int aColor;
	WrappedText *aText;

	this->GetButtonState(g,&aColor, &aText);
	// the arrow after the text, one pixel down and right while pressed (guidraw.rs header_arrow)
	WONDrawOps anOps([&](EeWonDrawOp* o, size_t n) {
		return ee_won_gui_draw_header_arrow((int)aSortType,aColor,aText!=NULL,aText ? aText->GetWidth() : 0,
			this->GetLeftTextPad(),this->Width(),this->Height(),this->IsPressed(),o,n); });
	WONDrawRun(g,anOps,WONDrawSlots());
}

// Sets a column's header to a skinned text button and its width; the first header fixes the header height (15 if
// none; skinlay.rs)
void WONListCtrl::SetStandarColumnHeader(int theCol, const GUIString &theTitle, int theWidth)
{
	WONSkinLayout(this,EeWonSkinLayKind::WonListTextHeader,theCol,(intptr_t)&theTitle,theWidth);
}

// Sets a column's header to the given component and its width; the first header fixes the header height (15 if
// none; skinlay.rs)
void WONListCtrl::SetColumnHeader(int theCol, Component *theHeader, int theWidth)
{
	WONSkinLayout(this,EeWonSkinLayKind::WonListHeader,theCol,(intptr_t)theHeader,theWidth);
}


// Lays out the scroll container, then places the header strip directly above the list area (skinlay.rs)
void WONListCtrl::Virtual08()
{
	Container::Virtual08();
	WONSkinLayout(this,EeWonSkinLayKind::WonList);
}
}

namespace WONAPI {
// Creates a skinned list header that takes its buttons from the given skin
WONHeaderControl::WONHeaderControl(WONSkin* skin)
{
    this->mSkin=skin;
    this->mTextLeftPad=0;
}
// Puts a new skinned header button with the title on a column, optionally returning its desired height
void WONHeaderControl::SetColumn(int column,const GUIString& title,int* desiredHeight)
{
    SmartPtr<WONHeaderButton> button=this->mSkin->GetHeaderButton();
    button->SetText(title,true);
    if(desiredHeight!=NULL) {
        int width;
        button->GetDesiredSize(width,*desiredHeight);
    }
    this->SetColumnItem(column,button.get());
}
}
