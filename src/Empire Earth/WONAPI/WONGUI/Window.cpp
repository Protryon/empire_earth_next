// Top-level GUI window: root container, input dispatch, cursor, resizing and closing
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
using namespace WONAPI;
// FUNCTION: WON 0x0072a020
// Destroys the window (members only)
Window::~Window()
{
}

// FUNCTION: WON 0x0072a230
// Returns the cached font for a descriptor, creating it through the platform on first use
SmartPtr<Font> Window::GetFont(const FontDescriptor &theDescriptor)
{
	std::map<FontDescriptor,SmartPtr<Font> >::iterator anItr = this->mFontMap.insert(std::map<FontDescriptor,SmartPtr<Font> >::value_type(theDescriptor, NULL)).first;
	SmartPtr<Font> aFont = anItr->second;
	if(aFont.get()==NULL)
	{
		aFont = this->PlatformGetFont(theDescriptor);
		anItr->second = aFont;
	}

	return aFont;
}

// FUNCTION: WON 0x0072ab80
// Returns whether every child window, the root container and the allow-close callback agree to close
bool Window::CanClose()
{
	bool childrenCanClose = true;
	for(std::set<Window*>::iterator anItr = this->mChildWindowSet.begin(); anItr!=this->mChildWindowSet.end(); ++anItr)
	{
		Window *aChild = *anItr;
		if(!aChild->CanClose())
			childrenCanClose = false;
	}


	if(!childrenCanClose)
		return false;

	if(!this->mRoot->CanClose())
		return false;

	if(this->mAllowCloseFunc!=NULL && !this->mAllowCloseFunc(this))
		return false;

	return true;
}

// FUNCTION: WON 0x0072ac30
// Starts closing an open window by queueing a window-close event; false when something refuses to close
bool Window::Close()
{
	if(this->mWindowState!=WindowState_Open)
		return true;

	if(!this->CanClose())
		return false;

	this->mWindowState = WindowState_Closing;
	this->QueueEvent(new InputEvent(InputEvent_WindowClose));
	return true;
}

// FUNCTION: WON 0x0072ac90
// Applies a pending resize once the delay passed and resizing is not suspended: new offscreen image and root size
void Window::CheckResize(DWORD theTick)
{
	if(!this->mNeedResize || !(theTick-this->mLastResizeTick>this->mResizeDelay) || !(this->mSuspendRootResize<=0))
		return;
	SmartPtr<NativeImage> anImage = this->CreateImage(this->mNewWidth,this->mNewHeight);
	Graphics &g = anImage->GetGraphics();
	g.DrawImage(this->mOffscreenImage.get(),0,0);
	this->mOffscreenImage = anImage;
	this->mRoot->SetSize(this->mNewWidth, this->mNewHeight);
	this->mRoot->Invalidate();
	this->mNeedResize = false;
	this->mLastResizeTick = GetTickCount();
}

// FUNCTION: WON 0x0072adc0
// Sets the window title, updating the platform window when open
void Window::SetTitle(const GUIString &theTitle)
{
	this->mTitle = theTitle;
	if(this->mWindowState==WindowState_Open)
		this->PlatformSetTitle();
}

// FUNCTION: WON 0x0072ae20
// Sets the window icon, updating the platform window when open
void Window::SetIcon(NativeIcon *theIcon)
{
	this->mIcon = theIcon;
	if(this->mWindowState==WindowState_Open)
		this->PlatformSetIcon();
}

// FUNCTION: WON 0x0072ae80
// Centres the window on the screen or on its visible parent, clamped to the top left
void Window::Center(bool inParent)
{
	WONRectangle aScreenRect, aWindowRect;
	if(!inParent || this->mParent==NULL || !this->mParent->PlatformIsVisible())
		this->mManager->GetScreenRect(aScreenRect);
	else
		this->mParent->GetRect(aScreenRect,false);

	this->GetRect(aWindowRect,false);

	int left = aScreenRect.Left() + (aScreenRect.Width() - aWindowRect.Width())/2;
	int top = aScreenRect.Top() + (aScreenRect.Height() - aWindowRect.Height())/2;
	if(left<0)
		left = 0;
	if(top<0)
		top = 0;

	this->SetPosSize(left,top,aWindowRect.Width(), aWindowRect.Height());
}

// FUNCTION: WON 0x0072af30
// Resizes the window, keeping its position
void Window::SetClientSize(int width, int height)
{
	WONRectangle aRect;
	this->GetRect(aRect,false);
	this->SetPosSize(aRect.x,aRect.y,width,height);
}

// FUNCTION: WON 0x0072af70
// Shows or hides the window; hiding ends any mouse-over or drag first
void Window::Show(bool show)
{
	this->mIsShowing = show;
	if(!show)
	{
		if(this->mMouseIsInClientArea || this->mMouseIsDown[0])
			this->HandleMouseExit();
	}

	this->PlatformShow(show);
}

// FUNCTION: WON 0x0072b120
// Handles the mouse leaving: shows the system cursor, releases all buttons and sends mouse-exit to the root
void Window::HandleMouseExit()
{
	this->ShowNativeCursor(true); // show cursor when it leaves the window

	this->mMouseIsInClientArea = false;
	this->mMouseX = (this->mRoot->Left()+this->mRoot->Width())+1;
	this->mMouseY = (this->mRoot->Top()+this->mRoot->Height())+1;
	this->mMouseIsDown[0] = this->mMouseIsDown[1] = this->mMouseIsDown[2] = false;
	this->mRoot->MouseExit();
	this->mCurrentCursor = NULL;
}

// FUNCTION: WON 0x0072b170
// Passes a key down, up or character to the root container
void Window::HandleKeyEvent(KeyEvent *theEvent)
{
	this->mKeyMod = theEvent->mKeyMod;
	switch(theEvent->mEventType)
	{
		case InputEvent_KeyDown:
			this->mRoot->KeyDown(theEvent->mKey);
			break;

		case InputEvent_KeyUp:
			this->mRoot->KeyUp(theEvent->mKey);
			break;

		case InputEvent_KeyChar:
			this->mRoot->KeyChar(theEvent->mKey);
			break;
	}
}

// FUNCTION: WON 0x0072b1c0
// Records a button press, detects a double click (same button within 250 ms) and passes the press to the root
void Window::HandleMouseDown(MouseEvent *theEvent)
{
	int aButton = theEvent->mButtonOrAmount;
	this->mMouseIsDown[aButton] = true;

	// Check for DoubleClick
	bool isDoubleClick = false;
	if(theEvent->mEventType==InputEvent_MouseDoubleClick)
		isDoubleClick = true;
	else if(theEvent->mEventType==InputEvent_MouseDownCheckDoubleClick)
	{
		if(!this->mDetectDoubleClick)
		{
			for(int i=0; i<3; i++)
				this->mButtonDownTick[i] = 0;

			this->mDetectDoubleClick = true;
			this->mDoubleClickX = theEvent->mx;
			this->mDoubleClickY = theEvent->my;
		}

		DWORD &aPrevTick = this->mButtonDownTick[aButton];
		DWORD aTick = GetTickCount();
		if(aTick==0) // 0 has special meaning
			aTick = 1;
		
		if(aPrevTick!=0 && aTick-aPrevTick<250)
		{
			isDoubleClick = true;
			aPrevTick = 0;
		}
		else
			aPrevTick = aTick;
	}
	
	this->mButtonDoubleClick[aButton] = isDoubleClick;
	this->mRoot->MouseDown(this->mMouseX, this->mMouseY, aButton);
}

// FUNCTION: WON 0x0072b260
// Dispatches a mouse move, press, release or wheel to the root, entering the window first; a move cancels
// double-click detection on any Y change (the && / || test binds that way)
void Window::HandleMouseEvent(MouseEvent *theEvent)
{
	this->mMouseX = theEvent->mx;
	this->mMouseY = theEvent->my;
	this->mKeyMod = theEvent->mKeyMod;

	if(!this->mMouseIsInClientArea && !this->mMouseIsDown[0])
	{
		this->mMouseIsInClientArea = true;
		this->mRoot->MouseEnter(this->mMouseX, this->mMouseY);
		this->CheckSetCursor(true);
	}

	switch(theEvent->mEventType)
	{		
		case InputEvent_MouseMove:
			this->mLastMoveTick = GetTickCount();
			if(this->mDetectDoubleClick && this->mMouseX!=this->mDoubleClickX || this->mMouseY!=this->mDoubleClickY)
				this->mDetectDoubleClick = false;

			if(this->mMouseIsDown[0])
				this->mRoot->MouseDrag(this->mMouseX, this->mMouseY); 
			else
				this->mRoot->MouseMove(this->mMouseX, this->mMouseY);
			break;

		case InputEvent_MouseDown:
		case InputEvent_MouseDoubleClick:
		case InputEvent_MouseDownCheckDoubleClick:
			this->HandleMouseDown(theEvent);
			break;

		case InputEvent_MouseUp:
		{
			this->mLastMoveTick = 0;

			int aButton = theEvent->mButtonOrAmount;
			this->mMouseIsDown[aButton] = false;

			if(aButton==0)
			{
				if(!this->mMouseIsInClientArea)
				{
					this->HandleMouseExit(); // Call MouseExit when dragging is done
					break;
				}
			}
			this->mRoot->MouseUp(this->mMouseX, this->mMouseY, aButton);			
		}
		break;

		case InputEvent_MouseWheel:
			this->mRoot->MouseWheel(theEvent->mButtonOrAmount);
			break;
	}

}

// FUNCTION: WON 0x0072b390
// Handles size change (pending resize), focus gain/loss and mouse exit (deferred while dragging)
void Window::HandleInputEvent(InputEvent *theEvent)
{
	switch(theEvent->mEventType)
	{
		case InputEvent_SizeChange: 
		{
			SizeEvent *anEvent = static_cast<SizeEvent*>(theEvent);
			if(anEvent->mWidth!=-1 && anEvent->mHeight!=-1)
			{
				if(this->mRoot->Width()!=anEvent->mWidth || this->mRoot->Height()!=anEvent->mHeight)
				{
					this->mNeedResize = true;
					this->mNewWidth = anEvent->mWidth;
					this->mNewHeight = anEvent->mHeight;
					this->CheckResize(GetTickCount());
				}
			}
		}
		break;
		
		case InputEvent_GotFocus:
			this->mRoot->GotFocus();
			break;

		case InputEvent_LostFocus:
			this->mRoot->LostFocus();				
			break;

		case InputEvent_MouseExit:
			this->mMouseIsInClientArea = false;
			if(!this->mMouseIsDown[0])
				this->HandleMouseExit();
			break;
	}
}

// FUNCTION: WON 0x0072b460
// Refreshes the cursor (forced once a second while the mouse is inside); returns whether the window is open
bool Window::IdleUpdate()
{
	if(this->mWindowState==WindowState_Open)
	{
		bool force = false;
		if(this->mMouseIsInClientArea)
		{
			DWORD aTick = GetTickCount();
			if(aTick - this->mLastCheckCursorTick >= 1000)
			{
				force = true;
				this->mLastCheckCursorTick = aTick; 
			}
		}
		
		this->CheckSetCursor(force);
		return true;
	}
	else
		return false;
}

// FUNCTION: WON 0x0072b4b0
// Sends mouse hover after 100 ms without motion, applies pending resizes and runs the root's timers
void Window::TimerEvent(DWORD theElapsed)
{
	if(this->mMouseIsInClientArea && this->mLastMoveTick!=0)
	{
		if(!this->mMouseIsDown[0] && !this->mMouseIsDown[1] && !this->mMouseIsDown[2])
		{
			DWORD anElapsed = GetTickCount() - this->mLastMoveTick;
			if(anElapsed >= 100)
				this->mRoot->MouseHover(anElapsed);
		}
	}

	this->CheckResize(GetTickCount());

	if(this->mRoot->mTimerFlags!=0)
		this->mRoot->TimerEvent(theElapsed);

}

// FUNCTION: WON 0x00729c30
// Creates a closed window on the default window manager with a new root container and the title "Sierra App"
Window::Window()
{
	this->mManager = WONAPI::WindowManager::GetDefaultWindowManager();
	this->mParent = NULL;

	this->mRoot = new RootContainer(this);
	this->mCurrentCursor = NULL;
	this->mMouseIsDown[0] = this->mMouseIsDown[1] = this->mMouseIsDown[2] = false;
	this->mMouseIsInClientArea = false;
	this->mNativeCursorShowing = true;
	this->mDetectDoubleClick = false;

	this->mMouseX = this->mMouseY = this->mKeyMod = 0;
	this->mLastMoveTick = 0;
	this->mLastCheckCursorTick = 0;

	this->mMinWidth = 0;
	this->mMinHeight = 0;
	this->mMaxWidth = 64000;
	this->mMaxHeight = 64000;

	this->mNeedResize = false;
	this->mNewWidth = this->mNewHeight = 0;
	this->mResizeDelay = 0;
	this->mLastResizeTick = 0;

	this->mStyle = 0;
	this->mWindowState = WindowState_Closed;
	this->mIsShowing = false;

	this->mIsPopup = false;
	this->mIsOneComponentWindow = false;
	this->mSuspendRootResize = 0;
	this->mAllowCloseFunc = NULL;
	this->mMinimizeOnResolutionRestore = false;

	this->mTitle = "Sierra App";
}

// FUNCTION: WON 0x0072a210
// Sets the parent window
void Window::SetParent(Window *theParent)
{
	this->mParent = theParent;
}

// FUNCTION: WON 0x0072a220
// Returns the default window manager's default window
Window* Window::GetDefaultWindow()
{
	return WONAPI::WindowManager::GetDefaultWindowManager()->mDefaultWindow;
}

// FUNCTION: WON 0x0072a8c0
// Creates an image of the given size for the window's display
SmartPtr<NativeImage> Window::CreateImage(int theWidth, int theHeight)
{
	return this->mDisplayContext->CreateImage(theWidth,theHeight);
}

// FUNCTION: WON 0x0072a900
// Opens the window: default icon, registration with the parent, root sized to the rectangle, added to the manager
void Window::Create(const WONRectangle& theWindowRectR)
{
	if(this->mIcon.get()==NULL)
		this->mIcon = this->mManager->mDefaultWindowIcon.get();

	this->mWindowState = WindowState_Open;

	if(this->mParent!=NULL)
		this->mParent->mChildWindowSet.insert(this);
	
	this->mRoot->SetSize(theWindowRectR.Width(), theWindowRectR.Height());

	this->mManager->AddWindow(this);
}

// FUNCTION: WON 0x0072aa60
// Marks the window closed and hidden and removes it from its parent's children
void Window::PlatformClose()
{
	this->ShowNativeCursor(true); 
	this->mWindowState = WindowState_Closed;
	this->mIsShowing = false;

	if(this->mParent!=NULL)
		this->mParent->mChildWindowSet.erase(this);
}

// FUNCTION: WON 0x0072afc0
// Shows or hides the system cursor
void Window::ShowNativeCursor(bool show)
{



	this->mNativeCursorShowing = show;
	this->PlatformShowNativeCursor(show);
}


static bool gWindowSkipDecodeImages = false;

// FUNCTION: WON 0x0072a640
// Turns image decoding off or on for every window (a blank placeholder is used)
void Window::SetSkipDecodeImages(bool skip) {
    gWindowSkipDecodeImages = skip;
}

// FUNCTION: WON 0x0072a650
// Decodes an image file for the window's display; while decoding is off returns one shared blank 10x10 image
SmartPtr<NativeImage> Window::DecodeImage(const char* path) {
    if(gWindowSkipDecodeImages) {
        static SmartPtr<NativeImage> image;
        if(image.get()==NULL) {
            image=this->CreateImage(10,10);
            image->GetGraphics().SetColor(0);
            image->GetGraphics().FillRect(0,0,20,20);
        }
        return image;
    }
    return this->mManager->DecodeImage(this->mDisplayContext.get(),path);
}

// FUNCTION: WON 0x0072a800
// Decodes a raw image file; while decoding is off returns a blank 10x10 image
SmartPtr<RawImage> Window::DecodeImageRaw(const char* path) {
    if(gWindowSkipDecodeImages)
        return new RawImage8(10,10,NULL);
    return this->mManager->DecodeImageRaw(path);
}

// FUNCTION: WON 0x0072b040
// Adds a component to the root container
void Window::AddComponent(Component* component) {
    this->mRoot->AddChild(component);
}

// FUNCTION: WON 0x0072b060
// Makes the root container modal or ends its modal state
void Window::SetModal(bool modal) {
    if(modal) this->mRoot->EnterModal(); else this->mRoot->LeaveModal();
}

// FUNCTION: WON 0x0072afd0
// Applies the root's cursor (arrow when none) when it changed or when forced; hides the system cursor for custom ones
void Window::CheckSetCursor(bool force) {
    if(!force && this->mRoot->mCurrentCursor.get()==this->mCurrentCursor)
        return;
    this->mCurrentCursor=this->mRoot->mCurrentCursor.get();
    Cursor* cursor=this->mRoot->mCurrentCursor.get();
    if(cursor==NULL)
        cursor=Cursor::GetStandardCursor(StandardCursor_Arrow);
    cursor=cursor->GetNative();
    if(cursor!=NULL) {
        this->ShowNativeCursor(true);
        this->PlatformSetCursor(cursor);
    } else {
        this->ShowNativeCursor(false);
    }
}

namespace WONAPI {
// FUNCTION: WON 0x0072a1e0
// Marks the whole root container for repainting
void Window::InvalidateRoot()
{
    this->mRoot->Invalidate();
    RootContainer* root=this->mRoot.get();
    root->mInvalidRect.DoUnion(root->GetBounds());
}
}

namespace WONAPI {
// FUNCTION: WON 0x0072afb0
// Minimizes or restores the window
void Window::Minimize(bool minimize)
{
    this->PlatformMinimize(minimize);
}
}

// The four functions after this one are not identified in the original exe
// FUNCTION: WON 0x0072a8f0
// Returns the window's offscreen image
NativeImage* Window::GetOffscreenImage()
{
    return this->mOffscreenImage.get();
}

Graphics& Window::GetGraphics()
{
    return this->mOffscreenImage->GetGraphics();
}

void Window::SuspendRootResize(bool suspend)
{
    if(suspend)
        this->mSuspendRootResize++;
    else
    {
        this->mSuspendRootResize--;
        if(this->mSuspendRootResize <= 0)
        {
            this->mSuspendRootResize = 0;
            this->CheckResize(GetTickCount());
        }
    }
}

void Window::RemoveComponent(Component* theComponent)
{
    this->mRoot->RemoveChild(theComponent);
}

bool Window::IsMouseDown(MouseButton theButton)
{
    return this->mMouseIsDown[theButton];
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0071de90
// (public: class std::_Tree<class WONAPI::Window *,class WONAPI::Window *,struct std::set<class WONAPI::Window *,struct std::less<class WONAPI::Window *>)
// TEMPLATE: WON 0x00729e90
// (public: virtual void * __thiscall WONAPI::Window::`scalar deleting destructor'(unsigned int)    [??_GWindow@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x00729eb0
// (public: __thiscall std::set<class WONAPI::Window *,struct std::less<class WONAPI::Window *>,class std::allocator<class WONAPI::Window *> >::~set<class)
// TEMPLATE: WON 0x0072a5b0
// (public: __thiscall std::pair<struct WONAPI::FontDescriptor const ,class WONAPI::SmartPtr<class WONAPI::Font> >::~pair<struct WONAPI::FontDescriptor co)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00729f60
// (public: __thiscall std::map<struct WONAPI::FontDescriptor,class WONAPI::SmartPtr<class WONAPI::Font>,struct std::less<struct WONAPI::FontDescriptor>,c)
// TEMPLATE: WON 0x0072a470
// (public: __thiscall WONAPI::FontDescriptor::FontDescriptor(struct WONAPI::FontDescriptor const &))
// TEMPLATE: WON 0x00731190
// (public: class std::_Tree<class WONAPI::Window *,class WONAPI::Window *,struct std::_Setk<class WONAPI::Window *,struct std::less<class WONAPI::Window )
// TEMPLATE: WON 0x00731240
// (protected: class std::_Tree<class WONAPI::Window *,class WONAPI::Window *,struct std::_Setk<class WONAPI::Window *,struct std::less<class WONAPI::Wind)
// TEMPLATE: WON 0x00731540
// (public: class std::_Tree<struct WONAPI::FontDescriptor,struct std::pair<struct WONAPI::FontDescriptor const ,class WONAPI::SmartPtr<class WONAPI::Font)
// TEMPLATE: WON 0x007315f0
// (protected: class std::_Tree<struct WONAPI::FontDescriptor,struct std::pair<struct WONAPI::FontDescriptor const ,class WONAPI::SmartPtr<class WONAPI::F)
// TEMPLATE: WON 0x00734460
// (public: class std::_Tree<class WONAPI::Window *,class WONAPI::Window *,struct std::_Setk<class WONAPI::Window *,struct std::less<class WONAPI::Window )
// TEMPLATE: WON 0x00734910
// (protected: void __thiscall std::_Tree<class WONAPI::Window *,class WONAPI::Window *,struct std::_Setk<class WONAPI::Window *,struct std::less<class WO)
// TEMPLATE: WON 0x00734950
// (protected: static struct std::_Tree<class WONAPI::Window *,class WONAPI::Window *,struct std::_Setk<class WONAPI::Window *,struct std::less<class WONA)
// TEMPLATE: WON 0x00734970
// (public: class std::_Tree<struct WONAPI::FontDescriptor,struct std::pair<struct WONAPI::FontDescriptor const ,class WONAPI::SmartPtr<class WONAPI::Font)
// TEMPLATE: WON 0x00734e90
// (protected: void __thiscall std::_Tree<struct WONAPI::FontDescriptor,struct std::pair<struct WONAPI::FontDescriptor const ,class WONAPI::SmartPtr<class)
// TEMPLATE: WON 0x00734ed0
// (protected: void __thiscall std::_Tree<struct WONAPI::FontDescriptor,struct std::pair<struct WONAPI::FontDescriptor const ,class WONAPI::SmartPtr<class)
// TEMPLATE: WON 0x00734f10
// (protected: struct std::_Tree<class WONAPI::Window *,class WONAPI::Window *,struct std::_Setk<class WONAPI::Window *,struct std::less<class WONAPI::Win)
// TEMPLATE: WON 0x00734f40
// (protected: struct std::_Tree<class WONAPI::Window *,class WONAPI::Window *,struct std::_Setk<class WONAPI::Window *,struct std::less<class WONAPI::Win)
// TEMPLATE: WON 0x00734fc0
// (public: void __thiscall std::_Tree<struct WONAPI::FontDescriptor,struct std::pair<struct WONAPI::FontDescriptor const ,class WONAPI::SmartPtr<class WO)
// TEMPLATE: WON 0x00735190
// (void __cdecl std::_Construct(struct std::pair<struct WONAPI::FontDescriptor const ,class WONAPI::SmartPtr<class WONAPI::Font> > *,struct std::pair<str)
// TEMPLATE: WON 0x00735320
// (public: void * __thiscall std::pair<struct WONAPI::FontDescriptor const ,class WONAPI::SmartPtr<class WONAPI::Font> >::`scalar deleting destructor'(un)
