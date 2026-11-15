// Component extra-state accessors, color schemes, fonts and mouse helpers (file name inferred)
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
// Returns the component's background grabber, or NULL
BackgroundGrabber* Component::GetGrabBG()
{
    return this->mExtraStuff!=NULL ? this->mExtraStuff->mGrabBG : NULL;
}
// Returns the component's default cursor, or NULL
Cursor* Component::GetDefaultCursor()
{
    return this->mExtraStuff!=NULL ? this->mExtraStuff->mDefaultCursor.get() : NULL;
}
// Sets the component's listener (one listener; a new one replaces the old)
void Component::AddListener(ComponentListener* listener)
{
    this->AllocateExtraStuff();
    this->mExtraStuff->mListener=listener;
}
// Sets the component's default cursor
void Component::SetDefaultCursor(Cursor* cursor)
{
    this->AllocateExtraStuff();
    this->mExtraStuff->mDefaultCursor=cursor;
}
// Subscribes or unsubscribes the component to broadcast events
void Component::SubscribeToBroadcast(bool subscribe)
{
    this->GetRoot()->GetWindowManager()->SubscribeToBroadcast(this,subscribe);
}
// Returns the window's mouse position in this component's coordinates
void Component::GetMousePos(int& x,int& y)
{
    Window* window=this->GetRoot();
    x=window->GetMouseX();
    y=window->GetMouseY();
    this->RootToChild(x,y);
}
// Returns the window's current key modifiers
int Component::GetKeyMod()
{
    return this->GetRoot()->GetKeyMod();
}
// Returns the component's own color scheme, or NULL
ColorScheme* Component::GetOwnColorScheme()
{
    return this->mExtraStuff!=NULL ? this->mExtraStuff->mColorScheme.get() : NULL;
}
// Returns the component's color scheme, else its parent's, else the window manager's
ColorScheme* Component::GetColorScheme()
{
    if(this->mExtraStuff!=NULL && this->mExtraStuff->mColorScheme.get()!=NULL)
        return this->mExtraStuff->mColorScheme.get();
    if(this->mParent!=NULL) return this->mParent->GetColorScheme();
    Window* w=this->GetRoot();
    return w->GetWindowManager()->mColorScheme.get();
}
// Sets the component's own color scheme
void Component::SetColorScheme(ColorScheme* scheme)
{
    this->AllocateExtraStuff();
    this->mExtraStuff->mColorScheme=scheme;
}
// Returns the window manager's default font
Font* Component::GetDefaultFont()
{
    return this->GetRoot()->mManager->GetDefaultFont();
}
// Returns the window manager's font with the given name
Font* Component::GetNamedFont(const std::string& name)
{
    return this->GetRoot()->GetWindowManager()->GetNamedFont(name);
}
// Returns the default font in another style and size
Font* Component::GetDefaultFontMod(int style,int size)
{
    return this->GetRoot()->GetWindowManager()->GetDefaultFontMod(style,size);
}
}
