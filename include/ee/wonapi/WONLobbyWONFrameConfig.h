#ifndef EE_WONAPI_WONLOBBYWONFRAMECONFIG_H
#define EE_WONAPI_WONLOBBYWONFRAMECONFIG_H
#include "ee/wonapi/WONLobbyGUI.h"
namespace WONAPI {
// Uses WONFrameComponent from WONLobbyGUI.h.
class WONFrameComponentConfig : public ComponentConfig {
public:
    static ComponentConfig* CfgFactory(); // at 0x00796b50
    virtual bool Virtual24(const std::string&); //398440
    virtual bool GetNamedValue(const std::string&,int&); //398590
    virtual void SetComponent(Component*); // at 0x00796ba0
    WONFrameComponent* mFrameComponent; //78 borrowed, prefix7c
};
}
#endif
