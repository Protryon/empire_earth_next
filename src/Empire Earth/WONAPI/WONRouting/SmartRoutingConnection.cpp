// WONAPI SmartRoutingConnection: client, group and member tracking, ignore list and chat commands
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONAPICore.h"
#include "ee/wonapi/WONRoutingOpTypes.h"
#include "ee/wonapi/WONStatus.h"
// The model, the ignore list, the chat commands and the copies are a Rust object (rust/crates/ee_won/src/
// smartroute.rs EeWonSmartRoute); the class's forwarders are in WONRouting.h, the copies into its records here
#include "rust/ee_won.h"
namespace WONAPI {
static std::wstring SmartText(const uint32_t* inUnits, size_t inCount)
{
    std::wstring s;
    s.reserve(inCount);
    for (size_t i = 0; i < inCount; i++)
        s += static_cast<wchar_t>(inUnits[i]);
    return s;
}
// Creates the smart routing connection with the ignore file under the WON directory and the default chat commands
SmartRoutingConnection::SmartRoutingConnection()
{
    std::string aPath = WONAPICore::GetDefaultFileDirectory() + "_wonignore.bin";
    this->mModel = ee_won_smart_new(sizeof(wchar_t), reinterpret_cast<const uint8_t*>(aPath.data()), aPath.length());
}

// the copies
static void CopyClient(SmartRoutingConnection::ClientInfo& out, const EeWonSmartRecord& r)
{
    out.mFlags = r.flags;
    out.mId = r.id;
    out.mName = SmartText(r.name, r.name_len);
    out.mCDKeyId = r.cdkey;
}
static void CopyGroup(SmartRoutingConnection::GroupInfo& out, const EeWonSmartRecord& r)
{
    out.mCaptainId = r.captain;
    out.mFlags = r.flags;
    out.mAsyncFlags = r.async_flags;
    out.mId = r.id;
    out.mMaxPlayers = r.max_clients;
    out.mName = SmartText(r.name, r.name_len);
    out.mObserverCount = r.observers;
    out.mMemberCount = r.member_count;
}
static void CopyMember(SmartRoutingConnection::MemberInfo& out, const EeWonSmartRecord& r)
{
    if (r.has_client) {
        out.mName = SmartText(r.name, r.name_len);
        out.mClientFlags = r.client_flags;
    }
    out.mClientId = r.id;
    out.mMemberFlags = r.member_flags;
}
#define SMART_GET(which, a, b, copy, out) \
    AutoCrit aCrit(this->GetDataLock()); \
    EeWonSmartRecord r; \
    if (!ee_won_smart_get(this->mModel, which, a, b, &r)) return false; \
    copy(out, r); \
    return true
bool SmartRoutingConnection::GetClient(unsigned short id, ClientInfo& out)
{ SMART_GET(EeWonSmartGet::Client, id, 0, CopyClient, out); }
bool SmartRoutingConnection::GetGroup(unsigned short id, GroupInfo& out)
{ SMART_GET(EeWonSmartGet::Group, id, 0, CopyGroup, out); }
bool SmartRoutingConnection::GetMember(unsigned short group, unsigned short client, MemberInfo& out)
{ SMART_GET(EeWonSmartGet::Member, group, client, CopyMember, out); }
bool SmartRoutingConnection::GetNextClient(ClientInfo& out) { SMART_GET(EeWonSmartGet::NextClient, 0, 0, CopyClient, out); }
bool SmartRoutingConnection::GetNextGroup(GroupInfo& out) { SMART_GET(EeWonSmartGet::NextGroup, 0, 0, CopyGroup, out); }
bool SmartRoutingConnection::GetNextMember(MemberInfo& out) { SMART_GET(EeWonSmartGet::NextMember, 0, 0, CopyMember, out); }
bool SmartRoutingConnection::GetLastTargetMember(MemberInfo& out)
{ SMART_GET(EeWonSmartGet::LastMember, 0, 0, CopyMember, out); }
bool SmartRoutingConnection::GetLastTargetClient(ClientInfo& out)
{ SMART_GET(EeWonSmartGet::LastClient, 0, 0, CopyClient, out); }
// Finds the client a typed name refers to: exact, unique prefix, ambiguous or not found
SmartRoutingConnection::MatchResult SmartRoutingConnection::FindClient(const std::wstring& inName, ClientInfo& out,
                                                                       bool& outFound)
{
    AutoCrit aCrit(this->GetDataLock());
    EeWonSmartRecord r;
    int result = ee_won_smart_find(this->mModel, inName.data(), inName.length(), &r, &outFound);
    if (outFound) CopyClient(out, r);
    return (MatchResult)result;
}
}
