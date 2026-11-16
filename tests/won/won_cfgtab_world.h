// The stand-in world of won_cfgtab_test.cpp, included once in each of its two namespaces (Orig: the handlers as they
// were, won_cfgtab_orig.h; Shell: the port, WONCfgTable.inl over ee_won's cfgtab.rs). Every read, call and field
// write is appended to the namespace's trace.
typedef unsigned long DWORD;
typedef WONAPI::GUIString GUIString;
static std::string sTrace;
static void tr(const std::string& s) { sTrace += s; sTrace += '\n'; }
struct ConfigObjectException { std::string mText; ConfigObjectException(const std::string& s) : mText(s) {} };
template<class X> struct SmartPtr {
    X* p;
    SmartPtr(X* x = NULL) : p(x) {}
    X* get() const { return p; }
    X* operator->() const { return p; }
    operator X*() const { return p; }
    SmartPtr& operator=(X* x) { p = x; return *this; }
};
struct Obj { int id; Obj(int i) : id(i) {} virtual ~Obj() {} };
struct Font : Obj { Font(int i) : Obj(i) {} };
struct Image : Obj { Image(int i) : Obj(i) {} void SetSize(int w, int h); };
struct RawImage : Image {
    int w, h;
    RawImage(int i, int ww, int hh) : Image(i), w(ww), h(hh) {}
    // a width divided by a count of 0 (the original's crash) is -999 here, the same on both sides
    struct Num { int v; operator int() const { return v; } Num operator/(int d) const { return Num{d ? v / d : -999}; } };
    Num GetWidth() { return Num{w}; }
    int GetHeight() { return h; }
    SmartPtr<RawImage> Duplicate();
};
struct StretchImage : Image {
    StretchImage(int i) : Image(i) {}
    void SetImage(RawImage* r);
    void SetTile(bool a, bool b);
    void SetBorders(int a, int b, int c, int d);
    void SetSkipCenter(bool a);
    void CopyAttributes(StretchImage* s);
};
struct Sound : Obj { Sound(int i) : Obj(i) {} };
struct Background { int id; Background() : id(-1) {} };
struct SelectionColor { int id; SelectionColor() : id(-1) {} };
struct WONFrame {
    int img, l, t, r, b, kind;
    WONFrame() : img(0), l(0), t(0), r(0), b(0), kind(0) {}
    WONFrame(StretchImage* s) : img(s ? s->id : 0), l(0), t(0), r(0), b(0), kind(1) {}
    WONFrame(Image* i, int w) : img(i ? i->id : 0), l(w), t(0), r(0), b(0), kind(2) {}
    WONFrame(Image* i, int a, int bb, int c, int d) : img(i ? i->id : 0), l(a), t(bb), r(c), b(d), kind(3) {}
};
enum Direction { Direction_Left, Direction_Right, Direction_Up, Direction_Down };
enum HorzAlign { HorzAlign_Left, HorzAlign_Center, HorzAlign_Right };
enum VertAlign { VertAlign_Top, VertAlign_Center, VertAlign_Bottom };
enum ListSortType { ListSortType_Ascending, ListSortType_Descending, ListSortType_None };
enum HTMLFontSize { HTMLFontSize_Smallest, HTMLFontSize_Small, HTMLFontSize_Medium, HTMLFontSize_Large, HTMLFontSize_Largest };
enum ImageButtonType { ImageButtonType_Normal, ImageButtonType_Over, ImageButtonType_Down, ImageButtonType_Disabled,
    ImageButtonType_Checked, ImageButtonType_None };
enum FontStyles { FontStyle_Plain = 0, FontStyle_Bold = 1, FontStyle_Italic = 2, FontStyle_Underline = 4 };
enum ScrollbarCondition { ScrollbarCondition_Disable = 0, ScrollbarCondition_Enable = 1, ScrollbarCondition_Conditional = 2 };

struct Component;
static std::string P(long long v) { char b[32]; snprintf(b, sizeof b, "%lld", v); return b; }
static std::string P(bool v) { return v ? "1" : "0"; }
static std::string P(const GUIString& s) { return "\"" + (std::string)s + "\""; }
static std::string PP(const Obj* o) { return o ? "#" + P((long long)o->id) : "null"; }
static std::string PP(const Component* c);
template<class X> static std::string P(X* p) { return PP(p); }
template<class X> static std::string P(const SmartPtr<X>& s) { return P(s.get()); }
static std::string P(const Background& b) { return "bg" + P((long long)b.id); }
static std::string P(const SelectionColor& b) { return "sel" + P((long long)b.id); }
static std::string P(const WONFrame& f)
{
    return "frame(" + P((long long)f.kind) + "," + P((long long)f.img) + "," + P((long long)f.l) + "," + P((long long)f.t) + ","
        + P((long long)f.r) + "," + P((long long)f.b) + ")";
}
typedef int (*SortFunc)(int);
static std::string P(SortFunc) { return "NumberSortFunc"; }
template<class X> static std::string P(const X& v) { return P((long long)v); }
static std::string Args() { return ""; }
template<class A, class... R> static std::string Args(const A& a, const R&... r)
{
    std::string rest = Args(r...);
    return P(a) + (rest.empty() ? "" : "," + rest);
}
struct Field {
    Component* o; const char* n;
    template<class V> Field& operator=(const V& v) { tr(P(o) + "." + n + "=" + P(v)); return *this; }
};
struct Component {
    int id; unsigned mask;
    SmartPtr<Component> mEdit;
    Field mTextColor, mSelColor, mDisabledTextColor, mUseDisabledColors, mTile, mStretch, mHeaderHeight, mDrawFrame,
        mButtonInFrame, mTransparent;
    enum QuadrantPosition { Position_Left = 1, Position_Top = 2, Position_Right = 4, Position_Bottom = 8,
        Position_TopLeft = 3, Position_TopRight = 6, Position_BottomLeft = 9, Position_BottomRight = 12 };
    enum SplitterType { Splitter_LeftRight = 5, Splitter_UpDown = 10, Splitter_LeftRight_Top = 0x15,
        Splitter_LeftRight_Bottom = 0x25, Splitter_UpDown_Left = 0x4a, Splitter_UpDown_Right = 0x8a, Splitter_Center = 0x10f };
    typedef int (*SortFunc)(int);
    static int NumberSortFunc(int) { return 0; }
    explicit Component(int i, unsigned m = 15) : id(i), mask(m), mEdit(NULL),
        mTextColor{this, "mTextColor"}, mSelColor{this, "mSelColor"}, mDisabledTextColor{this, "mDisabledTextColor"},
        mUseDisabledColors{this, "mUseDisabledColors"}, mTile{this, "mTile"}, mStretch{this, "mStretch"},
        mHeaderHeight{this, "mHeaderHeight"}, mDrawFrame{this, "mDrawFrame"}, mButtonInFrame{this, "mButtonInFrame"},
        mTransparent{this, "mTransparent"} {}
    Component();
    virtual ~Component() {}
    Field mControlId{this, "mControlId"}, mSkin{this, "mSkin"};
    int Left() { return 10 + id; }
    int Top() { return 20 + id; }
    int Width() { return 30 + id; }
    int Height() { return 40 + id; }
    int GetRightTextPad() { return 7 + id; }
    Component* GetScrollArea() { return this; }
    void SetScrollRect(Component* c) { tr(P(this) + ".SetScrollRect(" + P(c) + ")"); }
#define EE_CFGTAB_M(n) template<class... A> void n(const A&... a) { tr(P(this) + "." #n "(" + Args(a...) + ")"); }
    EE_CFGTAB_METHODS(EE_CFGTAB_M)
#undef EE_CFGTAB_M
};
static int sNextComp;
Component::Component() : Component(sNextComp++) {}
// a layout point's id is given at its first trace (the original made it before reading its name, the port after)
static int sNextPoint;
static std::string PP(const Component* c)
{
    if (c && c->id == -1) const_cast<Component*>(c)->id = 9000 + sNextPoint++;
    return c ? "c" + P((long long)c->id) : "null";
}
typedef Component Container; typedef Component Button; typedef Component Scroller; typedef Component MultiListArea;
typedef Component ScrollArea; typedef Component ListArea; typedef Component InputBox; typedef Component Splitter;
typedef Component WONButton; typedef Component WONSimpleButton; typedef Component WONHeaderButton; typedef Component WONScrollbar;
typedef Component WONScrollContainer; typedef Component WONListCtrl; typedef Component WONTabCtrl; typedef Component WONComboBox;
typedef Component WONFrameComponent;
struct LayoutPoint : Component { LayoutPoint() : Component(-1) {} };
// the layouts ContainerConfig's instructions make (R6 WON round 40): each made and each component added is traced
static int sNextLayout;
struct ChildLayoutInstructionBase : Obj {
    explicit ChildLayoutInstructionBase(const std::string& d) : Obj(sNextLayout++) { tr("new layout#" + P((long long)id) + " " + d); }
    void AddComponent(Component* c) { tr("layout#" + P((long long)id) + ".Add(" + PP(c) + ")"); }
};
#define EE_CFGTAB_LAYOUT(T) T* Add(Component* c) { this->AddComponent(c); return this; }
struct SameSizeLayout : ChildLayoutInstructionBase { SameSizeLayout(bool a, bool b) : ChildLayoutInstructionBase("SameSize(" + P(a) + "," + P(b) + ")") {} EE_CFGTAB_LAYOUT(SameSizeLayout) };
struct CenterLayout : ChildLayoutInstructionBase { CenterLayout(Component* c, bool a, bool b) : ChildLayoutInstructionBase("Center(" + PP(c) + "," + P(a) + "," + P(b) + ")") {} EE_CFGTAB_LAYOUT(CenterLayout) };
struct NoOverlapLayout : ChildLayoutInstructionBase { NoOverlapLayout(Component* t, int k, Component* r, int h, int v) : ChildLayoutInstructionBase("NoOverlap(" + PP(t) + "," + P((long long)k) + "," + PP(r) + "," + P((long long)h) + "," + P((long long)v) + ")") {} EE_CFGTAB_LAYOUT(NoOverlapLayout) };
struct ClearRightLayout : ChildLayoutInstructionBase { ClearRightLayout(Component* c, int p) : ChildLayoutInstructionBase("ClearRight(" + PP(c) + "," + P((long long)p) + ")") {} EE_CFGTAB_LAYOUT(ClearRightLayout) };
struct SpaceEvenlyLayout : ChildLayoutInstructionBase { SpaceEvenlyLayout(Component* c, int l, int r, bool h) : ChildLayoutInstructionBase("SpaceEvenly(" + PP(c) + "," + P((long long)l) + "," + P((long long)r) + "," + P(h) + ")") {} EE_CFGTAB_LAYOUT(SpaceEvenlyLayout) };
struct SizeAtLeastLayout : ChildLayoutInstructionBase { SizeAtLeastLayout(Component* c, int h, int v) : ChildLayoutInstructionBase("SizeAtLeast(" + PP(c) + "," + P((long long)h) + "," + P((long long)v) + ")") {} EE_CFGTAB_LAYOUT(SizeAtLeastLayout) };
#undef EE_CFGTAB_LAYOUT
struct ImageComponent : Component { ImageComponent(Image* i) : Component(5000 + (i ? i->id : 0)) {} };
enum StandardCursor { StandardCursor_Arrow, StandardCursor_Hand, StandardCursor_IBeam };
struct Cursor : Obj {
    Cursor(int i) : Obj(i) {}
    static void SetStandardCursor(StandardCursor t, Cursor* c) { tr("Cursor::SetStandardCursor(" + P((long long)t) + "," + P((long long)(c ? c->id : -1)) + ")"); }
};
struct ComponentCursor : Cursor { ComponentCursor(Component* c) : Cursor(c->id) {} };
struct Window {
    static Window* GetDefaultWindow() { static Window w; return &w; }
    void SetSkipDecodeImages(bool b) { tr("window.SetSkipDecodeImages(" + P(b) + ")"); }
};
struct ConfigParser {
    static void ResetLineCount() { tr("ConfigParser::ResetLineCount"); }
    static int GetLineCount() { tr("ConfigParser::GetLineCount"); return 42; }
};
struct PerfTimer {
    static void GlobalStart(void*) { tr("PerfTimer::GlobalStart"); }
    static void GlobalStop(void*) { tr("PerfTimer::GlobalStop"); }
};
static Field gShowParseTime{NULL, "gShowParseTime"}, gLinesParsed{NULL, "gLinesParsed"};
// the skin's template slots: an assignment is traced; -> reaches the component last assigned
struct SkinSlot {
    std::string n; Component* v = NULL;
    SkinSlot& operator=(Component* c) { tr("skin." + n + "=" + P(c)); v = c; return *this; }
    Component* operator->() { return v; }
};
struct SkinMap {
    std::string n;
    SkinSlot operator[](const GUIString& k) { SkinSlot s; s.n = n + "[" + (std::string)k + "]"; return s; }
};
// the image configs' raw images and filters (R6 WON round 41): every one made, copied and filtered is traced
static int sNextRaw;
SmartPtr<RawImage> RawImage::Duplicate() { RawImage* r = new RawImage(sNextRaw++, w, h); tr("raw#" + P((long long)id) + ".Duplicate -> #" + P((long long)r->id)); return r; }
void StretchImage::SetImage(RawImage* r) { tr("stretch#" + P((long long)id) + ".SetImage(" + P(r) + ")"); }
void StretchImage::SetTile(bool a, bool b) { tr("stretch#" + P((long long)id) + ".SetTile(" + P(a) + "," + P(b) + ")"); }
void StretchImage::SetBorders(int a, int b, int c, int d) { tr("stretch#" + P((long long)id) + ".SetBorders(" + Args(a, b, c, d) + ")"); }
void StretchImage::SetSkipCenter(bool a) { tr("stretch#" + P((long long)id) + ".SetSkipCenter(" + P(a) + ")"); }
void StretchImage::CopyAttributes(StretchImage* s) { tr("stretch#" + P((long long)id) + ".CopyAttributes(#" + P((long long)s->id) + ")"); }
struct ImageFilter : Obj {
    std::string d;
    explicit ImageFilter(const std::string& s) : Obj(0), d(s) {}
    SmartPtr<RawImage> Filter(RawImage* r)
    {
        RawImage* o = r ? new RawImage(sNextRaw++, r->w / 2 + 1, r->h / 2 + 1) : NULL;
        tr("filter " + d + "(" + P(r) + ") -> " + P(o)); return o;
    }
};
struct ScaleImageFilter : ImageFilter {
    explicit ScaleImageFilter(int) : ImageFilter("Scale0") {}
    ScaleImageFilter(int w, int h) : ImageFilter("Scale(" + Args(w, h) + ")") {}
    void SetPercentSize(int w, int h) { d = "ScalePercent(" + Args(w, h) + ")"; }
};
struct TintImageFilter : ImageFilter { TintImageFilter(int r, int g, int b) : ImageFilter("Tint(" + Args(r, g, b) + ")") {} };
struct TransparentImageFilter : ImageFilter { explicit TransparentImageFilter(DWORD c) : ImageFilter("Transparent(" + P((long long)c) + ")") {} };
struct CropImageFilter : ImageFilter {
    int x = 0, y = 0, w = 0, h = 0;
    CropImageFilter() : ImageFilter("Crop") {}
    CropImageFilter(int a, int b, int c, int e) : ImageFilter("Crop(" + Args(a, b, c, e) + ")") {}
    void SetSize(int a, int b) { w = a; h = b; d = "Crop(" + Args(x, y, w, h) + ")"; }
    void SetPos(int a, int b) { x = a; y = b; d = "Crop(" + Args(x, y, w, h) + ")"; }
};
void Image::SetSize(int w, int h) { tr("image#" + P((long long)id) + ".SetSize(" + P((long long)w) + "," + P((long long)h) + ")"); }
struct WindowManager {
    static WindowManager* GetDefaultWindowManager() { static WindowManager w; return &w; }
    void SetDefaultFont(Font* f) { tr("wm.SetDefaultFont(" + P(f) + ")"); }
    void SetNamedFont(const GUIString& n, Font* f) { tr("wm.SetNamedFont(" + P(n) + "," + P(f) + ")"); }
};
struct ColorScheme { void SetStandardColor(int i, DWORD c) { tr("scheme.SetStandardColor(" + P((long long)i) + "," + P((long long)c) + ")"); } };
struct Skin {
    SkinSlot mPushButton{"mPushButton"}, mCheckbox{"mCheckbox"}, mRadioButton{"mRadioButton"}, mHeaderButton{"mHeaderButton"},
        mTabButton{"mTabButton"}, mComboButton{"mComboButton"}, mCloseButton{"mCloseButton"},
        mScrollContainer{"mScrollContainer"}, mListCtrl{"mListCtrl"}, mTabCtrl{"mTabCtrl"}, mComboBox{"mComboBox"},
        mPopupBack{"mPopupBack"}, mDialogBack{"mDialogBack"};
    SkinSlot mArrowButton[4] = {{"mArrowButton0"}, {"mArrowButton1"}, {"mArrowButton2"}, {"mArrowButton3"}};
    SkinSlot mScrollGutter[2] = {{"mScrollGutter0"}, {"mScrollGutter1"}}, mScrollThumb[2] = {{"mScrollThumb0"}, {"mScrollThumb1"}},
        mScrollbar[2] = {{"mScrollbar0"}, {"mScrollbar1"}};
    SkinMap mButtonTypeMap{"mButtonTypeMap"}, mSimpleButtonTypeMap{"mSimpleButtonTypeMap"};
    bool CopyButtonType(const GUIString& t, Component* b) { tr("skin.CopyButtonType(" + Args(t, b) + ")"); return t.length() == 0 || t.at(0) != 'x'; }
    bool CopySimpleButtonType(const GUIString& t, Component* b) { tr("skin.CopySimpleButtonType(" + Args(t, b) + ")"); return t.length() == 0 || t.at(0) != 'x'; }
};
static Skin sSkin;
static Skin* gSkin = &sSkin;
template<class T> T SafeConfigGetComponent(Component* c, const char* name)
{
    unsigned bit = !strcmp(name, "Container") ? 1 : !strcmp(name, "Button") ? 2 : !strcmp(name, "Scroller") ? 4
        : !strcmp(name, "MultiListArea") ? 8 : 1u << (strlen(name) % 4);
    tr(std::string("SafeConfigGetComponent<") + name + ">(" + P(c) + ")");
    if (c == NULL || !(c->mask & bit)) throw ConfigObjectException(std::string("Invalid ") + name);
    return (T)c;
}

// The reads: a cursor over the value text. A token runs to the next comma (or whitespace for a flag); a token
// starting with '!' makes a read throw.
static std::map<std::string, Component*> sComps;
static Component* gComp;
struct ComponentConfig {
    std::string mText; size_t mPos;
    ComponentConfig() : mPos(0) {}
    virtual ~ComponentConfig() {}
    virtual bool Virtual24(const std::string& key);
    SmartPtr<Component> mComponent = gComp;
    ComponentConfig* mParent = NULL;
    void SetComponent(Component* c) { mComponent = c; }
    bool mHasSetWidth = false, mHasSetHeight = false;
    bool HandleModify() { tr("HandleModify"); return mText.size() % 2 == 0; }
    ComponentConfig* GetParentObject();
    void AddChildToConfigMap(const GUIString& name, ComponentConfig* c);
    SmartPtr<RawImage> ReadRawImage()
    {
        std::string t = Take("ReadRawImage");
        RawImage* r = new RawImage(1000 + Hash(t) % 1000, 3 + Hash(t) % 60, 2 + Hash(t) % 40);
        tr("ReadRawImage " + P(r)); return r;
    }
    SmartPtr<RawImage> GetRawImageFromStr(const std::string& n)
    {
        tr("GetRawImageFromStr \"" + n + "\"");
        if (n.find('!') != std::string::npos) throw ConfigObjectException("bad image " + n);
        return new RawImage(2000 + Hash(n) % 1000, 4 + Hash(n) % 30, 4);
    }
    GUIString ReadRawString(bool)
    {
        std::string t = Take("ReadRawString");
        tr("ReadRawString \"" + t + "\""); return GUIString(t.c_str());
    }
    bool HandleMapComponent() { tr("HandleMapComponent"); return true; }
    void SetDesiredSizeIfNecessary() { tr("SetDesiredSizeIfNecessary"); }
    SmartPtr<Cursor> ReadCursor() { Cursor* c = new Cursor(Hash(Take("ReadCursor"))); tr("ReadCursor " + P(c)); return c; }
    Direction ReadDirection() { int v = Hash(Take("ReadDirection")) % 4; tr("ReadDirection " + P((long long)v)); return (Direction)v; }
    StandardCursor ReadStandardCursor() { int v = Hash(Take("ReadStandardCursor")) % 3; tr("ReadStandardCursor " + P((long long)v)); return (StandardCursor)v; }
    bool Base(const char* cls, const std::string& key) { tr(std::string("base ") + cls + " " + key); return key == "BASEKEY"; }
    bool RunTable(int, const std::string&);
    void Skip() { while (mPos < mText.size() && (mText[mPos] == ' ' || mText[mPos] == '\t')) mPos++; }
    std::string Token(bool stopAtSpace = false)
    {
        Skip();
        size_t s = mPos;
        while (mPos < mText.size() && mText[mPos] != ',' && !(stopAtSpace && (mText[mPos] == ' ' || mText[mPos] == '\t'))) mPos++;
        size_t e = mPos;
        while (e > s && (mText[e - 1] == ' ' || mText[e - 1] == '\t')) e--;
        return mText.substr(s, e - s);
    }
    static int Hash(const std::string& t) { unsigned h = 5381; for (char c : t) h = h * 33 + (unsigned char)c; return (int)(h % 100000); }
    std::string Take(const char* what)
    {
        std::string t = Token();
        if (!t.empty() && t[0] == '!') { tr(std::string(what) + " throws"); throw ConfigObjectException(std::string("bad ") + what); }
        return t;
    }
    int ReadInt()
    {
        std::string t = Take("ReadInt");
        int v = (!t.empty() && (isdigit((unsigned char)t[0]) || t[0] == '-')) ? atoi(t.c_str()) : Hash(t);
        tr("ReadInt " + P((long long)v)); return v;
    }
    bool ReadBool()
    {
        std::string t = Take("ReadBool");
        bool v;
        if (t == "true" || t == "1" || t == "TRUE" || t == "True") v = true;
        else if (t == "false" || t == "0" || t == "FALSE" || t == "False") v = false;
        else { tr("ReadBool throws"); throw ConfigObjectException("Expecting bool"); }
        tr("ReadBool " + P(v)); return v;
    }
    int ReadColor() { int v = Hash(Take("ReadColor")); tr("ReadColor " + P((long long)v)); return v; }
    int ReadChar()
    {
        Skip();
        if (mPos >= mText.size()) { tr("ReadChar throws"); throw ConfigObjectException("Expecting char"); }
        int v = (unsigned char)mText[mPos++]; tr("ReadChar " + P((long long)v)); return v;
    }
    VertAlign ReadVertAlign() { int v = Hash(Take("ReadVertAlign")) % 3; tr("ReadVertAlign " + P((long long)v)); return (VertAlign)v; }
    HorzAlign ReadHorzAlign() { int v = Hash(Take("ReadHorzAlign")) % 3; tr("ReadHorzAlign " + P((long long)v)); return (HorzAlign)v; }
    GUIString ReadString(bool word)
    {
        std::string t;
        if (word) { Skip(); t = Token(true); if (!t.empty() && t[0] == '!') { tr("ReadWord throws"); throw ConfigObjectException("bad ReadWord"); } }
        else t = Take("ReadString");
        if (!t.empty() && t[0] == '@') t = "res:" + t.substr(1);
        tr((word ? "ReadWord \"" : "ReadString \"") + t + "\""); return GUIString(t.c_str());
    }
    GUIString ReadFlag(bool* on)
    {
        Skip();
        if (mPos < mText.size() && mText[mPos] == '+') { mPos++; if (on) *on = true; }
        else if (mPos < mText.size() && mText[mPos] == '-') { mPos++; if (on) *on = false; }
        std::string t = Token(true);
        tr("ReadFlag \"" + t + "\""); return GUIString(t.c_str());
    }
    SmartPtr<Font> ReadFont() { std::string t = Take("ReadFont"); Font* f = new Font(Hash(t)); tr("ReadFont " + P(f)); return f; }
    SmartPtr<Image> ReadImage(bool)
    {
        std::string t = Take("ReadImage");
        Image* i = (!t.empty() && t[t.size() - 1] == 'S') ? new StretchImage(Hash(t)) : new Image(Hash(t));
        tr("ReadImage " + P(i)); return i;
    }
    SmartPtr<Sound> ReadSound() { Sound* s = new Sound(Hash(Take("ReadSound"))); tr("ReadSound " + P(s)); return s; }
    Background ReadBackground() { Background b; b.id = Hash(Take("ReadBackground")); tr("ReadBackground " + P(b)); return b; }
    SelectionColor ReadSelectionColor() { SelectionColor b; b.id = Hash(Take("ReadSelectionColor")); tr("ReadSelectionColor " + P(b)); return b; }
    Component* ReadComponent()
    {
        std::string t = Take("ReadComponent");
        Component* c = NULL;
        if (t != "null") {
            Component*& slot = sComps[t];
            if (!slot) slot = new Component(100 + (int)sComps.size(), t.find("all") != std::string::npos ? 15u : (unsigned)(Hash(t) & 15));
            c = slot;
        }
        tr("ReadComponent " + P(c)); return c;
    }
    void EnsureComma()
    {
        Skip();
        if (mPos < mText.size() && mText[mPos] == ',') { mPos++; tr("EnsureComma"); return; }
        tr("EnsureComma throws"); throw ConfigObjectException("Expecting ','");
    }
    bool EndOfString() { Skip(); bool e = mPos >= mText.size(); tr(std::string("EndOfString ") + (e ? "1" : "0")); return e; }
};
#define V24 virtual bool Virtual24(const std::string&)
#define BASE(n) virtual bool Virtual24(const std::string& key) { return Base(#n, key); }
// ContainerConfig's instructions (R6 WON round 40; the LAYOUT family, NEWLAYOUTPOINT and NOOVERLAPLAYOUT round 41).
// The layout state starts as the original's uninitialised members might (mLayoutRef is set by the constructor).
struct ConfigMapSlot {
    std::string n;
    ConfigMapSlot& operator=(const SmartPtr<ComponentConfig>& c)
    { tr("configmap[" + n + "]=config(" + PP(c->mComponent.get()) + "," + (c->mParent ? "parent" : "orphan") + ")"); return *this; }
};
struct ConfigMap { ConfigMapSlot operator[](const std::string& k) { ConfigMapSlot s; s.n = k; return s; } };
struct ContainerConfig : ComponentConfig {
    V24; Component* mContainer = gComp; int mCurLayoutId = 0;
    Component* mLayoutRef = NULL; int mLayoutInstruction = 0x5a5; int mLayoutParams[4] = {11, -22, 33, -44};
    ConfigMap mConfigMap;
    bool HandleAdd(); bool HandleAddLayoutPoint(); bool HandleSameSizeLayout(bool, bool); bool HandleCenterLayout(bool, bool);
    bool HandleClearRightLayout(); bool HandleSpaceEvenlyLayout(bool); bool HandleSizeAtLeastLayout();
    bool HandleLayout(bool chain = false, bool repeat = false); bool HandleNewLayoutPoint(); bool HandleNoOverlapLayout();
};
struct ImageConfig : ComponentConfig { V24; SmartPtr<Image> mImage = new Image(77); };
static int sNextStretch;
struct StretchImageConfig : ImageConfig {
    V24; StretchImage* mStretchImage;
    StretchImageConfig() { mStretchImage = new StretchImage(sNextStretch++); mImage = mStretchImage; }
};
struct ImageFilterConfig : ImageConfig {
    V24; SmartPtr<RawImage> mRawImage = new RawImage(99, 40, 20); bool mImageIsRawImage = false;
    SmartPtr<RawImage> HandleFilter();
    // (a filter that made no image leaves an empty one here: the original's next use of it would crash)
    void SetRawImage(RawImage* r)
    {
        static RawImage sEmpty(0, 0, 0);
        tr("SetRawImage(" + P(r) + ")"); mRawImage = r ? r : &sEmpty; mImage = r; mImageIsRawImage = true;
    }
};
// the configs' parent: the children added to its map
static ComponentConfig sParentConfig;
ComponentConfig* ComponentConfig::GetParentObject() { return &sParentConfig; }
void ComponentConfig::AddChildToConfigMap(const GUIString& name, ComponentConfig* c)
{
    std::string what = "config";
    if (StretchImageConfig* s = dynamic_cast<StretchImageConfig*>(c)) what = "stretch#" + P((long long)s->mStretchImage->id);
    else if (ImageFilterConfig* f = dynamic_cast<ImageFilterConfig*>(c)) what = "filter " + P(f->mRawImage.get());
    tr("parent.AddChildToConfigMap(" + P(name) + "," + what + ")");
}
struct ButtonConfig : ComponentConfig { V24; Component* mButton = gComp; void HandleButtonFlags(); void HandleFitText() { tr("HandleFitText"); } };
struct ScrollAreaConfig : ContainerConfig { V24; Component* mScrollArea = gComp; };
struct ScrollerConfig : ContainerConfig { V24; Component* mScroller = gComp; };
struct ScrollbarScrollerConfig : ScrollerConfig { V24; ScrollbarCondition GetScrollbarCondition(); Component* mScrollbarScroller = gComp; };
struct ListAreaConfig : ScrollAreaConfig { V24; Component* mListArea = gComp; ListSortType ReadSortType(); static void ReadListFlags(ComponentConfig*, ListArea*); };
struct MultiListAreaConfig : ListAreaConfig { V24; Component* mMultiListArea = gComp; };
struct TreeAreaConfig : ListAreaConfig { V24; Component* mTreeArea = gComp; };
struct MultiListHeaderConfig : ContainerConfig { V24; Component* mHeader = gComp; };
struct MSHeaderControlConfig : MultiListHeaderConfig { V24; Component* mMSHeader = gComp; };
struct WONHeaderControlConfig : MultiListHeaderConfig { V24; };
struct ScrollbarConfig : ContainerConfig { V24; Component* mScrollbar = gComp; };
struct MSScrollbarConfig : ScrollbarConfig { V24; Component* mMSScrollbar = gComp; };
struct WONScrollbarConfig : ScrollbarConfig { V24; };
struct TabCtrlConfig : ContainerConfig { V24; Component* mTabCtrl = gComp; };
struct MSTabCtrlConfig : TabCtrlConfig { V24; Component* mMSTabCtrl = gComp; };
struct WONTabCtrlConfig : TabCtrlConfig { V24; };
struct ComboBoxConfig : ContainerConfig { V24; Component* mComboBox = gComp; };
struct MSComboBoxConfig : ComboBoxConfig { V24; Component* mMSComboBox = gComp; };
struct WONComboBoxConfig : ComboBoxConfig { V24; };
struct MSButtonConfig : ButtonConfig { V24; Component* mMSButton = gComp; };
struct MSTabButtonConfig : MSButtonConfig { V24; Component* mMSTabButton = gComp; };
struct MSArrowButtonConfig : ButtonConfig { V24; Component* mMSArrowButton = gComp; };
struct MS3DFrameConfig : ComponentConfig { V24; Component* mMS3DFrame = gComp; };
struct MSEtchedFrameGroupConfig : ContainerConfig { V24; Component* mFrame = gComp; };
struct MSScrollContainerConfig : ScrollbarScrollerConfig { V24; Component* mScrollContainer = gComp; };
struct MSListCtrlConfig : MSScrollContainerConfig { V24; Component* mListCtrl = gComp; };
struct WONScrollContainerConfig : ScrollbarScrollerConfig { V24; Component* mScrollContainer = gComp; };
struct WONListCtrlConfig : WONScrollContainerConfig { V24; Component* mListCtrl = gComp; };
struct PercentLayoutConfig : ComponentConfig { V24; Component* refs[4] = {}; int params[8] = {}; };
struct ColorSchemeConfig : ComponentConfig { V24; SmartPtr<ColorScheme> mColorScheme = new ColorScheme; };
struct RectangleComponentConfig : ComponentConfig { V24; Component* mRectangleComponent = gComp; };
struct RectangleComponent3DConfig : ComponentConfig { V24; Component* mRectangleComponent3D = gComp; };
struct ImageComponentConfig : ComponentConfig { V24; Component* mImageComponent = gComp; };
struct ScreenContainerConfig : ContainerConfig { V24; Component* mScreenContainer = gComp; };
struct BrowserComponentConfig : ContainerConfig { V24; Component* mBrowserComponent = gComp; };
struct DialogConfig : ContainerConfig { V24; Component* mDialog = gComp; };
struct ImageButtonConfig : ComponentConfig { V24; Component* mImageButton = gComp; };
struct LabelConfig : ComponentConfig { V24; Component* mLabel = gComp; };
struct InputBoxConfig : ComponentConfig { V24; Component* mInputBox = gComp; };
struct TextAreaConfig : ScrollAreaConfig { V24; Component* mTextArea = gComp; };
struct WONSimpleButtonConfig : ButtonConfig { V24; Component* mButton = gComp; void HandleButtonState(); void HandleButtonStates(); };
struct WONButtonConfig : ButtonConfig { V24; Component* mButton = gComp; void HandleSetButtonState(); };
struct SplitterConfig : ContainerConfig {
    V24; Component* mSplitter = gComp; Splitter::QuadrantPosition ReadQuadrant(); Splitter::SplitterType ReadSplitterType();
};
struct WONFrameComponentConfig : ComponentConfig { V24; Component* mFrameComponent = gComp; };
struct StretchCenterImageConfig : ImageConfig { V24; Component* mStretchCenterImage = gComp; };
struct FontDescriptor { Field mSize{NULL, "mDescriptor.mSize"}; int mStyle = 0; };
struct FontConfig : ComponentConfig {
    V24; Field mNeedRecalcFont{NULL, "mNeedRecalcFont"}; FontDescriptor mDescriptor;
    void SetFont(Font* f) { tr("SetFont(" + P(f) + ")"); }
};
struct WONSkinConfig : ComponentConfig { V24; bool ReadVert(); void HandleButtonClickSound() { tr("HandleButtonClickSound"); } };
#undef V24
#undef BASE
// The configs by name, for the driver.
#define EE_CFGTAB_CONFIGS(X) X(ButtonConfig) X(MSButtonConfig) X(MSArrowButtonConfig) X(MSTabButtonConfig) X(MS3DFrameConfig) \
    X(MSComboBoxConfig) X(MSScrollbarConfig) X(MSEtchedFrameGroupConfig) X(MSTabCtrlConfig) X(MSScrollContainerConfig) \
    X(MSHeaderControlConfig) X(MSListCtrlConfig) X(PercentLayoutConfig) X(ColorSchemeConfig) X(RectangleComponentConfig) \
    X(RectangleComponent3DConfig) X(ImageComponentConfig) X(ScreenContainerConfig) X(MultiListAreaConfig) \
    X(MultiListHeaderConfig) X(ScrollbarConfig) X(TabCtrlConfig) X(BrowserComponentConfig) X(ComboBoxConfig) X(DialogConfig) \
    X(ImageButtonConfig) X(LabelConfig) X(ListAreaConfig) X(InputBoxConfig) X(TextAreaConfig) X(TreeAreaConfig) \
    X(WONSimpleButtonConfig) X(WONButtonConfig) X(SplitterConfig) X(WONScrollbarConfig) X(WONHeaderControlConfig) \
    X(WONListCtrlConfig) X(WONFrameComponentConfig) X(WONComboBoxConfig) X(WONTabCtrlConfig) X(WONScrollContainerConfig) \
    X(ScrollAreaConfig) X(ScrollerConfig) X(ScrollbarScrollerConfig) X(StretchCenterImageConfig) X(ComponentConfig) X(ContainerConfig) X(WONSkinConfig) X(FontConfig) X(ImageConfig) \
    X(StretchImageConfig) X(ImageFilterConfig)
static ComponentConfig* MakeConfig(int i)
{
    int n = 0;
#define EE_CFGTAB_MK(T) if (n++ == i) return new T;
    EE_CFGTAB_CONFIGS(EE_CFGTAB_MK)
#undef EE_CFGTAB_MK
    return NULL;
}
// Instructions in turn on a new config of kind i: the trace, each result or exception, the layout config's arrays
// and state.
static std::string RunSeq(int i, const std::vector<std::pair<std::string, std::string>>& steps)
{
    sTrace.clear(); sComps.clear(); sNextComp = 1000; sNextLayout = 1; sNextPoint = 0; sNextRaw = 3000; sNextStretch = 500;
    gComp = new Component(1); gComp->mEdit = new Component(2);
    ComponentConfig* c = MakeConfig(i);
    for (const auto& s : steps) {
        c->mText = s.second; c->mPos = 0;
        try { tr(c->Virtual24(s.first) ? "=> 1" : "=> 0"); }
        catch (const ConfigObjectException& e) { tr("throws " + e.mText); }
    }
    if (FontConfig* f = dynamic_cast<FontConfig*>(c)) tr("style " + P((long long)f->mDescriptor.mStyle));
    if (ImageConfig* m = dynamic_cast<ImageConfig*>(c)) tr("image " + P(m->mImage));
    if (ContainerConfig* k = dynamic_cast<ContainerConfig*>(c))
        tr("layout id " + P((long long)k->mCurLayoutId) + " ref " + PP(k->mLayoutRef) + " rule " + P((long long)k->mLayoutInstruction) + " "
           + P((long long)k->mLayoutParams[0]) + " " + P((long long)k->mLayoutParams[1]) + " " + P((long long)k->mLayoutParams[2]) + " "
           + P((long long)k->mLayoutParams[3]));
    if (PercentLayoutConfig* p = dynamic_cast<PercentLayoutConfig*>(c)) {
        std::string s = "refs";
        for (Component* r : p->refs) s += " " + P(r);
        s += " params";
        for (int v : p->params) s += " " + P((long long)v);
        tr(s);
    }
    tr("pos " + P((long long)c->mPos) + " set " + P(c->mHasSetWidth) + P(c->mHasSetHeight));
    return sTrace;
}
static std::string RunOne(int i, const std::string& key, const std::string& value) { return RunSeq(i, {{key, value}}); }
