#ifndef EE_WONAPI_WONLOBBYWONBUTTONCONFIG_H
#define EE_WONAPI_WONLOBBYWONBUTTONCONFIG_H
#include "ee/wonapi/WONLobbyResourceConfig.h"
#include "ee/wonapi/WONLobbyMSSkinImage.h"
namespace WONAPI {
// Config classes (0x80 bytes): ButtonConfig (0x7c) + a raw pointer to the control at +0x7c.
class WONSimpleButtonConfig : public ButtonConfig {
public:
    static ComponentConfig* CfgFactory(); // at 0x007969c0
    virtual bool Virtual24(const std::string&); // slot 0x24, at 0x007974e0
    virtual void SetComponent(Component*); // at 0x00796a20 (shared body)
public: // (the table reader's setters read the members)
    WONSimpleButton* mButton; //7c
};
class WONButtonConfig : public ButtonConfig {
public:
    static ComponentConfig* CfgFactory(); // at 0x00796a30
    virtual bool Virtual24(const std::string&); // slot 0x24, at 0x00797aa0
    virtual void SetComponent(Component*); // at 0x00796a20 (shared body)
public: // (the table reader's setters read the members)
    WONButton* mButton; //7c
};
}
// The original's file-static skin at 0x00966af0 (declaration only).
extern WONAPI::SmartPtr<WONAPI::WONSkin> gSkin;
#endif
