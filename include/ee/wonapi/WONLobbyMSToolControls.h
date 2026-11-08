#ifndef EE_WONAPI_WONLOBBYMSTOOLCONTROLS_H
#define EE_WONAPI_WONLOBBYMSTOOLCONTROLS_H
#include "ee/wonapi/WONLobbyGUI.h"
namespace WONAPI {
// Classes with no data members of their own (not identified in the original exe).
class MSScrollButtonContainer : public ButtonScroller {
public:
    MSScrollButtonContainer(ScrollArea*);
};
class MSToolTip : public Label {
public:
    MSToolTip(const GUIString&);
};
class MSToolButton : public MSButtonBase {
public:
    virtual void Paint(Graphics&);
    MSToolButton(const GUIString&);
};
}
#endif
