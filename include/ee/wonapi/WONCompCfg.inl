#include "rust/won_gui/cfgkind.h"
#include "rust/won_gui/compcfg.h"
// The C++ end of ee_won_gui's compcfg.rs (R6 WONLobby round 25): ComponentConfig's readers, look-ups, config map and
// instructions, ResourceConfig and ResourceConfigTable are Rust objects; these are the classes' forwards and the host
// of what the Rust asks of the C++ objects (the hooks, the components, the decoders, references). Included inside a
// namespace by ComponentConfigHost.cpp (WONAPI) and by tests/won/won_compcfg_test.cpp (over its stand-ins), so both
// run these very statements. Every config pointer that crosses is its ConfigObject*.
struct CcFrame { std::exception_ptr mErr; std::string mMsg; };
static std::string CcStr(intptr_t p, intptr_t n) { return n > 0 ? std::string((const char*)p, (size_t)n) : std::string(); }
static ComponentConfig* CcCfg(void* c) { return static_cast<ComponentConfig*>((ConfigObject*)c); }
template<class T> static SmartPtr<T> CcAdopt(T* p) { SmartPtr<T> s = p; if (p) p->Release(); return s; }
template<class T> static intptr_t CcGive(const SmartPtr<T>& s) { T* p = s.get(); if (p) p->AddRef(); return (intptr_t)p; }
static void CcAppend(void* v, const std::string& s) { ee_won_cc_append(v, (const uint8_t*)s.data(), s.size()); }
#define CC_TRY(...) try { __VA_ARGS__ } catch (...) { static_cast<CcFrame*>(inFrame)->mErr = std::current_exception(); *ok = false; return 0; }
static intptr_t CcHost(void* inFrame, EeWonCcOp op, void* c, intptr_t a, intptr_t b, intptr_t d, intptr_t e, bool* ok)
{
    typedef EeWonCcOp O;
    ConfigObject* co = (ConfigObject*)c;
    switch (op) {
    case O::Ref:
        switch ((EeWonCcKind)a) {
        case EeWonCcKind::Config: case EeWonCcKind::Res: if (b > 0) co->AddRef(); else co->Release(); break;
        case EeWonCcKind::Image: if (b > 0) ((Image*)c)->AddRef(); else ((Image*)c)->Release(); break;
        case EeWonCcKind::Font: if (b > 0) ((Font*)c)->AddRef(); else ((Font*)c)->Release(); break;
        case EeWonCcKind::Sound: if (b > 0) ((Sound*)c)->AddRef(); else ((Sound*)c)->Release(); break;
        default: if (b > 0) ((Component*)c)->AddRef(); else ((Component*)c)->Release(); break;
        }
        return 0;
    case O::SetMessage: static_cast<CcFrame*>(inFrame)->mMsg = CcStr(a, b); return 0;
    case O::CfgObj: { ComponentConfig* k = dynamic_cast<ComponentConfig*>(co); return k ? (intptr_t)k->mCfg : 0; }
    case O::ResObj: return co ? (intptr_t)static_cast<ResourceConfig*>(co)->mRes : 0;
    case O::CurTable: return ComponentConfig::mResourceConfigTable.get() ? (intptr_t)ComponentConfig::mResourceConfigTable->mTable : 0;
    case O::Parent: return (intptr_t)co->mParent;
    case O::SetParent: co->mParent = (ConfigObject*)a; return 0;
    case O::Name: CcAppend((void*)a, co->mName); return 0;
    case O::BaseName: CcAppend((void*)a, co->mBaseName); return 0;
    case O::Flag: return a == 0 ? CcCfg(c)->mHasSetWidth : a == 1 ? CcCfg(c)->mHasSetHeight : CcCfg(c)->mHaveInitedComponents;
    case O::SetFlag: (a == 0 ? CcCfg(c)->mHasSetWidth : a == 1 ? CcCfg(c)->mHasSetHeight : CcCfg(c)->mHaveInitedComponents) = b != 0; return 0;
    case O::CompEdge: {
        Component* m = CcCfg(c)->mComponent.get();
        switch (a) { case 0: return m->Left(); case 1: return m->Right(); case 2: return m->Top(); case 3: return m->Bottom(); case 4: return m->Width(); default: return m->Height(); }
    }
    case O::CompDesired: CcCfg(c)->mComponent->GetDesiredSize(((int*)a)[0], ((int*)a)[1]); return 0;
    case O::CompSetSize: CcCfg(c)->mComponent->SetSize((int)a, (int)b); return 0;
    case O::CompWantGrab: CcCfg(c)->mComponent->SetWantGrabBG(true); return 0;
    case O::CompInit: CC_TRY(ComponentConfigInit anInit(CcCfg(c)); CcCfg(c)->mComponent->InitComponent(anInit); return 0;)
    case O::CompOf: return (intptr_t)CcCfg(c)->mComponent.get();
    case O::VNamed: CC_TRY(return CcCfg(c)->GetNamedValue(CcStr(a, b), *(int*)d);)
    case O::VCall: {
        const EeWonCcCallArgs* x = (const EeWonCcCallArgs*)a;
        CC_TRY(std::vector<int> v(x->params, x->params + x->count); CcCfg(c)->CallNamedFunction(CcStr((intptr_t)x->key, x->key_len), v, *x->out); return 0;)
    }
    case O::VPredefined: CC_TRY(
        ComponentConfig* aConfig = NULL; Component* aComp = NULL;
        bool r = CcCfg(c)->GetPredefinedComponent(CcStr(a, b), aConfig, aComp);
        ((void**)d)[0] = aConfig ? static_cast<ConfigObject*>(aConfig) : NULL; ((void**)d)[1] = aComp;
        return r;)
    case O::VSetComponent: CC_TRY(CcCfg(c)->SetComponent((Component*)a); return 0;)
    case O::V24: CC_TRY(return CcCfg(c)->Virtual24(CcStr(a, b));)
    case O::CoHandleInstruction: CC_TRY(return co->ConfigObject::HandleInstruction((ConfigParser*)a, CcStr(b, d));)
    case O::CoHandleValue: CC_TRY(return co->ConfigObject::HandleValueInstruction((ConfigParser*)a, CcStr(b, d), *(StringParser*)e);)
    case O::ReadGroupStart: CC_TRY(
        std::string n, i; bool r = co->ReadGroupStart(*(ConfigParser*)a, n, i);
        CcAppend((void*)b, n); CcAppend((void*)d, i); return r;)
    case O::ParseGroup: CC_TRY(return co->ParseGroup(*(ConfigParser*)a, (b & 1) != 0, (b & 2) != 0, (b & 4) != 0, (b & 8) != 0);)
    case O::ParseChild: CC_TRY(return co->ParseChild(*(ConfigParser*)a, (ConfigObject*)b, (d & 1) != 0, (d & 2) != 0);)
    case O::IncludeGroup: CC_TRY(return co->IncludeGroup(*(ConfigParser*)a, CcStr(b, d));)
    case O::AddWarning: CC_TRY(((ConfigParser*)c)->AddWarning(CcStr(a, b)); return 0;)
    case O::AsFont: { FontConfig* f = dynamic_cast<FontConfig*>(co); return f ? (intptr_t)f->GetFont() : -1; }
    case O::AsImage: {
        ImageConfig* i = dynamic_cast<ImageConfig*>(co);
        if (i == NULL) return -1;
        SmartPtr<Image> anImage = a ? static_cast<Image*>(i->GetRawImage()) : i->GetImage();
        return CcGive(anImage);
    }
    case O::DecodeImage: CC_TRY(
        std::string f = CcStr(a, b);
        if (d) return CcGive(Window::GetDefaultWindow()->DecodeImageRaw(f.c_str()));
        return CcGive(WindowManager::GetDefaultWindowManager()->DecodeDelayImage(f.c_str()));)
    case O::FontGet: CC_TRY(return CcGive(Window::GetDefaultWindow()->GetFont(FontDescriptor(CcStr(a, b), (int)d, (int)e)));)
    case O::SoundDecode: CC_TRY(
        SoundDescriptor aDesc;
        if (e) aDesc.SetSoundFlags((int)d, true);
        aDesc.mPath = CcStr(a, b);
        return CcGive(WindowManager::GetDefaultWindowManager()->DecodeSound(aDesc));)
    case O::BgMake: {
        const EeWonCcBackground* x = (const EeWonCcBackground*)a;
        Background aBg;
        aBg.mImage = (Image*)x->image; aBg.mColor = x->color; aBg.mBorderColor = x->border_color;
        aBg.mUseOffsets = x->use_offsets; aBg.mStretchImage = x->stretch_image; aBg.mWantGrabBG = x->want_grab_bg;
        *(Background*)c = aBg;
        return 0;
    }
    case O::Alloc: CC_TRY(
        SmartPtr<ComponentConfig> aConfig = ((ResourceConfigTable::ComponentConfigFactory)a)();
        SmartPtr<Component> aComp = ((ResourceConfigTable::ComponentFactory)b)();
        ((void**)d)[0] = aConfig.get() ? (void*)static_cast<ConfigObject*>(aConfig.get()) : NULL;
        ((void**)d)[1] = aComp.get();
        CcGive(aConfig); CcGive(aComp); return 0;)
    case O::Glue: CC_TRY(((ResourceConfigTable::GlueResourceFunc)a)((ResourceConfigTable*)c); return 0;)
    case O::NewRes: { ResourceConfig* r = new ResourceConfig; r->mName = CcStr(a, b); r->AddRef(); return (intptr_t)static_cast<ConfigObject*>(r); }
    case O::SetText: {
        std::wstring w;
        for (intptr_t i = 0; i < b; i++) w += (wchar_t)((const uint32_t*)a)[i];
        *(GUIString*)c = GUIString(w);
        return 0;
    }
    default: return 0;
    }
}
#undef CC_TRY
// one call of the Rust object `o`: its message thrown as the original's ConfigObjectException, a host op's exception again
static void CcRun(void* o, EeWonCcCall inCall, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0, intptr_t d = 0)
{
    CcFrame aFrame;
    int r = ee_won_cc_call(o, CcHost, &aFrame, inCall, a, b, c, d);
    if (r == 1) throw ConfigObjectException(aFrame.mMsg);
    if (r == 2) std::rethrow_exception(aFrame.mErr);
}
#define CC(k, ...) CcRun(this->mCfg, EeWonCcCall::k, __VA_ARGS__)
#define CC_P(s) (intptr_t)(s).data(), (intptr_t)(s).size()
static EeWonCcText CcText(const GUIString& s) { EeWonCcText t = {s.mData->mText, s.mData->mLength}; return t; }

ComponentConfig::ComponentConfig(EeWonCfgKind inKind) : mHasSetWidth(false), mHasSetHeight(false), mHaveInitedComponents(false), mKind(inKind)
{ this->mCfg = ee_won_cc_new(static_cast<ConfigObject*>(this), CcHost); }
ComponentConfig::~ComponentConfig() { ee_won_cc_free(this->mCfg); }
bool ComponentConfig::EndOfString() { bool v = false; CC(EndOfString, (intptr_t)&v); return v; }
void ComponentConfig::EnsureComma() { CC(EnsureComma, 0); }
int ComponentConfig::ReadInt() { int32_t v = 0; CC(ReadInt, (intptr_t)&v); return v; }
bool ComponentConfig::ReadBool() { bool v = false; CC(ReadBool, (intptr_t)&v); return v; }
int ComponentConfig::ReadChar() { int32_t v = 0; CC(ReadChar, (intptr_t)&v); return v; }
GUIString ComponentConfig::ReadFlag(bool* outOn) { GUIString v; CC(ReadFlag, (intptr_t)&v, (intptr_t)outOn); return v; }
GUIString ComponentConfig::ReadRawString(bool inStop) { GUIString v; CC(ReadRawString, (intptr_t)&v, inStop); return v; }
GUIString ComponentConfig::ReadString(bool inStop) { GUIString v; CC(ReadString, (intptr_t)&v, inStop); return v; }
Direction ComponentConfig::ReadDirection() { int32_t v = 0; CC(ReadDirection, (intptr_t)&v); return (Direction)v; }
HorzAlign ComponentConfig::ReadHorzAlign() { int32_t v = 0; CC(ReadHorzAlign, (intptr_t)&v); return (HorzAlign)v; }
VertAlign ComponentConfig::ReadVertAlign() { int32_t v = 0; CC(ReadVertAlign, (intptr_t)&v); return (VertAlign)v; }
int ComponentConfig::ReadColor() { int32_t v = 0; CC(ReadColor, (intptr_t)&v); return v; }
SmartPtr<Sound> ComponentConfig::ReadSound() { Sound* p = NULL; CC(ReadSound, (intptr_t)&p); return CcAdopt(p); }
Background ComponentConfig::ReadBackground() { Background v; CC(ReadBackground, (intptr_t)&v); return v; }
SmartPtr<Font> ComponentConfig::ReadFont() { Font* p = NULL; CC(ReadFont, (intptr_t)&p); return CcAdopt(p); }
SmartPtr<Image> ComponentConfig::ReadImage(bool inRaw) { Image* p = NULL; CC(ReadImage, (intptr_t)&p, inRaw); return CcAdopt(p); }
SmartPtr<RawImage> ComponentConfig::GetRawImageFromStr(const std::string& s)
{ Image* p = NULL; CC(ImageFromStr, (intptr_t)&p, CC_P(s), 1); return (RawImage*)CcAdopt(p).get(); }
SmartPtr<RawImage> ComponentConfig::ReadRawImage() { return (RawImage*)this->ReadImage(true).get(); }
StandardCursor ComponentConfig::ReadStandardCursor() { int32_t v = 0; CC(ReadStandardCursor, (intptr_t)&v); return (StandardCursor)v; }
SmartPtr<Cursor> ComponentConfig::ReadCursor() { int32_t v = 0; CC(ReadCursor, (intptr_t)&v); return Cursor::GetStandardCursor((StandardCursor)v); }
SelectionColor ComponentConfig::ReadSelectionColor()
{
    EeWonCcSelColor x; CC(ReadSelectionColor, (intptr_t)&x);
    if (x.mode == 0) return SelectionColor(x.background, (DWORD)x.foreground);
    return SelectionColor(x.background, x.mode == 1 ? ForegroundSelectionMode_InvertOriginalColor : ForegroundSelectionMode_UseOriginalColor);
}
// the kind's own named values (the WON frame's paddings: the sum of the frame borders cfgkind.rs names), else the base's
bool ComponentConfig::GetNamedValue(const std::string& k, int& v)
{
    int32_t aSides[2];
    const int n = ee_won_cfg_kind_named(this->mKind, (const uint8_t*)k.data(), k.size(), aSides);
    if (n > 0) {
        int aSum = 0;
        for (int i = 0; i < n; i++) aSum += (int)WONShell::Op(this->mComponent.get(), EeWonShellOp::GetFrameBorder, aSides[i]);
        v = aSum;
        return true;
    }
    bool r = false; CC(GetNamedValue, CC_P(k), (intptr_t)&v, (intptr_t)&r); return r;
}
// a part of the component a predefined child names (the original config classes' typed members' reads)
static Component* CcPart(Component* c, EeWonCfgPart p)
{
    typedef EeWonCfgPart P;
    switch (p) {
    case P::ComboList: return static_cast<ComboBox*>(c)->mListArea.get();
    case P::ComboEdit: return static_cast<ComboBox*>(c)->mEdit.get();
    case P::Thumb: return static_cast<Scrollbar*>(c)->GetThumb();
    case P::UpArrow: return static_cast<Scrollbar*>(c)->GetUpArrow();
    case P::DownArrow: return static_cast<Scrollbar*>(c)->GetDownArrow();
    case P::ScrollRect: return static_cast<Scrollbar*>(c)->GetScrollRect();
    case P::TabLayout: return static_cast<TabCtrl*>(c)->mContainerLayout;
    case P::HtmlView: return static_cast<BrowserComponent*>(c)->GetHTMLView();
    case P::HorzBar: return static_cast<ScrollbarScroller*>(c)->GetScrollbar(false);
    case P::VertBar: return static_cast<ScrollbarScroller*>(c)->GetScrollbar(true);
    case P::Header: return (Component*)WONShell::Op(c, EeWonShellOp::GetHeader);
    case P::ScrollAreaOf: return static_cast<ScrollbarScroller*>(c)->GetScrollArea();
    default: return (Component*)WONShell::Op(c, EeWonShellOp::GetLabel); // EtchedLabel
    }
}
// the kind's predefined children (cfgkind.rs): a new config of the child's kind and the part
bool ComponentConfig::GetPredefinedComponent(const std::string& n, ComponentConfig*& config, Component*& component)
{
    EeWonCfgKind aKind; EeWonCfgPart aPart;
    if (!ee_won_cfg_kind_predefined(this->mKind, (const uint8_t*)n.data(), n.size(), &aKind, &aPart)) return false;
    config = CfgNewOfKind(aKind);
    component = CcPart(this->mComponent.get(), aPart);
    return true;
}
bool ComponentConfig::CallNamedFunction(const std::string& k, const std::vector<int>& p, int& v)
{
    bool r = false;
    EeWonCcCallArgs x = {(const uint8_t*)k.data(), k.size(), p.empty() ? NULL : &p[0], p.size(), &v};
    CC(CallNamedFunction, (intptr_t)&x, (intptr_t)&r);
    return r;
}
// the base group included; a WON skin's config then makes the game's skin (the Windows look) when there is none
bool ComponentConfig::PreParse(ConfigParser* p)
{
    bool r = false; CC(PreParse, (intptr_t)&r, (intptr_t)p);
    if (r && this->mKind == EeWonCfgKind::WonSkin) ee_won_skin_ensure(ee_won_guicpp_shell_call);
    return r;
}
bool ComponentConfig::PostParse(ConfigParser*) { this->SetDesiredSizeIfNecessary(); return true; }
void ComponentConfig::SetDesiredSizeIfNecessary() { CC(SetDesiredSize, 0); }
bool ComponentConfig::HandleInstruction(ConfigParser* p, const std::string& t) { bool r = false; CC(HandleInstruction, (intptr_t)&r, (intptr_t)p, CC_P(t)); return r; }
bool ComponentConfig::HandleValueInstruction(ConfigParser* p, const std::string& k, StringParser& v)
{ bool r = false; EeWonCcKey x = {(const uint8_t*)k.data(), k.size()}; CC(HandleValueInstruction, (intptr_t)&r, (intptr_t)p, (intptr_t)&x, (intptr_t)&v); return r; }
Component* ComponentConfig::Fn386860(const std::string& n) { void* p = NULL; CC(ChildComponent, (intptr_t)&p, CC_P(n)); return (Component*)p; }
Component* ComponentConfig::ReadComponent() { void* p = NULL; CC(ReadComponent, (intptr_t)&p); return (Component*)p; }
bool ComponentConfig::HandleMapComponent() { bool r = false; CC(HandleMapComponent, (intptr_t)&r); return r; }
void ComponentConfig::AddChildToConfigMap(const GUIString& n, ComponentConfig* c, bool inAlways)
{ EeWonCcText t = CcText(n); CC(AddChild, (intptr_t)&t, (intptr_t)static_cast<ConfigObject*>(c), inAlways); }
bool ComponentConfig::HandleModify() { bool r = false; CC(HandleModify, (intptr_t)&r); return r; }
void ComponentConfig::InitComponents() { CC(InitComponents, 0); }
static void CcStringSink(void* c, const uint8_t* b, uintptr_t n) { static_cast<std::string*>(c)->assign((const char*)b, n); }
void ComponentConfig::SetResourceFolder(const std::string& f) { CcRun(NULL, EeWonCcCall::SetResourceFolder, 0, CC_P(f)); }
std::string ComponentConfig::GetResourceFile(const std::string& f)
{ std::string r; CcRun(NULL, EeWonCcCall::GetResourceFile, (intptr_t)&r, CC_P(f), (intptr_t)&CcStringSink); return r; }
#undef CC

ResourceConfig::ResourceConfig() { this->mRes = ee_won_cc_res_new(static_cast<ConfigObject*>(this)); }
ResourceConfig::~ResourceConfig() { ee_won_cc_res_free(this->mRes, CcHost); }
bool ResourceConfig::HandleValueInstruction(ConfigParser* p, const std::string& k, StringParser& v)
{ bool r = false; EeWonCcKey x = {(const uint8_t*)k.data(), k.size()}; CcRun(this->mRes, EeWonCcCall::ResHandleValue, (intptr_t)&r, (intptr_t)p, (intptr_t)&x, (intptr_t)&v); return r; }
bool ResourceConfig::PreParse(ConfigParser*) { CcRun(this->mRes, EeWonCcCall::ResPreParse); return true; }
GUIString ResourceConfig::SafeGetString(const std::string& k) { GUIString v; CcRun(this->mRes, EeWonCcCall::ResSafeGetString, (intptr_t)&v, CC_P(k)); return v; }
Image* ResourceConfig::SafeGetImage(const std::string& k) { void* p = NULL; CcRun(this->mRes, EeWonCcCall::ResSafeGetPtr, (intptr_t)&p, CC_P(k), 0); return (Image*)p; }
Sound* ResourceConfig::SafeGetSound(const std::string& k) { void* p = NULL; CcRun(this->mRes, EeWonCcCall::ResSafeGetPtr, (intptr_t)&p, CC_P(k), 2); return (Sound*)p; }
void ResourceConfig::AddSound(const std::string& k, Sound* s) { CcRun(this->mRes, EeWonCcCall::ResAddSound, 0, CC_P(k), (intptr_t)s); }

ResourceConfigTable::ResourceConfigTable() { this->mTable = ee_won_cc_table_new(this); }
ResourceConfigTable::~ResourceConfigTable() { ee_won_cc_table_free(this->mTable, CcHost); }
ResourceConfig* ResourceConfigTable::AddConfig(const std::string& n, bool* outExists)
{ void* p = NULL; CcRun(this->mTable, EeWonCcCall::TableAddConfig, (intptr_t)&p, CC_P(n), (intptr_t)outExists); return static_cast<ResourceConfig*>((ConfigObject*)p); }
ResourceConfig* ResourceConfigTable::SafeGetConfig(const std::string& n)
{ void* p = NULL; CcRun(this->mTable, EeWonCcCall::TableSafeGetConfig, (intptr_t)&p, CC_P(n)); return static_cast<ResourceConfig*>((ConfigObject*)p); }
void ResourceConfigTable::RegisterGlueResourceFunc(GlueResourceFunc f) { CcRun(this->mTable, EeWonCcCall::TableRegisterGlue, (intptr_t)f); }
void ResourceConfigTable::SafeGlueResources() { CcRun(this->mTable, EeWonCcCall::TableGlue); }
void ResourceConfigTable::AddAllocator(const std::string& n, ComponentConfigFactory c, ComponentFactory f)
{ CcRun(NULL, EeWonCcCall::AddAllocator, (intptr_t)c, CC_P(n), (intptr_t)f); }
void ResourceConfigTable::RegisterControlId(const std::string& n, int v) { CcRun(NULL, EeWonCcCall::RegisterControlId, v, CC_P(n)); }
#undef CC_P
