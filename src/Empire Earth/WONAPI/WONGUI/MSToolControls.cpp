// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
// Windows-style scroll button container, tooltip and toolbar button (not identified in the original exe)
#include <windows.h>
#include <string>
#include <vector>
#include <list>
#include <map>
#include <set>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbySimpleConfig.h"
#include "ee/wonapi/WONLobbyMSToolControls.h"
#include "ee/wonapi/WONComponentFlags.h"
namespace WONAPI {
MSScrollButtonContainer::MSScrollButtonContainer(ScrollArea *theScrollArea)
{
	SmartPtr<RectangleComponent> aBack = new RectangleComponent();
	aBack->SetSolidColor(StandardColor_3DFace);
	this->Fn2F6140(aBack.get(),3,this,0,0,0,0,0);
	this->AddChild(aBack.get());
	this->mLeftButton = new MSArrowButton(Direction_Left);
	this->mLeftButton->SetSize(15,15);
	this->mRightButton = new MSArrowButton(Direction_Right);
	this->mRightButton->SetSize(15,15);
	this->mUpButton = new MSArrowButton(Direction_Up);
	this->mUpButton->SetSize(15,15);
	this->mDownButton = new MSArrowButton(Direction_Down);
	this->mDownButton->SetSize(15,15);

	this->mScrollArea = theScrollArea;
//	AddControls();
}

MSToolTip::MSToolTip(const GUIString &theText)
{
//	mFont = GetFont(FontDescriptor("Tahoma",FontStyle_Plain,8));
	this->mWrappedText->SetFont(this->GetNamedFont("MSButton"));
	this->SetHorzAlign(HorzAlign_Center);
	this->mAlign3C=VertAlign_Center;
	this->SetText(theText,true);
	this->mBackground=Background(0xffffdd,0x000000);
	if(this->mBackground.GetWantGrabBG()) this->SetComponentFlags(ComponentFlag_GrabBG,true);
	this->SetComponentFlags(ComponentFlag_GrabBG,true);
//	SetSize(mFont->GetStringWidth(theText)+6,mFont->GetHeight()+4);
	int width=0,height=0;
	this->GetDesiredSize(width,height);
	this->SetSize(width+6,height+4);
}

MSToolButton::MSToolButton(const GUIString &theText)
{
	this->mFocusStyle = FocusStyle_None;
	this->mHAlign = HorzAlign_Left;
	this->mLeftTextPad = 3;

	this->SetText(theText,true);
//	SetSize(mFont->GetStringWidth(theText)+10,mFont->GetHeight()+4);
	int width=0,height=0;
	this->GetDesiredSize(width,height);
	this->SetSize(width+10,height+4);
	this->SetButtonFlags(0x0001,true);
}

void MSToolButton::Paint(Graphics &g)
{
	MSButtonBase::Paint(g);

	bool down = (mButtonFlags&0x0020)!=0;
	bool over = !down && this->IsMouseOver();
	int offset = down?1:0;

	g.SetColor(0xD0D0D0);
	g.FillRect(0,0,this->Width(),this->Height());

	if(down || over)
	{
		g.SetColor(down?0x848484:0xffffff);
		g.DrawLine(0,0,this->Width()-1,0);
		g.DrawLine(0,0,0,this->Height()-1);
	
		g.SetColor(over?0x848484:0xffffff);
		g.DrawLine(0,this->Height()-1,this->Width()-1,this->Height()-1);
		g.DrawLine(this->Width()-1,0,this->Width()-1,this->Height()-1);
	}

	this->DrawButtonText(g,offset,offset);
}
}
