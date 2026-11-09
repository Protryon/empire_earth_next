// Complete true DoPopupEvent3c: original2fd77e alloc3c/copies11DWORDs at10;
// RTTI44df58; inherited implicit empty lifetime311480/338c70. No owned popup refs.
#ifndef EE_WONAPI_WONLOBBYPOPUPEVENT_H
#define EE_WONAPI_WONLOBBYPOPUPEVENT_H
#include "ee/wonapi/WONLobbyGUI.h"
namespace WONAPI {
class DoPopupEvent : public WindowEventBase {
protected:
    PopupParams mParams; //10..3b, full2c POD containing borrowed raw pointers
public:
    DoPopupEvent(const PopupParams& params) : mParams(params) {}
    virtual void Deliver(); //32b650 originalexternal, forwards params to mWindow->OpenPopup
};
}
#endif
