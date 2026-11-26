// RegKey, the C++ class over the Rust registry key (ee_won regkey.rs EeWonRegKey), as include/ee/wonapi/
// WONLobbyConfig.h had it until R6 WON lobby and network round 37: the game makes its calls on the Rust key
// directly since; the drivers that test the key through the class include this after WONLobbyConfig.h.
#pragma once
#include "rust/won_net/regkey.h"
namespace WONAPI {
// a registry key (RTTI RegKey, three virtual slots; constructor and destructor out of line). The port keeps no
// registry: a key is a block of config.yaml (platform/PlatConfig.h), "won.<path with \ as .>" (the root is
// dropped), its values that block's keys: a DWORD an integer, a string a string, binary data a hex string. Opening
// always succeeds for a non-empty path (a missing key reads as an empty one); keys and values are not enumerated.
class RegKey {
public:
    enum GetResult { Ok, NotFound, BadType, NoMore, NoSize };
    enum DataType { Long, String, Binary, Other };
    // the key's block, path and open flag and every method are ee_won's (regkey.rs EeWonRegKey; RegKey.cpp is
    // deleted: net round 27)
    RegKey(int inRoot, const std::string& inPath, bool, bool)
        : mKey(ee_won_regkey_new(inRoot, B(inPath), inPath.size())) {}
    virtual ~RegKey() { ee_won_regkey_free(this->mKey); }
    GetResult GetValue(const std::string& inName, std::string& outValue) const // any value reads as a string
    { outValue.erase(); return this->Op(EE_WON_REGKEY_GET_VALUE, inName, &outValue) ? Ok : NotFound; }
    bool OpenSubKey(const std::string& inName, bool = false, bool = true) { return this->Op(EE_WON_REGKEY_OPEN_SUBKEY, inName) != 0; }
    bool GetSubKey(const std::string& inName, RegKey& outKey, bool = false, bool = true) const
    { return ee_won_regkey_child(this->mKey, outKey.mKey, false, B(inName), inName.size()); }
    GetResult GetFirstSubKey(RegKey&) const { return NotFound; } // config.yaml blocks are not enumerated
    GetResult GetFirstValueName(std::string&, DataType&) const { return NotFound; } // nor their keys
    bool SetValue(const std::string& inName, const std::string& inValue)
    { return this->Op(EE_WON_REGKEY_SET_TEXT, inName, NULL, NULL, B(inValue), inValue.size()) != 0; }
    bool SetValue(const std::string& inName, unsigned long inValue) // a REG_DWORD: 32 bits
    { long long n = (long long)inValue; return this->Op(EE_WON_REGKEY_SET_DWORD, inName, NULL, &n) != 0; }
    bool SetValue(const std::string& inName, const void* inData, unsigned long inLength) // as hex text
    { return this->Op(EE_WON_REGKEY_SET_BINARY, inName, NULL, NULL, static_cast<const uint8_t*>(inData), inLength) != 0; }
    bool DeleteValue(const std::string& inName) { return this->Op(EE_WON_REGKEY_DELETE_VALUE, inName) != 0; }
    virtual bool DeleteSubKey(const std::string& inKey) { return this->Op(EE_WON_REGKEY_DELETE_SUBKEY, inKey) != 0; }
    virtual bool DeleteSubKey(RegKey& inKey) { return ee_won_regkey_child(this->mKey, inKey.mKey, true, NULL, 0); }
    // 0 and the value, 2 for a value that is no integer, 1 when missing (the value 0 then)
    int QueryDWORD(const std::string& inName, DWORD& outValue)
    { long long n = 0; int r = this->Op(EE_WON_REGKEY_QUERY_DWORD, inName, NULL, &n); outValue = (DWORD)n; return r; }
    bool OpenNewKey(int inRoot, const std::string& inPath, bool, bool)
    { long long n = inRoot; return this->Op(EE_WON_REGKEY_OPEN_NEW, inPath, NULL, &n) != 0; }
    // 0, 2 wrong type, 4 empty (the caller's buffer left alone) or 1 missing
    int QueryBinary(const std::string& inName, ConstSmartPtr<ByteBuffer>& outValue)
    {
        std::string aBytes;
        int r = this->Op(EE_WON_REGKEY_QUERY_BINARY, inName, &aBytes);
        if (r == 0) outValue = new ByteBuffer(aBytes.data(), aBytes.length(), false);
        return r;
    }
    bool IsOpen() const { return ee_won_regkey_op(this->mKey, EE_WON_REGKEY_IS_OPEN, NULL, 0, NULL, 0, NULL, NULL, NULL) != 0; }
    std::string GetLeafName() const { std::string s; this->Op(EE_WON_REGKEY_LEAF_NAME, std::string(), &s); return s; }
    std::string GetBlock() const { std::string s; this->Op(EE_WON_REGKEY_BLOCK, std::string(), &s); return s; } // was mBlock
    EeWonRegKey* mKey;
private:
    RegKey(const RegKey&);
    RegKey& operator=(const RegKey&);
    static const uint8_t* B(const std::string& inText) { return reinterpret_cast<const uint8_t*>(inText.data()); }
    static void Sink(void* inText, const uint8_t* inBytes, size_t inLength)
    { static_cast<std::string*>(inText)->assign(reinterpret_cast<const char*>(inBytes), inLength); }
    int Op(int inWhich, const std::string& inName, std::string* outText = NULL, long long* ioNum = NULL,
           const uint8_t* inData = NULL, size_t inLength = 0) const
    { return ee_won_regkey_op(this->mKey, inWhich, B(inName), inName.size(), inData, inLength, ioNum, outText ? Sink : NULL, outText); }
};

}
