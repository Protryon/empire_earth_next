// Resource configuration of simple (image-only) and skinned buttons
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
#include "ee/wonapi/WONLobbyWONButtonConfig.h"
namespace WONAPI {

// FUNCTION: WON 0x007971c0
// Reads a BUTTONSTATE line (Normal, Pressed, Disabled or Over, and an image) and sets that image; throws on a bad state
void WONSimpleButtonConfig::HandleButtonState()
{
	GUIString aStateStr = this->ReadString(false);
	this->EnsureComma();

	int aState = 0;
	if(aStateStr.CompareNoCase("Normal")==0) aState = 0;
	else if(aStateStr.CompareNoCase("Pressed")==0) aState = 1;
	else if(aStateStr.CompareNoCase("Disabled")==0) aState = 2;
	else if(aStateStr.CompareNoCase("Over")==0) aState = 3;
	else
		throw ConfigObjectException("Invalid button state: " + (std::string)aStateStr);

	SmartPtr<Image> anImage = this->ReadImage(false);
	this->mButton->SetImage(aState,anImage.get());
}

// FUNCTION: WON 0x007973f0
// Reads up to four comma-separated images into the normal, pressed, disabled and over states
void WONSimpleButtonConfig::HandleButtonStates()
{	
	for(int i=0; i<4; i++)
	{
		this->mButton->SetImage(i,this->ReadImage(false).get()); 
		if(this->EndOfString())
			return;

		this->EnsureComma();
	}
}

// FUNCTION: WON 0x007974e0
// Handles the simple button keys SCALEIMAGE, TYPE (copy a skin type; throws if unknown), BUTTONSTATE(S)
// and CHECKMOUSEBUTTONDOWN; other keys go to the button config
bool WONSimpleButtonConfig::Virtual24(const std::string &theInstruction)
{
	if(theInstruction=="SCALEIMAGE")
		this->mButton->SetScaleImage(this->ReadBool());
	else if(theInstruction=="TYPE")
	{
		GUIString aType = this->ReadString(false);
		if(!gSkin->CopySimpleButtonType(aType,this->mButton))
			throw ConfigObjectException("Type not found: " + std::string(aType));
	}
	else if(theInstruction=="BUTTONSTATE")
		this->HandleButtonState();
	else if(theInstruction=="BUTTONSTATES")
		this->HandleButtonStates();
	else if(theInstruction=="CHECKMOUSEBUTTONDOWN")
		this->mButton->SetCheckMouseButtonDown(this->ReadBool());
	else
		return ButtonConfig::Virtual24(theInstruction);

	return true;
}

// FUNCTION: WON 0x00797740
// Reads a BUTTONSTATE line: a (Checked)Normal/Pressed/Disabled/Over/Default state, an image and an optional
// colour and font; throws on an unknown state
void WONButtonConfig::HandleSetButtonState()
{
	GUIString aStateStr = this->ReadString(false);
	this->EnsureComma();

	int aState = 0;
	bool checked = false;
	if(aStateStr.CompareNoCase("Normal")==0) aState = 0;
	else if(aStateStr.CompareNoCase("Pressed")==0) aState = 1;
	else if(aStateStr.CompareNoCase("Disabled")==0) aState = 2;
	else if(aStateStr.CompareNoCase("Over")==0) aState = 3;
	else if(aStateStr.CompareNoCase("Default")==0) aState = 4;
	else if(aStateStr.CompareNoCase("CheckedNormal")==0) { aState = 0; checked = true; }
	else if(aStateStr.CompareNoCase("CheckedPressed")==0) { aState = 1; checked = true; }
	else if(aStateStr.CompareNoCase("CheckedDisabled")==0) { aState = 2; checked = true; }
	else if(aStateStr.CompareNoCase("CheckedOver")==0) { aState = 3; checked = true; }
	else if(aStateStr.CompareNoCase("CheckedDefault")==0) { aState = 4; checked = true; }
	else
		throw ConfigObjectException("Invalid button state: " + (std::string)aStateStr);

	SmartPtr<Image> anImage = this->ReadImage(false);
	int aColor = -1;
	SmartPtr<Font> aFont;
	if(!this->EndOfString())
	{
		this->EnsureComma();
		aColor = this->ReadColor();
		if(!this->EndOfString())
		{
			this->EnsureComma();
			aFont = this->ReadFont();
		}
	}

	this->mButton->SetButtonState(aState,anImage.get(),aColor,aFont.get(),checked);
}

// FUNCTION: WON 0x00797aa0
// Handles the skinned button keys (font, type, colours, alignment, transparency, wrap, padding, button
// states, focus rectangle); the INCREMENT...TEXTPAD keys are accepted and ignored
bool WONButtonConfig::Virtual24(const std::string& theInstruction)
{
	if(theInstruction=="FONT")
		this->mButton->SetFont(this->ReadFont().get());
	else if(theInstruction=="TYPE")
	{
		GUIString aType = this->ReadString(false);
		if(!gSkin->CopyButtonType(aType,this->mButton))
			throw ConfigObjectException("Type not found: " + std::string(aType));
	}
	else if(theInstruction=="TEXTCOLOR")
		this->mButton->SetTextColor(this->ReadColor());
	else if(theInstruction=="DEFAULTTEXTCOLOR")
		this->mButton->SetDefaultTextColor(this->ReadColor());
	else if(theInstruction=="VERTALIGN")
		this->mButton->SetVertAlign(this->ReadVertAlign());
	else if(theInstruction=="HORZALIGN")
		this->mButton->SetHorzAlign(this->ReadHorzAlign());
	else if(theInstruction=="TRANSPARENT")
		this->mButton->SetTransparent(this->ReadBool());
	else if(theInstruction=="ADJUSTHEIGHTTOFITTEXT")
		this->mButton->SetAdjustHeightToFitText(this->ReadBool());
	else if(theInstruction=="WRAP")
		this->mButton->SetWrapText(this->ReadBool());
	else if(theInstruction=="DRAWGRAYTEXT")
		this->mButton->SetDrawGrayText(this->ReadBool());
	else if(theInstruction=="TEXTPADDING")
	{
		int left = this->ReadInt(); this->EnsureComma();
		int top = this->ReadInt(); this->EnsureComma();
		int right = this->ReadInt(); this->EnsureComma();
		int bottom = this->ReadInt();
		this->mButton->SetTextPadding(left,top,right,bottom);
	}
	else if(theInstruction=="FOCUSPADDING")
	{
		int left = this->ReadInt(); this->EnsureComma();
		int top = this->ReadInt(); this->EnsureComma();
		int right = this->ReadInt(); this->EnsureComma();
		int bottom = this->ReadInt();
		this->mButton->SetFocusPadding(left,top,right,bottom);
	}
	else if(theInstruction=="HORZTEXTPAD")
	{
		int left = this->ReadInt(); 
		int right = this->mButton->GetRightTextPad();
		if(!this->EndOfString())
		{
			this->EnsureComma(); 
			right = this->ReadInt();
		}
		this->mButton->SetHorzTextPad(left,right);
	}
	else if(theInstruction=="INCREMENTHORZTEXTPAD")
	{
/*		int left = ReadInt(); EnsureComma();
		int right = ReadInt();
		mButton->IncrementHorzTextPad(left,right);
*/	}
	else if(theInstruction=="INCREMENTVERTEXTPAD")
	{
/*		int top = ReadInt(); EnsureComma();
		int bottom = ReadInt();
		mButton->IncrementVertTextPad(top,bottom);
*/	}
	else if(theInstruction=="BUTTONSTATE")
		this->HandleSetButtonState();
	else if(theInstruction=="SCALEIMAGE")
		this->mButton->SetScaleImage(this->ReadBool());
	else if(theInstruction=="DRAWFOCUSRECT")
		this->mButton->SetFocusStyle(this->ReadBool()?1:0);
	else
		return ButtonConfig::Virtual24(theInstruction);

	return true;
}
}

namespace WONAPI {
// FUNCTION: WON 0x00796a20
// Binds the simple button config to its button
void WONSimpleButtonConfig::SetComponent(Component* component) { this->mButton=static_cast<WONSimpleButton*>(component); ButtonConfig::SetComponent(component); }
// Binds the skinned button config to its button (the linker folds this body into the identical one at
// 0x00796a20)
void WONButtonConfig::SetComponent(Component* component) { this->mButton=static_cast<WONButton*>(component); ButtonConfig::SetComponent(component); }
}
