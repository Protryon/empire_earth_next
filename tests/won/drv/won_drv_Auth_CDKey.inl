// CDKey's members only the drivers use, cut from ee/wonapi/WONAuthContext.h (R6 WON round 38): included in its
// class body when EE_WON_DRIVER is defined (tools/xbuild.py build_won_test)
void ClearFromRegistry(long inMode = 2, const char* inKey = mCDKeyPath) { this->Call(EeWonCdKeyCall::ClearRegistry, inKey, inMode); }
// the key encrypted for storage; NULL when invalid
ConstSmartPtr<ByteBuffer> GetEncrypted() const { ConstSmartPtr<ByteBuffer> r; ee_won_cdkey_obj_bytes(this->mKey, 3, WonByteBufferSink, &r); return r; }
bool SaveToFile(const std::string& inFileName) { return this->Call(EeWonCdKeyCall::SaveFile, inFileName.c_str(), 0); }
std::string GetProduct() const { return this->Bytes(0); } // the SDK's mProduct
bool IsBeta() const { return this->Call(EeWonCdKeyCall::Flag, NULL, 0); }
bool IsForceCheck() const { return this->Call(EeWonCdKeyCall::Flag, NULL, 1); }
// (R6 WON round 39)
// 2e3880: a stored key decrypted with the machine-bound or the old key (the other tried on failure)
bool InitEncrypted(const void* inData, unsigned long inLength) { return this->Call(EeWonCdKeyCall::InitEncrypted, inData, inLength); }
bool InitRaw(const void* inRaw) { return this->Call(EeWonCdKeyCall::InitRaw, inRaw, 8); }
