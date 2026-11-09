// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <vector>
#include <list>
#include <map>
#include <algorithm>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
namespace WONAPI {
// Creates a scroll area around one component, centred both ways (not identified in the original exe)
ComponentScrollArea::ComponentScrollArea(Component *theComponent)
{
	this->mComponent = theComponent;
	this->GetViewport()->AddChild(this->mComponent.get());
	this->mVertCenter = true;
	this->mHorzCenter = true;
	this->UpdateComponentSize();
}

void ComponentScrollArea::Virtual08()
{
	ScrollArea::Virtual08();
	this->PositionComponent();
}

void ComponentScrollArea::PositionComponent()
{
	int x = 0, y = 0;
	if(this->mHorzCenter)
	{
		if(this->mComponent->Width() < this->mScrollArea.width)
			x = (this->mScrollArea.width - this->mComponent->Width())/2;
	}

	if(this->mVertCenter)
	{
		if(this->mComponent->Height() < this->mScrollArea.height)
			y = (this->mScrollArea.height - this->mComponent->Height())/2;
	}

	this->mComponent->SetPos(x,y);
	this->Invalidate();
}

void ComponentScrollArea::SetComponentSize(int theWidth, int theHeight)
{
	this->SetScrollDimension(theWidth,theHeight);
	this->mComponent->SetSize(theWidth,theHeight);
	this->PositionComponent();
}

void ComponentScrollArea::UpdateComponentSize()
{
	this->SetComponentSize(this->mComponent->Width(), this->mComponent->Height());
}
}
