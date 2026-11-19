#ifndef EE_WONAPI_WONLOBBYMSCONFIG_H
#define EE_WONAPI_WONLOBBYMSCONFIG_H
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyResourceConfig.h"
#include "ee/wonapi/WONGUIShell.h" // the inline factories make shell kinds
namespace WONAPI {
class ResourceConfigTable;
void MSComponentConfig_Init(ResourceConfigTable*); // at 0x0078dcb0
class MSButtonBase;
// Config classes: only their base parts are declared.
class MSButtonConfig : public ButtonConfig {
public:
    MSButtonConfig() {} // empty, at 0x0078f3a0 (0x80 bytes)
    static ComponentConfig* CfgFactory(); // at 0x0078e600
    static Component* ButtonFactory(); // at 0x0078e480
    static Component* CheckboxFactory(); // at 0x0078e4e0
    static Component* RadioFactory(); // at 0x0078e540
    static Component* LabelFactory(); // at 0x0078e5a0
    virtual bool Virtual24(const std::string&);
    virtual void SetComponent(Component*); // defined out of line
    MSButtonBase* mMSButton; //7c borrowed
};
class MSArrowButtonConfig : public ButtonConfig {
public:
    static ComponentConfig* CfgFactory(); // at 0x0078e850
    static Component* CompFactory(); // at 0x0078e7f0
    virtual bool Virtual24(const std::string&);
    virtual void SetComponent(Component*); // defined out of line
    Component* mMSArrowButton; //7c borrowed (an MSArrowButton kind: WONShell::Op)
};
class MSTabButtonConfig : public MSButtonConfig {
public:
    static ComponentConfig* CfgFactory() { return new MSTabButtonConfig; } // inline (body at 0x0078ea40, 0x84 bytes)
    static Component* CompFactory(); // at 0x0078e960
    virtual bool Virtual24(const std::string&);
    virtual void SetComponent(Component*); // defined out of line
    Component* mMSTabButton; //80 borrowed (an MSTabButton kind)
};
class MS3DFrameConfig : public ComponentConfig {
public:
    static ComponentConfig* CfgFactory(); // at 0x0078ee70
    static Component* CompFactory(); // at 0x0078ee10
    virtual bool Virtual24(const std::string&);
    virtual void SetComponent(Component*); // defined out of line
    Component* mMS3DFrame; //78 borrowed (an MS3DFrame kind)
};
// Config subclasses declared for their static factories (defined out of line).
class MSInputBoxConfig : public InputBoxConfig {
public:
    static ComponentConfig* CfgFactory(); // at 0x0078e6c0
    static Component* CompFactory(); // at 0x0078e660
};
class MSCloseButtonConfig : public ButtonConfig {
public:
    static ComponentConfig* CfgFactory(); // at 0x0078e910
    static Component* CompFactory() { return WONShellNew(EeWonShellKind::MsCloseButton); } // inline (body at 0x0078e8b0)
};
class MSHeaderButtonConfig : public MSButtonConfig {
public:
    static ComponentConfig* CfgFactory() { return new MSHeaderButtonConfig; } // inline (body at 0x0078eb80, 0x84 bytes)
    static Component* CompFactory(); // at 0x0078eaa0
};
class MSSeperatorConfig : public ComponentConfig {
public:
    static ComponentConfig* CfgFactory(); // at 0x0078ed20
    static Component* CompFactory() { return WONShellNew(EeWonShellKind::MsSeperator,6); } // inline (body at 0x0078ecc0)
};
class MSEtchedFrameConfig : public ComponentConfig {
public:
    static ComponentConfig* CfgFactory(); // at 0x0078ef20
    static Component* CompFactory() { return WONShellNew(EeWonShellKind::MsEtchedFrame); } // inline (body at 0x0078eec0)
};
class MSDlgBackConfig : public ComponentConfig {
public:
    static ComponentConfig* CfgFactory(); // at 0x0078edc0
    static Component* CompFactory(); // at 0x0078ed70
};
}
#endif
