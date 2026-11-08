// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
// DialogLogic: the lobby's modal dialogs (OK/Cancel, Yes/No, status and error boxes), inline or in popup windows
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include <set>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyPersistence.h"
#include "ee/wonapi/WONLobbyCore.h"
#include "ee/wonapi/WONLobbyDialogs.h"
#include "ee/wonapi/WONLobbyDialogLogic.h"
#include "ee/wonapi/WONStatus.h"
namespace WONAPI {
// FUNCTION: WON 0x7dc3f0
// Creates the dialog logic, showing dialogs inside the lobby window (no popups)
DialogLogic::DialogLogic() { this->mPopupWindows = false; this->mStatusCancelled = false; }
// FUNCTION: WON 0x7dc480
// Destroys the dialog logic (members only)
DialogLogic::~DialogLogic() {}
// FUNCTION: WON 0x7dc5a0
// Looks up the OK/Cancel and status dialogs and takes the popup-window setting from the window manager
void DialogLogic::InitComponent(ComponentConfig* inConfig)
{
    this->mStatusVisible = false;
    this->mStatusMode = 0;
    this->mStatusOp = NULL;
    SafeGetComponent(inConfig, this->mOkCancelDialog, "OkCancelDialog", __FILE__, 48);
    SafeGetComponent(inConfig, this->mStatusDialog, "StatusDialog", __FILE__, 49);
    if (WONAPI::WindowManager::GetDefaultWindowManager() != NULL)
        this->mPopupWindows = WONAPI::WindowManager::GetDefaultWindowManager()->mBA5;
}
// FUNCTION: WON 0x7dc630
// Cancels the operation behind the status box and removes every dialog that is not running
void DialogLogic::KillHook()
{
    if (this->mStatusOp.get() != NULL) {
        this->mStatusOp->Kill(true);
        this->mStatusOp = NULL;
    }
    this->ClearDialogs();
}
// FUNCTION: WON 0x7dc680
// Closes the status box when its operation finishes (or succeeds, by mode), else shows the result in it
void DialogLogic::StatusOpComplete(AsyncOp* inOp)
{
    if (inOp != this->mStatusOp.get())
        return;
    bool aDone = this->mStatusMode == 2 || (this->mStatusMode == 1 && inOp->GetStatus() == WS_Success);
    if (aDone)
        GetLobbyWindow()->EndDialog(-1, NULL, this->mStatusDialog.get());
    else
        this->UpdateStatus(inOp->GetStatus());
}
// FUNCTION: WON 0x7dc6d0
// Removes the dialogs above the newest running one, closing their popup windows, and forgets them
void DialogLogic::ClearDialogs()
{
    if (this->mDialogStack.empty() || GetLobbyRoot() == NULL)
        return;
    bool aChanged = false;
    DialogStack::iterator aBackItr = this->mDialogStack.end();
    while (aBackItr != this->mDialogStack.begin()) {
        --aBackItr;
        if (aBackItr->mInModal)
            break;
        Component* aComponent = aBackItr->mComponent.get();
        if (aComponent->mParent != NULL)
            aComponent->mParent->RemoveChild(aComponent);
        if (this->mPopupWindows)
            aBackItr->mWindow->Close();
        aChanged = true;
    }
    if (!aChanged)
        return;
    DialogStack::iterator anItr = this->mDialogStack.begin();
    while (anItr != this->mDialogStack.end()) {
        if (!anItr->mInModal)
            this->mDialogStack.erase(anItr++);
        else
            ++anItr;
    }
}
// FUNCTION: WON 0x7dc790
// Returns whether a dialog is the top one; reports whether its size changed since it was shown
bool DialogLogic::CheckDialog(Component* inDialog, bool& outSizeChanged)
{
    outSizeChanged = true;
    if (this->mDialogStack.empty())
        return false;
    DialogInfo& anInfo = this->mDialogStack.back();
    if (anInfo.mComponent.get() != inDialog)
        return false;
    if (inDialog->Width() == anInfo.mOldRect.width && inDialog->Height() == anInfo.mOldRect.height)
        outSizeChanged = false;
    return true;
}
// FUNCTION: WON 0x7dc9a0
// Runs a dialog modally, centered in the lobby or in a popup window (reused if on top); returns its result
int DialogLogic::DoDialog(Component* inDialog)
{
    bool aSizeChanged = true;
    bool aReuse = this->CheckDialog(inDialog, aSizeChanged);
    if (!aReuse)
        this->ClearDialogs();
    SmartPtr<Window> aWindow = this->GetDialogWindow(aReuse);
    if (aWindow.get() == NULL)
        return -1;
    SmartPtr<Container> aRoot;
    if (this->mPopupWindows) {
        aWindow->SetClientSize(inDialog->Width(), inDialog->Height());
        aRoot = aWindow->mRoot.get();
    }
    else
        aRoot = GetLobbyRoot();
    if (aRoot.get() == NULL)
        return -1;
    if (aSizeChanged) {
        if (this->mPopupWindows)
            aWindow->Center(true);
        else
            CenterLayout::Center(aRoot.get(), inDialog, true, true);
    }
    if (!aReuse) {
        aRoot->AddChild(inDialog);
        this->mDialogStack.push_back(DialogInfo());
    }
    DialogInfo& anInfo = this->mDialogStack.back();
    anInfo.mWindow = aWindow.get();
    anInfo.mComponent = inDialog;
    anInfo.mInModal = true;
    anInfo.mOldRect = inDialog->GetBounds();
    if (this->mPopupWindows)
        aWindow->Show(true);
    int aResult = aWindow->RunDialog(inDialog);
    anInfo.mInModal = false;
    return aResult;
}
// FUNCTION: WON 0x7dcc70
// Runs an operation behind a status box and cancels it if the box closes first; killed if a box is already up
void DialogLogic::RunOpWithStatus(AsyncOp* inOp, unsigned long inUnusedTimeout,
                                const GUIString& inTitle, const GUIString& inStatus,
                                int inStatusMode, unsigned long inTimeout)
{
    if (this->mStatusVisible) {
        inOp->ForceFinish(WS_Killed);
        return;
    }
    this->mStatusOp = inOp;
    this->mStatusCancelled = false;
    this->RunOp(this->mStatusOp.get(), new LogicCompletion<DialogLogic>(&DialogLogic::StatusOpComplete, this), inTimeout);
    this->mStatusMode = inStatusMode;
    this->ShowStatus(inTitle, inStatus, true);
    if (this->mStatusOp->GetStatus() == WS_Pending) {
        this->mStatusCancelled = true;
        this->mStatusOp->Kill(true);
    }
    this->mStatusOp = NULL;
}
// FUNCTION: WON 0x7dcdb0
// Shows the status box with a title and text (optionally with Cancel), unless one is up or the lobby is suspended
void DialogLogic::ShowStatus(const GUIString& inTitle, const GUIString& inStatus, bool inCancel)
{
    if (this->mStatusVisible || LobbyIsSuspended())
        return;
    this->mStatusVisible = true;
    this->mStatusDialog->SetText(inTitle, inStatus, inCancel);
    this->DoDialog(this->mStatusDialog.get());
    this->mStatusDialog->mStartTick = GetTickCount();
    this->mStatusVisible = false;
}
// FUNCTION: WON 0x7dce10
// Plays the error sound and shows an error box, unless a status box is already up
void DialogLogic::ShowError(const GUIString& inTitle, const GUIString& inStatus)
{
    if (this->mStatusVisible)
        return;
    if (LobbyGetLobbySoundEffects())
        LobbyPlaySound(::LobbyGlobal_Error_Sound.get());
    this->ShowStatus(inTitle, inStatus, false);
}
// FUNCTION: WON 0x7dce50
// Shows an error box with a WON status's text (no sound)
void DialogLogic::ShowError(const GUIString& inTitle, WONStatus inStatus)
{
    this->ShowStatus(inTitle, GetWONStatusString(inStatus), false);
}
// FUNCTION: WON 0x7dd050
// Asks an OK/Cancel question, optionally with the error sound; returns the button pressed
int DialogLogic::ShowOkCancel(const GUIString& inTitle, const GUIString& inText, bool inPlaySound)
{
    this->mOkCancelDialog->SetText(inTitle, inText, ::LobbyGlobal_OK_String, ::LobbyGlobal_Cancel_String);
    if (inPlaySound && LobbyGetLobbySoundEffects())
        LobbyPlaySound(::LobbyGlobal_Error_Sound.get());
    return this->DoDialog(this->mOkCancelDialog.get());
}
// FUNCTION: WON 0x7dd0a0
// Asks a Yes/No question, optionally with the error sound; returns the button pressed
int DialogLogic::ShowYesNo(const GUIString& inTitle, const GUIString& inText, bool inPlaySound)
{
    this->mOkCancelDialog->SetText(inTitle, inText, ::LobbyGlobal_Yes_String, ::LobbyGlobal_No_String);
    if (inPlaySound && LobbyGetLobbySoundEffects())
        LobbyPlaySound(::LobbyGlobal_Error_Sound.get());
    return this->DoDialog(this->mOkCancelDialog.get());
}
// FUNCTION: WON 0x7dc7e0
// Returns a dialog's window: the lobby window, the top popup when reused, or a new popup owned by the top one
SmartPtr<Window> DialogLogic::GetDialogWindow(bool inReuse)
{
    if (!this->mPopupWindows)
        return LobbyGetRootWindow();
    if (inReuse && !this->mDialogStack.empty())
        return this->mDialogStack.back().mWindow;
    SmartPtr<Window> aWindow(new MSWindow);
    aWindow->mIsOneComponentWindow = true;
    if (!this->mDialogStack.empty())
        aWindow->SetParent(this->mDialogStack.back().mWindow.get());
    else
        aWindow->SetParent(LobbyGetRootWindow());
    aWindow->mStyle = 0x22;
    aWindow->Create(WONRectangle(0, 0, 100, 100));
    LobbyAddLogicListener(aWindow->mRoot.get());
    aWindow->mRoot->SetColorScheme(LobbyGetColorScheme());
    return aWindow;
}
// FUNCTION: WON 0x7dd0f0
// Removes every dialog that is not running
void DialogLogic::Reset() { this->ClearDialogs(); }
}

// UpdateStatus: the original also constructs and destroys an empty std::wstring that nothing uses
namespace WONAPI {
// FUNCTION: WON 0x007dced0
// Shows an operation's result in the status box (error sound on failure) and re-centers the box if it resized
void DialogLogic::UpdateStatus(WONStatus inStatus)
{
    WONRectangle anOldBounds = this->mStatusDialog->GetBounds();
    int anOldWidth = anOldBounds.Width();
    int anOldHeight = anOldBounds.Height();
    std::wstring aText; // constructed and destroyed; no statement reads it
    this->mStatusDialog->SetStatus(GetWONStatusString(inStatus), false);
    if (inStatus != 0 && LobbyGetLobbySoundEffects())
        LobbyPlaySound(::LobbyGlobal_Error_Sound.get());
    if (anOldWidth == this->mStatusDialog->Width() && anOldHeight == this->mStatusDialog->Height())
        return;
    if (this->mPopupWindows) {
        Window* aWindow = this->mStatusDialog->GetRootWindow();
        if (aWindow == NULL)
            return;
        aWindow->SetClientSize(this->mStatusDialog->Width(), this->mStatusDialog->Height());
        aWindow->Center(true);
    }
    else {
        Container* aRoot = GetLobbyRoot();
        if (aRoot != NULL) {
            CenterLayout::Center(aRoot, this->mStatusDialog.get(), true, true);
            WONRectangle aBounds(this->mStatusDialog->GetBounds());
            aRoot->InvalidateRect(aBounds);
        }
    }
}
}
// TEMPLATE: WON 0x007f47c0

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007dc460
// (public: virtual void * __thiscall WONAPI::DialogLogic::`scalar deleting destructor'(unsigned int)    [??_GDialogLogic@WONAPI@@UAEPAXI@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007f4750
// (void __cdecl std::_Construct(struct WONAPI::DialogLogic::DialogInfo *,struct WONAPI::DialogLogic::DialogInfo const &)    [?_Construct@std@@YAXPAUDialo)
// TEMPLATE: WON 0x007f33d0
// TEMPLATE: WON 0x007f3550

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007f1300
// (public: virtual void __thiscall WONAPI::LogicCompletion<class WONAPI::DialogLogic>::Run(class WONAPI::AsyncOp *)    [?Run@?$LogicCompletion@VDialogLog)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007dcbf0
// (public: __thiscall WONAPI::DialogLogic::DialogInfo::~DialogInfo(void)    [??1DialogInfo@DialogLogic@WONAPI@@QAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007f12a0
// (public: __thiscall std::list<struct WONAPI::DialogLogic::DialogInfo,class std::allocator<struct WONAPI::DialogLogic::DialogInfo> >::~list<struct WONAP)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007f1410
// (public: virtual void * __thiscall WONAPI::LogicCompletion<class WONAPI::DialogLogic>::`scalar deleting destructor'(unsigned int)    [??_G?$LogicComple)
// TEMPLATE: WON 0x007f14b0
// (public: virtual __thiscall WONAPI::LogicCompletion<class WONAPI::DialogLogic>::~LogicCompletion<class WONAPI::DialogLogic>(void)    [??1?$LogicComplet)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007f1690
// (public: void __thiscall std::list<struct WONAPI::DialogLogic::DialogInfo,class std::allocator<struct WONAPI::DialogLogic::DialogInfo> >::_Insert(class)
// TEMPLATE: WON 0x007f1f50
// (protected: void __thiscall std::list<struct WONAPI::DialogLogic::DialogInfo,class std::allocator<struct WONAPI::DialogLogic::DialogInfo> >::_Incsize(u)
