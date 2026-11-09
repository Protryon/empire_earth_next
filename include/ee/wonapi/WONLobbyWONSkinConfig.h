#ifndef EE_WONAPI_WONLOBBYWONSKINCONFIG_H
#define EE_WONAPI_WONLOBBYWONSKINCONFIG_H
#include "ee/wonapi/WONLobbyWONListConfig.h"
#include "ee/wonapi/WONLobbySkinTab.h"
namespace WONAPI {
void WONControlsConfig_Init(ResourceConfigTable*); // at 0x00795fa0
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
    virtual bool IsNative(); // slot 0x0c, at 0x0070a7b0
};
// Fieldless config (0x78 bytes; declarations only).
class WONSkinConfig : public ComponentConfig {
public:
    static ComponentConfig* CfgFactory(); // at 0x00796d20
    virtual bool PreParse(ConfigParser*);  // at 0x00799010
    virtual bool Virtual24(const std::string&); // dispatch, at 0x00799210
protected:
    bool ReadVert(); // at 0x00798f90
    void HandleButtonClickSound(); //3990e0
};
}
#endif
