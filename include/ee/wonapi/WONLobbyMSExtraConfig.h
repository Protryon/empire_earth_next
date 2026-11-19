#ifndef EE_WONAPI_WONLOBBYMSEXTRACONFIG_H
#define EE_WONAPI_WONLOBBYMSEXTRACONFIG_H
#include "ee/wonapi/WONLobbyResourceConfig.h"
#include "ee/wonapi/WONLobbyComboBoxConfig.h"
#include "ee/wonapi/WONLobbyTabCtrlConfig.h"
#include "ee/wonapi/WONGUIShell.h"
namespace WONAPI {
// The controls (MSComboBox, MSScrollbar, MSEtchedFrameGroup, MSTabCtrl, MSScrollContainer, MSHeaderControl,
// MSListCtrl) are shell kinds (ee/wonapi/WONGUIShell.h, ee_won_gui shell_mscont.rs); their configs hold them by
// the shell's base class and call a deleted class's own methods as WONShell::Op.
// Config subclasses (14-slot vtables; declarations only).
class MSComboBoxConfig : public ComboBoxConfig {
public:
    static ComponentConfig* CfgFactory() { return new MSComboBoxConfig; } // inline (body at 0x0078e770, 0xa0 bytes)
    static Component* CompFactory(); // at 0x0078e710
    virtual bool Virtual24(const std::string&); //38f810
    virtual void SetComponent(Component*); // at 0x0078e7d0
    ComboBox* mMSComboBox; //9c borrowed (an MSComboBox kind)
};
class MSScrollbarConfig : public ScrollbarConfig {
public:
    static ComponentConfig* CfgFactory(); // at 0x0078ec60
    static Component* CompFactory(); // at 0x0078ec00
    virtual bool Virtual24(const std::string&); //38f880
    virtual void SetComponent(Component*); // at 0x0078f860
    Scrollbar* mMSScrollbar; //9c borrowed (an MSScrollbar kind)
};
class MSEtchedFrameGroupConfig : public ContainerConfig {
public:
    static ComponentConfig* CfgFactory(); // at 0x0078efe0
    static Component* CompFactory(); // at 0x0078ef80
    virtual bool Virtual24(const std::string&); //38fa40
    virtual bool GetPredefinedComponent(const std::string&,ComponentConfig*&,Component*&); // at 0x0078f980
    virtual void SetComponent(Component*); // at 0x0078f960 (identical body shared with the inherited one)
    Container* mFrame; //98 borrowed (an MSEtchedFrameGroup kind)
};
class MSTabCtrlConfig : public TabCtrlConfig {
public:
    static ComponentConfig* CfgFactory() { return new MSTabCtrlConfig; } // inline (body at 0x0078f0a0, 0xa0 bytes)
    static Component* CompFactory(); // at 0x0078f040
    virtual bool Virtual24(const std::string&); //38fb30
    virtual void SetComponent(Component*); // at 0x0078f100
    TabCtrl* mMSTabCtrl; //9c borrowed (an MSTabCtrl kind)
};
class MSScrollContainerConfig : public ScrollbarScrollerConfig {
public:
    static ComponentConfig* CfgFactory(); // at 0x0078f180
    static Component* CompFactory(); // at 0x0078f120
    virtual bool Virtual24(const std::string&); //38fd30
    virtual bool GetPredefinedComponent(const std::string&,ComponentConfig*&,Component*&); // at 0x0078fdd0
    virtual void SetComponent(Component*); // at 0x0078fd10 (identical body shared with the inherited one)
    ScrollbarScroller* mScrollContainer; //a0 borrowed (an MSScrollContainer kind)
};
class MSHeaderControlConfig : public MultiListHeaderConfig {
public:
    static ComponentConfig* CfgFactory(); // at 0x0078f240
    static Component* CompFactory(); // at 0x0078f1e0
    virtual bool Virtual24(const std::string&); //38ff00
    virtual void SetComponent(Component*); // at 0x0078f2a0
    MultiListHeader* mMSHeader; //9c borrowed (an MSHeaderControl kind)
};
class MSListCtrlConfig : public MSScrollContainerConfig {
public:
    static ComponentConfig* CfgFactory(); // at 0x0078f320
    static Component* CompFactory(); // at 0x0078f2c0
    virtual bool Virtual24(const std::string&); //38ffd0
    virtual bool GetPredefinedComponent(const std::string&,ComponentConfig*&,Component*&); // at 0x00790020
    virtual void SetComponent(Component*); // defined out of line
    ScrollbarScroller* mListCtrl; //a4 borrowed (an MSListCtrl kind)
};
}
#endif
