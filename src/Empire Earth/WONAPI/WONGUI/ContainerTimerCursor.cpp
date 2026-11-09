// Container child timers and cursor propagation (file name inferred)
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
#include "ee/wonapi/WONComponentFlags.h"
namespace WONAPI {
// FUNCTION: WON 0x006f70c0
// Registers a child that wants timer events and requests timer events from the parent chain
void Container::AddTimerChild(Component* child)
{
    if(child->mTimerFlags!=0) {
        this->mChildTimerSet.insert(child);
        bool requestParent=this->mTimerFlags==0;
        this->mTimerFlags|=1;
        if(requestParent && this->mParent!=NULL) this->mParent->AddTimerChild(this);
    }
}
// FUNCTION: WON 0x006f7210
// Sends a timer tick to every registered child, dropping those that no longer want ticks; returns whether
// the container still wants them
bool Container::TimerEvent(int elapsed)
{
    bool childNeeds=false;
    ChildTimerSet::iterator i=this->mChildTimerSet.begin();
    while(i!=this->mChildTimerSet.end()) {
        Component* child=i->get();
        if(child->mTimerFlags!=0 && child->mParent==this && child->TimerEvent(elapsed)) {
            childNeeds=true;
            ++i;
        } else {
            child->RequestTimer(false);
            this->mChildTimerSet.erase(i++);
        }
    }
    if(!childNeeds) this->mTimerFlags&=~1;
    return this->mTimerFlags!=0;
}
// FUNCTION: WON 0x006f7360
// Sets the cursor (the default one for NULL) and passes it up; a mouse child with the 0x40 flag forces
// the default cursor
void Container::SetCursor(Cursor* cursor)
{
    if(this->mMouseChild!=NULL && this->mMouseChild->ComponentFlagSet(ComponentFlag_NoParentCursor)) {
        Cursor* fallback=this->GetDefaultCursor();
        if(this->mCurrentCursor.get()!=fallback) this->mParent->SetCursor(fallback);
        return;
    }
    if(cursor==NULL) cursor=this->GetDefaultCursor();
    if(cursor!=this->mCurrentCursor.get()) {
        this->mCurrentCursor=cursor;
        if(this->mParent!=NULL) this->mParent->SetCursor(cursor);
    }
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0071be00
// (protected: class std::_Tree<class WONAPI::SmartPtr<class WONAPI::Component>,class WONAPI::SmartPtr<class WONAPI::Component>,struct std::_Setk<class WO)
