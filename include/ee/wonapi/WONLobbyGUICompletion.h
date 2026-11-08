// GUI callback templates; the AsyncOpPtr members sit at +0x0c and +0x18.
#ifndef EE_WONAPI_WONLOBBYGUICOMPLETION_H
#define EE_WONAPI_WONLOBBYGUICOMPLETION_H
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONLobbyGUI.h"
namespace WONAPI {
template<class ResultType> class GUICompletionEvent;
template<class ResultType>
class GUICompletion : public CompletionBase<ResultType> {
    virtual void Complete(ResultType result);
    SmartPtr<CompletionBase<ResultType> > mCompletion;
public:
    GUICompletion(CompletionBase<ResultType>* completion) :mCompletion(completion) {}
};
template<class ResultType>
class GUICompletionEvent : public WindowEventBase {
protected:
    SmartPtr<CompletionBase<ResultType> > mCompletion;
    ResultType mResult;
public:
    GUICompletionEvent(AsyncOp* op,CompletionBase<ResultType>* completion)
        :mCompletion(completion),mResult(op) {}
    virtual void Deliver() { this->mCompletion->Complete(this->mResult); }
};
template<class ResultType>
inline void GUICompletion<ResultType>::Complete(ResultType result)
{
    if(GetCurrentThreadId()==WONAPI::WindowManager::GetThreadId())
        this->mCompletion->Complete(result);
    else
    {
        WindowManager* manager=WONAPI::WindowManager::GetDefaultWindowManager();
        if(manager!=NULL)
            manager->PostEvent(new GUICompletionEvent<ResultType>(result.get(),this->mCompletion.get()));
    }
}
typedef GUICompletion<AsyncOpPtr> OpGUICompletion;
}
#endif
