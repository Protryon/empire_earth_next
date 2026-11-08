// ServerContext: a lock-protected, shuffled list of server addresses to try (file grouping inferred)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include <algorithm>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
namespace WONAPI {
// FUNCTION: WON 0x006ac390
// Removes every server address and re-enables the one-time shuffle
void ServerContext::Clear()
{
    AutoCrit guard(this->mLock);
    this->mPingMap.clear();
    this->mAddresses.clear();
    this->mShuffleAddresses = true;
    this->mAddressesShuffled = false;
}
// FUNCTION: WON 0x006ac4b0
// Appends a valid address not yet in the list; returns whether it was added
bool ServerContext::AddAddress(const IPAddr& inAddress)
{
    if (!inAddress.IsValid())
        return false;
    AutoCrit guard(this->mLock);
    typedef std::map<IPAddr,AddrList::iterator> AddrMap;
    std::pair<AddrMap::iterator,bool> inserted =
        this->mPingMap.insert(AddrMap::value_type(inAddress,this->mAddresses.end()));
    if (inserted.second) {
        inserted.first->second = this->mAddresses.insert(this->mAddresses.end(),inAddress);
        return true;
    }
    return false;
}

// FUNCTION: WON 0x006ac980
// Moves a server that failed to the end of the list
void ServerContext::NotifyFailed(const IPAddr& inAddress)
{
    AutoCrit guard(this->mLock);
    std::map<IPAddr,AddrList::iterator>::iterator it=this->mPingMap.find(inAddress);
    if (it==this->mPingMap.end()) return;
    this->mAddresses.erase(it->second);
    it->second=this->mAddresses.insert(this->mAddresses.end(),it->first);
}

// FUNCTION: WON 0x006acab0
// Adds the addresses of the directory's services (optionally only those with the given name)
bool ServerContext::AddAddressesFromDir(const DirEntityList& inEntities,const wchar_t* inNameFilter)
{
    bool added=false;
    std::wstring filter;
    if (inNameFilter!=NULL) filter=inNameFilter;
    for (DirEntityList::const_iterator it=inEntities.begin();it!=inEntities.end();++it) {
        DirEntity* entity=it->get();
        if (entity->mType=='D') continue;
        if (!filter.empty() && !(filter==entity->mName)) continue;
        if (this->AddAddress(entity->GetNetAddrAsIP())) added=true;
    }
    return added;
}

// FUNCTION: WON 0x006ac6d0
// Copies the address list, shuffling it randomly the first time
void ServerContext::CopyAddresses(AddrList& inCopy)
{
    AutoCrit aGuard(this->mLock);
    if (this->mShuffleAddresses && !this->mAddressesShuffled) {
        std::vector<IPAddr> anAddresses;
        anAddresses.reserve(this->mAddresses.size());
        for (AddrList::iterator anItr=this->mAddresses.begin();anItr!=this->mAddresses.end();++anItr)
            anAddresses.push_back(*anItr);
        srand(time(NULL));
        std::random_shuffle(anAddresses.begin(),anAddresses.end());
        this->mPingMap.clear();
        this->mAddresses.clear();
        for (std::vector<IPAddr>::iterator aPos=anAddresses.begin();aPos!=anAddresses.end();++aPos) {
            AddrList::iterator aLast=this->mAddresses.insert(this->mAddresses.end(),*aPos);
            this->mPingMap[*aPos]=aLast;
        }
        this->mAddressesShuffled=true;
    }
    inCopy=this->mAddresses;
}

}

namespace WONAPI {
// FUNCTION: WON 0x006acc00
// Removes every server address
void ServerContext::RemoveAllAddresses() { this->Clear(); }
}

// No original address identified: WON SDK address-management methods
namespace WONAPI {
bool ServerContext::RemoveAddress(const IPAddr &theAddr)
{
	AutoCrit aCrit(this->mLock);
	std::map<IPAddr,AddrList::iterator>::iterator anItr = this->mPingMap.find(theAddr);
	if(anItr==this->mPingMap.end())
		return false;

	this->mAddresses.erase(anItr->second);
	this->mPingMap.erase(anItr);
	return true;
}

void ServerContext::CopyAddresses(AddrSet &theCopy)
{
	AutoCrit aCrit(this->mLock);
	theCopy.clear();
	std::map<IPAddr,AddrList::iterator>::iterator aMapItr = this->mPingMap.begin();
	AddrSet::iterator aSetItr = theCopy.begin();
	while(aMapItr!=this->mPingMap.end())
	{
		aSetItr = theCopy.insert(aSetItr,aMapItr->first);
		++aMapItr;
	}
}

bool ServerContext::AddAddressesFromDir(const DirEntityMap &theDir, const std::wstring &thePath, const wchar_t *theNameFilter)
{
	DirEntityMap::const_iterator anItr = theDir.find(thePath);
	if(anItr==theDir.end())
		return false;
	else
		return this->AddAddressesFromDir(anItr->second,theNameFilter);
}

bool ServerContext::AtomicClearAndAddAddressesFromDir(const DirEntityList &theDir, const wchar_t *theNameFilter)
{
	AutoCrit aCrit(this->mLock);
	this->Clear();
	return this->AddAddressesFromDir(theDir, theNameFilter);
}

bool ServerContext::AtomicClearAndAddAddressesFromDir(const DirEntityMap &theDir, const std::wstring &thePath, const wchar_t *theNameFilter)
{
	AutoCrit aCrit(this->mLock);
	this->Clear();
	return this->AddAddressesFromDir(theDir, thePath, theNameFilter);	
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006aeb30
// (public: void __thiscall std::list<class WONAPI::IPAddr,class std::allocator<class WONAPI::IPAddr> >::_Insert(class std::list<class WONAPI::IPAddr,clas)
// TEMPLATE: WON 0x006aebf0
// (protected: struct std::list<class WONAPI::IPAddr,class std::allocator<class WONAPI::IPAddr> >::_Node * __thiscall std::list<class WONAPI::IPAddr,class)
// TEMPLATE: WON 0x006aef70
// (public: class std::_Tree<class WONAPI::IPAddr,struct std::pair<class WONAPI::IPAddr const ,class std::list<class WONAPI::IPAddr,class std::allocator<c)
// TEMPLATE: WON 0x006aefe0
// (public: unsigned int __thiscall std::vector<class WONAPI::IPAddr,class std::allocator<class WONAPI::IPAddr> >::capacity(void)const     [?capacity@?$ve)
// TEMPLATE: WON 0x006af010
// (public: unsigned int __thiscall std::vector<class WONAPI::IPAddr,class std::allocator<class WONAPI::IPAddr> >::size(void)const     [?size@?$vector@VIP)
// TEMPLATE: WON 0x006af0b0
// (protected: bool __thiscall std::vector<class WONAPI::IPAddr,class std::allocator<class WONAPI::IPAddr> >::_Buy(unsigned int)    [?_Buy@?$vector@VIPAdd)
// TEMPLATE: WON 0x006af120
// (protected: void __thiscall std::vector<class WONAPI::IPAddr,class std::allocator<class WONAPI::IPAddr> >::_Xlen(void)const     [?_Xlen@?$vector@VIPAdd)
// TEMPLATE: WON 0x006af8c0
// (public: class std::_Ptrit<class WONAPI::IPAddr,int,class WONAPI::IPAddr *,class WONAPI::IPAddr &,class WONAPI::IPAddr *,class WONAPI::IPAddr &> __this)
// TEMPLATE: WON 0x006af8e0
// (public: class std::_Ptrit<class WONAPI::IPAddr,int,class WONAPI::IPAddr *,class WONAPI::IPAddr &,class WONAPI::IPAddr *,class WONAPI::IPAddr &> __this)
// TEMPLATE: WON 0x006af940
// (public: void __thiscall std::list<class WONAPI::IPAddr,class std::allocator<class WONAPI::IPAddr> >::assign(class std::list<class WONAPI::IPAddr,class)
// TEMPLATE: WON 0x006af9f0
// (protected: class WONAPI::IPAddr * __thiscall std::vector<class WONAPI::IPAddr,class std::allocator<class WONAPI::IPAddr> >::_Ucopy(class std::_Ptrit<c)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006afd60
// (void __cdecl std::fill(class std::_Ptrit<class WONAPI::IPAddr,int,class WONAPI::IPAddr *,class WONAPI::IPAddr &,class WONAPI::IPAddr *,class WONAPI::I)
// TEMPLATE: WON 0x006aff00
// (class std::_Ptrit<class WONAPI::IPAddr,int,class WONAPI::IPAddr *,class WONAPI::IPAddr &,class WONAPI::IPAddr *,class WONAPI::IPAddr &> __cdecl std::c)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006af260
// (protected: void __thiscall std::list<class WONAPI::IPAddr,class std::allocator<class WONAPI::IPAddr> >::_Incsize(unsigned int)    [?_Incsize@?$list@VI)
// TEMPLATE: WON 0x006af3b0
// (public: void __thiscall std::vector<class WONAPI::IPAddr,class std::allocator<class WONAPI::IPAddr> >::insert(class std::_Ptrit<class WONAPI::IPAddr,i)
// TEMPLATE: WON 0x006afa30
// (void __cdecl std::_Construct(class WONAPI::IPAddr *,class WONAPI::IPAddr const &)    [?_Construct@std@@YAXPAVIPAddr@WONAPI@@ABV23@@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006ae8f0
// (protected: void __thiscall std::vector<class WONAPI::IPAddr,class std::allocator<class WONAPI::IPAddr> >::_Tidy(void)    [?_Tidy@?$vector@VIPAddr@WONA)
// TEMPLATE: WON 0x006aec70
// (protected: class std::_Tree<class WONAPI::IPAddr,struct std::pair<class WONAPI::IPAddr const ,class std::list<class WONAPI::IPAddr,class std::allocato)
// TEMPLATE: WON 0x006af040
// (public: class std::_Ptrit<class WONAPI::IPAddr,int,class WONAPI::IPAddr *,class WONAPI::IPAddr &,class WONAPI::IPAddr *,class WONAPI::IPAddr &> __this)
// TEMPLATE: WON 0x006af0f0
// (protected: void __thiscall std::vector<class WONAPI::IPAddr,class std::allocator<class WONAPI::IPAddr> >::_Destroy(class WONAPI::IPAddr *,class WONAPI)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006ae580
// (public: struct std::pair<class std::_Tree<class WONAPI::IPAddr,struct std::pair<class WONAPI::IPAddr const ,class std::list<class WONAPI::IPAddr,class)
// TEMPLATE: WON 0x006aeb90
// (public: class std::list<class WONAPI::IPAddr,class std::allocator<class WONAPI::IPAddr> >::iterator __thiscall std::list<class WONAPI::IPAddr,class st)
// TEMPLATE: WON 0x006afbc0
// (void __cdecl std::_Construct(struct std::pair<class WONAPI::IPAddr const ,class std::list<class WONAPI::IPAddr,class std::allocator<class WONAPI::IPAd)
// TEMPLATE: WON 0x006b00e0
// (void __cdecl std::iter_swap(class std::_Ptrit<class WONAPI::IPAddr,int,class WONAPI::IPAddr *,class WONAPI::IPAddr &,class WONAPI::IPAddr *,class WONA)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006af900
// (void __cdecl std::random_shuffle(class std::_Ptrit<class WONAPI::IPAddr,int,class WONAPI::IPAddr *,class WONAPI::IPAddr &,class WONAPI::IPAddr *,class)
// TEMPLATE: WON 0x006b0110
// (void __cdecl std::_Iter_swap(class std::_Ptrit<class WONAPI::IPAddr,int,class WONAPI::IPAddr *,class WONAPI::IPAddr &,class WONAPI::IPAddr *,class WON)
// TEMPLATE: WON 0x006b0050
// (void __cdecl std::_Random_shuffle(class std::_Ptrit<class WONAPI::IPAddr,int,class WONAPI::IPAddr *,class WONAPI::IPAddr &,class WONAPI::IPAddr *,c)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006ae670
// (public: __thiscall std::pair<class WONAPI::IPAddr const ,class std::list<class WONAPI::IPAddr,class std::allocator<class WONAPI::IPAddr> >::iterator>:)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00770090
// (public: void * __thiscall WONAPI::IPAddr::`scalar deleting destructor'(unsigned int))
