#include "rust/won_gui/cfgcomp.h"
#include "rust/won_gui/cfgkind.h"
#include "rust/won_gui/cfgset.h"
#include "rust/won_gui/cfgtab.h"
#include "rust/won_gui/guiconf.h"
#include "rust/won_gui/shell.h"
#include "rust/won_gui/skinobj.h"
// The table reader's host (R6 WON round 39): the reads, the frame and the setters of the 49 table-shaped config
// handlers whose programs are ee_won's cfgtab.rs. Included inside a namespace by ConfigTableHost.cpp (WONAPI) and
// by tests/won/won_cfgtab_test.cpp (over its stand-ins), so both run these very statements. A read or setter that
// throws is caught and kept; ComponentConfig::RunTable rethrows it after the Rust returns.
struct CfgTabFrame {
    ComponentConfig* mConfig; std::exception_ptr mError;
    int i[12]; int ni; GUIString s[2]; int ns; SmartPtr<Font> f[1]; int nf; SmartPtr<Image> m[1]; int nm;
    Component* c[2]; int nc; SmartPtr<ChildLayoutInstructionBase> lay; SmartPtr<Sound> snd; Background bg; SelectionColor sel; WONFrame fr; SmartPtr<Cursor> cur;
    SmartPtr<RawImage> raw; std::string rs[2]; int nrs; SmartPtr<ComponentConfig> child;
};
static void CfgTabFail(void* p,const uint8_t* inMsg,uintptr_t inLen,int32_t inSlot)
{
    CfgTabFrame& a=*static_cast<CfgTabFrame*>(p);
    std::string aText((const char*)inMsg,inLen);
    if(inSlot>=0) aText+=(std::string)a.s[inSlot];
    a.mError=std::make_exception_ptr(ConfigObjectException(aText));
}
#define EE_CFG(T) static_cast<T*>(a.mConfig)
#define EE_C(T) static_cast<T*>(a.mConfig->mComponent.get()) // the config kind's component (the original classes' typed copy)
static void CfgAppend(void* v, const std::string& s) { ee_won_cfg_comp_append(v, (const uint8_t*)s.data(), s.size()); }
// an op's statement under a guard: its exception kept in the frame, the op answering 0 with *ok cleared
template<class F> static intptr_t CfgTabGuard(CfgTabFrame& a, bool* ok, F f)
{
    try { return (intptr_t)f(); } catch (...) { a.mError = std::current_exception(); *ok = false; return 0; }
}
#define CFG_G(...) CfgTabGuard(a, ok, [&] { return (intptr_t)(__VA_ARGS__); })
#define CFG_GV(...) CfgTabGuard(a, ok, [&] { __VA_ARGS__; return (intptr_t)0; })
// The compound setters' and reads' statements, one each, for their compositions in ee_won_gui cfgcomp.rs (R6 WON
// round 34): the frame's values, the components, the layout state, the stretch and filter configs' children
static intptr_t CfgTabOp(void* p,EeWonCfgTabOp op,intptr_t x,intptr_t y,intptr_t z,bool* ok)
{
    CfgTabFrame& a=*static_cast<CfgTabFrame*>(p);
    typedef EeWonCfgTabOp O;
    switch(op) {
    case O::Int: return a.i[x];
    case O::IntCount: return a.ni;
    case O::PopInt: return a.i[--a.ni];
    case O::PushInt: a.i[a.ni++]=(int)x; return 0;
    case O::PushComp: a.c[a.nc++]=(Component*)x; return 0;
    case O::Fail: CfgTabFail(&a,(const uint8_t*)x,(uintptr_t)y,(int32_t)z); return 0;
    case O::SkinCopySimple: return CFG_G(ee_won_skin_copy_type(WONSkinGlobal(),true,&a.s[0],EE_C(Component)));
    case O::SkinCopyButton: return CFG_G(ee_won_skin_copy_type(WONSkinGlobal(),false,&a.s[0],static_cast<Component*>(EE_C(WONButton))));
    case O::RightTextPad: return EE_C(WONButton)->GetRightTextPad();
    case O::SetHorzTextPad: EE_C(WONButton)->SetHorzTextPad((int)x,(int)y); return 0;
    case O::NewScrollRect: EE_C(Scrollbar)->SetScrollRect(new Button); return 0;
    case O::ClearScrollFrame: WONShellFrameOp(EE_C(ScrollbarScroller),EeWonShellOp::SetFrame,WONFrame()); return 0;
    case O::CompLeft: return a.mConfig->mComponent.get()->Left();
    case O::CompTop: return a.mConfig->mComponent.get()->Top();
    case O::CompWidth: return a.mConfig->mComponent.get()->Width();
    case O::CompHeight: return a.mConfig->mComponent.get()->Height();
    case O::CompSetPos: a.mConfig->mComponent.get()->SetPos((int)x,(int)y); return 0;
    case O::CompSetSize: a.mConfig->mComponent.get()->SetSize((int)x,(int)y); return 0;
    case O::RequestFocus: a.mConfig->mComponent->RequestFocus(); return 0;
    case O::SafeWonButton: return CFG_G(SafeConfigGetComponent<WONButton*>(a.c[0],"WONButton"));
    case O::SkinSetRadio: WONSkinSet(EeWonSkinSlot::RadioButton,0,(WONButton*)x); return 0;
    case O::RadioFlag: ((WONButton*)x)->SetButtonFlags(8,true); return 0;
    case O::SafeTabCtrl: return CFG_G(SafeConfigGetSkinPart(a.c[0],"WONTabCtrl"));
    case O::SkinSetTabCtrl: WONSkinSet(EeWonSkinSlot::TabCtrl,0,(Component*)x); return 0;
    case O::ShellSetSkin: WONShell::Op((Component*)x,EeWonShellOp::SetSkin,0); return 0;
    case O::FontStyleOf: return EE_CFG(FontConfig)->mDescriptor.mStyle;
    case O::SetFontStyle: EE_CFG(FontConfig)->mDescriptor.mStyle=(int)x; return 0;
    case O::PointConfigNew: return (intptr_t)new SmartPtr<ComponentConfig>(new ComponentConfig);
    case O::PointNew: return (intptr_t)new SmartPtr<LayoutPoint>(new LayoutPoint);
    case O::PointBind: return CFG_GV((*(SmartPtr<ComponentConfig>*)x)->SetComponent(((SmartPtr<LayoutPoint>*)y)->get()));
    case O::PointParent: (*(SmartPtr<ComponentConfig>*)x)->mParent=EE_CFG(ContainerConfig); return 0;
    case O::PointAddConfig: return CFG_GV(EE_CFG(ContainerConfig)->AddChildToConfigMap(a.s[--a.ns],((SmartPtr<ComponentConfig>*)x)->get(),true));
    case O::PointAdd: return CFG_GV(EE_CFG(ContainerConfig)->mContainer->AddLayoutPoint(((SmartPtr<LayoutPoint>*)x)->get()));
    case O::PointFree: delete (SmartPtr<LayoutPoint>*)x; return 0;
    case O::PointConfigFree: delete (SmartPtr<ComponentConfig>*)x; return 0;
    case O::EnableLayout: EE_CFG(ContainerConfig)->mContainer->EnableLayoutInstructions((int)x,y!=0); return 0;
    case O::LayoutInstruction: return EE_CFG(ContainerConfig)->mLayoutInstruction;
    case O::LayoutParam: return EE_CFG(ContainerConfig)->mLayoutParams[x];
    case O::LayoutRef: return (intptr_t)EE_CFG(ContainerConfig)->mLayoutRef;
    case O::SetLayoutParam: EE_CFG(ContainerConfig)->mLayoutParams[x]=(int)y; return 0;
    case O::AddLayoutRule: return CFG_GV(EE_CFG(ContainerConfig)->mContainer->Fn2F6140(a.c[0],((const int*)x)[0],a.c[1],((const int*)x)[1],((const int*)x)[2],((const int*)x)[3],((const int*)x)[4],EE_CFG(ContainerConfig)->mCurLayoutId));
    case O::SetLayoutRef: EE_CFG(ContainerConfig)->mLayoutRef=a.c[x]; return 0;
    case O::SetLayoutInstruction: EE_CFG(ContainerConfig)->mLayoutInstruction=(int)x; return 0;
    case O::RawStr: CfgAppend((void*)y,a.rs[x]); return 0;
    case O::StrNarrow: CfgAppend((void*)y,(std::string)a.s[x]); return 0;
    case O::SetIntCount: a.ni=(int)x; return 0;
    case O::RawFromStr: return CFG_G(new SmartPtr<RawImage>(EE_CFG(StretchImageConfig)->GetRawImageFromStr(std::string((const char*)x,(size_t)y))));
    case O::CropRaw: return (intptr_t)new SmartPtr<RawImage>(ImageCrop(a.raw.get(),(int)x,0,(int)y,a.raw->GetHeight()));
    case O::CropFilter: return (intptr_t)new SmartPtr<RawImage>(ImageCrop(EE_CFG(ImageFilterConfig)->mRawImage.get(),(int)x,0,(int)y,EE_CFG(ImageFilterConfig)->mRawImage->GetHeight()));
    case O::HeldRaw: return (intptr_t)((SmartPtr<RawImage>*)x)->get();
    case O::RawFree: delete (SmartPtr<RawImage>*)x; return 0;
    case O::StretchNew: return (intptr_t)new StretchImageConfig;
    case O::StretchCopy: ((StretchImageConfig*)x)->mStretchImage->CopyAttributes(EE_CFG(StretchImageConfig)->mStretchImage); return 0;
    case O::StretchHold: return (intptr_t)new SmartPtr<StretchImageConfig>((StretchImageConfig*)x);
    case O::StretchFree: delete (SmartPtr<StretchImageConfig>*)x; return 0;
    case O::StretchSetImage: ((StretchImageConfig*)x)->mStretchImage->SetImage((RawImage*)y); return 0;
    case O::StretchToParent: return CFG_GV(static_cast<ComponentConfig*>(a.mConfig->GetParentObject())->AddChildToConfigMap(GUIString(std::string((const char*)x,(size_t)y)),(StretchImageConfig*)z));
    case O::StretchToParentS0: return CFG_GV(static_cast<ComponentConfig*>(a.mConfig->GetParentObject())->AddChildToConfigMap(a.s[0],(StretchImageConfig*)z));
    case O::FrameChild: return (intptr_t)a.child.get();
    case O::FrameRaw: return (intptr_t)a.raw.get();
    case O::SetFrameChild: a.child=(StretchImageConfig*)x; return 0;
    case O::FilterNew: return (intptr_t)new SmartPtr<ImageFilterConfig>(new ImageFilterConfig);
    case O::FilterSetRaw: (*(SmartPtr<ImageFilterConfig>*)x)->SetRawImage((RawImage*)y); return 0;
    case O::FilterToParent: return CFG_GV(static_cast<ComponentConfig*>(a.mConfig->GetParentObject())->AddChildToConfigMap(GUIString(std::string((const char*)x,(size_t)y)),((SmartPtr<ImageFilterConfig>*)z)->get()));
    case O::FilterToParentS0: return CFG_GV(static_cast<ComponentConfig*>(a.mConfig->GetParentObject())->AddChildToConfigMap(a.s[0],((SmartPtr<ImageFilterConfig>*)z)->get()));
    case O::FilterFree: delete (SmartPtr<ImageFilterConfig>*)x; return 0;
    case O::ReadFlagText: return CFG_GV(a.s[a.ns++]=a.mConfig->ReadFlag((bool*)x));
    case O::ReadComp: return CFG_G(a.mConfig->ReadComponent());
    case O::IsScrollArea: return dynamic_cast<ScrollArea*>((Component*)x)!=NULL;
    case O::PushScrollArea: a.c[a.nc++]=dynamic_cast<ScrollArea*>((Component*)x); return 0;
    case O::ReadImageHold: return CFG_G(new SmartPtr<Image>(a.mConfig->ReadImage(false)));
    case O::IsStretch: return dynamic_cast<StretchImage*>(((SmartPtr<Image>*)x)->get())!=NULL;
    case O::FrameOfStretch: a.fr=dynamic_cast<StretchImage*>(((SmartPtr<Image>*)x)->get()); return 0;
    case O::Comma: return CFG_GV(a.mConfig->EnsureComma());
    case O::ReadInt: return CFG_G(a.mConfig->ReadInt());
    case O::EndOfString: return a.mConfig->EndOfString();
    case O::FrameOne: a.fr=WONFrame(((SmartPtr<Image>*)x)->get(),(int)y); return 0;
    case O::FrameFour: a.fr=WONFrame(((SmartPtr<Image>*)x)->get(),((const int*)y)[0],((const int*)y)[1],((const int*)y)[2],((const int*)y)[3]); return 0;
    case O::ImageFree: delete (SmartPtr<Image>*)x; return 0;
    case O::SetHasWidth: a.mConfig->mHasSetWidth=x!=0; return 0;
    case O::SetHasHeight: a.mConfig->mHasSetHeight=x!=0; return 0;
    case O::CompSizeAtLeast: a.mConfig->mComponent->SetSizeAtLeast((int)x,(int)y); return 0;
    case O::ResetLineCount: ConfigParser::ResetLineCount(); return 0;
    case O::TimerStart: PerfTimer::GlobalStart(NULL); return 0;
    case O::TimerStop: PerfTimer::GlobalStop(NULL); return 0;
    case O::KeepLinesParsed: gLinesParsed=ConfigParser::GetLineCount(); return 0;
    case O::CursorNew: return (intptr_t)new SmartPtr<Cursor>(new ComponentCursor(new ImageComponent(a.m[0].get())));
    case O::SetStdCursor: Cursor::SetStandardCursor((StandardCursor)a.i[0],((SmartPtr<Cursor>*)x)->get()); return 0;
    case O::CursorFree: delete (SmartPtr<Cursor>*)x; return 0;
    case O::LaySpaceEvenly: a.lay=new SpaceEvenlyLayout(a.c[--a.nc],a.i[0],a.i[1],x!=0); return 0;
    case O::LayClearRight: a.lay=new ClearRightLayout(a.c[--a.nc],a.i[0]); return 0;
    case O::LaySizeAtLeast: a.lay=new SizeAtLeastLayout(a.c[--a.nc],a.i[0],a.i[1]); return 0;
    case O::LayNoOverlap: a.lay=new NoOverlapLayout(a.c[0],a.i[2],a.c[1],a.i[0],a.i[1]); return 0;
    case O::SetCompCount: a.nc=(int)x; return 0;
    default: return 0;
    }
}
#undef CFG_G
#undef CFG_GV
// a composition of cfgcomp.rs over the frame; its op's exception (kept in the frame) rethrown
static void CfgComp(CfgTabFrame& a,EeWonCfgComp inWhich) { if(!ee_won_cfg_comp(CfgTabOp,&a,inWhich)) std::rethrow_exception(a.mError); }
static void CfgTabClear(void* p)
{
    CfgTabFrame& a=*static_cast<CfgTabFrame*>(p);
    a.ni=a.ns=a.nf=a.nm=a.nc=a.nrs=0; a.f[0]=NULL; a.m[0]=NULL; a.lay=NULL; a.raw=NULL;
}
static bool CfgTabRead(void* p,EeWonCfgRead inKind)
{
    CfgTabFrame& a=*static_cast<CfgTabFrame*>(p);
    ComponentConfig* cfg=a.mConfig;
    try {
        switch(inKind) {
        case EeWonCfgRead::Int: a.i[a.ni++]=cfg->ReadInt(); break;
        case EeWonCfgRead::Bool: a.i[a.ni++]=cfg->ReadBool(); break;
        case EeWonCfgRead::Color: a.i[a.ni++]=cfg->ReadColor(); break;
        case EeWonCfgRead::Char: a.i[a.ni++]=cfg->ReadChar(); break;
        case EeWonCfgRead::VertAlign: a.i[a.ni++]=cfg->ReadVertAlign(); break;
        case EeWonCfgRead::HorzAlign: a.i[a.ni++]=cfg->ReadHorzAlign(); break;
        case EeWonCfgRead::Str: a.s[a.ns++]=cfg->ReadString(false); break;
        case EeWonCfgRead::Flag: return ee_won_cfg_comp(CfgTabOp,&a,EeWonCfgComp::ReadFlag);
        case EeWonCfgRead::Font: a.f[a.nf++]=cfg->ReadFont(); break;
        case EeWonCfgRead::Image: a.m[a.nm++]=cfg->ReadImage(false); break;
        case EeWonCfgRead::Sound: a.snd=cfg->ReadSound(); break;
        case EeWonCfgRead::Background: a.bg=cfg->ReadBackground(); break;
        case EeWonCfgRead::SelColor: a.sel=cfg->ReadSelectionColor(); break;
        case EeWonCfgRead::Comp: a.c[a.nc++]=cfg->ReadComponent(); break;
        case EeWonCfgRead::Container: a.c[a.nc++]=SafeConfigGetComponent<Container*>(cfg->ReadComponent(),"Container"); break;
        case EeWonCfgRead::Button: a.c[a.nc++]=SafeConfigGetComponent<Button*>(cfg->ReadComponent(),"Button"); break;
        case EeWonCfgRead::Scroller: a.c[a.nc++]=SafeConfigGetComponent<Scroller*>(cfg->ReadComponent(),"Scroller"); break;
        case EeWonCfgRead::MultiListArea: a.c[a.nc++]=SafeConfigGetComponent<MultiListArea*>(cfg->ReadComponent(),"MultiListArea"); break;
        case EeWonCfgRead::ScrollArea: return ee_won_cfg_comp(CfgTabOp,&a,EeWonCfgComp::ReadScrollArea);
        case EeWonCfgRead::Frame: return ee_won_cfg_comp(CfgTabOp,&a,EeWonCfgComp::ReadFrame);
        case EeWonCfgRead::Cursor: a.cur=cfg->ReadCursor(); break;
        case EeWonCfgRead::Direction: a.i[a.ni++]=cfg->ReadDirection(); break;
        case EeWonCfgRead::StdCursor: a.i[a.ni++]=cfg->ReadStandardCursor(); break;
        case EeWonCfgRead::LayoutPoint: a.c[a.nc++]=SafeConfigGetComponent<LayoutPoint*>(cfg->ReadComponent(),"LayoutPoint"); break;
        case EeWonCfgRead::Word: a.s[0]=cfg->ReadString(true); a.ns=1; break;
        case EeWonCfgRead::RawImage: a.raw=cfg->ReadRawImage(); break;
        case EeWonCfgRead::RawStr: a.rs[a.nrs++]=cfg->ReadRawString(false); break;
        }
        return true;
    } catch(...) { a.mError=std::current_exception(); return false; }
}
static bool CfgTabComma(void* p)
{
    CfgTabFrame& a=*static_cast<CfgTabFrame*>(p);
    try { a.mConfig->EnsureComma(); return true; } catch(...) { a.mError=std::current_exception(); return false; }
}
static bool CfgTabEnd(void* p) { return static_cast<CfgTabFrame*>(p)->mConfig->EndOfString(); }
static const void* CfgTabText(void* p,int32_t inSlot,uint16_t* outLen,uint32_t* outUnit)
{
    const GUIString& s=static_cast<CfgTabFrame*>(p)->s[inSlot];
    *outLen=s.mData->mLength; *outUnit=sizeof(wchar_t);
    return s.mData->mText;
}
static void CfgTabPush(void* p,int32_t inValue) { CfgTabFrame& a=*static_cast<CfgTabFrame*>(p); a.i[a.ni++]=inValue; }
static int32_t CfgTabPeek(void* p,int32_t inSlot) { return static_cast<CfgTabFrame*>(p)->i[inSlot]; }
// the frame's values for the setters made in Rust (ee_won_gui cfgset.rs: those that call a widget's Rust object)
static intptr_t CfgTabArg(void* p,EeWonCfgArg inWhat,int32_t inSlot)
{
    CfgTabFrame& a=*static_cast<CfgTabFrame*>(p);
    switch(inWhat) {
    case EeWonCfgArg::Int: return a.i[inSlot];
    case EeWonCfgArg::IntCount: return a.ni;
    case EeWonCfgArg::StrPtr: return (intptr_t)&a.s[inSlot];
    case EeWonCfgArg::Font: return (intptr_t)a.f[inSlot].get();
    case EeWonCfgArg::FontCount: return a.nf;
    case EeWonCfgArg::Image: return (intptr_t)a.m[inSlot].get();
    case EeWonCfgArg::Comp: return (intptr_t)a.c[inSlot];
    case EeWonCfgArg::Component: return (intptr_t)a.mConfig->mComponent.get();
    case EeWonCfgArg::StrUnits: return (intptr_t)a.s[inSlot].mData->mText;
    case EeWonCfgArg::StrLen: return a.s[inSlot].length();
    case EeWonCfgArg::CompCount: return a.nc;
    }
    return 0;
}
enum {
    kButton=EE_WON_CFG_KEYS_BUTTONCONFIG_VIRTUAL24_ININSTRUCTION, kMSButton=EE_WON_CFG_KEYS_MSBUTTONCONFIG_VIRTUAL24_KEY,
    kMSArrow=EE_WON_CFG_KEYS_MSARROWBUTTONCONFIG_VIRTUAL24_KEY, kMSTabButton=EE_WON_CFG_KEYS_MSTABBUTTONCONFIG_VIRTUAL24_KEY,
    kMS3DFrame=EE_WON_CFG_KEYS_MS3DFRAMECONFIG_VIRTUAL24_KEY, kMSCombo=EE_WON_CFG_KEYS_MSCOMBOBOXCONFIG_VIRTUAL24_KEY,
    kMSScrollbar=EE_WON_CFG_KEYS_MSSCROLLBARCONFIG_VIRTUAL24_KEY, kMSEtched=EE_WON_CFG_KEYS_MSETCHEDFRAMEGROUPCONFIG_VIRTUAL24_KEY,
    kMSTabCtrl=EE_WON_CFG_KEYS_MSTABCTRLCONFIG_VIRTUAL24_KEY, kMSScrollCont=EE_WON_CFG_KEYS_MSSCROLLCONTAINERCONFIG_VIRTUAL24_KEY,
    kMSHeader=EE_WON_CFG_KEYS_MSHEADERCONTROLCONFIG_VIRTUAL24_KEY, kMSListCtrl=EE_WON_CFG_KEYS_MSLISTCTRLCONFIG_VIRTUAL24_KEY,
    kPercent=EE_WON_CFG_KEYS_PERCENTLAYOUTCONFIG_VIRTUAL24_INSTRUCTION, kScheme=EE_WON_CFG_KEYS_COLORSCHEMECONFIG_VIRTUAL24_INSTRUCTION,
    kRect=EE_WON_CFG_KEYS_RECTANGLECOMPONENTCONFIG_VIRTUAL24_KEY, kRect3D=EE_WON_CFG_KEYS_RECTANGLECOMPONENT3DCONFIG_VIRTUAL24_KEY,
    kImageComp=EE_WON_CFG_KEYS_IMAGECOMPONENTCONFIG_VIRTUAL24_KEY, kScreen=EE_WON_CFG_KEYS_SCREENCONTAINERCONFIG_VIRTUAL24_KEY,
    kMultiList=EE_WON_CFG_KEYS_MULTILISTAREACONFIG_VIRTUAL24_INSTRUCTION, kMultiHeader=EE_WON_CFG_KEYS_MULTILISTHEADERCONFIG_VIRTUAL24_INSTRUCTION,
    kScrollbar=EE_WON_CFG_KEYS_SCROLLBARCONFIG_VIRTUAL24_INSTRUCTION, kTabCtrl=EE_WON_CFG_KEYS_TABCTRLCONFIG_VIRTUAL24_KEY,
    kBrowser=EE_WON_CFG_KEYS_BROWSERCOMPONENTCONFIG_VIRTUAL24_INSTRUCTION, kCombo=EE_WON_CFG_KEYS_COMBOBOXCONFIG_VIRTUAL24_KEY,
    kDialog=EE_WON_CFG_KEYS_DIALOGCONFIG_VIRTUAL24_INSTRUCTION, kImageButton=EE_WON_CFG_KEYS_IMAGEBUTTONCONFIG_VIRTUAL24_INSTRUCTION,
    kLabel=EE_WON_CFG_KEYS_LABELCONFIG_VIRTUAL24_INSTRUCTION, kListArea=EE_WON_CFG_KEYS_LISTAREACONFIG_VIRTUAL24_ININSTRUCTION,
    kInput=EE_WON_CFG_KEYS_INPUTBOXCONFIG_VIRTUAL24_ININSTRUCTION, kTextArea=EE_WON_CFG_KEYS_TEXTAREACONFIG_VIRTUAL24_KEY,
    kTree=EE_WON_CFG_KEYS_TREEAREACONFIG_VIRTUAL24_INSTRUCTION, kSimple=EE_WON_CFG_KEYS_WONSIMPLEBUTTONCONFIG_VIRTUAL24_THEINSTRUCTION,
    kWONButton=EE_WON_CFG_KEYS_WONBUTTONCONFIG_VIRTUAL24_THEINSTRUCTION, kSplitter=EE_WON_CFG_KEYS_SPLITTERCONFIG_VIRTUAL24_THEINSTRUCTION,
    kWScrollbar=EE_WON_CFG_KEYS_WONSCROLLBARCONFIG_VIRTUAL24_KEY, kWHeader=EE_WON_CFG_KEYS_WONHEADERCONTROLCONFIG_VIRTUAL24_KEY,
    kWListCtrl=EE_WON_CFG_KEYS_WONLISTCTRLCONFIG_VIRTUAL24_KEY, kWFrame=EE_WON_CFG_KEYS_WONFRAMECOMPONENTCONFIG_VIRTUAL24_KEY,
    kWCombo=EE_WON_CFG_KEYS_WONCOMBOBOXCONFIG_VIRTUAL24_KEY, kWTabCtrl=EE_WON_CFG_KEYS_WONTABCTRLCONFIG_VIRTUAL24_KEY,
    kWScrollCont=EE_WON_CFG_KEYS_WONSCROLLCONTAINERCONFIG_VIRTUAL24_KEY, kScrollArea=EE_WON_CFG_KEYS_SCROLLAREACONFIG_VIRTUAL24_INSTRUCTION,
    kScroller=EE_WON_CFG_KEYS_SCROLLERCONFIG_VIRTUAL24_INSTRUCTION, kSBScroller=EE_WON_CFG_KEYS_SCROLLBARSCROLLERCONFIG_VIRTUAL24_INSTRUCTION,
    kStretchCenter=EE_WON_CFG_KEYS_STRETCHCENTERIMAGECONFIG_VIRTUAL24_INSTRUCTION,
    kComponent=EE_WON_CFG_KEYS_COMPONENTCONFIG_VIRTUAL24_KEY, kSkin=EE_WON_CFG_KEYS_WONSKINCONFIG_VIRTUAL24_THEINSTRUCTION,
    kFont=EE_WON_CFG_KEYS_FONTCONFIG_VIRTUAL24_INSTRUCTION, kImage=EE_WON_CFG_KEYS_IMAGECONFIG_VIRTUAL24_ININSTRUCTION,
    kContainer=EE_WON_CFG_KEYS_CONTAINERCONFIG_VIRTUAL24_THEINSTRUCTION,
    kStretchImage=EE_WON_CFG_KEYS_STRETCHIMAGECONFIG_VIRTUAL24_INSTRUCTION, kImageFilter=EE_WON_CFG_KEYS_IMAGEFILTERCONFIG_VIRTUAL24_ININSTRUCTION
};
#define EE_CFG_KEY(t,n) ((t)<<8|(n)) // a setter's number: its table's key and its place
// The setters, as each handler called them on its typed component (bools read as 0/1). Those that called an inline
// forward to the component's Rust object are cfgset.rs's (R6 WONLobby round 27): these are the rest.
static void CfgTabSet(CfgTabFrame& a,int inSetter)
{
    const int* i=a.i; const GUIString* s=a.s; Component* const* c=a.c;
    switch(inSetter) {
    case EE_CFG_KEY(kButton,1): EE_C(Button)->SetText(s[0],true); break;
    case EE_CFG_KEY(kButton,3): EE_C(Button)->SetClickSound(a.snd.get()); break;
    case EE_CFG_KEY(kButton,4): a.mConfig->HandleFitText(); break;
    case EE_CFG_KEY(kMSButton,0): EE_C(MSButtonBase)->SetFont(a.f[0]); break;
    case EE_CFG_KEY(kMSButton,6): EE_C(MSButtonBase)->SetWrap(i[0]!=0); break;
    case EE_CFG_KEY(kPercent,0): EE_CFG(PercentLayoutConfig)->refs[i[0]]=c[0]; break;
    case EE_CFG_KEY(kPercent,1): EE_CFG(PercentLayoutConfig)->params[i[0]]=i[1]; break;
    case EE_CFG_KEY(kScheme,0): EE_CFG(ColorSchemeConfig)->mColorScheme.get()->SetStandardColor(i[0],(DWORD)i[1]); break;
    case EE_CFG_KEY(kRect,0): EE_C(RectangleComponent)->SetSolidColor(i[0]); break;
    case EE_CFG_KEY(kRect,1): EE_C(RectangleComponent)->SetOutlineColor(i[0]); break;
    case EE_CFG_KEY(kRect3D,0): EE_C(RectangleComponent3D)->SetSolidColor(i[0]); break;
    case EE_CFG_KEY(kRect3D,1): EE_C(RectangleComponent3D)->SetLightColor(i[0]); break;
    case EE_CFG_KEY(kRect3D,2): EE_C(RectangleComponent3D)->SetDarkColor(i[0]); break;
    case EE_CFG_KEY(kRect3D,3): EE_C(RectangleComponent3D)->SetIsUp(i[0]!=0); break;
    case EE_CFG_KEY(kRect3D,4): EE_C(RectangleComponent3D)->SetThickness(i[0]); break;
    case EE_CFG_KEY(kImageComp,0): EE_C(ImageComponent)->SetImage(a.m[0]); break;
    case EE_CFG_KEY(kImageComp,1): EE_C(ImageComponent)->mTile=i[0]!=0; break;
    case EE_CFG_KEY(kImageComp,2): EE_C(ImageComponent)->mStretch=i[0]!=0; break;
    case EE_CFG_KEY(kScreen,0): EE_C(ScreenContainer)->AddScreen(c[0]); break;
    case EE_CFG_KEY(kScreen,1): EE_C(ScreenContainer)->ShowScreen(c[0],true); break;
    case EE_CFG_KEY(kMultiList,4): EE_C(MultiListArea)->SetSortColumnFunc(i[0],ListArea::NumberSortFunc); break;
    case EE_CFG_KEY(kBrowser,0): EE_C(BrowserComponent)->SetTransparent(true); break;
    case EE_CFG_KEY(kBrowser,1): EE_C(BrowserComponent)->SetScroller(static_cast<Scroller*>(c[0])); break;
    case EE_CFG_KEY(kBrowser,2): EE_C(BrowserComponent)->SetFontSize((HTMLFontSize)i[0]); break;
    case EE_CFG_KEY(kCombo,7): EE_C(ComboBox)->mEdit.get()->SetMaxChars(i[0]); break;
    case EE_CFG_KEY(kDialog,4): EE_C(Dialog)->SetDefaultButton(static_cast<Button*>(c[0])); break;
    case EE_CFG_KEY(kImageButton,0): EE_C(ImageButton)->SetImage((ImageButtonType)i[0],a.m[0].get()); break;
    case EE_CFG_KEY(kLabel,0): EE_C(Label)->SetText(s[0],true); break;
    case EE_CFG_KEY(kLabel,1): EE_C(Label)->SetFont(a.f[0]); break;
    case EE_CFG_KEY(kLabel,3): EE_C(Label)->SetBackground(a.bg); break;
    case EE_CFG_KEY(kListArea,1): EE_C(ListArea)->InsertString(s[0],-1); break;
    case EE_CFG_KEY(kListArea,2): EE_C(ListArea)->SetFont(a.f[0]); break;
    case EE_CFG_KEY(kListArea,3): EE_C(ListArea)->mTextColor=i[0]; break;
    case EE_CFG_KEY(kListArea,4): EE_C(ListArea)->mSelColor=a.sel; break;
    case EE_CFG_KEY(kInput,5): EE_C(InputBox)->SetBackground(a.bg); break;
    case EE_CFG_KEY(kInput,6): EE_C(InputBox)->SetDisabledBackground(a.bg); break;
    case EE_CFG_KEY(kInput,7): EE_C(InputBox)->SetIcon(a.m[0].get()); break;
    case EE_CFG_KEY(kInput,8): EE_C(InputBox)->mTextColor=i[0]; break;
    case EE_CFG_KEY(kInput,9): EE_C(InputBox)->mDisabledTextColor=i[0]; break;
    case EE_CFG_KEY(kInput,10): EE_C(InputBox)->mSelColor=a.sel; break;
    case EE_CFG_KEY(kInput,13): EE_C(InputBox)->mUseDisabledColors=i[0]!=0; break;
    case EE_CFG_KEY(kTextArea,1): EE_C(TextArea)->AddSegment(s[0],false); break;
    case EE_CFG_KEY(kTextArea,2): EE_C(TextArea)->AddFormatedText(s[0],false); break;
    case EE_CFG_KEY(kTextArea,3): EE_C(TextArea)->SetFont(a.f[0]); break;
    case EE_CFG_KEY(kTextArea,4): EE_C(TextArea)->mTextColor=i[0]; break;
    case EE_CFG_KEY(kTextArea,5): EE_C(TextArea)->mSelColor=a.sel; break;
    case EE_CFG_KEY(kTextArea,6): EE_C(TextArea)->SetLinkFont(a.f[0]); break;
    case EE_CFG_KEY(kSimple,1): CfgComp(a,EeWonCfgComp::SimpleType); break;
    case EE_CFG_KEY(kWONButton,1): CfgComp(a,EeWonCfgComp::ButtonType); break;
    case EE_CFG_KEY(kWONButton,12): CfgComp(a,EeWonCfgComp::HorzTextPad); break;
    case EE_CFG_KEY(kWScrollbar,0): CfgComp(a,EeWonCfgComp::ScrollRect); break;
    case EE_CFG_KEY(kWListCtrl,1): static_cast<MultiListArea*>(EE_C(ScrollbarScroller)->GetScrollArea())->SetNumColumns(i[0]); break;
    case EE_CFG_KEY(kWListCtrl,2): static_cast<MultiListArea*>(EE_C(ScrollbarScroller)->GetScrollArea())->SetDynamicSizeColumn(i[0]); break;
    case EE_CFG_KEY(kWListCtrl,3): static_cast<ListArea*>(static_cast<MultiListArea*>(EE_C(ScrollbarScroller)->GetScrollArea()))->SetListFlags((unsigned long)i[1],i[0]!=0); break;
    case EE_CFG_KEY(kWCombo,2): WONShellFrameOp(EE_C(Component),EeWonShellOp::SetFrame,a.fr); break;
    case EE_CFG_KEY(kWTabCtrl,2): WONShellFrameOp(EE_C(Component),EeWonShellOp::SetFrame,a.fr); break;
    case EE_CFG_KEY(kWScrollCont,0): CfgComp(a,EeWonCfgComp::ScrollContNoFrame); break;
    case EE_CFG_KEY(kWScrollCont,2): WONShellFrameOp(EE_C(ScrollbarScroller),EeWonShellOp::SetFrame,a.fr); break;
    case EE_CFG_KEY(kScrollArea,0): EE_C(ScrollArea)->SetBackground(a.bg); break;
    case EE_CFG_KEY(kStretchCenter,0): EE_CFG(StretchCenterImageConfig)->mStretchCenterImage->SetStretchImage(a.m[0].get()); break;
    case EE_CFG_KEY(kStretchCenter,1): EE_CFG(StretchCenterImageConfig)->mStretchCenterImage->SetCenterImage(a.m[0].get()); break;
    case EE_CFG_KEY(kComponent,2): a.mConfig->mComponent.get()->mControlId=i[0]; break;
    case EE_CFG_KEY(kComponent,3): CfgComp(a,EeWonCfgComp::Left); break;
    case EE_CFG_KEY(kComponent,4): CfgComp(a,EeWonCfgComp::Top); break;
    case EE_CFG_KEY(kComponent,5): CfgComp(a,EeWonCfgComp::Width); break;
    case EE_CFG_KEY(kComponent,6): CfgComp(a,EeWonCfgComp::Height); break;
    case EE_CFG_KEY(kComponent,7): a.mConfig->mComponent->SetPos(i[0],i[1]); break;
    case EE_CFG_KEY(kComponent,8): a.mConfig->mComponent->SetSize(i[0],i[1]); break;
    case EE_CFG_KEY(kComponent,9): a.mConfig->mComponent->SetTranslucentColor(i[0],a.ni>1?i[1]:4); break;
    case EE_CFG_KEY(kComponent,10): CfgComp(a,EeWonCfgComp::SizeAtLeast); break;
    case EE_CFG_KEY(kComponent,11): a.mConfig->mComponent->SetDesiredSize(i[0],i[1]); break;
    case EE_CFG_KEY(kComponent,12): a.mConfig->mComponent.get()->SetDesiredWidth(i[0]); break;
    case EE_CFG_KEY(kComponent,13): a.mConfig->mComponent.get()->SetDesiredHeight(i[0]); break;
    case EE_CFG_KEY(kComponent,14): a.mConfig->mComponent->SetPosSize(i[0],i[1],i[2],i[3]); break;
    case EE_CFG_KEY(kComponent,15): a.mConfig->mComponent->SetEnabled(i[0]!=0); break;
    case EE_CFG_KEY(kComponent,16): a.mConfig->mComponent->SetVisible(i[0]!=0); break;
    case EE_CFG_KEY(kComponent,17): CfgComp(a,EeWonCfgComp::Focus); break;
    case EE_CFG_KEY(kComponent,18): a.mConfig->mComponent->SetDefaultCursor(a.cur); break;
    case EE_CFG_KEY(kComponent,19): a.mConfig->mComponent->SetComponentFlags(i[1],i[0]!=0); break;
    case EE_CFG_KEY(kComponent,20): Window::GetDefaultWindow()->SetSkipDecodeImages(!(i[0]!=0)); break;
    case EE_CFG_KEY(kComponent,21): gShowParseTime=i[0]!=0; break;
    case EE_CFG_KEY(kComponent,22): CfgComp(a,EeWonCfgComp::ParseTimerStart); break;
    case EE_CFG_KEY(kComponent,23): CfgComp(a,EeWonCfgComp::ParseTimerStop); break;
    case EE_CFG_KEY(kComponent,30): a.mConfig->mHasSetWidth=true; break;
    case EE_CFG_KEY(kComponent,31): a.mConfig->mHasSetHeight=true; break;
    case EE_CFG_KEY(kComponent,32): a.mConfig->mHasSetWidth=a.mConfig->mHasSetHeight=true; break;
    case EE_CFG_KEY(kComponent,33): a.mConfig->SetDesiredSizeIfNecessary(); break;
    case EE_CFG_KEY(kSkin,1): ee_won_skin_set_type(WONSkinGlobal(),false,&s[0],static_cast<Component*>(SafeConfigGetComponent<WONButton*>(c[0],"WONButton"))); break;
    case EE_CFG_KEY(kSkin,2): ee_won_skin_set_type(WONSkinGlobal(),true,&s[0],SafeConfigGetSkinPart(c[0],"WONSimpleButton")); break;
    case EE_CFG_KEY(kSkin,3): WONSkinSet(EeWonSkinSlot::PushButton,0,SafeConfigGetComponent<WONButton*>(c[0],"WONButton")); break;
    case EE_CFG_KEY(kSkin,4): WONSkinSet(EeWonSkinSlot::Checkbox,0,SafeConfigGetComponent<WONButton*>(c[0],"WONButton")); break;
    case EE_CFG_KEY(kSkin,5): CfgComp(a,EeWonCfgComp::SkinRadio); break;
    case EE_CFG_KEY(kSkin,6): WONSkinSet(EeWonSkinSlot::HeaderButton,0,SafeConfigGetSkinPart(c[0],"WONHeaderButton")); break;
    case EE_CFG_KEY(kSkin,7): WONSkinSet(EeWonSkinSlot::TabButton,0,SafeConfigGetComponent<WONButton*>(c[0],"WONButton")); break;
    case EE_CFG_KEY(kSkin,8): WONSkinSet(EeWonSkinSlot::Arrow0,i[0],SafeConfigGetSkinPart(c[0],"WONSimpleButton")); break;
    case EE_CFG_KEY(kSkin,9): WONSkinSet(EeWonSkinSlot::ComboButton,0,SafeConfigGetSkinPart(c[0],"WONSimpleButton")); break;
    case EE_CFG_KEY(kSkin,10): WONSkinSet(EeWonSkinSlot::CloseButton,0,SafeConfigGetSkinPart(c[0],"WONSimpleButton")); break;
    case EE_CFG_KEY(kSkin,11): WONSkinSet(EeWonSkinSlot::ScrollGutter0,i[0]?1:0,SafeConfigGetSkinPart(c[0],"WONSimpleButton")); break;
    case EE_CFG_KEY(kSkin,12): WONSkinSet(EeWonSkinSlot::ScrollThumb0,i[0]?1:0,SafeConfigGetSkinPart(c[0],"WONSimpleButton")); break;
    case EE_CFG_KEY(kSkin,13): WONSkinSet(EeWonSkinSlot::Scrollbar0,i[0]?1:0,SafeConfigGetSkinPart(c[0],"WONScrollbar")); break;
    case EE_CFG_KEY(kSkin,14): WONSkinSet(EeWonSkinSlot::ScrollContainer,0,SafeConfigGetSkinPart(c[0],"WONScrollContainer")); break;
    case EE_CFG_KEY(kSkin,15): WONSkinSet(EeWonSkinSlot::ListCtrl,0,SafeConfigGetSkinPart(c[0],"WONListCtrl")); break;
    case EE_CFG_KEY(kSkin,16): CfgComp(a,EeWonCfgComp::SkinTabCtrl); break;
    case EE_CFG_KEY(kSkin,17): WONSkinSet(EeWonSkinSlot::ComboBox,0,SafeConfigGetSkinPart(c[0],"WONComboBox")); break;
    case EE_CFG_KEY(kSkin,18): WONSkinSet(EeWonSkinSlot::PopupBack,0,SafeConfigGetSkinPart(c[0],"WONPopupBack")); break;
    case EE_CFG_KEY(kSkin,19): WONSkinSet(EeWonSkinSlot::DialogBack,0,SafeConfigGetSkinPart(c[0],"WONDialogBack")); break;
    case EE_CFG_KEY(kSkin,20): CfgSkinClickSound(a.mConfig); break;
    case EE_CFG_KEY(kSkin,21): CfgComp(a,EeWonCfgComp::SkinCursor); break;
    case EE_CFG_KEY(kFont,0): EE_CFG(FontConfig)->SetFont(a.f[0].get()); break;
    case EE_CFG_KEY(kFont,1): EE_CFG(FontConfig)->mDescriptor.mSize=i[0]; break;
    case EE_CFG_KEY(kFont,2): CfgComp(a,EeWonCfgComp::FontStyle); break;
    case EE_CFG_KEY(kFont,3): WindowManager::GetDefaultWindowManager()->SetDefaultFont(a.f[0].get()); break;
    case EE_CFG_KEY(kFont,4): WindowManager::GetDefaultWindowManager()->SetNamedFont(s[0],a.f[0].get()); break;
    case EE_CFG_KEY(kFont,5): EE_CFG(FontConfig)->mNeedRecalcFont=true; break;
    case EE_CFG_KEY(kImage,0): EE_CFG(ImageConfig)->mImage=a.m[0]; break;
    case EE_CFG_KEY(kImage,1): EE_CFG(ImageConfig)->mImage->SetSize(i[0],i[1]); break;
    default: break;
    // ContainerConfig: the components and layouts read so far taken as each is used (c[--nc], i[--ni]); the
    // layout made by the instruction is the frame's until the container holds it
    case EE_CFG_KEY(kContainer,0): EE_CFG(ContainerConfig)->mContainer->AddChild(c[--a.nc]); break;
    case EE_CFG_KEY(kContainer,1): EE_CFG(ContainerConfig)->mContainer->AddLayoutPoint(static_cast<LayoutPoint*>(c[--a.nc])); break;
    case EE_CFG_KEY(kContainer,2): CfgComp(a,EeWonCfgComp::NewLayoutPoint); break; // NEWLAYOUTPOINT's name
    case EE_CFG_KEY(kContainer,4): EE_CFG(ContainerConfig)->mCurLayoutId=i[0]; break;
    case EE_CFG_KEY(kContainer,5): CfgComp(a,EeWonCfgComp::EnableLayout); break;
    case EE_CFG_KEY(kContainer,6): EE_CFG(ContainerConfig)->mContainer->Relayout(); break;
    case EE_CFG_KEY(kContainer,9): a.lay=new SameSizeLayout(true,true); break;
    case EE_CFG_KEY(kContainer,10): a.lay=new SameSizeLayout(true,false); break;
    case EE_CFG_KEY(kContainer,11): a.lay=new SameSizeLayout(false,true); break;
    case EE_CFG_KEY(kContainer,12): a.lay=new CenterLayout(c[--a.nc],true,true); break;
    case EE_CFG_KEY(kContainer,13): a.lay=new CenterLayout(c[--a.nc],true,false); break;
    case EE_CFG_KEY(kContainer,14): a.lay=new CenterLayout(c[--a.nc],false,true); break;
    case EE_CFG_KEY(kContainer,15): CfgComp(a,EeWonCfgComp::SpaceEvenlyH); break;
    case EE_CFG_KEY(kContainer,16): CfgComp(a,EeWonCfgComp::SpaceEvenlyV); break;
    case EE_CFG_KEY(kContainer,17): CfgComp(a,EeWonCfgComp::ClearRight); break;
    case EE_CFG_KEY(kContainer,19): CfgComp(a,EeWonCfgComp::SizeAtLeastLayout); break;
    case EE_CFG_KEY(kContainer,20): a.lay->AddComponent(c[--a.nc]); break;
    case EE_CFG_KEY(kContainer,21): EE_CFG(ContainerConfig)->mContainer->AddChildLayout(a.lay.get(),EE_CFG(ContainerConfig)->mCurLayoutId); break;
    case EE_CFG_KEY(kContainer,22): EE_CFG(ContainerConfig)->mContainer->AddChildLayout(a.lay.get()); break;
    case EE_CFG_KEY(kContainer,23): EE_CFG(ContainerConfig)->mContainer->SetIsolateTabFocus(i[0]!=0); break;
    case EE_CFG_KEY(kContainer,24): EE_CFG(ContainerConfig)->mContainer->SetOnlyChildInput(i[0]!=0); break;
    // the LAYOUT family's state (cfgtab.rs layout): the previous rule's instructions and offsets, its reference, the
    // offsets read, the rule added (the frame's last six ints: instructions, offsets, which component stays the reference)
    case EE_CFG_KEY(kContainer,25): CfgComp(a,EeWonCfgComp::LayoutPrev); break;
    case EE_CFG_KEY(kContainer,26): CfgComp(a,EeWonCfgComp::LayoutChain); break;
    case EE_CFG_KEY(kContainer,27): CfgComp(a,EeWonCfgComp::LayoutOffsets); break;
    case EE_CFG_KEY(kContainer,28): CfgComp(a,EeWonCfgComp::LayoutRule); break;
    case EE_CFG_KEY(kContainer,29): CfgComp(a,EeWonCfgComp::NoOverlap); break;
    // StretchImageConfig (round 41; cfgimg.rs): the exports' children by number or column
    case EE_CFG_KEY(kStretchImage,0): EE_CFG(StretchImageConfig)->mStretchImage->SetImage(a.raw.get()); break;
    case EE_CFG_KEY(kStretchImage,1): EE_CFG(StretchImageConfig)->mStretchImage->SetTile(i[a.ni-2]!=0,i[a.ni-1]!=0); break;
    case EE_CFG_KEY(kStretchImage,2): EE_CFG(StretchImageConfig)->mStretchImage->SetBorders(i[0],i[1],i[2],i[3]); break;
    case EE_CFG_KEY(kStretchImage,3): EE_CFG(StretchImageConfig)->mStretchImage->SetSkipCenter(i[0]!=0); break;
    case EE_CFG_KEY(kStretchImage,4): CfgComp(a,EeWonCfgComp::StretchExport); break;
    case EE_CFG_KEY(kStretchImage,5): a.i[a.ni++]=a.raw->GetWidth()/i[0]; break;
    case EE_CFG_KEY(kStretchImage,6): CfgComp(a,EeWonCfgComp::StretchCut); break;
    case EE_CFG_KEY(kStretchImage,7): CfgComp(a,EeWonCfgComp::StretchNamed); break;
    case EE_CFG_KEY(kStretchImage,8): CfgComp(a,EeWonCfgComp::StretchNewChild); break;
    // ImageFilterConfig (round 41; cfgimg.rs): the raw image, its copy, the filters by name, the crops' and export's children
    case EE_CFG_KEY(kImageFilter,0): EE_CFG(ImageFilterConfig)->SetRawImage(a.raw.get()); break;
    case EE_CFG_KEY(kImageFilter,2): EE_CFG(ImageFilterConfig)->SetRawImage(static_cast<RawImage*>(EE_CFG(ImageFilterConfig)->mRawImage->Duplicate().get())); break;
    case EE_CFG_KEY(kImageFilter,3): a.i[a.ni++]=EE_CFG(ImageFilterConfig)->mRawImage->GetWidth()/i[0]; break;
    case EE_CFG_KEY(kImageFilter,4): CfgComp(a,EeWonCfgComp::FilterCut); break;
    case EE_CFG_KEY(kImageFilter,5): CfgComp(a,EeWonCfgComp::FilterNamed); break;
    case EE_CFG_KEY(kImageFilter,10): a.raw=ImageScalePercent(EE_CFG(ImageFilterConfig)->mRawImage.get(),i[a.ni-2],i[a.ni-1]); break;
    case EE_CFG_KEY(kImageFilter,11): a.raw=ImageScaleTo(EE_CFG(ImageFilterConfig)->mRawImage.get(),i[a.ni-2],i[a.ni-1]); break;
    case EE_CFG_KEY(kImageFilter,12): a.raw=ImageTint(EE_CFG(ImageFilterConfig)->mRawImage.get(),i[a.ni-3],i[a.ni-2],i[a.ni-1]); break;
    case EE_CFG_KEY(kImageFilter,13): a.raw=ImageCrop(EE_CFG(ImageFilterConfig)->mRawImage.get(),i[a.ni-4],i[a.ni-3],i[a.ni-2],i[a.ni-1]); break;
    case EE_CFG_KEY(kImageFilter,14): a.raw=ImageTransparent(EE_CFG(ImageFilterConfig)->mRawImage.get(),(DWORD)i[a.ni-1]); break;
    case EE_CFG_KEY(kImageFilter,15): a.raw=NULL; break;
    }
}
#undef EE_CFG_KEY
#undef EE_CFG
#undef EE_C
static bool CfgTabApply(void* p,int32_t inSetter)
{
    CfgTabFrame& a=*static_cast<CfgTabFrame*>(p);
    try { CfgTabSet(a,inSetter); return true; } catch(...) { a.mError=std::current_exception(); return false; }
}
// The handler of inTable for inKey: true when the table holds the key (and the key was handled), false to ask the
// base; a read's or setter's exception is rethrown here.
bool ComponentConfig::RunTable(int inTable,const std::string& inKey)
{
    CfgTabFrame aFrame; aFrame.mConfig=this; aFrame.ni=aFrame.ns=aFrame.nf=aFrame.nm=aFrame.nc=aFrame.nrs=0;
    EeWonCfgTabHost aHost={&aFrame,CfgTabRead,CfgTabComma,CfgTabEnd,CfgTabText,CfgTabPush,CfgTabApply,CfgTabFail,CfgTabClear,CfgTabPeek,CfgTabArg};
    int aResult=ee_won_cfg_tab_run(&aHost,inTable,(const uint8_t*)inKey.data(),inKey.size());
    if(aResult==1) std::rethrow_exception(aFrame.mError);
    return aResult==0;
}
// A config's instructions (R6 WONLobby round 26): its kind's tables and its bases' in the original's order
// (ee_won_gui cfgkind.rs), walked by cfgcomp.rs (R6 WON round 34) over its three ops; a step's exception is kept and
// rethrown here.
static std::exception_ptr sCfgKeyError;
template<class F> static bool CfgKeyGuard(bool* ok,F f)
{
    try { return f(); } catch(...) { sCfgKeyError=std::current_exception(); *ok=false; return false; }
}
static bool CfgKeyHost(void* c,EeWonCfgKeyOp op,int32_t t,const uint8_t* k,uintptr_t n,bool* ok)
{
    ComponentConfig* aConfig=(ComponentConfig*)c;
    switch(op) {
    case EeWonCfgKeyOp::RunTable: return CfgKeyGuard(ok,[&] { return aConfig->RunTable(t,std::string((const char*)k,n)); });
    case EeWonCfgKeyOp::HandleModify: return CfgKeyGuard(ok,[&] { return aConfig->HandleModify(); });
    default: return CfgKeyGuard(ok,[&] { return aConfig->HandleMapComponent(); });
    }
}
static bool CfgKeyRethrow(int r) { if(r==2) std::rethrow_exception(std::exchange(sCfgKeyError,nullptr)); return r==1; }
bool ComponentConfig::Virtual24(const std::string& key) { return CfgKeyRethrow(ee_won_cfg_v24(CfgKeyHost,this,this->mKind,(const uint8_t*)key.data(),key.size())); }
