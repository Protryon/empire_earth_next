#ifndef EE_WONAPI_WONLOBBYWONFRAMEVALUECONFIG_H
#define EE_WONAPI_WONLOBBYWONFRAMEVALUECONFIG_H
#include "ee/wonapi/WONLobbyWONListConfig.h"
#include "ee/wonapi/WONLobbyComboBoxConfig.h"
#include "ee/wonapi/WONLobbyTabCtrlConfig.h"
namespace WONAPI {
// Config classes: only their base parts are declared.
class WONComboBoxConfig : public ComboBoxConfig {
public:
    virtual bool Virtual24(const std::string&); //397f70
    virtual void SetComponent(Component* component) { this->mComboBox=component; ComboBoxConfig::SetComponent(component); } // stores the derived pointer (+0x9c) before calling the base (body at 0x0078e7d0)
    Component* mComboBox; //9c (a WONComboBox kind)
};
class WONTabCtrlConfig : public TabCtrlConfig {
public:
    virtual bool Virtual24(const std::string&); //3981f0
    virtual void SetComponent(Component* component) { TabCtrlConfig::SetComponent(component); this->mTabCtrl=component; } // calls the base first, then stores +0x9c (body at 0x0078f100)
    Component* mTabCtrl; //9c (a WONTabCtrl kind)
};
}
#endif
