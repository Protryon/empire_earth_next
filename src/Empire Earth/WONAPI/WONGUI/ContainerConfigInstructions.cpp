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

			if(aStr.CompareNoCase("SameWidth")==0) instructions |= 0x1;
			else if(aStr.CompareNoCase("SameHeight")==0) instructions |= 0x2;
			else if(aStr.CompareNoCase("Above")==0) instructions |= 0x100;
			else if(aStr.CompareNoCase("Below")==0) instructions |= 0x200;
			else if(aStr.CompareNoCase("Right")==0) instructions |= 0x400;
			else if(aStr.CompareNoCase("Left")==0) instructions |= 0x800;
			else if(aStr.CompareNoCase("SameLeft")==0) instructions |= 0x1000;
			else if(aStr.CompareNoCase("SameRight")==0) instructions |= 0x2000;
			else if(aStr.CompareNoCase("SameTop")==0) instructions |= 0x4000;
			else if(aStr.CompareNoCase("SameBottom")==0) instructions |= 0x8000;
			else if(aStr.CompareNoCase("GrowToRight")==0) instructions |= 0x10000;
			else if(aStr.CompareNoCase("GrowToLeft")==0) instructions |= 0x20000;
			else if(aStr.CompareNoCase("GrowToTop")==0) instructions |= 0x40000;
			else if(aStr.CompareNoCase("GrowToBottom")==0) instructions |= 0x80000;
			else if(aStr.CompareNoCase("SameSize")==0) instructions |= 0x3;	
			else 
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

	if((!chain && !repeat) || i!=0)
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
	if(theInstruction=="ADD")
		return this->HandleAdd();
	else if(theInstruction=="ADDLAYOUTPOINT")
		return this->HandleAddLayoutPoint();
	else if(theInstruction=="NEWLAYOUTPOINT")
		return this->HandleNewLayoutPoint();
	else if(theInstruction=="LAYOUT")
		return this->HandleLayout();
	else if(theInstruction=="SETLAYOUTID")
		this->mCurLayoutId = this->ReadInt();
	else if(theInstruction=="ENABLELAYOUT")
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
	else if(theInstruction=="RECALCLAYOUT")
		this->mContainer->Relayout();
	else if(theInstruction=="CHAINLAYOUT")
		return this->HandleLayout(true);
	else if(theInstruction=="REPEATLAYOUT")
		return this->HandleLayout(false,true);
	else if(theInstruction=="SAMESIZELAYOUT")
		return this->HandleSameSizeLayout(true,true);
	else if(theInstruction=="SAMEWIDTHLAYOUT")
		return this->HandleSameSizeLayout(true,false);
	else if(theInstruction=="SAMEHEIGHTLAYOUT")
		return this->HandleSameSizeLayout(false,true);
	else if(theInstruction=="CENTERLAYOUT")
		return this->HandleCenterLayout(true,true);
	else if(theInstruction=="HORZCENTERLAYOUT")
		return this->HandleCenterLayout(true,false);
	else if(theInstruction=="VERTCENTERLAYOUT")
		return this->HandleCenterLayout(false,true);
	else if(theInstruction=="HORZSPACEEVENLYLAYOUT")
		return this->HandleSpaceEvenlyLayout(true);
	else if(theInstruction=="VERTSPACEEVENLYLAYOUT")
		return this->HandleSpaceEvenlyLayout(false);
	else if(theInstruction=="CLEARRIGHTLAYOUT")
		return this->HandleClearRightLayout();
	else if(theInstruction=="NOOVERLAPLAYOUT")
		return this->HandleNoOverlapLayout();
	else if(theInstruction=="SIZEATLEASTLAYOUT")
		return this->HandleSizeAtLeastLayout();
	else if(theInstruction=="ISOLATETABFOCUS")
		this->mContainer->SetIsolateTabFocus(this->ReadBool());
	else if(theInstruction=="ONLYCHILDINPUT")
		this->mContainer->SetOnlyChildInput(this->ReadBool());
	else
		return ComponentConfig::Virtual24(theInstruction);

	return true;
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007a1da0
// (class WONAPI::LayoutPoint * __cdecl WONAPI::SafeConfigGetComponent(class WONAPI::Component *,char const *)    [?SafeConfigGetComponent@WONAPI@@YAPAVLa)
