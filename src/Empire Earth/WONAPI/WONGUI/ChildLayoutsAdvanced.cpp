// Percentage, flow, same-size, spacing and overlap child layouts (file name inferred)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyLayout.h"
#include "rust/ee_rust.h"
// The layouts' arithmetic is ee_won's guilayout module (rust/crates/ee_won/src/guilayout.rs); this unit keeps the
// layout objects, their lists and the SetPos / SetSize calls.
namespace WONAPI {
// FUNCTION: WON 0x00737370
// Sets the four anchor components; a missing edge takes the opposite edge on the same axis, then the last
// anchor supplied
void PercentLayoutParam::SetBorders(Component* inLeft, Component* inTop, Component* inRight, Component* inBottom)
{
    Component* in[4] = { inLeft, inTop, inRight, inBottom };
    int anIndex[4];
    ee_won_gui_percent_borders((inLeft!=NULL?1u:0u)|(inTop!=NULL?2u:0u)|(inRight!=NULL?4u:0u)|(inBottom!=NULL?8u:0u),
                               anIndex);
    this->left=anIndex[0]<0?NULL:in[anIndex[0]];
    this->top=anIndex[1]<0?NULL:in[anIndex[1]];
    this->right=anIndex[2]<0?NULL:in[anIndex[2]];
    this->bottom=anIndex[3]<0?NULL:in[anIndex[3]];
}
// FUNCTION: WON 0x00737430
// Positions and sizes the target as percentages of the span between its padded anchor edges; a negative
// percentage leaves that coordinate unchanged (the padding is added even when it is the default -1)
void PercentLayoutParam::DoCalc(Component* target, Component* parent)
{
    // the anchors are read only for an axis the layout sets, as the original read them
    int anEdges[4] = { 0, 0, 0, 0 };
    if(this->px>=0 || this->pw>=0)
    {
        anEdges[0]=this->left==parent?0:this->left->Left();
        anEdges[1]=this->right==parent?parent->Width():this->right->Left()+this->right->Width();
    }
    if(this->py>=0 || this->ph>=0)
    {
        anEdges[2]=this->top==parent?0:this->top->Top();
        anEdges[3]=this->bottom==parent?parent->Height():this->bottom->Top()+this->bottom->Height();
    }
    EeWonRect aTarget = { target->Left(), target->Top(), target->Width(), target->Height() };
    EeWonPercent p = { this->px, this->py, this->pw, this->ph, this->lp, this->tp, this->rp, this->bp };
    EeWonRect r = ee_won_gui_percent_layout(aTarget, anEdges, p);
    target->SetPosSize(r.x,r.y,r.width,r.height);
}
// FUNCTION: WON 0x00737800
// Gives every listed component the largest width and/or height among them, per the horizontal and
// vertical flags
void SameSizeLayout::Execute(Component*)
{
    std::vector<int> aSizes;
    std::list<Component*>::iterator i;
    for(i=this->mComponentList.begin();i!=this->mComponentList.end();++i)
    {
        aSizes.push_back((*i)->Width());
        aSizes.push_back((*i)->Height());
    }
    int width=0,height=0;
    ee_won_gui_max_size(0,aSizes.empty()?NULL:&aSizes[0],this->mComponentList.size(),this->mHorz,this->mVert,0,0,
                        &width,&height);
    for(i=this->mComponentList.begin();i!=this->mComponentList.end();++i)
    {
        Component* child=*i;
        if(this->mHorz && this->mVert) child->SetSize(width,height);
        else if(this->mHorz) child->SetSize(width,child->Height());
        else if(this->mVert) child->SetSize(child->Width(),height);
    }
}
// FUNCTION: WON 0x007378d0
// Moves the component to the right of the rightmost listed component, plus the padding
void ClearRightLayout::Execute(Component*)
{
    std::vector<int> aRights;
    for(std::list<Component*>::iterator i=this->mComponentList.begin();i!=this->mComponentList.end();++i)
        aRights.push_back((*i)->Right());
    this->mComponent->SetPos(ee_won_gui_clear_right(aRights.empty()?NULL:&aRights[0],aRights.size(),this->mPad),
                             this->mComponent->Top());
}
// FUNCTION: WON 0x00737920
// Spreads the listed components evenly across the space box (horizontally or vertically), handing the
// leftover pixels one each to the first gaps
void SpaceEvenlyLayout::Execute(Component* parent)
{
    if(this->mComponentList.empty()) return;
    std::vector<int> anExtents;
    std::list<Component*>::iterator i;
    for(i=this->mComponentList.begin();i!=this->mComponentList.end();++i)
        anExtents.push_back(this->mIsHorz?(*i)->Width():(*i)->Height());
    Component* box=this->mSpaceBox;
    std::vector<int> aPositions(anExtents.size());
    ee_won_gui_space_evenly(&anExtents[0],anExtents.size(),this->mIsHorz?box->Left():box->Top(),
                            this->mIsHorz?box->Width():box->Height(),box==parent,this->mLeftPad,this->mRightPad,
                            &aPositions[0]);
    size_t n=0;
    for(i=this->mComponentList.begin();i!=this->mComponentList.end();++i,++n)
    {
        Component* child=*i;
        if(this->mIsHorz) child->SetPos(aPositions[n],child->Top());
        else child->SetPos(child->Left(),aPositions[n]);
    }
}
// The shift is clamped one way for the parent and the other way for a sibling reference
// FUNCTION: WON 0x00737a40
// Pushes the target (and the components that move with it) clear of the reference component, or back
// inside the parent when the reference is the parent, along the requested sides
void NoOverlapLayout::Execute(Component* parent)
{
    const WONRectangle& b=this->mTarget->GetBounds();
    EeWonRect aTarget = { b.x, b.y, b.width, b.height };
    EeWonRect aRef = { this->mRef->Left(), this->mRef->Top(), this->mRef->Width(), this->mRef->Height() };
    int dx=0,dy=0;
    if(!ee_won_gui_no_overlap(aTarget,aRef,this->mRef==parent,this->mType,this->mHorzPad,this->mVertPad,&dx,&dy))
        return;
    this->mTarget->SetPos(this->mTarget->Left()+dx,this->mTarget->Top()+dy);
    for(std::list<Component*>::iterator i=this->mComponentList.begin();i!=this->mComponentList.end();++i)
    {
        Component* child=*i;
        child->SetPos(child->Left()+dx,child->Top()+dy);
    }
}
// FUNCTION: WON 0x00737b90
// Grows the target to at least the largest listed component's width and height plus the padding
void SizeAtLeastLayout::Execute(Component*)
{
    std::vector<int> aSizes;
    for(std::list<Component*>::iterator i=this->mComponentList.begin();i!=this->mComponentList.end();++i)
    {
        aSizes.push_back((*i)->Width());
        aSizes.push_back((*i)->Height());
    }
    int width=0,height=0;
    ee_won_gui_max_size(1,aSizes.empty()?NULL:&aSizes[0],this->mComponentList.size(),false,false,this->mHorzPad,
                        this->mVertPad,&width,&height);
    this->mTarget->SetSizeAtLeast(width,height);
}
}
#include "ee/wonapi/WONPercentLayout.h"

namespace WONAPI {
// FUNCTION: WON 0x007372c0
// Sets the anchors, percentages and padding of a percentage layout
void PercentLayoutParam::Init(Component *left,Component *top,Component *right,Component *bottom,int px,int py,int pw,int ph,int lp,int tp,int rp,int bp)
{
    this->SetBorders(left,top,right,bottom);
    this->SetPercentages(px,py,pw,ph);
    this->SetPadding(lp,tp,rp,bp);
}
// FUNCTION: WON 0x007373f0
// Sets the position and size percentages (negative = leave that coordinate alone)
void PercentLayoutParam::SetPercentages(int thePercentX,int thePercentY,int thePercentWidth,int thePercentHeight)
{
    this->px=thePercentX;
    this->py=thePercentY;
    this->pw=thePercentWidth;
    this->ph=thePercentHeight;
}
// FUNCTION: WON 0x00737410
// Sets the padding applied inside each anchor edge
void PercentLayoutParam::SetPadding(int theLeftPad,int theTopPad,int theRightPad,int theBottomPad)
{
    this->lp=theLeftPad;
    this->tp=theTopPad;
    this->rp=theRightPad;
    this->bp=theBottomPad;
}
// FUNCTION: WON 0x00737580
// Positions and sizes the target from its percentage parameters
void PercentLayout::Execute(Component* theParent)
{
    this->mParam.DoCalc(this->mTarget,theParent);
}
}

namespace WONAPI {
// FUNCTION: WON 0x007377a0
// Adds a component to the same-size group; returns the layout for chaining
SameSizeLayout* SameSizeLayout::Add(Component* theComp)
{
    this->mComponentList.push_back(theComp);
    return this;
}
}

namespace WONAPI {
// FUNCTION: WON 0x00737320
// Creates percentage layout parameters from four anchors, percentages and padding
PercentLayoutParam::PercentLayoutParam(Component *left, Component *top, Component *right, Component *bottom, int px, int py, int pw, int ph, int lp, int tp, int rp, int bp)
{
	this->Init(left,top,right,bottom,px,py,pw,ph,lp,tp,rp,bp);
}
}

namespace WONAPI {
// Vertical flow layout: stacks components below the top component with per-item padding (not identified
// in the original exe)
VerticalFlowLayout::VerticalFlowLayout(Component* theTop)
{
    this->mTopComp = theTop;
}
VerticalFlowLayout* VerticalFlowLayout::Add(Component* theChild, int theTopPad, int theLeftPad)
{
    this->mComponentList.push_back(ItemInfo(theChild,theTopPad,theLeftPad));
    return this;
}
void VerticalFlowLayout::Execute(Component* theParent)
{
    if(this->mComponentList.empty())
        return;
    ComponentList::iterator anItr = this->mComponentList.begin();
    int aTop, aLeft;
    if(anItr->mComponent->GetParent()==this->mTopComp)
    {
        aTop = 0;
        aLeft = 0;
    }
    else
    {
        aTop = this->mTopComp->Top()+this->mTopComp->Height();
        aLeft = this->mTopComp->Left();
    }
    std::vector<EeWonFlowItem> anItems;
    for(ComponentList::iterator i = anItr; i!=this->mComponentList.end(); ++i)
    {
        EeWonFlowItem it = { i->mTopPad, i->mLeftPad, i->mComponent->Left(), i->mComponent->Height() };
        anItems.push_back(it);
    }
    std::vector<int> aPositions(2*anItems.size());
    ee_won_gui_vertical_flow(&anItems[0],anItems.size(),aLeft,aTop,&aPositions[0]);
    for(size_t n = 0; anItr!=this->mComponentList.end(); ++anItr, ++n)
        anItr->mComponent->SetPos(aPositions[2*n],aPositions[2*n+1]);
}
}

namespace WONAPI {
// Child layout list: an ordered list of up to eight components with a cursor (not identified in the
// original exe)
ChildLayoutList::ChildLayoutList(Component *c1)
{
	this->mChildList.push_back(c1);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
ChildLayoutList::ChildLayoutList(Component *c1, Component *c2)
{
	this->mChildList.push_back(c1); this->mChildList.push_back(c2);

}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
ChildLayoutList::ChildLayoutList(Component *c1, Component *c2, Component *c3)
{
	this->mChildList.push_back(c1); this->mChildList.push_back(c2); this->mChildList.push_back(c3);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
ChildLayoutList::ChildLayoutList(Component *c1, Component *c2, Component *c3, Component *c4)
{
	this->mChildList.push_back(c1); this->mChildList.push_back(c2); this->mChildList.push_back(c3); this->mChildList.push_back(c4);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
ChildLayoutList::ChildLayoutList(Component *c1, Component *c2, Component *c3, Component *c4, Component *c5)
{
	this->mChildList.push_back(c1); this->mChildList.push_back(c2); this->mChildList.push_back(c3); this->mChildList.push_back(c4); 
	this->mChildList.push_back(c5);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
ChildLayoutList::ChildLayoutList(Component *c1, Component *c2, Component *c3, Component *c4, Component *c5, Component *c6)
{
	this->mChildList.push_back(c1); this->mChildList.push_back(c2); this->mChildList.push_back(c3); this->mChildList.push_back(c4); 
	this->mChildList.push_back(c5); this->mChildList.push_back(c6);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
ChildLayoutList::ChildLayoutList(Component *c1, Component *c2, Component *c3, Component *c4, Component *c5, Component *c6, Component *c7)
{
	this->mChildList.push_back(c1); this->mChildList.push_back(c2); this->mChildList.push_back(c3); this->mChildList.push_back(c4); 
	this->mChildList.push_back(c5); this->mChildList.push_back(c6); this->mChildList.push_back(c7);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
ChildLayoutList::ChildLayoutList(Component *c1, Component *c2, Component *c3, Component *c4, Component *c5, Component *c6, Component *c7, Component *c8)
{
	this->mChildList.push_back(c1); this->mChildList.push_back(c2); this->mChildList.push_back(c3); this->mChildList.push_back(c4); 
	this->mChildList.push_back(c5); this->mChildList.push_back(c6); this->mChildList.push_back(c7); this->mChildList.push_back(c8);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void ChildLayoutList::Rewind() const
{
	this->mItr = this->mChildList.begin();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool ChildLayoutList::HasMore() const
{	
	return this->mItr!=this->mChildList.end();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
Component* ChildLayoutList::GetNext() const
{
	if(this->mItr==this->mChildList.end())
		return NULL;
	else
		return *this->mItr++;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void ChildLayoutList::AddTo(ChildList &theList) const
{
	this->Rewind();
	while(this->HasMore())
		theList.push_back(this->GetNext());
}

///////////////////////////////////////////////////////////////////////////////

}

namespace WONAPI {
// The other percentage parameter and layout constructors (not identified in the original exe)
PercentLayoutParam::PercentLayoutParam(Component *ref, int px, int py, int pw, int ph, int lp, int tp, int rp, int bp)
{
	this->Init(ref,ref,ref,ref,px,py,pw,ph,lp,tp,rp,bp);
}

PercentLayoutParam::PercentLayoutParam(Component *ref1, Component *ref2, int px, int py, int pw, int ph, int lp, int tp, int rp, int bp)
{
	this->Init(ref1,ref2,ref1,ref2,px,py,pw,ph,lp,tp,rp,bp);
}

PercentLayout::PercentLayout(Component *target, Component *ref, int px, int py, int pw, int ph, int lp, int tp, int rp, int bp)
: mParam(ref,px,py,pw,ph,lp,tp,rp,bp) 
{
	this->mTarget = target;
}

PercentLayout::PercentLayout(Component *target, Component *ref1, Component *ref2, int px, int py, int pw, int ph, int lp, int tp, int rp, int bp)
: mParam(ref1,ref2,px,py,pw,ph,lp,tp,rp,bp) 
{
	this->mTarget = target;
}

PercentLayout::PercentLayout(Component *target, Component *left, Component *top, Component *right, Component *bottom, int px, int py, int pw, int ph, int lp, int tp, int rp, int bp)
: mParam(left,top,right,bottom,px,py,pw,ph,lp,tp,rp,bp) 
{
	this->mTarget = target;
}
}
