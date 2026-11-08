// DialogLogic: runs the lobby's modal dialogs (status, error, OK/Cancel, Yes/No) on a dialog stack
#ifndef EE_WONAPI_WONLOBBYDIALOGLOGIC_H
#define EE_WONAPI_WONLOBBYDIALOGLOGIC_H
#include "ee/wonapi/WONLobbyLogic.h"
#include "ee/wonapi/WONLobbyDialogs.h"
namespace WONAPI {
class DialogLogic : public LobbyLogic {
protected:
    struct DialogInfo {
        DialogInfo() : mInModal(false) {}
        SmartPtr<Window> mWindow;
        SmartPtr<Component> mComponent;
        WONRectangle mOldRect;
        bool mInModal;
    };
    typedef std::list<DialogInfo> DialogStack;
public:
    DialogLogic();
    virtual ~DialogLogic();
    virtual void KillHook();
    virtual void InitComponent(ComponentConfig* inConfig);
    virtual void Reset();
    void StatusOpComplete(AsyncOp* inOp);
    void ClearDialogs();
    bool CheckDialog(Component* inDialog, bool& outSizeChanged);
    SmartPtr<Window> GetDialogWindow(bool inReuse);
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
    DialogStack mDialogStack;
    bool mPopupWindows;
    bool mStatusCancelled;
    SmartPtr<OkCancelDialog> mOkCancelDialog;
    SmartPtr<StatusDialog> mStatusDialog;
    bool mStatusVisible;
    AsyncOpPtr mStatusOp;
    int mStatusMode;
};
}
#endif
