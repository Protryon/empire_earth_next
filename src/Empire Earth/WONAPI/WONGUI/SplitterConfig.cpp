// Splitter config: reads quadrant positions, splitter types and the splitter keys (file name inferred)
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
#include "ee/wonapi/WONLobbySplitterConfig.h"
#include "rust/ee_won.h"
// The instruction keys and the flag and value names this unit compares are ee_won's guiconf tables
// (rust/crates/ee_won/src/guiconf.rs); this unit keeps the reads, the objects and what each entry does.
namespace WONAPI {
// FUNCTION: WON 0x00794ed0
// Reads a quadrant name (Left, Right, Top, Bottom, TopLeft ...); throws a config error for any other name
Splitter::QuadrantPosition SplitterConfig::ReadQuadrant()
{
	GUIString aStr = this->ReadString(false);
	const int aNameIdx_aStr=ee_won_gui_cfg_name(EE_WON_CFG_NAMES_SPLITTERCONFIG_READQUADRANT_ASTR,aStr.mData->mText,aStr.mData->mLength,sizeof(wchar_t));
	if(aNameIdx_aStr==0)
		return Splitter::Position_Left;
	else if(aNameIdx_aStr==1)
		return Splitter::Position_Right;
	else if(aNameIdx_aStr==2)
		return Splitter::Position_Top;
	else if(aNameIdx_aStr==3)
		return Splitter::Position_Bottom;
	else if(aNameIdx_aStr==4)
		return Splitter::Position_TopLeft;
	else if(aNameIdx_aStr==5)
		return Splitter::Position_TopRight;
	else if(aNameIdx_aStr==6)
		return Splitter::Position_BottomLeft;
	else if(aNameIdx_aStr==7)
		return Splitter::Position_BottomRight;
	else
		throw ConfigObjectException("Invalid quadrant: " + (std::string)aStr);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x007951b0
// Reads a splitter type name (LeftRight, UpDown, LeftRight_Top ..., Center); throws a config error otherwise
Splitter::SplitterType SplitterConfig::ReadSplitterType()
{
	GUIString aStr = this->ReadString(false);
	const int aNameIdx_aStr=ee_won_gui_cfg_name(EE_WON_CFG_NAMES_SPLITTERCONFIG_READSPLITTERTYPE_ASTR,aStr.mData->mText,aStr.mData->mLength,sizeof(wchar_t));
	if(aNameIdx_aStr==0)
		return Splitter::Splitter_LeftRight;
	else if(aNameIdx_aStr==1)
		return Splitter::Splitter_UpDown;
	else if(aNameIdx_aStr==2)
		return Splitter::Splitter_LeftRight_Top;
	else if(aNameIdx_aStr==3)
		return Splitter::Splitter_LeftRight_Bottom;
	else if(aNameIdx_aStr==4)
		return Splitter::Splitter_UpDown_Left;
	else if(aNameIdx_aStr==5)
		return Splitter::Splitter_UpDown_Right;
	else if(aNameIdx_aStr==6)
		return Splitter::Splitter_Center;
	else
		throw ConfigObjectException("Invalid splitter type: " + (std::string)aStr);
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x00795450
// Reads the splitter keys (quadrants, splitters, minimum sizes, percentages, maximize); others go to the container
// config. The restore key is spelled UNMAXQUADRAN
bool SplitterConfig::Virtual24(const std::string& theInstruction)
{
	const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_SPLITTERCONFIG_VIRTUAL24_THEINSTRUCTION,theInstruction.data(),theInstruction.size());
	if(aKeyIdx==0)
	{
		Component *aComponent = this->ReadComponent(); this->EnsureComma();
		Splitter::QuadrantPosition aPosition = this->ReadQuadrant();
		this->mSplitter->AddQuadrant(aComponent,aPosition);
	}
	else if(aKeyIdx==1)
	{
		Component *aComponent = this->ReadComponent(); this->EnsureComma();
		Splitter::SplitterType aType = this->ReadSplitterType();
		this->mSplitter->AddSplitter(aComponent,aType);
	}
	else if(aKeyIdx==2)
	{
		Splitter::SplitterType aType = this->ReadSplitterType();
		int aSize = 10;
		if(!this->EndOfString())
		{
			this->EnsureComma();
			aSize = this->ReadInt();
		}
		this->mSplitter->AddInvisibleSplitter(aType,aSize);
	}
	else if(aKeyIdx==3)
	{
		Container *aContainer= SafeConfigGetComponent<Container*>(this->ReadComponent(),"Container"); this->EnsureComma();
		Splitter::SplitterType aType = this->ReadSplitterType();
		Component *aDragComponent = NULL;
		if(!this->EndOfString())
		{
			this->EnsureComma();
			aDragComponent = this->ReadComponent();
		}
		this->mSplitter->AddComplexSplitter(aContainer,aType,aDragComponent);
	}
	else if(aKeyIdx==4)
	{
		int minLeft = this->ReadInt(); this->EnsureComma();
		int minRight = this->ReadInt(); 
		this->mSplitter->SetMinLeftRight(minLeft,minRight);
	}
	else if(aKeyIdx==5)
	{
		int minTop = this->ReadInt(); this->EnsureComma();
		int minBottom = this->ReadInt();
		this->mSplitter->SetMinTopBottom(minTop,minBottom);
	}
	else if(aKeyIdx==6)
	{
		int leftPercent = this->ReadInt(); this->EnsureComma();
		int topPercent = this->ReadInt();
		this->mSplitter->SetPercentages(leftPercent,topPercent);
	}
	else if(aKeyIdx==7)
		this->mSplitter->MaxQuadrant(this->ReadQuadrant());
	else if(aKeyIdx==8)
		this->mSplitter->UnMaxQuadrant();
	else if(aKeyIdx==9)
	{
		int x = this->ReadInt(); this->EnsureComma();
		int y = this->ReadInt();
		this->mSplitter->SetSplitterPos(x,y);
	}
	else if(aKeyIdx==10)
	{
		Button *aButton = SafeConfigGetComponent<Button*>(this->ReadComponent(),"Button"); this->EnsureComma();
		Splitter::QuadrantPosition aPosition = this->ReadQuadrant();
		this->mSplitter->RegisterMaxButton(aButton,aPosition);
	}
	else
		return ContainerConfig::Virtual24(theInstruction);

	return true;
}

}

namespace WONAPI {
// Binds the config to its splitter (the linker folds this body into the identical one at 0x0078f960)
void SplitterConfig::SetComponent(Component* component)
{
    ContainerConfig::SetComponent(component);
    this->mSplitter=static_cast<Splitter*>(component);
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007a1fc0
// (class WONAPI::Container * __cdecl WONAPI::SafeConfigGetComponent(class WONAPI::Component *,char const *)    [?SafeConfigGetComponent@WONAPI@@YAPAVCont)
