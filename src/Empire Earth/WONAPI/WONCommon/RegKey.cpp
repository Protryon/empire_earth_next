// RegKey: the WON library's registry key wrapper (open/create, typed queries, delete), kept in config.yaml
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <stdint.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyConfig.h"
#include "ee/wonapi/WONBuffer.h"
#include "platform/PlatConfig.h"

// The port has no registry (the harness stops on any registry write, and POSIX has none): a key is the config.yaml
// block "won.<path>", the path's '\' (or '/') separators turned into the block separator '.'. HKEY_CURRENT_USER and
// HKEY_LOCAL_MACHINE share it (the root is dropped). A value is a key of that block: a DWORD an integer, a string a
// string, binary data a string of hex digits. Volatile keys persist like the others.
namespace {

// A name as one config.yaml key: no '.' (the block separator) or ':' and no blanks around it
std::string ConfigName(const std::string& inName)
{
    std::string theName;
    for (std::string::size_type i = 0; i < inName.length(); ++i) {
        unsigned char c = static_cast<unsigned char>(inName[i]);
        theName += (c == '.' || c == ':' || c < 0x20) ? '_' : static_cast<char>(c);
    }
    std::string::size_type a = theName.find_first_not_of(' ');
    if (a == std::string::npos)
        return "(default)";   // the key's unnamed value
    return theName.substr(a, theName.find_last_not_of(' ') - a + 1);
}

// The block of inPath (a registry path) under inParent
std::string ConfigBlock(const std::string& inParent, const std::string& inPath)
{
    std::string theBlock = inParent;
    std::string::size_type theStart = 0;
    while (theStart <= inPath.length()) {
        std::string::size_type theEnd = inPath.find_first_of("\\/", theStart);
        if (theEnd == std::string::npos)
            theEnd = inPath.length();
        if (theEnd > theStart)
            theBlock += "." + ConfigName(inPath.substr(theStart, theEnd - theStart));
        theStart = theEnd + 1;
    }
    return theBlock;
}

int HexDigit(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

}

namespace WONAPI {
// 0x6a4260 (emitted from the list below)
RegKey::RegKey(int root,const std::string& path,bool create,bool nonvolatile)
    :mIsOpen(false)
{
    this->OpenNewKey(root,path,create,nonvolatile);
}
// FUNCTION: WON 0x6a42e0
// Destroys the key wrapper
RegKey::~RegKey()
{
}
// FUNCTION: WON 0x6a4370
// Closes the key if it is open
void RegKey::CloseKey()
{
    this->mIsOpen=false;
}
// FUNCTION: WON 0x6a4390
// Reads the named value's text; false when it is missing
bool RegKey::GetToBuf(const std::string& name,std::string& outText) const
{
    std::vector<char> buffer(65536);
    if(!PlatConfigGetString(this->mBlock.c_str(),ConfigName(name).c_str(),&buffer[0],
                            static_cast<uint32_t>(buffer.size())))
        return false;
    outText=&buffer[0];
    return true;
}
// FUNCTION: WON 0x6a4680
// Reads a DWORD value; returns 0 on success, 2 for a wrong type, 1 when missing (value is 0 then)
int RegKey::QueryDWORD(const std::string& name,DWORD& value)
{
    value=0;
    if(!this->mIsOpen)
        return 1;
    int32_t number;
    if(PlatConfigGetInt(this->mBlock.c_str(),ConfigName(name).c_str(),&number)) {
        value=static_cast<DWORD>(number);
        return 0;
    }
    std::string text;
    return this->GetToBuf(name,text) ? 2 : 1;
}
// An empty binary value returns 4 (no size) and leaves the caller's buffer alone
// FUNCTION: WON 0x6a46e0
// Reads a binary value into a new byte buffer; returns 0, 2 wrong type, 4 empty or 1 missing
int RegKey::QueryBinary(const std::string& name,ConstSmartPtr<ByteBuffer>& value)
{
    if(!this->mIsOpen)
        return 1;
    std::string text;
    if(!this->GetToBuf(name,text))
        return 1;
    if(text.length()%2!=0)
        return 2;
    std::string bytes;
    for(std::string::size_type i=0;i<text.length();i+=2) {
        int high=HexDigit(text[i]),low=HexDigit(text[i+1]);
        if(high<0 || low<0)
            return 2;
        bytes+=static_cast<char>(high*16+low);
    }
    if(bytes.empty())
        return 4;
    value=new ByteBuffer(bytes.data(),bytes.length(),false);
    return 0;
}
// FUNCTION: WON 0x6a4830
// Deletes the named subkey of an open key; returns whether it was deleted
bool RegKey::DeleteSubKey(const std::string& key)
{
    if(key.length()<=0 || !this->mIsOpen)
        return false;
    return PlatConfigDelete(ConfigBlock(this->mBlock,key).c_str(),NULL)!=0;
}
// The child is closed only when this parent is open
// FUNCTION: WON 0x6a4870
// Closes the given child key and deletes it by its path; returns whether it was deleted
bool RegKey::DeleteSubKey(RegKey& key)
{
    if(this->mIsOpen) {
        if(key.mIsOpen)
            key.CloseKey();
        return PlatConfigDelete(ConfigBlock(this->mBlock,key.mPath).c_str(),NULL)!=0;
    }
    return false;
}
}

namespace WONAPI {
// The root is passed as int (0 classes root .. 6 dyn data); every root maps to the same "won" block
// FUNCTION: WON 0x006a4440
// Opens or creates the key under the root given by index
bool RegKey::OpenNewKey(int root,const std::string& path,bool create,bool nonvolatile)
{
    if(root<0 || root>6) {
        this->CloseKey();
        return false;
    }
    return this->OpenBlock("won",path);
}
// A block needs no creating (setting a value creates it): opening a missing key succeeds as creating it did
// FUNCTION: WON 0x006a4470
// Closes the current key and opens the path under the parent block, remembering the path; returns success
bool RegKey::OpenBlock(const std::string& parent,const std::string& path)
{
    this->CloseKey();
    if(path.size()>0) {
        this->mBlock=ConfigBlock(parent,path);
        this->mIsOpen=true;
        this->mPath=path;
    }
    return this->mIsOpen;
}
}

namespace WONAPI {
using namespace std;

// No original address identified: WON SDK registry method
bool RegKey::SetValue(const string& theName, const string& theValue)
{
	if(!this->IsOpen())
		return false;
	return PlatConfigSetString(this->mBlock.c_str(), ConfigName(theName).c_str(), theValue.c_str()) != 0;
}

// No original address identified: WON SDK registry method
bool RegKey::SetValue(const string& theName, unsigned long theValue)
{
	if(!this->IsOpen())
		return false;
	// a REG_DWORD: 32 bits
	return PlatConfigSetInt(this->mBlock.c_str(), ConfigName(theName).c_str(),
	                        static_cast<int32_t>(static_cast<uint32_t>(theValue))) != 0;
}

// No original address identified: WON SDK registry method
bool RegKey::SetValue(const std::string& theName, const void* theValueP, unsigned long theLength)
{
	if(!this->IsOpen())
		return false;

	static const char kHex[] = "0123456789abcdef";
	const unsigned char* aByteP = static_cast<const unsigned char*>(theValueP);
	string aText;
	for(unsigned long i = 0; i < theLength; i++)
	{
		aText += kHex[aByteP[i] >> 4];
		aText += kHex[aByteP[i] & 15];
	}
	return PlatConfigSetString(this->mBlock.c_str(), ConfigName(theName).c_str(), aText.c_str()) != 0;
}

// No original address identified: WON SDK registry method
bool RegKey::DeleteValue(const string& theName)
{
	if(!this->IsOpen())
		return false;

	return PlatConfigDelete(this->mBlock.c_str(), ConfigName(theName).c_str()) != 0;
}

// No original address identified: WON SDK registry method
bool RegKey::OpenSubKey(const std::string& theSubKey, bool createSubKey, bool nonVolatile)
{
	if(theSubKey.size() > 0 && this->mIsOpen)
	{
		this->mBlock = ConfigBlock(this->mBlock, theSubKey);
		this->mPath = theSubKey;
		return true;
	}
	return false;
}

// No original address identified: WON SDK registry method
bool RegKey::GetSubKey(const std::string& theSubKeyName, RegKey& theSubKey, bool createSubKey, bool nonVolatile) const
{
	if(theSubKeyName.size() > 0 && this->mIsOpen)
		return theSubKey.OpenBlock(this->mBlock, theSubKeyName);

	return false;
}

// No original address identified: WON SDK registry method
RegKey::GetResult RegKey::GetFirstSubKey(RegKey& theSubKey) const
{
	return ((this->GetNextSubKey(theSubKey) == NoMore) ? NotFound : Ok);
}

// config.yaml blocks are not enumerated: no more subkeys
// No original address identified: WON SDK registry method
RegKey::GetResult RegKey::GetNextSubKey(RegKey& theSubKey) const
{
	return NoMore;
}

// No original address identified: WON SDK registry method
RegKey::GetResult RegKey::GetFirstValueName(std::string& theName, DataType& theType) const
{
	return ((this->GetNextValueName(theName, theType) == NoMore) ? NotFound : Ok);
}

// config.yaml keys are not enumerated: no more values
// No original address identified: WON SDK registry method
RegKey::GetResult RegKey::GetNextValueName(std::string& theName, DataType& theType) const
{
	return NoMore;
}
}

namespace WONAPI {
using namespace std;
// Any value reads as a string (config.yaml keeps no types)
// No original address identified: WON SDK string value reader
RegKey::GetResult RegKey::GetValue(const string& theName, string& theValR) const
{
	theValR.erase();
	if(!this->IsOpen())
		return NotFound;

	return this->GetToBuf(theName, theValR) ? Ok : NotFound;
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006a4240
// (public: virtual void * __thiscall WONAPI::RegKey::`scalar deleting destructor'(unsigned int)    [??_GRegKey@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x006a4260
// (public: __thiscall WONAPI::RegKey::RegKey(int,class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> > const &,bool,boo)
