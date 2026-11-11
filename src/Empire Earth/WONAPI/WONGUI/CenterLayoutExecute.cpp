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
#include "rust/ee_rust.h"
// The centring shift is ee_won's guilayout module (rust/crates/ee_won/src/guilayout.rs).
namespace WONAPI {
// FUNCTION: WON 0x007375a0
// Moves the layout's components together so their bounding box is centred horizontally and/or
// vertically in the parent, or in the centre box when one is set
void CenterLayout::Execute(Component* parent)
{
    if(this->mComponentList.empty())
        return;
    std::list<Component*>::iterator it;
    std::vector<EeWonRect> aBounds;
    for(it=this->mComponentList.begin();it!=this->mComponentList.end();++it)
    {
        const WONRectangle& b=(*it)->GetBounds();
        EeWonRect r = { b.x, b.y, b.width, b.height };
        aBounds.push_back(r);
    }
    bool aBox = this->mCenterBox!=NULL && this->mCenterBox!=parent;
    Component* center = aBox ? this->mCenterBox : parent;
    EeWonRect aCenter = { center->Left(), center->Top(), center->Width(), center->Height() };
    int dx=0,dy=0;
    if(!ee_won_gui_center_shift(&aBounds[0],aBounds.size(),aCenter,aBox,this->mHorz,this->mVert,&dx,&dy))
        return;
    for(it=this->mComponentList.begin();it!=this->mComponentList.end();++it)
    {
        Component* child=*it;
        child->SetPos(child->Left()+dx,child->Top()+dy);
    }
}
}
