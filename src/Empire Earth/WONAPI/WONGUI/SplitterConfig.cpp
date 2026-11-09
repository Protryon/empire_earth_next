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
namespace WONAPI {
// FUNCTION: WON 0x00794ed0
// Reads a quadrant name (Left, Right, Top, Bottom, TopLeft ...); throws a config error for any other name
Splitter::QuadrantPosition SplitterConfig::ReadQuadrant()
{
	GUIString aStr = this->ReadString(false);
	if(aStr.CompareNoCase("Left")==0)
		return Splitter::Position_Left;
	else if(aStr.CompareNoCase("Right")==0)
		return Splitter::Position_Right;
	else if(aStr.CompareNoCase("Top")==0)
		return Splitter::Position_Top;
	else if(aStr.CompareNoCase("Bottom")==0)
		return Splitter::Position_Bottom;
	else if(aStr.CompareNoCase("TopLeft")==0)
		return Splitter::Position_TopLeft;
	else if(aStr.CompareNoCase("TopRight")==0)
		return Splitter::Position_TopRight;
	else if(aStr.CompareNoCase("BottomLeft")==0)
		return Splitter::Position_BottomLeft;
	else if(aStr.CompareNoCase("BottomRight")==0)
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
	if(aStr.CompareNoCase("LeftRight")==0)
		return Splitter::Splitter_LeftRight;
	else if(aStr.CompareNoCase("UpDown")==0)
		return Splitter::Splitter_UpDown;
	else if(aStr.CompareNoCase("LeftRight_Top")==0)
		return Splitter::Splitter_LeftRight_Top;
	else if(aStr.CompareNoCase("LeftRight_Bottom")==0)
		return Splitter::Splitter_LeftRight_Bottom;
	else if(aStr.CompareNoCase("UpDown_Left")==0)
		return Splitter::Splitter_UpDown_Left;
	else if(aStr.CompareNoCase("UpDown_Right")==0)
		return Splitter::Splitter_UpDown_Right;
	else if(aStr.CompareNoCase("Center")==0)
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
	if(theInstruction=="ADDQUADRANT")
	{
		Component *aComponent = this->ReadComponent(); this->EnsureComma();
		Splitter::QuadrantPosition aPosition = this->ReadQuadrant();
		this->mSplitter->AddQuadrant(aComponent,aPosition);
	}
	else if(theInstruction=="ADDSPLITTER")
	{
		Component *aComponent = this->ReadComponent(); this->EnsureComma();
		Splitter::SplitterType aType = this->ReadSplitterType();
		this->mSplitter->AddSplitter(aComponent,aType);
	}
	else if(theInstruction=="ADDINVISIBLESPLITTER")
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
	else if(theInstruction=="ADDCOMPLEXSPLITTER")
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
	else if(theInstruction=="MINLEFTRIGHT")
	{
		int minLeft = this->ReadInt(); this->EnsureComma();
		int minRight = this->ReadInt(); 
		this->mSplitter->SetMinLeftRight(minLeft,minRight);
	}
	else if(theInstruction=="MINTOPBOTTOM")
	{
		int minTop = this->ReadInt(); this->EnsureComma();
		int minBottom = this->ReadInt();
		this->mSplitter->SetMinTopBottom(minTop,minBottom);
	}
	else if(theInstruction=="SPLITTERPERCENTAGES")
	{
		int leftPercent = this->ReadInt(); this->EnsureComma();
		int topPercent = this->ReadInt();
		this->mSplitter->SetPercentages(leftPercent,topPercent);
	}
	else if(theInstruction=="MAXQUADRANT")
		this->mSplitter->MaxQuadrant(this->ReadQuadrant());
	else if(theInstruction=="UNMAXQUADRAN")
		this->mSplitter->UnMaxQuadrant();
	else if(theInstruction=="SPLITTERPOS")
	{
		int x = this->ReadInt(); this->EnsureComma();
		int y = this->ReadInt();
		this->mSplitter->SetSplitterPos(x,y);
	}
	else if(theInstruction=="REGISTERMAXBUTTON")
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
