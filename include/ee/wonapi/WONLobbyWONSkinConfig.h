#ifndef EE_WONAPI_WONLOBBYWONSKINCONFIG_H
#define EE_WONAPI_WONLOBBYWONSKINCONFIG_H
#include "ee/wonapi/WONLobbyGUI.h"
namespace WONAPI {
// ComponentCursor (0x14 bytes, allocated at 0x0079a0ab, constructor 0x00726710), four slots;
// the constructor, destructor and virtuals are out of line.
class ComponentCursor : public Cursor {
protected:
    SmartPtr<Component> mComponent; //08
    int mMouseX,mMouseY; //0c,10
public:
    // Implicit destructor (0x00726770): releases the component, then Cursor; stores no own vptr.
    ComponentCursor(Component*,int=0,int=0); // at 0x00726710
    virtual Component* GetComponent(int&,int&); // slot 0x08, at 0x007267d0
};
}
#endif
