#include "rust/won_gui/cfgkind.h"
#include "rust/won_gui/compcfg.h"
#include "rust/won_gui/shell.h"
#include "rust/won_gui/skinobj.h"
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
// an op's statement under a guard: its exception kept in the frame, the op answering 0 with *ok cleared
template<class F> static intptr_t CcGuard(void* inFrame, bool* ok, F f)
{
    try { return (intptr_t)f(); } catch (...) { static_cast<CcFrame*>(inFrame)->mErr = std::current_exception(); *ok = false; return 0; }
}
#define CC_G(...) CcGuard(inFrame, ok, [&] { return (intptr_t)(__VA_ARGS__); })
#define CC_GV(...) CcGuard(inFrame, ok, [&] { __VA_ARGS__; return (intptr_t)0; })
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
#define CC_ARGS ((const EeWonCcCallArgs*)a) // VCall's arguments
static intptr_t CcHost(void* inFrame, EeWonCcOp op, void* c, intptr_t a, intptr_t b, intptr_t d, intptr_t e, bool* ok)
{
    typedef EeWonCcOp O;
    ConfigObject* co = (ConfigObject*)c;
    switch (op) {
    case O::CfgAddRef: co->AddRef(); return 0;
    case O::CfgRelease: co->Release(); return 0;
    case O::ImageAddRef: ((Image*)c)->AddRef(); return 0;
    case O::ImageRelease: ((Image*)c)->Release(); return 0;
    case O::FontAddRef: ((Font*)c)->AddRef(); return 0;
    case O::FontRelease: ((Font*)c)->Release(); return 0;
    case O::SoundAddRef: ((Sound*)c)->AddRef(); return 0;
    case O::SoundRelease: ((Sound*)c)->Release(); return 0;
    case O::CompAddRef: ((Component*)c)->AddRef(); return 0;
    case O::CompRelease: ((Component*)c)->Release(); return 0;
    case O::SetMessage: static_cast<CcFrame*>(inFrame)->mMsg = CcStr(a, b); return 0;
    case O::CurTable: return ComponentConfig::mResourceConfigTable.get() ? (intptr_t)ComponentConfig::mResourceConfigTable->mTable : 0;
    case O::Parent: return (intptr_t)co->mParent;
    case O::SetParent: co->mParent = (ConfigObject*)a; return 0;
    case O::Name: CcAppend((void*)a, co->mName); return 0;
    case O::SetName: co->mName = CcStr(a, b); return 0;
    case O::BaseName: CcAppend((void*)a, co->mBaseName); return 0;
    case O::Kind: return (intptr_t)CcCfg(c)->mKind;
    case O::Flag: return a == 0 ? CcCfg(c)->mHasSetWidth : a == 1 ? CcCfg(c)->mHasSetHeight : CcCfg(c)->mHaveInitedComponents;
    case O::SetFlag: (a == 0 ? CcCfg(c)->mHasSetWidth : a == 1 ? CcCfg(c)->mHasSetHeight : CcCfg(c)->mHaveInitedComponents) = b != 0; return 0;
    case O::CompLeft: return CcCfg(c)->mComponent->Left();
    case O::CompRight: return CcCfg(c)->mComponent->Right();
    case O::CompTop: return CcCfg(c)->mComponent->Top();
    case O::CompBottom: return CcCfg(c)->mComponent->Bottom();
    case O::CompWidth: return CcCfg(c)->mComponent->Width();
    case O::CompHeight: return CcCfg(c)->mComponent->Height();
    case O::CompDesired: CcCfg(c)->mComponent->GetDesiredSize(((int*)a)[0], ((int*)a)[1]); return 0;
    case O::CompSetSize: CcCfg(c)->mComponent->SetSize((int)a, (int)b); return 0;
    case O::CompWantGrab: CcCfg(c)->mComponent->SetWantGrabBG(true); return 0;
    case O::CompInit: return CC_GV(ComponentConfigInit anInit(CcCfg(c)); CcCfg(c)->mComponent->InitComponent(anInit));
    case O::CompOf: return (intptr_t)CcCfg(c)->mComponent.get();
    case O::CompPart: return (intptr_t)CcPart((Component*)c, (EeWonCfgPart)a);
    case O::CompFrameBorder: return WONShell::Op((Component*)c, EeWonShellOp::GetFrameBorder, a);
    case O::NewOfKind: return (intptr_t)CfgNewOfKind((EeWonCfgKind)a);
    case O::AsObject: return (intptr_t)static_cast<ConfigObject*>((ComponentConfig*)c);
    case O::SkinEnsure: ee_won_skin_ensure(ee_won_guicpp_shell_call); return 0;
    case O::VNamed: return CC_G(CcCfg(c)->GetNamedValue(CcStr(a, b), *(int*)d));
    case O::VCall: return CC_G(CcCfg(c)->CallNamedFunction(CcStr((intptr_t)CC_ARGS->key, CC_ARGS->key_len), std::vector<int>(CC_ARGS->params, CC_ARGS->params + CC_ARGS->count), *CC_ARGS->out));
    case O::VPredefined: return CC_G(CcCfg(c)->GetPredefinedComponent(CcStr(a, b), ((ComponentConfig**)d)[0], ((Component**)d)[1]));
    case O::VSetComponent: return CC_GV(CcCfg(c)->SetComponent((Component*)a));
    case O::V24: return CC_G(CcCfg(c)->Virtual24(CcStr(a, b)));
    case O::CoHandleInstruction: return CC_G(co->ConfigObject::HandleInstruction((ConfigParser*)a, CcStr(b, d)));
    case O::CoHandleValue: return CC_G(co->ConfigObject::HandleValueInstruction((ConfigParser*)a, CcStr(b, d), *(StringParser*)e));
    case O::StrNew: return (intptr_t)new std::string;
    case O::StrTake: CcAppend((void*)a, *(std::string*)c); return 0;
    case O::StrFree: delete (std::string*)c; return 0;
    case O::ReadGroupStart: return CC_G(co->ReadGroupStart(*(ConfigParser*)a, *(std::string*)b, *(std::string*)d));
    case O::ParseGroup: return CC_G(co->ParseGroup(*(ConfigParser*)a, (b & 1) != 0, (b & 2) != 0, (b & 4) != 0, (b & 8) != 0));
    case O::ParseChild: return CC_G(co->ParseChild(*(ConfigParser*)a, (ConfigObject*)b, (d & 1) != 0, (d & 2) != 0));
    case O::IncludeGroup: return CC_G(co->IncludeGroup(*(ConfigParser*)a, CcStr(b, d)));
    case O::AddWarning: return CC_GV(((ConfigParser*)c)->AddWarning(CcStr(a, b)));
    case O::IsFontCfg: return dynamic_cast<FontConfig*>(co) != NULL;
    case O::FontOf: return (intptr_t)static_cast<FontConfig*>(CcCfg(c))->GetFont();
    case O::IsImageCfg: return dynamic_cast<ImageConfig*>(co) != NULL;
    case O::ImageOf: return CcGive(SmartPtr<Image>(static_cast<ImageConfig*>(CcCfg(c))->GetImage()));
    case O::RawImageOf: return CcGive(SmartPtr<Image>(static_cast<Image*>(static_cast<ImageConfig*>(CcCfg(c))->GetRawImage())));
    case O::DecodeRaw: return CC_G(CcGive(Window::GetDefaultWindow()->DecodeImageRaw(CcStr(a, b).c_str())));
    case O::DecodeDelay: return CC_G(CcGive(WindowManager::GetDefaultWindowManager()->DecodeDelayImage(CcStr(a, b).c_str())));
    case O::FontGet: return CC_G(CcGive(Window::GetDefaultWindow()->GetFont(FontDescriptor(CcStr(a, b), (int)d, (int)e))));
    case O::SoundDescNew: return (intptr_t)new SoundDescriptor;
    case O::SoundDescFlags: ((SoundDescriptor*)c)->SetSoundFlags((int)a, true); return 0;
    case O::SoundDescPath: ((SoundDescriptor*)c)->mPath = CcStr(a, b); return 0;
    case O::SoundDecode: return CC_G(CcGive(WindowManager::GetDefaultWindowManager()->DecodeSound(*(SoundDescriptor*)c)));
    case O::SoundDescFree: delete (SoundDescriptor*)c; return 0;
    case O::BgMake: {
        const EeWonCcBackground* x = (const EeWonCcBackground*)a;
        Background aBg;
        aBg.mImage = (Image*)x->image; aBg.mColor = x->color; aBg.mBorderColor = x->border_color;
        aBg.mUseOffsets = x->use_offsets; aBg.mStretchImage = x->stretch_image; aBg.mWantGrabBG = x->want_grab_bg;
        *(Background*)c = aBg;
        return 0;
    }
    case O::AllocConfig: return CC_G(CcGive(SmartPtr<ConfigObject>(((ResourceConfigTable::ComponentConfigFactory)a)())));
    case O::AllocComp: return CC_G(CcGive(SmartPtr<Component>(((ResourceConfigTable::ComponentFactory)a)())));
    case O::Glue: return CC_GV(((ResourceConfigTable::GlueResourceFunc)a)((ResourceConfigTable*)c));
    case O::NewRes: return CcGive(SmartPtr<ConfigObject>(new ResourceConfig));
    case O::SetText: *(GUIString*)c = GUIString(std::wstring((const wchar_t*)a, (size_t)b)); return 0;
    default: return 0;
    }
}
#undef CC_ARGS
// one call of the Rust object `o`: its message thrown as the original's ConfigObjectException, a host op's exception again
static void CcRun(void* o, EeWonCcCall inCall, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0, intptr_t d = 0)
{
    CcFrame aFrame;
    int r = ee_won_cc_call(o, CcHost, &aFrame, inCall, a, b, c, d);
    if (r == 1) throw ConfigObjectException(aFrame.mMsg);
    if (r == 2) std::rethrow_exception(aFrame.mErr);
}
// a call whose answer the Rust writes at its first argument (a T)
template<class T> static T CcGet(void* o, EeWonCcCall inCall, intptr_t b = 0, intptr_t c = 0, intptr_t d = 0)
{
    T v = T();
    CcRun(o, inCall, (intptr_t)&v, b, c, d);
    return v;
}
typedef EeWonCcCall CcK;
#define CC(k, ...) CcRun(this->mCfg, CcK::k, __VA_ARGS__)
#define CC_GET(T, ...) CcGet<T>(this->mCfg, __VA_ARGS__)
#define CC_P(s) (intptr_t)(s).data(), (intptr_t)(s).size()
static EeWonCcText CcText(const GUIString& s) { EeWonCcText t = {s.mData->mText, s.mData->mLength}; return t; }

ComponentConfig::ComponentConfig(EeWonCfgKind inKind) : mHasSetWidth(false), mHasSetHeight(false), mHaveInitedComponents(false), mKind(inKind)
{ this->mCfg = ee_won_cc_new(static_cast<ConfigObject*>(this), CcHost); }
ComponentConfig::~ComponentConfig() { ee_won_cc_free(this->mCfg); }
bool ComponentConfig::EndOfString() { return CC_GET(bool, CcK::EndOfString); }
void ComponentConfig::EnsureComma() { CC(EnsureComma, 0); }
int ComponentConfig::ReadInt() { return CC_GET(int32_t, CcK::ReadInt); }
bool ComponentConfig::ReadBool() { return CC_GET(bool, CcK::ReadBool); }
int ComponentConfig::ReadChar() { return CC_GET(int32_t, CcK::ReadChar); }
GUIString ComponentConfig::ReadFlag(bool* outOn) { return CC_GET(GUIString, CcK::ReadFlag, (intptr_t)outOn); }
GUIString ComponentConfig::ReadRawString(bool inStop) { return CC_GET(GUIString, CcK::ReadRawString, inStop); }
GUIString ComponentConfig::ReadString(bool inStop) { return CC_GET(GUIString, CcK::ReadString, inStop); }
Direction ComponentConfig::ReadDirection() { return (Direction)CC_GET(int32_t, CcK::ReadDirection); }
HorzAlign ComponentConfig::ReadHorzAlign() { return (HorzAlign)CC_GET(int32_t, CcK::ReadHorzAlign); }
VertAlign ComponentConfig::ReadVertAlign() { return (VertAlign)CC_GET(int32_t, CcK::ReadVertAlign); }
int ComponentConfig::ReadColor() { return CC_GET(int32_t, CcK::ReadColor); }
SmartPtr<Sound> ComponentConfig::ReadSound() { return CcAdopt(CC_GET(Sound*, CcK::ReadSound)); }
Background ComponentConfig::ReadBackground() { return CC_GET(Background, CcK::ReadBackground); }
SmartPtr<Font> ComponentConfig::ReadFont() { return CcAdopt(CC_GET(Font*, CcK::ReadFont)); }
SmartPtr<Image> ComponentConfig::ReadImage(bool inRaw) { return CcAdopt(CC_GET(Image*, CcK::ReadImage, inRaw)); }
SmartPtr<RawImage> ComponentConfig::GetRawImageFromStr(const std::string& s) { return (RawImage*)CcAdopt(CC_GET(Image*, CcK::ImageFromStr, CC_P(s), 1)).get(); }
SmartPtr<RawImage> ComponentConfig::ReadRawImage() { return (RawImage*)this->ReadImage(true).get(); }
StandardCursor ComponentConfig::ReadStandardCursor() { return (StandardCursor)CC_GET(int32_t, CcK::ReadStandardCursor); }
SmartPtr<Cursor> ComponentConfig::ReadCursor() { return Cursor::GetStandardCursor((StandardCursor)CC_GET(int32_t, CcK::ReadCursor)); }
SelectionColor ComponentConfig::ReadSelectionColor()
{
    EeWonCcSelColor x = CC_GET(EeWonCcSelColor, CcK::ReadSelectionColor);
    if (x.mode == 0) return SelectionColor(x.background, (DWORD)x.foreground);
    return SelectionColor(x.background, x.mode == 1 ? ForegroundSelectionMode_InvertOriginalColor : ForegroundSelectionMode_UseOriginalColor);
}
// the kind's own named values (the WON frame's paddings: cfgkind.rs), else the component's edges (compcfg.rs)
bool ComponentConfig::GetNamedValue(const std::string& k, int& v) { return CC_GET(bool, CcK::GetNamedValue, CC_P(k), (intptr_t)&v); }
// the kind's predefined children (cfgkind.rs): a new config of the child's kind and the part
bool ComponentConfig::GetPredefinedComponent(const std::string& n, ComponentConfig*& config, Component*& component)
{
    EeWonCcKey x = {(const uint8_t*)n.data(), n.size()};
    return CC_GET(bool, CcK::Predefined, (intptr_t)&x, (intptr_t)&config, (intptr_t)&component);
}
bool ComponentConfig::CallNamedFunction(const std::string& k, const std::vector<int>& p, int& v)
{
    EeWonCcCallArgs x = {(const uint8_t*)k.data(), k.size(), p.empty() ? NULL : &p[0], p.size(), &v};
    return CC_GET(bool, CcK::CallNamedFunction, (intptr_t)&x);
}
// the base group included; a WON skin's config then makes the game's skin (the Windows look) when there is none
bool ComponentConfig::PreParse(ConfigParser* p) { return CC_GET(bool, CcK::PreParse, (intptr_t)p); }
bool ComponentConfig::PostParse(ConfigParser*) { this->SetDesiredSizeIfNecessary(); return true; }
void ComponentConfig::SetDesiredSizeIfNecessary() { CC(SetDesiredSize, 0); }
bool ComponentConfig::HandleInstruction(ConfigParser* p, const std::string& t) { return CC_GET(bool, CcK::HandleInstruction, (intptr_t)p, CC_P(t)); }
bool ComponentConfig::HandleValueInstruction(ConfigParser* p, const std::string& k, StringParser& v)
{ EeWonCcKey x = {(const uint8_t*)k.data(), k.size()}; return CC_GET(bool, CcK::HandleValueInstruction, (intptr_t)p, (intptr_t)&x, (intptr_t)&v); }
Component* ComponentConfig::Fn386860(const std::string& n) { return (Component*)CC_GET(void*, CcK::ChildComponent, CC_P(n)); }
Component* ComponentConfig::ReadComponent() { return (Component*)CC_GET(void*, CcK::ReadComponent); }
bool ComponentConfig::HandleMapComponent() { return CC_GET(bool, CcK::HandleMapComponent); }
void ComponentConfig::AddChildToConfigMap(const GUIString& n, ComponentConfig* c, bool inAlways)
{ EeWonCcText t = CcText(n); CC(AddChild, (intptr_t)&t, (intptr_t)static_cast<ConfigObject*>(c), inAlways); }
bool ComponentConfig::HandleModify() { return CC_GET(bool, CcK::HandleModify); }
void ComponentConfig::InitComponents() { CC(InitComponents, 0); }
static void CcStringSink(void* c, const uint8_t* b, uintptr_t n) { static_cast<std::string*>(c)->assign((const char*)b, n); }
void ComponentConfig::SetResourceFolder(const std::string& f) { CcRun(NULL, CcK::SetResourceFolder, 0, CC_P(f)); }
std::string ComponentConfig::GetResourceFile(const std::string& f) { return CcGet<std::string>(NULL, CcK::GetResourceFile, CC_P(f), (intptr_t)&CcStringSink); }
#undef CC
#undef CC_GET

ResourceConfig::ResourceConfig() { this->mRes = ee_won_cc_res_new(static_cast<ConfigObject*>(this)); }
ResourceConfig::~ResourceConfig() { ee_won_cc_res_free(this->mRes, CcHost); }
bool ResourceConfig::HandleValueInstruction(ConfigParser* p, const std::string& k, StringParser& v)
{ EeWonCcKey x = {(const uint8_t*)k.data(), k.size()}; return CcGet<bool>(this->mRes, CcK::ResHandleValue, (intptr_t)p, (intptr_t)&x, (intptr_t)&v); }
bool ResourceConfig::PreParse(ConfigParser*) { CcRun(this->mRes, CcK::ResPreParse); return true; }
GUIString ResourceConfig::SafeGetString(const std::string& k) { return CcGet<GUIString>(this->mRes, CcK::ResSafeGetString, CC_P(k)); }
Image* ResourceConfig::SafeGetImage(const std::string& k) { return (Image*)CcGet<void*>(this->mRes, CcK::ResSafeGetPtr, CC_P(k), 0); }
Sound* ResourceConfig::SafeGetSound(const std::string& k) { return (Sound*)CcGet<void*>(this->mRes, CcK::ResSafeGetPtr, CC_P(k), 2); }
void ResourceConfig::AddSound(const std::string& k, Sound* s) { CcRun(this->mRes, CcK::ResAddSound, 0, CC_P(k), (intptr_t)s); }

ResourceConfigTable::ResourceConfigTable() { this->mTable = ee_won_cc_table_new(this); }
ResourceConfigTable::~ResourceConfigTable() { ee_won_cc_table_free(this->mTable, CcHost); }
ResourceConfig* ResourceConfigTable::AddConfig(const std::string& n, bool* outExists)
{ return static_cast<ResourceConfig*>((ConfigObject*)CcGet<void*>(this->mTable, CcK::TableAddConfig, CC_P(n), (intptr_t)outExists)); }
ResourceConfig* ResourceConfigTable::SafeGetConfig(const std::string& n)
{ return static_cast<ResourceConfig*>((ConfigObject*)CcGet<void*>(this->mTable, CcK::TableSafeGetConfig, CC_P(n))); }
void ResourceConfigTable::RegisterGlueResourceFunc(GlueResourceFunc f) { CcRun(this->mTable, CcK::TableRegisterGlue, (intptr_t)f); }
void ResourceConfigTable::SafeGlueResources() { CcRun(this->mTable, CcK::TableGlue); }
void ResourceConfigTable::AddAllocator(const std::string& n, ComponentConfigFactory c, ComponentFactory f)
{ CcRun(NULL, CcK::AddAllocator, (intptr_t)c, CC_P(n), (intptr_t)f); }
void ResourceConfigTable::RegisterControlId(const std::string& n, int v) { CcRun(NULL, CcK::RegisterControlId, v, CC_P(n)); }
#undef CC_P
