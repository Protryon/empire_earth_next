#ifndef EE_WONAPI_WONLOBBYWONLISTCONFIG_H
#define EE_WONAPI_WONLOBBYWONLISTCONFIG_H
#include "ee/wonapi/WONLobbyResourceConfig.h"
#include "ee/wonapi/WONLobbySkinListControls.h"
namespace WONAPI {
// Derived control (declarations only).
class WONScrollbar : public Scrollbar {
public:
    WONScrollbar(bool=true); // at 0x00719540 (0xbc bytes)
    bool mIsTransparent; // offset 0xb8, padding to 0xbb; set by the constructor at 0x00719553
};
// Config base parts; their constructors and destructors are out of line.
class WONScrollbarConfig : public ScrollbarConfig {
public:
    static ComponentConfig* CfgFactory(); // at 0x00796bc0
    virtual bool Virtual24(const std::string&); //3986d0
    virtual void SetComponent(Component*); // at 0x0078f100
    WONScrollbar* mScrollbar; //9c
};
class WONScrollContainerConfig : public ScrollbarScrollerConfig {
public:
    static ComponentConfig* CfgFactory(); // at 0x00796c20
    virtual bool Virtual24(const std::string&); // at 0x007987e0
    virtual bool GetPredefinedComponent(const std::string&,ComponentConfig*&,Component*&); // at 0x007989f0
    virtual void SetComponent(Component* component) { ScrollbarScrollerConfig::SetComponent(component); this->mScrollContainer=static_cast<WONScrollContainer*>(component); } //396c80
    WONScrollContainer* mScrollContainer; //a0
};
class WONHeaderControlConfig : public MultiListHeaderConfig {
public:
    virtual bool Virtual24(const std::string&); //398b20
    virtual void SetComponent(Component*); // at 0x00798f70
    WONHeaderControl* mHeader; //9c
};
class WONListCtrlConfig : public WONScrollContainerConfig {
public:
    static ComponentConfig* CfgFactory(); // at 0x00796ca0
    virtual bool Virtual24(const std::string&); //398c00
    virtual bool GetPredefinedComponent(const std::string&,ComponentConfig*&,Component*&); // at 0x00798e30
    virtual void SetComponent(Component*); // at 0x00796d00
    WONListCtrl* mListCtrl; //a4
};
}
#endif
