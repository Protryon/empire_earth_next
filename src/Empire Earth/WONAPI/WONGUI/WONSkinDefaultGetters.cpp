// Skin getters that duplicate the template controls and backgrounds (file name inferred)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyMSSkinImage.h"
#include "ee/wonapi/WONLobbyWONInputBox.h"
#include "ee/wonapi/WONLobbySkinListControls.h"
namespace WONAPI {
// Returns a copy of the skin's close button
WONSimpleButton* WONSkin::GetCloseButton()
{
    return this->mCloseButton->Duplicate();
}
// Returns a copy of the skin's input box
WONInputBox* WONSkin::GetInputBox()
{
    return this->mInputBox->Duplicate();
}
// Returns a copy of the skin's push button
WONButton* WONSkin::GetPushButton()
{
    return this->mPushButton->Duplicate();
}
// Returns a copy of the skin's checkbox
WONButton* WONSkin::GetCheckbox()
{
    return this->mCheckbox->Duplicate();
}
// Returns a copy of the skin's radio button
WONButton* WONSkin::GetRadioButton()
{
    return this->mRadioButton->Duplicate();
}
// Returns a copy of the skin's list header button
WONHeaderButton* WONSkin::GetHeaderButton()
{
    return this->mHeaderButton->Duplicate();
}
// Returns a new frame component with a copy of the skin's popup background frame
WONFrameComponent* WONSkin::GetPopupBack()
{
    WONFrameComponent* component=new WONFrameComponent(this->mPopupBack->GetFrame());
    return component;
}
// Returns a new frame component with a copy of the skin's dialog background frame
WONFrameComponent* WONSkin::GetDialogBack()
{
    WONFrameComponent* component=new WONFrameComponent(this->mDialogBack->GetFrame());
    return component;
}
}

#include "ee/wonapi/WONLobbySkinTab.h"
#include "ee/wonapi/WONLobbyWONListConfig.h"
namespace WONAPI {
// Destroys the skin, releasing its template controls and button type maps
WONSkin::~WONSkin() {}
}
