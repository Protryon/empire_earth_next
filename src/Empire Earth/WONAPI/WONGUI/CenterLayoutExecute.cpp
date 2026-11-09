// Centre layout: centres a group of components in their parent or in a centre box (file name inferred)
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
#include "ee/wonapi/WONLobbyLayout.h"
namespace WONAPI {
// FUNCTION: WON 0x007375a0
// Moves the layout's components together so their bounding box is centred horizontally and/or
// vertically in the parent, or in the centre box when one is set
void CenterLayout::Execute(Component* parent)
{
    if(this->mComponentList.empty())
        return;
    std::list<Component*>::iterator it;
    WONRectangle rectangle;
    for(it=this->mComponentList.begin();it!=this->mComponentList.end();++it)
    {
        Component* child=*it;
        rectangle.DoUnion(child->GetBounds());

    }
    int dx=0,dy=0;
    Component* center=parent;
    if(this->mCenterBox!=NULL && this->mCenterBox!=parent)
    {
        center=this->mCenterBox;
        if(this->mHorz) dx=center->Left();
        if(this->mVert) dy=center->Top();
    }
    if(this->mHorz) dx+=(center->Width()-rectangle.width)/2-rectangle.x;
    if(this->mVert) dy+=(center->Height()-rectangle.height)/2-rectangle.y;
    if(dx==0 && dy==0)
        return;
    for(it=this->mComponentList.begin();it!=this->mComponentList.end();++it)
    {
        Component* child=*it;
        child->SetPos(child->Left()+dx,child->Top()+dy);
    }
}
}
