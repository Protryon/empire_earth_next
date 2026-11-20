#ifndef EE_WONAPI_WONLOBBYWONLISTCONFIG_H
#define EE_WONAPI_WONLOBBYWONLISTCONFIG_H
#include "ee/wonapi/WONLobbyResourceConfig.h"
namespace WONAPI {
// Config base parts; their constructors and destructors are out of line.
class WONScrollbarConfig : public ScrollbarConfig {
public:
    virtual bool Virtual24(const std::string&); //3986d0 (the scrollbar: ScrollbarConfig's mScrollbar)
};
class WONScrollContainerConfig : public ScrollbarScrollerConfig {
public:
    virtual bool Virtual24(const std::string&); // at 0x007987e0
    virtual bool GetPredefinedComponent(const std::string&,ComponentConfig*&,Component*&); // at 0x007989f0
    virtual void SetComponent(Component* component) { ScrollbarScrollerConfig::SetComponent(component); this->mScrollContainer=static_cast<ScrollbarScroller*>(component); } //396c80
    ScrollbarScroller* mScrollContainer; //a0 (a WONScrollContainer kind)
};
class WONHeaderControlConfig : public MultiListHeaderConfig {
public:
    virtual bool Virtual24(const std::string&); //398b20 (the header: MultiListHeaderConfig's mHeader)
};
class WONListCtrlConfig : public WONScrollContainerConfig {
public:
    virtual bool Virtual24(const std::string&); //398c00
    virtual bool GetPredefinedComponent(const std::string&,ComponentConfig*&,Component*&); // at 0x00798e30
    virtual void SetComponent(Component*); // at 0x00796d00
    ScrollbarScroller* mListCtrl; //a4 (a WONListCtrl kind)
};
}
#endif
