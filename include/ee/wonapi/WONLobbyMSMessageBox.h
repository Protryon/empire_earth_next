#ifndef EE_WONAPI_WONLOBBYMSMESSAGEBOX_H
#define EE_WONAPI_WONLOBBYMSMESSAGEBOX_H
#include "ee/wonapi/WONLobbyMSRemainingPainters.h"
#include "ee/wonapi/WONLobbyLayout.h"
namespace WONAPI {
// MSDialog: fieldless Dialog (0x98 bytes), constructor at 0x00700550, RTTI 0x0084ea4c; implicit destructor.
class MSDialog : public Dialog {
public:
    MSDialog(); // at 0x00700550; no extra fields or virtual entries
};
// MSMessageBox (0xb0 bytes): Msg (0x00701fd0) allocates it and calls 0x007016f0.
// mCreateFlags (+0xac) is left uninitialized.
class MSMessageBox : public MSDialog {
protected:
    SmartPtr<MSButton> mButton1,mButton2,mButton3; // offsets 0x98, 0x9c, 0xa0 (0x60-byte buttons)
    SmartPtr<MSLabel> mLabel; //a4, real complete fieldless5c child
    GUIString mTitle; //a8
    int mCreateFlags; // offset 0xac; not read by 0x007016f0
public:
    MSMessageBox(const GUIString&,const GUIString&,const GUIString&,const GUIString&,const GUIString&); // at 0x007016f0
    static int Msg(Window*,const GUIString&,const GUIString&,const GUIString&,const GUIString&,const GUIString&); //301fd0, cdecl plain RET, six arguments
    SmartPtr<Window> PrepareWindow(Window*); // at 0x00702090 (returns the SmartPtr through a hidden pointer)
    int DoDialog(Window*); // at 0x007021b0
    // The destructor (0x00701ea0) is implicit member and base cleanup.
};
}
#endif
