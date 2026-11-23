// The stand-in world of won_cfgload34_test.cpp, included once in each of its namespaces (Orig: ReadConfig_Unsafe as
// it was, won_cfgload34_orig.h; Port: the game's include/ee/wonapi/WONCfgLoad.inl over ee_won_gui cfgload.rs). Every
// call into the world is traced; the run's script (the shared `gScript`) says what each call answers or throws.
typedef std::string GUIString;
static std::string sTrace;
static void tr(const std::string& s) { sTrace += s; sTrace += '\n'; }
static std::string P(long long v) { char b[32]; snprintf(b, sizeof b, "%lld", v); return b; }

struct ConfigObjectException { std::string mText; bool mFlag; ConfigObjectException(const std::string& s, bool f = true) : mText(s), mFlag(f) {} };
struct OtherException { int mCode; }; // an exception the load does not catch

// the live objects by kind and id
static std::map<std::string, int>& Live() { static std::map<std::string, int> m; return m; }
static int sNextId;
static std::string Census()
{
    std::string s;
    for (std::map<std::string, int>::iterator i = Live().begin(); i != Live().end(); ++i) s += " " + i->first + "=" + P(i->second);
    return s;
}
struct RefCount {
    int mRefCount = 0; int mId; std::string mKind;
    explicit RefCount(const char* k) : mId(++sNextId), mKind(k) { Live()[Name()] = 0; tr("new " + Name()); }
    virtual ~RefCount() { Live().erase(Name()); tr("delete " + Name()); }
    std::string Name() const { return mKind + P(mId); }
    void AddRef() { ++mRefCount; Live()[Name()] = mRefCount; tr("addref " + Name() + " " + P(mRefCount)); }
    void Release() { --mRefCount; tr("release " + Name() + " " + P(mRefCount)); if (mRefCount <= 0) delete this; else Live()[Name()] = mRefCount; }
};
template<class T> struct SmartPtr {
    T* mPtr;
    SmartPtr(T* p = NULL) : mPtr(p) { if (p) p->AddRef(); }
    SmartPtr(const SmartPtr& o) : mPtr(o.mPtr) { if (mPtr) mPtr->AddRef(); }
    ~SmartPtr() { if (mPtr) mPtr->Release(); }
    SmartPtr& operator=(T* p) { if (p) p->AddRef(); T* o = mPtr; mPtr = p; if (o) o->Release(); return *this; }
    SmartPtr& operator=(const SmartPtr& s) { return *this = s.mPtr; }
    T* get() const { return mPtr; }
    T* operator->() const { return mPtr; }
};
static std::string PP(const RefCount* o) { return o ? o->Name() : "null"; }

// what one load meets (the same for both sides)
struct Script {
    int setComponent;   // 0 binds, 1 throws ConfigObjectException, 2 throws OtherException
    int parse;          // 0 false, 1 true, 2 throws ConfigObjectException, 3 throws OtherException
    bool parseShows;    // the parse sets gShowParseTime (a SHOWPARSETIME instruction)
    int lines;          // and gLinesParsed
    int glue, init;     // 0 fine, 1 ConfigObjectException, 2 OtherException
    int box;            // 0 shown, 1 throws OtherException
    double elapsed, counted, percent;
};
static Script gScript;

static void Throw(int k, const std::string& what)
{
    if (k == 1) throw ConfigObjectException(what + " failed");
    if (k >= 2) throw OtherException{(int)what.size()};
}
struct Component : RefCount { Component() : RefCount("comp") {} };
class ResourceConfigTable : public RefCount {
public:
    ResourceConfigTable() : RefCount("table") {}
    void SafeGlueResources() { tr("glue " + Name()); Throw(gScript.glue, "glue"); }
};
class ConfigParser {
public:
    int mId;
    ConfigParser() : mId(++sNextId) { Live()["parser" + P(mId)] = 1; tr("new parser" + P(mId)); }
    ~ConfigParser() { Live().erase("parser" + P(mId)); tr("delete parser" + P(mId)); }
    void SetWarningsAsErrors(bool on) { tr("parser" + P(mId) + " warnings as errors " + P(on)); }
    void CloseFile() { tr("parser" + P(mId) + " close"); }
    const std::string& GetAbortReason() { static std::string s; s = "abort from parser" + P(mId); tr("parser" + P(mId) + " abort reason"); return s; }
};
namespace WONAPI {
struct WindowManager {
    static WindowManager* GetDefaultWindowManager() { static WindowManager w; return &w; }
    void SetCacheImages(bool on) { tr("cache images " + P(on)); }
};
}
using WONAPI::WindowManager;
struct Window { static Window* GetDefaultWindow() { static Window w; return &w; } };
struct PerfTimer {
    static double GetElapsedTime() { tr("elapsed"); return gScript.elapsed; }
    static double GetCountedTime() { tr("counted"); return gScript.counted; }
    static double GetPercentTime() { tr("percent"); return gScript.percent; }
};
static GUIString gEmptyGUIString;
static void MSMessageBoxMsg(Window*, const GUIString& t, const GUIString& m, const GUIString&, const GUIString&, const GUIString&)
{
    tr("box [" + t + "] [" + m + "]");
    if (gScript.box) throw OtherException{7};
}
static bool gShowParseTime;
static int gLinesParsed;

class ComponentConfig : public RefCount {
public:
    ComponentConfig() : RefCount("cfg") {}
    SmartPtr<Component> mComponent;
    GUIString mConfigError;
    static SmartPtr<ResourceConfigTable> mResourceConfigTable;
    virtual void SetComponent(Component* c)
    {
        tr("SetComponent " + PP(c));
        Throw(gScript.setComponent, "bind");
        this->mComponent = c;
    }
    bool ParseTopLevel(ConfigParser& p, const char* f)
    {
        tr("ParseTopLevel parser" + P(p.mId) + " " + f + " table " + PP(mResourceConfigTable.get()) + " show " + P(gShowParseTime));
        if (gScript.parseShows) { gShowParseTime = true; gLinesParsed = gScript.lines; }
        if (gScript.parse >= 2) Throw(gScript.parse - 1, "parse");
        return gScript.parse == 1;
    }
    void InitComponents() { tr("InitComponents"); Throw(gScript.init, "init"); }
    // the game's (compcfg.rs's resource file, both sides)
    static void SinkTo(void* c, const uint8_t* b, uintptr_t n) { static_cast<std::string*>(c)->assign((const char*)b, n); }
    static std::string GetResourceFile(const std::string& f)
    {
        std::string r;
        ee_won_cc_call(NULL, NULL, NULL, EeWonCcCall::GetResourceFile, (intptr_t)&r, (intptr_t)f.data(), (intptr_t)f.size(), (intptr_t)&SinkTo);
        return r;
    }
    bool ReadConfig_Unsafe(Component*, const char*, ResourceConfigTable*, ConfigParser*);
};
SmartPtr<ResourceConfigTable> ComponentConfig::mResourceConfigTable;
