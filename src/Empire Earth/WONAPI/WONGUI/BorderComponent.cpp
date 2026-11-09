// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
// Border component: four corner and four edge components laid out around a frame (not identified in the
// original exe)
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyBorderComponent.h"
namespace WONAPI {
// Creates a border component that passes input only to its children; the edge controls are added later
BorderComponent::BorderComponent()
{
//	SetComponentFlags(ComponentFlag_NoInput,true);
	this->mOnlyChildInput = true;
	this->mControlsAdded = false;
}

void BorderComponent::SetComponent(BorderPos thePos, Component *theComponent, int theWidth, int theHeight)
{
	this->mComponents[thePos] = theComponent;
	if(theWidth!=-1 && theHeight!=-1)
		theComponent->SetSize(theWidth,theHeight);	
}

void BorderComponent::AddControls()
{
	for(int i=0; i<8; i++)
	{
		if(this->mComponents[i].get()==NULL)
			this->mComponents[i] = new Component;
			
		this->AddChild(this->mComponents[i]);
	}

	this->mLeftTop = this->mComponents[Pos_LeftTop];
	this->mRightTop = this->mComponents[Pos_RightTop];
	this->mLeftBottom = this->mComponents[Pos_LeftBottom];
	this->mRightBottom = this->mComponents[Pos_RightBottom];
	
	this->mLeft = this->mComponents[Pos_Left];
	this->mTop = this->mComponents[Pos_Top];
	this->mRight = this->mComponents[Pos_Right];
	this->mBottom = this->mComponents[Pos_Bottom];

	this->mControlsAdded = true;
	this->Virtual08();
}

void BorderComponent::Virtual08()
{
	Container::Virtual08();

	if(!this->mControlsAdded)
		return;

	this->mLeftTop->SetPos(0,0); 
	this->mRightTop->SetPos(this->Width()-this->mRightTop->Width(),0);
	this->mLeftBottom->SetPos(0,this->Height()-this->mLeftBottom->Height()); 
	this->mRightBottom->SetPos(this->Width()-this->mRightBottom->Width(),this->Height()-this->mRightBottom->Height());
	
	this->mLeft->SetPosSize(0,(this->mLeftTop->Top()+this->mLeftTop->Height()), this->mLeft->Width(), this->mLeftBottom->Top() - (this->mLeftTop->Top()+this->mLeftTop->Height()));
	this->mRight->SetPosSize(this->Width()-this->mRight->Width(), (this->mRightTop->Top()+this->mRightTop->Height()), this->mRight->Width(), this->mRightBottom->Top() - (this->mRightTop->Top()+this->mRightTop->Height()));
	this->mTop->SetPosSize((this->mLeftTop->Left()+this->mLeftTop->Width()), 0, this->mRightTop->Left() - (this->mLeftTop->Left()+this->mLeftTop->Width()), this->mTop->Height());
	this->mBottom->SetPosSize((this->mLeftBottom->Left()+this->mLeftBottom->Width()), this->Height()-this->mBottom->Height(), this->mRightBottom->Left() - (this->mLeftBottom->Left()+this->mLeftBottom->Width()), this->mBottom->Height());
}
}
