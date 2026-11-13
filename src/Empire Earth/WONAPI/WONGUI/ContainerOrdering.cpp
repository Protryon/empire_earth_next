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
#include "rust/ee_won.h"
// The last focused child's pick and the focus ids are ee_won's guifocus module (rust/crates/ee_won/src/guifocus.rs).
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
    std::vector<Component*> aChildren;
    std::vector<unsigned char> anIds;
    for(std::list<SmartPtr<Component> >::iterator i=this->mChildList.begin();i!=this->mChildList.end();++i) {
        aChildren.push_back(i->get());
        anIds.push_back((*i)->mFocusId);
    }
    // the byte difference is compared as a signed int before it is kept as a byte (as in the original)
    long long aPick=ee_won_gui_last_focus_child(current,skipCurrent,anIds.empty()?NULL:&anIds[0],anIds.size());
    if(aPick>=0) aChildren[(size_t)aPick]->RequestFocus();
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
// The sibling walk of Component::GetNextSibling (0x006f5230): the child after or before inChild, wrapping when asked
Component* Container::NextChild(Component* inChild,bool forward,bool wrap)
{
    std::list<SmartPtr<Component> >::iterator i=inChild->mChildListItr;
    if(forward) {
        ++i;
        if(i==this->mChildList.end()) {
            if(!wrap) return NULL;
            return this->mChildList.front().get();
        }
    } else {
        if(i==this->mChildList.begin()) {
            if(!wrap) return NULL;
            return this->mChildList.back().get();
        }
        --i;
    }
    return i->get();
}
// ScrollAreaViewport::Reset's statements (0x00705c70): the children are not told
void Container::ResetChildren()
{
    this->mMouseChild=this->mFocusChild=NULL;
    this->mChildTimerSet.clear();
    this->mChildList.clear();
}
// FUNCTION: WON 0x006f6010
// Returns the next focus id for a child, skipping 0 when the byte wraps
unsigned char Container::GetNextFocusId()
{
    this->mNextFocusId=ee_won_gui_next_focus_id(this->mNextFocusId);
    return this->mNextFocusId;
}
}
