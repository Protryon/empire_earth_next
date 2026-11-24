#ifndef EE_WONAPI_WONLOBBYWONSKINCONFIG_H
#define EE_WONAPI_WONLOBBYWONSKINCONFIG_H
#include "ee/wonapi/WONLobbyGUI.h"
namespace WONAPI {
// ComponentCursor (0x14 bytes, allocated at 0x0079a0ab, constructor 0x00726710): a cursor drawn by a component, with
// the mouse hot spot inside it. R6 WON GUI round 35: Cursor's component kind (cursorobj.rs holds the component and
// the hot spot and answers GetComponent, 0x007267d0); the class is its constructor.
class ComponentCursor : public Cursor {
public:
    ComponentCursor(Component* c,int x=0,int y=0) : Cursor(ee_won_cursor_new_component(WONCursorHost,(intptr_t)c,x,y)) {}
};
}
#endif
