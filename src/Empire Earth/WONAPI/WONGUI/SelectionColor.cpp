// Selection colors: a background color plus the rule for the foreground of selected text (file name inferred)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
namespace WONAPI {
// Creates the default selection colors: highlight text color on the highlight color
SelectionColor::SelectionColor()
{
    this->mForegroundColor=0x80000008;
    this->mBackgroundColor=0x80000007;
    this->mMode=ForegroundSelectionMode_UseForegroundColor;
}
// Creates a selection color with the given background, a white foreground and the given foreground mode
SelectionColor::SelectionColor(DWORD background,ForegroundSelectionMode mode)
{
    this->mForegroundColor=0xffffff;
    this->mBackgroundColor=background;
    this->mMode=mode;
}
SelectionColor::SelectionColor(DWORD background,DWORD foreground)
    :mBackgroundColor(background),mForegroundColor(foreground),mMode(ForegroundSelectionMode_UseForegroundColor) {}
// FUNCTION: WON 0x007494e0
// Creates a selection color with the given background and foreground colors (constructor body above)
// (??0SelectionColor@WONAPI@@QAE@KK@Z)

// FUNCTION: WON 0x00749500
// Returns the selection background color (the graphics context is unused)
unsigned long SelectionColor::GetBackgroundColor(Graphics&) const { return this->mBackgroundColor; }
// FUNCTION: WON 0x00749510
// Returns the selected-text color for the mode: the original color, its inverse, the set foreground, else white
unsigned long SelectionColor::GetForegroundColor(Graphics&,unsigned long original) const
{
    switch(this->mMode)
    {
    case ForegroundSelectionMode_UseOriginalColor: return original;
    case ForegroundSelectionMode_InvertOriginalColor: return original^0xffffff;
    case ForegroundSelectionMode_UseForegroundColor: return this->mForegroundColor;
    default: return 0xffffff;
    }
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007494a0
// (public: __thiscall WONAPI::SelectionColor::SelectionColor(void))
// TEMPLATE: WON 0x007494c0
// (public: __thiscall WONAPI::SelectionColor::SelectionColor(unsigned long,enum WONAPI::ForegroundSelectionMode))
