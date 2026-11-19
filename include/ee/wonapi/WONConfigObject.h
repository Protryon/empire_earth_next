// ConfigObject (0x54 bytes) and ConfigValueBase (12 bytes) declarations, moved from GUI.
#ifndef EE_WONAPI_WONCONFIGOBJECT_H
#define EE_WONAPI_WONCONFIGOBJECT_H
#include <string>
#include <list>
#include <map>
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONStringCompare.h"
#include "rust/won_net/cfgobject.h"
#include <string.h>
namespace WONAPI {
class ConfigParser;
class StringParser;
class ConfigObject;
typedef ConfigObject* (*ConfigObjectFactory)();
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

// a configuration object (0x54 bytes in the original: LobbyConfig's members started at +0x54). Its children, child
// iteration, registered key values and key groups and every method are ee_won's object (rust/crates/ee_won_net/src/
// cfgobject.rs; ConfigObject.cpp keeps only its host): these are the forwards. The class keeps its vtable (the
// subclasses override the eight hooks), its name, base name and parent (the subclasses read them) and the typed
// values (ConfigValueBase, bound to the subclasses' members). An exception a hook let through comes back in a
// CfgExc and is thrown again here, in the caller's frame, as the original's frames passed it on.
class ConfigObject : public RefCount {
public:
    struct CfgExc { int mKind; bool mFlag; std::string mText; CfgExc() : mKind(0), mFlag(false) {} };
    static void Sink(void* c, const uint8_t* b, size_t n) { static_cast<std::string*>(c)->append((const char*)b, n); }
    static const uint8_t* B(const std::string& s) { return (const uint8_t*)s.data(); }
    static bool Ret(int r, CfgExc& x); // throws x again (r < 0); defined with the host
    static intptr_t Host(EeWonCfgoOp, void*, intptr_t, intptr_t, intptr_t, intptr_t, void*);
    ConfigObject() : mParent(NULL) { this->mObj = ee_won_cfgo_new(this, &ConfigObject::Host); }
    virtual ~ConfigObject() { ee_won_cfgo_free(this->mObj); }
    // The original ConfigObject vtable has these eight slots after its implicit virtual destructor.
    virtual bool HandleInstruction(ConfigParser* p, const std::string& i)
    { CfgExc x; return Ret(ee_won_cfgo_handle_instruction(this->mObj, p, B(i), i.length(), &x), x); }
    virtual bool HandleValueInstruction(ConfigParser* p, const std::string& k, StringParser& v)
    { CfgExc x; return Ret(ee_won_cfgo_handle_value_instruction(this->mObj, p, B(k), k.length(), (EeWonCfgStringParser*)&v, &x), x); }
    virtual bool PreParse(ConfigParser*) { return true; } // the default hooks share one always-true function (0x7099f0)
    virtual bool PostParse(ConfigParser*) { return true; }
    virtual bool GetKeyValue(const std::string& k, std::string& out) { return ee_won_cfgo_get_key_value(this->mObj, B(k), k.length(), Sink, &out); }
    virtual void Reset(bool inResetName) { ee_won_cfgo_reset(this->mObj, inResetName); }
    virtual void GetAllKeyValues(std::string& out) { ee_won_cfgo_get_all_key_values(this->mObj, Sink, &out); }
    virtual int SetKeyValue(const std::string& k, StringParser& v) { return ee_won_cfgo_set_key_value(this->mObj, B(k), k.length(), (EeWonCfgStringParser*)&v); }
    void KeyAdd(const char* inKey, unsigned short& ioValue, unsigned short inDefault);
    void KeyAdd(const char* inKey, bool& ioValue, bool inDefault);
    void KeyAdd(const char* inKey, std::string& ioValue, const char* inDefault);
    void KeyAdd(const char* inKey, std::wstring& ioValue, const wchar_t* inDefault);
    void KeyAdd(const char* inKey, std::list<std::string>& ioValue, const char* inDefault, bool inFlag);
    void RegisterIntValue(const char *theKey, int &theInt, int theDefVal = 0);
    void RegisterIntListListValue(const char *theKey, std::list<std::list<int> > &theIntListList, const char *theDefVal = NULL, bool additive = false);

    void CopyKeyValsFrom(ConfigObject* f) { ee_won_cfgo_copy_key_vals_from(this->mObj, f->mObj); } // ordered, ignoring case
    ConfigObject* NextChild() { return (ConfigObject*)ee_won_cfgo_next_child(this->mObj); }
    void RegisterKeyGroup(const char* k, ConfigObjectFactory f, SmartPtr<ConfigObject>* g = NULL)
    { ee_won_cfgo_register_key_group(this->mObj, (const uint8_t*)k, strlen(k), (intptr_t)f, (intptr_t)g, NULL); }
    void RegisterKeyGroup(const char* k, ConfigObject* o)
    { ee_won_cfgo_register_key_group(this->mObj, (const uint8_t*)k, strlen(k), 0, 0, o); }
    void Clear() { ee_won_cfgo_clear(this->mObj); }
    void Rewind() { ee_won_cfgo_rewind(this->mObj); }
    void AddChild(ConfigObject* c) { ee_won_cfgo_add_child(this->mObj, c); }
    size_t GetChildCount() { return ee_won_cfgo_child_count(this->mObj); }
    bool ReadGroupStart(ConfigParser& p, std::string& n, std::string& b)
    { return ee_won_cfgo_read_group_start(this->mObj, &p, Assign, &n, Assign, &b); }
    bool ParseTopLevel(ConfigParser& p, const char* f)
    { CfgExc x; return Ret(ee_won_cfgo_parse_top_level(this->mObj, &p, (const uint8_t*)f, f ? strlen(f) : 0, &x), x); }
    bool ParseGroup(ConfigParser& p, bool c = true, bool s = false, bool pp = true, bool r = true)
    { CfgExc x; return Ret(ee_won_cfgo_parse_group(this->mObj, &p, c, s, pp, r, &x), x); }
    bool SkipGroup(ConfigParser& p, bool c = true, bool r = true)
    { CfgExc x; return Ret(ee_won_cfgo_skip_group(this->mObj, &p, c, r, &x), x); }
    bool IncludeFile(ConfigParser& p, const std::string& f, bool r = true)
    { CfgExc x; return Ret(ee_won_cfgo_include(this->mObj, &p, false, B(f), f.length(), r, &x), x); }
    bool IncludeGroup(ConfigParser& p, const std::string& g, bool r = true)
    { CfgExc x; return Ret(ee_won_cfgo_include(this->mObj, &p, true, B(g), g.length(), r, &x), x); }
    bool ParseChild(ConfigParser& p, ConfigObject* c, bool a = true, bool r = true)
    { CfgExc x; return Ret(ee_won_cfgo_parse_child(this->mObj, &p, c, a, r, &x), x); }
    void RegisterKeyValue(const char* k, ConfigValueBase* v) { ee_won_cfgo_register_key_value(this->mObj, (const uint8_t*)k, strlen(k), v); }

    std::string mName;                      // +0x18
    std::string mBaseName;                  // +0x28
    ConfigObject* GetParentObject() { return this->mParent; }
    ConfigObject* mParent;                           // +0x38
    EeWonCfgObject* mObj;
private:
    static void Assign(void* c, const uint8_t* b, size_t n) { static_cast<std::string*>(c)->assign((const char*)b, n); }
    ConfigObject(const ConfigObject&);
    ConfigObject& operator=(const ConfigObject&);
};
}
#endif
