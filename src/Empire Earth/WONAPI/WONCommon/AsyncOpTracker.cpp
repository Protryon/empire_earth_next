// AsyncOpTracker: ops tracked by id and by op, with optional duplicates
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
namespace WONAPI {

inline AsyncOpTrackerMaps::EntryMap::iterator
AsyncOpTrackerMaps::erase(EntryMap::iterator itr)
{
    AsyncOpTrackerEntry* value=itr->second;
    this->mEntries.erase(itr++);
    this->mOps.erase(value->mOpItr);
    delete value;
    return itr;
}

inline AsyncOpTrackerMaps::OpMap::iterator
AsyncOpTrackerMaps::erase_val(OpMap::iterator itr)
{
    this->erase((itr++)->second);
    return itr;
}

static inline void InsertTrackedOp(AsyncOpTrackerMaps& maps,const unsigned long& id,const AsyncOpPtr& op)
{
    AsyncOpTrackerMaps::EntryMap::iterator idItr=maps.mEntries.find(id);
    if(!maps.mAllowDuplicateIds && idItr!=maps.mEntries.end()) return;
    AsyncOpTrackerMaps::OpMap::iterator opItr=maps.mOps.find(op);
    if(!maps.mAllowDuplicateOps && opItr!=maps.mOps.end()) return;
    opItr=maps.mOps.insert(AsyncOpTrackerMaps::OpMap::value_type(op,maps.mEntries.end()));
    AsyncOpTrackerEntry* entry=new AsyncOpTrackerEntry(op,opItr);
    AsyncOpTrackerMaps::EntryMap::iterator it=maps.mEntries.insert(idItr,AsyncOpTrackerMaps::EntryMap::value_type(id,entry));
    opItr->second=it;
}

static inline bool EraseTrackedOp(AsyncOpTrackerMaps& maps,const AsyncOpPtr& op)
{
    if(!maps.mAllowDuplicateOps) {
        AsyncOpTrackerMaps::OpMap::iterator opItr=maps.mOps.find(op);
        if(opItr==maps.mOps.end()) return false;
        maps.erase(opItr->second);
    } else {
        AsyncOpTrackerMaps::OpMap::iterator first=maps.mOps.lower_bound(op);
        AsyncOpTrackerMaps::OpMap::iterator last=maps.mOps.upper_bound(op);
        if(first==last) return false;
        maps.erase_val(first,last);
    }
    return true;
}

// FUNCTION: WON 0x0069fc80
// Tracks an op under the given id (refused when duplicates are not allowed); returns the op
AsyncOp* AsyncOpTracker::AddOp(AsyncOp* op,unsigned long id)
{
    AutoCrit lock(this->mLock);
    InsertTrackedOp(this->mMaps,id,op);
    return op;
}
// FUNCTION: WON 0x0069fe50
// Stops tracking an op (every entry of it when duplicates are allowed); returns whether it was tracked
bool AsyncOpTracker::RemoveOp(AsyncOp* op)
{
    AutoCrit lock(this->mLock);
    AsyncOpPtr hold=op;
    bool erased=EraseTrackedOp(this->mMaps,hold);
    return erased;
}
// FUNCTION: WON 0x0069ffa0
// Stops tracking every op and kills each one with completion
void AsyncOpTracker::KillAll()
{
    AutoCrit lock(this->mLock);
    AsyncOpTrackerMaps::EntryMap::iterator itr=this->mMaps.begin();
    while(itr!=this->mMaps.end()) {
        AsyncOpPtr op=itr->second->mOp;
        this->mMaps.erase(itr++);
        op->Kill(true);
    }
}
}

namespace WONAPI {
// FUNCTION: WON 0x006a9040
// Erases the tracker entries for a range of the op map; returns the end of the range
AsyncOpTrackerMaps::OpMap::iterator
AsyncOpTrackerMaps::erase_val(OpMap::iterator first,OpMap::iterator last)
{
    while(first!=last)
        first=this->erase_val(first);
    return first;
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006a8e70
// (public: class std::_Tree<unsigned long,struct std::pair<unsigned long const ,struct WONAPI::AsyncOpTrackerEntry *>,struct std::_Mmapk<unsigned long,st)
// TEMPLATE: WON 0x006a9340
// (public: class std::_Tree<unsigned long,struct std::pair<unsigned long const ,struct WONAPI::AsyncOpTrackerEntry *>,struct std::_Mmapk<unsigned long,st)
// TEMPLATE: WON 0x006a99b0
// (protected: class std::_Tree<unsigned long,struct std::pair<unsigned long const ,struct WONAPI::AsyncOpTrackerEntry *>,struct std::_Mmapk<unsigned long)
// TEMPLATE: WON 0x006a9250
// (public: __thiscall std::pair<unsigned long const ,struct WONAPI::AsyncOpTrackerEntry *>::pair<unsigned long const ,struct WONAPI::AsyncOpTrackerEntry )
// TEMPLATE: WON 0x006a92d0
// (public: class std::_Tree<class WONAPI::SmartPtr<class WONAPI::AsyncOp>,struct std::pair<class WONAPI::SmartPtr<class WONAPI::AsyncOp> const ,class std)
// TEMPLATE: WON 0x006a9380
// (public: __thiscall WONAPI::AsyncOpTrackerEntry::AsyncOpTrackerEntry(class WONAPI::SmartPtr<class WONAPI::AsyncOp> const &,class std::_Tree<class WONAP)
// TEMPLATE: WON 0x006a9cb0
// (protected: class std::_Tree<class WONAPI::SmartPtr<class WONAPI::AsyncOp>,struct std::pair<class WONAPI::SmartPtr<class WONAPI::AsyncOp> const ,class )
// TEMPLATE: WON 0x006a9140
// (public: class std::_Tree<unsigned long,struct std::pair<unsigned long const ,struct WONAPI::AsyncOpTrackerEntry *>,struct std::_Mmapk<unsigned long,st)
// TEMPLATE: WON 0x006a9270
// ( ?? ?? :: ?? ::insert)
