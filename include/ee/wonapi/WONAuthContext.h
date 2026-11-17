// ee/wonapi/WONAuthContext.h: WONAPI's CDKey and AuthContext, moved from ee/EEWonNet.h (which declares them only) in
// net round 26 and made shims over ee_won whose forwarders are here; included at the end of WONRouting.h. What needs
// the platform layer (the registry and file storage, the WON directory) is WONAuth/AuthContext.cpp's.
#ifndef EE_WONAPI_WONAUTHCONTEXT_H
#define EE_WONAPI_WONAUTHCONTEXT_H
#include "platform/PlatMessage.h" // PlatSystemMachineId
#include "rust/ee_won.h"

namespace WONAPI {
inline const uint8_t* WonBytes(const std::string& inText) { return reinterpret_cast<const uint8_t*>(inText.data()); }

// CDKey (0x1c bytes, a vtable without a destructor): the key logic (the text form, the check byte, the storage key
// and cipher) is ee_won's (rust/crates/ee_won_net/src/cdkey.rs); this class keeps the product, the raw key and validity
class CDKey {
protected:
    static const char* mCDKeyPath; // genuine SDK static default registry path, no instance layout
public:
    CDKey() : mValid(false) {} //2e3090: an empty, invalid key
    CDKey(const std::string& inProduct) : mProduct(inProduct), mValid(false) {}
    // 2e3ac0: the product's key from a registry value (genuine SDK defaults: local machine, the Sierra path), false
    // when absent or invalid; the stores below (AuthContext.cpp)
    bool LoadFromRegistry(long inMode = 2, const char* inKey = mCDKeyPath);
    void ClearFromRegistry(long inMode = 2, const char* inKey = mCDKeyPath);
    bool SaveToRegistry(long inMode = 2, const char* inKey = mCDKeyPath); //genuine SDK bool result
    void Invalidate() { this->mValid = false; } //2e30c0
    void SetProductString(const std::string& inProduct) { this->Invalidate(); this->mProduct = inProduct; } //2e30d0
    // 2e3320: the machine's id (Windows: the volume serial number of the Windows drive), a failure without one
    static DWORD GetMachineId(bool* outSuccess = NULL)
    {
        int found = 0;
        DWORD serial = PlatSystemMachineId(&found);
        if (outSuccess != NULL) *outSuccess = found != 0;
        return serial;
    }
    // 2e3880: a stored key decrypted with the machine-bound or the old key (the other tried on failure)
    bool InitEncrypted(const void* inData, unsigned long inLength)
    {
        this->mValid = false;
        this->mKey = NULL;
        if (inData == NULL) return false;
        bool gotId = false;
        DWORD machineId = CDKey::GetMachineId(&gotId);
        EeWonCdKeyDecoded d;
        ee_won_cdkey_decrypt(WonBytes(this->mProduct), this->mProduct.length(), machineId, gotId,
                             this->GetUseNewEncryptMethod(), static_cast<const uint8_t*>(inData), inLength, &d);
        if (d.has_raw) {
            this->mKey = new ByteBuffer(d.raw, 8, false);
            this->mValid = d.valid;
        }
        if (!d.used_machine) this->NotifyUsedOldMethodToDecrypt();
        return this->mValid;
    }
    // the key's text ("XXXX-XXXX-XXXX-XXXX-XXXX", dashes and white space optional); true when its check byte is right
    bool Init(const std::string& inKey)
    {
        this->mValid = false;
        this->mKey = NULL;
        uint8_t raw[8];
        return ee_won_cdkey_parse(WonBytes(inKey), inKey.length(), raw) && this->CalcKeyFromBuffer((const char*)raw);
    }
    std::string GetString() const // the key's text; empty without a key
    {
        if (this->mKey.get() == NULL || this->mKey->length() < 8) return "";
        char text[25];
        ee_won_cdkey_format(reinterpret_cast<const uint8_t*>(this->mKey->data()), text);
        return text;
    }
    ConstSmartPtr<ByteBuffer> GetEncrypted() const // the key encrypted for storage; NULL when invalid
    {
        if (!this->IsValid()) return NULL;
        bool useMachine = this->GetUseNewEncryptMethod();
        bool gotId = false;
        DWORD machineId = useMachine ? CDKey::GetMachineId(&gotId) : 0;
        ConstSmartPtr<ByteBuffer> result;
        ee_won_cdkey_encrypt(WonBytes(this->mProduct), this->mProduct.length(), machineId, useMachine && gotId,
                             reinterpret_cast<const uint8_t*>(this->mKey->data()), WonByteBufferSink, &result);
        return result;
    }
    bool InitRaw(const void* inRaw)
    {
        this->mValid = false;
        this->mKey = NULL;
        return this->CalcKeyFromBuffer(static_cast<const char*>(inRaw));
    }
    bool SaveToFile(const std::string&); // AuthContext.cpp
    bool LoadFromFile(const std::string&);
    bool IsBeta() const { return this->Flag(0); }
    bool IsForceCheck() const { return this->Flag(1); }
    const ByteBuffer* GetRaw() const { return mKey.get(); } //genuine SDK borrowed key accessor
    bool IsValid() const { return mValid; } //genuine SDK CDKey validity bool18
protected:
    // 2e3220: 8 raw key bytes, valid when their check byte matches a CRC of the product and key
    bool CalcKeyFromBuffer(const char* inRaw)
    {
        this->mKey = new ByteBuffer(inRaw, 8, false);
        this->mValid = ee_won_cdkey_is_valid(WonBytes(this->mProduct), this->mProduct.length(),
                                             reinterpret_cast<const uint8_t*>(inRaw));
        return this->mValid;
    }
    // 2e35a0: the Blowfish key from the product name's CRCs, optionally after the machine id
    void GetSymmetricKey(Blowfish& outKey, bool inUseMachine = true) const
    {
        bool gotId = false;
        DWORD machineId = inUseMachine ? CDKey::GetMachineId(&gotId) : 0;
        uint8_t bytes[12];
        size_t length = ee_won_cdkey_symmetric_key(WonBytes(this->mProduct), this->mProduct.length(), machineId,
                                                   inUseMachine && gotId, bytes);
        outKey.SetKey(bytes, static_cast<int>(length));
    }
    virtual void NotifyUsedOldMethodToDecrypt() {} //actual slot0/326b60, no virtual destructor
    virtual bool GetUseNewEncryptMethod() const { return true; } //actual slot4/2f30d0
    bool Flag(int inWhich) const // the beta (0) and force-check (1) flags
    {
        return this->mKey.get() != NULL
            && ee_won_cdkey_flag(reinterpret_cast<const uint8_t*>(this->mKey->data()), this->mKey->length(), inWhich);
    }
public:
    std::string mProduct; //04..14; actual SDK mProduct
    ConstSmartPtr<ByteBuffer> mKey; //14..18; actual SDK mKey, real ByteBuffer owner
    bool mValid; //18; actual SDK mIsValid; complete extent1c
};

// AuthContext (0xc0 bytes in the original, allocation 362dba, ctor 2dddf0, dtor 2de370) is ee_won's
// (rust/crates/ee_won_net/src/authobj.rs): its lock, sessions and certificate flow, peer data, login data, server list
// and the class's verifier key (0x566018, its checked flag 0x5661e1). This class is a RefCount shim holding one
// reference to it; every method forwards. The flow's calls back are Rust's since net round 26 (authcpp.rs, over the
// core table: AuthContextFlow is gone).
class AuthContext : public RefCount {
    void Init(); //2ddb70 original private SDK initializer (AuthContext.cpp: the WON directory's files)
public:
protected:
    virtual ~AuthContext() { ee_won_authctx_free(this->mCore); } //2de370, scalar2ddf90/30; genuine SDK protected access
public:
    AuthContext() { this->Init(); } //2dddf0
    AuthContext(const std::wstring& inUserName, const std::wstring& inCommunity, const std::wstring& inPassword)
    {
        this->Init();
        this->SetUserName(inUserName);
        this->SetPassword(inPassword);
        this->AddCommunity(inCommunity);
    }
    bool AddAddress(const IPAddr& inAddr) { return this->GetServerContext()->AddAddress(inAddr); }
    bool AddAddressesFromDir(const DirEntityList& inDir, const wchar_t* inNameFilter = NULL)
    { return this->GetServerContext()->AddAddressesFromDir(inDir, inNameFilter); }
    void RemoveAllCommunity() { this->LoginSet(EE_WON_AUTH_LOGIN_REMOVE_ALL_COMMUNITY, std::wstring()); }
    void AddCDKeyCommunityJoin(const std::wstring& inCommunity, const CDKey& inKey)
    { this->LoginSet(EE_WON_AUTH_LOGIN_ADD_JOIN, inCommunity, KeyBytes(inKey), inKey.mKey.get() != NULL ? 1 : 0); }
    void AddUserData(const std::wstring& inCommunity, const ByteBuffer* inData)
    {
        this->LoginSet(EE_WON_AUTH_LOGIN_ADD_USER_DATA, inCommunity,
                       inData != NULL ? std::string(inData->data(), inData->length()) : std::string(), inData != NULL ? 1 : 0);
    }
    void SetLoginSecretFile(const std::string& inFile)
    {
        ee_won_auth_login_set(ee_won_authctx_login(this->mCore), EE_WON_AUTH_LOGIN_SECRET_FILE, NULL, 0, WonBytes(inFile),
                              inFile.size(), 0);
    }
    void AddNickname(const std::wstring& inKey) { this->LoginSet(EE_WON_AUTH_LOGIN_ADD_NICKNAME, inKey); }
    void SetNickname(const std::wstring& inKey, const std::wstring& inName)
    { this->LoginSet(EE_WON_AUTH_LOGIN_SET_NICKNAME, inKey, Utf16(inName)); }
    void ClearNicknames() { this->LoginSet(EE_WON_AUTH_LOGIN_CLEAR_NICKNAMES, std::wstring()); }
    bool RemoveAddress(const IPAddr& inAddr) { return this->GetServerContext()->RemoveAddress(inAddr); }
    void RemoveAllAddresses() { this->GetServerContext()->RemoveAllAddresses(); }
    bool AddAddressesFromDir(const std::map<std::wstring,DirEntityList>& inMap, const std::wstring& inPath,
                             const wchar_t* inNameFilter = NULL)
    { return this->GetServerContext()->AddAddressesFromDir(inMap, inPath, inNameFilter); }
    static bool LoadVerifierKey(const std::string& inFile) //2df440
    { return ee_won_auth_verifier_load(WonBytes(inFile), inFile.size()); }
    void SetServerContext(ServerContext* inContext) //2df330
    { ee_won_authctx_set_server_context(this->mCore, inContext != NULL ? static_cast<RefCount*>(inContext) : NULL); }
    SmartPtr<ServerContext> GetServerContext() //2df3e0: the context's own reference handed to the result
    {
        void* aServers = ee_won_authctx_server_context(this->mCore);
        if (aServers == NULL) return NULL;
        SmartPtr<ServerContext> servers(static_cast<ServerContext*>(static_cast<RefCount*>(aServers)));
        static_cast<RefCount*>(aServers)->Release();
        return servers;
    }
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
        this->LoginSet(EE_WON_AUTH_LOGIN_CDKEY, inCommunity, KeyBytes(inKey),
                       (inKey.mKey.get() != NULL ? 1 : 0) | (inKey.mValid ? 2 : 0));
    }
    bool SetHashFile(const std::wstring&); //2e06d0: the exe's path (AuthContext.cpp)
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
    void GetCertCallback(GetCertOp* inOp) //2de5f0
    { ee_won_authctx_get_cert_callback(this->mCore, static_cast<RefCount*>(static_cast<AsyncOp*>(inOp))); }
    static void StaticGetCertCallback(SmartPtr<AsyncOp> inOp, SmartPtr<RefCount> inContext) //2de560
    { static_cast<AuthContext*>(inContext.get())->GetCertCallback(static_cast<GetCertOp*>(inOp.get())); }
    // the Rust object (the certificate ops' host calls answer on it: ee_won hostauth.rs)
    const EeWonAuthCtx* Core() const { return this->mCore; }
private:
    const EeWonAuthCtx* mCore; // one reference
    static std::string Utf16(const std::wstring& inText) { std::string b; WideAppendUtf16LE(b, inText); return b; }
    static std::string KeyBytes(const CDKey& inKey)
    { return inKey.mKey.get() != NULL ? std::string(inKey.mKey->data(), inKey.mKey->length()) : std::string(); }
    std::wstring LoginText(int inWhich) const
    {
        std::string aBytes;
        ee_won_auth_login_text(ee_won_authctx_login(this->mCore), inWhich, WonStringSink, &aBytes);
        std::wstring aText;
        WideAssignUtf16LE(aText, aBytes.data(), (unsigned long)(aBytes.size() / 2));
        return aText;
    }
    void LoginSet(int inWhich, const std::wstring& inKey, const std::string& inValue = std::string(), long long inNum = 0)
    {
        std::string aKey = Utf16(inKey);
        ee_won_auth_login_set(ee_won_authctx_login(this->mCore), inWhich, WonBytes(aKey), aKey.size(), WonBytes(inValue),
                              inValue.size(), inNum);
    }
    void AppendBlock(int inWhich, WriteBuffer& outBuffer, const std::basic_string<unsigned char>* inChallenge)
    {
        std::string aBytes;
        ee_won_auth_login_block(ee_won_authctx_login(this->mCore), inWhich, inChallenge != NULL ? inChallenge->data() : NULL,
                                inChallenge != NULL ? inChallenge->size() : 0, WonStringSink, &aBytes);
        outBuffer.AppendBytes(aBytes.data(), aBytes.length());
    }
    bool Secrets(int inWhich, const void* inKey = NULL, time_t inT0 = 0, time_t inT1 = 0)
    {
        return ee_won_auth_login_secrets(ee_won_authctx_login(this->mCore), inWhich, static_cast<const uint8_t*>(inKey),
                                         (int64_t)inT0, (int64_t)inT1);
    }
};
} //namespace WONAPI

#endif
