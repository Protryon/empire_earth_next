// won_compcfg_test.cpp's subclasses and steps, included in each namespace after its ComponentConfig, ResourceConfig
// and ResourceConfigTable (Orig: the original classes; Port: the game's). CC_ADD_ALWAYS is NEWLAYOUTPOINT's map entry
// in each (the original's mConfigMap[name] = config).
struct ComponentInit { int mType; };
struct ComponentConfigInit : ComponentInit { ComponentConfigInit(ComponentConfig* c) : mConfig(c) { mType = 2; } ComponentConfig* mConfig; };
void ComponentConfig::SetComponent(Component* c) { this->mComponent = c; }
bool ComponentConfig::Virtual24(const std::string& k)
{
    if (k == "MODIFY") return this->HandleModify();
    if (k == "MAPCOMPONENT") return this->HandleMapComponent();
    return false;
}
// a font config holds a font every other one; an image config an image and a raw image the same way
class FontConfig : public ComponentConfig {
public:
    SmartPtr<Font> mFont;
    FontConfig() { if (sNextId % 2) mFont = new Font("cfgfont", 1, 9); }
    Font* GetFont() { return mFont.get(); }
    static ComponentConfig* CfgFactory() { return new FontConfig; }
};
class ImageConfig : public ComponentConfig {
public:
    SmartPtr<Image> mImage; SmartPtr<RawImage> mRaw;
    ImageConfig() { if (sNextId % 3) mImage = new Image("cfgimg"); if (sNextId % 2) mRaw = new RawImage("cfgraw"); }
    virtual Image* GetImage() { return mImage.get(); }
    virtual RawImage* GetRawImage() { return mRaw.get(); }
    static ComponentConfig* CfgFactory() { return new ImageConfig; }
};
static ComponentConfig* PlainFactory() { return new ComponentConfig; }
// the config classes the resource table registers, each with the original class (Orig) or the kind (Port), CC_KIND_FACTORY
#define CC_KINDS(X) X(KComponent, ComponentConfig, Component) X(KContainer, ContainerConfig, Container) X(KDialog, DialogConfig, Dialog) \
    X(KButton, ButtonConfig, Button) X(KImageButton, ImageButtonConfig, ImageButton) X(KLabel, LabelConfig, Label) \
    X(KInputBox, InputBoxConfig, InputBox) X(KScrollArea, ScrollAreaConfig, ScrollArea) X(KListArea, ListAreaConfig, ListArea) \
    X(KMultiListArea, MultiListAreaConfig, MultiListArea) X(KTreeArea, TreeAreaConfig, TreeArea) X(KTextArea, TextAreaConfig, TextArea) \
    X(KMultiListHeader, MultiListHeaderConfig, MultiListHeader) X(KScroller, ScrollerConfig, Scroller) \
    X(KScrollbarScroller, ScrollbarScrollerConfig, ScrollbarScroller) X(KScrollbar, ScrollbarConfig, Scrollbar) X(KTabCtrl, TabCtrlConfig, TabCtrl) \
    X(KComboBox, ComboBoxConfig, ComboBox) X(KSplitter, SplitterConfig, Splitter) X(KBrowser, BrowserComponentConfig, Browser) \
    X(KRectangle, RectangleComponentConfig, Rectangle) X(KRectangle3D, RectangleComponent3DConfig, Rectangle3D) \
    X(KImageComponent, ImageComponentConfig, ImageComponent) X(KScreenContainer, ScreenContainerConfig, ScreenContainer) \
    X(KMsButton, MSButtonConfig, MsButton) X(KMsArrowButton, MSArrowButtonConfig, MsArrowButton) X(KMsTabButton, MSTabButtonConfig, MsTabButton) \
    X(KMs3DFrame, MS3DFrameConfig, Ms3DFrame) X(KMsComboBox, MSComboBoxConfig, MsComboBox) X(KMsScrollbar, MSScrollbarConfig, MsScrollbar) \
    X(KMsEtchedFrameGroup, MSEtchedFrameGroupConfig, MsEtchedFrameGroup) X(KMsTabCtrl, MSTabCtrlConfig, MsTabCtrl) \
    X(KMsScrollContainer, MSScrollContainerConfig, MsScrollContainer) X(KMsHeaderControl, MSHeaderControlConfig, MsHeaderControl) \
    X(KMsListCtrl, MSListCtrlConfig, MsListCtrl) X(KWonSimpleButton, WONSimpleButtonConfig, WonSimpleButton) \
    X(KWonButton, WONButtonConfig, WonButton) X(KWonScrollbar, WONScrollbarConfig, WonScrollbar) \
    X(KWonScrollContainer, WONScrollContainerConfig, WonScrollContainer) X(KWonHeaderControl, WONHeaderControlConfig, WonHeaderControl) \
    X(KWonListCtrl, WONListCtrlConfig, WonListCtrl) X(KWonComboBox, WONComboBoxConfig, WonComboBox) X(KWonTabCtrl, WONTabCtrlConfig, WonTabCtrl) \
    X(KWonFrameComponent, WONFrameComponentConfig, WonFrameComponent) X(KWonSkin, WONSkinConfig, WonSkin)
static int sCompSeed;
static Component* CompFactory() { sCompSeed = sCompSeed * 7 + 3; return new Component(sCompSeed % 50, sCompSeed % 31, sCompSeed % 13 == 0 ? 13 : sCompSeed % 120, sCompSeed % 7 == 0 ? 0 : sCompSeed % 80); }
// a component with the parts the kinds' predefined children read
static Component* KindComp()
{
    Component* c = CompFactory();
    c->mListArea = CompFactory(); c->mEdit = CompFactory(); c->mContainerLayout = CompFactory();
    for (int i = 0; i < 10; i++) c->mPart[i] = CompFactory();
    return c;
}
// the test config: named values and functions, predefined children, and every reader by instruction key
// (every config the steps make has a component and a parent up to the root, as the game's: the original's reads of a
// config without a component crash, its (CFG,name) font search walked past a root without the font config)
static ComponentConfig* sRoot;
static ComponentConfig* NewChild()
{
    ComponentConfig* c = new ComponentConfig;
    c->SetComponent(new Component(5, 6, 7, 8));
    c->mParent = sRoot;
    return c;
}
class TestConfig : public ComponentConfig {
public:
    static ComponentConfig* CfgFactory() { return new TestConfig; }
    virtual bool GetNamedValue(const std::string& k, int& v)
    {
        if (k == "SEVEN") { v = 7; return true; }
        if (k == "BOOM") throw ConfigObjectException("boom");
        return ComponentConfig::GetNamedValue(k, v);
    }
    virtual bool CallNamedFunction(const std::string& k, const std::vector<int>& p, int& v)
    {
        if (k == "TWICE") { v = p.empty() ? -1 : 2 * p[0]; return true; }
        return ComponentConfig::CallNamedFunction(k, p, v);
    }
    virtual bool GetPredefinedComponent(const std::string& n, ComponentConfig*& c, Component*& m)
    {
        tr("Predefined(" + n + ")");
        if (n == "PRE") { c = new ComponentConfig; m = new Component(1, 2, 3, 4); return true; }
        if (n == "HALF") { c = NewChild(); m = NULL; return true; }
        return ComponentConfig::GetPredefinedComponent(n, c, m);
    }
    virtual bool Virtual24(const std::string& k);
};
bool TestConfig::Virtual24(const std::string& k)
{
    std::string r;
    if (k == "INT") r = P(this->ReadInt());
    else if (k == "BOOL") r = P(this->ReadBool());
    else if (k == "CHAR") r = P(this->ReadChar());
    else if (k == "FLAG") { bool on = true; GUIString f = this->ReadFlag(&on); r = PS(f) + P(on); }
    else if (k == "FLAGN") r = PS(this->ReadFlag());
    else if (k == "RAW") r = PS(this->ReadRawString(false));
    else if (k == "STR") r = PS(this->ReadString(false));
    else if (k == "WORD") r = PS(this->ReadString(true));
    else if (k == "DIR") r = P(this->ReadDirection());
    else if (k == "HALIGN") r = P(this->ReadHorzAlign());
    else if (k == "VALIGN") r = P(this->ReadVertAlign());
    else if (k == "COLOR") r = P(this->ReadColor());
    else if (k == "SOUND") r = PP(this->ReadSound().get());
    else if (k == "BG") r = PB(this->ReadBackground());
    else if (k == "FONT") r = PP(this->ReadFont().get());
    else if (k == "IMAGE") r = PP(this->ReadImage(false).get());
    else if (k == "RAWIMAGE") r = PP(this->ReadRawImage().get());
    else if (k == "RAWSTR") { std::string s = (std::string)this->ReadRawString(true); r = PP(this->GetRawImageFromStr(s).get()); }
    else if (k == "STDCURSOR") r = P(this->ReadStandardCursor());
    else if (k == "CURSOR") r = P(this->ReadCursor()->mType);
    else if (k == "SEL") { SelectionColor s = this->ReadSelectionColor(); r = P(s.mBackgroundColor) + "," + P(s.mForegroundColor) + "," + P(s.mMode); }
    else if (k == "COMP") r = PP(this->ReadComponent());
    else if (k == "EOS") r = P(this->EndOfString());
    else if (k == "COMMA") { this->EnsureComma(); r = "comma"; }
    else if (k == "CHILD") r = PP(this->Fn386860((std::string)this->ReadRawString(true)));
    else if (k == "SIZE") { this->SetDesiredSizeIfNecessary(); r = "sized"; }
    else if (k == "ADDCHILD") { ComponentConfig* c = NewChild(); this->AddChildToConfigMap(this->ReadRawString(true), c); r = "added"; }
    else if (k == "ADDALWAYS") { ComponentConfig* c = NewChild(); CC_ADD_ALWAYS(this->ReadRawString(true), c); r = "added"; }
    else if (k == "FILE") r = ComponentConfig::GetResourceFile((std::string)this->ReadRawString(true));
    else if (k == "FOLDER") { // (the original read the byte before an empty folder)
        std::string f = (std::string)this->ReadRawString(true);
        if (!f.empty()) ComponentConfig::SetResourceFolder(f);
        r = "folder";
    }
    else if (k == "INIT") { this->InitComponents(); r = "inited"; }
    else return ComponentConfig::Virtual24(k);
    tr(PP(this) + " " + k + " -> " + r);
    return true;
}
static void Glue1(ResourceConfigTable* t) { tr("Glue1"); }
static void Glue2(ResourceConfigTable* t) { tr("Glue2"); if (sCompSeed % 5 == 0) throw ConfigObjectException("glue"); }

// the kinds' hooks probed directly (R6 WONLobby round 26): a predefined child's config, bound to its part, then every
// predefined name on it in turn (its class's set: the hierarchy the kinds walk) and its named paddings
static const char* const kPreNames[] = {"LISTAREA", "INPUTBOX", "THUMB", "UPARROW", "DOWNARROW", "SCROLLRECT", "CONTAINERLAYOUT",
    "HTMLVIEW", "HORZSCROLLBAR", "VERTSCROLLBAR", "HEADER", "MULTILISTAREA", "LABEL", "header", "PRE", "NONE", ""};
static const char* const kPadNames[] = {"LEFTPAD", "RIGHTPAD", "TOPPAD", "BOTTOMPAD", "HORZPAD", "VERTPAD", "leftpad", "WIDTH", "NOPE"};
static std::string Probe(ComponentConfig* c, int depth)
{
    std::string r;
    for (size_t i = 0; i < sizeof kPreNames / sizeof *kPreNames; i++) {
        ComponentConfig* k = NULL; Component* m = NULL;
        bool got = c->GetPredefinedComponent(kPreNames[i], k, m);
        if (!got) continue;
        SmartPtr<ComponentConfig> keep = k;
        r += std::string(" ") + kPreNames[i] + "=" + ClassTag(k) + "," + PP(m);
        if (k != NULL && m != NULL && depth > 0) { k->SetComponent(m); r += "{" + Probe(k, depth - 1) + "}"; }
    }
    for (size_t i = 0; i < sizeof kPadNames / sizeof *kPadNames; i++) {
        int v = -1;
        bool got = c->GetNamedValue(kPadNames[i], v);
        r += " " + P(got) + ":" + P(v);
    }
    return r;
}
// one run: the table, the allocators, control ids and glue functions, a root test config, then the steps (the same
// script on both sides: the test's generator); after each step the census
struct Step { int kind; int target; std::string a, b; Script script; };
static ComponentConfig* Target(int i)
{
    std::vector<ComponentConfig*> v;
    for (std::map<int, RefCount*>::iterator it = Live().begin(); it != Live().end(); ++it)
        if (ComponentConfig* c = dynamic_cast<ComponentConfig*>(it->second)) if (c->mComponent.get()) v.push_back(c);
    return v.empty() ? NULL : v[(size_t)i % v.size()];
}
static std::string Run(const std::vector<Step>& steps)
{
    sTrace.clear(); sCompSeed = 1; sNextId = 0;
    ClearStatics();
    ConfigParser aParser;
    SmartPtr<ResourceConfigTable> aTable = new ResourceConfigTable;
    ComponentConfig::mResourceConfigTable = aTable;
    ComponentConfig::SetResourceFolder("res");
    aTable->RegisterGlueResourceFunc(Glue2);
    aTable->RegisterGlueResourceFunc(Glue1);
    aTable->RegisterGlueResourceFunc(Glue2);
    aTable->RegisterControlId("FOO", 11);
    aTable->RegisterControlId("Bar", 22);
    aTable->RegisterControlId("foo", 33);
    aTable->AddAllocator("Comp", PlainFactory, CompFactory);
    aTable->AddAllocator("Font", FontConfig::CfgFactory, CompFactory);
    aTable->AddAllocator("Image", ImageConfig::CfgFactory, CompFactory);
    aTable->AddAllocator("Test", TestConfig::CfgFactory, CompFactory);
    aTable->AddAllocator("TEST", PlainFactory, CompFactory);
#define CC_ALLOC(n, C, K) aTable->AddAllocator(#n, CC_KIND_FACTORY(C, K), KindComp);
    CC_KINDS(CC_ALLOC)
#undef CC_ALLOC
    SmartPtr<ComponentConfig> aRoot = new TestConfig;
    aRoot->mName = "root";
    sRoot = aRoot.get();
    aRoot->SetComponent(new Component(10, 20, 100, 50));
    // (a font config under the root: the original's (CFG,name) font search walked past the root when none answered)
    ComponentConfig* aFont = new FontConfig;
    aFont->SetComponent(new Component(1, 1, 1, 1));
    aRoot->AddChildToConfigMap("Font1", aFont);
    for (size_t i = 0; i < steps.size(); i++) {
        const Step& st = steps[i];
        sScript = st.script;
        SmartPtr<ComponentConfig> c = Target(st.target);
        tr("step " + P((long long)i) + " " + P(st.kind) + " on " + PP(c.get()) + " " + st.a + " | " + st.b);
        try {
            StringParser v(st.b.c_str());
            switch (st.kind) {
            case 0: tr("= " + P(c->HandleValueInstruction(&aParser, st.a, v)) + " at " + P(v.mPos)); break;
            case 1: tr("= " + P(c->HandleInstruction(&aParser, st.a))); break;
            case 2: c->mBaseName = st.a; tr("= " + P(c->PreParse(&aParser))); break;
            case 3: tr("= " + P(c->PostParse(&aParser))); break;
            case 4: c->InitComponents(); break;
            case 5: aTable->SafeGlueResources(); break;
            case 6: {
                ResourceConfig* g = aTable->SafeGetConfig(st.a);
                tr("string " + PS(g->SafeGetString(st.b)));
                tr("image " + PP(g->SafeGetImage(st.b)));
                tr("sound " + PP(g->SafeGetSound(st.b)));
                break;
            }
            case 7: { bool e = false; tr("= " + PP(aTable->AddConfig(st.a, &e)) + P(e)); break; }
            case 8: tr("= " + PP(c->Fn386860(st.a))); break;
            case 10: tr("probe" + Probe(c, 2)); break;
            default: {
                SmartPtr<Sound> s = new Sound("added", 0);
                aTable->SafeGetConfig(st.a)->AddSound(st.b, s.get());
                tr("added " + PP(s.get()));
                break;
            }
            }
        } catch (ConfigObjectException& e) { tr("threw " + e.mText); }
        tr("@" + Census());
    }
    aRoot = NULL;
    ComponentConfig::mResourceConfigTable = NULL;
    aTable = NULL;
    tr("end@" + Census());
    // what is left (the configs the predefined HALF leaked, the cycles MAPCOMPONENT makes) is forgotten, not freed
    Live().clear();
    return sTrace;
}
