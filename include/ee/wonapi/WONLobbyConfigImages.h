#ifndef EE_WONAPI_WONLOBBYCONFIGIMAGES_H
#define EE_WONAPI_WONLOBBYCONFIGIMAGES_H
#include "ee/wonapi/WONLobbyStretchImage.h"
#include "rust/won_gui/cfgkind.h"
#include "rust/won_gui/cfgbody.h"
namespace WONAPI {

// The media configs (R6 WON GUI round 36): the original's ImageConfig, StretchImageConfig, StretchCenterImageConfig,
// ImageFilterConfig, FontConfig, PercentLayoutConfig and ColorSchemeConfig are one class, MediaConfig, their kind
// (mKind) the original's class; their instructions are their kinds' tables (ee_won_gui cfgkind.rs, cfgtab.rs), their
// bodies cfgbody.rs's (WONCfgBody.inl). It holds every kind's members; each kind uses its own (the image's, the
// stretch images' raw aliases of it, the filter's raw image, the font's, the percent layout's, the scheme's).
class MediaConfig : public ComponentConfig {
public:
    explicit MediaConfig(EeWonCfgKind inKind); // the kind's constructor (ConfigTableHost.cpp)
    virtual bool PreParse(ConfigParser*); // the percent layout's and the scheme's own; the base's for the others
    virtual bool PostParse(ConfigParser*) { return true; } // 0x3099f0, 0x7099f0, 0x787ff0: no desired size
    virtual bool GetNamedValue(const std::string&,int&); // the font's (0x38a580) and the images' (0x38b370)
    bool Is(EeWonCfgKind inKind) const { return ee_won_cfg_kind_is(this->mKind,inKind); }
    Image* GetImage(); // an image kind's (the filter's 0x38cc80 builds its native image)
    RawImage* GetRawImage() { return this->mKind==EeWonCfgKind::ImageFilter ? this->mRawImage.get() : NULL; }
    void SetRawImage(RawImage*); //38c6e0 (the filter)
    Font* GetFont(); // 0x38a300; recalculates the font on demand
    void SetFont(Font*); //38a3e0
    SmartPtr<Image> mImage; StretchImage* mStretchImage; StretchCenterImage* mStretchCenterImage; // raw aliases
    SmartPtr<RawImage> mRawImage; bool mImageIsRawImage;
    SmartPtr<Font> mFont; FontDescriptor mDescriptor; bool mNeedRecalcFont;
    Component* target; Component* refs[4]; int params[8]; // the percent layout's, borrowed
    SmartPtr<ColorScheme> mColorScheme; // the scheme's, owned
};
// the original classes' names
typedef MediaConfig ImageConfig; typedef MediaConfig StretchImageConfig; typedef MediaConfig StretchCenterImageConfig;
typedef MediaConfig ImageFilterConfig; typedef MediaConfig FontConfig;
// whether a config is of a media kind (a world's stand-ins answer by their classes: WONCompCfg.inl)
template<class T> bool WONCfgIs(ConfigObject* inConfig, EeWonCfgKind inKind)
{ MediaConfig* m = dynamic_cast<MediaConfig*>(inConfig); return m != NULL && m->Is(inKind); }
// a config subclass's body in ee_won_gui cfgbody.rs (R6 WON round 34; the host: WONCfgBody.inl in ComponentConfigHost.cpp)
intptr_t CfgBodyCall(ComponentConfig* inConfig, EeWonCfgBody inBody, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0);
}
// the layout configs (the original's target-local classes with global RTTI names; WONLayoutConfigs.h until R6 WON GUI
// round 37): PercentLayoutConfig (0xac bytes, constructor 0x787fb0) and ColorSchemeConfig (constructor 0x788260)
typedef WONAPI::MediaConfig PercentLayoutConfig;
typedef WONAPI::MediaConfig ColorSchemeConfig;
#endif
