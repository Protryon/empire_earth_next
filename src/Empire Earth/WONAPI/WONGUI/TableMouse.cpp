// Table mouse handling: hover over text segments, child hit tests and drag selection (file name inferred)
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
#include "ee/wonapi/WONLobbyHTMLImageSegments.h"
namespace WONAPI {
// FUNCTION: WON 0x0070ff20
// Finds the text segment under the mouse (ignoring it while a modal popup is up unless it allows modal input),
// moves the hover from the old segment to it and records the segment-relative offset
void Table::MouseAt(int &x, int &y, bool mouseDown)
{
	LineSegment::mTableContext = this;
	if(this->mContents.get()!=NULL)
	{
		int oldx = x;
		int oldy = y;
		this->ScrollTranslate(x,y);
		LineSegment *aSeg = this->mContents->GetSegmentAt(x,y,mouseDown);
		if(this->mModalCount && aSeg!=NULL && !aSeg->AllowModalInput()) // fix for when combo box popup is up
			aSeg = NULL;

		if(aSeg!=this->mMouseSegment)
		{
			if(this->mMouseSegment!=NULL)
			{
				this->mMouseSegment->MouseExit();
				this->SetCursor(this->GetDefaultCursor());
			}

			this->mMouseSegment = aSeg;
			if(this->mMouseSegment!=NULL)
			{
				if(mouseDown && (x<0 || y<0 || x>this->mMouseSegment->GetWidth() || y>this->mMouseSegment->GetHeight()))
					return;

				this->mMouseSegment->MouseEnter(x,y);
			}
		}
		
		this->mMouseSegmentDX = x-oldx;
		this->mMouseSegmentDY = y-oldy;
	}
	else
		this->mMouseSegment = NULL;
}

// FUNCTION: WON 0x00710040
// Returns the component embedded in the contents at a point, or NULL
Component* Table::FindClickChildAt(int x, int y) 
{ 
	if(this->mContents.get()==NULL)
		return NULL;
	LineSegment::mTableContext = this;
	this->ScrollTranslate(x,y);
	ComponentLineSegment *aSeg = dynamic_cast<ComponentLineSegment*>(this->mContents->GetSegmentAt(x,y,false));
	if(aSeg!=NULL)
		return aSeg->mComponent.get();

	return NULL;
}

// FUNCTION: WON 0x007100b0
// Handles the mouse entering the table and updates the hovered segment
void Table::MouseEnter(int x, int y)
{
	Container::MouseEnter(x,y);
	this->MouseAt(x,y);
}

// FUNCTION: WON 0x007100e0
// Handles the mouse leaving the table: stops selecting and tells the hovered segment (which stays recorded)
void Table::MouseExit()
{
	Container::MouseExit();
	this->mSelecting = false;
	if(this->mMouseSegment!=NULL)
	{
		this->mMouseSegment->MouseExit();
	}
}

// FUNCTION: WON 0x00710100
// Starts a text selection on a left click over selectable text (dropping child focus and starting the
// autoscroll timer); otherwise passes the click to the segment and clears any selection
void Table::MouseDown(int x, int y, int theButton)
{
	Container::MouseDown(x,y,theButton);
	this->MouseAt(x,y,true);
	if(this->mMouseSegment!=NULL)
	{	
		if(theButton==MouseButton_Left)
		{
			bool canStartSelection = this->mMouseSegment->CanStartSelection();
			if(!canStartSelection)
			{
				if(x<0 || y<0 || x>this->mMouseSegment->GetWidth() || y>this->mMouseSegment->GetHeight())
					canStartSelection = true;
			}

			if(canStartSelection && this->mMouseSegment->mStartSelIndex>=0)
			{
				if(this->mFocusChild!=NULL)
				{
					this->mFocusChild->LostFocus();
					this->mFocusChild = NULL;
				}

				this->mStartSelSegment = this->mEndSelSegment = this->mMouseSegment->mStartSelIndex;
				this->mStartSelSegmentPos = this->mEndSelSegmentPos = this->mMouseSegment->GetSelChar(x,y);
				this->mBackwardsSel = false;
				this->mMouseSegment->MouseExit();
				this->mMouseSegment = NULL;
				this->mSelecting = true;
				this->RequestTimer(true);
				this->Invalidate();
				return;
			}
		}

		this->mMouseSegment->MouseDown(x,y,static_cast<MouseButton>(theButton));
	}
	
	if(this->mStartSelSegment>=0)
	{
		this->mStartSelSegment = this->mEndSelSegment = -1;
		this->Invalidate();
	}
}

// FUNCTION: WON 0x00710250
// Ends a left-button selection and passes the release to the segment under the mouse
void Table::MouseUp(int x, int y, int theButton)
{
	Container::MouseUp(x,y,theButton);

	if(theButton==MouseButton_Left)
		this->mSelecting = false;

	this->MouseAt(x,y);
	if(this->mMouseSegment!=NULL)
	{
		this->mMouseSegment->MouseUp(x,y,static_cast<MouseButton>(theButton));
	}
}

// FUNCTION: WON 0x007102b0
// Updates the hovered segment and passes the move to it
void Table::MouseMove(int x, int y)
{
	Container::MouseMove(x,y);
	this->MouseAt(x,y);
	if(this->mMouseSegment!=NULL)
	{
		this->mMouseSegment->MouseMove(x,y);
	}
}

// FUNCTION: WON 0x00710300
// Passes the drag to the hovered segment, or while selecting moves the selection's free end to the text
// under the mouse, swapping ends when it crosses the anchor
void Table::MouseDrag(int x, int y)
{
	Container::MouseDrag(x,y);
	if(this->mMouseSegment!=NULL)
	{
		LineSegment::mTableContext = this;
		x+=this->mMouseSegmentDX;
		y+=this->mMouseSegmentDY;
		this->mMouseSegment->MouseDrag(x,y);
	}
	else if(this->mStartSelSegment>=0)
	{
		this->ScrollTranslate(x,y);
		LineSegment *aSeg = this->mContents->GetSegmentAt(x,y,true);
		if(aSeg!=NULL && aSeg->mStartSelIndex>=0)
		{
			if(this->mBackwardsSel)
			{
				this->mStartSelSegment = aSeg->mStartSelIndex;
				this->mStartSelSegmentPos = aSeg->GetSelChar(x,y);
			}
			else
			{
				this->mEndSelSegment = aSeg->mStartSelIndex;
				this->mEndSelSegmentPos = aSeg->GetSelChar(x,y);
			}

			if(this->mEndSelSegment<this->mStartSelSegment || (this->mEndSelSegment==this->mStartSelSegment && this->mEndSelSegmentPos<this->mStartSelSegmentPos))
			{
				int temp = this->mStartSelSegment; this->mStartSelSegment = this->mEndSelSegment; this->mEndSelSegment = temp;
				temp = this->mStartSelSegmentPos; this->mStartSelSegmentPos = this->mEndSelSegmentPos; this->mEndSelSegmentPos = temp;
				this->mBackwardsSel = !this->mBackwardsSel;
			}
			this->Invalidate();
		}
	}
}
}
