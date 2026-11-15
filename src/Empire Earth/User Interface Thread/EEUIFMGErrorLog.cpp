// C:\EEX\Empire Earth\User Interface Thread\EEUIFMGErrorLog.cpp (file name inferred: no __FILE__; RTTI class
// EEUIFMGErrorLog). The in-game form's error log list box: 0x267ad9..0x267b77 (ctor, ??_G, MouseLButtonDown).
// Unit flags: the exe module set.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GERhombus.h"
#include "lle/GEText.h"
#include "lle/UIListBox.h"
#include "lle/UILBString.h"
#include "ee/EEUIFMGErrorLog.h"

// Creates the in-game error log list box: transparent text, no selection highlight
EEUIFMGErrorLog::EEUIFMGErrorLog(const GERectangle& inRectangle, UIForm* inForm, GEFont* inFont,
                                 std::vector<UWideString>& inData, unsigned long inIndex, unsigned long inId)
    : UILBString(inId, inData, inFont, false, inRectangle, inForm, inIndex, 0)
{
    GEText* theText = this->GetGEText();
    theText->SetFgColor(GEColor());
    theText->SetIsTransparent(true);
    this->GetSelectedBackground()->SetOpacity(0.0f);
}

// Handles a click in the error log, releasing the mouse unless the scroll bar holds it
long EEUIFMGErrorLog::MouseLButtonDown()
{
    long theResult = UIListBox<UWideString>::MouseLButtonDown();
    if (!this->GetScrollBarCaptured())
        this->ReleaseCapturedMouse();
    return theResult;
}
