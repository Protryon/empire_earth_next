// C:\EEX\Empire Earth\User Interface Thread\EEUIBColor.cpp (file name inferred: no __FILE__; RTTI class EEUIBColor).
// The player colour button: 0x2547e0..0x254911 (ctor, ??_G, the mouse slots, SetColor defined last; the
// EEUIBGenericBackground unit's init chains start at 0x254911). Unit flags: the exe module set.
#include <windows.h>
#include <mbstring.h>
#include <algorithm>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "GEWindow.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/GEBitmap.h"
#include "lle/FSFileSpec.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "lle/TMPoll.h"
#include "lle/TSCriticalSection.h"
#include "lle/TSScheduler.h"
#include "lle/SESample.h"
#include "lle/SESMInstanceNode.h"
#include "lle/SESoundManager.h"
#include "lle/Input System.h"
#include "lle/ISKeyboard.h"
#include "lle/ISMouse.h"
#include "lle/UIControl.h"
#include "lle/UIEvent.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIBackground.h"
#include "lle/UIFontTable.h"
#include "lle/UIHotKey.h"
#include "lle/UIIMECandidate.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UIButton.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMessage.h"
#include "ee/EECommunications.h"
#include "ee/EEGameOptions.h"
#include "ee/EEUIBColor.h"

// FUNCTION: EXE 0x6547e0
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

// SYNTHETIC: EXE 0x65486f
// (the scalar deleting destructor, emitted with the vtable; slot EEUIBColor+0x0)

// FUNCTION: EXE 0x654892
// Advances to the next player colour on a left click
long EEUIBColor::MouseLButtonUp()
{
    this->SetColor(this->mColor + 1);
    return UIButton::MouseLButtonUp();
}

// FUNCTION: EXE 0x6548ac
// Treats a right button press like a left one
long EEUIBColor::MouseRButtonDown()
{
    return UIButton::MouseLButtonDown();
}

// FUNCTION: EXE 0x6548b2
// Steps back to the previous player colour on a right click
long EEUIBColor::MouseRButtonUp()
{
    this->SetColor(this->mColor - 1);
    UIButton::MouseLButtonUp();
    return 0;
}

// FUNCTION: EXE 0x6548ce
// Sets the button's player colour, wrapping 16 to 0 and one below 0 to 15
void EEUIBColor::SetColor(unsigned long inColor)
{
    this->mColor = inColor;
    // Stepping back from colour 0 gives 0 - 1 = 0xffffffff on the unsigned colour: wrap it to 15 (the old
    // "< 0" test never fired, so it wrapped to 0).
    if (this->mColor == (unsigned long)-1)
        this->mColor = 15;
    else if (this->mColor >= 16)
        this->mColor = 0;
    this->SetBgColor(EEGetPlayerColor(this->mColor));
}
