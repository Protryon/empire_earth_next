// C:\EEX\Low-Level Engine\User Interface\UIESliderChange.h (RECONSTRUCTION). An event that writes a slider's value
// (scaled by mModifier, plus mOffset) into a text label, or the string registered for that value. UIESliderChange.cpp
// (0x1006e6de..0x1006f278) emits the inline members first (declaration order), then the implicit copy members and
// ??_E, then ProcessEvent (the only out-of-line member). Vtable 0x100999a0. sizeof 0x24. Requires <map>,
// lle/UIEvent.h.
#ifndef LLE_UIESLIDERCHANGE_H
#define LLE_UIESLIDERCHANGE_H

#include <map>

#include "lle/UIEvent.h"
#include "lle/UITextLabel.h"
#include "lle/UISlider.h"

class UITextLabel;

class LLE_API UIESliderChange : public UIEvent {
public:
    // header-inline (the ??_E expands the dtor): 0x1006e6de..0x1006e7b2
    UIESliderChange(UITextLabel* inLabel, float inModifier, long inOffset)
    {
        this->mLabel = inLabel;
        this->mModifier = inModifier;
        this->mOffset = inOffset;
    }
    virtual ~UIESliderChange() { this->mLabel = NULL; }
    void AddString(float inValue, UWideString inString) { this->mStrings.insert(std::make_pair(inValue, inString)); }
    // Shows the slider's value (scaled by the modifier, plus the offset) in the label: its string when one was
    // added for that value, else the number
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity)
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
    void SetModifier(float inModifier) { this->mModifier = inModifier; }
    void SetOffset(long inOffset) { this->mOffset = inOffset; }
private:
    std::map<float, UWideString> mStrings;  // +0x0c value -> text shown instead of the number
    UITextLabel* mLabel;                    // +0x18
    long mOffset;                           // +0x1c
    float mModifier;                        // +0x20
};

#endif
