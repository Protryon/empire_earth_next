#include "rust/won_net/ffi_parse.h"
#include "rust/won_gui/shell.h"
#include "rust/won_gui/skinobj.h"
// The stand-in world of won_compcfg_test.cpp, included once in each of its two namespaces (Orig: the deleted units'
// statements, won_compcfg_orig.h; Port: the game's forwards and host, include/ee/wonapi/WONCompCfg.inl, over ee_won_gui
// compcfg.rs). Every object is counted: the census after each step lists what lives and its references; every
// call into the world is traced.
typedef unsigned long DWORD;
typedef WONAPI::GUIString GUIString;
using WONAPI::StringLessNoCase;
using WONAPI::StringToUpperCaseInPlace;
static std::string sTrace;
static void tr(const std::string& s) { sTrace += s; sTrace += '\n'; if (getenv("EE_CC_ECHO")) fprintf(stderr, "%s\n", s.c_str()); }
static std::string P(long long v) { char b[32]; snprintf(b, sizeof b, "%lld", v); return b; }
static std::string PS(const GUIString& s) { return "\"" + (std::string)s + "\""; }

struct ConfigObjectException { std::string mText; bool mFlag; ConfigObjectException(const std::string& s, bool f = true) : mText(s), mFlag(f) {} };

// the live objects: kind, id and reference count
struct RefCount;
static std::map<int, RefCount*>& Live() { static std::map<int, RefCount*> m; return m; }
static int sNextId;
struct RefCount {
    int mRefCount = 0; int mId; const char* mKind;
    explicit RefCount(const char* k = "obj") : mId(++sNextId), mKind(k) { Live()[mId] = this; }
    virtual ~RefCount() { Live().erase(mId); }
    void AddRef() { ++mRefCount; }
    void Release() { if (--mRefCount <= 0) delete this; }
};
template<class T> struct SmartPtr {
    T* mPtr;
    SmartPtr(T* p = NULL) : mPtr(p) { if (p) p->AddRef(); }
    SmartPtr(const SmartPtr& o) : mPtr(o.mPtr) { if (mPtr) mPtr->AddRef(); }
    template<class U> SmartPtr(const SmartPtr<U>& o) : mPtr(o.get()) { if (mPtr) mPtr->AddRef(); }
    ~SmartPtr() { if (mPtr) mPtr->Release(); }
    SmartPtr& operator=(T* p) { if (p) p->AddRef(); T* o = mPtr; mPtr = p; if (o) o->Release(); return *this; }
    SmartPtr& operator=(const SmartPtr& s) { return *this = s.mPtr; }
    T* get() const { return mPtr; }
    T* operator->() const { return mPtr; }
    operator T*() const { return mPtr; }
};
static std::string Census()
{
    std::string s;
    for (std::map<int, RefCount*>::iterator i = Live().begin(); i != Live().end(); ++i)
        s += " " + std::string(i->second->mKind) + P(i->first) + ":" + P(i->second->mRefCount);
    return s;
}

struct Font : RefCount { std::string mFace; int mStyle, mSize; Font(const std::string& f, int st, int sz) : RefCount("font"), mFace(f), mStyle(st), mSize(sz) {} };
struct Image : RefCount { std::string mPath; Image(const std::string& p, const char* k = "img") : RefCount(k), mPath(p) {} };
struct RawImage : Image { RawImage(const std::string& p) : Image(p, "raw") {} };
struct Sound : RefCount { std::string mPath; int mFlags; Sound(const std::string& p, int f) : RefCount("snd"), mPath(p), mFlags(f) {} };
struct SoundDescriptor {
    std::string mPath; int mFlags = 0;
    void SetSoundFlags(int f, bool on) { if (on) mFlags |= f; else mFlags &= ~f; }
};
static std::string PP(const RefCount* o) { return o ? std::string(o->mKind) + P(o->mId) : "null"; }

struct Component : RefCount {
    int mLeft, mTop, mWidth, mHeight, mDesiredW, mDesiredH;
    Component(int l, int t, int w, int h) : RefCount("comp"), mLeft(l), mTop(t), mWidth(w), mHeight(h), mDesiredW(w + 7), mDesiredH(h + 9) {}
    int Left() { return mLeft; } int Top() { return mTop; } int Right() { return mLeft + mWidth; } int Bottom() { return mTop + mHeight; }
    int Width() { return mWidth; } int Height() { return mHeight; }
    void GetDesiredSize(int& w, int& h) { tr(PP(this) + ".GetDesiredSize"); w = mDesiredW; h = mDesiredH; }
    void SetSize(int w, int h) { tr(PP(this) + ".SetSize(" + P(w) + "," + P(h) + ")"); mWidth = w; mHeight = h; }
    void SetWantGrabBG(bool b) { tr(PP(this) + ".SetWantGrabBG(" + P(b) + ")"); }
    template<class I> void InitComponent(I& i) { tr(PP(this) + ".InitComponent(" + P(i.mType) + "," + PP(i.mConfig) + ")"); if (mWidth == 13) throw ConfigObjectException("init 13"); }
    // the parts the config kinds' predefined children read (R6 WONLobby round 26): made with the component by the
    // kinds' factory (KindComp), else none
    SmartPtr<Component> mListArea, mEdit, mContainerLayout, mPart[10];
    Component* Part(int i) { tr(PP(this) + ".part" + P(i)); return mPart[i].get(); }
    Component* GetThumb() { return Part(0); }
    Component* GetUpArrow() { return Part(1); }
    Component* GetDownArrow() { return Part(2); }
    Component* GetScrollRect() { return Part(3); }
    Component* GetHTMLView() { return Part(4); }
    Component* GetScrollbar(bool v) { return Part(v ? 6 : 5); }
    Component* GetScrollArea() { return Part(7); }
};
// the widget types the config classes held their component as (one Component here)
typedef Component Button; typedef Component Container; typedef Component Dialog; typedef Component ScrollArea; typedef Component ListArea;
typedef Component MultiListArea; typedef Component MultiListHeader; typedef Component InputBox; typedef Component Label;
typedef Component Scroller; typedef Component ScrollbarScroller; typedef Component Scrollbar; typedef Component TreeArea;
typedef Component ComboBox; typedef Component TabCtrl; typedef Component Splitter; typedef Component TextArea; typedef Component ImageButton;
typedef Component RectangleComponent; typedef Component RectangleComponent3D; typedef Component ImageComponent;
typedef Component ScreenContainer; typedef Component BrowserComponent; typedef Component MSButtonBase; typedef Component WONButton;
// a deleted class's own methods (WONGUIShell.h): the list controls' header, the etched frame's label, the frame's borders
struct WONShell {
    static intptr_t Op(Component* c, EeWonShellOp op, intptr_t a = 0)
    {
        tr(PP(c) + ".Op(" + P((int)op) + "," + P(a) + ")");
        if (op == EeWonShellOp::GetHeader) return (intptr_t)c->mPart[8].get();
        if (op == EeWonShellOp::GetLabel) return (intptr_t)c->mPart[9].get();
        if (op == EeWonShellOp::GetFrameBorder) return (c->mId * 7 + a * 3) % 11;
        return 0;
    }
};
// the game's skin (skinobj.rs) a WON skin config's PreParse makes
static intptr_t ee_won_guicpp_shell_call = 0;
static void ee_won_skin_ensure(intptr_t) { tr("skin ensure"); }
static Component* WONShellNew(EeWonShellKind, intptr_t, intptr_t, intptr_t) { return NULL; } // (the GUI's registrations' shells: not this golden's)

struct Background {
    SmartPtr<Image> mImage; int mColor, mBorderColor; bool mUseOffsets, mStretchImage, mWantGrabBG; SmartPtr<Component> mComponent;
    void Init() { mColor = -1; mBorderColor = -1; mUseOffsets = true; mStretchImage = false; mWantGrabBG = false; }
    Background() { Init(); mColor = (int)0x8000000d; }
    Background(Image* i) { Init(); mImage = i; }
    Background(int c, int b = -1) { Init(); mColor = c; mBorderColor = b; }
    bool GetWantGrabBG() const { return mWantGrabBG; }
};
static std::string PB(const Background& b)
{
    return "bg(" + PP(b.mImage.get()) + "," + P(b.mColor) + "," + P(b.mBorderColor) + "," + P(b.mUseOffsets) + P(b.mStretchImage) + P(b.mWantGrabBG) + ")";
}
enum ForegroundSelectionMode { ForegroundSelectionMode_UseForegroundColor, ForegroundSelectionMode_InvertOriginalColor, ForegroundSelectionMode_UseOriginalColor };
struct SelectionColor {
    DWORD mBackgroundColor, mForegroundColor; ForegroundSelectionMode mMode;
    SelectionColor() : mBackgroundColor(0x80000007), mForegroundColor(0x80000008), mMode(ForegroundSelectionMode_UseForegroundColor) {}
    SelectionColor(DWORD b, ForegroundSelectionMode m = ForegroundSelectionMode_InvertOriginalColor) : mBackgroundColor(b), mForegroundColor(0xffffff), mMode(m) {}
    SelectionColor(DWORD b, DWORD f) : mBackgroundColor(b), mForegroundColor(f), mMode(ForegroundSelectionMode_UseForegroundColor) {}
};
enum StandardCursor { StandardCursor_Arrow, StandardCursor_Hand, StandardCursor_IBeam, StandardCursor_EastWest, StandardCursor_NorthSouth, StandardCursor_NorthSouthEastWest };
struct Cursor : RefCount {
    int mType; Cursor(int t) : RefCount("cursor"), mType(t) {}
    static SmartPtr<Cursor> GetStandardCursor(StandardCursor t) { static Cursor* s[6]; if (!s[t]) { s[t] = new Cursor(t); s[t]->AddRef(); } return s[t]; }
};
enum Direction { Direction_Left, Direction_Right, Direction_Up, Direction_Down };
enum HorzAlign { HorzAlign_Left, HorzAlign_Center, HorzAlign_Right };
enum VertAlign { VertAlign_Top, VertAlign_Center, VertAlign_Bottom };
struct FontDescriptor { std::string mFace; int mStyle, mSize; FontDescriptor(const std::string& f, int st, int sz) : mFace(f), mStyle(st), mSize(sz) {} };
static bool Bad(const std::string& p) { return p.find("bad") != std::string::npos; }
struct Window {
    static Window* GetDefaultWindow() { static Window w; return &w; }
    SmartPtr<Font> GetFont(const FontDescriptor& d) { tr("GetFont(" + d.mFace + "," + P(d.mStyle) + "," + P(d.mSize) + ")"); return new Font(d.mFace, d.mStyle, d.mSize); }
    SmartPtr<RawImage> DecodeImageRaw(const char* p) { tr("DecodeImageRaw(" + std::string(p) + ")"); return Bad(p) ? NULL : new RawImage(p); }
};
struct WindowManager {
    static WindowManager* GetDefaultWindowManager() { static WindowManager w; return &w; }
    SmartPtr<Image> DecodeDelayImage(const char* p) { tr("DecodeDelayImage(" + std::string(p) + ")"); return Bad(p) ? NULL : new Image(p); }
    SmartPtr<Sound> DecodeSound(const SoundDescriptor& d) { tr("DecodeSound(" + d.mPath + "," + P(d.mFlags) + ")"); return Bad(d.mPath) ? NULL : new Sound(d.mPath, d.mFlags); }
};
// the value parser: the game's StringParser's members and the reads the configs make, over ee_won's parser
class StringParser {
public:
    StringParser(const char* t) : mNarrow(t), mPos(0), mWide(false) {}
    unsigned char GetChar(int o) { return ee_won_parser_get_char(Text(), Unit(), mPos, o); }
    wchar_t GetWChar(int o) { return (wchar_t)ee_won_parser_get_wchar(Text(), Unit(), mPos, o); }
    void IncrementPos(int n) { mPos += n; }
    bool EndOfString() { return ee_won_parser_get_wchar(Text(), Unit(), mPos, 0) == 0; }
    void SkipWhitespace() { ee_won_parser_skip_whitespace(Text(), Unit(), &mPos); }
    bool CheckNextChar(int c) { return ee_won_parser_check_next_char(Text(), Unit(), &mPos, c); }
    bool ReadString(std::wstring& s, bool stop) { s.erase(); ee_won_parser_read_wstring(Text(), Unit(), &mPos, stop, AppendUnits, &s); return true; }
    bool ReadString(std::string& s, bool stop) { s.erase(); ee_won_parser_read_string(Text(), Unit(), &mPos, stop, AppendBytes, &s); return true; }
    bool ReadValue(int& v) { int32_t x = 0; if (!ee_won_parser_read_int(Text(), Unit(), &mPos, &x)) return false; v = x; return true; }
    bool ReadValue(bool& v) { return ee_won_parser_read_bool(Text(), Unit(), &mPos, &v); }
    union { const char* mNarrow; const wchar_t* mWideText; };
    int mPos;
    bool mWide;
private:
    const void* Text() const { return mWide ? (const void*)mWideText : (const void*)mNarrow; }
    uint32_t Unit() const { return mWide ? (uint32_t)sizeof(wchar_t) : 1u; }
    static void AppendBytes(void* o, const uint8_t* b, size_t n) { static_cast<std::string*>(o)->append((const char*)b, n); }
    static void AppendUnits(void* o, const uint32_t* u, size_t n) { for (size_t i = 0; i < n; i++) *static_cast<std::wstring*>(o) += (wchar_t)u[i]; }
};
class ConfigParser {
public:
    void AddWarning(const std::string& w) { tr("AddWarning(" + w + ")"); if (w.find("THROW") != std::string::npos) throw ConfigObjectException("warning as error"); }
};
// the scripted parse: what the next ReadGroupStart, ParseChild and IncludeGroup answer, and the lines a child's parse
// hands it (set by the test before each step)
struct Script {
    std::vector<std::string> groups; // "name|inherited" or "" for none
    std::vector<std::string> names;  // a parsed child's name
    std::vector<std::vector<std::pair<std::string, std::string> > > lines; // a parsed child's key / value lines
    bool include = true;
};
static Script sScript;
template<class V> static V Next(std::vector<V>& v, const V& d) { if (v.empty()) return d; V x = v.front(); v.erase(v.begin()); return x; }

class ConfigObject : public RefCount {
public:
    ConfigObject() : RefCount("cfg") {}
    std::string mName, mBaseName;
    ConfigObject* mParent = NULL;
    virtual bool HandleInstruction(ConfigParser*, const std::string& t) { tr(PP(this) + ".CO::HandleInstruction(" + t + ")"); return t == "BASEGROUP"; }
    virtual bool HandleValueInstruction(ConfigParser*, const std::string& k, StringParser&) { tr(PP(this) + ".CO::HandleValueInstruction(" + k + ")"); return k == "BASEKEY"; }
    virtual bool PreParse(ConfigParser*) { return true; }
    virtual bool PostParse(ConfigParser*) { return true; }
    ConfigObject* GetParentObject() { return mParent; }
    bool ReadGroupStart(ConfigParser&, std::string& n, std::string& i)
    {
        std::string g = Next(sScript.groups, std::string());
        tr(PP(this) + ".ReadGroupStart=" + g);
        if (g.empty()) return false;
        size_t bar = g.find('|'); n = g.substr(0, bar); i = bar == std::string::npos ? "" : g.substr(bar + 1);
        if (n == "THROW") throw ConfigObjectException("group start");
        return true;
    }
    bool ParseGroup(ConfigParser&, bool a, bool b, bool c, bool d) { tr(PP(this) + ".ParseGroup(" + P(a) + P(b) + P(c) + P(d) + ")"); return true; }
    bool IncludeGroup(ConfigParser&, const std::string& g) { tr(PP(this) + ".IncludeGroup(" + g + ")"); return sScript.include; }
    // a child's parse: its name, PreParse, its lines through HandleValueInstruction (a ConfigObjectException caught
    // and traced, as ParseGroup did), PostParse
    bool ParseChild(ConfigParser& p, ConfigObject* c, bool a, bool r)
    {
        SmartPtr<ConfigObject> keep = c;
        c->mParent = this; // (ConfigObject's ParseChild sets the parent first)
        c->mName = Next(sScript.names, std::string());
        tr(PP(this) + ".ParseChild(" + PP(c) + "," + P(a) + P(r) + ") name " + c->mName);
        c->PreParse(&p);
        std::vector<std::pair<std::string, std::string> > lines = Next(sScript.lines, std::vector<std::pair<std::string, std::string> >());
        for (size_t i = 0; i < lines.size(); i++) {
            StringParser v(lines[i].second.c_str());
            try { bool h = c->HandleValueInstruction(&p, lines[i].first, v); tr(" line " + lines[i].first + "=" + P(h) + " at " + P(v.mPos)); }
            catch (ConfigObjectException& e) { tr(" line " + lines[i].first + " threw " + e.mText); }
        }
        bool ok = c->PostParse(&p);
        return ok && c->mName != "FAIL";
    }
};
