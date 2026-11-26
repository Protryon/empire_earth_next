// RootContainer (R6 WON round 29): the Rust RootContainer's calls out (rust/crates/ee_won_gui/src/rootobj.rs, EeWonGuiOp's
// root ops; the container's fall back to Container.cpp's), its constructor and destructor. The original's
// RootContainer.cpp (the invalid rectangle, the queued backgrounds, the cursor's component, the window drag) is the
// Rust object.
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
#include "ee/wonapi/WONComponentFlags.h"
#include "rust/won_gui/guihost.h"
#include "rust/won_gui/rootobj.h"
using namespace WONAPI;

typedef EeWonGuiOp Op;

extern "C" intptr_t ee_won_guicpp_root_call(void* inComp, EeWonGuiOp op, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    RootContainer* t = static_cast<RootContainer*>((Component*)inComp);
    switch (op) {
    case Op::RootCursorComponent:
        // (no cursor: no component; the original's MouseMove read through a NULL current cursor)
        if (a == 0) { *(int*)b = *(int*)c = 0; return 0; }
        return (intptr_t)((Cursor*)a)->GetComponent(*(int*)b, *(int*)c);
    // (the window's state, rectangle and place, the mouse child, the colour scheme, the cursor component's place,
    // order and showing are gamewin.rs root_op's in the game: R6 WON GUI round 38; the grabbers' and the images'
    // round 39)
    default: return ContainerHostCall(t, op, a, b, c, d);
    }
}

// Creates the top-level container of a window: arrow cursor, window dragging allowed, tab focus kept inside (the Rust
// root: no invalid rectangle, no cursor component, no drag, no background)
RootContainer::RootContainer(Window *theWindow)
{
	this->mRootObj = ee_won_root_new(static_cast<Component*>(this), ee_won_guicpp_root_call, this->mCt);
	this->mWindow = theWindow;
	this->SetDefaultCursor(Cursor::GetStandardCursor(StandardCursor_Arrow));
	this->SetComponentFlags(0x8 | 0x40000,true);
	this->SetIsolateTabFocus(true);
}

// Destroys the root: the Rust root gives the queued backgrounds' images back (the implicit destructor in the original)
RootContainer::~RootContainer() { ee_won_root_free(this->mRootObj); }
