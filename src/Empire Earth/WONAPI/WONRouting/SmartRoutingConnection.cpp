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
// smartroute.rs EeWonSmartRoute); this class hands each finished op (a Rust op, routeop.rs) in and sends the requests
// chat makes
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
// FUNCTION: WON 0x006bfb00
// Creates the smart routing connection with the ignore file under the WON directory and the default chat commands
SmartRoutingConnection::SmartRoutingConnection()
{
    std::string aPath = WONAPICore::GetDefaultFileDirectory() + "_wonignore.bin";
    this->mModel = ee_won_smart_new(sizeof(wchar_t), reinterpret_cast<const uint8_t*>(aPath.data()), aPath.length());
}
SmartRoutingConnection::~SmartRoutingConnection() { ee_won_smart_free(this->mModel); }

// FUNCTION: WON 0x6c0be0
// Updates the client, group and member maps from a finished routing op or notification, by op type; a chat,
// invitation or join attempt from an ignored client loses its completion
void SmartRoutingConnection::RoutingOpCompleteHook(RoutingOp *theOp)
{
    AutoCrit aCrit(this->GetDataLock());
    if (ee_won_smart_routeop(this->mModel, theOp->mRust, theOp->GetStatus()))
        theOp->SetCompletion(NULL); // swallow the completion
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
void SmartRoutingConnection::StartClientItr() { AutoCrit aCrit(this->GetDataLock()); ee_won_smart_start(this->mModel, 0, 0); }
void SmartRoutingConnection::StartGroupItr() { AutoCrit aCrit(this->GetDataLock()); ee_won_smart_start(this->mModel, 1, 0); }
bool SmartRoutingConnection::StartMemberItr(unsigned short id)
{ AutoCrit aCrit(this->GetDataLock()); return ee_won_smart_start(this->mModel, 2, id); }

// FUNCTION: WON 0x6c35a0
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
// FUNCTION: WON 0x006c37d0
// Ignores or unignores a client and saves the ignore list
void SmartRoutingConnection::IgnoreClient(unsigned short inClientId, bool on)
{ AutoCrit aCrit(this->GetDataLock()); ee_won_smart_ignore(this->mModel, inClientId, on); }
// FUNCTION: WON 0x006c3970
// Maps a chat command word (case-insensitive) to a command
void SmartRoutingConnection::AddCommandString(const std::wstring& inText, InputCommand inCommand)
{ AutoCrit aCrit(this->GetDataLock()); ee_won_smart_add_command(this->mModel, inText.data(), inText.length(), inCommand); }
void SmartRoutingConnection::ClearCommandStrings()
{ AutoCrit aCrit(this->GetDataLock()); ee_won_smart_clear_commands(this->mModel); }
// FUNCTION: WON 0x006c40a0
// Sets whether chat commands accept a unique name prefix or only exact names
void SmartRoutingConnection::SetAllowPartialNameMatch(bool inAllow)
{ AutoCrit aCrit(this->GetDataLock()); ee_won_smart_set(this->mModel, EeWonSmartNumber::PartialNames, inAllow); }
// FUNCTION: WON 0x006c40f0
// Sets whether mute/ban commands apply server-wide
void SmartRoutingConnection::SetServerWideCommands(bool inOn)
{ AutoCrit aCrit(this->GetDataLock()); ee_won_smart_set(this->mModel, EeWonSmartNumber::ServerWide, inOn); }
void SmartRoutingConnection::SetDoColonEmote(bool inOn)
{ AutoCrit aCrit(this->GetDataLock()); ee_won_smart_set(this->mModel, EeWonSmartNumber::ColonEmote, inOn); }
// FUNCTION: WON 0x006c2fc0
// Sets the path of the ignore list file
void SmartRoutingConnection::SetProductName(const std::string& inPath)
{
    AutoCrit aCrit(this->GetDataLock());
    ee_won_smart_set_path(this->mModel, reinterpret_cast<const uint8_t*>(inPath.data()), inPath.length());
}
unsigned short SmartRoutingConnection::GetMyClientId()
{
    AutoCrit aCrit(this->GetDataLock());
    return (unsigned short)ee_won_smart_number(this->mModel, EeWonSmartNumber::ClientId);
}

// A line of chat input: a command, a colon emote or a broadcast; the request it makes is sent here
bool SmartRoutingConnection::HandleInput(const std::wstring& inInput, int inGroupIdContext)
{
    AutoCrit aCrit(this->GetDataLock());
    EeWonSmartAction a;
    bool ok = ee_won_smart_input(this->mModel, inInput.data(), inInput.length(), inGroupIdContext, &a);
    std::wstring aText = SmartText(a.text, a.text_len);
    switch (a.kind) {
    case EE_WON_SMART_ACT_CHAT: this->SendChat(aText, a.flags, a.group, NULL); break;
    case EE_WON_SMART_ACT_CLIENT_FLAGS: this->SetClientFlags(a.flags, a.value, 0, 0, NULL); break;
    case EE_WON_SMART_ACT_INVITE: this->InviteClient(a.group, a.client, a.on, aText, NULL); break;
    case EE_WON_SMART_ACT_MODERATOR: this->BecomeModerator(a.on, NULL); break;
    case EE_WON_SMART_ACT_MUTE: this->MuteClient(a.group, a.on, a.seconds, aText, a.client, NULL); break;
    case EE_WON_SMART_ACT_BAN: this->BanClient(a.group, a.on, a.seconds, aText, a.client, NULL); break;
    case EE_WON_SMART_ACT_BAN_CDKEY: this->BanClientByCDKeyId(a.group, a.on, a.seconds, aText, a.value, NULL); break;
    }
    return ok;
}

// FUNCTION: WON 0x006c4120
// Forgets this client's id and record, then closes the routing connection
void SmartRoutingConnection::CloseHook()
{
    {
        AutoCrit aCrit(this->GetDataLock());
        ee_won_smart_close(this->mModel);
    }
    RoutingConnection::CloseHook();
}
}
