
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
#include "ee/wonapi/WONLobbyMSSkinImage.h"
#include "ee/wonapi/WONLobbySkinnedContainers.h"
#include "ee/wonapi/WONLobbySkinTab.h"
namespace WONAPI {
// FUNCTION: WON 0x00717ae0
// Returns a copy of the simple button: state images, scaling, mouse-down check, attributes and size
WONSimpleButton* WONSimpleButton::Duplicate()
{
    WONSimpleButton* button=new WONSimpleButton;
    for(int i=0;i<4;++i) button->mImages[i]=this->mImages[i];
    button->mScaleImage=this->mScaleImage;
    button->mCheckMouseButtonDown=this->mCheckMouseButtonDown;
    button->CopyAttributes(this);
    button->SetSize(this->Width(),this->Height());
    return button;
}
// FUNCTION: WON 0x007199e0
// Returns a new skinned combo box with the same frame and frame options
WONComboBox* WONComboBox::Duplicate()
{
    WONComboBox* combo=new WONComboBox;
    combo->SetFrame(this->mFrame);
    combo->mDrawFrame=this->mDrawFrame;
    combo->mButtonInFrame=this->mButtonInFrame;
    return combo;
}
// FUNCTION: WON 0x00719f00
// Returns a new skinned scroll container with the same attributes
WONScrollContainer* WONScrollContainer::Duplicate()
{
    WONScrollContainer* container=new WONScrollContainer;
    container->CopyAttributes(this);
    return container;
}
// FUNCTION: WON 0x0071ad20
// Returns a new skinned tab control on the same skin with the same attributes, transparency and frame
WONTabCtrl* WONTabCtrl::Duplicate()
{
    WONTabCtrl* tab=new WONTabCtrl(this->mSkin.get());
    tab->CopyAttributes(this);
    tab->SetTransparent(this->mIsTransparent);
    tab->SetFrame(this->mFrame);
    return tab;
}
}
