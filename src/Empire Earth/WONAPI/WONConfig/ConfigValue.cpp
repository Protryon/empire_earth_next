// Typed config values (bool, 16-bit, string, wide string, string list) and their key registration
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
#include "ee/wonapi/WONConfigValue.h"
namespace WONAPI {
// Instantiations: bool 0x7b3f70, unsigned short 0x7b4000, string 0x7b41a0, wstring 0x7b4480,
// string list 0x7b47b0
// FUNCTION: WON 0x007b3f70
// FUNCTION: WON 0x007b4000
// FUNCTION: WON 0x007b41a0
// FUNCTION: WON 0x007b4480
// FUNCTION: WON 0x007b47b0
// Copies the value from another config value of the same type; false when the type differs
template<class T> bool ConfigValue<T>::CopyValueHook(ConfigValueBase* source)
{
    ConfigValue<T>* value=dynamic_cast<ConfigValue<T>*>(source);
    if(value!=NULL) {
        this->mVal=value->mVal;
        return true;
    }
    return false;
}
template<class T> void SimpleConfigValue<T>::InitializeDefaultHook() {this->mVal=this->mDefVal;}
template<class T> bool SimpleConfigValue<T>::ReadValue(StringParser& parser) {return parser.ReadValue(this->mVal);}
template<> bool SimpleConfigValue<unsigned short>::ReadValue(StringParser& parser)
{
    int value;
    bool result=parser.ReadValue(value);
    this->mVal=(unsigned short)value;
    return result;
}
template<class T> void SimpleConfigValue<T>::WriteValue(std::string& out) {StringParser::ValueToString(this->mVal,out);}
// Instantiations: char 0x7b4040, wchar_t 0x7b4320
// FUNCTION: WON 0x007b4040
// FUNCTION: WON 0x007b4320
// Resets the string to its default text, or empties it when there is none
template<class T> void ConfigStringValue<T>::InitializeDefaultHook()
{
    if(this->mDefVal!=NULL) this->mVal=this->mDefVal;
    else this->mVal.erase();
}
template<class T> bool ConfigStringValue<T>::ReadValue(StringParser& parser) {return parser.ReadString(this->mVal,false);}
template<class T> void ConfigStringValue<T>::WriteValue(std::string& out) {StringParser::ValueToString(this->mVal,out);}
// FUNCTION: WON 0x007b4610
// Resets the list to the items parsed from its default text
template<class T> void ConfigListValue<T>::InitializeDefaultHook()
{
    this->mVal.clear();
    if(this->mDefStr!=NULL) {
        StringParser parser(this->mDefStr);
        parser.SetIsWString(this->mIsWString);
        parser.ReadValue(this->mVal);
    }
}
// FUNCTION: WON 0x007b46b0
// Parses list items, replacing the list unless the key is additive
template<class T> bool ConfigListValue<T>::ReadValue(StringParser& parser)
{
    if(!this->mAdditive) this->mVal.clear();
    return parser.ReadValue(this->mVal);
}
template<class T> void ConfigListValue<T>::WriteValue(std::string& out) {StringParser::ValueToString(this->mVal,out);}
// FUNCTION: WON 0x007b1b40
// Registers a bool key bound to the given variable with a default
void ConfigObject::KeyAdd(const char* key,bool& value,bool initial) {this->RegisterKeyValue(key,new SimpleConfigValue<bool>(value,initial));}
// FUNCTION: WON 0x007b1ba0
// Registers a 16-bit key bound to the given variable with a default
void ConfigObject::KeyAdd(const char* key,unsigned short& value,unsigned short initial) {this->RegisterKeyValue(key,new SimpleConfigValue<unsigned short>(value,initial));}
// FUNCTION: WON 0x007b1c00
// Registers a string key bound to the given variable with a default
void ConfigObject::KeyAdd(const char* key,std::string& value,const char* initial) {this->RegisterKeyValue(key,new ConfigStringValue<char>(value,initial));}
// FUNCTION: WON 0x007b1c60
// Registers a wide string key bound to the given variable with a default
void ConfigObject::KeyAdd(const char* key,std::wstring& value,const wchar_t* initial) {this->RegisterKeyValue(key,new ConfigStringValue<wchar_t>(value,initial));}
// FUNCTION: WON 0x007b1cc0
// Registers a string list key bound to the given list, with a default and an additive flag
void ConfigObject::KeyAdd(const char* key,std::list<std::string>& value,const char* initial,bool additive) {this->RegisterKeyValue(key,new ConfigListValue<std::list<std::string> >(value,initial,additive));}
}
namespace WONAPI {
// No original address identified: WON SDK typed key registration
void ConfigObject::RegisterInt64Value(const char *theKey, __int64 &theInt64, __int64 theDefVal)
{
	this->RegisterKeyValue(theKey, new SimpleConfigValue<__int64>(theInt64,theDefVal));
}

void ConfigObject::RegisterIntValue(const char *theKey, int &theInt, int theDefVal)
{
	this->RegisterKeyValue(theKey, new SimpleConfigValue<int>(theInt,theDefVal));
}

void ConfigObject::RegisterLongValue(const char *theKey, long &theLong, long theDefVal)
{
	this->RegisterKeyValue(theKey, new SimpleConfigValue<long>(theLong, theDefVal));
}

void ConfigObject::RegisterShortValue(const char *theKey, short &theShort, short theDefVal)
{
	this->RegisterKeyValue(theKey, new SimpleConfigValue<short>(theShort, theDefVal));
}

void ConfigObject::RegisterByteValue(const char *theKey, char &theByte, char theDefVal)
{
	this->RegisterKeyValue(theKey, new SimpleConfigValue<char>(theByte,theDefVal));
}

void ConfigObject::RegisterUInt64Value(const char *theKey, unsigned __int64 &theInt64, unsigned __int64 theDefVal)
{
	this->RegisterKeyValue(theKey, new SimpleConfigValue<unsigned __int64>(theInt64,theDefVal));
}

void ConfigObject::RegisterBoolListValue(const char *theKey, std::list<bool> &theBoolList, const char *theDefVal, bool additive)
{
	this->RegisterKeyValue(theKey, new ConfigListValue<std::list<bool> >(theBoolList, theDefVal, additive));
}

void ConfigObject::RegisterIntListValue(const char *theKey, std::list<int> &theIntList, const char *theDefVal, bool additive)
{
	this->RegisterKeyValue(theKey, new ConfigListValue<std::list<int> >(theIntList, theDefVal, additive));
}

void ConfigObject::RegisterWStringListValue(const char *theKey, std::list<std::wstring> &theStringList, const wchar_t *theDefVal, bool additive)
{
	this->RegisterKeyValue(theKey, new ConfigListValue<std::list<std::wstring> >(theStringList, theDefVal, additive));
}

void ConfigObject::RegisterBoolListListValue(const char *theKey, std::list<std::list<bool> > &theBoolListList, const char *theDefVal, bool additive)
{
	this->RegisterKeyValue(theKey, new ConfigListValue<std::list<std::list<bool> > >(theBoolListList, theDefVal, additive));
}

void ConfigObject::RegisterIntListListValue(const char *theKey, std::list<std::list<int> > &theIntListList, const char *theDefVal, bool additive)
{
	this->RegisterKeyValue(theKey, new ConfigListValue<std::list<std::list<int> > >(theIntListList, theDefVal, additive));
}

void ConfigObject::RegisterStringListListValue(const char *theKey, std::list<std::list<std::string> > &theStringListList, const char *theDefVal, bool additive)
{
	this->RegisterKeyValue(theKey, new ConfigListValue<std::list<std::list<std::string> > >(theStringListList, theDefVal, additive));
}

void ConfigObject::RegisterWStringListListValue(const char *theKey, std::list<std::list<std::wstring> > &theStringListList, const wchar_t *theDefVal, bool additive)
{
	this->RegisterKeyValue(theKey, new ConfigListValue<std::list<std::list<std::wstring> > >(theStringListList, theDefVal, additive));
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007b4790
// (public: virtual void __thiscall WONAPI::ConfigListValue<class std::list<class std::basic_string<char,struct std::char_traits<char>,class std::allocato)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006aa710
// (public: void * __thiscall std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short> >::`scalar del)
// TEMPLATE: WON 0x006aa760
// (public: void * __thiscall std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >::`scalar deleting destructor'(unsigned int)
