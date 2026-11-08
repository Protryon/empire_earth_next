// Child layout helper that remembers the last instruction's settings for repeats
// (not identified in the original exe)
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
#include "ee/wonapi/WONLobbyLayoutHelper.h"
namespace WONAPI {
LayoutHelper::LayoutHelper(Container *theParent)
{
	this->mContainer = theParent;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void LayoutHelper::Add()
{
	ChildLayoutInstruction *anInstr = new ChildLayoutInstruction;
	anInstr->mFlags				= this->mType;	
	anInstr->mChild			= this->mTarget;
	anInstr->mReference			= this->mReference;
	anInstr->mX		= this->mHorzPosPad;
	anInstr->mY		= this->mVertPosPad;
	anInstr->mWidth		= this->mHorzSizePad;
	anInstr->mHeight		= this->mVertSizePad;
	this->mContainer->AddChildLayout(anInstr);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void LayoutHelper::Add(Component *target, int theType, Component *ref, int horzPosPad, int vertPosPad, int horzSizePad, int vertSizePad)
{
	this->mType = theType;
	this->mTarget = target;
	this->mReference = ref;
	this->mHorzPosPad = horzPosPad;
	this->mVertPosPad = vertPosPad;
	this->mHorzSizePad = horzSizePad;
	this->mVertSizePad = vertSizePad;
	this->Add();
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void LayoutHelper::Repeat(Component *target)
{
	this->mTarget = target;
	this->Add();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void LayoutHelper::Repeat(Component *target, Component *ref)
{
	this->mTarget = target;
	this->mReference = ref;
	this->Add();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void LayoutHelper::Repeat(Component *target, int horzPosPad, int vertPosPad, int horzSizePad, int vertSizePad)
{
	this->mTarget = target;
	this->mHorzPosPad = horzPosPad;
	this->mVertPosPad = vertPosPad;
	this->mHorzSizePad = horzSizePad;
	this->mVertSizePad = vertSizePad;
	this->Add();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void LayoutHelper::FlowRepeat(Component *target)
{
	this->mReference = this->mTarget;
	this->Repeat(target);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void LayoutHelper::FlowRepeat(Component *target, int horzPosPad, int vertPosPad, int horzSizePad, int vertSizePad)
{	
	this->mReference = this->mTarget;
	this->Repeat(target,horzPosPad,vertPosPad,horzSizePad,vertSizePad);
}

}
