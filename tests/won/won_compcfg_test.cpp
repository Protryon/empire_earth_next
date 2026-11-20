// ComponentConfig's readers, look-ups, config map and instructions, ResourceConfig and ResourceConfigTable whole in
// Rust (R6 WONLobby round 25: ee_won_gui compcfg.rs): the deleted units' statements (won_compcfg_orig.h, namespace
// Orig, over the original classes' members) against the game's forwards and host (include/ee/wonapi/WONCompCfg.inl,
// namespace Port, over compcfg.rs), both over one stand-in world (won_compcfg_world.h: counted objects, decoders,
// fonts, components, a scripted ConfigObject parse) and one test config (won_compcfg_run.h: every reader by key,
// named values and functions, predefined children).
//
// Inputs: random runs of 60 steps on random configs of the tree the steps grow: value instructions (every reader,
// SETRESOURCE, MODIFY, MAPCOMPONENT, the base's keys, unknown ones) over values built from numbers, expressions,
// control ids, int, string, colour, font, image, sound and background resources, named values and functions of
// dotted configs, colours, fonts, sounds, images, backgrounds, cursors, components, junk; group instructions (RESOURCE
// groups whose lines are every resource kind, MODIFY groups, each allocator's type, the base's, unknown ones);
// PreParse with and without a base group, PostParse, InitComponents, the glue functions, the groups' SafeGet*, AddConfig,
// Fn386860 and AddSound. Each step's trace (every world call, every result or message, the parser's position) and the
// census of live objects and their references after it must be identical. Exit code: the number of mismatching runs.
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <list>
#include <exception>
#include <typeinfo>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "platform/PlatString.h"
#include "rust/won_gui/compcfg.h"
#include "rust/won_gui/guiconf.h"
#include "rust/won_gui/guicfg.h"
#include "rust/won_gui/cfgkind.h"
#include "rust/won_gui/shell.h" // the shell ops the kinds' parts are read with
#include <cctype>
#include "rust/won_net/ffi_parse.h"
wchar_t gEmptyWideString[2]; // EEWonNet.cpp's (GetEmptyGUIStringData)

namespace Orig {
#include "won_compcfg_world.h"
class ComponentConfig;
class ResourceConfigTable;
// the original ResourceConfig and ResourceConfigTable (d28b5863a's WONLobbyResource.h; the statics public here)
class ResourceConfig : public ConfigObject {
protected:
    virtual ~ResourceConfig() {}
    bool mAllowOverwrite;
    int mParseCount;
    template<class T> class ResMap : public std::map<std::string,std::pair<T,int>,StringLessNoCase> {};
    typedef ResMap<GUIString> StringMap;
    typedef ResMap<SmartPtr<Image> > ImageMap;
    typedef ResMap<SmartPtr<Font> > FontMap;
    typedef ResMap<DWORD> ColorMap;
    typedef ResMap<Background> BackgroundMap;
    typedef ResMap<int> IntMap;
    typedef ResMap<SmartPtr<Sound> > SoundMap;
    StringMap mStringMap; ImageMap mImageMap; FontMap mFontMap; ColorMap mColorMap; BackgroundMap mBackgroundMap; IntMap mIntMap; SoundMap mSoundMap;
    bool HandleString(StringParser&); bool HandleImage(StringParser&); bool HandleSound(StringParser&);
    bool HandleFont(StringParser&); bool HandleColor(StringParser&); bool HandleBackground(StringParser&);
public:
    ResourceConfig() { this->mAllowOverwrite = false; this->mParseCount = 0; } // world: ResourceConfigLifetime.cpp's
    virtual bool HandleValueInstruction(ConfigParser*,const std::string&,StringParser&);
    virtual bool PreParse(ConfigParser*);
    void SetAllowOverwrite(bool value) { this->mAllowOverwrite=value; }
    bool GetString(const std::string&,GUIString&); bool GetImage(const std::string&,Image*&); bool GetFont(const std::string&,Font*&);
    bool GetSound(const std::string&,Sound*&); bool GetColor(const std::string&,DWORD&); bool GetBackground(const std::string&,Background&);
    bool GetInt(const std::string&,int&);
    const GUIString& SafeGetString(const std::string&); Image* SafeGetImage(const std::string&); Sound* SafeGetSound(const std::string&);
    void AddSound(const std::string&,Sound*);
    static void EnsureComma(StringParser&); static void EnsureCloseParen(StringParser&);
    static int SafeReadColor(StringParser&); static int SafeReadFontStyle(StringParser&);
    static SmartPtr<Font> SafeReadFont(StringParser&); static SmartPtr<Sound> SafeReadSound(StringParser&);
    static SmartPtr<Image> SafeReadImage(StringParser&); static Background SafeReadBackground(StringParser&);
};
class ResourceConfigTable : public RefCount {
public:
    typedef void (*GlueResourceFunc)(ResourceConfigTable*);
    typedef ComponentConfig* (*ComponentConfigFactory)();
    typedef Component* (*ComponentFactory)();
    ResourceConfigTable() {}
    ResourceConfig* AddConfig(const std::string&,bool* = NULL);
    ResourceConfig* SafeGetConfig(const std::string&);
    void RegisterGlueResourceFunc(GlueResourceFunc);
    void SafeGlueResources();
    void RegisterControlId(const std::string&,int);
    void AddAllocator(const std::string&,ComponentConfigFactory,ComponentFactory);
    bool GetControlId(const std::string&,int&);
    bool AllocateConfig(const std::string&,SmartPtr<ComponentConfig>&,SmartPtr<Component>&);
    void ClearGlueResourceFuncs(); void ClearAllocators(); void ClearControlIds(); void ClearResources(); void ClearAll();
    typedef std::map<std::string,SmartPtr<ResourceConfig>,StringLessNoCase> ResourceMap;
    ResourceMap mResourceMap;
    typedef std::set<GlueResourceFunc> GlueResourceFuncSet;
    GlueResourceFuncSet mGlueResourceFuncSet;
    typedef std::map<std::string,int,StringLessNoCase> ControlIdMap;
    static ControlIdMap mControlIdMap;
    struct ConfigAllocator {
        ComponentConfigFactory mConfigFactory; ComponentFactory mComponentFactory;
        ConfigAllocator(ComponentConfigFactory a,ComponentFactory b) : mConfigFactory(a),mComponentFactory(b) { }
    };
    typedef std::map<std::string,ConfigAllocator,StringLessNoCase> ConfigAllocatorMap;
    static ConfigAllocatorMap mConfigAllocatorMap;
};
// the original ComponentConfig (d28b5863a's WONLobbyGUI.h, the members its statements use)
class ComponentConfig : public ConfigObject {
public:
    void InitComponents();
    Direction ReadDirection(); HorzAlign ReadHorzAlign(); VertAlign ReadVertAlign();
    static void SetResourceFolder(const std::string&);
    static std::string mResourceFolder;
    SmartPtr<Sound> ReadSound();
    void EnsureComma(); int GetIntValue(); int ParseExpression(int); int ReadChar();
    GUIString ReadFlag(bool* = NULL); GUIString ReadRawString(bool);
    SmartPtr<Font> ReadFont(); SmartPtr<Image> ReadImage(bool); SelectionColor ReadSelectionColor();
    ComponentConfig* GetComponentConfig(const GUIString&,bool); ComponentConfig* ReadComponentConfig();
    bool HandleMapComponent(); bool HandleModifyGroup(ConfigParser&); bool HandleResourceGroup(ConfigParser&); bool HandleModify();
    bool BaseHandleInstruction(ConfigParser&,const std::string&,StringParser&);
    void SetDesiredSizeIfNecessary();
    SmartPtr<Cursor> ReadCursor();
    int GetIntResource(const std::string&);
    static SmartPtr<ResourceConfigTable> mResourceConfigTable;
    SmartPtr<RawImage> ReadRawImage();
    SmartPtr<Image> GetImageFromStr(const std::string&,bool);
    SmartPtr<RawImage> GetRawImageFromStr(const std::string&);
    StandardCursor ReadStandardCursor();
    void AddChildToConfigMap(const GUIString&,ComponentConfig*);
    GUIString GetStringResource(const std::string&); Image* GetImageResource(const std::string&); Font* GetFontResource(const std::string&);
    SmartPtr<Sound> GetSoundResource(const std::string&); DWORD GetColorResource(const std::string&);
    void GetBackgroundResource(const std::string&,Background&);
    Background ReadBackground(); int ReadInt(); Component* ReadComponent(); bool EndOfString(); void SkipComma();
    static std::string GetResourceFile(const std::string&);
    ComponentConfig() { this->mHasSetWidth=false; this->mHasSetHeight=false; this->mHaveInitedComponents=false; } // world: ComponentConfigLifetime.cpp's
    virtual ~ComponentConfig() {}
    virtual bool HandleInstruction(ConfigParser*, const std::string&);
    virtual bool HandleValueInstruction(ConfigParser*, const std::string&, StringParser&);
    virtual bool PreParse(ConfigParser*);
    virtual bool PostParse(ConfigParser*);
    virtual bool Virtual24(const std::string&);
    virtual bool GetPredefinedComponent(const std::string&, ComponentConfig*&, Component*&);
    virtual bool GetNamedValue(const std::string&, int&);
    virtual bool CallNamedFunction(const std::string&, const std::vector<int>&, int&);
    virtual void SetComponent(Component*);
    GUIString ReadString(bool); int ReadColor(); Component* Fn386860(const std::string& inName); bool ReadBool();
    SmartPtr<Component> mComponent;
    ConfigParser* mConfigParser;
    StringParser* mStringParser;
    GUIString mConfigError;
    bool mHasSetWidth,mHasSetHeight,mHaveInitedComponents;
    std::map<std::string,SmartPtr<ComponentConfig>,StringLessNoCase> mConfigMap;
    SmartPtr<ResourceConfig> mCurResourceConfig;
};
SmartPtr<ResourceConfigTable> ComponentConfig::mResourceConfigTable;
std::string ComponentConfig::mResourceFolder;
static void ClearStatics() { ResourceConfigTable::mControlIdMap.clear(); ResourceConfigTable::mConfigAllocatorMap.clear(); }
#define CC_ADD_ALWAYS(n, c) this->mConfigMap[n] = c
template<class C> ComponentConfig* NewConfigOf() { return new C; }
#include "won_cfgkind_orig.h"
#define CC_KIND_FACTORY(C, K) NewConfigOf<C>
// a config's class, for the probes: its C++ class's name
static std::string ClassTag(ComponentConfig* k)
{
    if (k == NULL) return "null";
    std::string n = typeid(*k).name();
    size_t c = n.rfind("::");
    return c == std::string::npos ? n : n.substr(c + 2);
}
#include "won_compcfg_run.h"
#include "won_compcfg_orig.h"
// world: ComponentConfigLoad.cpp's InitComponents
void ComponentConfig::InitComponents()
{
    if(this->mHaveInitedComponents) return;
    this->mHaveInitedComponents=true;
    ComponentConfigInit init(this);
    this->mComponent->InitComponent(init);
    std::map<std::string,SmartPtr<ComponentConfig>,StringLessNoCase>::iterator it=this->mConfigMap.begin();
    while(it!=this->mConfigMap.end()) {
        it->second->InitComponents();
        ++it;
    }
}
}

namespace Port {
#include "won_compcfg_world.h"
class ComponentConfig;
class ResourceConfigTable;
// the game's classes (include/ee/wonapi/WONLobbyResource.h, WONLobbyGUI.h)
class ResourceConfig : public ConfigObject {
protected:
    virtual ~ResourceConfig();
public:
    ResourceConfig();
    virtual bool HandleValueInstruction(ConfigParser*,const std::string&,StringParser&);
    virtual bool PreParse(ConfigParser*);
    GUIString SafeGetString(const std::string&);
    Image* SafeGetImage(const std::string&);
    Sound* SafeGetSound(const std::string&);
    void AddSound(const std::string&,Sound*);
    EeWonResCfg* mRes;
};
class ResourceConfigTable : public RefCount {
public:
    typedef void (*GlueResourceFunc)(ResourceConfigTable*);
    typedef ComponentConfig* (*ComponentConfigFactory)();
    typedef Component* (*ComponentFactory)();
    ResourceConfigTable();
    ResourceConfig* AddConfig(const std::string&,bool* = NULL);
    ResourceConfig* SafeGetConfig(const std::string&);
    void RegisterGlueResourceFunc(GlueResourceFunc);
    void SafeGlueResources();
    void RegisterControlId(const std::string&,int);
    void AddAllocator(const std::string&,ComponentConfigFactory,ComponentFactory);
    EeWonResTable* mTable;
protected:
    virtual ~ResourceConfigTable();
};
class ComponentConfig : public ConfigObject {
public:
    void InitComponents();
    Direction ReadDirection(); HorzAlign ReadHorzAlign(); VertAlign ReadVertAlign();
    static void SetResourceFolder(const std::string&);
    SmartPtr<Sound> ReadSound();
    void EnsureComma(); int ReadChar();
    GUIString ReadFlag(bool* = NULL); GUIString ReadRawString(bool);
    SmartPtr<Font> ReadFont(); SmartPtr<Image> ReadImage(bool); SelectionColor ReadSelectionColor();
    bool HandleMapComponent(); bool HandleModify();
    void SetDesiredSizeIfNecessary();
    SmartPtr<Cursor> ReadCursor();
    static SmartPtr<ResourceConfigTable> mResourceConfigTable;
    SmartPtr<RawImage> ReadRawImage(); SmartPtr<RawImage> GetRawImageFromStr(const std::string&);
    StandardCursor ReadStandardCursor();
    void AddChildToConfigMap(const GUIString&,ComponentConfig*,bool inAlways=false);
    Background ReadBackground(); int ReadInt(); Component* ReadComponent(); bool EndOfString();
    static std::string GetResourceFile(const std::string&);
    explicit ComponentConfig(EeWonCfgKind inKind = EeWonCfgKind::Component);
    virtual ~ComponentConfig();
    virtual bool HandleInstruction(ConfigParser*, const std::string&);
    virtual bool HandleValueInstruction(ConfigParser*, const std::string&, StringParser&);
    virtual bool PreParse(ConfigParser*);
    virtual bool PostParse(ConfigParser*);
    virtual bool Virtual24(const std::string&);
    virtual bool GetPredefinedComponent(const std::string&, ComponentConfig*&, Component*&);
    virtual bool GetNamedValue(const std::string&, int&);
    virtual bool CallNamedFunction(const std::string&, const std::vector<int>&, int&);
    virtual void SetComponent(Component*);
    GUIString ReadString(bool); int ReadColor(); Component* Fn386860(const std::string& inName); bool ReadBool();
    SmartPtr<Component> mComponent;
    GUIString mConfigError;
    bool mHasSetWidth,mHasSetHeight,mHaveInitedComponents;
    EeWonCfgKind mKind;
    EeWonCompCfg* mCfg;
};
// the game's (ConfigTableHost.cpp: a ContainerConfig for a container kind, whose own members these steps never read)
ComponentConfig* CfgNewOfKind(EeWonCfgKind k) { return new ComponentConfig(k); }
template<EeWonCfgKind K> ComponentConfig* KindConfig() { return CfgNewOfKind(K); }
#define CC_KIND_FACTORY(C, K) KindConfig<EeWonCfgKind::K>
// a config's class, for the probes: the original class its kind is (cfgkind.rs's order)
static std::string ClassTag(ComponentConfig* k)
{
    static const char* const kClass[] = {"ComponentConfig", "ContainerConfig", "DialogConfig", "ButtonConfig", "ImageButtonConfig",
        "LabelConfig", "InputBoxConfig", "ScrollAreaConfig", "ListAreaConfig", "MultiListAreaConfig", "TreeAreaConfig", "TextAreaConfig",
        "MultiListHeaderConfig", "ScrollerConfig", "ScrollbarScrollerConfig", "ScrollbarConfig", "TabCtrlConfig", "ComboBoxConfig",
        "SplitterConfig", "BrowserComponentConfig", "RectangleComponentConfig", "RectangleComponent3DConfig", "ImageComponentConfig",
        "ScreenContainerConfig", "MSButtonConfig", "MSArrowButtonConfig", "MSTabButtonConfig", "MS3DFrameConfig", "MSComboBoxConfig",
        "MSScrollbarConfig", "MSEtchedFrameGroupConfig", "MSTabCtrlConfig", "MSScrollContainerConfig", "MSHeaderControlConfig",
        "MSListCtrlConfig", "WONSimpleButtonConfig", "WONButtonConfig", "WONScrollbarConfig", "WONScrollContainerConfig",
        "WONHeaderControlConfig", "WONListCtrlConfig", "WONComboBoxConfig", "WONTabCtrlConfig", "WONFrameComponentConfig",
        "WONSkinConfig"};
    if (k == NULL) return "null";
    return (size_t)k->mKind < sizeof kClass / sizeof *kClass ? kClass[(size_t)k->mKind] : "kind?";
}
SmartPtr<ResourceConfigTable> ComponentConfig::mResourceConfigTable;
static void ClearStatics()
{
    ee_won_cc_call(NULL, NULL, NULL, EeWonCcCall::ClearAllocators, 0, 0, 0, 0);
    ee_won_cc_call(NULL, NULL, NULL, EeWonCcCall::ClearControlIds, 0, 0, 0, 0);
}
#define CC_ADD_ALWAYS(n, c) this->AddChildToConfigMap(n, c, true)
#include "won_compcfg_run.h"
#include "ee/wonapi/WONCompCfg.inl"
}

// ---- the generator (a fixed LCG)
static unsigned sSeed = 20261119;
static unsigned Rnd(unsigned n) { sSeed = sSeed * 1103515245u + 12345u; return n ? (sSeed >> 8) % n : 0; }
template<size_t N> static std::string Pick(const char* const (&a)[N]) { return a[Rnd(N)]; }
static const char* const kNames[] = {"child1", "child2", "img1", "sub", "Child1", "", "FAIL", "pre", "x"};
static const char* const kTypes[] = {"Comp", "Font", "Image", "Test", "test", "RESOURCE", "MODIFY", "BASEGROUP", "Unknown", "Resource",
#define CC_TYPE(n, C, K) #n,
    CC_KINDS(CC_TYPE) CC_KINDS(CC_TYPE)
#undef CC_TYPE
    };
static const char* const kGroups[] = {"Global", "global", "Other", "THROW", "", "Global|Base"};
static const char* const kKeys[] = {"INT", "BOOL", "CHAR", "FLAG", "FLAGN", "RAW", "STR", "WORD", "DIR", "HALIGN", "VALIGN", "COLOR",
    "SOUND", "BG", "FONT", "IMAGE", "RAWIMAGE", "RAWSTR", "STDCURSOR", "CURSOR", "SEL", "COMP", "EOS", "COMMA", "CHILD", "SIZE",
    "ADDCHILD", "ADDALWAYS", "FILE", "FOLDER", "INIT", "SETRESOURCE", "MODIFY", "MAPCOMPONENT", "BASEKEY", "NOPE"};
static const char* const kResKeys[] = {"STRING", "CODESTRING", "IMAGE", "CODEIMAGE", "SOUND", "CODESOUND", "FONT", "COLOR",
    "BACKGROUND", "CONTROLID", "BASEKEY", "OTHER", "string"};
static const char* const kAtoms[] = {"12", "-3", "0x1f", "(1+2)*3", "7/0", "(4", "@ID_FOO", "@ID_Bar", "@ID_NONE", "@ID_",
    "@intres", "Parent.Width", "this.LEFT", "child1.SEVEN", "child1.BOOM", "SEVEN", "BOOM", "WIDTH", "HEIGHT", "TOP",
    "BOTTOM", "RIGHT", "MAX(1,2,3)", "MIN(4,-5)", "TWICE(4)", "TWICE()",  "MAX(1,)", "child2.MAX(9)", "nope.LEFT",
    "@s1", "@S2", "@nostr", "\"quoted text\"", "plain words", "true", "no", "maybe", "ff00aa", "FF00AA", "12g456", "(STD,Hilight)",
    "(STD, LinkDown)", "(STD,Nope)", "(DEC,1,2,300)", "(DEC 1)", "(XYZ)", "@c1", "@nocolor", "Arial, Bold, 12", "Arial, Heavy, 12",
    "Arial, Plain, 0", "Arial Bold 12", "(CFG,Font1)", "(cfg,font1)", "(XX,a)", "@f1", "@nofont", "a.png",
    "bad.png", "./b.png", "c:x.png", "@CFG_img1", "@CFG_nope", "@CFG_child1", "@NULL", "@null", "@i1", "@noimg", "@X_y",
    "s.wav", "bad.wav", "(s.wav, Preload Music)", "(s.wav, Muted Loud)", "(s.wav Preload)", "@snd1", "@nosnd",
    "COLOR, ff0000", "IMAGE, a.png", "STRETCHIMAGE, b.png", "WATERMARK, c.png", "NONE", "IMAGE, bad.png", "FANCY", "@bg1", "@nobg",
    "Predefined, Hand", "Predefined, nope", "Custom, Hand", "Hand", "NorthSouthEastWest", "Left", "right", "UP", "Down", "Center",
    "Bottom", "Middle", "ff0000, USEFOREGROUND, 00ff00", "ff0000, INVERTORIGINAL", "ff0000, USEORIGINAL", "ff0000, OTHER",
    "child1", "child1.sub", "_PRE", "_HALF", "_NONE", "Parent", "this", "Font1", "sub.x", "root", "child1, alias",
    "child1, child2.alias", "child1, child2.", "child1", "child1, INT, 5", "child1, NOPE", "child1, THROW", "Global", "Other",
    "x", "+flag", "-flag", "  @s1", "", ",", "a,b", "\\x", "\xc5\xbftd", "\xc3\xa9", "res\\", "dir/", "@"};
// the values a key's reads take, most of the time (else any atoms)
static const std::map<std::string, std::vector<std::string> >& Pools()
{
    static std::map<std::string, std::vector<std::string> > m = {
        {"INT", {"child1.LEFTPAD", "child2.RIGHTPAD", "child1.TOPPAD", "child2.BOTTOMPAD", "child1.HORZPAD", "child2.VERTPAD",
                 "child1.leftpad", "x.HORZPAD", "12", "(1+2)*3", "7/0", "(4", "@ID_FOO", "@ID_bar", "@ID_NONE", "@intres", "Parent.Width", "this.LEFT", "child1.SEVEN",
                 "child1.BOOM", "SEVEN", "BOOM", "WIDTH - 3", "HEIGHT*2", "TOP", "BOTTOM", "RIGHT", "MAX(1,2,3)", "MIN(4,-5)",
                 "TWICE(4)", "TWICE()", "MAX(1,)", "child2.MAX(9)", "nope.LEFT", "_PRE.WIDTH", "x", "(SEVEN+1)/2"}},
        {"COLOR", {"ff00aa", "12g456", "(STD,Hilight)", "(std, linkdown)", "(STD,Nope)", "(DEC,1,2,300)", "(DEC 1)", "(XYZ)", "@c1",
                   "@nocolor", "(DEC,1,2,3"}},
        {"FONT", {"Arial, Bold, 12", "Arial, Heavy, 12", "Arial, Plain, 0", "Arial, Bold Italic Underline, 7", "(CFG,Font1)",
                  "(cfg,font1)", "(CFG,Font1", "(XX,a)", "@f1", "@nofont", "Arial Bold 12"}},
        {"IMAGE", {"a.png", "bad.png", "./b.png", "c:x.png", "@CFG_img1", "@CFG_nope", "@CFG_child1", "@NULL", "@null", "@i1",
                   "@noimg", "@X_y", "@CFG_Font1"}},
        {"SOUND", {"s.wav", "bad.wav", "(s.wav, Preload Music)", "(s.wav, Muted Loud)", "(s.wav Preload)", "@snd1", "@nosnd"}},
        {"BG", {"COLOR, ff0000", "IMAGE, a.png", "STRETCHIMAGE, b.png", "WATERMARK, c.png", "NONE", "IMAGE, bad.png", "FANCY",
                "@bg1", "@nobg", " @bg1"}},
        {"CURSOR", {"Predefined, Hand", "Predefined, nope", "Custom, Hand", "predefined, ibeam", "Predefined Hand"}},
        {"STDCURSOR", {"Hand", "NorthSouthEastWest", "arrow", "EastWest", "nope", "@s1"}},
        {"DIR", {"Left", "right", "UP", "Down", "Center", "@s1"}},
        {"HALIGN", {"Left", "Center", "Right", "Middle"}},
        {"VALIGN", {"Top", "center", "Bottom", "Middle"}},
        {"SEL", {"ff0000, USEFOREGROUND, 00ff00", "ff0000, INVERTORIGINAL", "ff0000, USEORIGINAL", "ff0000, OTHER", "ff0000",
                 "@c1, useforeground, @c1"}},
        {"COMP", {"child1", "child1.sub", "_PRE", "_HALF", "_NONE", "Parent", "this", "Font1", "sub.x", "root", "parent.child1",
                  // the kinds' predefined children (and names no kind has, and the case the compares keep)
                  "child1._LISTAREA", "child2._INPUTBOX", "child1._THUMB", "child2._UPARROW", "child1._DOWNARROW", "child2._SCROLLRECT",
                  "child1._CONTAINERLAYOUT", "child2._HTMLVIEW", "child1._HORZSCROLLBAR", "child2._VERTSCROLLBAR", "child1._HEADER",
                  "child2._MULTILISTAREA", "child1._LABEL", "child1._header", "sub._THUMB", "child2._VERTSCROLLBAR._THUMB",
                  "child1._HEADER._LABEL", "child2._MULTILISTAREA._HEADER", "img1._LISTAREA", "x._HTMLVIEW"}},
        {"MAPCOMPONENT", {"child1, alias", "child1, child2.alias", "child1, child2.", "child1", "Font1, x", "this, self"}},
        {"MODIFY", {"child1, INT, 5", "child1, NOPE", "child1, THROW", "Font1, COLOR, ff", "this, STR, abc", "nope, INT"}},
        {"SETRESOURCE", {"Global", "global", "Other", "nope"}},
        {"STR", {"@s1", "@S2", "@nostr", "\"quoted text\"", "plain words", "  @s1"}},
        {"FLAG", {"+flag", "-flag", "flag", "@s1", "+@s1"}},
        {"BOOL", {"true", "no", "maybe", "1"}},
    };
    return m;
}
static std::string Value(const std::string& inKey = "")
{
    std::map<std::string, std::vector<std::string> >::const_iterator p = Pools().find(inKey);
    std::string v = p != Pools().end() && Rnd(10) < 7 ? p->second[Rnd((unsigned)p->second.size())] : Pick(kAtoms);
    unsigned n = Rnd(4);
    for (unsigned i = 0; i < n; i++) v += (Rnd(2) ? ", " : " ") + Pick(kAtoms);
    return v;
}
// a resource line's value: its name, then mostly what its kind reads
static std::string ResValue(const std::string& inKey)
{
    static const char* const kRes[] = {"s1", "S2", "c1", "f1", "i1", "snd1", "bg1", "intres", "", "x"};
    std::string k = inKey == "STRING" || inKey == "CODESTRING" || inKey == "string" ? "STR" : inKey == "CODEIMAGE" ? "IMAGE"
        : inKey == "CODESOUND" ? "SOUND" : inKey == "BACKGROUND" ? "BG" : inKey;
    return Pick(kRes) + " " + Value(k);
}
static Port::Script MakeScript()
{
    Port::Script s;
    for (unsigned i = Rnd(3); i > 0; i--) s.groups.push_back(Pick(kGroups));
    for (unsigned i = Rnd(3); i > 0; i--) s.names.push_back(Pick(kNames));
    for (unsigned i = Rnd(3); i > 0; i--) {
        std::vector<std::pair<std::string, std::string> > lines;
        for (unsigned j = Rnd(8); j > 0; j--) {
            if (Rnd(3) == 0) { std::string k = Pick(kKeys); lines.push_back(std::make_pair(k, Value(k))); }
            else { std::string k = Pick(kResKeys); lines.push_back(std::make_pair(k, ResValue(k))); }
        }
        s.lines.push_back(lines);
    }
    s.include = Rnd(2) != 0;
    return s;
}
static Orig::Step OS(const Port::Step& p) { Orig::Step o; o.kind = p.kind; o.target = p.target; o.a = p.a; o.b = p.b;
    o.script.groups = p.script.groups; o.script.names = p.script.names; o.script.lines = p.script.lines; o.script.include = p.script.include; return o; }
static std::vector<Port::Step> MakeRun()
{
    std::vector<Port::Step> v;
    for (int i = 0; i < 60; i++) {
        Port::Step s;
        unsigned r = Rnd(100);
        s.kind = r < 45 ? 0 : r < 75 ? 1 : r < 80 ? 2 : r < 83 ? 3 : r < 86 ? 4 : r < 89 ? 5 : r < 93 ? 6 : r < 95 ? 7 : r < 98 ? 8 : 9;
        if (Rnd(8) == 0) s.kind = 10; // (a probe of the kinds' predefined children and paddings)
        s.target = (int)Rnd(1000);
        Port::Script sc = MakeScript();
        s.script.groups = sc.groups; s.script.names = sc.names; s.script.lines = sc.lines; s.script.include = sc.include;
        if (s.kind == 0) { s.a = Pick(kKeys); s.b = Value(s.a); }
        else if (s.kind == 1) s.a = Pick(kTypes);
        else if (s.kind == 2) s.a = Rnd(2) ? "" : "Base";
        else if (s.kind == 6 || s.kind == 7 || s.kind == 9) { s.a = Pick(kGroups); s.b = Rnd(2) ? "s1" : Rnd(2) ? "i1" : "snd1"; }
        else if (s.kind == 8) s.a = Pick(kNames);
        v.push_back(s);
    }
    return v;
}

int main(int argc, char** argv)
{
    int runs = argc > 1 ? atoi(argv[1]) : 3000;
    int bad = 0;
    size_t chars = 0;
    for (int r = 0; r < runs; r++) {
        std::vector<Port::Step> p = MakeRun();
        std::vector<Orig::Step> o;
        for (size_t i = 0; i < p.size(); i++) o.push_back(OS(p[i]));
        const char* only = getenv("EE_CC_ONLY");
        std::string a = only && *only == 'p' ? std::string() : Orig::Run(o);
        std::string b = only && *only == 'o' ? std::string() : Port::Run(p);
        chars += a.size();
        if (a != b) {
            if (bad++ < 3) {
                size_t i = 0;
                while (i < a.size() && i < b.size() && a[i] == b[i]) i++;
                size_t from = a.rfind("step ", i);
                if (from == std::string::npos) from = 0;
                printf("run %d differs at %u:\n--- orig\n%s\n--- port\n%s\n", r, (unsigned)i, a.substr(from, i - from + 400).c_str(),
                       b.substr(from, i - from + 400).c_str());
            }
        }
    }
    printf("won_compcfg_test: %d runs, %u trace bytes each side, %d mismatching\n", runs, (unsigned)chars, bad);
    return bad;
}
