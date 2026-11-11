// Container config instructions: ADD, layout points and the LAYOUT family (file name inferred)
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
#include "rust/ee_rust.h"
// The instruction keys and the flag and value names this unit compares are ee_won's guiconf tables
// (rust/crates/ee_won/src/guiconf.rs).
// The LAYOUT rule's instruction names and its offsets' rule are ee_won's guilayout module
// (rust/crates/ee_won/src/guilayout.rs).
namespace WONAPI {
// FUNCTION: WON 0x00788840
// Reads a comma-separated list of components and adds each to the container
bool ContainerConfig::HandleAdd()
{
	while(true)
	{
		Component *aComp = this->ReadComponent();
		this->mContainer->AddChild(aComp);
		if(this->EndOfString())
			break;

		this->EnsureComma();
	}


	return true;
}
// FUNCTION: WON 0x00788870
// Reads a comma-separated list of layout points and adds each to the container
bool ContainerConfig::HandleAddLayoutPoint()
{
	while(true)
	{
		LayoutPoint *aPoint = SafeConfigGetComponent<LayoutPoint*>(this->ReadComponent(),"LayoutPoint");
		this->mContainer->AddLayoutPoint(aPoint);
		if(this->EndOfString())
			break;

		this->EnsureComma();
	}


	return true;
}
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
// FUNCTION: WON 0x00789c80
// Handles the container config instructions (ADD, LAYOUT and its variants, layout ids, tab focus,
// child-only input); other instructions go to the base config
bool ContainerConfig::Virtual24(const std::string& theInstruction)
{
	const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_CONTAINERCONFIG_VIRTUAL24_THEINSTRUCTION,theInstruction.data(),theInstruction.size());
	if(aKeyIdx==0)
		return this->HandleAdd();
	else if(aKeyIdx==1)
		return this->HandleAddLayoutPoint();
	else if(aKeyIdx==2)
		return this->HandleNewLayoutPoint();
	else if(aKeyIdx==3)
		return this->HandleLayout();
	else if(aKeyIdx==4)
		this->mCurLayoutId = this->ReadInt();
	else if(aKeyIdx==5)
	{
		bool enable = this->ReadBool();
		while(true)
		{
			this->EnsureComma();
			int anId = this->ReadInt();
			this->mContainer->EnableLayoutInstructions(anId,enable);
			if(this->EndOfString())
				break;
		}
	}
	else if(aKeyIdx==6)
		this->mContainer->Relayout();
	else if(aKeyIdx==7)
		return this->HandleLayout(true);
	else if(aKeyIdx==8)
		return this->HandleLayout(false,true);
	else if(aKeyIdx==9)
		return this->HandleSameSizeLayout(true,true);
	else if(aKeyIdx==10)
		return this->HandleSameSizeLayout(true,false);
	else if(aKeyIdx==11)
		return this->HandleSameSizeLayout(false,true);
	else if(aKeyIdx==12)
		return this->HandleCenterLayout(true,true);
	else if(aKeyIdx==13)
		return this->HandleCenterLayout(true,false);
	else if(aKeyIdx==14)
		return this->HandleCenterLayout(false,true);
	else if(aKeyIdx==15)
		return this->HandleSpaceEvenlyLayout(true);
	else if(aKeyIdx==16)
		return this->HandleSpaceEvenlyLayout(false);
	else if(aKeyIdx==17)
		return this->HandleClearRightLayout();
	else if(aKeyIdx==18)
		return this->HandleNoOverlapLayout();
	else if(aKeyIdx==19)
		return this->HandleSizeAtLeastLayout();
	else if(aKeyIdx==20)
		this->mContainer->SetIsolateTabFocus(this->ReadBool());
	else if(aKeyIdx==21)
		this->mContainer->SetOnlyChildInput(this->ReadBool());
	else
		return ComponentConfig::Virtual24(theInstruction);

	return true;
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007a1da0
// (class WONAPI::LayoutPoint * __cdecl WONAPI::SafeConfigGetComponent(class WONAPI::Component *,char const *)    [?SafeConfigGetComponent@WONAPI@@YAPAVLa)
