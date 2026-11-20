// Target local configuration classes; RTTI records have global namespace identities.
#ifndef EE_WONAPI_WONLAYOUTCONFIGS_H
#define EE_WONAPI_WONLAYOUTCONFIGS_H
#include "ee/wonapi/WONLobbyGUI.h"
// Borrowed view (0xac bytes), allocated at 0x0078860b, constructor 0x00787fb0.
class PercentLayoutConfig : public WONAPI::ComponentConfig {
public:
    // at 0x00787fb0: no component references and every parameter unset (-1)
    PercentLayoutConfig() : WONAPI::ComponentConfig(EeWonCfgKind::PercentLayout)
    { for(int i=0;i<4;++i) this->refs[i]=NULL; for(int i=0;i<8;++i) this->params[i]=-1; }
    virtual bool PreParse(WONAPI::ConfigParser*);  //388000
    virtual bool PostParse(WONAPI::ConfigParser*) { return true; } // inline (body at 0x00787ff0)
    WONAPI::Component* target; //78, borrowed
    WONAPI::Component* refs[4]; //7c..88, borrowed
    int params[8]; //8c..a8
};
// Target is80, SDKlater mSetGlobal80/SETGLOBAL/COPYSYSTEM do not survive.
class ColorSchemeConfig : public WONAPI::ComponentConfig {
public:
    // at 0x00788260 (implicit destructor 0x00788340): owning a new color scheme
    ColorSchemeConfig() : WONAPI::ComponentConfig(EeWonCfgKind::ColorScheme) { this->mColorScheme=new WONAPI::ColorScheme; }
    virtual bool PreParse(WONAPI::ConfigParser*);  //3883b0
    virtual bool PostParse(WONAPI::ConfigParser*) { return true; } // inline (body at 0x00787ff0)
    WONAPI::Component* target; // offset 0x78, borrowed; the constructor leaves it untouched
    WONAPI::SmartPtr<WONAPI::ColorScheme> mColorScheme; //7c, true owned4B member
};
#endif
