#ifndef EE_WONAPI_WONLOBBYTABCTRLCONFIG_H
#define EE_WONAPI_WONLOBBYTABCTRLCONFIG_H
#include "ee/wonapi/WONLobbyGUI.h"
namespace WONAPI {
// TabCtrlConfig (0x9c bytes): ContainerConfig (0x98) plus a borrowed TabCtrl pointer at +0x98.
// The explicit constructor (0x00795720) calls the base and installs the vtable 0x00852710.
class TabCtrlConfig : public ContainerConfig {
public:
    TabCtrlConfig(); // at 0x00795720; leaves +0x98 untouched
    static ComponentConfig* CfgFactory(); // at 0x0079add0
    static Component* CompFactory(); // at 0x0079ad70
    virtual bool Virtual24(const std::string&); //395740
    virtual bool GetPredefinedComponent(const std::string&,ComponentConfig*&,Component*&); //3958e0
    virtual void SetComponent(Component*); // at 0x0078f960
    TabCtrl* mTabCtrl; //98 borrowed
};
}
#endif
