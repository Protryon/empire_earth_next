// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
// Spinner: a numeric input box with up/down arrows, range, wrap-around and change events
// (not identified in the original exe)
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
Spinner::Spinner()
{
	this->mMinValue   = 0;
	this->mMaxValue   = 1000;

	this->mValue = 0;
	this->mValueIsValid = true;
	this->mWrapAround = false;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void Spinner::GetDesiredSize(int &width, int &height)
{
	width = 40;
	height = 22;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void Spinner::AddedToParent()
{
	if(this->mUpArrow->mParent!=this)
		this->AddControls();

	Container::AddedToParent();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void Spinner::AddControls()
{
	this->mUpArrow->SetButtonFlags(0x0002,true);
	this->mDownArrow->SetButtonFlags(0x0002,true);	

	this->AddChild(this->mInputBox);
	this->AddChild(this->mUpArrow);
	this->AddChild(this->mDownArrow);

	this->SetRange(this->mMinValue,this->mMaxValue,false);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void Spinner::SetTextToValue()
{
	if(!this->mValueIsValid)
	{
		this->mInputBox->Clear(true);
		return;
	}

	char aBuf[32];
	sprintf(aBuf,"%d",this->mValue);
	this->mInputBox->SetText(aBuf,false);
	this->mInputBox->SelectRange(-1,-1);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool Spinner::KeyDown(int theKey)
{
	if(theKey==VK_UP)
		this->IncrementValue();
	else if(theKey==VK_DOWN)
		this->DecrementValue();
	else
		return Container::KeyDown(theKey);

	return true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void Spinner::EnabledChanged(bool isEnabled)
{
	Container::EnabledChanged(isEnabled);

	this->mInputBox->SetEnabled(isEnabled);
	this->mUpArrow->SetEnabled(isEnabled);
	this->mDownArrow->SetEnabled(isEnabled);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void Spinner::IncrementValue(int theAmount)
{
	bool oldValid = this->mValueIsValid;
	int oldValue = this->mValue;

	this->mValue += theAmount;
	if(this->mWrapAround)
	{
		if(this->mValue > this->mMaxValue)
			this->mValue = this->mMinValue;
		else if(this->mValue < this->mMinValue)
			this->mValue = this->mMaxValue;
	}
	else
		this->ForceValueCompliance();
	
	this->SetTextToValue();

	this->CheckValueChanged(oldValid,oldValue);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void Spinner::DecrementValue(int theAmount)
{
	this->IncrementValue(-theAmount);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void Spinner::SetMax(int theMax) 
{ 
	this->SetRange(this->mMinValue, theMax);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void Spinner::SetMin(int theMin) 
{ 
	this->SetRange(theMin, this->mMaxValue);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void Spinner::SetRange(int theMin, int theMax, bool sendEvent) 
{ 
	bool oldValid = this->mValueIsValid;
	int oldValue = this->mValue;

	this->mMinValue = theMin;
	this->mMaxValue = theMax;
	this->ForceValueCompliance();
	this->SetTextToValue();

	int maxDigits = 0;
	int aNum = this->mMaxValue;
	do
	{
		maxDigits++;
		aNum/=10;
	} while(aNum>0);

	if(this->mMinValue<0)
		maxDigits++;

	this->mInputBox->mMaxChars=maxDigits;

	if(sendEvent)
		this->CheckValueChanged(oldValid,oldValue);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void Spinner::SetValue(int theValue, bool sendEvent)
{
	bool oldValid = this->mValueIsValid;
	int oldValue = this->mValue;

	if(!(theValue>=this->mMinValue) || !(theValue<=this->mMaxValue))
		return;
	this->mValue = theValue;
	this->mValueIsValid = true;
	this->SetTextToValue();
	if(sendEvent)
		this->CheckValueChanged(oldValid,oldValue);


}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void Spinner::ForceValueCompliance()
{
	if(this->mMaxValue < this->mMinValue)
	{
		this->mValueIsValid = false;
		return;
	}
	
	if(this->mValue > this->mMaxValue)
		this->mValue = this->mMaxValue;
	else if(this->mValue < this->mMinValue)
		this->mValue = this->mMinValue;

	this->mValueIsValid = true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void Spinner::SetWrapAround(bool wrap)
{
	this->mWrapAround = wrap;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void Spinner::SetNoType(bool noType)
{
	this->mInputBox->SetComponentFlags(ComponentFlag_WantFocus,!noType);
	this->mInputBox->SetAllowSelection(!noType);
	if(noType && this->mFocusChild==this->mInputBox.get())
		this->SetFocusChild(NULL);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void Spinner::CalcValueFromText()
{
	bool oldValid = this->mValueIsValid;
	int oldValue = this->mValue;

	if(this->mInputBox->GetText().length()==0)
		this->mValueIsValid = false;
	else
	{
		this->mValue = this->mInputBox->GetText().mData->ToInt(0);
		this->mValueIsValid = this->mValue>=this->mMinValue && this->mValue<=this->mMaxValue;
	}

	this->CheckValueChanged(oldValid, oldValue);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
int Spinner::GetValue()
{
	return this->mValue;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool Spinner::IsValueValid()
{
	return this->mValueIsValid;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void Spinner::CheckValueChanged(bool oldValid, int oldValue)
{
	bool changed = false;
	if(oldValid!=this->mValueIsValid)
		changed = true;
	else if(this->mValueIsValid && oldValue!=this->mValue)
		changed = true;

	if(changed)
		this->FireEvent(ComponentEvent_SpinnerValueChanged);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void Spinner::HandleComponentEvent(ComponentEvent* pRawEvent)
{
	if (pRawEvent->mType == ComponentEvent_ButtonPressed)
	{
		int nAdj = 1;
		if ((this->GetKeyMod() & KEYMOD_SHIFT) != 0)
			nAdj = 10;
		if ((this->GetKeyMod() & KEYMOD_CTRL) != 0)
			nAdj = 100;

		if (pRawEvent->mComponent == this->mUpArrow)
		{
			this->IncrementValue(nAdj);
			return;
		}
		else if (pRawEvent->mComponent == this->mDownArrow)
		{
			this->DecrementValue(nAdj);
			return;
		}
	}
	else if (pRawEvent->mType == ComponentEvent_InputTextChanged)
	{
		this->CalcValueFromText();
		return;
	}

	Container::HandleComponentEvent(pRawEvent);
}

}
