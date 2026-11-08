// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
// Special-purpose controls: sortable column header button, image check button, splitter container with
// minimize/maximize buttons and bordered image button (not identified in the original exe)
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbySpecialPurposeControls.h"
#include "ee/wonapi/WONComponentFlags.h"
namespace WONAPI {
ColHeaderButton::ColHeaderButton()
{
	this->mFont = this->GetFont(FontDescriptor("Verdana",FontStyle_Bold,7));
	this->mBackColor = 0xffffff;
	this->mIsUp = true;
	this->SetButtonFlags(0x0008, true);
	this->mMultiListBox = NULL;
	this->mCol = -1;
}

	
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
ColHeaderButton::~ColHeaderButton()
{
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void ColHeaderButton::DrawButtonText(Graphics &g, int xoff, int yoff)
{
	g.SetFont(this->mFont);
	Font *aFM = this->mFont;
	int aTextWidth = aFM->GetStringWidth(this->mText);
	int aTextHeight = aFM->GetHeight();

	int x = 0;
	int y = 0;

	x+=xoff;
	y+=yoff;

	g.SetColor(0x000000);
	g.DrawString(this->mText, x, y);
}

///////////////////////////////////////////////////////////////////////////////
void ColHeaderButton::Activate()
{
	if (((mButtonFlags&0x0004)!=0)) // Change up down state if checked
	{
		this->mIsUp = !this->mIsUp;
	}

	if (this->mMultiListBox.get() != NULL && this->mCol >= 0)
	{
		// Sort the Col
		this->mMultiListBox->SetSortColumn(this->mCol, this->mIsUp?ListSortType_Ascending:ListSortType_Descending);
	}

	Button::Fn2F3440();
}

///////////////////////////////////////////////////////////////////////////////
void ColHeaderButton::Paint(Graphics &g)
{
	Button::Paint(g);

	// Background
	g.SetColor(this->mBackColor);
	g.FillRect(0,0,this->Width(),this->Height());

	this->DrawButtonText(g,2,0);

	if(((mButtonFlags&0x0004)!=0)) // Draw arrow
	{	
		int aTextWidth = this->mFont->GetStringWidth(this->mText) + 2;
		int x = this->Width()/2;

		// Make sure arrow is in center or just to the right of text
		if (aTextWidth + 6 > x)
		{
			x = aTextWidth + 6;
		}
			
		int y = 5;
		

		if(this->mIsUp)
		{
			for(int i=0; i<7; i++)
			{
				int w = i/2;
				g.DrawLine(x-w, y+i, x+w+1, y+i);	
			}
		}
		else 
		{
			for(int i=0; i<7; i++)
			{
				int w = i/2;
				g.DrawLine(x-(3-w), y+i, x+(3-w), y+i);	
			}

			if(this->IsDisabled())
				g.SetPixel(x+1,y+4,0xFFFFFF);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
ImageCheckButton::ImageCheckButton(int theNormalControlID, int theCheckedControlID) : mAlternateControlID(theCheckedControlID)
{
	this->mControlId=theNormalControlID;
	this->SetButtonFlags(0x0001,true);
	this->SetComponentFlags(ComponentFlag_WantFocus,true);
}

///////////////////////////////////////////////////////////////////////////////
ImageCheckButton::~ImageCheckButton(void)
{
}

///////////////////////////////////////////////////////////////////////////////
void ImageCheckButton::AlternateControlIDs(void)
{
	int aSavedID = this->mAlternateControlID;
	this->mAlternateControlID = this->mControlId;
	this->mControlId=aSavedID;
}

///////////////////////////////////////////////////////////////////////////////
void ImageCheckButton::Paint(Graphics &g)
{
	Component::Paint(g);

	Image *anImage = NULL;
	if(this->IsMouseOver())
	{
		if(((mButtonFlags&0x0004)!=0))
			anImage = this->mImages[ImageCheckButtonType_CheckedOver];
		else
			anImage = this->mImages[ImageCheckButtonType_Over];
	}
	else
	{
		if(((mButtonFlags&0x0004)!=0))
			anImage = this->mImages[ImageCheckButtonType_Checked];
		else
			anImage = this->mImages[ImageCheckButtonType_Normal];
	}

	if(anImage==NULL)
		anImage = this->mImages[ImageCheckButtonType_Normal];

	if(anImage!=NULL)
		g.DrawImage(anImage,0,0);
}

///////////////////////////////////////////////////////////////////////////////
CheckButtonSplitterContainer::CheckButtonSplitterContainer(void)
{
	this->mNormalSplitter = NULL;
	this->mMaxMinSplitter = NULL;
	this->mFirstMinButton = NULL;
	this->mSecondMinButton = NULL;
	this->mFirstMaxButton = NULL;
	this->mSecondMaxButton = NULL;
	this->mStatus = Status_Normal;
}

///////////////////////////////////////////////////////////////////////////////
CheckButtonSplitterContainer::~CheckButtonSplitterContainer(void)
{
}

///////////////////////////////////////////////////////////////////////////////
void CheckButtonSplitterContainer::AddSplitterBars(BorderComponent* theNormalSplitter, BorderComponent* theMaxMinSplitter)
{
	if(theNormalSplitter)
	{
		this->mNormalSplitter = theNormalSplitter;
		this->mNormalSplitter->SetComponentFlags(ComponentFlag_ParentDrag, true);
		this->AddChild(this->mNormalSplitter);
	}
	if(!theMaxMinSplitter)
		return;
	this->mMaxMinSplitter = theMaxMinSplitter;
	this->mMaxMinSplitter->SetComponentFlags(ComponentFlag_ParentDrag, true);
	this->mMaxMinSplitter->SetVisible(false);
	this->AddChild(this->mMaxMinSplitter);
}

///////////////////////////////////////////////////////////////////////////////
void CheckButtonSplitterContainer::AddMinButtons(ImageCheckButton* theFirstMinButton, ImageCheckButton* theSecondMinButton)
{
	if(theFirstMinButton)
	{
		this->mFirstMinButton = theFirstMinButton;
		this->mFirstMinButton->SetComponentFlags(ComponentFlag_NoParentCursor, true);
		this->mFirstMinButton->SetButtonFlags(0x0002, false);
		this->AddChild(this->mFirstMinButton);
	}
	if(!theSecondMinButton)
		return;
	this->mSecondMinButton = theSecondMinButton;
	this->mSecondMinButton->SetComponentFlags(ComponentFlag_NoParentCursor, true);
	this->mSecondMinButton->SetButtonFlags(0x0002, false);
	this->AddChild(this->mSecondMinButton);
}

///////////////////////////////////////////////////////////////////////////////
void CheckButtonSplitterContainer::AddMaxButtons(ImageCheckButton* theFirstMaxButton, ImageCheckButton* theSecondMaxButton)
{
	if(theFirstMaxButton)
	{
		this->mFirstMaxButton = theFirstMaxButton;
		this->mFirstMaxButton->SetComponentFlags(ComponentFlag_NoParentCursor, true);
		this->mFirstMaxButton->SetButtonFlags(0x0002, false);
		this->AddChild(this->mFirstMaxButton);
	}
	if(!theSecondMaxButton)
		return;
	this->mSecondMaxButton = theSecondMaxButton;
	this->mSecondMaxButton->SetComponentFlags(ComponentFlag_NoParentCursor, true);
	this->mSecondMaxButton->SetButtonFlags(0x0002, false);
	this->AddChild(this->mSecondMaxButton);
}

///////////////////////////////////////////////////////////////////////////////
void CheckButtonSplitterContainer::ChangeAlternateButtonState(void)
{
	if(this->mStatus == Status_Max)
	{
		if(this->mFirstMinButton && ((this->mFirstMinButton->mButtonFlags&0x0004)!=0))
		{
			this->mFirstMinButton->SetCheck(false);
			this->mFirstMinButton->AlternateControlIDs();
		}
		if(this->mSecondMinButton && ((this->mSecondMinButton->mButtonFlags&0x0004)!=0))
		{
			this->mSecondMinButton->SetCheck(false);
			this->mSecondMinButton->AlternateControlIDs();
		}
	}
	else if(this->mStatus == Status_Min)
	{
		if(this->mFirstMaxButton && ((this->mFirstMaxButton->mButtonFlags&0x0004)!=0))
		{
			this->mFirstMaxButton->SetCheck(false);
			this->mFirstMaxButton->AlternateControlIDs();
		}
		if(this->mSecondMaxButton && ((this->mSecondMaxButton->mButtonFlags&0x0004)!=0))
		{
			this->mSecondMaxButton->SetCheck(false);
			this->mSecondMaxButton->AlternateControlIDs();
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
void CheckButtonSplitterContainer::HandleComponentEvent(ComponentEvent *theEvent)
{
	if(theEvent->mType == ComponentEvent_ButtonPressed)
	{
		ImageCheckButton* aButton = dynamic_cast<ImageCheckButton*>(theEvent->mComponent);
		if(aButton)
		{
			Container::HandleComponentEvent(theEvent);

			bool isChecked = ((aButton->mButtonFlags&0x0004)!=0);
			this->mStatus = Status_Normal;
			aButton->AlternateControlIDs();
			if(theEvent->mComponent == this->mFirstMinButton)
			{
				if(isChecked)
					this->mStatus = Status_Min;

				if(this->mSecondMinButton)
				{
					this->mSecondMinButton->SetCheck(isChecked);
					this->mSecondMinButton->AlternateControlIDs();
				}
			}
			else if(theEvent->mComponent == this->mSecondMinButton)
			{
				if(isChecked)
					this->mStatus = Status_Min;

				if(this->mFirstMinButton)
				{
					this->mFirstMinButton->SetCheck(isChecked);
					this->mFirstMinButton->AlternateControlIDs();
				}
			}
			else if(theEvent->mComponent == this->mFirstMaxButton)
			{
				if(isChecked)
					this->mStatus = Status_Max;

				if(this->mSecondMaxButton)
				{
					this->mSecondMaxButton->SetCheck(isChecked);
					this->mSecondMaxButton->AlternateControlIDs();
				}
			}
			else if(theEvent->mComponent == this->mSecondMaxButton)
			{
				if(isChecked)
					this->mStatus = Status_Max;

				if(this->mFirstMaxButton)
				{
					this->mFirstMaxButton->SetCheck(isChecked);
					this->mFirstMaxButton->AlternateControlIDs();
				}
			}

			if(!isChecked && this->mNormalSplitter)
			{
				if(this->mNormalSplitter && !this->mNormalSplitter->IsVisible())
					this->mNormalSplitter->SetVisible(true);
				if(this->mMaxMinSplitter && this->mNormalSplitter && this->mMaxMinSplitter->IsVisible())
					this->mMaxMinSplitter->SetVisible(false);
			}
			if(isChecked && this->mMaxMinSplitter)
			{
				if(this->mMaxMinSplitter && !this->mMaxMinSplitter->IsVisible())
					this->mMaxMinSplitter->SetVisible(true);
				if(this->mNormalSplitter && this->mMaxMinSplitter && this->mNormalSplitter->IsVisible())
					this->mNormalSplitter->SetVisible(false);
			}

			this->ChangeAlternateButtonState();
			return;
		}
	}
	Container::HandleComponentEvent(theEvent);
}

void CheckButtonSplitterContainer::MouseDown(int x, int y, int theButton)
{
	if(theButton==MouseButton_Left && this->mMaxMinSplitter && this->mMaxMinSplitter->IsVisible() && this->mMaxMinSplitter->Contains(x, y))
	{
		switch(this->mStatus)
		{
		case Status_Min:
			if(this->mFirstMinButton)
				this->mFirstMinButton->Fn2F3440();
			else if(this->mSecondMinButton)
				this->mSecondMinButton->Fn2F3440();
			break;
		case Status_Max:
			if(this->mFirstMaxButton)
				this->mFirstMaxButton->Fn2F3440();
			else if(this->mSecondMaxButton)
				this->mSecondMaxButton->Fn2F3440();
			break;
		default:
			break;
		}
	}
	Container::MouseDown(x, y, theButton); 
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void BorderImageButton::Paint(Graphics &g)
{
	Component::Paint(g);

	BorderComponent *anImage = NULL;
	if(this->IsDisabled())
		anImage = this->mImages[ImageButtonType_Disabled];
	else if(((mButtonFlags&0x0004)!=0) && this->mImages[ImageButtonType_Checked].get()!=NULL)
		anImage = this->mImages[ImageButtonType_Checked];
	else if(((mButtonFlags&0x0020)!=0))
		anImage = this->mImages[ImageButtonType_Down];
	else if(this->IsMouseOver())// || HasFocus())
		anImage = this->mImages[ImageButtonType_Over];

	if(anImage==NULL)
		anImage = this->mImages[ImageButtonType_Normal];

	if(anImage!=NULL)
	{
		anImage->Invalidate();
		anImage->Paint(g);
	}
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void BorderImageButton::Virtual08()
{
	for(int i = 0; i < 5; i++)
	{
		if(this->mImages[i].get() != NULL)
			this->mImages[i]->SetSize(this->Width(), this->Height());
	}
}
}
