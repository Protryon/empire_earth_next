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
#include "ee/wonapi/WONGUIDraw.h"
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

// Returns the selection background color (the graphics context is unused)
unsigned long SelectionColor::GetBackgroundColor(Graphics&) const { return this->mBackgroundColor; }
// Returns the selected-text color for the mode: the original color, its inverse, the set foreground, else white
unsigned long SelectionColor::GetForegroundColor(Graphics&,unsigned long original) const
{
    // ee_won's guidraw.rs sel_foreground
    return ee_won_gui_sel_foreground((uint32_t)this->mBackgroundColor,(uint32_t)this->mForegroundColor,
                                     (int32_t)this->mMode,(uint32_t)original);
}
}
