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

// Sets up the peer data, login-secret file path and server context, and loads the key file once per process
void AuthContext::Init()
{
    this->mCore=AuthContextFlow::New(this,WONAPICore::GetDefaultFileDirectory()+"_wonlogin.ks");
    ee_won_authctx_set_server_context(this->mCore,static_cast<RefCount*>(new ServerContext));
    const std::string& aDir=WONAPICore::GetDefaultFileDirectory();
    ee_won_auth_verifier_init(U8(aDir),aDir.size());
}
// Creates an empty auth context
AuthContext::AuthContext()
{
    this->Init();
}
// Destroys the auth context (its reference to the Rust object)
AuthContext::~AuthContext()
{
    ee_won_authctx_free(this->mCore);
}

// Starts a certificate refresh when the current auto-refresh timer fires (not when killed)
void AuthContext::AutoRefreshCallback(AsyncOp* op)
{
    ee_won_authctx_auto_refresh_callback(this->mCore,static_cast<RefCount*>(op));
}
// Kills and drops the pending auto-refresh timer
void AuthContext::StopAutoRefresh() { ee_won_authctx_stop_auto_refresh(this->mCore); }
// Turns certificate auto-refresh on or off and restarts the timer
void AuthContext::SetDoAutoRefresh(bool enabled) { ee_won_authctx_set_do_auto_refresh(this->mCore,enabled); }
// Kills the running certificate request and the auto-refresh timer
void AuthContext::Kill() { ee_won_authctx_kill(this->mCore); }
// Forwards the auto-refresh timer completion to its context
void AuthContext::StaticAutoRefreshCallback(SmartPtr<AsyncOp> op,SmartPtr<RefCount> parameter)
{
    static_cast<AuthContext*>(parameter.get())->AutoRefreshCallback(op.get());
}
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

// Reads the saved login secrets (expiry + 8 bytes each) from the secret file; false when it cannot open
bool AuthContext::LoadSecretList() { return AuthSecrets(this->mCore,0); }
// Writes the login secrets to the secret file; false when it cannot be created
bool AuthContext::SaveSecretList() { return AuthSecrets(this->mCore,1); }
// Appends a count byte and every login secret to the login request
void AuthContext::AppendLoginSecrets(WriteBuffer& output) { AuthAppendBlock(this->mCore,2,output,NULL); }
// Adds a new uncommitted 8-byte login secret at the front and saves the list
void AuthContext::AddSecret(const void* secret) { AuthSecrets(this->mCore,2,secret); }
// Removes the matching uncommitted login secret and saves the list
void AuthContext::RemoveSecret(const void* secret) { AuthSecrets(this->mCore,3,secret); }
// Gives uncommitted secrets an expiry, drops expired ones, caps the list below 256 entries and saves it
void AuthContext::CommitSecrets(time_t authTime,time_t duration) { AuthSecrets(this->mCore,4,NULL,authTime,duration); }
// Returns the current peer data (certificate and keys) under the lock: a shim over the Rust object
ConstSmartPtr<AuthPeerData> AuthContext::GetPeerData() { return AuthPeerData::Adopt(ee_won_authctx_peer_data(this->mCore)); }
// Returns whether the auth2 certificate expires within the given number of seconds
bool AuthContext::IsExpired(time_t extra) { return ee_won_authctx_is_expired(this->mCore,(int64_t)extra); }
// Replaces the peer data and clears the peer-session cache
void AuthContext::SetPeerData(const AuthPeerData* data)
{
    ee_won_authctx_set_peer_data(this->mCore,data!=NULL?data->Impl():NULL);
}

// Appends each hashed community's file hash and its hash keyed with the server challenge
void AuthContext::AppendHashes(WriteBuffer& output,const RawBuffer& challenge) { AuthAppendBlock(this->mCore,1,output,&challenge); }
// Appends the login's community names and typed records (CD keys, joins, user data, secrets, nicknames)
void AuthContext::AppendCommunityData(WriteBuffer& output) { AuthAppendBlock(this->mCore,0,output,NULL); }

// Forwards the certificate request completion to its context
void AuthContext::StaticGetCertCallback(SmartPtr<AsyncOp> op,SmartPtr<RefCount> param)
{
    static_cast<AuthContext*>(param.get())->GetCertCallback(static_cast<GetCertOp*>(op.get()));
}
// Hands the new peer data and status to every waiting refresh op, then restarts auto-refresh
void AuthContext::GetCertCallback(GetCertOp* op)
{
    ee_won_authctx_get_cert_callback(this->mCore,static_cast<RefCount*>(static_cast<AsyncOp*>(op)));
}
// Queues a refresh op (its run and its member) and starts a certificate request when none is running
void AuthContext::RefreshAsync(AsyncOp* op,EeWonLightCert* cert,DWORD timeout)
{
    ee_won_authctx_refresh_async(this->mCore,op!=NULL?static_cast<RefCount*>(op):NULL,cert,timeout);
}
// Fetches a new certificate synchronously; restarts auto-refresh on success and returns the status (no lock, as
// the original)
WONStatus AuthContext::RefreshBlock(DWORD timeout) { return ee_won_authctx_refresh_block(this->mCore,timeout); }

// Sets the CD key sent for a community
void AuthContext::SetCDKey(const std::wstring& community,const CDKey& key)
{
    this->LoginSet(EE_WON_AUTH_LOGIN_CDKEY,community,AuthKeyBytes(key),(key.mKey.get()!=NULL?1:0)|(key.mValid?2:0));
}
// Clears the CD-key community joins
void AuthContext::ClearCDKeyCommunityJoins() { this->LoginSet(EE_WON_AUTH_LOGIN_CLEAR_JOINS,std::wstring()); }
// Clears the per-community user data to set
void AuthContext::ClearUserData() { this->LoginSet(EE_WON_AUTH_LOGIN_CLEAR_USER_DATA,std::wstring()); }
// Clears the one-time user data and CD-key joins
void AuthContext::ClearOneTimeData() { this->LoginSet(EE_WON_AUTH_LOGIN_CLEAR_ONE_TIME,std::wstring()); }
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
// Hashes one file for the community
bool AuthContext::SetHashFile(const std::wstring& community,const std::string& file)
{
    return this->SetHashFileList(community,std::list<std::string>(1,file));
}
// Computes the community's MD5 of the files plus one MD5 per 16 KB section; false when a file cannot open
bool AuthContext::SetHashFileList(const std::wstring& community,const std::list<std::string>& files)
{
    std::string aCommunity=AuthUtf16(community);
    std::string aFiles;
    for(std::list<std::string>::const_iterator next=files.begin();next!=files.end();++next)
        aFiles.append(next->c_str(),next->size()+1);
    return ee_won_auth_login_hash_files(ee_won_authctx_login(this->mCore),U8(aCommunity),aCommunity.size(),U8(aFiles),aFiles.size());
}
// Sets the login user name
void AuthContext::SetUserName(const std::wstring& name) { this->LoginSet(EE_WON_AUTH_LOGIN_NAME,std::wstring(),AuthUtf16(name)); }
// Sets the login password
void AuthContext::SetPassword(const std::wstring& password)
{
    this->LoginSet(EE_WON_AUTH_LOGIN_PASSWORD,std::wstring(),AuthUtf16(password));
}
// Adds a community to log in to (no change when already present)
void AuthContext::AddCommunity(const std::wstring& community) { this->LoginSet(EE_WON_AUTH_LOGIN_ADD_COMMUNITY,community); }
// Makes the given community the only one, keeping its data when it was already present
void AuthContext::SetCommunity(const std::wstring& community) { this->LoginSet(EE_WON_AUTH_LOGIN_SET_COMMUNITY,community); }
// Removes a community from the login
void AuthContext::RemoveCommunity(const std::wstring& community)
{
    this->LoginSet(EE_WON_AUTH_LOGIN_REMOVE_COMMUNITY,community);
}

// Loads the signing public key from a file; false when the file is missing or the key invalid
bool AuthContext::LoadVerifierKey(const std::string& filename)
{
    return ee_won_auth_verifier_load(U8(filename),filename.size());
}

// Sets the auth server list
void AuthContext::SetServerContext(ServerContext* context)
{
    ee_won_authctx_set_server_context(this->mCore,context!=NULL?static_cast<RefCount*>(context):NULL);
}
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
