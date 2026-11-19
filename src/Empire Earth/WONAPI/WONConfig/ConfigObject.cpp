// ConfigObject's host: the calls ee_won's config group (rust/crates/ee_won_net/src/cfgobject.rs, the whole class
// since WON round 21) makes into the C++ object, its typed values, its group pointers and its parsers, and the
// exceptions a hook lets through. The typed values (ConfigValue.cpp merged here) bind to the subclasses' members.
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <stddef.h>
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
static_assert(sizeof(StringParser) == sizeof(EeWonCfgStringParser), "the Rust view of a StringParser");
static_assert(offsetof(StringParser, mPos) == offsetof(EeWonCfgStringParser, pos), "the cursor");
static_assert(offsetof(StringParser, mWide) == offsetof(EeWonCfgStringParser, wide), "the unit flag");

bool ConfigObject::Ret(int r, CfgExc& x)
{
    if (r == -1) throw ConfigObjectException(x.mText, x.mFlag);
    if (r == -2) throw FileReaderException(x.mText);
    return r != 0;
}

intptr_t ConfigObject::Host(EeWonCfgoOp op, void* o, intptr_t a, intptr_t b, intptr_t c, intptr_t d, void* x)
{
    typedef EeWonCfgoOp Op;
    ConfigObject* obj = static_cast<ConfigObject*>(o);
    ConfigValueBase* v = static_cast<ConfigValueBase*>(o);
    CfgExc* e = static_cast<CfgExc*>(x);
    switch (op) {
    case Op::ObjAddRef: obj->AddRef(); return 0;
    case Op::ObjRelease: obj->Release(); return 0;
    case Op::ObjNew: return (intptr_t)new ConfigObject;
    case Op::ObjFactory: return (intptr_t)((ConfigObjectFactory)a)();
    case Op::ObjRust: return (intptr_t)obj->mObj;
    case Op::ObjSetParent: obj->mParent = (ConfigObject*)a; return 0;
    case Op::ObjSetName: (a ? obj->mBaseName : obj->mName).assign((const char*)b, (size_t)c); return 0;
    case Op::ValAddRef: v->AddRef(); return 0;
    case Op::ValRelease: v->Release(); return 0;
    case Op::ValInit: v->InitializeDefaultHook(); return 0;
    case Op::ValRead: return v->ReadValue(*(StringParser*)a);
    case Op::ValWrite: { std::string s; v->WriteValue(s); ((EeWonByteSink)a)((void*)b, B(s), s.length()); return 0; }
    case Op::ValCopy: v->CopyValue((ConfigValueBase*)a); return 0;
    case Op::ValIsDefault: return v->mIsDefault;
    case Op::ValSetDefault: v->mIsDefault = a != 0; return 0;
    case Op::GroupSet: *(SmartPtr<ConfigObject>*)a = (ConfigObject*)b; return 0;
    case Op::ParserNew: return (intptr_t)new ConfigParser;
    case Op::ParserFree: delete (ConfigParser*)a; return 0;
    case Op::ParserRust: return (intptr_t)((ConfigParser*)a)->Obj();
    case Op::SetExc: e->mKind = (int)a; e->mFlag = b != 0; e->mText.assign((const char*)c, (size_t)d); return 0;
    case Op::TakeExc: {
        ((EeWonByteSink)a)((void*)b, B(e->mText), e->mText.length());
        bool f = e->mFlag;
        e->mKind = 0;
        e->mText.erase();
        return f;
    }
    default: break;
    }
    // the hooks: what the original's frames let through is recorded for the Rust caller
    try {
        switch (op) {
        case Op::ObjPreParse: return obj->PreParse((ConfigParser*)a);
        case Op::ObjPostParse: return obj->PostParse((ConfigParser*)a);
        case Op::ObjHandleInstruction: return obj->HandleInstruction((ConfigParser*)a, std::string((const char*)b, (size_t)c));
        case Op::ObjHandleValueInstruction:
            return obj->HandleValueInstruction((ConfigParser*)a, std::string((const char*)b, (size_t)c), *(StringParser*)d);
        case Op::ObjReset: obj->Reset(a != 0); return 0;
        default: return 0;
        }
    } catch (ConfigObjectException& ex) {
        e->mKind = 1; e->mFlag = ex.mFlag; e->mText = ex.mText;
        return -1;
    } catch (FileReaderException& ex) {
        e->mKind = 2; e->mFlag = false; e->mText = ex.mDescription;
        return -2;
    }
}

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
template<> bool SimpleConfigValue<unsigned short>::ReadValue(StringParser& parser)
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
// Registers a key bound to the given variable with a default (a string list also with an additive flag)
void ConfigObject::KeyAdd(const char* key,bool& value,bool initial) {this->RegisterKeyValue(key,new SimpleConfigValue<bool>(value,initial));}
void ConfigObject::KeyAdd(const char* key,unsigned short& value,unsigned short initial) {this->RegisterKeyValue(key,new SimpleConfigValue<unsigned short>(value,initial));}
void ConfigObject::KeyAdd(const char* key,std::string& value,const char* initial) {this->RegisterKeyValue(key,new ConfigStringValue<char>(value,initial));}
void ConfigObject::KeyAdd(const char* key,std::wstring& value,const wchar_t* initial) {this->RegisterKeyValue(key,new ConfigStringValue<wchar_t>(value,initial));}
void ConfigObject::KeyAdd(const char* key,std::list<std::string>& value,const char* initial,bool additive) {this->RegisterKeyValue(key,new ConfigListValue<std::list<std::string> >(value,initial,additive));}
// No original address identified: WON SDK typed key registration (TitleConfig's two, before WON round 22; the drivers')
void ConfigObject::RegisterIntValue(const char *theKey, int &theInt, int theDefVal)
{ this->RegisterKeyValue(theKey, new SimpleConfigValue<int>(theInt,theDefVal)); }
void ConfigObject::RegisterIntListListValue(const char *theKey, std::list<std::list<int> > &theIntListList, const char *theDefVal, bool additive)
{ this->RegisterKeyValue(theKey, new ConfigListValue<std::list<std::list<int> > >(theIntListList, theDefVal, additive)); }
}
