#pragma once
#include "ee/wonapi/WONLobbyConfig.h"
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
}