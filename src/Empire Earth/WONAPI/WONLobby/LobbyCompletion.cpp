// Runs lobby operation completions on the window thread (file name inferred: no __FILE__)
// The original object of 0x757d70..0x757f24 is unknown: it may be part of the neighbouring chat or config unit
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
namespace WONAPI {
class LobbyCompletionEvent : public WindowEventBase {
public:
    LobbyCompletionEvent(LobbyCompletionBase* inCompletion, AsyncOp* inOp)
        : mCompletion(inCompletion), mOp(inOp) {}
    virtual void Deliver();
    SmartPtr<LobbyCompletionBase> mCompletion; //10
    AsyncOpPtr mOp; //14
};
// FUNCTION: WON 0x00757d70
// Runs the completion at once on the window thread, else posts it there as a window event
void LobbyCompletionBase::Complete(AsyncOpPtr theParam)
{
    DWORD aThreadId = WONAPI::WindowManager::GetThreadId();
    if (GetCurrentThreadId() == aThreadId)
        this->Run(theParam.get());
    else {
        WindowManager* aManager = WONAPI::WindowManager::GetDefaultWindowManager();
        if (aManager != NULL)
            aManager->PostEvent(new LobbyCompletionEvent(this, theParam.get()), true);
    }
}
// FUNCTION: WON 0x00757e70
// Runs the posted completion for its operation on the window thread
void LobbyCompletionEvent::Deliver()
{
    this->mCompletion->Run(this->mOp.get());
}
// The implicit destructor (0x757ea0) releases mOp, then mCompletion, then the WindowEventBase part
}



// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00757ea0
// (public: virtual __thiscall WONAPI::LobbyCompletionEvent::~LobbyCompletionEvent(void)    [??1LobbyCompletionEvent@WONAPI@@UAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00757e80
// (public: virtual void * __thiscall WONAPI::LobbyCompletionEvent::`scalar deleting destructor'(unsigned int)    [??_GLobbyCompletionEvent@WONAPI@@UAEPAX)
