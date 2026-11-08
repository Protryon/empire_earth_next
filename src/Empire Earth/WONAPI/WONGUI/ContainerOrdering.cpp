// Container focus history and child stacking order (file name inferred)
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
namespace WONAPI {
// FUNCTION: WON 0x006f6030
// Takes focus from the focused child and gives it to the child focused most recently before it (by focus id)
void Container::SetFocusToLastFocusChild()
{
    unsigned char current=this->mNextFocusId;
    bool skipCurrent=false;
    if(this->mFocusChild!=NULL) {
        current=this->mFocusChild->mFocusId;
        this->mFocusChild->LostFocus();
        this->mFocusChild=NULL;
        skipCurrent=true;
    }
    Component* last=NULL;
    unsigned char minimum=0;
    for(std::list<SmartPtr<Component> >::iterator i=this->mChildList.begin();i!=this->mChildList.end();++i) {
        Component* child=i->get();
        unsigned char id=child->mFocusId;
        if(id==0 || (id==current && skipCurrent)) continue;
        // The difference is compared as a signed int before it is stored as a byte (as in the original)
        if(minimum==0 || int(current)-int(id)<int(minimum)) {
            minimum=(unsigned char)(current-id);
            last=child;
        }
    }
    if(last!=NULL) last->RequestFocus();
}
// FUNCTION: WON 0x006f6f60
// Makes sure one child is drawn above another, moving the other one just beneath it if needed
void Container::EnsureOnTopOf(Component* bottom,Component* top)
{
    if(bottom->mParent!=this || top->mParent!=this) return;
    std::list<SmartPtr<Component> >::iterator i=bottom->mChildListItr;
    while(i!=top->mChildListItr && i!=this->mChildList.end()) ++i;
    if(i!=this->mChildList.end()) return;
    SmartPtr<Component> keep=bottom;
    this->mChildList.erase(bottom->mChildListItr);
    bottom->mChildListItr=this->mChildList.insert(top->mChildListItr,bottom);
}
// FUNCTION: WON 0x006f6010
// Returns the next focus id for a child, skipping 0 when the byte wraps
unsigned char Container::GetNextFocusId()
{
    if(++this->mNextFocusId==0) ++this->mNextFocusId;
    return this->mNextFocusId;
}
}
