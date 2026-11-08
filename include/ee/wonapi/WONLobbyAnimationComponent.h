#ifndef EE_WONAPI_WONLOBBYANIMATIONCOMPONENT_H
#define EE_WONAPI_WONLOBBYANIMATIONCOMPONENT_H
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyAnimation.h"
namespace WONAPI {
// Complete actual3c nativeAnimationComponent,43slots/RTTI4d66e8/VT44fb40.
// Constructor306e10 and nativeallocations33a57d/39b256 prove ownedAnimation38.
class AnimationComponent : public Component {
public:
    AnimationComponent(Animation* = NULL,bool = false); //306e10
    virtual void Virtual08(); //306ff0 SizeChanged
    virtual void Paint(Graphics&); //307000
    virtual bool TimerEvent(int); //307020 TimerEvent
    virtual void AddedToParent(); //306fd0 AddedToParent
    void SetAnimation(Animation*); //306f40
    void Start(bool = true); //306fa0
    int mDelayAcc; //30
    bool mStarted; //34, padding35..37
    SmartPtr<Animation> mAnimation; //38; implicitdtor306ed0 releases beforeComponentbase
};
}
#endif
