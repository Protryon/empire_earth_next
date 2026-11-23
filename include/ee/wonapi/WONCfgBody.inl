#include "rust/won_gui/cfgbody.h"
// The config subclasses' bodies (R6 WON round 34): ee_won_gui cfgbody.rs over this host's one-statement ops.
// Included inside a namespace by ConfigTableHost.cpp (WONAPI) and by tests/won/won_cfgbody34_test.cpp (over its
// stand-ins), so both run these very statements; the classes' one-statement forwards (in their units) call
// CfgBodyCall. An op that can throw runs under the guard; CfgBodyCall rethrows its exception.
struct CfgBodyFrame { std::exception_ptr mErr; };
template<class F> static intptr_t CfgBodyGuard(void* inFrame, bool* ok, F f)
{
    try { return (intptr_t)f(); } catch (...) { static_cast<CfgBodyFrame*>(inFrame)->mErr = std::current_exception(); *ok = false; return 0; }
}
#define CFG_BODY_G(...) CfgBodyGuard(inFrame, ok, [&] { return (intptr_t)(__VA_ARGS__); })
#define CFG_BODY_GV(...) CfgBodyGuard(inFrame, ok, [&] { __VA_ARGS__; return (intptr_t)0; })
#define CFG_BTN static_cast<Button*>(k->mComponent.get())
#define CFG_PCT(h) (*(SmartPtr<PercentLayoutConfig>*)(h))
#define CFG_FONT static_cast<FontConfig*>(k)
#define CFG_FILTER static_cast<ImageFilterConfig*>(k)
static intptr_t CfgBodyHost(void* inFrame, EeWonCfgBodyOp op, void* c, intptr_t x, intptr_t y, intptr_t z, bool* ok)
{
    typedef EeWonCfgBodyOp O;
    ComponentConfig* k = (ComponentConfig*)c;
    switch (op) {
    case O::BtnWidth: return CFG_BTN->Width();
    case O::BtnHeight: return CFG_BTN->Height();
    case O::BtnTextHold: return (intptr_t)new GUIString(CFG_BTN->GetText());
    case O::ReadTextHold: return CFG_BODY_G(new GUIString(k->ReadString(false)));
    case O::BtnSetText: return CFG_BODY_GV(CFG_BTN->SetText(*(GUIString*)x, true));
    case O::BtnDesired: CFG_BTN->GetDesiredSize(((int*)x)[0], ((int*)x)[1]); return 0;
    case O::EndOfString: return k->EndOfString();
    case O::EnsureComma: return CFG_BODY_GV(k->EnsureComma());
    case O::BtnSetSize: CFG_BTN->SetSize((int)x, (int)y); return 0;
    case O::TextFree: delete (GUIString*)x; return 0;
    case O::PercentNew: return (intptr_t)new SmartPtr<PercentLayoutConfig>(new PercentLayoutConfig);
    case O::PercentParse: return CFG_BODY_G(k->ParseChild(*(ConfigParser*)y, CFG_PCT(x).get(), false, true));
    case O::PercentRef: return (intptr_t)CFG_PCT(x)->refs[y];
    case O::PercentRefSelf: CFG_PCT(x)->refs[0] = k->mComponent.get(); return 0;
    case O::PercentAdd: return CFG_BODY_GV(static_cast<ContainerConfig*>(k)->mContainer->AddChildLayout(new PercentLayout(CFG_PCT(x)->target, PercentLayoutParam(CFG_PCT(x)->refs[0], CFG_PCT(x)->refs[1], CFG_PCT(x)->refs[2], CFG_PCT(x)->refs[3], CFG_PCT(x)->params[0], CFG_PCT(x)->params[1], CFG_PCT(x)->params[2], CFG_PCT(x)->params[3], CFG_PCT(x)->params[4], CFG_PCT(x)->params[5], CFG_PCT(x)->params[6], CFG_PCT(x)->params[7])), static_cast<ContainerConfig*>(k)->mCurLayoutId));
    case O::PercentFree: delete &CFG_PCT(x); return 0;
    case O::SchemeNew: return (intptr_t)new SmartPtr<ColorSchemeConfig>(new ColorSchemeConfig);
    case O::SchemeParse: return CFG_BODY_G(k->ParseChild(*(ConfigParser*)y, ((SmartPtr<ColorSchemeConfig>*)x)->get(), false, true));
    case O::SchemeSet: static_cast<ContainerConfig*>(k)->mContainer->SetColorScheme((*(SmartPtr<ColorSchemeConfig>*)x)->mColorScheme.get()); return 0;
    case O::SchemeFree: delete (SmartPtr<ColorSchemeConfig>*)x; return 0;
    case O::BaseInstruction: return CFG_BODY_G(k->ComponentConfig::HandleInstruction((ConfigParser*)x, std::string((const char*)y, (size_t)z)));
    case O::PercentFind: return CFG_BODY_G(static_cast<PercentLayoutConfig*>(k)->target = static_cast<ComponentConfig*>(k->mParent)->Fn386860(k->mName));
    case O::PercentNotFound: ((ConfigParser*)x)->SetError("Component not found: " + k->mName, false); return 0;
    case O::CfgParent: return (intptr_t)static_cast<ComponentConfig*>(((ComponentConfig*)x)->mParent);
    case O::CfgComponent: return (intptr_t)((ComponentConfig*)x)->mComponent.get();
    case O::OwnScheme: return (intptr_t)((Component*)x)->GetOwnColorScheme();
    case O::CopyScheme: memcpy(static_cast<ColorSchemeConfig*>(k)->mColorScheme->mColors, ((ColorScheme*)x)->mColors, sizeof(static_cast<ColorSchemeConfig*>(k)->mColorScheme->mColors)); return 0;
    case O::DefaultScheme: return (intptr_t)WindowManager::GetDefaultWindowManager()->mColorScheme.get();
    case O::NeedRecalc: return CFG_FONT->mNeedRecalcFont;
    case O::FontPtr: return (intptr_t)CFG_FONT->mFont.get();
    case O::FontStale: return CFG_FONT->mFont->mDescriptor != CFG_FONT->mDescriptor;
    case O::FontFetch: return CFG_BODY_GV(CFG_FONT->mFont = Window::GetDefaultWindow()->GetFont(CFG_FONT->mDescriptor).get());
    case O::SetFontPtr: CFG_FONT->mFont = (Font*)x; return 0;
    case O::TakeDescriptor: CFG_FONT->mDescriptor = ((Font*)x)->mDescriptor; return 0;
    case O::ClearRecalc: CFG_FONT->mNeedRecalcFont = false; return 0;
    case O::DescSize: return CFG_FONT->mDescriptor.mSize;
    case O::FontHeight: return CFG_FONT->mFont->GetHeight();
    case O::ImgWidth: return static_cast<ImageConfig*>(k)->mImage->GetWidth();
    case O::ImgHeight: return static_cast<ImageConfig*>(k)->mImage->GetHeight();
    case O::FilterIsRaw: return CFG_FILTER->mImageIsRawImage;
    case O::FilterNative: return CFG_BODY_GV(CFG_FILTER->mImage = CFG_FILTER->mRawImage->GetNative(Window::GetDefaultWindow()->mDisplayContext.get()).get());
    case O::FilterRawDone: CFG_FILTER->mImageIsRawImage = false; return 0;
    case O::ImagePtr: return (intptr_t)static_cast<ImageConfig*>(k)->mImage.get();
    default: return 0;
    }
}
#undef CFG_BODY_G
#undef CFG_BODY_GV
#undef CFG_BTN
#undef CFG_PCT
#undef CFG_FONT
#undef CFG_FILTER
// a body of cfgbody.rs: its answer; an op's exception rethrown
intptr_t CfgBodyCall(ComponentConfig* inConfig, EeWonCfgBody inBody, intptr_t a, intptr_t b, intptr_t c)
{
    CfgBodyFrame aFrame;
    intptr_t aAnswer = 0;
    if (ee_won_cfg_body(CfgBodyHost, &aFrame, inConfig, inBody, a, b, c, &aAnswer) == 2) std::rethrow_exception(aFrame.mErr);
    return aAnswer;
}
