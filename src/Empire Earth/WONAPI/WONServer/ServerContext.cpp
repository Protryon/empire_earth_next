// ServerContext: a shim over ee_won's server list (rust/crates/ee_won/src/servctx.rs), which holds the addresses,
// their order, the one-time shuffle and the lock; the shim converts IPAddr values and walks directory entities
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
namespace WONAPI {
static EeWonAddr ToRust(const IPAddr& inAddress)
{
    EeWonAddr a;
    memcpy(a.sockaddr, &inAddress.mAddr, sizeof(a.sockaddr));
    a.valid = inAddress.mIsValid;
    a.remember_host = inAddress.mRememberHostString;
    a.host = reinterpret_cast<const uint8_t*>(inAddress.mHost.data());
    a.host_len = inAddress.mHost.size();
    return a;
}
static IPAddr FromRust(const EeWonAddr* inAddress)
{
    IPAddr anAddr;
    memcpy(&anAddr.mAddr, inAddress->sockaddr, sizeof(inAddress->sockaddr));
    anAddr.mIsValid = inAddress->valid;
    anAddr.mRememberHostString = inAddress->remember_host;
    anAddr.mHost.assign(reinterpret_cast<const char*>(inAddress->host), inAddress->host_len);
    return anAddr;
}
static void ToList(void* inList, const EeWonAddr* inAddress)
{
    static_cast<ServerContext::AddrList*>(inList)->push_back(FromRust(inAddress));
}
static void ToSet(void* inSet, const EeWonAddr* inAddress)
{
    AddrSet* aSet = static_cast<AddrSet*>(inSet);
    aSet->insert(aSet->end(), FromRust(inAddress));
}
// srand(time(NULL)) and rand(), the CRT's (the original's random_shuffle drew from rand)
static int32_t ContextRand(bool inSeed)
{
    if (inSeed) {
        srand(time(NULL));
        return 0;
    }
    return rand();
}

ServerContext::ServerContext() : mList(ee_won_ctx_new()) {}
ServerContext::~ServerContext() { ee_won_ctx_free(this->mList); }

// FUNCTION: WON 0x006ac390
// Removes every server address and re-enables the one-time shuffle
void ServerContext::Clear() { ee_won_ctx_clear(this->mList); }
// FUNCTION: WON 0x006ac4b0
// Appends a valid address not yet in the list; returns whether it was added
bool ServerContext::AddAddress(const IPAddr& inAddress)
{
    EeWonAddr a = ToRust(inAddress);
    return ee_won_ctx_add(this->mList, &a);
}
// FUNCTION: WON 0x006ac980
// Moves a server that failed to the end of the list
void ServerContext::NotifyFailed(const IPAddr& inAddress)
{
    EeWonAddr a = ToRust(inAddress);
    ee_won_ctx_notify_failed(this->mList, &a);
}
// the directory's services' addresses (optionally only those with the given name)
static void DirAddresses(const DirEntityList& inEntities, const wchar_t* inNameFilter, std::vector<IPAddr>& outAddrs)
{
    std::wstring filter;
    if (inNameFilter != NULL) filter = inNameFilter;
    for (DirEntityList::const_iterator it = inEntities.begin(); it != inEntities.end(); ++it) {
        DirEntity* entity = it->get();
        if (entity->mType == 'D') continue;
        if (!filter.empty() && !(filter == entity->mName)) continue;
        outAddrs.push_back(entity->GetNetAddrAsIP());
    }
}
// FUNCTION: WON 0x006acab0
// Adds the addresses of the directory's services (optionally only those with the given name)
bool ServerContext::AddAddressesFromDir(const DirEntityList& inEntities, const wchar_t* inNameFilter)
{
    std::vector<IPAddr> anAddrs;
    DirAddresses(inEntities, inNameFilter, anAddrs);
    bool added = false;
    for (size_t i = 0; i < anAddrs.size(); i++)
        if (this->AddAddress(anAddrs[i])) added = true;
    return added;
}
// FUNCTION: WON 0x006ac6d0
// Copies the address list, shuffling it randomly the first time
void ServerContext::CopyAddresses(AddrList& inCopy)
{
    inCopy.clear();
    ee_won_ctx_copy(this->mList, ContextRand, ToList, &inCopy);
}
// FUNCTION: WON 0x006acc00
// Removes every server address
void ServerContext::RemoveAllAddresses() { this->Clear(); }
unsigned long ServerContext::GetNumAddresses() { return (unsigned long)ee_won_ctx_len(this->mList); }
void ServerContext::SetShuffleAddresses(bool inShuffle) { ee_won_ctx_set_shuffle(this->mList, inShuffle); }
bool ServerContext::GetShuffleAddresses() { return ee_won_ctx_shuffle(this->mList); }
}

// No original address identified: WON SDK address-management methods
namespace WONAPI {
bool ServerContext::RemoveAddress(const IPAddr& theAddr)
{
    EeWonAddr a = ToRust(theAddr);
    return ee_won_ctx_remove(this->mList, &a);
}

void ServerContext::CopyAddresses(AddrSet& theCopy)
{
    theCopy.clear();
    ee_won_ctx_copy_sorted(this->mList, ToSet, &theCopy);
}

bool ServerContext::AddAddressesFromDir(const DirEntityMap& theDir, const std::wstring& thePath,
                                        const wchar_t* theNameFilter)
{
    DirEntityMap::const_iterator anItr = theDir.find(thePath);
    if (anItr == theDir.end())
        return false;
    return this->AddAddressesFromDir(anItr->second, theNameFilter);
}

// cleared and refilled under the list's one lock
bool ServerContext::AtomicClearAndAddAddressesFromDir(const DirEntityList& theDir, const wchar_t* theNameFilter)
{
    std::vector<IPAddr> anAddrs;
    DirAddresses(theDir, theNameFilter, anAddrs);
    std::vector<EeWonAddr> aRust;
    for (size_t i = 0; i < anAddrs.size(); i++) aRust.push_back(ToRust(anAddrs[i]));
    return ee_won_ctx_replace(this->mList, aRust.empty() ? NULL : &aRust[0], aRust.size());
}

bool ServerContext::AtomicClearAndAddAddressesFromDir(const DirEntityMap& theDir, const std::wstring& thePath,
                                                      const wchar_t* theNameFilter)
{
    DirEntityMap::const_iterator anItr = theDir.find(thePath);
    if (anItr == theDir.end()) {
        // the original cleared the list before looking the path up
        ee_won_ctx_replace(this->mList, NULL, 0);
        return false;
    }
    return this->AtomicClearAndAddAddressesFromDir(anItr->second, theNameFilter);
}
}
