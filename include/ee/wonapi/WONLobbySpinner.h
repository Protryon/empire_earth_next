// Spinner and MSSpinner (not identified in the original exe).
#ifndef EE_WONAPI_WONLOBBYSPINNER_H
#define EE_WONAPI_WONLOBBYSPINNER_H
#include "ee/wonapi/WONLobbyGUI.h"
namespace WONAPI {
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
class Spinner : public Container
{
protected:
	SmartPtr<InputBox> mInputBox; 
	SmartPtr<Button> mUpArrow; 
	SmartPtr<Button> mDownArrow; 
	int mMaxValue;   
	int mMinValue;   
	int	mValue;
	bool mValueIsValid;
	bool mWrapAround;

public:
	virtual void AddedToParent();
	virtual bool KeyDown(int theKey);
	virtual void GetDesiredSize(int &width, int &height);
	virtual void EnabledChanged(bool isEnabled);
	virtual void HandleComponentEvent(ComponentEvent* pEvent);

	void ForceValueCompliance();
	void SetTextToValue();
	void CalcValueFromText();
	void AddControls();

	void CheckValueChanged(bool oldValid, int oldValue);

public:
	Spinner();

	void IncrementValue(int theAmount = 1);
	void DecrementValue(int theAmount = 1);
	void SetValue(int theValue, bool sendEvent = true);

	void SetMax(int theMax);
	void SetMin(int theMin);
	void SetRange(int theMin, int theMax, bool sendEvent = true);
	void SetNoType(bool noType);
	void SetWrapAround(bool wrap);

	int GetMax(); 
	int GetMin(); 
	int GetValue();
	bool IsValueValid();
};

class MSSpinner : public Spinner
{
protected:
	SmartPtr<Label> mLabel;

	virtual void GetDesiredSize(int &width, int &height);
	virtual void Virtual08();

	void Init();
public:
	MSSpinner();
	MSSpinner(const GUIString &theText, int minValue, int maxValue);

	void SetLabelText(const GUIString &theText);
	Label* GetLabel() { return this->mLabel; }
};
typedef SmartPtr<MSSpinner> MSSpinnerPtr;
}
#endif
