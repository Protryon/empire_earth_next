// Container config instructions: the LAYOUT family and the no-overlap layout (file name inferred). R6 WON round 40: the
// other instructions (ADD, ADDLAYOUTPOINT, LAYOUTID, ENABLELAYOUT, RELAYOUT, the same-size, centre, clear-right,
// space-evenly and size-at-least layouts, the tab focus and child input flags) are programs of ee_won's table reader
// (rust/crates/ee_won/src/cfgtab.rs) over WONCfgTable.inl, which calls these two for their keys.
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
#include "ee/wonapi/WONLobbyConfig.h"
#include "ee/wonapi/WONLobbyLayout.h"
#include "rust/ee_won.h"
// The instruction keys and the flag and value names this unit compares are ee_won's guiconf tables
// (rust/crates/ee_won/src/guiconf.rs).
// The LAYOUT rule's instruction names and its offsets' rule are ee_won's guilayout module
// (rust/crates/ee_won/src/guilayout.rs).
namespace WONAPI {
// FUNCTION: WON 0x007888b0
// Reads a layout rule (target, instruction names, reference, up to four offsets) and adds it to the container;
// chain and repeat forms reuse the previous rule's instructions and reference
bool ContainerConfig::HandleLayout(bool chain, bool repeat)
{
	Component *target = this->ReadComponent(); 
	int instructions = 0;
	Component *ref;

	if(!chain && !repeat)
	{
		this->EnsureComma();
		while(true)
		{
			GUIString aStr = this->ReadString(true);
			if(aStr.length()==0)
				break;

			// the names in the original's order, each compared as the original compared it
			const uint8_t* aName = NULL;
			int aFlag = 0;
			bool aFound = false;
			for(int k = 0; !aFound && ee_won_gui_layout_keyword(k, &aName, &aFlag); k++)
				if(aStr.CompareNoCase((const char*)aName)==0) { instructions |= aFlag; aFound = true; }
			if(!aFound)
				throw ConfigObjectException("Unknown layout instruction: " + (std::string)aStr);
		}
	}
	else
		instructions = this->mLayoutInstruction;

	if(chain || (repeat && this->EndOfString()))
	{
		if(this->mLayoutRef==NULL)
			throw ConfigObjectException("Invalid LayoutChain");

		ref = this->mLayoutRef;
	}
	else
	{
		this->EnsureComma();
		ref = this->ReadComponent();
	}

	if(!this->EndOfString())
		this->EnsureComma();

	int i;
	for(i=0; i<4; i++)
	{
		if(this->EndOfString())
			break;

		this->mLayoutParams[i] = this->ReadInt();
		if(!this->EndOfString())
			this->EnsureComma();
	}

	if(ee_won_gui_layout_zero_rest(chain, repeat, i))
	{
		for(; i<4; i++)
			this->mLayoutParams[i] = 0;
	}

	this->mContainer->Fn2F6140(target,instructions,ref,this->mLayoutParams[0],this->mLayoutParams[1],
		this->mLayoutParams[2],this->mLayoutParams[3],this->mCurLayoutId);

	if(repeat)
		this->mLayoutRef = ref;
	else
		this->mLayoutRef = target;

	this->mLayoutInstruction = instructions;

	return true;
}
// FUNCTION: WON 0x007894b0
// Reads a no-overlap layout (target, sides, reference, paddings, more components); it is added
// under the default layout id, not the current one
bool ContainerConfig::HandleNoOverlapLayout()
{
	Component *target = this->ReadComponent();
	this->EnsureComma();

	int instructions = 0;
	while(true)
	{
		GUIString aStr = this->ReadString(true);
		if(aStr.length()==0)
			break;

		const int aNameIdx_aStr=ee_won_gui_cfg_name(EE_WON_CFG_NAMES_CONTAINERCONFIG_HANDLENOOVERLAPLAYOUT_ASTR,aStr.mData->mText,aStr.mData->mLength,sizeof(wchar_t));
		if(aNameIdx_aStr==0) instructions |= 0x100;
		else if(aNameIdx_aStr==1) instructions |= 0x200;
		else if(aNameIdx_aStr==2) instructions |= 0x400;
		else if(aNameIdx_aStr==3) instructions |= 0x800;
		else 
			throw ConfigObjectException("Unknown no overlap instruction: " + (std::string)aStr);
	}

	this->EnsureComma();
	Component *ref = this->ReadComponent(); this->EnsureComma();
	int horzPad, vertPad;
	horzPad = this->ReadInt(); this->EnsureComma();
	vertPad = this->ReadInt(); 

	SmartPtr<NoOverlapLayout> aLayout = new NoOverlapLayout(target,instructions,ref,horzPad,vertPad);
	while(!this->EndOfString())
	{
		this->EnsureComma();
		Component *aComponent = this->ReadComponent();
		aLayout->Add(aComponent);
	}

	this->mContainer->AddChildLayout(aLayout.get());

	return true;
}
}
