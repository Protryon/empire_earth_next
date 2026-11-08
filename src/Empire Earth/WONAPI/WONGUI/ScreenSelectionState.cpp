// Screen container switching and input box selection state
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
// FUNCTION: WON 0x00706a70
// Adds a hidden screen to the container, laid out to the container's size
void ScreenContainer::AddScreen(Component* screen)
{
    screen->SetVisible(false);
    this->Fn2F6140(screen,3,this,0,0,0,0,0);
    this->AddChild(screen);
}

// FUNCTION: WON 0x00706ab0
// Hides the current screen and shows the given one, optionally giving it focus; no-op if it is already current
void ScreenContainer::ShowScreen(Component* screen,bool takeFocus)
{
    if(screen==this->mCurScreen)
        return;
    if(this->mCurScreen!=NULL)
        this->mCurScreen->SetVisible(false);
    screen->SetVisible(true);
    this->mCurScreen=screen;
    if(screen!=NULL && takeFocus)
        screen->RequestFocus();
    this->Invalidate();
}

// FUNCTION: WON 0x006f84b0
// Turns text selection on (I-beam cursor) or off (default cursor), clearing any selection when turned off
void InputBox::SetAllowSelection(bool allowSelection)
{
    if(allowSelection==this->mAllowSelection)
        return;
    this->mAllowSelection=allowSelection;
    if(allowSelection)
        this->SetDefaultCursor(Cursor::GetStandardCursor(StandardCursor_IBeam));
    else
        this->SetDefaultCursor(NULL);
    this->mSelecting=false;
    if(!this->mAllowSelection && this->mSelStartChar!=this->mSelEndChar) {
        this->mSelStartChar=this->mSelEndChar=0;
        this->Invalidate();
    }
}
}
