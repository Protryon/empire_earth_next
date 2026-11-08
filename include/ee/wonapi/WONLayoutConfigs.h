// Target local configuration classes; RTTI records have global namespace identities.
#ifndef EE_WONAPI_WONLAYOUTCONFIGS_H
#define EE_WONAPI_WONLAYOUTCONFIGS_H
#include "ee/wonapi/WONLobbyGUI.h"
// Borrowed view (0xac bytes), allocated at 0x0078860b, constructor 0x00787fb0.
class PercentLayoutConfig : public WONAPI::ComponentConfig {
public:
    PercentLayoutConfig(); // at 0x00787fb0
    virtual bool PreParse(WONAPI::ConfigParser*);  //388000
    virtual bool PostParse(WONAPI::ConfigParser*) { return true; } // inline (body at 0x00787ff0)
    virtual bool Virtual24(const std::string&); //3880c0
    WONAPI::Component* target; //78, borrowed
    WONAPI::Component* refs[4]; //7c..88, borrowed
    int params[8]; //8c..a8
};
// Target is80, SDKlater mSetGlobal80/SETGLOBAL/COPYSYSTEM do not survive.
class ColorSchemeConfig : public WONAPI::ComponentConfig {
public:
    ColorSchemeConfig(); // at 0x00788260; implicit destructor 0x00788340
    virtual bool PreParse(WONAPI::ConfigParser*);  //3883b0
    virtual bool PostParse(WONAPI::ConfigParser*) { return true; } // inline (body at 0x00787ff0)
    virtual bool Virtual24(const std::string&); //388410
    WONAPI::Component* target; // offset 0x78, borrowed; the constructor leaves it untouched
    WONAPI::SmartPtr<WONAPI::ColorScheme> mColorScheme; //7c, true owned4B member
};
#endif
