// The typed config values as they were before R6 WON lobby and network round 35 moved them into Rust (ee_won_lobby
// cfgval.rs): ConfigValueBase (ee/wonapi/WONConfigObject.h), the templates (ee/wonapi/WONConfigValue.h) and their
// bodies (WONConfig/ConfigObject.cpp, 207fb805f). For the drivers only: won_cfgobject_orig.h's OrigConfigObject
// holds these, so won_cfgobject_test runs them against the Rust values the game's ConfigObject holds.
#ifndef EE_TESTS_WON_CFGVALUE_ORIG_H
#define EE_TESTS_WON_CFGVALUE_ORIG_H
#include "ee/wonapi/WONLobbyConfig.h"
namespace WONAPI {
// 12-byte abstract base; ConfigObject uses Initialize/Read/Write and the default flag.
class ConfigValueBase : public RefCount {
public:
    virtual void InitializeDefaultHook()=0; //slot04
    virtual bool CopyValueHook(ConfigValueBase*)=0; //slot08
    virtual bool ReadValue(StringParser&)=0; //slot0c
    virtual void WriteValue(std::string&)=0; //slot10
    void CopyValue(ConfigValueBase* source) { if(this->CopyValueHook(source)) this->mIsDefault=source->mIsDefault; } //SDK inline; keeps the default flag only after a successful typed copy
    ConfigValueBase():mIsDefault(false) {}
    bool mIsDefault; //08; 3aed47/3b1149/3af2de, 09..0b padding
};

}
// WON SDK ConfigValue template hierarchy: 12/16/20/24-byte classes and five slots.
// Requires RefCount/StringParser/ConfigValueBase included before it.
namespace WONAPI {
template<class T> class ConfigValue : public ConfigValueBase {
protected:
    T& mVal; //0c, borrowed target; ConfigValue size 16
    virtual bool CopyValueHook(ConfigValueBase*);
public:
    ConfigValue(T& value):mVal(value) {}
};
template<class T> class SimpleConfigValue : public ConfigValue<T> {
protected:
    T mDefVal; //10, size 20 for bool and unsigned short
    virtual void InitializeDefaultHook();
public:
    SimpleConfigValue(T& value,T initial):ConfigValue<T>(value),mDefVal(initial) {}
    virtual bool ReadValue(StringParser&);
    virtual void WriteValue(std::string&);
};
template<class T> class ConfigStringValue : public ConfigValue<std::basic_string<T> > {
protected:
    const T* mDefVal; //10, borrowed; size 20
    virtual void InitializeDefaultHook();
public:
    ConfigStringValue(std::basic_string<T>& value,const T* initial):ConfigValue<std::basic_string<T> >(value),mDefVal(initial) {}
    virtual bool ReadValue(StringParser&);
    virtual void WriteValue(std::string&);
};
template<class T> class ConfigListValue : public ConfigValue<T> {
protected:
    const char* mDefStr; //10, borrowed narrow/wide character view
    bool mIsWString; //14
    bool mAdditive; //15, size 24
    virtual void InitializeDefaultHook();
public:
    ConfigListValue(T& value,const char* initial,bool additive):ConfigValue<T>(value),mDefStr(initial),mIsWString(false),mAdditive(additive) {}
    ConfigListValue(T& value,const wchar_t* initial,bool additive):ConfigValue<T>(value),mDefStr(reinterpret_cast<const char*>(initial)),mIsWString(true),mAdditive(additive) {}
    virtual bool ReadValue(StringParser&);
    virtual void WriteValue(std::string&);
};
}namespace WONAPI {
// ---- the typed values (ConfigValue.cpp). Instantiations: bool 0x7b3f70, unsigned short 0x7b4000, string 0x7b41a0,
// wstring 0x7b4480, string list 0x7b47b0
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
// The original assigned the int even when the read failed (uninitialised then); the value is kept now, as the
// parser's sized reads do (PORT_LEARNINGS "R6: ... the chat parser, GUIStringData, ConfigObject's grammar")
template<> inline bool SimpleConfigValue<unsigned short>::ReadValue(StringParser& parser)
{
    int value=0;
    bool result=parser.ReadValue(value);
    if(result)
        this->mVal=(unsigned short)value;
    return result;
}
template<class T> void SimpleConfigValue<T>::WriteValue(std::string& out) {StringParser::ValueToString(this->mVal,out);}
// Instantiations: char 0x7b4040, wchar_t 0x7b4320
// Resets the string to its default text, or empties it when there is none
template<class T> void ConfigStringValue<T>::InitializeDefaultHook()
{
    if(this->mDefVal!=NULL) this->mVal=this->mDefVal;
    else this->mVal.erase();
}
template<class T> bool ConfigStringValue<T>::ReadValue(StringParser& parser) {return parser.ReadString(this->mVal,false);}
template<class T> void ConfigStringValue<T>::WriteValue(std::string& out) {StringParser::ValueToString(this->mVal,out);}
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
// Parses list items, replacing the list unless the key is additive
template<class T> bool ConfigListValue<T>::ReadValue(StringParser& parser)
{
    if(!this->mAdditive) this->mVal.clear();
    return parser.ReadValue(this->mVal);
}
template<class T> void ConfigListValue<T>::WriteValue(std::string& out) {StringParser::ValueToString(this->mVal,out);}
}
#endif
