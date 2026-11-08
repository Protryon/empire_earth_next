// RegKey: a Windows registry key wrapper (open/create, typed queries, delete)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyConfig.h"
#include "ee/wonapi/WONBuffer.h"
namespace WONAPI {
// 0x6a4260 (emitted from the list below): the original leaves mKey unset before OpenNewKey
RegKey::RegKey(int root,const std::string& path,bool create,bool nonvolatile)
    :mIsOpen(false),mBuffer(NULL),mBufferLength(0),mSubKeyIndex(0),mValueIndex(0)
{
    this->OpenNewKey(root,path,create,nonvolatile);
}
// FUNCTION: WON 0x6a42e0
// Destroys the key wrapper, closing an open key and freeing the value buffer
RegKey::~RegKey()
{
    if(this->mIsOpen)
        RegCloseKey(this->mKey);
    delete [] this->mBuffer;
}
// FUNCTION: WON 0x6a4370
// Closes the key if it is open
void RegKey::CloseKey()
{
    if(this->mIsOpen) {
        RegCloseKey(this->mKey);
        this->mIsOpen=false;
    }
}
// outLength is the first query's size, not the second's
// FUNCTION: WON 0x6a4390
// Reads the named value into the growable buffer; returns its registry type, 0 when it cannot be read
DWORD RegKey::GetToBuf(const std::string& name,unsigned long& outLength) const
{
    DWORD type;
    DWORD length;
    if(RegQueryValueExA(this->mKey,name.c_str(),NULL,&type,NULL,&length)!=ERROR_SUCCESS)
        return 0;
    outLength=length;
    if(this->mBufferLength<length) {
        delete [] this->mBuffer;
        this->mBufferLength=length;
        this->mBuffer=new char[length];
    }
    if(RegQueryValueExA(this->mKey,name.c_str(),NULL,&type,
                       reinterpret_cast<BYTE*>(this->mBuffer),&length)==ERROR_SUCCESS)
        return type;
    return 0;
}
// FUNCTION: WON 0x6a4680
// Reads a DWORD value; returns 0 on success, 2 for a wrong type, 1 when missing (value is 0 then)
int RegKey::QueryDWORD(const std::string& name,DWORD& value)
{
    value=0;
    if(!this->mIsOpen)
        return 1;
    unsigned long length;
    DWORD type=this->GetToBuf(name,length);
    if(type!=0) {
        if(type!=REG_DWORD)
            return 2;
        value=*reinterpret_cast<DWORD*>(this->mBuffer);
        return 0;
    }
    return 1;
}
// An empty binary value returns 4 (no size) and leaves the caller's buffer alone
// FUNCTION: WON 0x6a46e0
// Reads a binary value into a new byte buffer; returns 0, 2 wrong type, 4 empty or 1 missing
int RegKey::QueryBinary(const std::string& name,ConstSmartPtr<ByteBuffer>& value)
{
    if(!this->mIsOpen)
        return 1;
    unsigned long length=0;
    DWORD type=this->GetToBuf(name,length);
    if(type!=0) {
        if(type!=REG_BINARY)
            return 2;
        if(length!=0) {
            value=new ByteBuffer(this->mBuffer,length,false);
            return 0;
        }
        return 4;
    }
    return 1;
}
// FUNCTION: WON 0x6a4830
// Deletes the named subkey of an open key; returns whether it was deleted
bool RegKey::DeleteSubKey(const std::string& key)
{
    if(key.length()<=0 || !this->mIsOpen)
        return false;
    return RegDeleteKeyA(this->mKey,key.c_str())==ERROR_SUCCESS;
}
// The child is closed only when this parent is open
// FUNCTION: WON 0x6a4870
// Closes the given child key and deletes it by its path; returns whether it was deleted
bool RegKey::DeleteSubKey(RegKey& key)
{
    if(this->mIsOpen) {
        if(key.mIsOpen)
            key.CloseKey();
        return RegDeleteKeyA(this->mKey,key.mPath.c_str())==ERROR_SUCCESS;
    }
    return false;
}
}

namespace WONAPI {
// FUNCTION: WON 0x006a41d0
// Maps a root index (0 classes root .. 6 dyn data) to its predefined HKEY; NULL when out of range
HKEY RegKey::GetRootHKey(int root)
{
    switch(root) {
    case 0: return HKEY_CLASSES_ROOT;
    case 1: return HKEY_CURRENT_USER;
    case 2: return HKEY_LOCAL_MACHINE;
    case 3: return HKEY_USERS;
    case 4: return HKEY_PERFORMANCE_DATA;
    case 5: return HKEY_CURRENT_CONFIG;
    case 6: return HKEY_DYN_DATA;
    }
    return NULL;
}
// The root is passed as int
// FUNCTION: WON 0x006a4440
// Opens or creates the key under the root given by index
bool RegKey::OpenNewKey(int root,const std::string& path,bool create,bool nonvolatile)
{
    return this->OpenNewKey(this->GetRootHKey(root),path,create,nonvolatile);
}
// FUNCTION: WON 0x006a4470
// Closes the current key and opens or creates the path under root, remembering the path; returns success
bool RegKey::OpenNewKey(HKEY root,const std::string& path,bool create,bool nonvolatile)
{
    this->CloseKey();
    if(path.size()>0) {
        if(create) {
            char aClass;
            DWORD wasCreated;
            DWORD option=nonvolatile?REG_OPTION_NON_VOLATILE:REG_OPTION_VOLATILE;
            this->mIsOpen=(RegCreateKeyExA(root,path.c_str(),NULL,&aClass,option,
                        KEY_ALL_ACCESS,NULL,&this->mKey,&wasCreated)==ERROR_SUCCESS);
        }
        else
            this->mIsOpen=(RegOpenKeyExA(root,path.c_str(),NULL,KEY_ALL_ACCESS,&this->mKey)==ERROR_SUCCESS);
        if(this->mIsOpen) this->mPath=path;
    }
    return this->mIsOpen;
}
}

namespace WONAPI {
using namespace std;

// No original address identified: WON SDK registry method
bool RegKey::SetValue(const string& theName, const string& theValue){

	if(!this->IsOpen())
		return false;

	if(RegSetValueEx(this->mKey, theName.c_str(), NULL, REG_SZ,
	                  reinterpret_cast<CONST BYTE*>(theValue.c_str()),
	                  (theValue.size()+1)) == ERROR_SUCCESS)
	{
		return true;

	}
	else
		return false;

}

// No original address identified: WON SDK registry method
bool RegKey::SetValue(const string& theName, unsigned long theValue)
{
	if(!this->IsOpen())
		return false;
	
	if(RegSetValueEx(this->mKey, theName.c_str(), NULL, REG_DWORD,
	                  reinterpret_cast<CONST BYTE*>(&theValue),
	                  sizeof(theValue)) == ERROR_SUCCESS)
	{
		return true;
	}
	else
		return false;

}

// No original address identified: WON SDK registry method
bool RegKey::SetValue(const std::string& theName, const void* theValueP, unsigned long theLength)
{
	if(!this->IsOpen())
		return false;

	if(RegSetValueEx(this->mKey, theName.c_str(), NULL, REG_BINARY,
	                 static_cast<const unsigned char*>(theValueP),
	                 theLength) == ERROR_SUCCESS)
	{
		return true;

	}
	else
		return false;
}

// No original address identified: WON SDK registry method
bool RegKey::DeleteValue(const string& theName)
{
	if(!this->IsOpen())
		return false;

	return (RegDeleteValue(this->mKey, theName.c_str()) == ERROR_SUCCESS);
}

// No original address identified: WON SDK registry method
bool RegKey::OpenSubKey(const std::string& theSubKey, bool createSubKey, bool nonVolatile)
{
	bool subkeyOpen(false);


	if(theSubKey.size() > 0 && this->mIsOpen)
	{
		HKEY aSubKey;
		
		if(createSubKey)
		{
			char aClass;
			DWORD wasCreated;
			DWORD anOptionValue((nonVolatile) ? REG_OPTION_NON_VOLATILE : REG_OPTION_VOLATILE);

			subkeyOpen = (RegCreateKeyEx(this->mKey, theSubKey.c_str(), NULL, &aClass, anOptionValue, KEY_ALL_ACCESS, NULL, &aSubKey, &wasCreated) == ERROR_SUCCESS);
		}
		else
			subkeyOpen = (RegOpenKeyEx(this->mKey, theSubKey.c_str(), NULL, KEY_ALL_ACCESS, &aSubKey) == ERROR_SUCCESS);

		if(subkeyOpen)
		{
			this->CloseKey();
			this->mPath = theSubKey;
			this->mKey = aSubKey;
			this->mIsOpen = subkeyOpen;
		}
	}


	return subkeyOpen;
}

// No original address identified: WON SDK registry method
bool RegKey::GetSubKey(const std::string& theSubKeyName, RegKey& theSubKey, bool createSubKey, bool nonVolatile) const 
{
	if(theSubKeyName.size() > 0 && this->mIsOpen)
		return theSubKey.OpenNewKey(this->mKey,theSubKeyName, createSubKey, nonVolatile);

	return false;
}

// No original address identified: WON SDK registry method
RegKey::GetResult RegKey::GetFirstSubKey(RegKey& theSubKey) const 
{
	this->mSubKeyIndex = 0;
	return ((this->GetNextSubKey(theSubKey) == NoMore) ? NotFound : Ok);

}

// No original address identified: WON SDK registry method
RegKey::GetResult RegKey::GetNextSubKey(RegKey& theSubKey) const 
{
	FILETIME aFileTime;
	char aNameBuffer[256];
	DWORD aNameLength(sizeof(aNameBuffer));
	GetResult ret(NoMore);

	if(RegEnumKeyEx(this->mKey, this->mSubKeyIndex, aNameBuffer, &aNameLength, 0, NULL, NULL, &aFileTime) != ERROR_NO_MORE_ITEMS)
	{
		this->mSubKeyIndex++;
		if(theSubKey.OpenNewKey(this->mKey,aNameBuffer))
			ret = Ok;
	}

	return ret;
}

// No original address identified: WON SDK registry method
RegKey::GetResult RegKey::GetFirstValueName(std::string& theName, DataType& theType) const 
{
	this->mValueIndex = 0;
	return ((this->GetNextValueName(theName, theType) == NoMore) ? NotFound : Ok);
}

// No original address identified: WON SDK registry method
RegKey::GetResult RegKey::GetNextValueName(std::string& theName, DataType& theType) const 
{
	char aNameBuffer[256];
	DWORD aNameLength(sizeof(aNameBuffer)), aDataType;
	GetResult ret(NoMore);

	if(RegEnumValue(this->mKey, this->mValueIndex, aNameBuffer, &aNameLength, 0, &aDataType, NULL, NULL) != ERROR_NO_MORE_ITEMS)
	{
		switch(aDataType)
		{
			case REG_DWORD:
				theType=Long;
				break;
			case REG_SZ:
				theType=String;
				break;
			case REG_BINARY:
				theType=Binary;
				break;
			default:
				theType=Other;
				break;
		}
		this->mValueIndex++;
		theName = aNameBuffer;
		ret = Ok;
	}

	return ret;
}
}

namespace WONAPI {
using namespace std;
// No original address identified: WON SDK string value reader
RegKey::GetResult RegKey::GetValue(const string& theName, string& theValR) const
{
	GetResult ret = NotFound;

	theValR.erase();
	if(!this->IsOpen())
		return NotFound;

	unsigned long aLength;
	switch (this->GetToBuf(theName, aLength))
	{
		case REG_SZ:
			theValR = reinterpret_cast<char *>(this->mBuffer);
			ret = Ok;
			break;
		case REG_NONE:
			ret = NotFound;
			break;
		default:
			ret = BadType;
			break;
	}

	return ret;
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006a4240
// (public: virtual void * __thiscall WONAPI::RegKey::`scalar deleting destructor'(unsigned int)    [??_GRegKey@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x006a4260
// (public: __thiscall WONAPI::RegKey::RegKey(int,class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> > const &,bool,boo)
