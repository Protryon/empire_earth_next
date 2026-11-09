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
namespace WONAPI {

// FUNCTION: WON 0x007193C0
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
	g.SetColor(aColor);
	int x;
	if(aText==NULL || aText->GetWidth()==0)
		x = (this->Width()-4)/2;
	else
		x = aText->GetWidth() + 10 + this->mLeftTextPad;
	
	int y = this->Height()/2-2;
	int offset = ((mButtonFlags&0x20)!=0)?1:0;
	x+=offset;
	y+=offset;

	int i;
	if(aSortType==ListSortType_Ascending)
	{
		for(i=0; i<4; i++)
			g.DrawLine(x-i, y+i, x+i, y+i);	

	} 
	else if(aSortType==ListSortType_Descending)
	{
		for(i=0; i<4; i++)
			g.DrawLine(x-(3-i), y+i, x+(3-i), y+i);			
	}

}

// FUNCTION: WON 0x0071A450
// Sets a column's header to a skinned text button and its width; the first header fixes the header height (15 if none)
void WONListCtrl::SetStandarColumnHeader(int theCol, const GUIString &theTitle, int theWidth)
{
	int aDesiredHeight = 0;
	this->mHeader->SetColumn(theCol,theTitle, &aDesiredHeight);

	if(this->mHeaderHeight==0)
	{
		if(aDesiredHeight>0)
			this->mHeaderHeight = aDesiredHeight;
		else
			this->mHeaderHeight = 15;
	}
		
	static_cast<MultiListArea*>(this->mScrollArea.get())->SetColumnWidth(theCol,theWidth,false);
}

// FUNCTION: WON 0x0071A4C0
// Sets a column's header to the given component and its width; the first header fixes the header height (15 if none)
void WONListCtrl::SetColumnHeader(int theCol, Component *theHeader, int theWidth)
{
	if(this->mHeaderHeight==0)
	{
		if(theHeader->Height()>0)
			this->mHeaderHeight = theHeader->Height();
		else
			this->mHeaderHeight = 15;
	}

	this->mHeader->SetColumnItem(theCol,theHeader);
	static_cast<MultiListArea*>(this->mScrollArea.get())->SetColumnWidth(theCol,theWidth,false);
}


// FUNCTION: WON 0x0071a580
// Lays out the scroll container, then places the header strip directly above the list area
void WONListCtrl::Virtual08()
{
	WONScrollContainer::Virtual08();

	this->mHeader->SetPosSize(this->mScrollArea->Left(),this->mScrollArea->Top()-this->mHeaderHeight,this->mScrollArea->Width(),this->mHeaderHeight);
}
}

namespace WONAPI {
// FUNCTION: WON 0x0071a060
// Creates a skinned list header that takes its buttons from the given skin
WONHeaderControl::WONHeaderControl(WONSkin* skin)
{
    this->mSkin=skin;
    this->mTextLeftPad=0;
}
// FUNCTION: WON 0x0071a1c0
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

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0071a100
// (public: virtual void * __thiscall WONAPI::WONHeaderControl::`scalar deleting destructor'(unsigned int)    [??_GWONHeaderControl@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x0071a120
// (public: virtual __thiscall WONAPI::WONHeaderControl::~WONHeaderControl(void)    [??1WONHeaderControl@WONAPI@@UAE@XZ])
