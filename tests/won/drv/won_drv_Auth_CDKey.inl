// CDKey's members only the drivers use, cut from ee/wonapi/WONAuthContext.h (R6 WON round 38): included in its
// class body when EE_WON_DRIVER is defined (tools/xbuild.py build_won_test)
void ClearFromRegistry(long inMode = 2, const char* inKey = mCDKeyPath) { this->Call(EeWonCdKeyCall::ClearRegistry, inKey, inMode); }
// the key encrypted for storage; NULL when invalid
ConstSmartPtr<ByteBuffer> GetEncrypted() const { ConstSmartPtr<ByteBuffer> r; ee_won_cdkey_obj_bytes(this->mKey, 3, WonByteBufferSink, &r); return r; }
bool SaveToFile(const std::string& inFileName) { return this->Call(EeWonCdKeyCall::SaveFile, inFileName.c_str(), 0); }
std::string GetProduct() const { return this->Bytes(0); } // the SDK's mProduct
bool IsBeta() const { return this->Call(EeWonCdKeyCall::Flag, NULL, 0); }
bool IsForceCheck() const { return this->Call(EeWonCdKeyCall::Flag, NULL, 1); }
