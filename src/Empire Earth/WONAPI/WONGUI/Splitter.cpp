// Splitter container: quadrants, draggable splitter bars and maximize buttons (file name inferred)
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
#include "ee/wonapi/WONComponentFlags.h"
namespace WONAPI {

// FUNCTION: WON 0x00707300
// Registers a button that maximizes a quadrant, giving it control id 100005 + the quadrant position
void Splitter::RegisterMaxButton(Button *theButton, QuadrantPosition thePosition)
{
	theButton->mControlId=thePosition + 100005;
	this->mButtonList.push_back(theButton);
}

// FUNCTION: WON 0x00707440
// Adds a component as the pane for a quadrant position
void Splitter::AddQuadrant(Component *theComponent, QuadrantPosition thePosition)
{
	this->mQuadrantList.push_back(Quadrant(theComponent,thePosition));
	this->AddChild(theComponent);
}

// FUNCTION: WON 0x007074b0
// Adds a splitter bar with its drag handle (resize cursor if none set); the first bar of each kind sets the bar size
void Splitter::AddSplitterPrv(Component *theSplitter, SplitterType theType, Component *theDragComponent)
{
	this->mSplitterList.push_back(SplitterParam(theSplitter,theType));
	SplitterParam &aParam = this->mSplitterList.back();

	theDragComponent->SetComponentFlags(ComponentFlag_ParentDrag,true);
	if(theDragComponent->GetDefaultCursor()==NULL)
	{
		if(theType==Splitter_Center)
			theDragComponent->SetDefaultCursor(Cursor::GetStandardCursor(StandardCursor_NorthSouthEastWest));
		else if(theType & Splitter_LeftRight)
			theDragComponent->SetDefaultCursor(Cursor::GetStandardCursor(StandardCursor_EastWest));
		else if(theType & Splitter_UpDown)
			theDragComponent->SetDefaultCursor(Cursor::GetStandardCursor(StandardCursor_NorthSouth));
	}
	aParam.mDragComponent = theDragComponent;

	this->AddChild(theSplitter);

	if(this->mLeftRightSplitterWidth==0 && (theType & Splitter_LeftRight))
		this->mLeftRightSplitterWidth = theSplitter->Width();

	if(this->mUpDownSplitterHeight==0 && (theType & Splitter_UpDown))
		this->mUpDownSplitterHeight = theSplitter->Height();
}

// FUNCTION: WON 0x00707670
// Adds a splitter bar that is its own drag handle
void Splitter::AddSplitter(Component *theComponent, SplitterType theType)
{
	this->AddSplitterPrv(theComponent,theType,theComponent);
}

// FUNCTION: WON 0x00707690
// Adds a container as a splitter bar; without a drag handle a new one covers the whole container
void Splitter::AddComplexSplitter(Container *theContainer, SplitterType theType, Component *theDragComponent)
{
	if(theDragComponent==NULL)
	{
		SmartPtr<Component> aDragComponent = new Component;
		theContainer->Fn2F6140(aDragComponent.get(),3,theContainer,0,0,0,0,0);
		theContainer->AddChildEx(aDragComponent.get(),0);
		theDragComponent = aDragComponent.get();
	}

	this->AddSplitterPrv(theContainer,theType,theDragComponent);
}

// FUNCTION: WON 0x00707760
// Adds an empty square component of the given size as a splitter bar
void Splitter::AddInvisibleSplitter(SplitterType theType, int theSize)
{
	SmartPtr<Component> aComp = new Component;
	aComp->SetSize(theSize,theSize);
	this->AddSplitter(aComp.get(),theType);
}

// FUNCTION: WON 0x00707810
// Maximizes one quadrant: checks only its button, disables the bars beside it, hides other panes, moves the split
void Splitter::MaxQuadrant(QuadrantPosition thePosition)
{
	this->mHaveMaxQuadrant = true;
	this->mCurMaxQuadrant = thePosition;

	this->mLeftRightMax = this->mUpDownMax = true;

	int x = this->mSplitX, y = this->mSplitY;
	if(thePosition & Position_Left)
		x = this->Width()-this->mLeftRightSplitterWidth/2;
	else if(thePosition & Position_Right)
		x = this->mLeftRightSplitterWidth/2;
	else 
		this->mLeftRightMax = false;

	if(thePosition & Position_Top)
		y = this->Height()-this->mUpDownSplitterHeight/2;
	else if(thePosition & Position_Bottom)
		y = this->mUpDownSplitterHeight/2;
	else
		this->mUpDownMax = false;


	for(ButtonList::iterator aButtonItr = this->mButtonList.begin(); aButtonItr!=this->mButtonList.end(); ++aButtonItr)
	{
		Button *aButton = aButtonItr->get();
		int aPosition = aButton->mControlId - 100006 + Position_Left;
		aButton->SetCheck(aPosition==thePosition,false);
	}

	SplitterList::iterator aSplitterItr = this->mSplitterList.begin();
	while(aSplitterItr!=this->mSplitterList.end())
	{
		SplitterParam &aParam = *aSplitterItr;
		Component *aComponent = aParam.mComponent;
		if(aParam.mType & thePosition)
			aParam.mDragComponent->SetEnabled(false);
		else
			aParam.mDragComponent->SetEnabled(true);

		++aSplitterItr;
	}

	QuadrantList::iterator aQuadrantItr = this->mQuadrantList.begin();
	while(aQuadrantItr!=this->mQuadrantList.end())
	{
		Quadrant &aParam = *aQuadrantItr;
		Component *aComponent = aParam.mComponent;
		if(aParam.mPosition & thePosition)
			aComponent->SetVisible(true);
		else
			aComponent->SetVisible(false);

		++aQuadrantItr;
	}

	this->SetSplitterPos(x,y,false);
}

// FUNCTION: WON 0x00707990
// Restores all panes and bars and moves the split back to the stored percentages
void Splitter::UnMaxQuadrant()
{
	this->mHaveMaxQuadrant = false;
	this->mUpDownMax = this->mLeftRightMax = false;

	SplitterList::iterator aSplitterItr = this->mSplitterList.begin();
	while(aSplitterItr!=this->mSplitterList.end())
	{
		SplitterParam &aParam = *aSplitterItr;
		Component *aComponent = aParam.mComponent;
		aParam.mDragComponent->SetEnabled(true);

		++aSplitterItr;
	}

	QuadrantList::iterator aQuadrantItr = this->mQuadrantList.begin();
	while(aQuadrantItr!=this->mQuadrantList.end())
	{
		Quadrant &aParam = *aQuadrantItr;
		Component *aComponent = aParam.mComponent;
		aComponent->SetVisible(true);
		++aQuadrantItr;
	}

	this->SetSplitterPos(this->Width()*this->mLeftPercent/100,this->Height()*this->mTopPercent/100,false);
}

// FUNCTION: WON 0x00707a50
// Sizes each quadrant pane to its side of the current split
void Splitter::CalcQuadrants()
{
	QuadrantList::iterator anItr = this->mQuadrantList.begin();
	while(anItr!=this->mQuadrantList.end())
	{
		Quadrant &aParam = *anItr;
		Component *aComponent = aParam.mComponent;
		int aPosition = aParam.mPosition;

		int left = 0, right = this->Width(), top = 0, bottom = this->Height();

		if(aPosition & Position_Left)
			right = this->mSplitLeft;

		if(aPosition & Position_Right)
			left = this->mSplitRight;

		if(aPosition & Position_Top)
			bottom = this->mSplitTop;

		if(aPosition & Position_Bottom)
			top = this->mSplitBottom;

		aComponent->SetPosSize(left,top,right-left,bottom-top);
		++anItr;
	}

}

// FUNCTION: WON 0x00707ad0
// Places each visible splitter bar along the split, half-length bars covering only their quadrant side
void Splitter::CalcSplitterPositions()
{
	SplitterList::iterator anItr = this->mSplitterList.begin();
	while(anItr!=this->mSplitterList.end())
	{
		SplitterParam &aParam = *anItr;
		Component *aComponent = aParam.mComponent;
		if(aComponent->IsVisible())
		{
			int left = 0, top = 0, right = this->Width(), bottom = this->Height();

			if(aParam.mType & Splitter_LeftRight)
			{
				left = this->mSplitLeft;
				right = this->mSplitRight;
			}

			if(aParam.mType & Splitter_UpDown)
			{
				top = this->mSplitTop;
				bottom = this->mSplitBottom;
			}

			switch(aParam.mType)
			{
				case Splitter_LeftRight_Top: bottom = this->mSplitTop; break;
				case Splitter_LeftRight_Bottom: top = this->mSplitBottom; break;
				case Splitter_UpDown_Left: right = this->mSplitLeft; break;
				case Splitter_UpDown_Right: left = this->mSplitRight; break;
			}

			aComponent->SetPosSize(left,top,right-left,bottom-top);
		}
		++anItr;
	}
}

// FUNCTION: WON 0x00707c30
// Sets the minimum left and right pane widths
void Splitter::SetMinLeftRight(int minLeft, int minRight)
{
	this->mMinLeft = minLeft;
	this->mMinRight = minRight;
}

// FUNCTION: WON 0x00707c50
// Sets the minimum top and bottom pane heights
void Splitter::SetMinTopBottom(int minTop, int minBottom)
{
	this->mMinTop = minTop;
	this->mMinBottom = minBottom;
}

// FUNCTION: WON 0x00707c70
// Sets the split position as percentages of the width and height
void Splitter::SetPercentages(int theLeftPercent, int theTopPercent)
{
	this->mLeftPercent = theLeftPercent;
	this->mTopPercent = theTopPercent;
}

// FUNCTION: WON 0x00707c90
// Moves the split (clamped to the minimum pane sizes unless maximized), optionally storing it as percentages
void Splitter::SetSplitterPos(int x, int y, bool rememberPercentage)
{
	if(!this->mLeftRightMax)
	{
		if(x < this->mMinLeft)
			x = this->mMinLeft;
		if(x > this->Width()-this->mMinRight)
			x = this->Width()-this->mMinRight;
	}

	if(!this->mUpDownMax)
	{
		if(y < this->mMinTop)
			y = this->mMinTop;
		if(y > this->Height()-this->mMinBottom)
			y = this->Height()-this->mMinBottom;
	}

	if(this->mSplitCoordsValid && x==this->mSplitX && y==this->mSplitY)
		return;

	this->mSplitCoordsValid = true;
	this->mSplitX = x;
	this->mSplitY = y;
	this->mSplitLeft = x - this->mLeftRightSplitterWidth/2;
	this->mSplitRight = x + this->mLeftRightSplitterWidth/2;
	this->mSplitTop = y - this->mUpDownSplitterHeight/2;
	this->mSplitBottom = y + this->mUpDownSplitterHeight/2;

	if(rememberPercentage)
	{
		if(this->Width() > 0)
			this->mLeftPercent = x*100.0/this->Width();
		if(this->Height() > 0)
			this->mTopPercent = y*100.0/this->Height();
	}

	this->CalcSplitterPositions();
	this->CalcQuadrants();
	this->Invalidate();

}

// FUNCTION: WON 0x00707de0
// Lays out the splitter: puts the split at the stored percentages, or at the edge of a maximized quadrant
void Splitter::Virtual08()
{
	Container::Virtual08();

	this->mSplitCoordsValid = false;
	double aLeftPercent = this->mLeftPercent;
	double aTopPercent = this->mTopPercent;
	if(this->mHaveMaxQuadrant)
	{
		int xoff = 0, yoff = 0;
		if(this->mCurMaxQuadrant & Position_Left)
		{
			aLeftPercent = 100;
			xoff = -this->mLeftRightSplitterWidth/2;
		}
		else if(this->mCurMaxQuadrant & Position_Right)
		{
			aLeftPercent = 0;
			xoff = this->mLeftRightSplitterWidth/2;
		}

		if(this->mCurMaxQuadrant & Position_Top)
		{
			aTopPercent = 100;
			yoff = -this->mUpDownSplitterHeight/2;
		}
		else if(this->mCurMaxQuadrant & Position_Bottom)
		{
			aTopPercent = 0;
			yoff = this->mUpDownSplitterHeight/2;
		}

		this->SetSplitterPos(this->Width()*aLeftPercent/100 + xoff, this->Height()*aTopPercent/100 + yoff,false);
	}
	else		
		this->SetSplitterPos(this->Width()*aLeftPercent/100,this->Height()*aTopPercent/100,false);
}

// FUNCTION: WON 0x00707f30
// Starts a left-button drag when the pressed child is a splitter bar's drag handle
void Splitter::MouseDown(int x, int y, int theButton)
{
	Container::MouseDown(x,y,theButton);
	if(theButton!=0 || this->mMouseChild==NULL)
		return;

	this->mLeftRightDrag = this->mUpDownDrag = false;
	this->mDragX = x - this->mMouseChild->Left();
	this->mDragY = y - this->mMouseChild->Top();

	Component *aGrandChild = this->mMouseChild->GetMouseChild();
	if(aGrandChild==NULL)
	{
		if(!this->mMouseChild->ComponentFlagSet(ComponentFlag_ParentDrag))
			return;
	}
	else if(!aGrandChild->ComponentFlagSet(ComponentFlag_ParentDrag))
		return;

	SplitterList::iterator anItr = this->mSplitterList.begin();
	while(anItr!=this->mSplitterList.end())
	{
		SplitterParam &aParam = *anItr;
		if(aParam.mComponent==this->mMouseChild)
		{
			if(aParam.mType & Splitter_UpDown)
				this->mUpDownDrag = true;

			if(aParam.mType & Splitter_LeftRight)
				this->mLeftRightDrag = true;

			break;
		}

		++anItr;
	}
}

// FUNCTION: WON 0x00707ff0
// Ends a splitter drag on left-button release
void Splitter::MouseUp(int x, int y, int theButton)
{
	Container::MouseUp(x,y,theButton);
	if(theButton!=0 || this->mMouseChild==NULL)
		return;

	this->mLeftRightDrag = this->mUpDownDrag = false;
}

// FUNCTION: WON 0x00708030
// Moves the split with the mouse along the dragged bar's free axes
void Splitter::MouseDrag(int x, int y)
{
	Container::MouseDrag(x,y);
	if(this->mMouseChild==NULL)
		return;
	int aDeltaX = 0, aDeltaY = 0;
	if(this->mLeftRightDrag && !this->mLeftRightMax)
		aDeltaX = x - this->mMouseChild->Left() - this->mDragX;

	if(this->mUpDownDrag && !this->mUpDownMax)
		aDeltaY = y - this->mMouseChild->Top() - this->mDragY;

	this->SetSplitterPos(this->mSplitX+aDeltaX, this->mSplitY+aDeltaY);


}

// FUNCTION: WON 0x007080b0
// Handles a press of a registered maximize button: maximizes its quadrant when checked, else restores
bool Splitter::CheckMaxButton(ComponentEvent *theEvent)
{
	if(theEvent->mType!=ComponentEvent_ButtonPressed)
		return false;

	if(theEvent->GetControlId()<100006 || theEvent->GetControlId()>100017)
		return false;

	Button *aButton = dynamic_cast<Button*>(theEvent->mComponent);
	if(aButton==NULL)
		return false;
	
	if(!aButton->IsChecked())
	{
		this->UnMaxQuadrant();
		return true;
	}

	int aType = theEvent->GetControlId() - 100006 + Position_Left;
	this->MaxQuadrant((QuadrantPosition)aType);
	return true;
}

// FUNCTION: WON 0x00708140
// Handles maximize button presses; passes other events to the container
void Splitter::HandleComponentEvent(ComponentEvent *theEvent)
{
	if(this->CheckMaxButton(theEvent))
		return;
	else 
		Container::HandleComponentEvent(theEvent);
}
}

namespace WONAPI {
// FUNCTION: WON 0x00707060
// Creates an empty clipping splitter with the split at 50% / 50% and no minimum pane sizes
Splitter::Splitter()
{
    this->mSplitLeft=0;
    this->mSplitRight=0;
    this->mSplitTop=0;
    this->mSplitBottom=0;
    this->mUpDownSplitterHeight=0;
    this->mLeftRightSplitterWidth=0;
    this->mHaveMaxQuadrant=false;
    this->mMinLeft=this->mMinRight=this->mMinTop=this->mMinBottom=0;
    this->mLeftPercent=this->mTopPercent=50;
    this->mUpDownMax=false;
    this->mLeftRightMax=false;
    this->mLeftRightDrag=false;
    this->mUpDownDrag=false;
    this->mSplitX=0;
    this->mSplitY=0;
    this->mSplitCoordsValid=false;
    this->SetComponentFlags(ComponentFlag_Clip,true);
}
// FUNCTION: WON 0x007071e0
// Destroys the splitter, releasing its button, quadrant and splitter bar lists
Splitter::~Splitter() {}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007071c0
// (protected: virtual void * __thiscall WONAPI::Splitter::`scalar deleting destructor'(unsigned int)    [??_GSplitter@WONAPI@@MAEPAXI@Z])
// TEMPLATE: WON 0x00707640
// (public: __thiscall WONAPI::Splitter::SplitterParam::~SplitterParam(void)    [??1SplitterParam@Splitter@WONAPI@@QAE@XZ])
// TEMPLATE: WON 0x0071b720
// (public: __thiscall std::list<struct WONAPI::Splitter::SplitterParam,class std::allocator<struct WONAPI::Splitter::SplitterParam> >::~list<struct WONAP)
// TEMPLATE: WON 0x007221c0
// (public: class std::list<struct WONAPI::Splitter::SplitterParam,class std::allocator<struct WONAPI::Splitter::SplitterParam> >::iterator __thiscall std)
// TEMPLATE: WON 0x00724cd0
// (void __cdecl std::_Construct(struct WONAPI::Splitter::SplitterParam *,struct WONAPI::Splitter::SplitterParam const &)    [?_Construct@std@@YAXPAUSplit)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00722070
// (protected: void __thiscall std::list<struct WONAPI::Splitter::Quadrant,class std::allocator<struct WONAPI::Splitter::Quadrant> >::_Incsize(unsigned in)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0071d320
// (protected: struct std::list<struct WONAPI::Splitter::SplitterParam,class std::allocator<struct WONAPI::Splitter::SplitterParam> >::_Node * __thiscall )
// TEMPLATE: WON 0x00722200
// (protected: void __thiscall std::list<struct WONAPI::Splitter::SplitterParam,class std::allocator<struct WONAPI::Splitter::SplitterParam> >::_Incsize(u)
