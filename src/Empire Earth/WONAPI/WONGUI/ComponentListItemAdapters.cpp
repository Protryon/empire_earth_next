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
// Takes the item's size from its component, when it has one
void ComponentListItem::CalcDimensions(ListArea*)
{
    if(this->mComponent.get()!=NULL) {
        this->mWidth=this->mComponent->Width();
        this->mHeight=this->mComponent->Height();
    }
}
// Parents the component to the list viewport and notifies it when attached
void ComponentListItem::SetViewport(ListArea*,Container* viewport)
{
    this->mComponent->mParent=viewport;
    if(viewport!=NULL) this->mComponent->AddedToParent();
}
// Moves the item and its component
void ComponentListItem::SetPos(int x,int y)
{
    ListItem::SetPos(x,y);
    this->mComponent->SetPos(x,y);
}
// Tests the point against the component
bool ComponentListItem::Contains(int x,int y) { return this->mComponent->Contains(x,y); }
// Forwards mouse enter to the component
void ComponentListItem::MouseEnter(ListArea*,int x,int y) { this->mComponent->MouseEnter(x,y); }
// Forwards mouse exit to the component
void ComponentListItem::MouseExit(ListArea*) { this->mComponent->MouseExit(); }
// Forwards a mouse move to the component
void ComponentListItem::MouseMove(ListArea*,int x,int y) { this->mComponent->MouseMove(x,y); }
// Forwards a mouse drag to the component
void ComponentListItem::MouseDrag(ListArea*,int x,int y) { this->mComponent->MouseDrag(x,y); }
// Forwards a mouse press to the component
void ComponentListItem::MouseDown(ListArea*,int x,int y,int button) { this->mComponent->MouseDown(x,y,button); }
// Forwards a mouse release to the component
void ComponentListItem::MouseUp(ListArea*,int x,int y,int button) { this->mComponent->MouseUp(x,y,button); }
// Destroys the list item, releasing its component (emitted at 0x006fdaa0)
ComponentListItem::~ComponentListItem() {}
}
