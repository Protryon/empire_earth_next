// Container config builders for the same-size, center, clear-right, space-evenly, no-overlap and
// size-at-least layouts (file name inferred)
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
namespace WONAPI {
// FUNCTION: WON 0x00788e20
// Reads a component list into a layout that gives them the same width and/or height
bool ContainerConfig::HandleSameSizeLayout(bool sameWidth, bool sameHeight)
{	
	SmartPtr<SameSizeLayout> aLayout = new SameSizeLayout(sameWidth, sameHeight);

	while(true)
	{	
		Component *aComp = this->ReadComponent();
		aLayout->Add(aComp);
		if(this->EndOfString())
			break;
		else
			this->EnsureComma();
	}

	this->mContainer->AddChildLayout(aLayout.get(),this->mCurLayoutId);
	return true;
}
// FUNCTION: WON 0x00788f30
// Reads a reference component and a component list into a layout that centres them on it
bool ContainerConfig::HandleCenterLayout(bool horz, bool vert)
{
	Component *aComp = this->ReadComponent();
	SmartPtr<CenterLayout> aLayout = new CenterLayout(aComp,horz,vert);
	while(!this->EndOfString())
	{	
		this->EnsureComma();
		aComp = this->ReadComponent();
		aLayout->Add(aComp);
	}

	this->mContainer->AddChildLayout(aLayout.get(),this->mCurLayoutId);
	return true;
}
// FUNCTION: WON 0x00789080
// Reads a component, a padding and a component list into a clear-right layout
bool ContainerConfig::HandleClearRightLayout()
{
	Component *aComp = this->ReadComponent(); this->EnsureComma();
	int aPad = this->ReadInt(); this->EnsureComma();

	SmartPtr<ClearRightLayout> aLayout = new ClearRightLayout(aComp,aPad);
	while(true)
	{
		aComp = this->ReadComponent();
		aLayout->Add(aComp);
		if(this->EndOfString())
			break;

		this->EnsureComma();
	}

	this->mContainer->AddChildLayout(aLayout.get(),this->mCurLayoutId);
	return true;
}
// FUNCTION: WON 0x00789280
// Reads a bounding component, two paddings and a component list into a layout that spaces them evenly
bool ContainerConfig::HandleSpaceEvenlyLayout(bool horz)
{
	Component *aSpaceBox = this->ReadComponent(); this->EnsureComma();
	int aLeftPad = this->ReadInt(); this->EnsureComma();
	int aRightPad = this->ReadInt(); this->EnsureComma();

	SmartPtr<SpaceEvenlyLayout> aLayout = new SpaceEvenlyLayout(aSpaceBox,aLeftPad,aRightPad,horz);
	while(!this->EndOfString())
	{
		Component *aComponent = this->ReadComponent();
		aLayout->Add(aComponent);
		if(this->EndOfString())
			break;

		this->EnsureComma();
	}

	this->mContainer->AddChildLayout(aLayout.get(),this->mCurLayoutId);
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

		if(aStr.CompareNoCase("Above")==0) instructions |= 0x100;
		else if(aStr.CompareNoCase("Below")==0) instructions |= 0x200;
		else if(aStr.CompareNoCase("Right")==0) instructions |= 0x400;
		else if(aStr.CompareNoCase("Left")==0) instructions |= 0x800;
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
// FUNCTION: WON 0x00789860
// Reads a size-at-least layout (target, paddings, components); it is added under the default layout id
bool ContainerConfig::HandleSizeAtLeastLayout()
{
	Component *target = this->ReadComponent(); this->EnsureComma();
	int aHorzPad = this->ReadInt(); this->EnsureComma();
	int aVertPad = this->ReadInt(); 

	SmartPtr<SizeAtLeastLayout> aLayout = new SizeAtLeastLayout(target,aHorzPad, aVertPad);
	while(!this->EndOfString())
	{
		this->EnsureComma();
		aLayout->Add(this->ReadComponent());		
	}
	this->mContainer->AddChildLayout(aLayout.get());
	return true;
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00789200
// (public: virtual __thiscall WONAPI::CenterLayout::~CenterLayout(void)    [??1CenterLayout@WONAPI@@UAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007891e0
// (public: virtual void * __thiscall WONAPI::CenterLayout::`scalar deleting destructor'(unsigned int)    [??_GCenterLayout@WONAPI@@UAEPAXI@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00789410
// (public: virtual void * __thiscall WONAPI::SpaceEvenlyLayout::`scalar deleting destructor'(unsigned int)    [??_GSpaceEvenlyLayout@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x007897c0
// (public: virtual void * __thiscall WONAPI::NoOverlapLayout::`scalar deleting destructor'(unsigned int)    [??_GNoOverlapLayout@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x007899d0
// (public: virtual void * __thiscall WONAPI::SizeAtLeastLayout::`scalar deleting destructor'(unsigned int)    [??_GSizeAtLeastLayout@WONAPI@@UAEPAXI@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00789430
// (public: virtual __thiscall WONAPI::SpaceEvenlyLayout::~SpaceEvenlyLayout(void)    [??1SpaceEvenlyLayout@WONAPI@@UAE@XZ])
// TEMPLATE: WON 0x007897e0
// (public: virtual __thiscall WONAPI::NoOverlapLayout::~NoOverlapLayout(void)    [??1NoOverlapLayout@WONAPI@@UAE@XZ])
// TEMPLATE: WON 0x007899f0
// (public: virtual __thiscall WONAPI::SizeAtLeastLayout::~SizeAtLeastLayout(void)    [??1SizeAtLeastLayout@WONAPI@@UAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007a2cc0
// (void __cdecl std::_Construct(class WONAPI::Component * *,class WONAPI::Component * const &)    [?_Construct@std@@YAXPAPAVComponent@WONAPI@@ABQAV23@@Z])
