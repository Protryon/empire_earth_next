// AuthContext's members only the drivers use, cut from ee/wonapi/WONAuthContext.h (R6 WON round 38): included in its
// class body when EE_WON_DRIVER is defined (tools/xbuild.py build_won_test)
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
static bool LoadVerifierKey(const std::string& inFile) //2df440
{ return ee_won_auth_verifier_load(WonBytes(inFile), inFile.size()); }
void ClearCDKeyCommunityJoins() { this->LoginSet(EE_WON_AUTH_LOGIN_CLEAR_JOINS, std::wstring()); } //2e0510
void ClearUserData() { this->LoginSet(EE_WON_AUTH_LOGIN_CLEAR_USER_DATA, std::wstring()); } //2e05e0
void ClearOneTimeData() { this->LoginSet(EE_WON_AUTH_LOGIN_CLEAR_ONE_TIME, std::wstring()); } //2e06b0
bool LoadSecretList() { return this->Secrets(0); } //2e1240
bool SaveSecretList() { return this->Secrets(1); } //2e1460
void AppendLoginSecrets(WriteBuffer& outBuffer) { this->AppendBlock(2, outBuffer, NULL); } //2e1560
void AddSecret(const void* inSecret) { this->Secrets(2, inSecret); } //2e1650
void RemoveSecret(const void* inSecret) { this->Secrets(3, inSecret); } //2e1790
void CommitSecrets(time_t inAuthTime, time_t inDuration) { this->Secrets(4, NULL, inAuthTime, inDuration); } //2e1890
void SetPeerData(const AuthPeerData* inData) //2e1aa0
{ ee_won_authctx_set_peer_data(this->mCore, inData != NULL ? inData->Impl() : NULL); }
std::wstring GetUserName() const { return this->LoginText(1); }
void AppendHashes(WriteBuffer& outBuffer, const std::basic_string<unsigned char>& inChallenge) //2e0c50
{ this->AppendBlock(1, outBuffer, &inChallenge); }
void AppendCommunityData(WriteBuffer& outBuffer) { this->AppendBlock(0, outBuffer, NULL); } //2e0e50
static void StaticAutoRefreshCallback(SmartPtr<AsyncOp> inOp, SmartPtr<RefCount> inContext) //2de760
{ static_cast<AuthContext*>(inContext.get())->AutoRefreshCallback(inOp.get()); }
void AutoRefreshCallback(AsyncOp* inOp) //2de7f0
{ ee_won_authctx_auto_refresh_callback(this->mCore, static_cast<RefCount*>(inOp)); }
void StopAutoRefresh() { ee_won_authctx_stop_auto_refresh(this->mCore); } //2de8e0
void StartAutoRefresh(bool inForce = false) { ee_won_authctx_start_auto_refresh(this->mCore, inForce); } //2de970
// a waiting LightGetCertOp: its run (an AsyncOp) and its member (ee_won authflow.rs), or NULL for none (2deba0)
void RefreshAsync(AsyncOp* inOp = NULL, EeWonLightCert* inCert = NULL, DWORD inTimeout = INFINITE)
{ ee_won_authctx_refresh_async(this->mCore, inOp != NULL ? static_cast<RefCount*>(inOp) : NULL, inCert, inTimeout); }
long RefreshBlock(DWORD inTimeout = INFINITE) { return ee_won_authctx_refresh_block(this->mCore, inTimeout); } //2dedf0
void GetCertCallback(AsyncOp* inOp) //2de5f0
{ ee_won_authctx_get_cert_callback(this->mCore, static_cast<RefCount*>(inOp)); }
static void StaticGetCertCallback(SmartPtr<AsyncOp> inOp, SmartPtr<RefCount> inContext) //2de560
{ static_cast<AuthContext*>(inContext.get())->GetCertCallback(inOp.get()); }
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
ConstSmartPtr<AuthPeerData> GetPeerData() { return AuthPeerData::Adopt(ee_won_authctx_peer_data(this->mCore)); } //2e19e0
bool RemoveAddress(const IPAddr& inAddr) { return this->GetServerContext()->RemoveAddress(inAddr); }
bool SetHashFile(const std::wstring& inCommunity, const std::string& inFile) //2e07f0
{ return this->SetHashFileList(inCommunity, std::list<std::string>(1, inFile)); }
