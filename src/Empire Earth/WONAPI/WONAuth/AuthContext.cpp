// AuthContext: login credentials, communities, CD keys, login secrets, peer-session cache and certificate refresh
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
#include "ee/wonapi/WONMD5Digest.h"
#include "ee/wonapi/WONMD5.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONBuffer.h"
#include "platform/PlatFile.h"
#include "platform/PlatPath.h"
// File-local lazy critical section: the original's getter is 0x006ddda0 (50 bytes), its atexit cleanup 0x2ddde0
static WONAPI::CriticalSection& GetVerifierCrit()
{
    static WONAPI::CriticalSection verifierCritical;
    return verifierCritical;
}


static char VerifierKeyBuf[] = 
	"\x30\x82\x03\x2c\x02\x82\x01\x01\x00\x86\x18\x0e\x3d\xd4\x9e\xfa\x42\xb9\x4c\xb3\x3e"
	"\xf7\x2b\x76\xce\xe6\xb0\x27\x8c\x64\x07\x93\x3b\x24\x64\x6c\x47\xb6\x9e\x51\x7b\xbf"
	"\xc9\xd5\xa1\xfb\xcf\x04\xfa\x7b\x8e\xc3\x6e\xd1\x88\xf6\x2e\xbd\x2b\x29\xda\xb5\x76"
	"\x66\x12\xb9\x7a\x64\x73\x19\x53\xa9\x78\xd8\x1e\xe5\x11\x0a\x23\x1c\x08\x21\x33\xad"
	"\x65\x76\x48\xff\xc5\x0b\xa2\xaa\x10\x22\xcb\x63\xbb\x68\xfc\xf0\x15\xf3\xe9\x90\x5f"
	"\x9d\x17\x8e\x77\x67\xec\xde\xdf\x8b\xcc\xbb\xa4\xd6\x06\xfa\x0e\xd8\xc0\x67\xd4\x36"
	"\xc9\x3a\xeb\x3f\xa4\x93\x57\xad\x62\x29\x24\x08\x67\x91\x6a\x0b\xd4\xdc\xad\xa4\x86"
	"\x85\x9f\xed\xf8\xb8\xda\xe1\xac\xf3\xf4\x44\xaf\x85\x8d\x21\x95\x6a\xf4\x6e\xac\x36"
	"\x83\x8f\x70\x0a\xc6\x17\xc1\x90\xc4\x7f\xbb\xe8\x18\xed\x4d\x0b\x8c\xdc\x76\x7f\xa9"
	"\xb2\x45\x0f\xc6\xa8\xca\x0a\x9f\xec\x9f\xe5\xe0\xbd\x71\x63\x06\xef\x32\xba\xe0\x40"
	"\x47\xf1\x91\xa8\xbf\x5e\xad\xb8\x2d\x28\x3a\x94\x63\xfa\x85\x1d\x24\xe3\xd2\x78\x7f"
	"\x0e\x85\xb1\xbb\x3f\x2d\x27\x16\x34\xcf\x94\x7d\x71\xdd\xf3\xec\x41\xc1\x27\xe5\x6f"
	"\xe6\x88\x78\xc8\x65\x15\x45\x3c\x0c\x5f\x30\x60\xc7\x02\x1d\x02\xe1\x4f\x25\x13\x97"
	"\xee\xdb\x46\x94\xb2\x54\xfc\x54\x26\x20\xa2\x6d\x57\x72\xdd\x47\xd2\xf0\x92\x00\x38"
	"\x51\xad\x02\x82\x01\x00\x72\x44\x9d\x49\x40\xff\x84\x75\xd8\x73\x57\x1f\xa7\xff\x52"
	"\x08\x82\x63\xfa\xd1\xa7\x30\xb8\x63\xc5\x10\x45\x8e\x12\xca\x9c\x36\x4f\xc8\x3e\xe8"
	"\x9a\x95\xfc\x48\xae\xcf\x80\xea\x75\xf6\x11\x3b\x2a\xf0\xd9\x52\xfb\x66\xee\x4b\xdb"
	"\x6c\xc4\x06\x28\x12\x28\xd2\x2c\x53\x3f\x08\x86\x89\x69\x6a\x89\x57\x7e\xb0\xd7\x81"
	"\xac\x79\xae\xfb\x56\xc0\x24\x0d\x84\x5a\xe1\x22\xc8\x3f\xba\x3c\x77\xbc\x22\x56\xdf"
	"\xf1\x78\x55\xcc\xd5\x97\x88\x00\x71\x1a\xe8\x8e\xa7\xb6\x87\x62\x55\x4d\x53\x87\xa2"
	"\x49\x3e\xa2\x79\x99\x51\x94\xd7\xe2\xf5\xce\x37\x1e\xc7\x79\xbd\xea\x0a\x31\x56\x1d"
	"\x56\x1a\x8c\xb7\xf3\xa5\xdc\x2a\xcd\xdc\xb1\x5c\x3e\x47\x20\xc7\xc2\x30\x32\xdf\x64"
	"\x91\xd5\xc6\xab\x85\x21\x67\x59\xc6\x09\x7c\x61\xd5\x90\x9b\xff\x5c\x6b\x48\x5e\x41"
	"\x85\xdb\x12\xed\x63\x04\xd3\x35\xca\x86\xce\x64\xa6\x54\x5b\x0f\x77\x90\x7e\xad\xd9"
	"\x56\x5d\x2f\x8a\x52\xd9\xcc\x79\x41\x69\x73\xca\x15\x0a\xbc\x68\x93\xbe\x2f\x03\x7c"
	"\x60\x7b\xf2\xa3\x03\x98\x34\x36\x9f\x88\x8e\xfb\x2b\x8a\x40\xdd\x35\xa9\x21\x2d\x92"
	"\xba\x1b\xe8\x58\xe1\x1c\xc2\x47\x34\x9c\x02\x82\x01\x00\x7f\xb4\x68\x1d\xde\x12\xd3"
	"\x00\x3a\x26\xb6\x60\x7e\xb9\x62\x9f\x31\xcb\x6b\x3a\x4f\x0e\x18\x64\x4f\x65\xad\x79"
	"\x1d\xd7\xe8\xbf\x83\x68\x78\xc4\x9e\x2b\x90\xcd\xd8\xad\x36\x1a\x1d\x4f\x56\x4b\x8a"
	"\x6f\xc3\x77\x93\x5e\xf4\x7d\x61\xfd\x48\xa5\xa5\xbe\x27\xa3\x5d\xa9\x0a\xb0\x77\xc9"
	"\x6d\x8d\xae\x94\x5a\x90\x07\xb4\x96\x1b\xda\x93\x56\xc5\x50\x19\xdd\xa4\xb3\x40\x66"
	"\x0b\xb0\x55\xdd\xd7\x9a\x50\x7b\x9a\xe7\xbc\x63\x65\x9b\x45\x2d\x82\xf3\xe7\x49\x19"
	"\x71\x10\x9d\x46\x2e\x82\x8e\x15\x35\xf8\x53\x99\x3f\x4f\xec\x83\x48\xef\x0f\x27\x28"
	"\x75\x1f\x73\x97\x1e\x14\x16\x24\x16\xe6\x3f\x67\xdc\x50\x4a\xdc\x83\x89\x47\xf9\x41"
	"\xa5\xec\xe1\x86\x08\x46\xf4\xa9\x97\x14\x73\xaf\xe6\x01\x30\xa1\xad\x2f\xb0\x2a\x35"
	"\xdd\xad\x1d\x70\x25\x95\xbf\x07\xd2\x8c\x14\x46\xc4\xd2\x79\x21\xdc\xa9\x21\x6e\xff"
	"\x99\x69\x47\x8b\x67\x95\x98\x4d\x09\x75\xee\x97\x0b\xd9\x3d\x75\xd1\xa8\x82\x1f\xc8"
	"\x15\x2e\x39\x29\x6f\x95\x4b\xf4\x94\x74\xeb\xd8\xe3\x4a\xfb\x41\xa8\x4c\x5c\x3d\xb6"
	"\x42\xc0\x15\x35\x9e\x1a\xe2\x54\x2c\x0c\x0b\x34\x5f\xe2\x35\x8d\x30\x73";
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
    static void* CertPeerData(void* p)
    {
        const AuthPeerData* peer = static_cast<GetCertOp*>(Op(p))->GetPeerData();
        if(peer==NULL) return NULL;
        peer->CreateRef();
        return const_cast<RefCount*>(static_cast<const RefCount*>(peer));
    }
    static void LightSetPeerData(void* l, void* p)
    {
        static_cast<LightGetCertOp*>(Op(l))->SetPeerData(p!=NULL?static_cast<const AuthPeerData*>(Obj(p)):NULL);
    }
    static bool PeerValid(void* c) { return static_cast<AuthContext*>(c)->mPeerData->IsValid(); }
    static int64_t PeerExpireDelta(void* c) { return (int64_t)static_cast<AuthContext*>(c)->mPeerData->GetExpireDelta(); }
    static EeWonAuthFlow* New(AuthContext* c)
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
        h.light_set_peer_data = LightSetPeerData;
        h.peer_valid = PeerValid;
        h.peer_expire_delta = PeerExpireDelta;
        return ee_won_auth_flow_new(&h);
    }
};

// Class statics: the signing public key (0x566018, 0x1a8 bytes) and its file-checked flag (0x5661e1)
ElGamal AuthContext::mVerifierKey(VerifierKeyBuf,sizeof(VerifierKeyBuf),false);
bool AuthContext::mCheckedVerifierFile=false;

// FUNCTION: WON 0x006ddb70
// Sets up the peer data, login-secret file path and server context, and loads the key file once per process
void AuthContext::Init()
{
    this->mFlow=AuthContextFlow::New(this);
    std::string aSecretFile=WONAPICore::GetDefaultFileDirectory()+"_wonlogin.ks";
    this->mLogin=ee_won_auth_login_new(reinterpret_cast<const uint8_t*>(aSecretFile.data()),aSecretFile.size());
    this->mPeerData=new AuthPeerData;
    this->mServerContext=new ServerContext;
    AutoCrit lock(GetVerifierCrit());
    if(AuthContext::mCheckedVerifierFile) return;
    AuthContext::LoadVerifierKey(WONAPICore::GetDefaultFileDirectory()+"_wonkver.pub");
}
// FUNCTION: WON 0x006dddf0
// Creates an empty auth context
AuthContext::AuthContext()
{
    this->Init();
}
// FUNCTION: WON 0x006de370
// Destroys the auth context (members only)
AuthContext::~AuthContext()
{
    ee_won_auth_flow_free(this->mFlow);
    ee_won_auth_login_free(this->mLogin);
}

// The peer sessions and the certificate flow are ee_won's (authflow.rs): the session map and its clean-up cursor,
// the waiting ops, the GetCertOp, the auto-refresh timer and the refresh status and flag; the lock stays here.

// FUNCTION: WON 0x006deeb0
// Caches a peer session for an address and drops up to five map entries unused for over 300 seconds
void AuthContext::PutSession(const IPAddr& address,AuthSession* session)
{
    AutoCrit lock(this->mDataCrit);
    if(session==NULL) return;
    ee_won_auth_flow_put_session(this->mFlow,address.GetValue(),static_cast<RefCount*>(session),(int64_t)time(NULL));
}
// FUNCTION: WON 0x006df080
// Takes a session used within 300 seconds for the address out of the cache; NULL when none
SmartPtr<AuthSession> AuthContext::GetSession(const IPAddr& address)
{
    AutoCrit lock(this->mDataCrit);
    void* aSession=ee_won_auth_flow_get_session(this->mFlow,address.GetValue(),(int64_t)time(NULL));
    if(aSession==NULL) return NULL;
    SmartPtr<AuthSession> session(static_cast<AuthSession*>(static_cast<RefCount*>(aSession)));
    static_cast<RefCount*>(aSession)->Release();
    return session;
}
// FUNCTION: WON 0x006de7f0
// Starts a certificate refresh when the current auto-refresh timer fires (not when killed)
void AuthContext::AutoRefreshCallback(AsyncOp* op)
{
    AutoCrit lock(this->mDataCrit);
    ee_won_auth_flow_auto_refresh_callback(this->mFlow,static_cast<RefCount*>(op));
}
// FUNCTION: WON 0x006de8e0
// Kills and drops the pending auto-refresh timer
void AuthContext::StopAutoRefresh()
{
    AutoCrit lock(this->mDataCrit);
    ee_won_auth_flow_stop_auto_refresh(this->mFlow);
}
// FUNCTION: WON 0x006deb30
// Turns certificate auto-refresh on or off and restarts the timer
void AuthContext::SetDoAutoRefresh(bool enabled)
{
    AutoCrit lock(this->mDataCrit);
    ee_won_auth_flow_set_do_auto_refresh(this->mFlow,enabled);
}
// FUNCTION: WON 0x006df260
// Kills the running certificate request and the auto-refresh timer
void AuthContext::Kill()
{
    AutoCrit lock(this->mDataCrit);
    ee_won_auth_flow_kill(this->mFlow);
}

// FUNCTION: WON 0x006de760
// Forwards the auto-refresh timer completion to its context
void AuthContext::StaticAutoRefreshCallback(SmartPtr<AsyncOp> op,SmartPtr<RefCount> parameter)
{
    static_cast<AuthContext*>(parameter.get())->AutoRefreshCallback(op.get());
}
// FUNCTION: WON 0x006de970
// Restarts the auto-refresh timer: 2 minutes before the certificate expires (at least 2), or 5 when forced
void AuthContext::StartAutoRefresh(bool force)
{
    AutoCrit lock(this->mDataCrit);
    ee_won_auth_flow_start_auto_refresh(this->mFlow,force);
}


// The login data is ee_won's (rust/crates/ee_won/src/authlogin.rs): the user name and password, the secrets and their
// file (_wonlogin.ks: a count byte, then a 32-bit expiry and 8 bytes each), the communities, their CD keys and file
// hashes, the joins, the user data and the nicknames, and the login request's blocks; its own lock.
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
    ee_won_auth_login_text(this->mLogin,inWhich,AuthAppendBytes,&aBytes);
    std::wstring aText;
    WideAssignUtf16LE(aText,aBytes.data(),(unsigned long)(aBytes.size()/2));
    return aText;
}
void AuthContext::LoginSet(int inWhich, const std::wstring& inKey, const std::string& inValue, long long inNum)
{
    std::string aKey=AuthUtf16(inKey);
    ee_won_auth_login_set(this->mLogin,inWhich,reinterpret_cast<const uint8_t*>(aKey.data()),aKey.size(),
                          reinterpret_cast<const uint8_t*>(inValue.data()),inValue.size(),inNum);
}
static std::string AuthKeyBytes(const CDKey& inKey)
{
    return inKey.mKey.get()!=NULL?std::string(inKey.mKey->data(),inKey.mKey->length()):std::string();
}
static void AuthAppendBlock(EeWonAuthLogin* inLogin, int inWhich, WriteBuffer& output, const RawBuffer* inChallenge)
{
    std::string aBytes;
    ee_won_auth_login_block(inLogin,inWhich,inChallenge!=NULL?inChallenge->data():NULL,
                            inChallenge!=NULL?inChallenge->size():0,AuthAppendBytes,&aBytes);
    output.AppendBytes(aBytes.data(),aBytes.length());
}

// FUNCTION: WON 0x006e1240
// Reads the saved login secrets (expiry + 8 bytes each) from the secret file; false when it cannot open
bool AuthContext::LoadSecretList() { return ee_won_auth_login_secrets(this->mLogin,0,NULL,0,0); }
// FUNCTION: WON 0x006e1460
// Writes the login secrets to the secret file; false when it cannot be created
bool AuthContext::SaveSecretList() { return ee_won_auth_login_secrets(this->mLogin,1,NULL,0,0); }
// FUNCTION: WON 0x006e1560
// Appends a count byte and every login secret to the login request
void AuthContext::AppendLoginSecrets(WriteBuffer& output) { AuthAppendBlock(this->mLogin,2,output,NULL); }
// FUNCTION: WON 0x006e1650
// Adds a new uncommitted 8-byte login secret at the front and saves the list
void AuthContext::AddSecret(const void* secret)
{
    ee_won_auth_login_secrets(this->mLogin,2,static_cast<const uint8_t*>(secret),0,0);
}
// FUNCTION: WON 0x006e1790
// Removes the matching uncommitted login secret and saves the list
void AuthContext::RemoveSecret(const void* secret)
{
    ee_won_auth_login_secrets(this->mLogin,3,static_cast<const uint8_t*>(secret),0,0);
}
// FUNCTION: WON 0x006e1890
// Gives uncommitted secrets an expiry, drops expired ones, caps the list below 256 entries and saves it
void AuthContext::CommitSecrets(time_t authTime,time_t duration)
{
    ee_won_auth_login_secrets(this->mLogin,4,NULL,(int64_t)authTime,(int64_t)duration);
}
// FUNCTION: WON 0x006e19e0
// Returns the current peer data (certificate and keys) under the lock
ConstSmartPtr<AuthPeerData> AuthContext::GetPeerData()
{
    AutoCrit lock(this->mDataCrit);
    return this->mPeerData;
}
// FUNCTION: WON 0x006e1a30
// Returns whether the auth2 certificate expires within the given number of seconds
bool AuthContext::IsExpired(time_t extra)
{
    AutoCrit lock(this->mDataCrit);
    return this->mPeerData->Certificate2Expired(extra);
}
// FUNCTION: WON 0x006e1aa0
// Replaces the peer data and clears the peer-session cache
void AuthContext::SetPeerData(const AuthPeerData* data)
{
    AutoCrit lock(this->mDataCrit);
    this->mPeerData=data;
    ee_won_auth_flow_clear_sessions(this->mFlow);
}


// FUNCTION: WON 0x006e0c50
// Appends each hashed community's file hash and its hash keyed with the server challenge
void AuthContext::AppendHashes(WriteBuffer& output,const RawBuffer& challenge)
{
    AuthAppendBlock(this->mLogin,1,output,&challenge);
}
// FUNCTION: WON 0x006e0e50
// Appends the login's community names and typed records (CD keys, joins, user data, secrets, nicknames)
void AuthContext::AppendCommunityData(WriteBuffer& output) { AuthAppendBlock(this->mLogin,0,output,NULL); }

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
    AutoCrit lock(this->mDataCrit);
    ee_won_auth_flow_get_cert_callback(this->mFlow,static_cast<RefCount*>(static_cast<AsyncOp*>(op)));
}
// FUNCTION: WON 0x006deba0
// Queues a refresh op and starts a certificate request when none is running
void AuthContext::RefreshAsync(LightGetCertOp* op,DWORD timeout)
{
    AutoCrit lock(this->mDataCrit);
    ee_won_auth_flow_refresh_async(this->mFlow,op!=NULL?static_cast<RefCount*>(op):NULL,timeout);
}
// FUNCTION: WON 0x006dedf0
// Fetches a new certificate synchronously; restarts auto-refresh on success and returns the status (no lock, as
// the original)
WONStatus AuthContext::RefreshBlock(DWORD timeout)
{
    return ee_won_auth_flow_refresh_block(this->mFlow,timeout);
}


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
    return ee_won_auth_login_hash_files(this->mLogin,reinterpret_cast<const uint8_t*>(aCommunity.data()),aCommunity.size(),
                                        reinterpret_cast<const uint8_t*>(aFiles.data()),aFiles.size());
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
    AuthContext::mCheckedVerifierFile=true;
    FILE* file=PlatFOpen(filename.c_str(),"rb");
    if(file==NULL) return false;
    unsigned char buffer[1024];
    RawBuffer key;
    while(!feof(file)) {
        int count=fread(buffer,1,1024,file);
        if(count>0) key.append(buffer,count);
    }
    fclose(file);
    AutoCrit lock(GetVerifierCrit());
    return AuthContext::mVerifierKey.SetPublicKey(key.data(),key.length());
}

// FUNCTION: WON 0x006df330
// Sets the auth server list
void AuthContext::SetServerContext(ServerContext* context)
{
    AutoCrit lock(this->mDataCrit);
    this->mServerContext=context;
}
// FUNCTION: WON 0x006df3e0
// Returns the auth server list
SmartPtr<ServerContext> AuthContext::GetServerContext()
{
    AutoCrit lock(this->mDataCrit);
    return this->mServerContext;
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

// No original address identified: WON SDK support body
bool AuthContext::AddAddress(const IPAddr &theAddr)
{
	AutoCrit aCrit(this->mDataCrit);
	return this->mServerContext->AddAddress(theAddr);
}

// No original address identified: WON SDK support body
bool AuthContext::AddAddressesFromDir(const DirEntityList &theDir, const wchar_t *theNameFilter)
{
	AutoCrit aCrit(this->mDataCrit);
	return this->mServerContext->AddAddressesFromDir(theDir, theNameFilter);
}

// No original address identified: WON SDK support bodies
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
	ee_won_auth_login_set(this->mLogin,EE_WON_AUTH_LOGIN_SECRET_FILE,NULL,0,
	                      reinterpret_cast<const uint8_t*>(theFile.data()),theFile.size(),0);
}
void AuthContext::AddNickname(const std::wstring &theKey) { this->LoginSet(EE_WON_AUTH_LOGIN_ADD_NICKNAME,theKey); }
void AuthContext::SetNickname(const std::wstring &theKey, const std::wstring &theName)
{
	this->LoginSet(EE_WON_AUTH_LOGIN_SET_NICKNAME,theKey,AuthUtf16(theName));
}
void AuthContext::ClearNicknames() { this->LoginSet(EE_WON_AUTH_LOGIN_CLEAR_NICKNAMES,std::wstring()); }
}

namespace WONAPI {

// No original address identified: WON SDK locked server-context API
bool AuthContext::RemoveAddress(const IPAddr &theAddr)
{
	AutoCrit aCrit(this->mDataCrit);
	return this->mServerContext->RemoveAddress(theAddr);
}

// No original address identified: WON SDK locked server-context API
void AuthContext::RemoveAllAddresses()
{
	AutoCrit aCrit(this->mDataCrit);
	this->mServerContext->RemoveAllAddresses();
}

// No original address identified: WON SDK locked server-context API
bool AuthContext::AddAddressesFromDir(const DirEntityMap &theDir, const std::wstring &thePath, const wchar_t *theNameFilter)
{
	AutoCrit aCrit(this->mDataCrit);
	return this->mServerContext->AddAddressesFromDir(theDir, thePath, theNameFilter);
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
