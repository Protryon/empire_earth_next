// Skin-file instructions that replace the global skin's template controls and set image cursors (file name inferred)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include <set>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyWONButtonConfig.h"
#include "ee/wonapi/WONLobbyWONSkinConfig.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyMSExtraConfig.h"
#include "ee/wonapi/WONLobbyWONListConfig.h"
#include "rust/ee_won.h"
// The instruction keys and the flag and value names this unit compares are ee_won's guiconf tables
// (rust/crates/ee_won/src/guiconf.rs); this unit keeps the reads, the objects and what each entry does.
namespace WONAPI {
// FUNCTION: WON 0x00798f90
// Reads an orientation word; true for "Vert" (any case), false for anything else
bool WONSkinConfig::ReadVert()
{
    GUIString text=this->ReadString(false);
    const int aNameIdx_text=ee_won_gui_cfg_name(EE_WON_CFG_NAMES_WONSKINCONFIG_READVERT_TEXT,text.mData->mText,text.mData->mLength,sizeof(wchar_t));
    return aNameIdx_text==0;
}
// FUNCTION: WON 0x00799010
// Runs the base pre-parse and creates the global skin as the Windows-look skin when there is none
bool WONSkinConfig::PreParse(ConfigParser* parser)
{
    if(!ComponentConfig::PreParse(parser))
        return false;
    if(gSkin.get()==NULL)
        gSkin=new WONMSSkin;
    return true;
}
// FUNCTION: WON 0x007990e0
// Reads a sound and sets it as the click sound of the skin's template buttons and every named button type
void WONSkinConfig::HandleButtonClickSound()
{
    SmartPtr<Sound> sound=this->ReadSound();
    gSkin->mPushButton->SetClickSound(sound.get());
    gSkin->mCheckbox->SetClickSound(sound.get());
    gSkin->mRadioButton->SetClickSound(sound.get());
    gSkin->mHeaderButton->SetClickSound(sound.get());
    gSkin->mTabButton->SetClickSound(sound.get());
    gSkin->mCloseButton->SetClickSound(sound.get());
    gSkin->mComboButton->SetClickSound(sound.get());
    WONSkin::ButtonTypeMap::iterator it=gSkin->mButtonTypeMap.begin();
    for(;it!=gSkin->mButtonTypeMap.end();++it)
        it->second->SetClickSound(sound.get());
}
// FUNCTION: WON 0x00799210
// Handles the skin keys that replace the global skin's template controls, add named button types, set the
// click sound and image cursors; false for other keys (WINSKIN is accepted and ignored)
bool WONSkinConfig::Virtual24(const std::string &theInstruction)
{
	const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_WONSKINCONFIG_VIRTUAL24_THEINSTRUCTION,theInstruction.data(),theInstruction.size());
	if(aKeyIdx==0)
	{

	}
	else if(aKeyIdx==1)
	{
		GUIString aName = this->ReadString(false); this->EnsureComma();
		WONButton* aButton = SafeConfigGetComponent<WONButton*>(this->ReadComponent(),"WONButton");
		gSkin->mButtonTypeMap[aName] = aButton;
	}
	else if(aKeyIdx==2)
	{
		GUIString aName = this->ReadString(false); this->EnsureComma();
		WONSimpleButton* aButton = SafeConfigGetComponent<WONSimpleButton*>(this->ReadComponent(),"WONSimpleButton");
		gSkin->mSimpleButtonTypeMap[aName] = aButton;
	}
	else if(aKeyIdx==3)
		gSkin->mPushButton = SafeConfigGetComponent<WONButton*>(this->ReadComponent(),"WONButton");
	else if(aKeyIdx==4)
		gSkin->mCheckbox = SafeConfigGetComponent<WONButton*>(this->ReadComponent(),"WONButton");
	else if(aKeyIdx==5)
	{
		gSkin->mRadioButton = SafeConfigGetComponent<WONButton*>(this->ReadComponent(),"WONButton");
		gSkin->mRadioButton->SetButtonFlags(8,true);
	}
	else if(aKeyIdx==6)
		gSkin->mHeaderButton = SafeConfigGetComponent<WONHeaderButton*>(this->ReadComponent(),"WONHeaderButton");
	else if(aKeyIdx==7)
		gSkin->mTabButton = SafeConfigGetComponent<WONButton*>(this->ReadComponent(),"WONButton");
	else if(aKeyIdx==8)
	{
		Direction aDirection = this->ReadDirection(); this->EnsureComma();
		gSkin->mArrowButton[aDirection] = SafeConfigGetComponent<WONSimpleButton*>(this->ReadComponent(),"WONSimpleButton");

	}
	else if(aKeyIdx==9)
		gSkin->mComboButton = SafeConfigGetComponent<WONSimpleButton*>(this->ReadComponent(),"WONSimpleButton");
	else if(aKeyIdx==10)
		gSkin->mCloseButton = SafeConfigGetComponent<WONSimpleButton*>(this->ReadComponent(),"WONSimpleButton");
	else if(aKeyIdx==11)
	{
		bool isVert = this->ReadVert(); this->EnsureComma();
		gSkin->mScrollGutter[isVert?1:0] = SafeConfigGetComponent<WONSimpleButton*>(this->ReadComponent(),"WONSimpleButton");
	}
	else if(aKeyIdx==12)
	{
		bool isVert = this->ReadVert(); this->EnsureComma();
		gSkin->mScrollThumb[isVert?1:0] = SafeConfigGetComponent<WONSimpleButton*>(this->ReadComponent(),"WONSimpleButton");
	}
	else if(aKeyIdx==13)
	{
		bool isVert = this->ReadVert(); this->EnsureComma();
		gSkin->mScrollbar[isVert?1:0] = SafeConfigGetComponent<WONScrollbar*>(this->ReadComponent(),"WONScrollbar");
	}
	else if(aKeyIdx==14)
		gSkin->mScrollContainer = SafeConfigGetComponent<WONScrollContainer*>(this->ReadComponent(),"WONScrollContainer");
	else if(aKeyIdx==15)
		gSkin->mListCtrl = SafeConfigGetComponent<WONListCtrl*>(this->ReadComponent(),"WONListCtrl");
	else if(aKeyIdx==16)
	{
		gSkin->mTabCtrl = SafeConfigGetComponent<WONTabCtrl*>(this->ReadComponent(),"WONTabCtrl");
		gSkin->mTabCtrl->mSkin=NULL;
	}
	else if(aKeyIdx==17)
		gSkin->mComboBox = SafeConfigGetComponent<WONComboBox*>(this->ReadComponent(),"WONComboBox");
	else if(aKeyIdx==18)
		gSkin->mPopupBack = SafeConfigGetComponent<WONFrameComponent*>(this->ReadComponent(),"WONPopupBack");
	else if(aKeyIdx==19)
		gSkin->mDialogBack = SafeConfigGetComponent<WONFrameComponent*>(this->ReadComponent(),"WONDialogBack");
	else if(aKeyIdx==20)
		this->HandleButtonClickSound();
	else if(aKeyIdx==21)
	{
		StandardCursor aType = this->ReadStandardCursor(); this->EnsureComma();
		SmartPtr<Image> anImage = this->ReadImage(false);
		SmartPtr<Cursor> aCursor = new ComponentCursor(new ImageComponent(anImage.get()));
		Cursor::SetStandardCursor(aType,aCursor.get());
	}
	else
		return false;

	return true;
}


// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007926a0
// (public: __thiscall WONAPI::ConfigObjectException::ConfigObjectException(class std::basic_string<char,struct std::char_traits<char>,class std::allocato)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0079c9e0
// (public: struct std::pair<class std::_Tree<class WONAPI::GUIString,struct std::pair<class WONAPI::GUIString const ,class WONAPI::SmartPtr<class WONAPI:)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0079cae0
// (public: struct std::pair<class std::_Tree<class WONAPI::GUIString,struct std::pair<class WONAPI::GUIString const ,class WONAPI::SmartPtr<class WONAPI:)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007a1560
// TEMPLATE: WON 0x007a1860
// TEMPLATE: WON 0x007a2ef0
// (void __cdecl std::_Construct(struct std::pair<class WONAPI::GUIString const ,class WONAPI::SmartPtr<class WONAPI::WONButton> > *,struct std::pair<clas)

// Compiler-generated and header-inline functions this unit emits:
}
