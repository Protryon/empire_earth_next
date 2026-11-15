// C:\EEX\Empire Earth\User Interface Thread\EEUIBColor.cpp (file name inferred: no __FILE__; RTTI class EEUIBColor).
// The player colour button: 0x2547e0..0x254911 (ctor, ??_G, the mouse slots, SetColor defined last; the
// EEUIBGenericBackground unit's init chains start at 0x254911). Unit flags: the exe module set.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GERhombus.h"
#include "lle/UIButton.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEUIBColor.h"

// Creates a 25x25 player colour button showing the given colour
EEUIBColor::EEUIBColor(unsigned long inColor, unsigned long inId, const GE2DPoint& inLocation, UIForm* inForm,
                       unsigned long inIndex)
    : UIButton(inId, NULL, NULL, NULL,
               GERectangle(inLocation, 25.0f, 25.0f), inForm, inIndex, 3),
      mColor(inColor)
{
    this->SetBgColor(EEGetPlayerColor(inColor));
    this->SetBgOpacity(1.0f);
}

// Advances to the next player colour on a left click
long EEUIBColor::MouseLButtonUp()
{
    this->SetColor(this->mColor + 1);
    return UIButton::MouseLButtonUp();
}

// Treats a right button press like a left one
long EEUIBColor::MouseRButtonDown()
{
    return UIButton::MouseLButtonDown();
}

// Steps back to the previous player colour on a right click
long EEUIBColor::MouseRButtonUp()
{
    this->SetColor(this->mColor - 1);
    UIButton::MouseLButtonUp();
    return 0;
}

// Sets the button's player colour, wrapping 16 to 0 and one below 0 to 15
void EEUIBColor::SetColor(unsigned long inColor)
{
    this->mColor = inColor;
    // Stepping back from colour 0 gives 0 - 1 = 0xffffffff on the unsigned colour: wrap it to 15 (the old
    // "< 0" test never fired, so it wrapped to 0).
    if (UIsNone32(this->mColor))
        this->mColor = 15;
    else if (this->mColor >= 16)
        this->mColor = 0;
    this->SetBgColor(EEGetPlayerColor(this->mColor));
}
