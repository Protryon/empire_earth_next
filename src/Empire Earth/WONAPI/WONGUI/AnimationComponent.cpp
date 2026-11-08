// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyAnimationComponent.h"

using namespace WONAPI;

// FUNCTION: WON 0x00706e10
// Creates a component that plays an animation, sized to it and optionally started at once
AnimationComponent::AnimationComponent(Animation* animation,bool startNow)
    :mAnimation(animation)
{
    this->mDelayAcc=0;
    this->mStarted=false;
    this->SetAnimation(animation);
    if(animation==NULL)
        return;
    this->SetSize(animation->GetWidth(),animation->GetHeight());
    if(startNow)
        this->Start();
}

// FUNCTION: WON 0x00706f40
// Sets the animation the component plays
void AnimationComponent::SetAnimation(Animation* animation)
{
    this->mAnimation=animation;
}

// FUNCTION: WON 0x00706fa0
// Starts playback (optionally from the first frame) and requests timer ticks
void AnimationComponent::Start(bool restart)
{
    if(restart)
    {
        this->mAnimation->Rewind();
        this->mDelayAcc=0;
    }
    this->mStarted=true;
    this->RequestTimer(true);
}

// FUNCTION: WON 0x00706fd0
// Re-requests timer ticks when the component joins a parent while playing
void AnimationComponent::AddedToParent()
{
    Component::AddedToParent();
    if(this->mStarted)
        this->RequestTimer(true);
}

// FUNCTION: WON 0x00706ff0
// Forwards to the base component's handler with no extra behaviour
void AnimationComponent::Virtual08()
{
    Component::Virtual08();
}

// FUNCTION: WON 0x00707000
// Paints the component, then the animation's current frame at its top left
void AnimationComponent::Paint(Graphics& g)
{
    Component::Paint(g);
    this->mAnimation->Draw(g,0,0);
}

// FUNCTION: WON 0x00707020
// Advances the animation once the current frame's delay has passed; returns false when it has finished
bool AnimationComponent::TimerEvent(int delta)
{
    this->mDelayAcc+=delta;
    if(!(this->mDelayAcc>this->mAnimation->GetFrameDelayTime()))
        return true;
    if(!this->mAnimation->NextFrame())
        return false;
    this->mDelayAcc=0;
    this->Invalidate();
    return true;
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00706ed0
// (public: virtual __thiscall WONAPI::AnimationComponent::~AnimationComponent(void)    [??1AnimationComponent@WONAPI@@UAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00706eb0
// (public: virtual void * __thiscall WONAPI::AnimationComponent::`scalar deleting destructor'(unsigned int)    [??_GAnimationComponent@WONAPI@@UAEPAXI@Z])
