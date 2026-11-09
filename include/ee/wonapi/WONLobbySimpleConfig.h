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
    RectangleComponent(); // at 0x00706840
    void SetSolidColor(int); // at 0x00706870
    void SetOutlineColor(int); // at 0x00706880
    virtual void Paint(Graphics&); //original external override
};
class RectangleComponentConfig : public ComponentConfig {
public:
    static ComponentConfig* CfgFactory(); // at 0x0079b090
    static Component* CompFactory(); // at 0x0079b030
    virtual bool Virtual24(const std::string&);
    virtual void SetComponent(Component*); //original external SetComponent
    RectangleComponent* mRectangleComponent; //78 borrowed
};
class RectangleComponent3DConfig : public ComponentConfig {
public:
    static ComponentConfig* CfgFactory(); // at 0x0079b140
    static Component* CompFactory(); // at 0x0079b0e0
    virtual bool Virtual24(const std::string&);
    virtual void SetComponent(Component*); //original external SetComponent
    RectangleComponent3D* mRectangleComponent3D; //78 borrowed
};
class ImageComponentConfig : public ComponentConfig {
public:
    static ComponentConfig* CfgFactory(); // at 0x0079b1f0
    static Component* CompFactory(); // at 0x0079b190
    virtual bool Virtual24(const std::string&);
    virtual void SetComponent(Component*); //original external SetComponent
    ImageComponent* mImageComponent; //78 borrowed
};
// AnimationComponentConfig (0x7c bytes), RTTI 0x008deb80, vtable 0x00852a94: the factory 0x0079b2a0
// allocates 0x7c bytes, calls ComponentConfig's constructor (0x007825b0), then sets its own vtable.
class AnimationComponent;
class AnimationComponentConfig : public ComponentConfig {
public:
    virtual bool Virtual24(const std::string&); // at 0x00794e10: calls the base and normalizes the bool
    virtual void SetComponent(Component*); // at 0x0078f8d0 (shared body; stores the typed pointer at +0x78)
    static ComponentConfig* CfgFactory(); // at 0x0079b2a0
    static Component* CompFactory(); // at 0x0079b240
    AnimationComponent* mAnimationComponent; // offset 0x78, borrowed; class size 0x7c; implicit destructor
};
class ScreenContainerConfig : public ContainerConfig {
public:
    static ComponentConfig* CfgFactory(); // at 0x0079b350
    static Component* CompFactory(); // at 0x0079b2f0
    virtual bool Virtual24(const std::string&);
    virtual void SetComponent(Component*); //original external SetComponent
    ScreenContainer* mScreenContainer; //98 borrowed
};
}
#endif
