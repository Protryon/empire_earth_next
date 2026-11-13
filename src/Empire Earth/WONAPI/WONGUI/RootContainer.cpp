// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include <set>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONComponentFlags.h"
#include "rust/ee_won.h"
// The window drag's start, place and end and the cursor component's place are ee_won's guievent module
// (rust/crates/ee_won/src/guievent.rs); this unit keeps the root, the cursor component, the backgrounds and the calls.
using namespace WONAPI;

// FUNCTION: WON 0x007040d0
// Creates the top-level container of a window: arrow cursor, window dragging allowed, tab focus kept inside
RootContainer::RootContainer(Window *theWindow)
{
	this->mModalComponent = NULL;
	this->mWindow = theWindow;
	this->mCursorComponent = NULL;
	this->SetDefaultCursor(Cursor::GetStandardCursor(StandardCursor_Arrow));
	this->SetComponentFlags(0x8 | 0x40000,true);

	this->mAllowDrag = true;
	this->mDragX = this->mDragY = 0;
	this->mDragging = false;
	this->SetIsolateTabFocus(true);
}

// FUNCTION: WON 0x00704510
// Queues a grabber's background image for drawing under the children (keyed by image order), then clears the grabber
void RootContainer::AddBG(BackgroundGrabber *theGrabber)
{
	if(theGrabber==NULL || !theGrabber->NeedDrawBG())
		return;

	this->mBGStructMap[(int)theGrabber->mImageOrder] = BGStruct(theGrabber->mImageX,theGrabber->mImageY,theGrabber->mBGImage.get(), theGrabber->mClipRect);
	theGrabber->Clear();
}

// FUNCTION: WON 0x00704710
// Returns the child at a point (plain container lookup)
Component* RootContainer::FindChildAt(int x, int y)
{
	Component *aComponent = Container::FindChildAt(x,y);


	return aComponent;
}

// FUNCTION: WON 0x00704730
// Lays out the children and marks the whole window as needing a repaint
void RootContainer::Virtual08()
{
	Container::Virtual08();
	this->mInvalidRect=this->mBounds;
}

// FUNCTION: WON 0x00704760
// Draws the queued background images (highest order first), each clipped to its rectangle, then empties the queue
void RootContainer::PrePaint(Graphics &g)
{

	Container::PrePaint(g);
	std::map<int,BGStruct>::reverse_iterator anItr = this->mBGStructMap.rbegin();
	while(anItr!=this->mBGStructMap.rend())
	{
		BGStruct &aStruct = anItr->second;
		WONRectangle anImageRect(aStruct.mImageX,aStruct.mImageY,aStruct.mImage->mWidth,aStruct.mImage->mHeight);
		anImageRect.DoIntersect(aStruct.mClipRect);
		g.DrawImage(aStruct.mImage.get(), anImageRect.x, anImageRect.y, anImageRect.x - aStruct.mImageX, anImageRect.y - aStruct.mImageY, anImageRect.width, anImageRect.height); 


		++anItr;
	}
	this->mBGStructMap.clear();

}

// FUNCTION: WON 0x00704900
// Paints the window's invalid rectangle with the container's color scheme, then clears that rectangle
void RootContainer::Paint(Graphics &g)
{
	g.mColorScheme=this->GetColorScheme();
	g.PushClipRect(this->mInvalidRect.x, this->mInvalidRect.y, this->mInvalidRect.width, this->mInvalidRect.height);
	Container::Paint(g);
	g.PopClipRect();

	this->mInvalidRect.x = this->mInvalidRect.y = this->mInvalidRect.width = this->mInvalidRect.height = 0;
}

// FUNCTION: WON 0x00704970
// Forwards a child's invalidation to the container base
bool RootContainer::ChildInvalidated(Component *theChild)
{
	return Container::ChildInvalidated(theChild);
}

// FUNCTION: WON 0x00704980
// Grows the window's invalid rectangle to include the given one
void RootContainer::InvalidateRect(WONRectangle &theRect)
{
	this->mInvalidRect.DoUnion(theRect);
}

// FUNCTION: WON 0x007049a0
// Passes a key press to the children, then to the keyboard listener (event 0); always handled
bool RootContainer::KeyDown(int theKey)
{
	Container::KeyDown(theKey);

	if(this->mKeyboardListener.get()!=NULL)
		this->mKeyboardListener->KeyEvent(theKey,0);	

	return true;
}

// FUNCTION: WON 0x007049d0
// Passes a key release to the children, then to the keyboard listener (event 1); always handled
bool RootContainer::KeyUp(int theKey)
{
	Container::KeyUp(theKey);
	if(this->mKeyboardListener.get()!=NULL)
		this->mKeyboardListener->KeyEvent(theKey,1);

	return true;
}

// FUNCTION: WON 0x00704a00
// Passes a typed character to the children, then to the keyboard listener (event 2); always handled
bool RootContainer::KeyChar(int theKey)
{
	Container::KeyChar(theKey);
	if(this->mKeyboardListener.get()!=NULL)
		this->mKeyboardListener->KeyEvent(theKey,2);

	return true;
}

// FUNCTION: WON 0x00704a30
// Handles a mouse move and moves the software cursor component to the pointer, repainting old and new spots
void RootContainer::MouseMove(int x, int y)
{
	Container::MouseMove(x,y);
	if(this->mCursorComponent==NULL)
		return;
	int mx,my;
	this->GetCurrentCursor()->GetComponent(mx,my);
	x = ee_won_gui_root_cursor_place(x, y, mx, my, &y);

	this->mCursorComponent->Invalidate();
	this->mCursorComponent->SetPos(x,y);
	WONRectangle aRect(x,y,this->mCursorComponent->Width(),this->mCursorComponent->Height());
	this->InvalidateRect(aRect);
}

// FUNCTION: WON 0x00704ac0
// Drags the window by the mouse while a window drag is active; otherwise drags as usual and moves the cursor component
void RootContainer::MouseDrag(int x, int y)
{
	if(!this->mDragging)
	{
		Container::MouseDrag(x,y);
		if(this->mCursorComponent!=NULL)
		{
			int mx,my;
			this->GetCurrentCursor()->GetComponent(mx,my);
			x = ee_won_gui_root_cursor_place(x, y, mx, my, &y);

			this->mCursorComponent->Invalidate();
			this->mCursorComponent->SetPos(x,y);
			WONRectangle aRect(x,y,this->mCursorComponent->Width(),this->mCursorComponent->Height());
			this->InvalidateRect(aRect);
		}
	}
	else
	{
		int aNewX, aNewY;
		if(!ee_won_gui_root_drag_place(this->Left(), this->Top(), x, y, this->mDragX, this->mDragY, &aNewX, &aNewY))
			return;

		Window *aWindow = this->mWindow;
		WONRectangle r;
		aWindow->GetRect(r,false);
		int aWindowY;
		int aWindowX = ee_won_gui_root_drag_window(r.x, r.y, aNewX, aNewY, &aWindowY);
		aWindow->SetPosSize(aWindowX,aWindowY,-1,-1);
	}
}

// FUNCTION: WON 0x00704bd0
// On a left press over a child flagged for parent dragging, starts dragging the window
void RootContainer::MouseDown(int x, int y, int theButton)
{
	Container::MouseDown(x,y,theButton);
	if(theButton!=0 || !this->mAllowDrag)
		return;
	Window *aWindow = this->mWindow;
	Component *aMouseChild = this->GetMouseChild();
	if(aWindow->mIsOneComponentWindow && aMouseChild!=NULL)
		aMouseChild = aMouseChild->GetMouseChild();

	if(aMouseChild==NULL)
		return;
	if(!ee_won_gui_root_drag_starts(theButton, this->mAllowDrag, aMouseChild->ComponentFlagSet(ComponentFlag_ParentDrag)))
		return;
	this->mDragging = true;
	this->mDragX = x;
	this->mDragY = y;
}

// FUNCTION: WON 0x00704c40
// Handles the mouse entering the window (container behaviour)
void RootContainer::MouseEnter(int x, int y)
{
	Container::MouseEnter(x,y);
}

// FUNCTION: WON 0x00704c60
// Ends any window drag and removes the cursor component when the mouse leaves the window
void RootContainer::MouseExit()
{
	Container::MouseExit();
	this->mDragging = false;
	if(this->mCursorComponent!=NULL)
		this->RemoveChild(this->mCursorComponent);
}

// FUNCTION: WON 0x00704c90
// Handles a mouse release; releasing the left button ends a window drag
void RootContainer::MouseUp(int x, int y, int theButton)
{
	Container::MouseUp(x,y,theButton);

	if(ee_won_gui_root_drag_ends(theButton))
		this->mDragging = false;
}

// FUNCTION: WON 0x00704cc0
// Removes a child, forgetting it first if it is the cursor component
void RootContainer::RemoveChild(Component *theChild)
{

	if(theChild==this->mCursorComponent)
		this->mCursorComponent = NULL;

	Container::RemoveChild(theChild);
}

// FUNCTION: WON 0x00704cf0
// Sets the cursor; a cursor drawn by a component gets that component placed at the mouse on top of all children
void RootContainer::SetCursor(Cursor *theCursor)
{
	Container::SetCursor(theCursor);
	if(theCursor==NULL)
		theCursor = this->GetDefaultCursor();

	Component *aComponent = NULL;
	int mx = 0, my = 0;
	if(theCursor!=NULL)
		aComponent = theCursor->GetComponent(mx,my);

	if(this->mCursorComponent!=aComponent)
	{
		if(this->mCursorComponent!=NULL)
			this->RemoveChild(this->mCursorComponent);

		this->mCursorComponent = aComponent;
		if(aComponent!=NULL)
		{
			if(aComponent->mParent!=NULL)
			{
				aComponent->mParent->RemoveChild(aComponent);
				BackgroundGrabber *aBG = aComponent->GetGrabBG();
				if(aBG!=NULL)
				{
					aBG->Clear();
					aBG->mBGImage = NULL; 
				}
			}

			aComponent->SetComponentFlags(0x100 | 0x800 | 0x1000, true);
			aComponent->SetVisible(true);
			
			int y;
			int x = ee_won_gui_root_cursor_place(this->mWindow->mMouseX, this->mWindow->mMouseY, mx, my, &y);
			aComponent->SetPos(x,y);

			this->AddChild(aComponent);
			Component *aTop = this->GetBackChild();
			if(aTop!=aComponent)
				aComponent->EnsureOnTopOf(aTop);
		}
	}
}

// The implicit destructor (0x007043c0) destroys the background map, the set and the keyboard listener, then the Container base.

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00704230
// (public: virtual void * __thiscall WONAPI::RootContainer::`scalar deleting destructor'(unsigned int)    [??_GRootContainer@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x00704300
// (public: __thiscall std::map<int,struct WONAPI::RootContainer::BGStruct,struct std::less<int>,class std::allocator<struct std::pair<int const ,struct W)
// TEMPLATE: WON 0x007046e0
// (public: __thiscall WONAPI::RootContainer::BGStruct::~BGStruct(void)    [??1BGStruct@RootContainer@WONAPI@@QAE@XZ])
// TEMPLATE: WON 0x0071bd20
// (public: __thiscall std::pair<int const ,struct WONAPI::RootContainer::BGStruct>::~pair<int const ,struct WONAPI::RootContainer::BGStruct>(void)    [??)
// TEMPLATE: WON 0x0071c100
// (public: struct std::pair<class std::_Tree<int,struct std::pair<int const ,struct WONAPI::RootContainer::BGStruct>,struct std::map<int,struct WONAPI::R)
// TEMPLATE: WON 0x0071c1f0
// (public: class std::_Tree<int,struct std::pair<int const ,struct WONAPI::RootContainer::BGStruct>,struct std::map<int,struct WONAPI::RootContainer::BGS)
// TEMPLATE: WON 0x0071dca0
// (public: __thiscall WONAPI::RootContainer::BGStruct::BGStruct(struct WONAPI::RootContainer::BGStruct const &)    [??0BGStruct@RootContainer@WONAPI@@QAE)
// TEMPLATE: WON 0x0071e380
// (public: class std::_Tree<int,struct std::pair<int const ,struct WONAPI::RootContainer::BGStruct>,struct std::map<int,struct WONAPI::RootContainer::BGS)
// TEMPLATE: WON 0x0071e870
// (protected: void __thiscall std::_Tree<int,struct std::pair<int const ,struct WONAPI::RootContainer::BGStruct>,struct std::map<int,struct WONAPI::RootC)
// TEMPLATE: WON 0x0071e8b0
// (protected: class std::_Tree<int,struct std::pair<int const ,struct WONAPI::RootContainer::BGStruct>,struct std::map<int,struct WONAPI::RootContainer::)
// TEMPLATE: WON 0x00724d10
// (void __cdecl std::_Construct(struct std::pair<int const ,struct WONAPI::RootContainer::BGStruct> *,struct std::pair<int const ,struct WONAPI::RootCont)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0071e340
// (protected: void __thiscall std::_Tree<class WONAPI::Component *,class WONAPI::Component *,struct std::_Setk<class WONAPI::Component *,struct std::less)
// TEMPLATE: WON 0x00724fe0
// (public: void * __thiscall std::pair<int const ,struct WONAPI::RootContainer::BGStruct>::`scalar deleting destructor'(unsigned int)    [??_G?$pair@$$CB)
// TEMPLATE: WON 0x007043c0
// (RootContainer implicit destructor)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00704250
// (public: __thiscall std::set<class WONAPI::Component *,struct std::less<class WONAPI::Component *>,class std::allocator<class WONAPI::Component *> >::~)
