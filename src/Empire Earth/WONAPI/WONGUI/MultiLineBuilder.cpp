// Rich-text line builder: breaks segments into lines around left/right floating segments (file name inferred)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyLineLayout.h"
namespace WONAPI {

// FUNCTION: WON 0x0070be50
// Sets the left and right indents and recomputes the current line margins
void MultiLineBuilder::SetIndentSize(int theLeftIndent, int theRightIndent)
{
	this->mLeftIndent = theLeftIndent;
	this->mRightIndent = theRightIndent;
	this->CalcMargins();

}

// FUNCTION: WON 0x0070BE70
// Finishes the current line and shifts it for center/right alignment, widening the block if the line sticks out
void MultiLineBuilder::AlignCurLine()
{
	if(!this->mLineVector->empty()) // Calc alignment
	{
		this->CurLine()->FinishBuild();
		LineElement *anElement = this->CurLine()->mLine.get();
		int aDelta = 0;
		switch(this->mAlignment)
		{
			case HorzAlign_Center: 
			{
				int anExtraWidth = this->GetCurMaxLineWidth() - anElement->GetWidth();
				aDelta = anExtraWidth>>1; 
				if((anExtraWidth&1) && (anElement->GetWidth()&1) && aDelta<anExtraWidth)
					aDelta++;
				
				break;
			}
			case HorzAlign_Right: aDelta = this->GetCurMaxLineWidth() - anElement->GetWidth(); break;//(mSpecifiedWidth - mRightMargin - mRightIndent - anElement->GetWidth()); break;
		}
		anElement->mLeftMargin = aDelta + this->mLeftMargin;//+ mLeftIndent;
		if(anElement->mLeftMargin<0)
			anElement->mLeftMargin = 0;

		if(anElement->GetWidth() + anElement->mLeftMargin > this->mWidth)
			this->mWidth = anElement->GetWidth() + anElement->mLeftMargin;
	}
}

// FUNCTION: WON 0x0070BF20
// Places the floating segments deferred while a line was being filled; returns false when there were none
bool MultiLineBuilder::FlushDeferredItems()
{
	if(this->mHaveDeferredItems)
	{
		if(!this->mDeferredLeftItems.empty())
		{
			for(LineSegmentList::iterator anItr = this->mDeferredLeftItems.begin(); anItr!=this->mDeferredLeftItems.end(); ++anItr)
				this->AddFloatingSegment(*anItr,true);

			this->mDeferredLeftItems.clear();
		}
		if(!this->mDeferredRightItems.empty())
		{
			for(LineSegmentList::iterator anItr = this->mDeferredRightItems.begin(); anItr!=this->mDeferredRightItems.end(); ++anItr)
				this->AddFloatingSegment(*anItr,false);
			
			this->mDeferredRightItems.clear();
		}
		this->mHaveDeferredItems = false;
		return true;
	}
	else
		return false;
}

// FUNCTION: WON 0x0070C000
// Aligns the current line, starts a new one below it (optionally clearing left/right floats) and returns it
LineElement* MultiLineBuilder::AddLine(bool clearLeft, bool clearRight)
{
	this->AlignCurLine();
	if(this->mHaveDeferredItems)
	{
		this->CurLine()->Clear();
		this->FlushDeferredItems();
	}

	if(clearLeft && this->mHeight<this->mMaxLeftHeight)
		this->mHeight = this->mMaxLeftHeight;

	if(clearRight && this->mHeight<this->mMaxRightHeight)
		this->mHeight = this->mMaxRightHeight;

	this->CalcMargins();
	LineElement *anElement = new LineElement;
	anElement->my = this->mHeight;

	this->mLineVector->push_back(anElement);
	this->mCurLine.StartBuild(anElement);

	return anElement;
}

// FUNCTION: WON 0x0070C150
// Appends a segment to the current line, growing the block's width and height to fit
void MultiLineBuilder::AddSegmentToCurLine(LineSegment *theSegment)
{

	int anOldHeight = this->CurLine()->GetHeight();
	this->CurLine()->AddSegment(theSegment);
	int aWidth = this->CurLine()->GetWidth()+this->mLeftMargin;//+mLeftIndent;
	int aNewHeight = this->CurLine()->GetHeight();
	
	if(aWidth>this->mWidth)
	{


		if(aWidth>this->mWidth)
			this->mWidth = aWidth;
	}

	if(aNewHeight!=anOldHeight)
		this->mHeight += aNewHeight-anOldHeight;
}

// FUNCTION: WON 0x0070C1B0
// Recomputes the left/right margins from the floats still beside the current height, dropping floats already passed
void MultiLineBuilder::CalcMargins()
{
	FloatingList::iterator anItr;

	int aBottom;
	int aLeftMargin = 0;
	int aRightMargin = this->mSpecifiedWidth;
	for(anItr=this->mCurLeftItems.begin(); anItr!=this->mCurLeftItems.end(); )
	{
		FloatingSegment *aSegment = &*anItr;
		aBottom = aSegment->my + aSegment->GetHeight();
		if(aBottom > this->mHeight)
		{
			if(aSegment->mx + aSegment->GetWidth() > aLeftMargin)
				aLeftMargin = aSegment->mx + aSegment->GetWidth();

			if(aBottom>this->mMaxLeftHeight)
				this->mMaxLeftHeight = aBottom;

			++anItr;
		}
		else
			this->mCurLeftItems.erase(anItr++);
	}

	for(anItr=this->mCurRightItems.begin(); anItr!=this->mCurRightItems.end(); )
	{
		FloatingSegment *aSegment = &*anItr;
		aBottom = aSegment->my + aSegment->GetHeight();
		if(aBottom > this->mHeight)
		{
			if(aSegment->mx < aRightMargin)
				aRightMargin = aSegment->mx;

			if(aBottom>this->mMaxRightHeight)
				this->mMaxRightHeight = aBottom;

			++anItr;
		}
		else
			this->mCurRightItems.erase(anItr++);
	}

	if(aLeftMargin<this->mLeftIndent)
		aLeftMargin = this->mLeftIndent;
	if(this->mSpecifiedWidth-aRightMargin<this->mRightIndent)
		aRightMargin = this->mSpecifiedWidth-this->mRightIndent;

	this->mLeftMargin = aLeftMargin;
	this->mRightMargin = aRightMargin;
}

// FUNCTION: WON 0x0070C310
// Places a segment floating at the left or right edge, or defers it until the current non-empty line ends
void MultiLineBuilder::AddFloatingSegment(LineSegment *theSegment, bool left)
{

	if(!this->CurLine()->IsEmpty())
	{
		this->mHaveDeferredItems = true;
		if(left)
			this->mDeferredLeftItems.push_back(theSegment);
		else
			this->mDeferredRightItems.push_back(theSegment);

		return;
	}

	int aWidth = theSegment->GetWidth();
	if(left)
	{

		if(this->mLeftMargin+aWidth>this->mRightMargin)
			this->mLeftMargin = this->mRightMargin-aWidth;
		if(this->mLeftMargin<0)
			this->mLeftMargin = 0;

		if(this->mLeftMargin+aWidth>this->mWidth)
			this->mWidth = this->mLeftMargin+aWidth;

		this->mLeftItems->push_back(FloatingSegment(theSegment,this->mLeftMargin,this->mHeight));
		this->mCurLeftItems.push_back(this->mLeftItems->back());
	}
	else 
	{
		if(this->mWidth<this->mSpecifiedWidth)
			this->mWidth = this->mSpecifiedWidth;

		if(this->mRightMargin-aWidth<this->mLeftMargin)
		{
			this->mRightMargin = this->mLeftMargin+aWidth;
			if(this->mRightMargin>this->mWidth)
				this->mWidth = this->mRightMargin;
		}

		this->mRightItems->push_back(FloatingSegment(theSegment,this->mRightMargin - aWidth,this->mHeight));
		this->mCurRightItems.push_back(this->mRightItems->back());
	}

	this->CalcMargins();
}

// FUNCTION: WON 0x0070C560
// Lays out a segment list into lines and floating items within a maximum width (per-segment float/break/clear flags)
void MultiLineBuilder::AddSegments(SegmentList *theSegmentList, LineVector *theLineVector,
								   FloatingVector *theLeftItems, FloatingVector *theRightItems,
								   int theMaxWidth)
{
	this->mSegmentList = theSegmentList;
	this->mLineVector = theLineVector;
	this->mLeftItems = theLeftItems;
	this->mRightItems = theRightItems;
	this->mSpecifiedWidth = theMaxWidth;
	this->mWidth = this->mHeight = 0;
	this->mLeftMargin = this->mRightMargin = 0;
	this->mLeftIndent = this->mRightIndent = 0;
	this->mMaxLeftHeight =  this->mMaxRightHeight = 0;
	this->mAlignment = HorzAlign_Left;
	this->mHaveDeferredItems = false;

	this->mLeftItems->clear();
	this->mRightItems->clear();
	this->mLineVector->clear();

	this->AddLine();

	SegmentList::iterator anItr = this->mSegmentList->begin();
	while(anItr!=this->mSegmentList->end())
	{
		SegmentInfo &anInfo = *anItr;
		LineSegment *aSegment = anInfo.mSegment.get();
		unsigned short aFlags = anInfo.mFlags;

		aSegment->SetMaxWidth(theMaxWidth-this->mLeftIndent-this->mRightIndent);
		if(aFlags&4 && (!this->CurLine()->IsEmpty() || 
			(aFlags&0x10) || (aFlags&0x20)))
				this->AddLine(aFlags&0x10?true:false,aFlags&0x20?true:false);

		if(aSegment->PerformInstructions(this))
		{
			if(aFlags&(1 | 2))
				this->AddFloatingSegment(aSegment,aFlags&1?true:false);
			else
			{
				if(this->CurLine()->GetWidth()+aSegment->GetWidth() > this->GetCurMaxLineWidth())
					aSegment->Wrap(this, this->CurLine()->IsEmpty());
				else
					this->AddSegmentToCurLine(aSegment);
			}
		}

		if(aFlags&8 && (!this->CurLine()->IsEmpty() || 
			(aFlags&0x10) || (aFlags&0x20)))
				this->AddLine(aFlags&0x10?true:false,aFlags&0x20?true:false);
	
		++anItr;
	}

	this->AlignCurLine();
	if(this->mHaveDeferredItems)
	{
		this->CurLine()->Clear();
		this->FlushDeferredItems();
	}

	if(this->mHeight<this->mMaxLeftHeight)
		this->mHeight = this->mMaxLeftHeight;

	if(this->mHeight<this->mMaxRightHeight)
		this->mHeight = this->mMaxRightHeight;
}

// FUNCTION: WON 0x0070d1d0
// Re-lays out the element's segments for a maximum width; width becomes the larger of that and the content
void MultiLineElement::SetMaxWidth(int theMaxWidth)
{
	this->GetMinWidth();


	MultiLineBuilder aBuilder;
	aBuilder.AddSegments(&this->mSegmentList, &this->mLineVector, &this->mFloatingItems[0], &this->mFloatingItems[1], theMaxWidth);

	if(aBuilder.mWidth > theMaxWidth)
		this->mWidth = aBuilder.mWidth;
	else
		this->mWidth = theMaxWidth;

	this->mHeight = aBuilder.mHeight;
}

}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0071c9b0
// (public: void __thiscall std::list<class WONAPI::LineSegment *,class std::allocator<class WONAPI::LineSegment *> >::_Insert(class std::list<class WONAP)
// TEMPLATE: WON 0x0071cba0
// (public: class std::_Ptrit<struct WONAPI::FloatingSegment,int,struct WONAPI::FloatingSegment *,struct WONAPI::FloatingSegment &,struct WONAPI::Floating)
// TEMPLATE: WON 0x0071f570
// (public: class std::_Ptrit<struct WONAPI::FloatingSegment,int,struct WONAPI::FloatingSegment *,struct WONAPI::FloatingSegment &,struct WONAPI::Floating)
// TEMPLATE: WON 0x00724060
// (struct WONAPI::FloatingSegment * __cdecl std::copy(class std::_Ptrit<struct WONAPI::FloatingSegment,int,struct WONAPI::FloatingSegment *,struct WONAPI)
// TEMPLATE: WON 0x00724280
// (void __cdecl std::fill(class std::_Ptrit<struct WONAPI::FloatingSegment,int,struct WONAPI::FloatingSegment *,struct WONAPI::FloatingSegment &,struct W)
// TEMPLATE: WON 0x007242f0
// (class std::_Ptrit<struct WONAPI::FloatingSegment,int,struct WONAPI::FloatingSegment *,struct WONAPI::FloatingSegment &,struct WONAPI::FloatingSegment )

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0071cb20
// (public: class std::_Ptrit<struct WONAPI::FloatingSegment,int,struct WONAPI::FloatingSegment *,struct WONAPI::FloatingSegment &,struct WONAPI::Floating)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0071ee80
// (protected: void __thiscall std::list<class WONAPI::LineSegment *,class std::allocator<class WONAPI::LineSegment *> >::_Incsize(unsigned int)    [?_Inc)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0071ca60
// (public: void __thiscall std::list<struct WONAPI::FloatingSegment,class std::allocator<struct WONAPI::FloatingSegment> >::_Insert(class std::list<struc)
// TEMPLATE: WON 0x0071f010
// (protected: void __thiscall std::list<struct WONAPI::FloatingSegment,class std::allocator<struct WONAPI::FloatingSegment> >::_Incsize(unsigned int))
// TEMPLATE: WON 0x00724200
// (void __cdecl std::_Construct(struct WONAPI::FloatingSegment *,struct WONAPI::FloatingSegment const &))
// TEMPLATE: WON 0x00724240
// (protected: struct WONAPI::FloatingSegment * __thiscall std::vector<struct WONAPI::FloatingSegment,class std::allocator<struct WONAPI::FloatingSegment>)
// TEMPLATE: WON 0x0071cac0
// (public: class std::_Ptrit<struct WONAPI::FloatingSegment,int,struct WONAPI::FloatingSegment *,struct WONAPI::FloatingSegment &,struct WONAPI::Floating)
// TEMPLATE: WON 0x0071cbc0
// (public: class std::_Ptrit<class WONAPI::SmartPtr<class WONAPI::LineElement>,int,class WONAPI::SmartPtr<class WONAPI::LineElement> *,class WONAPI::Smar)
// TEMPLATE: WON 0x0071f160
// (public: void __thiscall std::vector<struct WONAPI::FloatingSegment,class std::allocator<struct WONAPI::FloatingSegment> >::insert(class std::_Ptrit<st)
// TEMPLATE: WON 0x0071f590
// (public: void __thiscall std::vector<class WONAPI::SmartPtr<class WONAPI::LineElement>,class std::allocator<class WONAPI::SmartPtr<class WONAPI::LineEl)
// TEMPLATE: WON 0x0070d3e0
// (public: __thiscall WONAPI::MultiLineBuilder::~MultiLineBuilder(void))
