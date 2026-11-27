// ConfigObject's host: the calls ee_won's config group (rust/crates/ee_won_net/src/cfgobject.rs, the whole class
// since WON round 21) makes into the C++ object, its typed values, its group pointers and its parsers, and the
// exceptions a hook lets through. The typed values (ConfigValue.cpp, merged here until R6 WON lobby and network
// round 35) are Rust objects (ee_won_lobby cfgval.rs) bound to the subclasses' members; the containers' host below
// is their statements on the C++ targets.
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "ee/wonapi/WONLobbyConfig.h"
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
    EeWonCfgVal* v = static_cast<EeWonCfgVal*>(o);
    typedef EeWonCfgValCall VC;
    CfgExc* e = static_cast<CfgExc*>(x);
    switch (op) {
    case Op::ObjAddRef: obj->AddRef(); return 0;
    case Op::ObjRelease: obj->Release(); return 0;
    case Op::ObjNew: return (intptr_t)new ConfigObject;
    case Op::ObjFactory: return (intptr_t)((ConfigObjectFactory)a)();
    case Op::ObjRust: return (intptr_t)obj->mObj;
    case Op::ObjSetParent: obj->mParent = (ConfigObject*)a; return 0;
    case Op::ObjSetName: (a ? obj->mBaseName : obj->mName).assign((const char*)b, (size_t)c); return 0;
    case Op::ValAddRef: ee_won_cfgval_call(v, VC::AddRef, 0, 0); return 0;
    case Op::ValRelease: ee_won_cfgval_call(v, VC::Release, 0, 0); return 0;
    case Op::ValInit: ee_won_cfgval_call(v, VC::Init, 0, 0); return 0;
    case Op::ValRead: return ee_won_cfgval_call(v, VC::Read, a, 0);
    case Op::ValWrite: ee_won_cfgval_call(v, VC::Write, a, b); return 0;
    case Op::ValCopy: ee_won_cfgval_call(v, VC::Copy, a, 0); return 0;
    case Op::ValIsDefault: return ee_won_cfgval_call(v, VC::IsDefault, 0, 0);
    case Op::ValSetDefault: ee_won_cfgval_call(v, VC::SetDefault, a, 0); return 0;
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

// ---- the typed values' containers (cfgval.rs's host): one statement each on a C++ target
typedef std::list<std::string> CfgStrings;
typedef std::list<std::list<int> > CfgIntLists;
static intptr_t CfgValHost(EeWonCfgValOp op, void* t, intptr_t a, intptr_t b, intptr_t c)
{
    typedef EeWonCfgValOp O;
    std::string* s = static_cast<std::string*>(t);
    std::wstring* w = static_cast<std::wstring*>(t);
    CfgStrings* l = static_cast<CfgStrings*>(t);
    CfgIntLists* ll = static_cast<CfgIntLists*>(t);
    switch (op) {
    case O::StrSet: s->assign((const char*)a, (size_t)b); return 0;
    case O::StrSetC: *s = (const char*)a; return 0;
    case O::StrErase: s->erase(); return 0;
    case O::StrOut: ((EeWonByteSink)a)((void*)b, (const uint8_t*)s->data(), s->length()); return 0;
    case O::StrCopy: *s = *(const std::string*)a; return 0;
    case O::WStrSet: w->assign((const wchar_t*)a, (size_t)b); return 0;
    case O::WStrSetC: *w = (const wchar_t*)a; return 0;
    case O::WStrErase: w->erase(); return 0;
    case O::WStrOut: ((void (*)(void*, const void*, size_t))a)((void*)b, w->data(), w->length()); return 0;
    case O::WStrCopy: *w = *(const std::wstring*)a; return 0;
    case O::ListClear: l->clear(); return 0;
    case O::ListPush: l->push_back(std::string((const char*)a, (size_t)b)); return 0;
    case O::ListCount: return (intptr_t)l->size();
    case O::ListOut: ((EeWonByteSink)b)((void*)c, (const uint8_t*)std::next(l->begin(), a)->data(), std::next(l->begin(), a)->length()); return 0;
    case O::ListCopy: *l = *(const CfgStrings*)a; return 0;
    case O::LlClear: ll->clear(); return 0;
    case O::LlPushGroup: ll->push_back(std::list<int>()); return 0;
    case O::LlPushInt: ll->back().push_back((int)a); return 0;
    case O::LlCount: return (intptr_t)ll->size();
    case O::LlGroupCount: return (intptr_t)std::next(ll->begin(), a)->size();
    case O::LlGet: return *std::next(std::next(ll->begin(), a)->begin(), b);
    case O::LlCopy: *ll = *(const CfgIntLists*)a; return 0;
    }
    return 0;
}
static const bool sCfgValHostSet = (ee_won_cfgval_set_host(CfgValHost), true);
// Registers a key bound to the given variable with a default (a string list also with an additive flag)
void ConfigObject::KeyAdd(const char* key,bool& value,bool initial) {this->RegisterKeyValue(key,ee_won_cfgval_bool(&value,initial));}
void ConfigObject::KeyAdd(const char* key,unsigned short& value,unsigned short initial) {this->RegisterKeyValue(key,ee_won_cfgval_u16(&value,initial));}
void ConfigObject::KeyAdd(const char* key,std::string& value,const char* initial) {this->RegisterKeyValue(key,ee_won_cfgval_string(&value,(const uint8_t*)initial));}
void ConfigObject::KeyAdd(const char* key,std::wstring& value,const wchar_t* initial) {this->RegisterKeyValue(key,ee_won_cfgval_wstring(&value,initial));}
void ConfigObject::KeyAdd(const char* key,std::list<std::string>& value,const char* initial,bool additive) {this->RegisterKeyValue(key,ee_won_cfgval_string_list(&value,initial,false,additive));}
// No original address identified: WON SDK typed key registration (TitleConfig's two, before WON round 22; the drivers')
void ConfigObject::RegisterIntValue(const char *theKey, int &theInt, int theDefVal)
{ this->RegisterKeyValue(theKey, ee_won_cfgval_int(&theInt,theDefVal)); }
void ConfigObject::RegisterIntListListValue(const char *theKey, std::list<std::list<int> > &theIntListList, const char *theDefVal, bool additive)
{ this->RegisterKeyValue(theKey, ee_won_cfgval_int_list_list(&theIntListList, theDefVal, false, additive)); }
}
