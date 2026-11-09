// Skinned combo box and scroll container: layout and frame painting (file name inferred)
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
#include "ee/wonapi/WONLobbySkinnedContainers.h"
namespace WONAPI {

// FUNCTION: WON 0x00719A70
// Returns the combo box's desired size: edit height + 2 (else 22), widest item + 10 + button (else 100) wide
void WONComboBox::GetDesiredSize(int &width, int &height)
{
	if(this->mEdit.get()!=NULL)
	{
		this->mEdit->GetDesiredSize(width,height);
		height+=2;
//		if(mDrawFrame)
//			height+=mFrame.mTop + mFrame.mBottom;
	}
	else 
		height = 22;

	if(this->mListArea.get()!=NULL && this->mListArea->GetNumItems()>0)
	{
		width = 10 + this->mListArea->GetTotalWidth();
		if(this->mButton.get()!=NULL)
			width += this->mButton->Width();
	}
	else
		width = 100;
}

// FUNCTION: WON 0x00719B00
// Lays out the combo box: the edit fills the width left of the button, the button sits inside or beside the frame
void WONComboBox::Virtual08()
{
	ComboBox::Virtual08();
	int aTopPad = 0, aBottomPad = 0, aRightPad = 0, aLeftPad = 0;
	if(this->mDrawFrame)
	{
		aLeftPad = this->mFrame.mLeft;
		aTopPad = this->mFrame.mTop;
		aBottomPad = this->mFrame.mBottom;
		aRightPad = this->mFrame.mRight;
	}

	int anInputBoxHeight = this->Height(); //mEdit->mHeight;
	int aButtonHeight = anInputBoxHeight-aBottomPad-aTopPad;
	int aButtonWidth = this->mButton->Width();
	if(!this->mButton->IsVisible())
		aButtonWidth = 0;
//	if(aButtonWidth>aButtonHeight)
//		aButtonWidth = aButtonHeight;
 
	int anInputBoxWidth = this->Width()-aButtonWidth-aRightPad;


	this->mEdit->mLeftPad = aLeftPad + 2;
	this->mEdit->SetPosSize(0,0,anInputBoxWidth,anInputBoxHeight);
	
	if(!this->mButton->IsVisible())
		return;
	if(this->mButtonInFrame)
		this->mButton->SetPosSize(this->mEdit->Right(),aTopPad,aButtonWidth,aButtonHeight);
	else
		this->mButton->SetPosSize(this->Width()-aButtonWidth,0,aButtonWidth,this->Height());

}

// FUNCTION: WON 0x00719BF0
// Paints the combo box and its frame, leaving the button out of the frame when it sits outside it
void WONComboBox::Paint(Graphics& g)
{
	ComboBox::Paint(g);

	if(!this->mDrawFrame)
		return;
	if(this->mButtonInFrame || !this->mButton->IsVisible())
		this->mFrame.Paint(g,this);
	else
		this->mFrame.Paint(g,this,0,0,this->mButton->Width(),0);
}

// FUNCTION: WON 0x00719DF0
// Lays out the visible scrollbars inside the frame and gives the rest, below the header, to the scroll area
void WONScrollContainer::Virtual08()
{
	Container::Virtual08();
	if(this->mScrollArea.get()==NULL)
		return;

	bool horzOn = this->mHorzScrollbar->IsVisible();
	bool vertOn = this->mVertScrollbar->IsVisible();

	int left = this->mFrame.mLeft, right = this->mFrame.mRight, top = this->mFrame.mTop, bottom = this->mFrame.mBottom;

	int aScrollbarWidth = this->mVertScrollbar->Width();
	int aScrollbarHeight = this->mHorzScrollbar->Height();

	int aScrollWidth = this->Width() - left - right;
	if(vertOn)
		aScrollWidth -= aScrollbarWidth;

	int aScrollHeight = this->Height() - top - bottom;
	if(horzOn)
		aScrollHeight -= aScrollbarHeight;

	if(vertOn)
		this->mVertScrollbar->SetPosSize(this->Width()-aScrollbarWidth-right,top,aScrollbarWidth,aScrollHeight);

	if(horzOn)
		this->mHorzScrollbar->SetPosSize(left,this->Height()-aScrollbarHeight-bottom,aScrollWidth,aScrollbarHeight);

	this->mScrollArea->SetPosSize(left,this->mHeaderHeight+top,aScrollWidth,aScrollHeight-this->mHeaderHeight);
}

// FUNCTION: WON 0x00719FF0
// Paints the container, the corner image between two visible scrollbars (when opaque) and the frame
void WONScrollContainer::Paint(Graphics &g)
{
	Container::Paint(g);

	if(this->mVertScrollbar->IsVisible() && this->mHorzScrollbar->IsVisible() && !this->mTransparent && this->mCorner.get()!=NULL) // grey square in corner
	{
		g.DrawImage(this->mCorner.get(),this->mVertScrollbar->Left(),(this->mVertScrollbar->Top()+this->mVertScrollbar->Height()));

//		g.ApplyColorSchemeColor(StandardColor_Scrollbar);
//		g.FillRect(mVertScrollbar->mX,(mVertScrollbar->mY+mVertScrollbar->mHeight),mVertScrollbar->mWidth,mHorzScrollbar->mHeight);
	}

	this->mFrame.Paint(g,this);
}
}
