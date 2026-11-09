#ifndef EE_WONAPI_WONLOBBYCOMBOBOXCONFIG_H
#define EE_WONAPI_WONLOBBYCOMBOBOXCONFIG_H
#include "ee/wonapi/WONLobbyGUI.h"
namespace WONAPI {
class ComboBoxConfig : public ContainerConfig {
public:
    ComboBoxConfig();
    virtual bool Virtual24(const std::string&); //3821b0
    virtual bool GetPredefinedComponent(const std::string&,ComponentConfig*&,Component*&); //382070 external original
    virtual void SetComponent(Component*); //original SetComponent external
    ComboBox* mComboBox; //98
};
}
#endif
