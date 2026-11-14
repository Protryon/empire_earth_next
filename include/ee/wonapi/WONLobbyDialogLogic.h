// DialogLogic: runs the lobby's modal dialogs (status, error, OK/Cancel, Yes/No) on a dialog stack
#ifndef EE_WONAPI_WONLOBBYDIALOGLOGIC_H
#define EE_WONAPI_WONLOBBYDIALOGLOGIC_H
#include "ee/wonapi/WONLobbyLogic.h"
#include "ee/wonapi/WONLobbyDialogs.h"
namespace WONAPI {
// The class is one Rust object (R6 WONLobby round 2: ee_won dlglogic.rs holds the dialog stack, the two dialogs,
// the status operation and runs every method); these are its forwarders. The host is in DialogLogic.cpp.
class DialogLogic : public LobbyLogic {
public:
    DialogLogic();
    virtual ~DialogLogic();
    virtual void KillHook();
    virtual void InitComponent(ComponentConfig* inConfig);
    virtual void Reset();
    void StatusOpComplete(AsyncOp* inOp);
    void ClearDialogs();
    int DoDialog(Component* inDialog);
    void RunOpWithStatus(AsyncOp* inOp, unsigned long inUnusedTimeout,
                         const GUIString& inTitle, const GUIString& inStatus,
                         int inStatusMode, unsigned long inTimeout);
    void ShowStatus(const GUIString& inTitle, const GUIString& inStatus, bool inCancel);
    void ShowError(const GUIString& inTitle, const GUIString& inStatus);
    void ShowError(const GUIString& inTitle, WONStatus inStatus);
    void UpdateStatus(WONStatus inStatus);
    int ShowOkCancel(const GUIString& inTitle, const GUIString& inText, bool inPlaySound);
    int ShowYesNo(const GUIString& inTitle, const GUIString& inText, bool inPlaySound);
    bool GetStatusCancelled(); // the original's mStatusCancelled
    EeWonDlgLogic* mRust;
private:
    intptr_t Call(EeWonDlgLogicCall inCall, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0, intptr_t d = 0,
                  intptr_t e = 0) { return ee_won_dlglogic_call(this->mRust, inCall, a, b, c, d, e); }
};
}
#endif
