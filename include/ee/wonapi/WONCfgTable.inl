#include "rust/won_gui/cfgcomp.h"
#include "rust/won_gui/cfgkind.h"
#include "rust/won_gui/cfgset.h"
#include "rust/won_gui/cfgtab.h"
#include "rust/won_gui/guiconf.h"
#include "rust/won_gui/shell.h"
#include "rust/won_gui/skinobj.h"
// The table reader's host (R6 WON round 39): the reads, the frame and the setters of the 49 table-shaped config
// handlers whose programs are ee_won's cfgtab.rs. Included inside a namespace by ComponentConfigHost.cpp (WONAPI) and
// by tests/won/won_cfgtab_test.cpp (over its stand-ins), so both run these very statements. A read or setter that
// throws is caught and kept; ComponentConfig::RunTable rethrows it after the Rust returns. Since R6 WON lobby and
// network round 39 the value reads, commas and ends are made in Rust on the config's own object (compcfg.rs): a
// host op's exception there is kept in mCc.
struct CfgTabFrame {
    ComponentConfig* mConfig; std::exception_ptr mError; CcFrame mCc;
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
    case O::SkinSet: WONSkinSet((EeWonSkinSlot)x,(int)y,(Component*)z); return 0;
    case O::SkinSetType: ee_won_skin_set_type(WONSkinGlobal(),x!=0,&a.s[0],(Component*)y); return 0;
    case O::RadioFlag: ((WONButton*)x)->SetButtonFlags(8,true); return 0;
    case O::SafePart: return CFG_G(SafeConfigGetSkinPart(a.c[0],(const char*)x));
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
    case O::StretchNew: return (intptr_t)new StretchImageConfig(EeWonCfgKind::StretchImage);
    case O::StretchCopy: ((StretchImageConfig*)x)->mStretchImage->CopyAttributes(EE_CFG(StretchImageConfig)->mStretchImage); return 0;
    case O::StretchHold: return (intptr_t)new SmartPtr<StretchImageConfig>((StretchImageConfig*)x);
    case O::StretchFree: delete (SmartPtr<StretchImageConfig>*)x; return 0;
    case O::StretchSetImage: ((StretchImageConfig*)x)->mStretchImage->SetImage((RawImage*)y); return 0;
    case O::StretchToParent: return CFG_GV(static_cast<ComponentConfig*>(a.mConfig->GetParentObject())->AddChildToConfigMap(GUIString(std::string((const char*)x,(size_t)y)),(StretchImageConfig*)z));
    case O::StretchToParentS0: return CFG_GV(static_cast<ComponentConfig*>(a.mConfig->GetParentObject())->AddChildToConfigMap(a.s[0],(StretchImageConfig*)z));
    case O::FrameChild: return (intptr_t)a.child.get();
    case O::FrameRaw: return (intptr_t)a.raw.get();
    case O::SetFrameChild: a.child=(StretchImageConfig*)x; return 0;
    case O::FilterNew: return (intptr_t)new SmartPtr<ImageFilterConfig>(new ImageFilterConfig(EeWonCfgKind::ImageFilter));
    case O::FilterSetRaw: (*(SmartPtr<ImageFilterConfig>*)x)->SetRawImage((RawImage*)y); return 0;
    case O::FilterToParent: return CFG_GV(static_cast<ComponentConfig*>(a.mConfig->GetParentObject())->AddChildToConfigMap(GUIString(std::string((const char*)x,(size_t)y)),((SmartPtr<ImageFilterConfig>*)z)->get()));
    case O::FilterToParentS0: return CFG_GV(static_cast<ComponentConfig*>(a.mConfig->GetParentObject())->AddChildToConfigMap(a.s[0],((SmartPtr<ImageFilterConfig>*)z)->get()));
    case O::FilterFree: delete (SmartPtr<ImageFilterConfig>*)x; return 0;
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
    case O::LaySpaceEvenly: a.lay=new ChildLayoutInstructionBase(EeWonLayoutKind::SpaceEvenly,a.c[--a.nc],NULL,a.i[0],a.i[1],0,x!=0); return 0;
    case O::LayClearRight: a.lay=new ChildLayoutInstructionBase(EeWonLayoutKind::ClearRight,a.c[--a.nc],NULL,a.i[0]); return 0;
    case O::LaySizeAtLeast: a.lay=new ChildLayoutInstructionBase(EeWonLayoutKind::SizeAtLeast,a.c[--a.nc],NULL,a.i[0],a.i[1]); return 0;
    case O::LayNoOverlap: a.lay=new ChildLayoutInstructionBase(EeWonLayoutKind::NoOverlap,a.c[1],a.c[0],a.i[2],a.i[0],a.i[1]); return 0;
    case O::SetCompCount: a.nc=(int)x; return 0;
    default: return 0;
    }
}
#undef CFG_G
#undef CFG_GV
static void CfgTabClear(void* p)
{
    CfgTabFrame& a=*static_cast<CfgTabFrame*>(p);
    a.ni=a.ns=a.nf=a.nm=a.nc=a.nrs=0; a.f[0]=NULL; a.m[0]=NULL; a.lay=NULL; a.raw=NULL;
}
// a host call's statements over the frame: false with the exception kept in the frame
template<class F> static bool CfgTabTry(void* p, F f)
{
    CfgTabFrame& a=*static_cast<CfgTabFrame*>(p);
    try { return f(a); } catch(...) { a.mError=std::current_exception(); return false; }
}
static bool CfgTabRead(void* p,EeWonCfgRead inKind)
{
    return CfgTabTry(p,[inKind](CfgTabFrame& a) {
        ComponentConfig* cfg=a.mConfig;
        switch(inKind) {
        case EeWonCfgRead::Font: a.f[a.nf++]=cfg->ReadFont(); break;
        case EeWonCfgRead::Image: a.m[a.nm++]=cfg->ReadImage(false); break;
        case EeWonCfgRead::Sound: a.snd=cfg->ReadSound(); break;
        case EeWonCfgRead::Background: a.bg=cfg->ReadBackground(); break;
        case EeWonCfgRead::Comp: a.c[a.nc++]=cfg->ReadComponent(); break;
        case EeWonCfgRead::Container: a.c[a.nc++]=SafeConfigGetComponent<Container*>(cfg->ReadComponent(),"Container"); break;
        case EeWonCfgRead::Button: a.c[a.nc++]=SafeConfigGetComponent<Button*>(cfg->ReadComponent(),"Button"); break;
        case EeWonCfgRead::Scroller: a.c[a.nc++]=SafeConfigGetComponent<Scroller*>(cfg->ReadComponent(),"Scroller"); break;
        case EeWonCfgRead::MultiListArea: a.c[a.nc++]=SafeConfigGetComponent<MultiListArea*>(cfg->ReadComponent(),"MultiListArea"); break;
        case EeWonCfgRead::ScrollArea: return ee_won_cfg_comp(CfgTabOp,&a,EeWonCfgComp::ReadScrollArea);
        case EeWonCfgRead::Frame: return ee_won_cfg_comp(CfgTabOp,&a,EeWonCfgComp::ReadFrame);
        case EeWonCfgRead::LayoutPoint: a.c[a.nc++]=SafeConfigGetComponent<LayoutPoint*>(cfg->ReadComponent(),"LayoutPoint"); break;
        case EeWonCfgRead::RawStr: a.rs[a.nrs++]=cfg->ReadRawString(false); break;
        default: break; // the reads made in Rust (CfgTabPut)
        }
        return true;
    });
}
// where a read made in Rust goes: a string slot's or the selection colour's address; the raw image's reference adopted,
// the standard cursor's object
static intptr_t CfgTabPut(void* p,EeWonCfgRead inKind,intptr_t v)
{
    CfgTabFrame& a=*static_cast<CfgTabFrame*>(p);
    switch(inKind) {
    case EeWonCfgRead::Word: a.ns=1; return (intptr_t)&a.s[0];
    case EeWonCfgRead::SelColor: return (intptr_t)&a.sel;
    case EeWonCfgRead::RawImage: a.raw=(RawImage*)CcAdopt((Image*)v).get(); return 0;
    case EeWonCfgRead::Cursor: a.cur=Cursor::GetStandardCursor((StandardCursor)v); return 0;
    default: return (intptr_t)&a.s[a.ns++]; // Str, Flag
    }
}
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
#define EE_CFG_KEY(t,n) ((EE_WON_CFG_T_##t)<<8|(n)) // a setter's number: its table's key (cfgtab.rs EE_WON_CFG_T_*) and its place
// The setters, as each handler called them on its typed component (bools read as 0/1). Those that called an inline
// forward to the component's Rust object are cfgset.rs's (R6 WONLobby round 27), those that ran a composition of
// cfgcomp.rs are its (ee_won_cfg_comp_setter, R6 WON lobby and network round 39): these are the rest.
static void CfgTabSet(CfgTabFrame& a,int inSetter)
{
    const int* i=a.i; const GUIString* s=a.s; Component* const* c=a.c;
    switch(inSetter) {
    case EE_CFG_KEY(BUTTON,1): EE_C(Button)->SetText(s[0],true); break;
    case EE_CFG_KEY(BUTTON,3): EE_C(Button)->SetClickSound(a.snd.get()); break;
    case EE_CFG_KEY(BUTTON,4): a.mConfig->HandleFitText(); break;
    case EE_CFG_KEY(MSBUTTON,0): EE_C(MSButtonBase)->SetFont(a.f[0]); break;
    case EE_CFG_KEY(MSBUTTON,6): EE_C(MSButtonBase)->SetWrap(i[0]!=0); break;
    case EE_CFG_KEY(PERCENT,0): EE_CFG(PercentLayoutConfig)->refs[i[0]]=c[0]; break;
    case EE_CFG_KEY(PERCENT,1): EE_CFG(PercentLayoutConfig)->params[i[0]]=i[1]; break;
    case EE_CFG_KEY(SCHEME,0): EE_CFG(ColorSchemeConfig)->mColorScheme.get()->SetStandardColor(i[0],(DWORD)i[1]); break;
    case EE_CFG_KEY(RECT,0): EE_C(RectangleComponent)->SetSolidColor(i[0]); break;
    case EE_CFG_KEY(RECT,1): EE_C(RectangleComponent)->SetOutlineColor(i[0]); break;
    case EE_CFG_KEY(RECT3D,0): EE_C(RectangleComponent3D)->SetSolidColor(i[0]); break;
    case EE_CFG_KEY(RECT3D,1): EE_C(RectangleComponent3D)->SetLightColor(i[0]); break;
    case EE_CFG_KEY(RECT3D,2): EE_C(RectangleComponent3D)->SetDarkColor(i[0]); break;
    case EE_CFG_KEY(RECT3D,3): EE_C(RectangleComponent3D)->SetIsUp(i[0]!=0); break;
    case EE_CFG_KEY(RECT3D,4): EE_C(RectangleComponent3D)->SetThickness(i[0]); break;
    case EE_CFG_KEY(IMAGECOMP,0): EE_C(ImageComponent)->SetImage(a.m[0]); break;
    case EE_CFG_KEY(IMAGECOMP,1): EE_C(ImageComponent)->mTile=i[0]!=0; break;
    case EE_CFG_KEY(IMAGECOMP,2): EE_C(ImageComponent)->mStretch=i[0]!=0; break;
    case EE_CFG_KEY(SCREEN,0): EE_C(ScreenContainer)->AddScreen(c[0]); break;
    case EE_CFG_KEY(SCREEN,1): EE_C(ScreenContainer)->ShowScreen(c[0],true); break;
    case EE_CFG_KEY(MULTILIST,4): EE_C(MultiListArea)->SetSortColumnFunc(i[0],ListArea::NumberSortFunc); break;
    case EE_CFG_KEY(BROWSER,0): EE_C(BrowserComponent)->SetTransparent(true); break;
    case EE_CFG_KEY(BROWSER,1): EE_C(BrowserComponent)->SetScroller(static_cast<Scroller*>(c[0])); break;
    case EE_CFG_KEY(BROWSER,2): EE_C(BrowserComponent)->SetFontSize((HTMLFontSize)i[0]); break;
    case EE_CFG_KEY(COMBO,7): EE_C(ComboBox)->mEdit.get()->SetMaxChars(i[0]); break;
    case EE_CFG_KEY(DIALOG,4): EE_C(Dialog)->SetDefaultButton(static_cast<Button*>(c[0])); break;
    case EE_CFG_KEY(IMAGEBUTTON,0): EE_C(ImageButton)->SetImage((ImageButtonType)i[0],a.m[0].get()); break;
    case EE_CFG_KEY(LABEL,0): EE_C(Label)->SetText(s[0],true); break;
    case EE_CFG_KEY(LABEL,1): EE_C(Label)->SetFont(a.f[0]); break;
    case EE_CFG_KEY(LABEL,3): EE_C(Label)->SetBackground(a.bg); break;
    case EE_CFG_KEY(LISTAREA,1): EE_C(ListArea)->InsertString(s[0],-1); break;
    case EE_CFG_KEY(LISTAREA,2): EE_C(ListArea)->SetFont(a.f[0]); break;
    case EE_CFG_KEY(LISTAREA,3): EE_C(ListArea)->mTextColor=i[0]; break;
    case EE_CFG_KEY(LISTAREA,4): EE_C(ListArea)->mSelColor=a.sel; break;
    case EE_CFG_KEY(INPUT,5): EE_C(InputBox)->SetBackground(a.bg); break;
    case EE_CFG_KEY(INPUT,6): EE_C(InputBox)->SetDisabledBackground(a.bg); break;
    case EE_CFG_KEY(INPUT,7): EE_C(InputBox)->SetIcon(a.m[0].get()); break;
    case EE_CFG_KEY(INPUT,8): EE_C(InputBox)->mTextColor=i[0]; break;
    case EE_CFG_KEY(INPUT,9): EE_C(InputBox)->mDisabledTextColor=i[0]; break;
    case EE_CFG_KEY(INPUT,10): EE_C(InputBox)->mSelColor=a.sel; break;
    case EE_CFG_KEY(INPUT,13): EE_C(InputBox)->mUseDisabledColors=i[0]!=0; break;
    case EE_CFG_KEY(TEXTAREA,1): EE_C(TextArea)->AddSegment(s[0],false); break;
    case EE_CFG_KEY(TEXTAREA,2): EE_C(TextArea)->AddFormatedText(s[0],false); break;
    case EE_CFG_KEY(TEXTAREA,3): EE_C(TextArea)->SetFont(a.f[0]); break;
    case EE_CFG_KEY(TEXTAREA,4): EE_C(TextArea)->mTextColor=i[0]; break;
    case EE_CFG_KEY(TEXTAREA,5): EE_C(TextArea)->mSelColor=a.sel; break;
    case EE_CFG_KEY(TEXTAREA,6): EE_C(TextArea)->SetLinkFont(a.f[0]); break;
    case EE_CFG_KEY(WLISTCTRL,1): static_cast<MultiListArea*>(EE_C(ScrollbarScroller)->GetScrollArea())->SetNumColumns(i[0]); break;
    case EE_CFG_KEY(WLISTCTRL,2): static_cast<MultiListArea*>(EE_C(ScrollbarScroller)->GetScrollArea())->SetDynamicSizeColumn(i[0]); break;
    case EE_CFG_KEY(WLISTCTRL,3): static_cast<ListArea*>(static_cast<MultiListArea*>(EE_C(ScrollbarScroller)->GetScrollArea()))->SetListFlags((unsigned long)i[1],i[0]!=0); break;
    case EE_CFG_KEY(WCOMBO,2): WONShellFrameOp(EE_C(Component),EeWonShellOp::SetFrame,a.fr); break;
    case EE_CFG_KEY(WTABCTRL,2): WONShellFrameOp(EE_C(Component),EeWonShellOp::SetFrame,a.fr); break;
    case EE_CFG_KEY(WSCROLLCONT,2): WONShellFrameOp(EE_C(ScrollbarScroller),EeWonShellOp::SetFrame,a.fr); break;
    case EE_CFG_KEY(SCROLLAREA,0): EE_C(ScrollArea)->SetBackground(a.bg); break;
    case EE_CFG_KEY(STRETCHCENTER,0): EE_CFG(StretchCenterImageConfig)->mStretchCenterImage->SetStretchImage(a.m[0].get()); break;
    case EE_CFG_KEY(STRETCHCENTER,1): EE_CFG(StretchCenterImageConfig)->mStretchCenterImage->SetCenterImage(a.m[0].get()); break;
    case EE_CFG_KEY(COMPONENT,2): a.mConfig->mComponent.get()->mControlId=i[0]; break;
    case EE_CFG_KEY(COMPONENT,9): a.mConfig->mComponent->SetTranslucentColor(i[0],a.ni>1?i[1]:4); break;
    case EE_CFG_KEY(COMPONENT,11): a.mConfig->mComponent->SetDesiredSize(i[0],i[1]); break;
    case EE_CFG_KEY(COMPONENT,12): a.mConfig->mComponent.get()->SetDesiredWidth(i[0]); break;
    case EE_CFG_KEY(COMPONENT,13): a.mConfig->mComponent.get()->SetDesiredHeight(i[0]); break;
    case EE_CFG_KEY(COMPONENT,14): a.mConfig->mComponent->SetPosSize(i[0],i[1],i[2],i[3]); break;
    case EE_CFG_KEY(COMPONENT,15): a.mConfig->mComponent->SetEnabled(i[0]!=0); break;
    case EE_CFG_KEY(COMPONENT,16): a.mConfig->mComponent->SetVisible(i[0]!=0); break;
    case EE_CFG_KEY(COMPONENT,18): a.mConfig->mComponent->SetDefaultCursor(a.cur); break;
    case EE_CFG_KEY(COMPONENT,19): a.mConfig->mComponent->SetComponentFlags(i[1],i[0]!=0); break;
    case EE_CFG_KEY(COMPONENT,20): Window::GetDefaultWindow()->SetSkipDecodeImages(!(i[0]!=0)); break;
    case EE_CFG_KEY(COMPONENT,21): gShowParseTime=i[0]!=0; break;
    case EE_CFG_KEY(COMPONENT,33): a.mConfig->SetDesiredSizeIfNecessary(); break;
    case EE_CFG_KEY(SKIN,20): CfgSkinClickSound(a.mConfig); break;
    case EE_CFG_KEY(FONT,0): EE_CFG(FontConfig)->SetFont(a.f[0].get()); break;
    case EE_CFG_KEY(FONT,1): EE_CFG(FontConfig)->mDescriptor.mSize=i[0]; break;
    case EE_CFG_KEY(FONT,3): WindowManager::GetDefaultWindowManager()->SetDefaultFont(a.f[0].get()); break;
    case EE_CFG_KEY(FONT,4): WindowManager::GetDefaultWindowManager()->SetNamedFont(s[0],a.f[0].get()); break;
    case EE_CFG_KEY(FONT,5): EE_CFG(FontConfig)->mNeedRecalcFont=true; break;
    case EE_CFG_KEY(IMAGE,0): EE_CFG(ImageConfig)->mImage=a.m[0]; break;
    case EE_CFG_KEY(IMAGE,1): EE_CFG(ImageConfig)->mImage->SetSize(i[0],i[1]); break;
    default: break;
    // ContainerConfig: the components and layouts read so far taken as each is used (c[--nc], i[--ni]); the
    // layout made by the instruction is the frame's until the container holds it
    case EE_CFG_KEY(CONTAINER,0): EE_CFG(ContainerConfig)->mContainer->AddChild(c[--a.nc]); break;
    case EE_CFG_KEY(CONTAINER,1): EE_CFG(ContainerConfig)->mContainer->AddLayoutPoint(static_cast<LayoutPoint*>(c[--a.nc])); break;
    case EE_CFG_KEY(CONTAINER,4): EE_CFG(ContainerConfig)->mCurLayoutId=i[0]; break;
    case EE_CFG_KEY(CONTAINER,6): EE_CFG(ContainerConfig)->mContainer->Relayout(); break;
    case EE_CFG_KEY(CONTAINER,9): a.lay=new ChildLayoutInstructionBase(EeWonLayoutKind::SameSize,NULL,NULL,0,0,0,true,true); break;
    case EE_CFG_KEY(CONTAINER,10): a.lay=new ChildLayoutInstructionBase(EeWonLayoutKind::SameSize,NULL,NULL,0,0,0,true,false); break;
    case EE_CFG_KEY(CONTAINER,11): a.lay=new ChildLayoutInstructionBase(EeWonLayoutKind::SameSize,NULL,NULL,0,0,0,false,true); break;
    case EE_CFG_KEY(CONTAINER,12): a.lay=new CenterLayout(c[--a.nc],true,true); break;
    case EE_CFG_KEY(CONTAINER,13): a.lay=new CenterLayout(c[--a.nc],true,false); break;
    case EE_CFG_KEY(CONTAINER,14): a.lay=new CenterLayout(c[--a.nc],false,true); break;
    case EE_CFG_KEY(CONTAINER,20): a.lay->AddComponent(c[--a.nc]); break;
    case EE_CFG_KEY(CONTAINER,21): EE_CFG(ContainerConfig)->mContainer->AddChildLayout(a.lay.get(),EE_CFG(ContainerConfig)->mCurLayoutId); break;
    case EE_CFG_KEY(CONTAINER,22): EE_CFG(ContainerConfig)->mContainer->AddChildLayout(a.lay.get()); break;
    case EE_CFG_KEY(CONTAINER,23): EE_CFG(ContainerConfig)->mContainer->SetIsolateTabFocus(i[0]!=0); break;
    case EE_CFG_KEY(CONTAINER,24): EE_CFG(ContainerConfig)->mContainer->SetOnlyChildInput(i[0]!=0); break;
    // StretchImageConfig (round 41; cfgimg.rs)
    case EE_CFG_KEY(STRETCHIMAGE,0): EE_CFG(StretchImageConfig)->mStretchImage->SetImage(a.raw.get()); break;
    case EE_CFG_KEY(STRETCHIMAGE,1): EE_CFG(StretchImageConfig)->mStretchImage->SetTile(i[a.ni-2]!=0,i[a.ni-1]!=0); break;
    case EE_CFG_KEY(STRETCHIMAGE,2): EE_CFG(StretchImageConfig)->mStretchImage->SetBorders(i[0],i[1],i[2],i[3]); break;
    case EE_CFG_KEY(STRETCHIMAGE,3): EE_CFG(StretchImageConfig)->mStretchImage->SetSkipCenter(i[0]!=0); break;
    case EE_CFG_KEY(STRETCHIMAGE,5): a.i[a.ni++]=a.raw->GetWidth()/i[0]; break;
    // ImageFilterConfig (round 41; cfgimg.rs): the raw image, its copy, the filters by name
    case EE_CFG_KEY(IMAGEFILTER,0): EE_CFG(ImageFilterConfig)->SetRawImage(a.raw.get()); break;
    case EE_CFG_KEY(IMAGEFILTER,2): EE_CFG(ImageFilterConfig)->SetRawImage(static_cast<RawImage*>(EE_CFG(ImageFilterConfig)->mRawImage->Duplicate().get())); break;
    case EE_CFG_KEY(IMAGEFILTER,3): a.i[a.ni++]=EE_CFG(ImageFilterConfig)->mRawImage->GetWidth()/i[0]; break;
    case EE_CFG_KEY(IMAGEFILTER,10): a.raw=ImageScalePercent(EE_CFG(ImageFilterConfig)->mRawImage.get(),i[a.ni-2],i[a.ni-1]); break;
    case EE_CFG_KEY(IMAGEFILTER,11): a.raw=ImageScaleTo(EE_CFG(ImageFilterConfig)->mRawImage.get(),i[a.ni-2],i[a.ni-1]); break;
    case EE_CFG_KEY(IMAGEFILTER,12): a.raw=ImageTint(EE_CFG(ImageFilterConfig)->mRawImage.get(),i[a.ni-3],i[a.ni-2],i[a.ni-1]); break;
    case EE_CFG_KEY(IMAGEFILTER,13): a.raw=ImageCrop(EE_CFG(ImageFilterConfig)->mRawImage.get(),i[a.ni-4],i[a.ni-3],i[a.ni-2],i[a.ni-1]); break;
    case EE_CFG_KEY(IMAGEFILTER,14): a.raw=ImageTransparent(EE_CFG(ImageFilterConfig)->mRawImage.get(),(DWORD)i[a.ni-1]); break;
    case EE_CFG_KEY(IMAGEFILTER,15): a.raw=NULL; break;
    }
}
#undef EE_CFG_KEY
#undef EE_CFG
#undef EE_C
// a setter: a composition of cfgcomp.rs over the frame (its op's exception kept in the frame), else CfgTabSet's case
static bool CfgTabApply(void* p,int32_t inSetter)
{
    int r=ee_won_cfg_comp_setter(CfgTabOp,p,inSetter);
    return r>=0 ? r!=0 : CfgTabTry(p,[inSetter](CfgTabFrame& a) { CfgTabSet(a,inSetter); return true; });
}
// The handler of inTable for inKey: true when the table holds the key (and the key was handled), false to ask the
// base; a read's or setter's exception is rethrown here.
bool ComponentConfig::RunTable(int inTable,const std::string& inKey)
{
    CfgTabFrame aFrame; aFrame.mConfig=this; aFrame.ni=aFrame.ns=aFrame.nf=aFrame.nm=aFrame.nc=aFrame.nrs=0;
    EeWonCfgTabHost aHost={&aFrame,CfgTabRead,NULL,NULL,CfgTabText,CfgTabPush,CfgTabApply,CfgTabFail,CfgTabClear,CfgTabPeek,CfgTabArg,this->mCfg,&aFrame.mCc,CfgTabPut};
    int aResult=ee_won_cfg_tab_run(&aHost,inTable,(const uint8_t*)inKey.data(),inKey.size());
    if(aResult==1) std::rethrow_exception(aFrame.mError ? aFrame.mError : aFrame.mCc.mErr);
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
