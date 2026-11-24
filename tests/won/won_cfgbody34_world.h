// The stand-in world of won_cfgbody34_test.cpp, included once in each of its namespaces (Orig: the bodies as they
// were, won_cfgbody34_orig.h; Port: the game's include/ee/wonapi/WONCfgBody.inl over ee_won_gui cfgbody.rs and the
// classes' forwards). Every call is traced, every object counted; the shared script (gScript) says what each read,
// parse and look-up answers or throws.
static std::string sTrace;
static void tr(const std::string& s) { sTrace += s; sTrace += '\n'; }
static std::string P(long long v) { char b[32]; snprintf(b, sizeof b, "%lld", v); return b; }
struct ConfigObjectException { std::string mText; bool mFlag; ConfigObjectException(const std::string& s, bool f = true) : mText(s), mFlag(f) {} };
struct OtherException { int mCode; };

static std::map<std::string, int>& Live() { static std::map<std::string, int> m; return m; }
static int sNextId, sTexts;
static std::string Census()
{
    std::string s = " texts=" + P(sTexts);
    for (std::map<std::string, int>::iterator i = Live().begin(); i != Live().end(); ++i) s += " " + i->first + "=" + P(i->second);
    return s;
}
// a counted text (the GUIStrings a body holds)
struct GUIString {
    std::string s;
    GUIString() { ++sTexts; }
    GUIString(const std::string& t) : s(t) { ++sTexts; }
    GUIString(const char* t) : s(t) { ++sTexts; }
    GUIString(const GUIString& o) : s(o.s) { ++sTexts; }
    GUIString& operator=(const GUIString& o) { s = o.s; return *this; }
    ~GUIString() { --sTexts; }
};
struct RefCount {
    int mRefCount = 0; int mId; std::string mKind;
    explicit RefCount(const char* k) : mId(++sNextId), mKind(k) { Live()[Name()] = 0; tr("new " + Name()); }
    virtual ~RefCount() { Live().erase(Name()); tr("delete " + Name()); }
    std::string Name() const { return mKind + P(mId); }
    void AddRef() { ++mRefCount; Live()[Name()] = mRefCount; }
    void Release() { if (--mRefCount <= 0) delete this; else Live()[Name()] = mRefCount; }
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

// what the bodies meet (both sides alike)
typedef ::BodyScript Script; // (won_cfgbody34_test.cpp's, shared by both sides)
static Script gScript;
static int sTextsRead, sCommas, sEos;
static void Throw(int k, const std::string& what)
{
    if (k == 1 || k == 2) throw ConfigObjectException(what + " failed");
    if (k >= 3) throw OtherException{(int)what.size()};
}

struct ColorScheme : RefCount {
    int mColors[4];
    ColorScheme(int c) : RefCount("scheme") { for (int i = 0; i < 4; i++) mColors[i] = c + i; }
};
static std::string PS(const ColorScheme* s) { return s ? P(s->mColors[0]) + "," + P(s->mColors[1]) + "," + P(s->mColors[2]) + "," + P(s->mColors[3]) : "null"; }
struct Component : RefCount {
    int mW, mH; GUIString mText; SmartPtr<ColorScheme> mOwnScheme;
    Component() : RefCount("comp"), mW(gScript.width), mH(gScript.height), mText("old") {}
    int Width() { tr(Name() + ".Width"); return mW; }
    int Height() { tr(Name() + ".Height"); return mH; }
    GUIString GetText() { tr(Name() + ".GetText"); return mText; }
    void SetText(const GUIString& t, bool b)
    {
        tr(Name() + ".SetText(" + t.s + "," + P(b) + ")");
        if (gScript.setTextThrows && t.s == "boom") throw OtherException{3};
        mText = t;
    }
    void GetDesiredSize(int& w, int& h) { tr(Name() + ".GetDesiredSize"); w = (int)mText.s.size() * 7 - 20; h = (int)mText.s.size() * 3 + 2; }
    void SetSize(int w, int h) { tr(Name() + ".SetSize(" + P(w) + "," + P(h) + ")"); mW = w; mH = h; }
    ColorScheme* GetOwnColorScheme() { tr(Name() + ".GetOwnColorScheme"); return mOwnScheme.get(); }
};
typedef Component Button;
struct Layout : RefCount { Layout() : RefCount("layout") {} };
struct PercentLayoutParam {
    Component* r[4]; int p[8];
    PercentLayoutParam(Component* a, Component* b, Component* c, Component* d, int p0, int p1, int p2, int p3, int p4, int p5, int p6, int p7)
    { r[0] = a; r[1] = b; r[2] = c; r[3] = d; int q[8] = {p0, p1, p2, p3, p4, p5, p6, p7}; memcpy(p, q, sizeof p); }
};
struct PercentLayout : Layout {
    PercentLayout(Component* t, const PercentLayoutParam& x)
    {
        std::string s = "PercentLayout(" + PP(t);
        for (int i = 0; i < 4; i++) s += "," + PP(x.r[i]);
        for (int i = 0; i < 8; i++) s += "," + P(x.p[i]);
        tr(s + ")");
    }
};
// the game's maker (R6 WON GUI round 35: the param's borders are the Rust record's)
static Layout* WONNewPercentLayout(Component* t, Component* const* r, const int* p)
{
    return new PercentLayout(t, PercentLayoutParam(r[0], r[1], r[2], r[3], p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]));
}
struct Container : Component {
    void AddChildLayout(Layout* l, int id) { SmartPtr<Layout> keep = l; tr(Name() + ".AddChildLayout(" + PP(l) + "," + P(id) + ")"); }
    void SetColorScheme(ColorScheme* s) { tr(Name() + ".SetColorScheme(" + PS(s) + ")"); }
};
class ConfigParser {
public:
    void SetError(const std::string& e, bool a) { tr("SetError(" + e + "," + P(a) + ")"); }
};
namespace WONAPI {
struct WindowManager {
    SmartPtr<ColorScheme> mColorScheme;
    static WindowManager* GetDefaultWindowManager() { static WindowManager w; tr("wm"); return &w; }
};
}
using WONAPI::WindowManager;
struct FontDescriptor {
    std::string mFace; int mStyle, mSize;
    bool operator!=(const FontDescriptor& o) const { return mFace != o.mFace || mStyle != o.mStyle || mSize != o.mSize; }
};
struct Font : RefCount {
    FontDescriptor mDescriptor;
    Font(const FontDescriptor& d) : RefCount("font"), mDescriptor(d) {}
    int GetHeight() { tr(Name() + ".GetHeight"); return mDescriptor.mSize * 2 + 1; }
};
struct DisplayContext : RefCount { DisplayContext() : RefCount("dc") {} };
struct Image : RefCount {
    int mW;
    Image(int w, const char* k = "img") : RefCount(k), mW(w) {}
    int GetWidth() { tr(Name() + ".GetWidth"); return mW; }
    int GetHeight() { tr(Name() + ".GetHeight"); return mW + 3; }
};
struct RawImage : Image {
    RawImage(int w) : Image(w, "raw") {}
    SmartPtr<Image> GetNative(DisplayContext* d) { tr(Name() + ".GetNative(" + PP(d) + ")"); if (gScript.fontThrows == 2) throw OtherException{9}; return new Image(mW * 2); }
};
struct Window {
    SmartPtr<DisplayContext> mDisplayContext;
    static Window* GetDefaultWindow() { static Window w; if (!w.mDisplayContext.get()) w.mDisplayContext = new DisplayContext; return &w; }
    SmartPtr<Font> GetFont(const FontDescriptor& d) { tr("GetFont(" + d.mFace + "," + P(d.mStyle) + "," + P(d.mSize) + ")"); if (gScript.fontThrows == 1) throw OtherException{4}; return new Font(d); }
};

class ConfigObject : public RefCount {
public:
    explicit ConfigObject(const char* k) : RefCount(k) {}
    ConfigObject* mParent = NULL;
    std::string mName;
};
class ComponentConfig : public ConfigObject {
public:
    ComponentConfig(const char* k = "cfg") : ConfigObject(k) {}
    SmartPtr<Component> mComponent;
    GUIString ReadString(bool)
    {
        int n = sTextsRead++;
        tr(Name() + ".ReadString " + P(n));
        if (n == gScript.textThrowAt) throw ConfigObjectException("read");
        return GUIString(n < (int)gScript.texts.size() ? gScript.texts[n] : "");
    }
    bool EndOfString() { int n = sEos++; bool e = n >= (int)gScript.eos.size() || gScript.eos[n] != 0; tr(Name() + ".EndOfString=" + P(e)); return e; }
    void EnsureComma() { int n = sCommas++; tr(Name() + ".EnsureComma " + P(n)); if (n == gScript.commaThrowAt) throw ConfigObjectException("comma"); }
    bool ParseChild(ConfigParser&, ConfigObject* c, bool a, bool r);
    virtual bool HandleInstruction(ConfigParser*, const std::string& t)
    {
        tr(Name() + ".base HandleInstruction(" + t + ")");
        if (gScript.base == 2) throw ConfigObjectException("base");
        return gScript.base == 1;
    }
    Component* Fn386860(const std::string& n) { tr(Name() + ".Fn386860(" + n + ")"); return gScript.found ? mComponent.get() : NULL; }
    void HandleFitText();
};
class ContainerConfig : public ComponentConfig {
public:
    ContainerConfig() : ComponentConfig("container") {}
    Container* mContainer = NULL;
    int mCurLayoutId = 0;
    virtual bool HandleInstruction(ConfigParser*, const std::string&);
};
class PercentLayoutConfig : public ComponentConfig {
public:
    PercentLayoutConfig() : ComponentConfig("pctcfg") { for (int i = 0; i < 4; ++i) this->refs[i] = NULL; for (int i = 0; i < 8; ++i) this->params[i] = -1; }
    virtual bool PreParse(ConfigParser*);
    Component* target = NULL;
    Component* refs[4];
    int params[8];
};
class ColorSchemeConfig : public ComponentConfig {
public:
    ColorSchemeConfig() : ComponentConfig("schemecfg") { this->mColorScheme = new ColorScheme(0); }
    virtual bool PreParse(ConfigParser*);
    Component* target = NULL;
    SmartPtr<ColorScheme> mColorScheme;
};
// the child parse: the child's parent and what its lines set (the percent layout's references and parameters, the
// scheme's colours)
static std::vector<SmartPtr<Component> > sRefComps;
bool ComponentConfig::ParseChild(ConfigParser&, ConfigObject* c, bool a, bool r)
{
    tr(Name() + ".ParseChild(" + PP(c) + "," + P(a) + P(r) + ")");
    c->mParent = this;
    if (PercentLayoutConfig* p = dynamic_cast<PercentLayoutConfig*>(c)) {
        for (int i = 0; i < 4; i++) p->refs[i] = gScript.refs[i] ? sRefComps[i].get() : NULL;
        for (int i = 0; i < 8; i++) if (gScript.params[i] != -1) p->params[i] = gScript.params[i];
    }
    if (ColorSchemeConfig* s = dynamic_cast<ColorSchemeConfig*>(c)) memcpy(s->mColorScheme->mColors, gScript.colors, sizeof gScript.colors);
    if (gScript.parse >= 2) Throw(gScript.parse, "parse");
    return gScript.parse == 1;
}
class FontConfig : public ComponentConfig {
public:
    FontConfig() : ComponentConfig("fontcfg") {}
    Font* GetFont();
    void SetFont(Font*);
    virtual bool GetNamedValue(const std::string&, int&);
    SmartPtr<Font> mFont;
    FontDescriptor mDescriptor;
    bool mNeedRecalcFont = true;
};
class ImageConfig : public ComponentConfig {
public:
    ImageConfig(const char* k = "imgcfg") : ComponentConfig(k) {}
    virtual bool GetNamedValue(const std::string&, int&);
    virtual Image* GetImage() { return this->mImage.get(); }
    SmartPtr<Image> mImage;
};
class ImageFilterConfig : public ImageConfig {
public:
    ImageFilterConfig() : ImageConfig("filtercfg") {}
    virtual Image* GetImage();
    SmartPtr<RawImage> mRawImage;
    bool mImageIsRawImage = false;
};
