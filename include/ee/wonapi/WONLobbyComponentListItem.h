#ifndef EE_WONAPI_WONLOBBYCOMPONENTLISTITEM_H
#define EE_WONAPI_WONLOBBYCOMPONENTLISTITEM_H
#include "ee/wonapi/WONLobbyGUI.h"
namespace WONAPI {
// 0x20 bytes, vtable 0x0084df70: ListItem (0x18) + owned component at +0x18 + sizable flag at +0x1c.
class ComponentListItem : public ListItem {
protected:
    virtual ~ComponentListItem(); // explicit destructor, at 0x006fdaa0
public:
    ComponentListItem(Component* component,bool sizable=false,bool selectable=false)
        :mComponent(component),mIsSizable(sizable) { this->mSelectable=selectable; }
    void CheckInvalid(ListArea*); // empty, at 0x006fa500
    virtual void Paint(Graphics&,ListArea*);     // at 0x006fa510
    virtual void CalcDimensions(ListArea*); // at 0x006fa480
    virtual void SetViewport(ListArea*,Container*); // at 0x006fa4a0
    virtual void SetPos(int,int);    // at 0x006fa4c0
    virtual bool Contains(int,int);  // at 0x006fa4e0
    virtual void MouseEnter(ListArea*,int,int); // at 0x006fd970
    virtual void MouseExit(ListArea*); // at 0x006fd9a0
    virtual void MouseMove(ListArea*,int,int); // at 0x006fd9c0
    virtual void MouseDrag(ListArea*,int,int); // at 0x006fd9f0
    virtual void MouseDown(ListArea*,int,int,int); // at 0x006fda20
    virtual void MouseUp(ListArea*,int,int,int);   // at 0x006fda50
    SmartPtr<Component> mComponent; //18
    bool mIsSizable; //1c, natural1d..1f padding
};
}
#endif
