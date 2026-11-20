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
// WONFrameValueConfig's frame value: a stretch image alone (its own borders), or an image and one border or four
static WONFrame CfgReadFrame(ComponentConfig* config)
{
    SmartPtr<Image> image=config->ReadImage(false);
    StretchImage* stretch=dynamic_cast<StretchImage*>(image.get());
    if(stretch!=NULL) return stretch;
    config->EnsureComma();
    int left=config->ReadInt();
    if(config->EndOfString()) return WONFrame(image.get(),left);
    config->EnsureComma();
    int top=config->ReadInt(); config->EnsureComma();
    int right=config->ReadInt(); config->EnsureComma();
    int bottom=config->ReadInt();
    return WONFrame(image.get(),left,top,right,bottom);
}
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
        case EeWonCfgRead::Flag: { bool on=true; a.s[a.ns++]=cfg->ReadFlag(&on); a.i[a.ni++]=on; break; }
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
        case EeWonCfgRead::ScrollArea: {
            ScrollArea* area=dynamic_cast<ScrollArea*>(cfg->ReadComponent());
            if(area==NULL) throw ConfigObjectException("Expecting ScrollArea");
            a.c[a.nc++]=area; break;
        }
        case EeWonCfgRead::Frame: a.fr=CfgReadFrame(cfg); break;
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
static void CfgTabFail(void* p,const uint8_t* inMsg,uintptr_t inLen,int32_t inSlot)
{
    CfgTabFrame& a=*static_cast<CfgTabFrame*>(p);
    std::string aText((const char*)inMsg,inLen);
    if(inSlot>=0) aText+=(std::string)a.s[inSlot];
    a.mError=std::make_exception_ptr(ConfigObjectException(aText));
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
// a stretch image config's child: its stretch attributes (CfgStretchChild), then the image, under the name in the parent
static StretchImageConfig* CfgStretchChild(StretchImageConfig* k)
{
    StretchImageConfig* child=new StretchImageConfig; child->mStretchImage->CopyAttributes(k->mStretchImage); return child;
}
static void CfgStretchAdd(StretchImageConfig* k,const GUIString& name,StretchImageConfig* child,RawImage* image)
{
    SmartPtr<StretchImageConfig> keep=child; child->mStretchImage->SetImage(image);
    static_cast<ComponentConfig*>(k->GetParentObject())->AddChildToConfigMap(name,child);
}
// an image filter config's child: the image, under the name in the parent
static void CfgFilterChild(ImageFilterConfig* k,const GUIString& name,RawImage* image)
{
    SmartPtr<ImageFilterConfig> child=new ImageFilterConfig; child->SetRawImage(image);
    static_cast<ComponentConfig*>(k->GetParentObject())->AddChildToConfigMap(name,child.get());
}
#define EE_CFG_SET(t,n) case (t)<<8|(n):
#define EE_CFG(T) static_cast<T*>(a.mConfig)
#define EE_C(T) static_cast<T*>(a.mConfig->mComponent.get()) // the config kind's component (the original classes' typed copy)
// The setters, as each handler called them on its typed component (bools read as 0/1). Those that called an inline
// forward to the component's Rust object are cfgset.rs's (R6 WONLobby round 27): these are the rest.
static void CfgTabSet(CfgTabFrame& a,int inSetter)
{
    const int* i=a.i; const GUIString* s=a.s; Component* const* c=a.c;
    switch(inSetter) {
    EE_CFG_SET(kButton,1) EE_C(Button)->SetText(s[0],true); break;
    EE_CFG_SET(kButton,3) EE_C(Button)->SetClickSound(a.snd.get()); break;
    EE_CFG_SET(kButton,4) a.mConfig->HandleFitText(); break;
    EE_CFG_SET(kMSButton,0) EE_C(MSButtonBase)->SetFont(a.f[0]); break;
    EE_CFG_SET(kMSButton,6) EE_C(MSButtonBase)->SetWrap(i[0]!=0); break;
    EE_CFG_SET(kPercent,0) EE_CFG(PercentLayoutConfig)->refs[i[0]]=c[0]; break;
    EE_CFG_SET(kPercent,1) EE_CFG(PercentLayoutConfig)->params[i[0]]=i[1]; break;
    EE_CFG_SET(kScheme,0) EE_CFG(ColorSchemeConfig)->mColorScheme.get()->SetStandardColor(i[0],(DWORD)i[1]); break;
    EE_CFG_SET(kRect,0) EE_C(RectangleComponent)->SetSolidColor(i[0]); break;
    EE_CFG_SET(kRect,1) EE_C(RectangleComponent)->SetOutlineColor(i[0]); break;
    EE_CFG_SET(kRect3D,0) EE_C(RectangleComponent3D)->SetSolidColor(i[0]); break;
    EE_CFG_SET(kRect3D,1) EE_C(RectangleComponent3D)->SetLightColor(i[0]); break;
    EE_CFG_SET(kRect3D,2) EE_C(RectangleComponent3D)->SetDarkColor(i[0]); break;
    EE_CFG_SET(kRect3D,3) EE_C(RectangleComponent3D)->SetIsUp(i[0]!=0); break;
    EE_CFG_SET(kRect3D,4) EE_C(RectangleComponent3D)->SetThickness(i[0]); break;
    EE_CFG_SET(kImageComp,0) EE_C(ImageComponent)->SetImage(a.m[0]); break;
    EE_CFG_SET(kImageComp,1) EE_C(ImageComponent)->mTile=i[0]!=0; break;
    EE_CFG_SET(kImageComp,2) EE_C(ImageComponent)->mStretch=i[0]!=0; break;
    EE_CFG_SET(kScreen,0) EE_C(ScreenContainer)->AddScreen(c[0]); break;
    EE_CFG_SET(kScreen,1) EE_C(ScreenContainer)->ShowScreen(c[0],true); break;
    EE_CFG_SET(kMultiList,4) EE_C(MultiListArea)->SetSortColumnFunc(i[0],ListArea::NumberSortFunc); break;
    EE_CFG_SET(kBrowser,0) EE_C(BrowserComponent)->SetTransparent(true); break;
    EE_CFG_SET(kBrowser,1) EE_C(BrowserComponent)->SetScroller(static_cast<Scroller*>(c[0])); break;
    EE_CFG_SET(kBrowser,2) EE_C(BrowserComponent)->SetFontSize((HTMLFontSize)i[0]); break;
    EE_CFG_SET(kCombo,7) EE_C(ComboBox)->mEdit.get()->SetMaxChars(i[0]); break;
    EE_CFG_SET(kDialog,4) EE_C(Dialog)->SetDefaultButton(static_cast<Button*>(c[0])); break;
    EE_CFG_SET(kImageButton,0) EE_C(ImageButton)->SetImage((ImageButtonType)i[0],a.m[0].get()); break;
    EE_CFG_SET(kLabel,0) EE_C(Label)->SetText(s[0],true); break;
    EE_CFG_SET(kLabel,1) EE_C(Label)->SetFont(a.f[0]); break;
    EE_CFG_SET(kLabel,3) EE_C(Label)->SetBackground(a.bg); break;
    EE_CFG_SET(kListArea,1) EE_C(ListArea)->InsertString(s[0],-1); break;
    EE_CFG_SET(kListArea,2) EE_C(ListArea)->SetFont(a.f[0]); break;
    EE_CFG_SET(kListArea,3) EE_C(ListArea)->mTextColor=i[0]; break;
    EE_CFG_SET(kListArea,4) EE_C(ListArea)->mSelColor=a.sel; break;
    EE_CFG_SET(kInput,5) EE_C(InputBox)->SetBackground(a.bg); break;
    EE_CFG_SET(kInput,6) EE_C(InputBox)->SetDisabledBackground(a.bg); break;
    EE_CFG_SET(kInput,7) EE_C(InputBox)->SetIcon(a.m[0].get()); break;
    EE_CFG_SET(kInput,8) EE_C(InputBox)->mTextColor=i[0]; break;
    EE_CFG_SET(kInput,9) EE_C(InputBox)->mDisabledTextColor=i[0]; break;
    EE_CFG_SET(kInput,10) EE_C(InputBox)->mSelColor=a.sel; break;
    EE_CFG_SET(kInput,13) EE_C(InputBox)->mUseDisabledColors=i[0]!=0; break;
    EE_CFG_SET(kTextArea,1) EE_C(TextArea)->AddSegment(s[0],false); break;
    EE_CFG_SET(kTextArea,2) EE_C(TextArea)->AddFormatedText(s[0],false); break;
    EE_CFG_SET(kTextArea,3) EE_C(TextArea)->SetFont(a.f[0]); break;
    EE_CFG_SET(kTextArea,4) EE_C(TextArea)->mTextColor=i[0]; break;
    EE_CFG_SET(kTextArea,5) EE_C(TextArea)->mSelColor=a.sel; break;
    EE_CFG_SET(kTextArea,6) EE_C(TextArea)->SetLinkFont(a.f[0]); break;
    EE_CFG_SET(kSimple,1)
        if(!ee_won_skin_copy_type(WONSkinGlobal(),true,&s[0],EE_C(Component)))
            throw ConfigObjectException("Type not found: " + std::string(s[0]));
        break;
    EE_CFG_SET(kWONButton,1)
        if(!ee_won_skin_copy_type(WONSkinGlobal(),false,&s[0],static_cast<Component*>(EE_C(WONButton))))
            throw ConfigObjectException("Type not found: " + std::string(s[0]));
        break;
    EE_CFG_SET(kWONButton,12) { WONButton* b=EE_C(WONButton); b->SetHorzTextPad(i[0],a.ni>1?i[1]:b->GetRightTextPad()); break; }
    EE_CFG_SET(kWScrollbar,0) if(i[0]!=0) { Button* rect=new Button; EE_C(Scrollbar)->SetScrollRect(rect); } break;
    EE_CFG_SET(kWListCtrl,1) static_cast<MultiListArea*>(EE_C(ScrollbarScroller)->GetScrollArea())->SetNumColumns(i[0]); break;
    EE_CFG_SET(kWListCtrl,2) static_cast<MultiListArea*>(EE_C(ScrollbarScroller)->GetScrollArea())->SetDynamicSizeColumn(i[0]); break;
    EE_CFG_SET(kWListCtrl,3) static_cast<ListArea*>(static_cast<MultiListArea*>(EE_C(ScrollbarScroller)->GetScrollArea()))->SetListFlags((unsigned long)i[1],i[0]!=0); break;
    EE_CFG_SET(kWCombo,2) WONShellFrameOp(EE_C(Component),EeWonShellOp::SetFrame,a.fr); break;
    EE_CFG_SET(kWTabCtrl,2) WONShellFrameOp(EE_C(Component),EeWonShellOp::SetFrame,a.fr); break;
    EE_CFG_SET(kWScrollCont,0) if(i[0]==0) WONShellFrameOp(EE_C(ScrollbarScroller),EeWonShellOp::SetFrame,WONFrame()); break;
    EE_CFG_SET(kWScrollCont,2) WONShellFrameOp(EE_C(ScrollbarScroller),EeWonShellOp::SetFrame,a.fr); break;
    EE_CFG_SET(kScrollArea,0) EE_C(ScrollArea)->SetBackground(a.bg); break;
    EE_CFG_SET(kStretchCenter,0) EE_CFG(StretchCenterImageConfig)->mStretchCenterImage->SetStretchImage(a.m[0].get()); break;
    EE_CFG_SET(kStretchCenter,1) EE_CFG(StretchCenterImageConfig)->mStretchCenterImage->SetCenterImage(a.m[0].get()); break;
    EE_CFG_SET(kComponent,2) a.mConfig->mComponent.get()->mControlId=i[0]; break;
    EE_CFG_SET(kComponent,3) { Component* p=a.mConfig->mComponent.get(); p->SetPos(i[0],p->Top()); break; }
    EE_CFG_SET(kComponent,4) { Component* p=a.mConfig->mComponent.get(); p->SetPos(p->Left(),i[0]); break; }
    EE_CFG_SET(kComponent,5) { Component* p=a.mConfig->mComponent.get(); p->SetSize(i[0],p->Height()); break; }
    EE_CFG_SET(kComponent,6) { Component* p=a.mConfig->mComponent.get(); p->SetSize(p->Width(),i[0]); break; }
    EE_CFG_SET(kComponent,7) a.mConfig->mComponent->SetPos(i[0],i[1]); break;
    EE_CFG_SET(kComponent,8) a.mConfig->mComponent->SetSize(i[0],i[1]); break;
    EE_CFG_SET(kComponent,9) a.mConfig->mComponent->SetTranslucentColor(i[0],a.ni>1?i[1]:4); break;
    EE_CFG_SET(kComponent,10)
        a.mConfig->mHasSetWidth=i[0]>0; a.mConfig->mHasSetHeight=i[1]>0;
        a.mConfig->mComponent->SetSizeAtLeast(i[0],i[1]); break;
    EE_CFG_SET(kComponent,11) a.mConfig->mComponent->SetDesiredSize(i[0],i[1]); break;
    EE_CFG_SET(kComponent,12) a.mConfig->mComponent.get()->SetDesiredWidth(i[0]); break;
    EE_CFG_SET(kComponent,13) a.mConfig->mComponent.get()->SetDesiredHeight(i[0]); break;
    EE_CFG_SET(kComponent,14) a.mConfig->mComponent->SetPosSize(i[0],i[1],i[2],i[3]); break;
    EE_CFG_SET(kComponent,15) a.mConfig->mComponent->SetEnabled(i[0]!=0); break;
    EE_CFG_SET(kComponent,16) a.mConfig->mComponent->SetVisible(i[0]!=0); break;
    EE_CFG_SET(kComponent,17) if(i[0]!=0) a.mConfig->mComponent->RequestFocus(); break;
    EE_CFG_SET(kComponent,18) a.mConfig->mComponent->SetDefaultCursor(a.cur); break;
    EE_CFG_SET(kComponent,19) a.mConfig->mComponent->SetComponentFlags(i[1],i[0]!=0); break;
    EE_CFG_SET(kComponent,20) Window::GetDefaultWindow()->SetSkipDecodeImages(!(i[0]!=0)); break;
    EE_CFG_SET(kComponent,21) gShowParseTime=i[0]!=0; break;
    EE_CFG_SET(kComponent,22) ConfigParser::ResetLineCount(); PerfTimer::GlobalStart(NULL); break;
    EE_CFG_SET(kComponent,23) PerfTimer::GlobalStop(NULL); gLinesParsed=ConfigParser::GetLineCount(); break;
    EE_CFG_SET(kComponent,30) a.mConfig->mHasSetWidth=true; break;
    EE_CFG_SET(kComponent,31) a.mConfig->mHasSetHeight=true; break;
    EE_CFG_SET(kComponent,32) a.mConfig->mHasSetWidth=a.mConfig->mHasSetHeight=true; break;
    EE_CFG_SET(kComponent,33) a.mConfig->SetDesiredSizeIfNecessary(); break;
    EE_CFG_SET(kSkin,1) ee_won_skin_set_type(WONSkinGlobal(),false,&s[0],static_cast<Component*>(SafeConfigGetComponent<WONButton*>(c[0],"WONButton"))); break;
    EE_CFG_SET(kSkin,2) ee_won_skin_set_type(WONSkinGlobal(),true,&s[0],SafeConfigGetSkinPart(c[0],"WONSimpleButton")); break;
    EE_CFG_SET(kSkin,3) WONSkinSet(EeWonSkinSlot::PushButton,0,SafeConfigGetComponent<WONButton*>(c[0],"WONButton")); break;
    EE_CFG_SET(kSkin,4) WONSkinSet(EeWonSkinSlot::Checkbox,0,SafeConfigGetComponent<WONButton*>(c[0],"WONButton")); break;
    EE_CFG_SET(kSkin,5) {
        WONButton* aRadio=SafeConfigGetComponent<WONButton*>(c[0],"WONButton");
        WONSkinSet(EeWonSkinSlot::RadioButton,0,aRadio);
        aRadio->SetButtonFlags(8,true); break;
    }
    EE_CFG_SET(kSkin,6) WONSkinSet(EeWonSkinSlot::HeaderButton,0,SafeConfigGetSkinPart(c[0],"WONHeaderButton")); break;
    EE_CFG_SET(kSkin,7) WONSkinSet(EeWonSkinSlot::TabButton,0,SafeConfigGetComponent<WONButton*>(c[0],"WONButton")); break;
    EE_CFG_SET(kSkin,8) WONSkinSet(EeWonSkinSlot::Arrow0,i[0],SafeConfigGetSkinPart(c[0],"WONSimpleButton")); break;
    EE_CFG_SET(kSkin,9) WONSkinSet(EeWonSkinSlot::ComboButton,0,SafeConfigGetSkinPart(c[0],"WONSimpleButton")); break;
    EE_CFG_SET(kSkin,10) WONSkinSet(EeWonSkinSlot::CloseButton,0,SafeConfigGetSkinPart(c[0],"WONSimpleButton")); break;
    EE_CFG_SET(kSkin,11) WONSkinSet(EeWonSkinSlot::ScrollGutter0,i[0]?1:0,SafeConfigGetSkinPart(c[0],"WONSimpleButton")); break;
    EE_CFG_SET(kSkin,12) WONSkinSet(EeWonSkinSlot::ScrollThumb0,i[0]?1:0,SafeConfigGetSkinPart(c[0],"WONSimpleButton")); break;
    EE_CFG_SET(kSkin,13) WONSkinSet(EeWonSkinSlot::Scrollbar0,i[0]?1:0,SafeConfigGetSkinPart(c[0],"WONScrollbar")); break;
    EE_CFG_SET(kSkin,14) WONSkinSet(EeWonSkinSlot::ScrollContainer,0,SafeConfigGetSkinPart(c[0],"WONScrollContainer")); break;
    EE_CFG_SET(kSkin,15) WONSkinSet(EeWonSkinSlot::ListCtrl,0,SafeConfigGetSkinPart(c[0],"WONListCtrl")); break;
    EE_CFG_SET(kSkin,16) {
        Component* aTab=SafeConfigGetSkinPart(c[0],"WONTabCtrl");
        WONSkinSet(EeWonSkinSlot::TabCtrl,0,aTab);
        WONShell::Op(aTab,EeWonShellOp::SetSkin,0); break;
    }
    EE_CFG_SET(kSkin,17) WONSkinSet(EeWonSkinSlot::ComboBox,0,SafeConfigGetSkinPart(c[0],"WONComboBox")); break;
    EE_CFG_SET(kSkin,18) WONSkinSet(EeWonSkinSlot::PopupBack,0,SafeConfigGetSkinPart(c[0],"WONPopupBack")); break;
    EE_CFG_SET(kSkin,19) WONSkinSet(EeWonSkinSlot::DialogBack,0,SafeConfigGetSkinPart(c[0],"WONDialogBack")); break;
    EE_CFG_SET(kSkin,20) CfgSkinClickSound(a.mConfig); break;
    EE_CFG_SET(kSkin,21) {
        SmartPtr<Cursor> aCursor=new ComponentCursor(new ImageComponent(a.m[0].get()));
        Cursor::SetStandardCursor((StandardCursor)i[0],aCursor.get()); break;
    }
    EE_CFG_SET(kFont,0) EE_CFG(FontConfig)->SetFont(a.f[0].get()); break;
    EE_CFG_SET(kFont,1) EE_CFG(FontConfig)->mDescriptor.mSize=i[0]; break;
    EE_CFG_SET(kFont,2)
        if(i[0]!=0) EE_CFG(FontConfig)->mDescriptor.mStyle|=i[1];
        else EE_CFG(FontConfig)->mDescriptor.mStyle&=~i[1];
        break;
    EE_CFG_SET(kFont,3) WindowManager::GetDefaultWindowManager()->SetDefaultFont(a.f[0].get()); break;
    EE_CFG_SET(kFont,4) WindowManager::GetDefaultWindowManager()->SetNamedFont(s[0],a.f[0].get()); break;
    EE_CFG_SET(kFont,5) EE_CFG(FontConfig)->mNeedRecalcFont=true; break;
    EE_CFG_SET(kImage,0) EE_CFG(ImageConfig)->mImage=a.m[0]; break;
    EE_CFG_SET(kImage,1) EE_CFG(ImageConfig)->mImage->SetSize(i[0],i[1]); break;
    default: break;
    // ContainerConfig: the components and layouts read so far taken as each is used (c[--nc], i[--ni]); the
    // layout made by the instruction is the frame's until the container holds it
    EE_CFG_SET(kContainer,0) EE_CFG(ContainerConfig)->mContainer->AddChild(c[--a.nc]); break;
    EE_CFG_SET(kContainer,1) EE_CFG(ContainerConfig)->mContainer->AddLayoutPoint(static_cast<LayoutPoint*>(c[--a.nc])); break;
    EE_CFG_SET(kContainer,2) { // NEWLAYOUTPOINT's name: a layout point, its config registered under the name
        ContainerConfig* k=EE_CFG(ContainerConfig); SmartPtr<ComponentConfig> config=new ComponentConfig; SmartPtr<LayoutPoint> point=new LayoutPoint;
        config->SetComponent(point.get()); config->mParent=k; k->AddChildToConfigMap(s[--a.ns],config.get(),true); k->mContainer->AddLayoutPoint(point.get());
    } break;
    EE_CFG_SET(kContainer,4) EE_CFG(ContainerConfig)->mCurLayoutId=i[0]; break;
    EE_CFG_SET(kContainer,5) { int anId=i[--a.ni]; EE_CFG(ContainerConfig)->mContainer->EnableLayoutInstructions(anId,i[0]!=0); } break;
    EE_CFG_SET(kContainer,6) EE_CFG(ContainerConfig)->mContainer->Relayout(); break;
    EE_CFG_SET(kContainer,9) a.lay=new SameSizeLayout(true,true); break;
    EE_CFG_SET(kContainer,10) a.lay=new SameSizeLayout(true,false); break;
    EE_CFG_SET(kContainer,11) a.lay=new SameSizeLayout(false,true); break;
    EE_CFG_SET(kContainer,12) a.lay=new CenterLayout(c[--a.nc],true,true); break;
    EE_CFG_SET(kContainer,13) a.lay=new CenterLayout(c[--a.nc],true,false); break;
    EE_CFG_SET(kContainer,14) a.lay=new CenterLayout(c[--a.nc],false,true); break;
    EE_CFG_SET(kContainer,15) a.lay=new SpaceEvenlyLayout(c[--a.nc],i[0],i[1],true); a.ni=0; break;
    EE_CFG_SET(kContainer,16) a.lay=new SpaceEvenlyLayout(c[--a.nc],i[0],i[1],false); a.ni=0; break;
    EE_CFG_SET(kContainer,17) a.lay=new ClearRightLayout(c[--a.nc],i[0]); a.ni=0; break;
    EE_CFG_SET(kContainer,19) a.lay=new SizeAtLeastLayout(c[--a.nc],i[0],i[1]); a.ni=0; break;
    EE_CFG_SET(kContainer,20) a.lay->AddComponent(c[--a.nc]); break;
    EE_CFG_SET(kContainer,21) EE_CFG(ContainerConfig)->mContainer->AddChildLayout(a.lay.get(),EE_CFG(ContainerConfig)->mCurLayoutId); break;
    EE_CFG_SET(kContainer,22) EE_CFG(ContainerConfig)->mContainer->AddChildLayout(a.lay.get()); break;
    EE_CFG_SET(kContainer,23) EE_CFG(ContainerConfig)->mContainer->SetIsolateTabFocus(i[0]!=0); break;
    EE_CFG_SET(kContainer,24) EE_CFG(ContainerConfig)->mContainer->SetOnlyChildInput(i[0]!=0); break;
    // the LAYOUT family's state (cfgtab.rs layout): the previous rule's instructions and offsets, its reference, the
    // offsets read, the rule added (the frame's last six ints: instructions, offsets, which component stays the reference)
    EE_CFG_SET(kContainer,25) { ContainerConfig* k=EE_CFG(ContainerConfig); a.i[a.ni++]=k->mLayoutInstruction; for(int j=0;j<4;j++) a.i[a.ni++]=k->mLayoutParams[j]; } break;
    EE_CFG_SET(kContainer,26) { ContainerConfig* k=EE_CFG(ContainerConfig); if(k->mLayoutRef==NULL) throw ConfigObjectException("Invalid LayoutChain"); a.c[a.nc++]=k->mLayoutRef; } break;
    EE_CFG_SET(kContainer,27) for(int j=0;j<4;j++) EE_CFG(ContainerConfig)->mLayoutParams[j]=i[a.ni-4+j]; break;
    EE_CFG_SET(kContainer,28) {
        ContainerConfig* k=EE_CFG(ContainerConfig); const int* v=i+a.ni-6;
        for(int j=0;j<4;j++) k->mLayoutParams[j]=v[1+j];
        k->mContainer->Fn2F6140(c[0],v[0],c[1],v[1],v[2],v[3],v[4],k->mCurLayoutId); k->mLayoutRef=c[v[5]]; k->mLayoutInstruction=v[0];
    } break;
    EE_CFG_SET(kContainer,29) a.lay=new NoOverlapLayout(c[0],i[2],c[1],i[0],i[1]); a.nc=0; break;
    // StretchImageConfig (round 41; cfgimg.rs): the exports' children by number or column
    EE_CFG_SET(kStretchImage,0) EE_CFG(StretchImageConfig)->mStretchImage->SetImage(a.raw.get()); break;
    EE_CFG_SET(kStretchImage,1) EE_CFG(StretchImageConfig)->mStretchImage->SetTile(i[a.ni-2]!=0,i[a.ni-1]!=0); break;
    EE_CFG_SET(kStretchImage,2) EE_CFG(StretchImageConfig)->mStretchImage->SetBorders(i[0],i[1],i[2],i[3]); break;
    EE_CFG_SET(kStretchImage,3) EE_CFG(StretchImageConfig)->mStretchImage->SetSkipCenter(i[0]!=0); break;
    EE_CFG_SET(kStretchImage,4) {
        char image[512],config[512]; sprintf(image,"%s%d",a.rs[0].c_str(),i[a.ni-2]); sprintf(config,"%s%d",a.rs[1].c_str(),i[a.ni-1]); a.ni-=2;
        StretchImageConfig* k=EE_CFG(StretchImageConfig); SmartPtr<RawImage> raw=k->GetRawImageFromStr(image); CfgStretchAdd(k,config,CfgStretchChild(k),raw.get());
    } break;
    EE_CFG_SET(kStretchImage,5) a.i[a.ni++]=a.raw->GetWidth()/i[0]; break;
    EE_CFG_SET(kStretchImage,6) {
        int n=i[--a.ni];
        char config[512]; sprintf(config,"%s%d",a.rs[0].c_str(),n+1); SmartPtr<RawImage> cut=ImageCrop(a.raw.get(),n*i[1],0,i[1],a.raw->GetHeight());
        CfgStretchAdd(EE_CFG(StretchImageConfig),config,CfgStretchChild(EE_CFG(StretchImageConfig)),cut.get());
    } break;
    EE_CFG_SET(kStretchImage,7) CfgStretchAdd(EE_CFG(StretchImageConfig),s[0],static_cast<StretchImageConfig*>(a.child.get()),a.raw.get()); break;
    EE_CFG_SET(kStretchImage,8) a.child=CfgStretchChild(EE_CFG(StretchImageConfig)); break;
    // ImageFilterConfig (round 41; cfgimg.rs): the raw image, its copy, the filters by name, the crops' and export's children
    EE_CFG_SET(kImageFilter,0) EE_CFG(ImageFilterConfig)->SetRawImage(a.raw.get()); break;
    EE_CFG_SET(kImageFilter,2) EE_CFG(ImageFilterConfig)->SetRawImage(static_cast<RawImage*>(EE_CFG(ImageFilterConfig)->mRawImage->Duplicate().get())); break;
    EE_CFG_SET(kImageFilter,3) a.i[a.ni++]=EE_CFG(ImageFilterConfig)->mRawImage->GetWidth()/i[0]; break;
    EE_CFG_SET(kImageFilter,4) {
        ImageFilterConfig* k=EE_CFG(ImageFilterConfig); int n=i[--a.ni];
        SmartPtr<RawImage> image=ImageCrop(k->mRawImage.get(),n*i[1],0,i[1],k->mRawImage->GetHeight());
        char name[512]; sprintf(name,"%s%d",((std::string)s[0]).c_str(),n+1); CfgFilterChild(k,GUIString(name),image.get());
    } break;
    EE_CFG_SET(kImageFilter,5) CfgFilterChild(EE_CFG(ImageFilterConfig),s[0],a.raw.get()); break;
    EE_CFG_SET(kImageFilter,10) a.raw=ImageScalePercent(EE_CFG(ImageFilterConfig)->mRawImage.get(),i[a.ni-2],i[a.ni-1]); break;
    EE_CFG_SET(kImageFilter,11) a.raw=ImageScaleTo(EE_CFG(ImageFilterConfig)->mRawImage.get(),i[a.ni-2],i[a.ni-1]); break;
    EE_CFG_SET(kImageFilter,12) a.raw=ImageTint(EE_CFG(ImageFilterConfig)->mRawImage.get(),i[a.ni-3],i[a.ni-2],i[a.ni-1]); break;
    EE_CFG_SET(kImageFilter,13) a.raw=ImageCrop(EE_CFG(ImageFilterConfig)->mRawImage.get(),i[a.ni-4],i[a.ni-3],i[a.ni-2],i[a.ni-1]); break;
    EE_CFG_SET(kImageFilter,14) a.raw=ImageTransparent(EE_CFG(ImageFilterConfig)->mRawImage.get(),(DWORD)i[a.ni-1]); break;
    EE_CFG_SET(kImageFilter,15) a.raw=NULL; break;
    }
}
#undef EE_CFG_SET
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
// A config's instructions (R6 WONLobby round 26): its kind's tables and its base classes' in the original's order
// (ee_won_gui cfgkind.rs); ComponentConfig's own step answers MODIFY and MAPCOMPONENT through their handlers, the rest
// is its table. The first step that holds the key ends the call.
bool ComponentConfig::Virtual24(const std::string& key)
{
    int32_t aSteps[8];
    const int n=ee_won_cfg_kind_steps(this->mKind,aSteps,8);
    for(int i=0;i<n;i++) {
        if(aSteps[i]>=0) { if(this->RunTable(aSteps[i],key)) return true; continue; }
        const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_COMPONENTCONFIG_VIRTUAL24_KEY,key.data(),key.size());
        if(aKeyIdx==0 ? this->HandleModify() : aKeyIdx==1 ? this->HandleMapComponent() : this->RunTable(kComponent,key)) return true;
    }
    return false;
}
