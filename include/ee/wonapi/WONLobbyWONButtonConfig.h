#ifndef EE_WONAPI_WONLOBBYWONBUTTONCONFIG_H
#define EE_WONAPI_WONLOBBYWONBUTTONCONFIG_H
#include "ee/wonapi/WONLobbyResourceConfig.h"
#include "ee/wonapi/WONGUIShell.h"
namespace WONAPI {
// Config classes (0x80 bytes): ButtonConfig (0x7c) + a raw pointer to the control at +0x7c.
class WONSimpleButtonConfig : public ButtonConfig {
public:
    virtual bool Virtual24(const std::string&); // slot 0x24, at 0x007974e0
    virtual void SetComponent(Component*); // at 0x00796a20 (shared body)
public: // (the table reader's setters read the members)
    Component* mButton; //7c (a WONSimpleButton kind, WONGUIShell.h)
};
class WONButtonConfig : public ButtonConfig {
public:
    virtual bool Virtual24(const std::string&); // slot 0x24, at 0x00797aa0
    virtual void SetComponent(Component*); // at 0x00796a20 (shared body)
public: // (the table reader's setters read the members)
    WONButton* mButton; //7c
};
}
#endif
