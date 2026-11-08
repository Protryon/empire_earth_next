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
namespace WONAPI {
// FUNCTION: WON 0x00737370
// Sets the four anchor components; a missing edge takes the opposite edge on the same axis, then the last
// anchor supplied
void PercentLayoutParam::SetBorders(Component* inLeft, Component* inTop, Component* inRight, Component* inBottom)
{
    this->left=inLeft; this->top=inTop; this->right=inRight; this->bottom=inBottom;
    Component* last=NULL;
    if(inLeft==NULL) this->left=inRight; else last=inLeft;
    if(inRight==NULL) this->right=this->left; else last=inRight;
    if(inTop==NULL) this->top=inBottom; else last=inTop;
    if(inBottom==NULL) this->bottom=this->top; else last=inBottom;
    if(this->left==NULL) this->left=last;
    if(this->right==NULL) this->right=last;
    if(this->top==NULL) this->top=last;
    if(this->bottom==NULL) this->bottom=last;
}
// FUNCTION: WON 0x00737430
// Positions and sizes the target as percentages of the span between its padded anchor edges; a negative
// percentage leaves that coordinate unchanged (the padding is added even when it is the default -1)
void PercentLayoutParam::DoCalc(Component* target, Component* parent)
{
    int x=target->Left(), y=target->Top(), width=target->Width(), height=target->Height();
    if(this->px>=0 || this->pw>=0)
    {
        int leftEdge=this->left==parent?0:this->left->Left();
        int rightEdge=this->right==parent?parent->Width():this->right->Left()+this->right->Width();
        leftEdge+=this->lp;
        rightEdge-=this->rp;
        if(this->px>=0) x=leftEdge+(rightEdge-leftEdge)*this->px/100;
        if(this->pw>=0) width=(rightEdge-leftEdge)*this->pw/100;
    }
    if(this->py>=0 || this->ph>=0)
    {
        int topEdge=this->top==parent?0:this->top->Top();
        int bottomEdge=this->bottom==parent?parent->Height():this->bottom->Top()+this->bottom->Height();
        topEdge+=this->tp;
        bottomEdge-=this->bp;
        if(this->py>=0) y=topEdge+(bottomEdge-topEdge)*this->py/100;
        if(this->ph>=0) height=(bottomEdge-topEdge)*this->ph/100;
    }
    target->SetPosSize(x,y,width,height);
}
// FUNCTION: WON 0x00737800
// Gives every listed component the largest width and/or height among them, per the horizontal and
// vertical flags
void SameSizeLayout::Execute(Component*)
{
    int width=0,height=0;
    std::list<Component*>::iterator i;
    for(i=this->mComponentList.begin();i!=this->mComponentList.end();++i)
    {
        Component* child=*i;
        if(this->mHorz && child->Width()>width) width=child->Width();
        if(this->mVert && child->Height()>height) height=child->Height();
    }
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
    int right=0;
    for(std::list<Component*>::iterator i=this->mComponentList.begin();i!=this->mComponentList.end();++i)
    {
        Component* child=*i;
        int r=child->Right();
        if(r>right) right=r;

    }
    this->mComponent->SetPos(right+this->mPad,this->mComponent->Top());
}
// FUNCTION: WON 0x00737920
// Spreads the listed components evenly across the space box (horizontally or vertically), handing the
// leftover pixels one each to the first gaps
void SpaceEvenlyLayout::Execute(Component* parent)
{
    if(this->mComponentList.empty()) return;
    int total=0;
    std::list<Component*>::iterator i;
    for(i=this->mComponentList.begin();i!=this->mComponentList.end();++i)
        total+=this->mIsHorz?(*i)->Width():(*i)->Height();
    Component* box=this->mSpaceBox;
    int available=(this->mIsHorz?box->Width():box->Height())-this->mRightPad-this->mLeftPad-total;
    if(available<0) available=0;
    int spacing=0,remainder=0;
    if(this->mComponentList.size()>1)
    {
        spacing=available/(this->mComponentList.size()-1);
        remainder=available%(this->mComponentList.size()-1);
    }
    int position=this->mIsHorz?box->Left():box->Top();
    if(box==parent) position=0;
    position+=this->mLeftPad;
    for(i=this->mComponentList.begin();i!=this->mComponentList.end();++i)
    {
        Component* child=*i;
        if(this->mIsHorz) child->SetPos(position,child->Top());
        else child->SetPos(child->Left(),position);
        if(this->mIsHorz) position+=child->Width()+spacing;
        else position+=child->Height()+spacing;
        if(remainder>0) { ++position; --remainder; }
    }
}
// The shift is clamped one way for the parent and the other way for a sibling reference
// FUNCTION: WON 0x00737a40
// Pushes the target (and the components that move with it) clear of the reference component, or back
// inside the parent when the reference is the parent, along the requested sides
void NoOverlapLayout::Execute(Component* parent)
{
    int dx=0,dy=0;
    if(this->mRef==parent)
    {
        if(this->mType&0x800) { dx=this->mHorzPad-this->mTarget->Left(); if(dx<0) dx=0; }
        else if(this->mType&0x400) { const WONRectangle& b=this->mTarget->GetBounds(); dx=this->mRef->Width()-b.width+this->mHorzPad-b.x; if(dx>0) dx=0; }
        if(this->mType&0x100) { dy=this->mVertPad-this->mTarget->Top(); if(dy<0) dy=0; }
        else if(this->mType&0x200) { const WONRectangle& b=this->mTarget->GetBounds(); dy=this->mRef->Height()-b.height-b.y+this->mVertPad; if(dy>0) dy=0; }
    }
    else
    {
        if(this->mType&0x800) { const WONRectangle& b=this->mTarget->GetBounds(); dx=this->mRef->Left()-b.width+this->mHorzPad-b.x; if(dx>0) dx=0; }
        else if(this->mType&0x400) { dx=this->mRef->Right()-this->mTarget->Left()+this->mHorzPad; if(dx<0) dx=0; }
        if(this->mType&0x100) { const WONRectangle& b=this->mTarget->GetBounds(); dy=this->mRef->Top()-b.height-b.y+this->mVertPad; if(dy>0) dy=0; }
        else if(this->mType&0x200) { dy=this->mRef->Height()-this->mTarget->Top()+this->mRef->Top()+this->mVertPad; if(dy<0) dy=0; }
    }
    if(dx==0 && dy==0) return;
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
    int width=0,height=0;
    for(std::list<Component*>::iterator i=this->mComponentList.begin();i!=this->mComponentList.end();++i)
    {
        Component* child=*i;
        if(child->Width()>width) width=child->Width();
        if(child->Height()>height) height=child->Height();
    }
    this->mTarget->SetSizeAtLeast(width+this->mHorzPad,height+this->mVertPad);
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
    for(; anItr!=this->mComponentList.end(); ++anItr)
    {
        aTop += anItr->mTopPad;
        Component* aComp = anItr->mComponent;
        int aLeftPad = anItr->mLeftPad;
        if(aLeftPad>=0)
            aComp->SetPos(aLeft+aLeftPad,aTop);
        else
            aComp->SetPos(aComp->Left(),aTop);
        aTop += aComp->Height();
    }
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
