// Microsoft-style default skin: the template controls built from drawn state images (file name inferred)
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
#include "ee/wonapi/WONLobbySkinListControls.h"
#include "ee/wonapi/WONLobbyWONListConfig.h"
#include "ee/wonapi/WONLobbySkinTab.h"
#include "ee/wonapi/WONLobbyWONInputBox.h"
namespace WONAPI {
// FUNCTION: WON 0x00715d70
// Creates the Windows-look skin: template buttons, arrows, scrollbars, frames and backgrounds drawn as MS images
// (the tab button's checked image replaces its normal one: both go to state 0 without the checked flag)
WONMSSkin::WONMSSkin()
{
	SmartPtr<Font> aFont = WONAPI::WindowManager::GetDefaultWindowManager()->GetNamedFont(std::string("MSButton"));

	// Push Button
	this->mPushButton = new WONButton;
	this->mPushButton->SetButtonFlags(0x0001,false);
	this->mPushButton->SetButtonFlags(0x0400,true);
	this->mPushButton->SetDrawGrayText(true);
	this->mPushButton->SetButtonState(0,new MSImage(MSImageType_PushButton,0),-1,aFont.get());
	this->mPushButton->SetButtonState(1,new MSImage(MSImageType_PushButton,MSImageFlag_Down),-1,NULL);
	this->mPushButton->SetButtonState(4,new MSImage(MSImageType_PushButton,MSImageFlag_Default),-1,NULL);

	// Checkbox

	this->mCheckbox = new WONButton;
	this->mCheckbox->SetPushOffsets(0,0);
	this->mCheckbox->SetHasCheck(true);
	this->mCheckbox->SetScaleImage(false);
	this->mCheckbox->SetTextPadding(13+4,2,2,2);
	this->mCheckbox->SetButtonState(0,new MSImage(MSImageType_Checkbox,0,13,13),-1,aFont.get());
	this->mCheckbox->SetButtonState(1,new MSImage(MSImageType_Checkbox,MSImageFlag_Down,13,13),-1,NULL);
	this->mCheckbox->SetButtonState(0,new MSImage(MSImageType_Checkbox,MSImageFlag_Checked,13,13),-1,NULL,true);
	this->mCheckbox->SetButtonState(1,new MSImage(MSImageType_Checkbox,MSImageFlag_Checked | MSImageFlag_Down,13,13),-1,NULL,true);

	// RadioButton

	this->mRadioButton = new WONButton;
	this->mRadioButton->SetButtonFlags(0x0008,true);
	this->mRadioButton->SetPushOffsets(0,0);
	this->mRadioButton->SetHasCheck(true);
	this->mRadioButton->SetScaleImage(false);
	this->mRadioButton->SetTextPadding(13+4,2,2,2);
	this->mRadioButton->SetButtonState(0,new MSImage(MSImageType_RadioButton,0,13,13),-1,aFont.get());
	this->mRadioButton->SetButtonState(1,new MSImage(MSImageType_RadioButton,MSImageFlag_Down,13,13),-1,NULL);
	this->mRadioButton->SetButtonState(0,new MSImage(MSImageType_RadioButton,MSImageFlag_Checked,13,13),-1,NULL,true);
	this->mRadioButton->SetButtonState(1,new MSImage(MSImageType_RadioButton,MSImageFlag_Checked | MSImageFlag_Down,13,13),-1,NULL,true);

	// HeaderButton
	this->mHeaderButton = new WONHeaderButton;
	this->mHeaderButton->SetScaleImage(true);
	this->mHeaderButton->SetButtonState(0, new MSImage(MSImageType_ListHeader,0),-1,aFont.get());
	this->mHeaderButton->SetButtonState(1, new MSImage(MSImageType_ListHeader,MSImageFlag_Down),-1,aFont.get());

	// Tab Button
	this->mTabButton = new WONButton;
	this->mTabButton->SetScaleImage(true);
	this->mTabButton->SetHasCheck(true);
	this->mTabButton->SetPushOffsets(0,0);
	this->mTabButton->SetButtonState(0, new MSImage(MSImageType_TabButton,0),-1,aFont.get());
	this->mTabButton->SetButtonState(0, new MSImage(MSImageType_TabButton,MSImageFlag_Checked),-1,aFont.get());

	// Close Button
	this->mCloseButton = new WONSimpleButton;
	this->mCloseButton->SetImage(0, new MSImage(MSImageType_CloseButton,0,16,14));
	this->mCloseButton->SetImage(1, new MSImage(MSImageType_CloseButton,MSImageFlag_Down,16,14));

	// Arrow Buttons
	int i;
	for(i=0; i<4; i++)
	{
		this->mArrowButton[i] = new WONSimpleButton;
		this->mArrowButton[i]->SetImage(0,new MSImage(MSImageType_LeftArrow+i,0,15,15));
		this->mArrowButton[i]->SetImage(1,new MSImage(MSImageType_LeftArrow+i,MSImageFlag_Down,15,15));
		this->mArrowButton[i]->SetImage(2,new MSImage(MSImageType_LeftArrow+i,MSImageFlag_Disabled,15,15));
	}

	// Scrollbar, ScrollThumb, and ScrollGutter
	for(i=0; i<2; i++)
	{
		this->mScrollbar[i] = new WONScrollbar;

		this->mScrollThumb[i] = new WONScrollThumb;
		this->mScrollThumb[i]->SetImage(0,new MSImage(MSImageType_ScrollThumb,0));
		this->mScrollThumb[i]->SetScaleImage(true);

		this->mScrollGutter[i] = new WONSimpleButton;
		this->mScrollGutter[i]->SetScaleImage(true);
		this->mScrollGutter[i]->SetImage(0,new MSImage(MSImageType_ScrollGutter,0));
	}

	// ComboBox Button
	this->mComboButton = new WONSimpleButton;
	this->mComboButton->SetScaleImage(true);
	this->mComboButton->SetImage(0,new MSImage(MSImageType_DownArrow,0,16,18));
	this->mComboButton->SetImage(1,new MSImage(MSImageType_DownArrow,MSImageFlag_Down,16,18));
	this->mComboButton->SetImage(2,new MSImage(MSImageType_DownArrow,MSImageFlag_Disabled,16,18));

	this->mPopupBack = new WONFrameComponent;
	this->mPopupBack->SetFrameImage(new MSImage(MSImageType_ScrollThumb,0));

	this->mDialogBack = new WONFrameComponent;
	this->mDialogBack->SetFrameImage(new MSImage(MSImageType_ScrollThumb,0));

	this->mInputBox = new WONInputBox;
	this->mComboBox = new WONComboBox;
	this->mScrollContainer = new WONScrollContainer;
	this->mListCtrl = new WONListCtrl;
	this->mTabCtrl = new WONTabCtrl;
	this->mTabCtrl->SetFrame(WONFrame(new MSImage(MSImageType_TabCtrlFrame),2));

	this->mInputBox->SetFrame(WONFrame(new MSImage(MSImageType_Frame3,MSImageFlag_Down),2));
	this->mComboBox->SetFrame(WONFrame(new MSImage(MSImageType_Frame3,MSImageFlag_Down),2));
	this->mScrollContainer->SetFrame(WONFrame(new MSImage(MSImageType_Frame3,MSImageFlag_Down),2));
	this->mScrollContainer->SetCorner(new MSImage(MSImageType_ScrollCorner,0));
	this->mListCtrl->CopyAttributes(this->mScrollContainer.get());
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00717100
// (public: virtual __thiscall WONAPI::WONMSSkin::~WONMSSkin(void)    [??1WONMSSkin@WONAPI@@UAE@XZ])
// TEMPLATE: WON 0x0071bc10
// (public: __thiscall std::map<class WONAPI::GUIString,class WONAPI::SmartPtr<class WONAPI::WONButton>,struct WONAPI::GUIStringLessNoCase,class std::allo)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007170e0
// (public: virtual void * __thiscall WONAPI::WONMSSkin::`scalar deleting destructor'(unsigned int)    [??_GWONMSSkin@WONAPI@@UAEPAXI@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00725040
// (public: void * __thiscall std::pair<class WONAPI::GUIString const ,class WONAPI::SmartPtr<class WONAPI::WONButton> >::`scalar deleting destructor'(uns)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00722a50
// (public: class std::_Tree<class WONAPI::GUIString,struct std::pair<class WONAPI::GUIString const ,class WONAPI::SmartPtr<class WONAPI::WONButton> >,str)
// TEMPLATE: WON 0x00722f70
// (protected: void __thiscall std::_Tree<class WONAPI::GUIString,struct std::pair<class WONAPI::GUIString const ,class WONAPI::SmartPtr<class WONAPI::WON)
