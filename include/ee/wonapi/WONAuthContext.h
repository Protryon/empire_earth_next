// ee/wonapi/WONAuthContext.h: WONAPI's CDKey and AuthContext, moved from ee/EEWonNet.h (which declares them only) in
// net round 26 and made shims over ee_won whose forwarders are here; included at the end of WONRouting.h. What needs
// the classes below it (Init: the certificate op and the server list) is WONRouting.h's, after them.
#ifndef EE_WONAPI_WONAUTHCONTEXT_H
#define EE_WONAPI_WONAUTHCONTEXT_H
#include "platform/PlatMessage.h" // PlatSystemMachineId
#include "rust/won_net/authflow.h"
#include "rust/won_net/authlogin.h"
#include "rust/won_net/authobj.h"
#include "rust/won_net/cdkeystore.h"
#include "rust/won_net/ffi.h"

namespace WONAPI {
inline const uint8_t* WonBytes(const std::string& inText) { return reinterpret_cast<const uint8_t*>(inText.data()); }

// CDKey (0x1c bytes in the original, a vtable without a destructor) is ee_won's (rust/crates/ee_won_net/src/
// cdkeystore.rs EeWonCdKeyObj, R6 WON round 33): the product, the raw key and its validity, the key logic (cdkey.rs)
// and the stores; this class owns one and every method forwards. (The two virtuals had no override: the Rust always
// uses the new encryption method and the old method's notice did nothing.)
class CDKey {
protected:
    static constexpr const char* mCDKeyPath = "SOFTWARE\Sierra\CDKeys"; // the WON SDK's default registry path
public:
    CDKey() : mKey(ee_won_cdkey_obj_new(NULL, 0)) {} //2e3090: an empty, invalid key
    CDKey(const std::string& inProduct) : mKey(ee_won_cdkey_obj_new(WonBytes(inProduct), inProduct.size())) {}
    CDKey(const CDKey& inKey) : mKey(ee_won_cdkey_obj_clone(inKey.mKey)) {}
    CDKey& operator=(const CDKey& inKey) { ee_won_cdkey_obj_assign(this->mKey, inKey.mKey); return *this; }
    ~CDKey() { ee_won_cdkey_obj_free(this->mKey); }
    // 2e3ac0: the product's key from a registry value (genuine SDK defaults: local machine, the Sierra path), false
    // when absent or invalid; the stores are cdkeystore.rs's
    bool LoadFromRegistry(long inMode = 2, const char* inKey = mCDKeyPath) { return this->Call(EeWonCdKeyCall::LoadRegistry, inKey, inMode); }
    void ClearFromRegistry(long inMode = 2, const char* inKey = mCDKeyPath) { this->Call(EeWonCdKeyCall::ClearRegistry, inKey, inMode); }
    bool SaveToRegistry(long inMode = 2, const char* inKey = mCDKeyPath) { return this->Call(EeWonCdKeyCall::SaveRegistry, inKey, inMode); }
    void Invalidate() { this->Call(EeWonCdKeyCall::Invalidate, NULL, 0); } //2e30c0
    void SetProductString(const std::string& inProduct) { this->Call(EeWonCdKeyCall::SetProduct, inProduct.data(), inProduct.size()); } //2e30d0
    // 2e3320: the machine's id (Windows: the volume serial number of the Windows drive), a failure without one
    static DWORD GetMachineId(bool* outSuccess = NULL) { return ee_won_cdkey_machine_id(outSuccess); }
    // 2e3880: a stored key decrypted with the machine-bound or the old key (the other tried on failure)
    bool InitEncrypted(const void* inData, unsigned long inLength) { return this->Call(EeWonCdKeyCall::InitEncrypted, inData, inLength); }
    // the key's text ("XXXX-XXXX-XXXX-XXXX-XXXX", dashes and white space optional); true when its check byte is right
    bool Init(const std::string& inKey) { return this->Call(EeWonCdKeyCall::Init, inKey.data(), inKey.size()); }
    std::string GetString() const { return this->Bytes(2); } // the key's text; empty without a key
    // the key encrypted for storage; NULL when invalid
    ConstSmartPtr<ByteBuffer> GetEncrypted() const { ConstSmartPtr<ByteBuffer> r; ee_won_cdkey_obj_bytes(this->mKey, 3, WonByteBufferSink, &r); return r; }
    bool InitRaw(const void* inRaw) { return this->Call(EeWonCdKeyCall::InitRaw, inRaw, 8); }
    bool SaveToFile(const std::string& inFileName) { return this->Call(EeWonCdKeyCall::SaveFile, inFileName.c_str(), 0); }
    bool LoadFromFile(const std::string& inFileName) { return this->Call(EeWonCdKeyCall::LoadFile, inFileName.c_str(), 0); }
    // genuine SDK borrowed key accessor (now a copy: the key's bytes are Rust's); NULL without a key
    ConstSmartPtr<ByteBuffer> GetRaw() const { ConstSmartPtr<ByteBuffer> r; ee_won_cdkey_obj_bytes(this->mKey, 1, WonByteBufferSink, &r); return r; }
    bool IsValid() const { return this->Call(EeWonCdKeyCall::IsValid, NULL, 0); } //genuine SDK CDKey validity bool18
    bool HasKey() const { return this->Call(EeWonCdKeyCall::HasKey, NULL, 0); } // the SDK's mKey.get() != NULL
    std::string KeyBytes() const { return this->Bytes(1); } // the raw key's bytes (empty without a key)
    std::string GetProduct() const { return this->Bytes(0); } // the SDK's mProduct
    bool IsBeta() const { return this->Call(EeWonCdKeyCall::Flag, NULL, 0); }
    bool IsForceCheck() const { return this->Call(EeWonCdKeyCall::Flag, NULL, 1); }
private:
    bool Call(EeWonCdKeyCall inCall, const void* inData, intptr_t inA) const { return ee_won_cdkey_obj_call(this->mKey, inCall, static_cast<const uint8_t*>(inData), inA); }
    std::string Bytes(int inWhich) const { std::string s; ee_won_cdkey_obj_bytes(this->mKey, inWhich, WonStringSink, &s); return s; }
    EeWonCdKeyObj* mKey; // owned
};

// AuthContext (0xc0 bytes in the original, allocation 362dba, ctor 2dddf0, dtor 2de370) is ee_won's
// (rust/crates/ee_won_net/src/authobj.rs): its lock, sessions and certificate flow, peer data, login data, server list
// and the class's verifier key (0x566018, its checked flag 0x5661e1). This class is a RefCount shim holding one
// reference to it; every method forwards. The flow's calls back are Rust's since net round 26 (authcpp.rs, over the
// core table: AuthContextFlow is gone).
class AuthContext : public RefCount {
    // 2ddb70: the Rust object (authcpp.rs ee_won_authctx_new_default: the login secrets in the WON directory, a new
    // server list, the directory's verifier key) with the certificate op maker (WONRouting.h)
    void Init();
    static void* NewCertOp(void* inContext);
public:
protected:
    virtual ~AuthContext() { ee_won_authctx_free(this->mCore); } //2de370, scalar2ddf90/30; genuine SDK protected access
public:
    AuthContext() { this->Init(); } //2dddf0
    // (the SDK's AuthContext(user, community, password), Init then the three setters, had no caller and went in R6
    // WON lobby and network round 37)
    bool AddAddress(const IPAddr& inAddr) { return this->GetServerContext()->AddAddress(inAddr); }
    bool AddAddressesFromDir(const DirList& inDir, const wchar_t* inNameFilter = NULL)
    { return this->GetServerContext()->AddAddressesFromDir(inDir, inNameFilter); }
    void RemoveAllCommunity() { this->LoginSet(EE_WON_AUTH_LOGIN_REMOVE_ALL_COMMUNITY, std::wstring()); }
    void AddCDKeyCommunityJoin(const std::wstring& inCommunity, const CDKey& inKey)
    { this->LoginSet(EE_WON_AUTH_LOGIN_ADD_JOIN, inCommunity, inKey.KeyBytes(), inKey.HasKey() ? 1 : 0); }
    void AddUserData(const std::wstring& inCommunity, const ByteBuffer* inData)
    {
        this->LoginSet(EE_WON_AUTH_LOGIN_ADD_USER_DATA, inCommunity,
                       inData != NULL ? std::string(inData->data(), inData->length()) : std::string(), inData != NULL ? 1 : 0);
    }
    void AddNickname(const std::wstring& inKey) { this->LoginSet(EE_WON_AUTH_LOGIN_ADD_NICKNAME, inKey); }
    void SetNickname(const std::wstring& inKey, const std::wstring& inName)
    { this->LoginSet(EE_WON_AUTH_LOGIN_SET_NICKNAME, inKey, Utf16(inName)); }
    void ClearNicknames() { this->LoginSet(EE_WON_AUTH_LOGIN_CLEAR_NICKNAMES, std::wstring()); }
    bool RemoveAddress(const IPAddr& inAddr) { return this->GetServerContext()->RemoveAddress(inAddr); }
    void RemoveAllAddresses() { this->GetServerContext()->RemoveAllAddresses(); }
    static bool LoadVerifierKey(const std::string& inFile) //2df440
    { return ee_won_auth_verifier_load(WonBytes(inFile), inFile.size()); }
    void SetServerContext(ServerContext* inContext) //2df330
    { ee_won_authctx_set_server_context(this->mCore, inContext != NULL ? static_cast<RefCount*>(inContext) : NULL); }
    SmartPtr<ServerContext> GetServerContext() //2df3e0: a reference of the result's own
    { return static_cast<ServerContext*>(static_cast<RefCount*>(ee_won_authctx_server_context_borrowed(this->mCore))); }
    void ClearCDKeyCommunityJoins() { this->LoginSet(EE_WON_AUTH_LOGIN_CLEAR_JOINS, std::wstring()); } //2e0510
    void ClearUserData() { this->LoginSet(EE_WON_AUTH_LOGIN_CLEAR_USER_DATA, std::wstring()); } //2e05e0
    void ClearOneTimeData() { this->LoginSet(EE_WON_AUTH_LOGIN_CLEAR_ONE_TIME, std::wstring()); } //2e06b0
    bool LoadSecretList() { return this->Secrets(0); } //2e1240
    bool SaveSecretList() { return this->Secrets(1); } //2e1460
    void AppendLoginSecrets(WriteBuffer& outBuffer) { this->AppendBlock(2, outBuffer, NULL); } //2e1560
    void AddSecret(const void* inSecret) { this->Secrets(2, inSecret); } //2e1650
    void RemoveSecret(const void* inSecret) { this->Secrets(3, inSecret); } //2e1790
    void CommitSecrets(time_t inAuthTime, time_t inDuration) { this->Secrets(4, NULL, inAuthTime, inDuration); } //2e1890
    ConstSmartPtr<AuthPeerData> GetPeerData() { return AuthPeerData::Adopt(ee_won_authctx_peer_data(this->mCore)); } //2e19e0
    bool IsExpired(time_t inExtra = 0) { return ee_won_authctx_is_expired(this->mCore, (int64_t)inExtra); } //2e1a30
    void SetPeerData(const AuthPeerData* inData) //2e1aa0
    { ee_won_authctx_set_peer_data(this->mCore, inData != NULL ? inData->Impl() : NULL); }
    void SetUserName(const std::wstring& inName) { this->LoginSet(EE_WON_AUTH_LOGIN_NAME, std::wstring(), Utf16(inName)); } //2df620
    void SetPassword(const std::wstring& inPassword) //2df760
    { this->LoginSet(EE_WON_AUTH_LOGIN_PASSWORD, std::wstring(), Utf16(inPassword)); }
    void AddCommunity(const std::wstring& inCommunity) { this->LoginSet(EE_WON_AUTH_LOGIN_ADD_COMMUNITY, inCommunity); } //2df8a0
    void RemoveCommunity(const std::wstring& inCommunity) //2e02d0
    { this->LoginSet(EE_WON_AUTH_LOGIN_REMOVE_COMMUNITY, inCommunity); }

    // inline accessor: one more inline expansion in EEUIEWNAROpenScreen::ProcessEvent makes VC6 refuse the fourth
    // UIEvent ctor as in the original (0x1eb3a2)
    std::wstring GetName() const { return this->LoginText(1); } // a copy (the original returned its member)
    std::wstring GetUserName() const { return this->LoginText(1); }
    std::wstring GetPassword() const { return this->LoginText(2); }

    void SetDoAutoRefresh(bool inOn) { ee_won_authctx_set_do_auto_refresh(this->mCore, inOn); } //2deb30
    // 2dfcc0: the selected community's record kept before the clear
    void SetCommunity(const std::wstring& inCommunity) { this->LoginSet(EE_WON_AUTH_LOGIN_SET_COMMUNITY, inCommunity); }
    void SetCDKey(const std::wstring& inCommunity, const CDKey& inKey) //2e0380
    {
        this->LoginSet(EE_WON_AUTH_LOGIN_CDKEY, inCommunity, inKey.KeyBytes(), (inKey.HasKey() ? 1 : 0) | (inKey.IsValid() ? 2 : 0));
    }
    bool SetHashFile(const std::wstring& inCommunity) //2e06d0: the running exe in the data root (authlogin.rs)
    {
        return ee_won_auth_login_hash_exe(ee_won_authctx_login(this->mCore), inCommunity.data(), sizeof(wchar_t),
                                          inCommunity.size());
    }
    bool SetHashFile(const std::wstring& inCommunity, const std::string& inFile) //2e07f0
    { return this->SetHashFileList(inCommunity, std::list<std::string>(1, inFile)); }
    bool SetHashFileList(const std::wstring& inCommunity, const std::list<std::string>& inFiles) //2e08e0
    {
        std::string aCommunity = Utf16(inCommunity);
        std::string aFiles;
        for (std::list<std::string>::const_iterator i = inFiles.begin(); i != inFiles.end(); ++i)
            aFiles.append(i->c_str(), i->size() + 1);
        return ee_won_auth_login_hash_files(ee_won_authctx_login(this->mCore), WonBytes(aCommunity), aCommunity.size(),
                                            WonBytes(aFiles), aFiles.size());
    }
    void AppendHashes(WriteBuffer& outBuffer, const std::basic_string<unsigned char>& inChallenge) //2e0c50
    { this->AppendBlock(1, outBuffer, &inChallenge); }
    void AppendCommunityData(WriteBuffer& outBuffer) { this->AppendBlock(0, outBuffer, NULL); } //2e0e50
    static void StaticAutoRefreshCallback(SmartPtr<AsyncOp> inOp, SmartPtr<RefCount> inContext) //2de760
    { static_cast<AuthContext*>(inContext.get())->AutoRefreshCallback(inOp.get()); }
    void AutoRefreshCallback(AsyncOp* inOp) //2de7f0
    { ee_won_authctx_auto_refresh_callback(this->mCore, static_cast<RefCount*>(inOp)); }
    void StopAutoRefresh() { ee_won_authctx_stop_auto_refresh(this->mCore); } //2de8e0
    void Kill() { ee_won_authctx_kill(this->mCore); } //2df260
    void StartAutoRefresh(bool inForce = false) { ee_won_authctx_start_auto_refresh(this->mCore, inForce); } //2de970
    // a waiting LightGetCertOp: its run (an AsyncOp) and its member (ee_won authflow.rs), or NULL for none (2deba0)
    void RefreshAsync(AsyncOp* inOp = NULL, EeWonLightCert* inCert = NULL, DWORD inTimeout = INFINITE)
    { ee_won_authctx_refresh_async(this->mCore, inOp != NULL ? static_cast<RefCount*>(inOp) : NULL, inCert, inTimeout); }
    long RefreshBlock(DWORD inTimeout = INFINITE) { return ee_won_authctx_refresh_block(this->mCore, inTimeout); } //2dedf0
    void GetCertCallback(AsyncOp* inOp) //2de5f0
    { ee_won_authctx_get_cert_callback(this->mCore, static_cast<RefCount*>(inOp)); }
    static void StaticGetCertCallback(SmartPtr<AsyncOp> inOp, SmartPtr<RefCount> inContext) //2de560
    { static_cast<AuthContext*>(inContext.get())->GetCertCallback(inOp.get()); }
    // the Rust object (the certificate ops' host calls answer on it: ee_won hostauth.rs)
    const EeWonAuthCtx* Core() const { return this->mCore; }
private:
    const EeWonAuthCtx* mCore; // one reference
    static std::string Utf16(const std::wstring& inText) { std::string b; WideAppendUtf16LE(b, inText); return b; }
    std::wstring LoginText(int inWhich) const
    { std::wstring s; ee_won_auth_login_text_wide(ee_won_authctx_login(this->mCore), inWhich, sizeof(wchar_t), WonWideTextSink, &s); return s; }
    void LoginSet(int inWhich, const std::wstring& inKey, const std::string& inValue = std::string(), long long inNum = 0)
    {
        ee_won_auth_login_set_wide(ee_won_authctx_login(this->mCore), inWhich, inKey.data(), sizeof(wchar_t), inKey.size(),
                                   WonBytes(inValue), inValue.size(), inNum);
    }
    void AppendBlock(int inWhich, WriteBuffer& outBuffer, const std::basic_string<unsigned char>* inChallenge)
    {
        ee_won_auth_login_block_to(ee_won_authctx_login(this->mCore), inWhich, inChallenge != NULL ? inChallenge->data() : NULL,
                                   inChallenge != NULL ? inChallenge->size() : 0, outBuffer.mBuffer);
    }
    bool Secrets(int inWhich, const void* inKey = NULL, time_t inT0 = 0, time_t inT1 = 0)
    {
        return ee_won_auth_login_secrets(ee_won_authctx_login(this->mCore), inWhich, static_cast<const uint8_t*>(inKey),
                                         (int64_t)inT0, (int64_t)inT1);
    }
};
} //namespace WONAPI

#endif
