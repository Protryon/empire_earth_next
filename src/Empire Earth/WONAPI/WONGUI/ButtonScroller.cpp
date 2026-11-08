// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <vector>
#include <list>
#include <map>
#include <algorithm>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONComponentFlags.h"
namespace WONAPI {
// Scroller driven by left, right, up and down buttons (not identified in the original exe)
ButtonScroller::ButtonScroller()
{
	this->mSettingScrollButtonConditions = false;
	this->mVertCondition = this->mHorzCondition = ScrollButtonCondition_Enable;
}

void ButtonScroller::AddedToParent()
{

	if(this->mScrollArea.get()!=NULL && this->mScrollArea->GetParent()!=this)
		this->AddControls();

	Scroller::AddedToParent();
}

void ButtonScroller::AddControls()
{
	this->mScrollArea->SetScroller(this);
	SmartPtr<Button>* buttons[] = {&this->mLeftButton, &this->mRightButton, &this->mUpButton, &this->mDownButton};
	for(int i=0; i<4; i++)
	{
		SmartPtr<Button> &aButton = *buttons[i];
		if(aButton.get()==NULL)
		{
			aButton = new Button;
			aButton->SetComponentFlags(ComponentFlag_NoInput,true);
		}
	}

	this->AddChild(this->mScrollArea);
	this->AddChild(this->mLeftButton);
	this->AddChild(this->mRightButton);
	this->AddChild(this->mUpButton);
	this->AddChild(this->mDownButton);

	if(this->mScrollArea->mTotalWidth>0 || this->mScrollArea->mTotalHeight>0)
		this->NotifyScrollChange(false);
}

bool ButtonScroller::EnableScrollButtons(Button *theLeft, Button *theRight, ScrollButtonCondition theCondition)
{	
	if(theLeft==NULL || theRight==NULL)
		return false;

	bool showLeft = theLeft->IsVisible(), showRight = theRight->IsVisible();
	switch(theCondition)
	{
		case ScrollButtonCondition_Disable: showLeft = false; showRight = false; break;
		case ScrollButtonCondition_Enable: showLeft = true; showRight = true; break;
		case ScrollButtonCondition_Conditional: showLeft = showRight = !(theLeft->IsDisabled() && theRight->IsDisabled()); break;
		case ScrollButtonCondition_PartialConditional: showLeft = !theLeft->IsDisabled(); showRight = !theRight->IsDisabled(); break;
	}

	// Prevent situations where conditional scrollbars are enabled and disabled recursively since
	// it turns out that it's possible for the appearance of a scrollbar to change the wordwrapping
	// such that the scrollbar is no longer necessary.
	if(this->mSettingScrollButtonConditions)
	{
		if(!showLeft && theLeft->IsVisible())
			return false;
		if(!showRight && theRight->IsVisible())
			return false;
	}

	// Did anything change?
	bool changed = showLeft!=theLeft->IsVisible() || showRight!=theRight->IsVisible();
	theLeft->SetVisible(showLeft);
	theRight->SetVisible(showRight);
	return changed;
}

void ButtonScroller::SetScrollButtonConditions(ScrollButtonCondition theHorzCondition, ScrollButtonCondition theVertCondition)
{
	bool needChange = false;
	this->mHorzCondition = theHorzCondition;
	this->mVertCondition = theVertCondition;

	needChange = this->EnableScrollButtons(this->mLeftButton,this->mRightButton, this->mHorzCondition) || needChange;
	needChange = this->EnableScrollButtons(this->mUpButton, this->mDownButton, this->mVertCondition) || needChange;

	this->mSettingScrollButtonConditions = true;
	if(needChange)
	{
		this->Invalidate();
		this->Virtual08();
	}
	this->mSettingScrollButtonConditions = false;
}

void ButtonScroller::Virtual08()
{
	Scroller::Virtual08();
/*	if(mLeftButton.get()==NULL || mRightButton.get()==NULL || mUpButton.get()==NULL || mDownButton.get()==NULL)
		return;

	int l = 0, r = 0, u = 0, d = 0;
	if(mLeftButton->IsVisible())
	{
		mLeftButton->SetPos(0,(Height()-mLeftButton->Height())/2);
		l = mLeftButton->Width();
	}

	if(mRightButton->IsVisible())
	{
		mRightButton->SetPos(Width()-mRightButton->Width(),(Height()-mRightButton->Height())/2);
		r = mRightButton->Width();
	}

	if(mUpButton->IsVisible())
	{
		mUpButton->SetPos((Width()-mUpButton->Width())/2,0);
		u = mUpButton->Height();
	}

	if(mDownButton->IsVisible())
	{
		mDownButton->SetPos((Width()-mDownButton->Width())/2,(Height()-mDownButton->Height()));
		d = mDownButton->Height();
	}

	mScrollArea->SetPosSize(l,u,Width()-l-r,Height()-u-d);*/
}

void ButtonScroller::SetButton(Button *theButton, Direction theDirection)
{
	switch(theDirection)
	{
		case Direction_Left: this->mLeftButton = theButton; break;
		case Direction_Right: this->mRightButton = theButton; break;
		case Direction_Up:	this->mUpButton = theButton; break;
		case Direction_Down: this->mDownButton = theButton; break;
		default:
			return;
	}

	theButton->AddListener(new ComponentListenerComp(this));
}

void ButtonScroller::NotifyScrollChange(bool becauseOfScroller)
{
	if(this->mLeftButton.get()!=NULL)
		this->mLeftButton->SetEnabled(this->mScrollArea->mHorzOffset>0);

	if(this->mUpButton.get()!=NULL)
		this->mUpButton->SetEnabled(this->mScrollArea->mVertOffset>0);

	if(this->mRightButton.get()!=NULL)
		this->mRightButton->SetEnabled(this->mScrollArea->mHorzOffset + this->mScrollArea->mScrollArea.width < this->mScrollArea->mTotalWidth);

	if(this->mDownButton.get()!=NULL)
		this->mDownButton->SetEnabled(this->mScrollArea->mVertOffset + this->mScrollArea->mScrollArea.height < (this->mScrollArea->mTotalHeight+this->mScrollArea->mExtraHeight));

	this->SetScrollButtonConditions(this->mHorzCondition, this->mVertCondition);
}

void ButtonScroller::HandleComponentEvent(ComponentEvent *theEvent)
{
	if(theEvent->mType==ComponentEvent_ButtonPressed)
	{
		if(theEvent->mComponent==this->mLeftButton)
			this->SetHorzOffset(this->mScrollArea->mHorzOffset-this->mScrollArea->mLeftScrollAmount);
		else if(theEvent->mComponent==this->mRightButton)
			this->SetHorzOffset(this->mScrollArea->mHorzOffset+this->mScrollArea->mRightScrollAmount);
		else if(theEvent->mComponent==this->mUpButton)
			this->SetVertOffset(this->mScrollArea->mVertOffset-this->mScrollArea->mUpScrollAmount);
		else if(theEvent->mComponent==this->mDownButton)
			this->SetVertOffset(this->mScrollArea->mVertOffset+this->mScrollArea->mDownScrollAmount);
		else
			Scroller::HandleComponentEvent(theEvent);
	}
	else
		Scroller::HandleComponentEvent(theEvent);
}
}
