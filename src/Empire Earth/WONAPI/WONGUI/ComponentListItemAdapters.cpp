// ComponentListItem: a list item that hosts a component (file name inferred)
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
#include "ee/wonapi/WONLobbyComponentListItem.h"
namespace WONAPI {
// FUNCTION: WON 0x006fa480
// Takes the item's size from its component, when it has one
void ComponentListItem::CalcDimensions(ListArea*)
{
    if(this->mComponent.get()!=NULL) {
        this->mWidth=this->mComponent->Width();
        this->mHeight=this->mComponent->Height();
    }
}
// FUNCTION: WON 0x006fa4a0
// Parents the component to the list viewport and notifies it when attached
void ComponentListItem::SetViewport(ListArea*,Container* viewport)
{
    this->mComponent->mParent=viewport;
    if(viewport!=NULL) this->mComponent->AddedToParent();
}
// FUNCTION: WON 0x006fa4c0
// Moves the item and its component
void ComponentListItem::SetPos(int x,int y)
{
    ListItem::SetPos(x,y);
    this->mComponent->SetPos(x,y);
}
// FUNCTION: WON 0x006fa4e0
// Tests the point against the component
bool ComponentListItem::Contains(int x,int y) { return this->mComponent->Contains(x,y); }
// FUNCTION: WON 0x006fa500
// Does nothing (component items need no repaint check)
void ComponentListItem::CheckInvalid(ListArea*) {}
// FUNCTION: WON 0x006fd970
// Forwards mouse enter to the component, then checks for repaint
void ComponentListItem::MouseEnter(ListArea* area,int x,int y)
{
    this->mComponent->MouseEnter(x,y);
    this->CheckInvalid(area);
}
// FUNCTION: WON 0x006fd9a0
// Forwards mouse exit to the component, then checks for repaint
void ComponentListItem::MouseExit(ListArea* area)
{
    this->mComponent->MouseExit();
    this->CheckInvalid(area);
}
// FUNCTION: WON 0x006fd9c0
// Forwards a mouse move to the component, then checks for repaint
void ComponentListItem::MouseMove(ListArea* area,int x,int y)
{
    this->mComponent->MouseMove(x,y);
    this->CheckInvalid(area);
}
// FUNCTION: WON 0x006fd9f0
// Forwards a mouse drag to the component, then checks for repaint
void ComponentListItem::MouseDrag(ListArea* area,int x,int y)
{
    this->mComponent->MouseDrag(x,y);
    this->CheckInvalid(area);
}
// FUNCTION: WON 0x006fda20
// Forwards a mouse press to the component, then checks for repaint
void ComponentListItem::MouseDown(ListArea* area,int x,int y,int button)
{
    this->mComponent->MouseDown(x,y,button);
    this->CheckInvalid(area);
}
// FUNCTION: WON 0x006fda50
// Forwards a mouse release to the component, then checks for repaint
void ComponentListItem::MouseUp(ListArea* area,int x,int y,int button)
{
    this->mComponent->MouseUp(x,y,button);
    this->CheckInvalid(area);
}
// Destroys the list item, releasing its component (emitted at 0x006fdaa0)
ComponentListItem::~ComponentListItem() {}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006fdaa0
// (protected: virtual __thiscall WONAPI::ComponentListItem::~ComponentListItem(void)    [??1ComponentListItem@WONAPI@@MAE@XZ])
