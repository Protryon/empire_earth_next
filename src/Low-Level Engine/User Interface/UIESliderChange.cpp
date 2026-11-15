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

// FUNCTION: LLE 0x1006e8cf
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

// Header-inline members (emitted first, declaration order), then the implicit copy members and ??_E
// SYNTHETIC: LLE 0x1006e6de
// SYNTHETIC: LLE 0x1006e727
// SYNTHETIC: LLE 0x1006e744
// SYNTHETIC: LLE 0x1006e79e
// SYNTHETIC: LLE 0x1006e7a8
// FUNCTION: LLE 0x1006e7b2
// FUNCTION: LLE 0x1006e813
// SYNTHETIC: LLE 0x1006e84c

// Compiler-generated for this unit (names in analysis/lle/names.txt):
// TEMPLATE: LLE 0x1006e966
// TEMPLATE: LLE 0x1006e9b1
// TEMPLATE: LLE 0x1006e9dc
// TEMPLATE: LLE 0x1006ea10
// TEMPLATE: LLE 0x1006eac9
// TEMPLATE: LLE 0x1006eb06
// TEMPLATE: LLE 0x1006eb77
// TEMPLATE: LLE 0x1006ebd7
// TEMPLATE: LLE 0x1006ec0c
// TEMPLATE: LLE 0x1006eda0
// TEMPLATE: LLE 0x1006f056
// TEMPLATE: LLE 0x1006f0d9
// TEMPLATE: LLE 0x1006f113
// TEMPLATE: LLE 0x1006f13f
// TEMPLATE: LLE 0x1006f181
// TEMPLATE: LLE 0x1006f1c5
// TEMPLATE: LLE 0x1006f206
// TEMPLATE: LLE 0x1006f23e
// TEMPLATE: LLE 0x1006f25b
