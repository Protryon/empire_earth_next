// C:\EEX\Low-Level Engine\User Interface\UIESliderChange.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x1006e6de..0x1006f278: the header-inline ctor, dtor, AddString, SetModifier, SetOffset, the implicit copy ctor,
// operator= and ??_E, ProcessEvent, then the map<float, UWideString> COMDATs. No U2D key chains. UIEvent.cpp's key chains start
// at 0x1006f278.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GERhombus.h"
#include "lle/GEBitmap.h"
#include "lle/UIControl.h"
#include "lle/UISlider.h"
#include "lle/UITextLabel.h"
#include "lle/UIEvent.h"
#include "lle/UIESliderChange.h"

// Shows the slider's value (times the modifier, plus the offset) in the label, or the text mapped to that value
long UIESliderChange::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UWideString theText;
    float theValue = static_cast<UISlider*>(inEntity)->GetCurrentRangeValue() * this->mModifier + this->mOffset;
    if (this->mStrings.find(theValue) != this->mStrings.end())
        sprintf(theText, "%s", (const char*)this->mStrings[theValue]);
    else
        sprintf(theText, "%g", theValue);
    this->mLabel->SetText(theText, true);
    return kErrorOK;
}
