// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
// Windows-style numeric spinner: a label, a numeric input box and up/down arrow buttons; Init builds and lays
// them out (not identified in the original exe)
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include <cstdio>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyMSBorderPainters.h"
#include "ee/wonapi/WONLobbySpinner.h"
#include "ee/wonapi/WONComponentFlags.h"
namespace WONAPI {
static const int MSSpinner_ButtonWidth = 13;
void MSSpinner::Init()
{
	this->mLabel = new Label;
	this->mLabel->mAlign3C=VertAlign_Center;
	this->AddChild(this->mLabel);

	this->mInputBox = new MSInputBox();
	this->mInputBox->mInputFilter=InputBox::NumericFilter;

	this->mUpArrow = new MSArrowButton(Direction_Up);
	this->mDownArrow = new MSArrowButton(Direction_Down);
	this->mUpArrow->SetComponentFlags(ComponentFlag_WantFocus,true);
	this->mDownArrow->SetComponentFlags(ComponentFlag_WantFocus,true);
	this->mUpArrow->SetComponentFlags(ComponentFlag_WantTabFocus,false);
	this->mDownArrow->SetComponentFlags(ComponentFlag_WantTabFocus,false);

	this->Fn2F6140(this->mLabel,0x0002,this,0,0,0,0,0);
	this->Fn2F6140(this->mUpArrow,0x6000,this,0,0,0,0,0);
	this->Fn2F6140(this->mDownArrow,0xa000,this,0,0,0,0,0);
	this->Fn2F6140(this->mInputBox,0x0402,this->mLabel,2,0,0,0,0);
	this->Fn2F6140(this->mInputBox,0x20000,this->mUpArrow,0,0,0,0,0);

	this->AddControls();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Creates an empty spinner
MSSpinner::MSSpinner()
{
	this->Init();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Creates a spinner with the label text and value range, sized to fit
MSSpinner::MSSpinner(const GUIString &theText, int minValue, int maxValue)
{
	this->Init();
	this->SetLabelText(theText);
	this->SetRange(minValue,maxValue,false);
	this->SetDesiredSize(0,0);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Sets the label text and lets the label resize to it
void MSSpinner::SetLabelText(const GUIString &theText)
{
	this->mLabel->SetText(theText);
	this->mLabel->SetDesiredWidth(0);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Returns a size fitting the label, the widest value (digits of the maximum, plus a sign if the minimum is
// negative) and the arrows; height is rounded so that half of it is odd
void MSSpinner::GetDesiredSize(int &width, int &height)
{
	int maxDigits = 0;
	int aNum = this->mMaxValue;
	do
	{
		maxDigits++;
		aNum/=10;
	} while(aNum>0);

	if(this->mMinValue<0)
		maxDigits++;

	this->mInputBox->GetDesiredSize(width,height);

	Font *aFont = this->mInputBox->mFont.get();
	int anInputWidth = aFont->GetCharacterWidth('0')*maxDigits + 10;
	width = this->mLabel->Width() + 2 + anInputWidth + MSSpinner_ButtonWidth;

	// want height/2 is odd -> height/2 = 2n+1 -> height = 4n+2
	int rem = (height+2)%4; // make sure height is divisible by 4
	if(rem!=0)
		height += (4-rem);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Sizes each arrow button to half the spinner's height, then lays out the children
void MSSpinner::Virtual08()
{
	int nWidth = this->Width();
	int nHeight = this->Height();
	int nBtnHt = nHeight/2;
	int nBtnWd = MSSpinner_ButtonWidth;
	this->mUpArrow->SetSize(nBtnWd, nBtnHt);
	this->mDownArrow->SetSize(nBtnWd, nBtnHt);

	Container::Virtual08();
}


}
