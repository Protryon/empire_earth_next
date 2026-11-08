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
namespace WONAPI {
// FUNCTION: WON 0x00798f90
// Reads an orientation word; true for "Vert" (any case), false for anything else
bool WONSkinConfig::ReadVert()
{
    GUIString text=this->ReadString(false);
    return text.CompareNoCase("Vert")==0;
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
	if(theInstruction=="WINSKIN")
	{

	}
	else if(theInstruction=="ADDBUTTONTYPE")
	{
		GUIString aName = this->ReadString(false); this->EnsureComma();
		WONButton* aButton = SafeConfigGetComponent<WONButton*>(this->ReadComponent(),"WONButton");
		gSkin->mButtonTypeMap[aName] = aButton;
	}
	else if(theInstruction=="ADDSIMPLEBUTTONTYPE")
	{
		GUIString aName = this->ReadString(false); this->EnsureComma();
		WONSimpleButton* aButton = SafeConfigGetComponent<WONSimpleButton*>(this->ReadComponent(),"WONSimpleButton");
		gSkin->mSimpleButtonTypeMap[aName] = aButton;
	}
	else if(theInstruction=="SETPUSHBUTTON")
		gSkin->mPushButton = SafeConfigGetComponent<WONButton*>(this->ReadComponent(),"WONButton");
	else if(theInstruction=="SETCHECKBOX")
		gSkin->mCheckbox = SafeConfigGetComponent<WONButton*>(this->ReadComponent(),"WONButton");
	else if(theInstruction=="SETRADIOBUTTON")
	{
		gSkin->mRadioButton = SafeConfigGetComponent<WONButton*>(this->ReadComponent(),"WONButton");
		gSkin->mRadioButton->SetButtonFlags(8,true);
	}
	else if(theInstruction=="SETHEADERBUTTON")
		gSkin->mHeaderButton = SafeConfigGetComponent<WONHeaderButton*>(this->ReadComponent(),"WONHeaderButton");
	else if(theInstruction=="SETTABBUTTON")
		gSkin->mTabButton = SafeConfigGetComponent<WONButton*>(this->ReadComponent(),"WONButton");
	else if(theInstruction=="SETARROWBUTTON")
	{
		Direction aDirection = this->ReadDirection(); this->EnsureComma();
		gSkin->mArrowButton[aDirection] = SafeConfigGetComponent<WONSimpleButton*>(this->ReadComponent(),"WONSimpleButton");

	}
	else if(theInstruction=="SETCOMBOBUTTON")
		gSkin->mComboButton = SafeConfigGetComponent<WONSimpleButton*>(this->ReadComponent(),"WONSimpleButton");
	else if(theInstruction=="SETCLOSEBUTTON")
		gSkin->mCloseButton = SafeConfigGetComponent<WONSimpleButton*>(this->ReadComponent(),"WONSimpleButton");
	else if(theInstruction=="SETSCROLLGUTTER")
	{
		bool isVert = this->ReadVert(); this->EnsureComma();
		gSkin->mScrollGutter[isVert?1:0] = SafeConfigGetComponent<WONSimpleButton*>(this->ReadComponent(),"WONSimpleButton");
	}
	else if(theInstruction=="SETSCROLLTHUMB")
	{
		bool isVert = this->ReadVert(); this->EnsureComma();
		gSkin->mScrollThumb[isVert?1:0] = SafeConfigGetComponent<WONSimpleButton*>(this->ReadComponent(),"WONSimpleButton");
	}
	else if(theInstruction=="SETSCROLLBAR")
	{
		bool isVert = this->ReadVert(); this->EnsureComma();
		gSkin->mScrollbar[isVert?1:0] = SafeConfigGetComponent<WONScrollbar*>(this->ReadComponent(),"WONScrollbar");
	}
	else if(theInstruction=="SETSCROLLCONTAINER")
		gSkin->mScrollContainer = SafeConfigGetComponent<WONScrollContainer*>(this->ReadComponent(),"WONScrollContainer");
	else if(theInstruction=="SETLISTCTRL")
		gSkin->mListCtrl = SafeConfigGetComponent<WONListCtrl*>(this->ReadComponent(),"WONListCtrl");
	else if(theInstruction=="SETTABCTRL")
	{
		gSkin->mTabCtrl = SafeConfigGetComponent<WONTabCtrl*>(this->ReadComponent(),"WONTabCtrl");
		gSkin->mTabCtrl->mSkin=NULL;
	}
	else if(theInstruction=="SETCOMBOBOX")
		gSkin->mComboBox = SafeConfigGetComponent<WONComboBox*>(this->ReadComponent(),"WONComboBox");
	else if(theInstruction=="SETPOPUPBACK")
		gSkin->mPopupBack = SafeConfigGetComponent<WONFrameComponent*>(this->ReadComponent(),"WONPopupBack");
	else if(theInstruction=="SETDIALOGBACK")
		gSkin->mDialogBack = SafeConfigGetComponent<WONFrameComponent*>(this->ReadComponent(),"WONDialogBack");
	else if(theInstruction=="BUTTONCLICKSOUND")
		this->HandleButtonClickSound();
	else if(theInstruction=="IMAGECURSOR")
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
