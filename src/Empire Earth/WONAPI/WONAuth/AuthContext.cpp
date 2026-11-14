// AuthContext: a RefCount shim over ee_won's auth context (rust/crates/ee_won/src/authobj.rs: the lock, the peer
// sessions and certificate flow, the peer data, the login data with its secrets, CD keys and hashes, the server list
// and the verifier key); every method forwards
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <string>
#include <list>
#include <set>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONAuthPeerData.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONBuffer.h"
#include "platform/PlatPath.h"
namespace WONAPI {
// The hooks of the sessions and certificate flow (ee_won authflow.rs): the objects it holds are RefCounts
struct AuthContextFlow {
    static RefCount* Obj(void* p) { return static_cast<RefCount*>(p); }
    static AsyncOp* Op(void* p) { return static_cast<AsyncOp*>(Obj(p)); }
    static void AddRef(void* p) { Obj(p)->AddRef(); }
    static void Release(void* p) { Obj(p)->Release(); }
    static int64_t LastUse(void* p) { return (int64_t)static_cast<AuthSession*>(Obj(p))->GetLastUseTime(); }
    static void Kill(void* p) { Op(p)->Kill(true); }
    static bool Killed(void* p) { return Op(p)->IsKilled(); }
    static long Status(void* p) { return Op(p)->GetStatus(); }
    static void RunAsync(void* p, long t) { Op(p)->RunAsync(t); }
    static long Run(void* p, long m, unsigned long t) { return Op(p)->Run(m, t); }
    static void Finish(void* p, long s) { Op(p)->Finish(s); }
    static void* NewTimer(void* c)
    {
        AsyncOp* op = new AsyncOp;
        op->AddRef();
        op->SetCompletion(new ParamCompletion<SmartPtr<AsyncOp>,SmartPtr<RefCount> >(&AuthContext::StaticAutoRefreshCallback,static_cast<AuthContext*>(c)));
        return static_cast<RefCount*>(op);
    }
    static void* NewCertOp(void* c, bool completion)
    {
        GetCertOp* op = new GetCertOp(static_cast<AuthContext*>(c));
        op->AddRef();
        if(completion)
            op->SetCompletion(new ParamCompletion<SmartPtr<AsyncOp>,SmartPtr<RefCount> >(AuthContext::StaticGetCertCallback,static_cast<AuthContext*>(c)));
        return static_cast<RefCount*>(static_cast<AsyncOp*>(op));
    }
    // GetCertOp::GetPeerData: a new reference to its Rust peer data, or NULL
    static EeWonAuthObj* CertPeerData(void* p)
    {
        return reinterpret_cast<EeWonAuthObj*>((intptr_t)ee_won_hostop_num(static_cast<GetCertOp*>(Op(p))->GetRustOp(),
                                                                           EE_WON_GETCERT_PEER_DATA, 0));
    }
    static const EeWonAuthCtx* New(AuthContext* c, const std::string& inSecretFile)
    {
        EeWonAuthFlowHooks h;
        h.context = c;
        h.add_ref = AddRef;
        h.release = Release;
        h.session_last_use = LastUse;
        h.op_kill = Kill;
        h.op_killed = Killed;
        h.op_status = Status;
        h.op_run_async = RunAsync;
        h.op_run = Run;
        h.op_finish = Finish;
        h.new_timer = NewTimer;
        h.new_cert_op = NewCertOp;
        h.cert_peer_data = CertPeerData;
        return ee_won_authctx_new(&h, reinterpret_cast<const uint8_t*>(inSecretFile.data()), inSecretFile.size());
    }
};
static const uint8_t* U8(const std::string& s) { return reinterpret_cast<const uint8_t*>(s.data()); }

// FUNCTION: WON 0x006ddb70
// Sets up the peer data, login-secret file path and server context, and loads the key file once per process
void AuthContext::Init()
{
    this->mCore=AuthContextFlow::New(this,WONAPICore::GetDefaultFileDirectory()+"_wonlogin.ks");
    ee_won_authctx_set_server_context(this->mCore,static_cast<RefCount*>(new ServerContext));
    const std::string& aDir=WONAPICore::GetDefaultFileDirectory();
    ee_won_auth_verifier_init(U8(aDir),aDir.size());
}
// FUNCTION: WON 0x006dddf0
// Creates an empty auth context
AuthContext::AuthContext()
{
    this->Init();
}
// FUNCTION: WON 0x006de370
// Destroys the auth context (its reference to the Rust object)
AuthContext::~AuthContext()
{
    ee_won_authctx_free(this->mCore);
}

// FUNCTION: WON 0x006deeb0
// Caches a peer session for an address and drops up to five map entries unused for over 300 seconds
void AuthContext::PutSession(const IPAddr& address,AuthSession* session)
{
    ee_won_authctx_put_session(this->mCore,address.GetValue(),session!=NULL?static_cast<RefCount*>(session):NULL);
}
// FUNCTION: WON 0x006df080
// Takes a session used within 300 seconds for the address out of the cache; NULL when none
SmartPtr<AuthSession> AuthContext::GetSession(const IPAddr& address)
{
    void* aSession=ee_won_authctx_get_session(this->mCore,address.GetValue());
    if(aSession==NULL) return NULL;
    SmartPtr<AuthSession> session(static_cast<AuthSession*>(static_cast<RefCount*>(aSession)));
    static_cast<RefCount*>(aSession)->Release();
    return session;
}
// FUNCTION: WON 0x006de7f0
// Starts a certificate refresh when the current auto-refresh timer fires (not when killed)
void AuthContext::AutoRefreshCallback(AsyncOp* op)
{
    ee_won_authctx_auto_refresh_callback(this->mCore,static_cast<RefCount*>(op));
}
// FUNCTION: WON 0x006de8e0
// Kills and drops the pending auto-refresh timer
void AuthContext::StopAutoRefresh() { ee_won_authctx_stop_auto_refresh(this->mCore); }
// FUNCTION: WON 0x006deb30
// Turns certificate auto-refresh on or off and restarts the timer
void AuthContext::SetDoAutoRefresh(bool enabled) { ee_won_authctx_set_do_auto_refresh(this->mCore,enabled); }
// FUNCTION: WON 0x006df260
// Kills the running certificate request and the auto-refresh timer
void AuthContext::Kill() { ee_won_authctx_kill(this->mCore); }
// FUNCTION: WON 0x006de760
// Forwards the auto-refresh timer completion to its context
void AuthContext::StaticAutoRefreshCallback(SmartPtr<AsyncOp> op,SmartPtr<RefCount> parameter)
{
    static_cast<AuthContext*>(parameter.get())->AutoRefreshCallback(op.get());
}
// FUNCTION: WON 0x006de970
// Restarts the auto-refresh timer: 2 minutes before the certificate expires (at least 2), or 5 when forced
void AuthContext::StartAutoRefresh(bool force) { ee_won_authctx_start_auto_refresh(this->mCore,force); }

// The login data (ee_won authlogin.rs, its own lock): the user name and password, the secrets and their file
// (_wonlogin.ks), the communities, their CD keys and file hashes, the joins, the user data and the nicknames, and the
// login request's blocks.
static void AuthAppendBytes(void* inContext, const uint8_t* inBytes, size_t inLength)
{
    static_cast<std::string*>(inContext)->append(reinterpret_cast<const char*>(inBytes), inLength);
}
static std::string AuthUtf16(const std::wstring& inText)
{
    std::string aBytes;
    WideAppendUtf16LE(aBytes,inText);
    return aBytes;
}
std::wstring AuthContext::LoginText(int inWhich) const
{
    std::string aBytes;
    ee_won_auth_login_text(ee_won_authctx_login(this->mCore),inWhich,AuthAppendBytes,&aBytes);
    std::wstring aText;
    WideAssignUtf16LE(aText,aBytes.data(),(unsigned long)(aBytes.size()/2));
    return aText;
}
void AuthContext::LoginSet(int inWhich, const std::wstring& inKey, const std::string& inValue, long long inNum)
{
    std::string aKey=AuthUtf16(inKey);
    ee_won_auth_login_set(ee_won_authctx_login(this->mCore),inWhich,U8(aKey),aKey.size(),U8(inValue),inValue.size(),inNum);
}
static std::string AuthKeyBytes(const CDKey& inKey)
{
    return inKey.mKey.get()!=NULL?std::string(inKey.mKey->data(),inKey.mKey->length()):std::string();
}
static void AuthAppendBlock(const EeWonAuthCtx* inCore, int inWhich, WriteBuffer& output, const RawBuffer* inChallenge)
{
    std::string aBytes;
    ee_won_auth_login_block(ee_won_authctx_login(inCore),inWhich,inChallenge!=NULL?inChallenge->data():NULL,
                            inChallenge!=NULL?inChallenge->size():0,AuthAppendBytes,&aBytes);
    output.AppendBytes(aBytes.data(),aBytes.length());
}
static bool AuthSecrets(const EeWonAuthCtx* inCore, int inWhich, const void* inKey = NULL, time_t t0 = 0, time_t t1 = 0)
{
    return ee_won_auth_login_secrets(ee_won_authctx_login(inCore),inWhich,static_cast<const uint8_t*>(inKey),(int64_t)t0,(int64_t)t1);
}

// FUNCTION: WON 0x006e1240
// Reads the saved login secrets (expiry + 8 bytes each) from the secret file; false when it cannot open
bool AuthContext::LoadSecretList() { return AuthSecrets(this->mCore,0); }
// FUNCTION: WON 0x006e1460
// Writes the login secrets to the secret file; false when it cannot be created
bool AuthContext::SaveSecretList() { return AuthSecrets(this->mCore,1); }
// FUNCTION: WON 0x006e1560
// Appends a count byte and every login secret to the login request
void AuthContext::AppendLoginSecrets(WriteBuffer& output) { AuthAppendBlock(this->mCore,2,output,NULL); }
// FUNCTION: WON 0x006e1650
// Adds a new uncommitted 8-byte login secret at the front and saves the list
void AuthContext::AddSecret(const void* secret) { AuthSecrets(this->mCore,2,secret); }
// FUNCTION: WON 0x006e1790
// Removes the matching uncommitted login secret and saves the list
void AuthContext::RemoveSecret(const void* secret) { AuthSecrets(this->mCore,3,secret); }
// FUNCTION: WON 0x006e1890
// Gives uncommitted secrets an expiry, drops expired ones, caps the list below 256 entries and saves it
void AuthContext::CommitSecrets(time_t authTime,time_t duration) { AuthSecrets(this->mCore,4,NULL,authTime,duration); }
// FUNCTION: WON 0x006e19e0
// Returns the current peer data (certificate and keys) under the lock: a shim over the Rust object
ConstSmartPtr<AuthPeerData> AuthContext::GetPeerData() { return AuthPeerData::Adopt(ee_won_authctx_peer_data(this->mCore)); }
// FUNCTION: WON 0x006e1a30
// Returns whether the auth2 certificate expires within the given number of seconds
bool AuthContext::IsExpired(time_t extra) { return ee_won_authctx_is_expired(this->mCore,(int64_t)extra); }
// FUNCTION: WON 0x006e1aa0
// Replaces the peer data and clears the peer-session cache
void AuthContext::SetPeerData(const AuthPeerData* data)
{
    ee_won_authctx_set_peer_data(this->mCore,data!=NULL?data->Impl():NULL);
}

// FUNCTION: WON 0x006e0c50
// Appends each hashed community's file hash and its hash keyed with the server challenge
void AuthContext::AppendHashes(WriteBuffer& output,const RawBuffer& challenge) { AuthAppendBlock(this->mCore,1,output,&challenge); }
// FUNCTION: WON 0x006e0e50
// Appends the login's community names and typed records (CD keys, joins, user data, secrets, nicknames)
void AuthContext::AppendCommunityData(WriteBuffer& output) { AuthAppendBlock(this->mCore,0,output,NULL); }

// FUNCTION: WON 0x006de560
// Forwards the certificate request completion to its context
void AuthContext::StaticGetCertCallback(SmartPtr<AsyncOp> op,SmartPtr<RefCount> param)
{
    static_cast<AuthContext*>(param.get())->GetCertCallback(static_cast<GetCertOp*>(op.get()));
}
// FUNCTION: WON 0x006de5f0
// Hands the new peer data and status to every waiting refresh op, then restarts auto-refresh
void AuthContext::GetCertCallback(GetCertOp* op)
{
    ee_won_authctx_get_cert_callback(this->mCore,static_cast<RefCount*>(static_cast<AsyncOp*>(op)));
}
// FUNCTION: WON 0x006deba0
// Queues a refresh op (its run and its member) and starts a certificate request when none is running
void AuthContext::RefreshAsync(AsyncOp* op,EeWonLightCert* cert,DWORD timeout)
{
    ee_won_authctx_refresh_async(this->mCore,op!=NULL?static_cast<RefCount*>(op):NULL,cert,timeout);
}
// FUNCTION: WON 0x006dedf0
// Fetches a new certificate synchronously; restarts auto-refresh on success and returns the status (no lock, as
// the original)
WONStatus AuthContext::RefreshBlock(DWORD timeout) { return ee_won_authctx_refresh_block(this->mCore,timeout); }

// FUNCTION: WON 0x006e0380
// Sets the CD key sent for a community
void AuthContext::SetCDKey(const std::wstring& community,const CDKey& key)
{
    this->LoginSet(EE_WON_AUTH_LOGIN_CDKEY,community,AuthKeyBytes(key),(key.mKey.get()!=NULL?1:0)|(key.mValid?2:0));
}
// FUNCTION: WON 0x006e0510
// Clears the CD-key community joins
void AuthContext::ClearCDKeyCommunityJoins() { this->LoginSet(EE_WON_AUTH_LOGIN_CLEAR_JOINS,std::wstring()); }
// FUNCTION: WON 0x006e05e0
// Clears the per-community user data to set
void AuthContext::ClearUserData() { this->LoginSet(EE_WON_AUTH_LOGIN_CLEAR_USER_DATA,std::wstring()); }
// FUNCTION: WON 0x006e06b0
// Clears the one-time user data and CD-key joins
void AuthContext::ClearOneTimeData() { this->LoginSet(EE_WON_AUTH_LOGIN_CLEAR_ONE_TIME,std::wstring()); }
// FUNCTION: WON 0x006e06d0
// Hashes the running executable for the community; false when its path is unavailable
// (GetModuleFileNameA: the data root is the executable's directory, unless EE_DATA_ROOT moves it on POSIX)
bool AuthContext::SetHashFile(const std::wstring& community)
{
    char directory[PLAT_MAX_PATH];
    char name[PLAT_MAX_PATH];
    if(PlatPathGetDataRoot(directory,PLAT_MAX_PATH)>0 && PlatPathGetExecutableName(name,PLAT_MAX_PATH)>0)
        return this->SetHashFile(community,std::string(directory)+name);
    return false;
}
// FUNCTION: WON 0x006e07f0
// Hashes one file for the community
bool AuthContext::SetHashFile(const std::wstring& community,const std::string& file)
{
    return this->SetHashFileList(community,std::list<std::string>(1,file));
}
// FUNCTION: WON 0x006e08e0
// Computes the community's MD5 of the files plus one MD5 per 16 KB section; false when a file cannot open
bool AuthContext::SetHashFileList(const std::wstring& community,const std::list<std::string>& files)
{
    std::string aCommunity=AuthUtf16(community);
    std::string aFiles;
    for(std::list<std::string>::const_iterator next=files.begin();next!=files.end();++next)
        aFiles.append(next->c_str(),next->size()+1);
    return ee_won_auth_login_hash_files(ee_won_authctx_login(this->mCore),U8(aCommunity),aCommunity.size(),U8(aFiles),aFiles.size());
}
// FUNCTION: WON 0x006df620
// Sets the login user name
void AuthContext::SetUserName(const std::wstring& name) { this->LoginSet(EE_WON_AUTH_LOGIN_NAME,std::wstring(),AuthUtf16(name)); }
// FUNCTION: WON 0x006df760
// Sets the login password
void AuthContext::SetPassword(const std::wstring& password)
{
    this->LoginSet(EE_WON_AUTH_LOGIN_PASSWORD,std::wstring(),AuthUtf16(password));
}
// FUNCTION: WON 0x006df8a0
// Adds a community to log in to (no change when already present)
void AuthContext::AddCommunity(const std::wstring& community) { this->LoginSet(EE_WON_AUTH_LOGIN_ADD_COMMUNITY,community); }
// FUNCTION: WON 0x006dfcc0
// Makes the given community the only one, keeping its data when it was already present
void AuthContext::SetCommunity(const std::wstring& community) { this->LoginSet(EE_WON_AUTH_LOGIN_SET_COMMUNITY,community); }
// FUNCTION: WON 0x006e02d0
// Removes a community from the login
void AuthContext::RemoveCommunity(const std::wstring& community)
{
    this->LoginSet(EE_WON_AUTH_LOGIN_REMOVE_COMMUNITY,community);
}

// FUNCTION: WON 0x006df440
// Loads the signing public key from a file; false when the file is missing or the key invalid
bool AuthContext::LoadVerifierKey(const std::string& filename)
{
    return ee_won_auth_verifier_load(U8(filename),filename.size());
}

// FUNCTION: WON 0x006df330
// Sets the auth server list
void AuthContext::SetServerContext(ServerContext* context)
{
    ee_won_authctx_set_server_context(this->mCore,context!=NULL?static_cast<RefCount*>(context):NULL);
}
// FUNCTION: WON 0x006df3e0
// Returns the auth server list
SmartPtr<ServerContext> AuthContext::GetServerContext()
{
    void* aServers=ee_won_authctx_server_context(this->mCore);
    if(aServers==NULL) return NULL;
    SmartPtr<ServerContext> servers(static_cast<ServerContext*>(static_cast<RefCount*>(aServers)));
    static_cast<RefCount*>(aServers)->Release();
    return servers;
}

}

namespace WONAPI {

// No original address identified: WON SDK support body
AuthContext::AuthContext(const std::wstring &theUserName, const std::wstring& theCommunity,
                         const std::wstring &thePassword)
{
    this->Init();
    this->SetUserName(theUserName);
    this->SetPassword(thePassword);
    this->AddCommunity(theCommunity);
}

// No original address identified: WON SDK support bodies. The server list's own lock serializes its calls; the
// context's lock is taken to read the member (the original held it around the call).
bool AuthContext::AddAddress(const IPAddr &theAddr) { return this->GetServerContext()->AddAddress(theAddr); }
bool AuthContext::AddAddressesFromDir(const DirEntityList &theDir, const wchar_t *theNameFilter)
{
	return this->GetServerContext()->AddAddressesFromDir(theDir, theNameFilter);
}
void AuthContext::RemoveAllCommunity() { this->LoginSet(EE_WON_AUTH_LOGIN_REMOVE_ALL_COMMUNITY,std::wstring()); }
void AuthContext::AddCDKeyCommunityJoin(const std::wstring &theCommunity, const CDKey &theKey)
{
	this->LoginSet(EE_WON_AUTH_LOGIN_ADD_JOIN,theCommunity,AuthKeyBytes(theKey),theKey.mKey.get()!=NULL?1:0);
}
void AuthContext::AddUserData(const std::wstring &theCommunity, const ByteBuffer *theData)
{
	this->LoginSet(EE_WON_AUTH_LOGIN_ADD_USER_DATA,theCommunity,
	               theData!=NULL?std::string(theData->data(),theData->length()):std::string(),theData!=NULL?1:0);
}
void AuthContext::SetLoginSecretFile(const std::string &theFile)
{
	ee_won_auth_login_set(ee_won_authctx_login(this->mCore),EE_WON_AUTH_LOGIN_SECRET_FILE,NULL,0,U8(theFile),theFile.size(),0);
}
void AuthContext::AddNickname(const std::wstring &theKey) { this->LoginSet(EE_WON_AUTH_LOGIN_ADD_NICKNAME,theKey); }
void AuthContext::SetNickname(const std::wstring &theKey, const std::wstring &theName)
{
	this->LoginSet(EE_WON_AUTH_LOGIN_SET_NICKNAME,theKey,AuthUtf16(theName));
}
void AuthContext::ClearNicknames() { this->LoginSet(EE_WON_AUTH_LOGIN_CLEAR_NICKNAMES,std::wstring()); }
bool AuthContext::RemoveAddress(const IPAddr &theAddr) { return this->GetServerContext()->RemoveAddress(theAddr); }
void AuthContext::RemoveAllAddresses() { this->GetServerContext()->RemoveAllAddresses(); }
bool AuthContext::AddAddressesFromDir(const DirEntityMap &theDir, const std::wstring &thePath, const wchar_t *theNameFilter)
{
	return this->GetServerContext()->AddAddressesFromDir(theDir, thePath, theNameFilter);
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006ddf90
// (protected: virtual void * __thiscall WONAPI::AuthContext::`scalar deleting destructor'(unsigned int)    [??_GAuthContext@WONAPI@@MAEPAXI@Z])
// TEMPLATE: WON 0x006dfae0
// (public: __thiscall WONAPI::AuthLoginCommunityData::~AuthLoginCommunityData(void)    [??1AuthLoginCommunityData@WONAPI@@QAE@XZ])
// TEMPLATE: WON 0x006dfbd0
// (public: __thiscall std::pair<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short> > con)
// TEMPLATE: WON 0x006dff00
// (public: __thiscall WONAPI::AuthLoginCommunityData::AuthLoginCommunityData(void)    [??0AuthLoginCommunityData@WONAPI@@QAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006ddfb0
// (public: __thiscall std::set<class WONAPI::SmartPtr<class WONAPI::LightGetCertOp>,struct WONAPI::SmartPtr<class WONAPI::LightGetCertOp>::Comp,class std)
// TEMPLATE: WON 0x006de070
// (public: __thiscall std::map<class WONAPI::IPAddr,struct WONAPI::AuthContext::SessionStruct,struct std::less<class WONAPI::IPAddr>,class std::allocator)
// TEMPLATE: WON 0x006de130
// (public: __thiscall std::map<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short> >,clas)
// TEMPLATE: WON 0x006de1f0
// (public: __thiscall std::map<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short> >,clas)
// TEMPLATE: WON 0x006de2b0
// (public: __thiscall std::map<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short> >,clas)
// TEMPLATE: WON 0x006dff60
// (public: __thiscall WONAPI::AuthLoginCommunityData::AuthLoginCommunityData(class WONAPI::AuthLoginCommunityData const &)    [??0AuthLoginCommunityData@)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006ddda0
// (class WONAPI::CriticalSection & __cdecl GetVerifierCrit(void)    [?GetVerifierCrit@@YAAAVCriticalSection@WONAPI@@XZ])
// TEMPLATE: WON 0x006e0140
// (public: class WONAPI::CDKey & __thiscall WONAPI::CDKey::operator=(class WONAPI::CDKey const &)    [??4CDKey@WONAPI@@QAEAAV01@ABV01@@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006ddde0
// (_$E72    [_$E72])
// TEMPLATE: WON 0x006df2d0
// (_$E77    [_$E77])
// TEMPLATE: WON 0x006df2e0
// (_$E73    [_$E73])
// TEMPLATE: WON 0x006df300
// (_$E76    [_$E76])
// TEMPLATE: WON 0x006df310
// (_$E74    [_$E74])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006cac70
// (protected: void __thiscall std::_Tree<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned sho)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006e9530
// (public: class std::basic_string<unsigned char,struct std::char_traits<unsigned char>,class std::allocator<unsigned char> > & __thiscall std::basic_str)
// TEMPLATE: WON 0x006e9630
// (public: class std::basic_string<unsigned char,struct std::char_traits<unsigned char>,class std::allocator<unsigned char> > & __thiscall std::basic_str)
// TEMPLATE: WON 0x006ea2e0
// (public: __thiscall std::pair<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short> > con)
// TEMPLATE: WON 0x006ea3b0
// (public: __thiscall std::pair<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short> > con)
// TEMPLATE: WON 0x006ea440
// (public: class std::basic_string<unsigned char,struct std::char_traits<unsigned char>,class std::allocator<unsigned char> > & __thiscall std::basic_str)
// TEMPLATE: WON 0x006ea550
// (public: class std::basic_string<unsigned char,struct std::char_traits<unsigned char>,class std::allocator<unsigned char> > & __thiscall std::basic_str)
// TEMPLATE: WON 0x006ea760
// (private: void __thiscall std::basic_string<unsigned char,struct std::char_traits<unsigned char>,class std::allocator<unsigned char> >::_Eos(unsigned i)
// TEMPLATE: WON 0x006ea780
// (private: void __thiscall std::basic_string<unsigned char,struct std::char_traits<unsigned char>,class std::allocator<unsigned char> >::_Freeze(void)  )
// TEMPLATE: WON 0x006ea820
// (private: bool __thiscall std::basic_string<unsigned char,struct std::char_traits<unsigned char>,class std::allocator<unsigned char> >::_Grow(unsigned )
// TEMPLATE: WON 0x006eaa00
// (private: bool __thiscall std::basic_string<unsigned char,struct std::char_traits<unsigned char>,class std::allocator<unsigned char> >::_Inside(unsigne)
// TEMPLATE: WON 0x006eb590
// (public: bool __thiscall std::list<class WONAPI::SmartPtr<class WONAPI::AuthSession>,class std::allocator<class WONAPI::SmartPtr<class WONAPI::AuthSess)
// TEMPLATE: WON 0x006eb750
// (public: class std::_Tree<class WONAPI::IPAddr,struct std::pair<class WONAPI::IPAddr const ,struct WONAPI::AuthContext::SessionStruct>,struct std::_Map)
// TEMPLATE: WON 0x006eb890
// (public: void __thiscall std::_Tree<class WONAPI::IPAddr,struct std::pair<class WONAPI::IPAddr const ,struct WONAPI::AuthContext::SessionStruct>,struct)
// TEMPLATE: WON 0x006eb8e0
// (public: class std::_Tree<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short> >,struct )
// TEMPLATE: WON 0x006ebc90
// (public: void __thiscall std::_Tree<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short>)
// TEMPLATE: WON 0x006ebd40
// (protected: struct std::list<struct WONAPI::AuthContext::SecretData,class std::allocator<struct WONAPI::AuthContext::SecretData> >::_Node * __thiscall )
// TEMPLATE: WON 0x006ebe30
// (public: static unsigned char * __cdecl std::char_traits<unsigned char>::copy(unsigned char *,unsigned char const *,unsigned int)    [?copy@?$char_trai)
// TEMPLATE: WON 0x006ebe60
// (public: static unsigned char * __cdecl std::char_traits<unsigned char>::move(unsigned char *,unsigned char const *,unsigned int)    [?move@?$char_trai)
// TEMPLATE: WON 0x006ebeb0
// (public: unsigned int __thiscall std::basic_string<unsigned char,struct std::char_traits<unsigned char>,class std::allocator<unsigned char> >::max_size)
// TEMPLATE: WON 0x006ebec0
// (private: void __thiscall std::basic_string<unsigned char,struct std::char_traits<unsigned char>,class std::allocator<unsigned char> >::_Copy(unsigned )
// TEMPLATE: WON 0x006ed960
// (public: class std::_Tree<class WONAPI::SmartPtr<class WONAPI::LightGetCertOp>,class WONAPI::SmartPtr<class WONAPI::LightGetCertOp>,struct std::_Setk<c)
// TEMPLATE: WON 0x006ee020
// (protected: void __thiscall std::_Tree<class WONAPI::IPAddr,struct std::pair<class WONAPI::IPAddr const ,struct WONAPI::AuthContext::SessionStruct>,str)
// TEMPLATE: WON 0x006ee8d0
// (protected: void __thiscall std::_Tree<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned sho)
// TEMPLATE: WON 0x006ee910
// (protected: void __thiscall std::_Tree<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned sho)
// TEMPLATE: WON 0x006ee950
// (protected: struct std::_Tree<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short> >,str)
// TEMPLATE: WON 0x006ee9a0
// (public: void __thiscall std::_Tree<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short>)
// TEMPLATE: WON 0x006ef0a0
// (protected: void __thiscall std::_Tree<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned sho)
// TEMPLATE: WON 0x006ef0e0
// (protected: void __thiscall std::_Tree<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned sho)
// TEMPLATE: WON 0x006ef120
// (public: class std::_Tree<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short> >,struct )
// TEMPLATE: WON 0x006ef650
// (protected: void __thiscall std::_Tree<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned sho)
// TEMPLATE: WON 0x006ef890
// (public: void __thiscall std::_Tree<class WONAPI::IPAddr,struct std::pair<class WONAPI::IPAddr const ,struct WONAPI::AuthContext::SessionStruct>,struct)
// TEMPLATE: WON 0x006ef8f0
// (protected: struct std::_Tree<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short> >,str)
// TEMPLATE: WON 0x006f0090
// (public: void * __thiscall std::pair<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006e9400
// (public: __thiscall std::basic_string<unsigned char,struct std::char_traits<unsigned char>,class std::allocator<unsigned char> >::basic_string<unsigned)
// TEMPLATE: WON 0x006e98b0
// (public: class std::_Tree<class WONAPI::IPAddr,struct std::pair<class WONAPI::IPAddr const ,struct WONAPI::AuthContext::SessionStruct>,struct std::_Map)
// TEMPLATE: WON 0x006e9e20
// (public: struct std::pair<class std::_Tree<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned)
// TEMPLATE: WON 0x006ea1d0
// (public: __thiscall WONAPI::AuthContext::SessionStruct::~SessionStruct(void))
// TEMPLATE: WON 0x006ea230
// (public: __thiscall std::pair<class WONAPI::IPAddr const ,struct WONAPI::AuthContext::SessionStruct>::~pair<class WONAPI::IPAddr const ,struct WONAPI::)
// TEMPLATE: WON 0x006eb180
// (public: class std::_Tree<class WONAPI::SmartPtr<class WONAPI::LightGetCertOp>,class WONAPI::SmartPtr<class WONAPI::LightGetCertOp>,struct std::_Setk<c)
// TEMPLATE: WON 0x006eb230
// (protected: class std::_Tree<class WONAPI::SmartPtr<class WONAPI::LightGetCertOp>,class WONAPI::SmartPtr<class WONAPI::LightGetCertOp>,struct std::_Set)
// TEMPLATE: WON 0x006eb530
// (public: void __thiscall std::list<class WONAPI::SmartPtr<class WONAPI::AuthSession>,class std::allocator<class WONAPI::SmartPtr<class WONAPI::AuthSess)
// TEMPLATE: WON 0x006eb5b0
// (public: struct std::pair<class std::_Tree<class WONAPI::IPAddr,struct std::pair<class WONAPI::IPAddr const ,struct WONAPI::AuthContext::SessionStruct>)
// TEMPLATE: WON 0x006eb6a0
// (public: class std::_Tree<class WONAPI::IPAddr,struct std::pair<class WONAPI::IPAddr const ,struct WONAPI::AuthContext::SessionStruct>,struct std::_Map)
// TEMPLATE: WON 0x006eb7a0
// (public: __thiscall std::pair<class WONAPI::IPAddr const ,struct WONAPI::AuthContext::SessionStruct>::pair<class WONAPI::IPAddr const ,struct WONAPI::A)
// TEMPLATE: WON 0x006eb990
// (protected: class std::_Tree<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short> >,stru)
// TEMPLATE: WON 0x006ede50
// (protected: void __thiscall std::_Tree<class WONAPI::SmartPtr<class WONAPI::LightGetCertOp>,class WONAPI::SmartPtr<class WONAPI::LightGetCertOp>,struct)
// TEMPLATE: WON 0x006ede90
// (protected: void __thiscall std::list<class WONAPI::SmartPtr<class WONAPI::AuthSession>,class std::allocator<class WONAPI::SmartPtr<class WONAPI::AuthS)
// TEMPLATE: WON 0x006edfe0
// (protected: void __thiscall std::_Tree<class WONAPI::IPAddr,struct std::pair<class WONAPI::IPAddr const ,struct WONAPI::AuthContext::SessionStruct>,str)
// TEMPLATE: WON 0x006ee060
// (protected: class std::_Tree<class WONAPI::IPAddr,struct std::pair<class WONAPI::IPAddr const ,struct WONAPI::AuthContext::SessionStruct>,struct std::_)
// TEMPLATE: WON 0x006ee360
// (public: class std::_Tree<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short> >,struct )
// TEMPLATE: WON 0x006eea00
// (protected: void __thiscall std::list<struct WONAPI::AuthContext::SecretData,class std::allocator<struct WONAPI::AuthContext::SecretData> >::_Incsize(u)
// TEMPLATE: WON 0x006eeb50
// (public: class std::_Tree<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short> >,struct )
// TEMPLATE: WON 0x006eff70
// (void __cdecl std::_Construct(struct std::pair<class WONAPI::IPAddr const ,struct WONAPI::AuthContext::SessionStruct> *,struct std::pair<class WONAPI::)
// TEMPLATE: WON 0x006f0050
// (public: void * __thiscall std::pair<class WONAPI::IPAddr const ,struct WONAPI::AuthContext::SessionStruct>::`scalar deleting destructor'(unsigned int))
// TEMPLATE: WON 0x006f00b0
// (public: void * __thiscall std::pair<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short)
// TEMPLATE: WON 0x006f00d0
// (public: void * __thiscall std::pair<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006aec20
// (public: __thiscall std::_Tree<class WONAPI::IPAddr,struct std::pair<class WONAPI::IPAddr const ,class std::list<class WONAPI::IPAddr,class std::alloca)
// TEMPLATE: WON 0x006e00d0
// (public: __thiscall WONAPI::CDKey::CDKey(class WONAPI::CDKey const &))
// TEMPLATE: WON 0x006e9f20
// (public: __thiscall std::pair<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short> > con)
// TEMPLATE: WON 0x006ea030
// (public: __thiscall std::list<struct WONAPI::AuthContext::SecretData,class std::allocator<struct WONAPI::AuthContext::SecretData> >::~list<struct WONAP)
// TEMPLATE: WON 0x006ea090
// (public: class std::list<struct WONAPI::AuthContext::SecretData,class std::allocator<struct WONAPI::AuthContext::SecretData> >::iterator __thiscall std)
// TEMPLATE: WON 0x006eabd0
// (public: __thiscall std::_Tree<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short> >,st)
// TEMPLATE: WON 0x006ebce0
// (public: class std::list<struct WONAPI::AuthContext::SecretData,class std::allocator<struct WONAPI::AuthContext::SecretData> >::iterator __thiscall std)
// TEMPLATE: WON 0x006efb80
// (void __cdecl std::_Construct(struct std::pair<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsi)
// TEMPLATE: WON 0x006f0110
// (public: void __thiscall std::list<class WONAPI::SmartPtr<class WONAPI::AuthSession>,class std::allocator<class WONAPI::SmartPtr<class WONAPI::AuthSess)
// TEMPLATE: WON 0x006aaa50
// (public: __thiscall std::map<class WONAPI::IPAddr,class std::list<class WONAPI::IPAddr,class std::allocator<class WONAPI::IPAddr> >::iterator,struct st)
