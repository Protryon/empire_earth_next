#ifndef EE_WONAPI_WONLOBBYSIMPLECONFIG_H
#define EE_WONAPI_WONLOBBYSIMPLECONFIG_H
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyRoomServer.h"
namespace WONAPI {
// RectangleComponent (0x38 bytes): solid and outline colours.
class RectangleComponent : public Component {
    int mSolidColor; // offset 0x30
    int mOutlineColor; // offset 0x34
public:
    RectangleComponent() : mSolidColor(-1),mOutlineColor(-1) {} // at 0x00706840: no fill, no outline
    void SetSolidColor(int c) { this->mSolidColor=c; } // at 0x00706870
    void SetOutlineColor(int c) { this->mOutlineColor=c; } // at 0x00706880
    virtual void Paint(Graphics&); // 0x706890 (guidraw.rs rectangle_paint; GuiHost.cpp)
};
}
#endif
