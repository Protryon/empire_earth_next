// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include <set>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyWONSkinConfig.h"
using namespace WONAPI;
// The standard cursors, one slot per StandardCursor value (initialised and cleaned up by the $E functions
// listed at the end)
static SmartPtr<CursorRef> gStandardCursors[StandardCursor_Max+1];
// Creates a cursor (base class, no state)
Cursor::Cursor() {}
// Destroys the cursor (empty; emitted at 0x00728590)
Cursor::~Cursor() {}
// Default hooks: no Windows cursor and no attached component (the linker folds these bodies into the
// identical ones at 0x006c7b10 and 0x00726b50)
Cursor* Cursor::GetNative() { return NULL; }
Component* Cursor::GetComponent(int&,int&) { return NULL; }
// Returns the cursor currently set for a standard cursor type (NULL if none)
Cursor* Cursor::GetStandardCursor(StandardCursor type) { return gStandardCursors[type].get(); }
// Sets the cursor for a standard cursor type, creating its shared reference on first use
void Cursor::SetStandardCursor(StandardCursor type,Cursor* cursor) {
    if(gStandardCursors[type].get()==NULL)
        gStandardCursors[type]=new CursorRef(cursor);
    else
        gStandardCursors[type]->SetCursor(cursor);
}
// ComponentCursor follows ~Cursor in the original object (VC6 then omits the -1 EH state before the base
// destructor call in ~ComponentCursor, 0x726770).
namespace WONAPI {
// Creates a cursor drawn by a component, with the mouse hot spot inside it
ComponentCursor::ComponentCursor(Component* component,int mouseX,int mouseY)
    : mComponent(component)
{
    this->mMouseX=mouseX;
    this->mMouseY=mouseY;
}
// Returns the cursor's component and its mouse hot spot
Component* ComponentCursor::GetComponent(int& mouseX,int& mouseY)
{
    mouseX=this->mMouseX;
    mouseY=this->mMouseY;
    return this->mComponent.get();
}
}
// (MSCursor's destructor, 0x007d0320, went with the GDI backend: PlatCursor is in PlatNativeImage.cpp)
