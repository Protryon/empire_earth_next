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
// FUNCTION: WON 0x00726540
// Creates a cursor (base class, no state)
Cursor::Cursor() {}
// Destroys the cursor (empty; emitted at 0x00728590)
Cursor::~Cursor() {}
// Default hooks: no Windows cursor and no attached component (the linker folds these bodies into the
// identical ones at 0x006c7b10 and 0x00726b50)
Cursor* Cursor::GetNative() { return NULL; }
Component* Cursor::GetComponent(int&,int&) { return NULL; }
// FUNCTION: WON 0x00726550
// Returns the cursor currently set for a standard cursor type (NULL if none)
Cursor* Cursor::GetStandardCursor(StandardCursor type) { return gStandardCursors[type].get(); }
// FUNCTION: WON 0x00726560
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
// FUNCTION: WON 0x00726710
// Creates a cursor drawn by a component, with the mouse hot spot inside it
ComponentCursor::ComponentCursor(Component* component,int mouseX,int mouseY)
    : mComponent(component)
{
    this->mMouseX=mouseX;
    this->mMouseY=mouseY;
}
// FUNCTION: WON 0x007267d0
// Returns the cursor's component and its mouse hot spot
Component* ComponentCursor::GetComponent(int& mouseX,int& mouseY)
{
    mouseX=this->mMouseX;
    mouseY=this->mMouseY;
    return this->mComponent.get();
}
// A component cursor is not a Windows cursor (the linker folds this body into the identical one at
// 0x0070a7b0)
bool ComponentCursor::IsNative() { return false; }
}
// FUNCTION: WON 0x007d0320
// Destroys the Windows cursor, freeing the cursor handle if it owns it
MSCursor::~MSCursor() { if(this->mDoDestroy) DestroyCursor(this->mNativeCursor); }

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00726690
// (public: virtual void * __thiscall WONAPI::CursorRef::`scalar deleting destructor'(unsigned int)    [??_GCursorRef@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x007266b0
// (public: virtual __thiscall WONAPI::CursorRef::~CursorRef(void)    [??1CursorRef@WONAPI@@UAE@XZ])
// TEMPLATE: WON 0x00728590
// (protected: virtual __thiscall WONAPI::Cursor::~Cursor(void)    [??1Cursor@WONAPI@@MAE@XZ])
// TEMPLATE: WON 0x007286b0
// (protected: virtual void * __thiscall WONAPI::Cursor::`scalar deleting destructor'(unsigned int)    [??_GCursor@WONAPI@@MAEPAXI@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00726750
// (public: virtual void * __thiscall WONAPI::ComponentCursor::`scalar deleting destructor'(unsigned int)    [??_GComponentCursor@WONAPI@@UAEPAXI@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00726770
// (public: virtual __thiscall WONAPI::ComponentCursor::~ComponentCursor(void)    [??1ComponentCursor@WONAPI@@UAE@XZ])
// TEMPLATE: WON 0x007264e0
// (_$E74)
// TEMPLATE: WON 0x007264f0
// (_$E71)
// TEMPLATE: WON 0x00726510
// (_$E73)
// TEMPLATE: WON 0x00726520
// (_$E72)
