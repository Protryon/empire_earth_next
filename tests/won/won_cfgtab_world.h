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
struct Image : Obj { Image(int i) : Obj(i) {} };
struct StretchImage : Image { StretchImage(int i) : Image(i) {} };
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
static std::string PP(const Component* c) { return c ? "c" + P((long long)c->id) : "null"; }
typedef Component Container; typedef Component Button; typedef Component Scroller; typedef Component MultiListArea;
typedef Component ScrollArea; typedef Component ListArea; typedef Component InputBox; typedef Component Splitter;
typedef Component WONButton; typedef Component WONSimpleButton; typedef Component WONHeaderButton; typedef Component WONScrollbar;
typedef Component WONScrollContainer; typedef Component WONListCtrl; typedef Component WONTabCtrl; typedef Component WONComboBox;
typedef Component WONFrameComponent;
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
    return c;
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
    bool mHasSetWidth = false, mHasSetHeight = false;
    bool HandleModify() { tr("HandleModify"); return mText.size() % 2 == 0; }
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
    GUIString ReadString(bool)
    {
        std::string t = Take("ReadString");
        if (!t.empty() && t[0] == '@') t = "res:" + t.substr(1);
        tr("ReadString \"" + t + "\""); return GUIString(t.c_str());
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
struct ContainerConfig : ComponentConfig { BASE(ContainerConfig) };
struct ImageConfig : ComponentConfig { BASE(ImageConfig) };
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
    X(ScrollAreaConfig) X(ScrollerConfig) X(ScrollbarScrollerConfig) X(StretchCenterImageConfig) X(ComponentConfig) X(WONSkinConfig)
static ComponentConfig* MakeConfig(int i)
{
    int n = 0;
#define EE_CFGTAB_MK(T) if (n++ == i) return new T;
    EE_CFGTAB_CONFIGS(EE_CFGTAB_MK)
#undef EE_CFGTAB_MK
    return NULL;
}
// One instruction on a new config of kind i: the trace, the result or exception, the layout config's arrays.
static std::string RunOne(int i, const std::string& key, const std::string& value)
{
    sTrace.clear(); sComps.clear(); sNextComp = 1000;
    gComp = new Component(1); gComp->mEdit = new Component(2);
    ComponentConfig* c = MakeConfig(i);
    c->mText = value;
    try { tr(c->Virtual24(key) ? "=> 1" : "=> 0"); }
    catch (const ConfigObjectException& e) { tr("throws " + e.mText); }
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
