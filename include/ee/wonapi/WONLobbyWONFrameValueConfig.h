#ifndef EE_WONAPI_WONLOBBYWONFRAMEVALUECONFIG_H
#define EE_WONAPI_WONLOBBYWONFRAMEVALUECONFIG_H
#include "ee/wonapi/WONLobbyWONListConfig.h"
#include "ee/wonapi/WONLobbyComboBoxConfig.h"
#include "ee/wonapi/WONLobbyTabCtrlConfig.h"
#include "ee/wonapi/WONLobbySkinTab.h"
#include "ee/wonapi/WONLobbySkinnedContainers.h" // full WONComboBox (0xbc) for the inline binding
namespace WONAPI {
// Config classes: only their base parts are declared.
class WONComboBoxConfig : public ComboBoxConfig {
public:
    static ComponentConfig* CfgFactory() { return new WONComboBoxConfig; } // inline (body at 0x00796a90, 0xa0 bytes)
    virtual bool Virtual24(const std::string&); //397f70
    virtual void SetComponent(Component* component) { this->mComboBox=static_cast<WONComboBox*>(component); ComboBoxConfig::SetComponent(component); } // stores the derived pointer (+0x9c) before calling the base (body at 0x0078e7d0)
    WONComboBox* mComboBox; //9c
};
class WONTabCtrlConfig : public TabCtrlConfig {
public:
    static ComponentConfig* CfgFactory() { return new WONTabCtrlConfig; } // inline (body at 0x00796af0, 0xa0 bytes)
    virtual bool Virtual24(const std::string&); //3981f0
    virtual void SetComponent(Component* component) { TabCtrlConfig::SetComponent(component); this->mTabCtrl=static_cast<WONTabCtrl*>(component); } // calls the base first, then stores +0x9c (body at 0x0078f100)
    WONTabCtrl* mTabCtrl; //9c
};
}
#endif
