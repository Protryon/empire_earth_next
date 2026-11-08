// Win32 top-level window of the GUI: creation, window procedure, painting and input events
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
// FUNCTION: WON 0x007d0d90
// Creates a window object with no Win32 window yet, no key modifiers and no key forwarding
MSWindow::MSWindow()
{
	this->mHWND = NULL;
	this->mCurKeyMod = 0;
	this->mMinimizeOnAltTab = false;
	this->mForwardKeyMsgToParent = false;
}

// FUNCTION: WON 0x007d0de0
// Destroys the window, detaching it from its Win32 window first so no more messages reach it
MSWindow::~MSWindow()
{
	if(this->mHWND!=NULL)
	{
		SetWindowLongPtrA(this->mHWND, GWLP_USERDATA, 0);
		DestroyWindow(this->mHWND);
	}
}

// FUNCTION: WON 0x007d0e70
// Returns a new GDI font for the descriptor on the window's display context
SmartPtr<Font> MSWindow::PlatformGetFont(const FontDescriptor &theDescriptor)
{
	return new MSFont(this->mDisplayContext.get(),theDescriptor);
}

// FUNCTION: WON 0x007d11d0
// Closes the window and destroys its Win32 window
void MSWindow::PlatformClose()
{
	Window::PlatformClose();

	if(this->mHWND!=NULL)
	{
		DestroyWindow(this->mHWND);
		this->mHWND = NULL;
	}
}

// FUNCTION: WON 0x007d1200
// Sets the Win32 window's caption to the title (as ANSI text)
void MSWindow::PlatformSetTitle()
{
	std::string aTitle = this->mTitle;
	SetWindowTextA(this->mHWND,aTitle.c_str());
}

// FUNCTION: WON 0x007d1300
// Sets the window's small icon (none when no icon is set)
void MSWindow::PlatformSetIcon()
{
	HICON anIcon = NULL;
	if(this->mIcon.get()!=NULL)
		anIcon = ((MSNativeIcon*)this->mIcon.get())->mIcon;

	SendMessageA(this->mHWND,WM_SETICON,(WPARAM)ICON_SMALL,(LPARAM)anIcon);
}

// FUNCTION: WON 0x007d1330
// Moves and resizes an open window; a width or height of -1 keeps the current one
void MSWindow::SetPosSize(int x, int y, int width, int height)
{
	if(this->mWindowState!=WindowState_Open)
		return;

	if(width==-1 || height==-1)
	{
		WONRectangle aRect;
		this->GetRect(aRect,false);
		if(width==-1)
			width = aRect.width;

		if(height==-1)
			height = aRect.height;
	}

	MoveWindow(this->mHWND, x, y, width, height, TRUE);	
}

// FUNCTION: WON 0x007d13a0
// Returns the window's screen rectangle, or its client area in screen coordinates
void MSWindow::GetRect(WONRectangle &theRectR, bool clientArea)
{
	RECT aRect;
	if(clientArea)
	{
		GetClientRect(this->mHWND, &aRect);
		POINT aPoint = { aRect.left, aRect.top };
		ClientToScreen(this->mHWND,&aPoint);
		theRectR.x = aPoint.x;
		theRectR.y = aPoint.y;
	}
	else
	{
		GetWindowRect(this->mHWND,&aRect);
		theRectR.x = aRect.left;
		theRectR.y = aRect.top;
	}

	theRectR.width = aRect.right - aRect.left;
	theRectR.height = aRect.bottom - aRect.top;
}

// FUNCTION: WON 0x007d1430
// Gives the Win32 window the keyboard focus
void MSWindow::PlatformSetFocus()
{
	::SetFocus(this->mHWND);
}

// FUNCTION: WON 0x007d1440
// Shows the window (without activating it for a popup) or hides it
void MSWindow::PlatformShow(bool show)
{
	int aCmd = SW_SHOW;
	if(this->mIsPopup)
		aCmd = SW_SHOWNA;

	if(!show)
		aCmd = SW_HIDE;


	::ShowWindow(this->mHWND, aCmd);
}

// FUNCTION: WON 0x007d1470
// Minimizes or restores the window
void MSWindow::PlatformMinimize(bool minimize)
{
	int aCmd = minimize?SW_MINIMIZE:SW_RESTORE;
	::ShowWindow(this->mHWND, aCmd);
}

// FUNCTION: WON 0x007d1490
// Makes the window topmost or not, keeping its position and size
void MSWindow::PlatformSetTopMost(bool topMost)
{
	if (topMost)
		::SetWindowPos(this->mHWND, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
	else
		::SetWindowPos(this->mHWND, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
}

// FUNCTION: WON 0x007d14f0
// Hides the system cursor when asked to; showing it does nothing
void MSWindow::PlatformShowNativeCursor(bool show)
{
	if(!show)
		SetCursor(NULL);

}

// FUNCTION: WON 0x007d1510
// Returns whether the Win32 window is visible
bool MSWindow::PlatformIsVisible()
{
	return ::IsWindowVisible(this->mHWND)?true:false;
}

// FUNCTION: WON 0x007d1670
// Captures the mouse for the window or releases the capture
void MSWindow::PlatformSetMouseCapture(bool on)
{
	if(on)
		SetCapture(this->mHWND);
	else
		ReleaseCapture();
}
// FUNCTION: WON 0x007d2150
// Runs the idle update and asks Windows to repaint the root's invalid rectangle; false when the base
// update fails
bool MSWindow::IdleUpdate() {
    if(!Window::IdleUpdate()) return false;
    if(this->mRoot->mInvalidState==0)
        return true;
    const WONRectangle& r=this->mRoot->mInvalidRect;
    RECT rect={r.x,r.y,r.x+r.width,r.y+r.height};
    ::InvalidateRect(this->mHWND,&rect,FALSE);
    return true;
}
// FUNCTION: WON 0x007d21d0
// Runs the timer update and queues a mouse-exit event once the cursor has left the window (being over
// the manager's popup window while its popup flag 0x4000 is set does not count)
void MSWindow::TimerEvent(DWORD elapsed) {
    Window::TimerEvent(elapsed);
    if(!this->mMouseIsInClientArea)
        return;
    POINT point;GetCursorPos(&point);
    HWND hwnd=WindowFromPoint(point);
    ScreenToClient(this->mHWND,&point);
    if(hwnd==this->mHWND && this->mRoot->Contains(point.x,point.y))
        return;
    if(hwnd!=this->mHWND) {
        WindowManager* manager=this->mManager;
        MSWindow* popup=static_cast<MSWindow*>(manager->GetPopupWindow());
        if(popup!=NULL && ((manager->mPopupFlags&PopupFlag_DontWantMouse)?true:false) && popup->mHWND==hwnd) return;
    }
    this->QueueEvent(new InputEvent(InputEvent_MouseExit));
}
// FUNCTION: WON 0x007d22c0
// Sets up the display context, an offscreen image the size of the root and a 50 ms timer
void MSWindow::OnCreate() {
    this->mDisplayContext=new DisplayContext(::GetDC(this->mHWND));
    this->mOffscreenImage=this->CreateImage(this->mRoot->Width(),this->mRoot->Height());
    SetTimer(this->mHWND,0,50,NULL);
}
// FUNCTION: WON 0x007d23f0
// Repaints the root into the offscreen image if anything is invalid (asking again for any part outside
// the paint area), then copies the whole image to the window
void MSWindow::OnPaint() {
    PAINTSTRUCT ps;BeginPaint(this->mHWND,&ps);
    const RECT& paint=ps.rcPaint;
    if(this->mRoot->mInvalidState!=0) {
        const WONRectangle& r=this->mRoot->mInvalidRect;
        RECT invalid={r.x,r.y,r.x+r.width,r.y+r.height};
        if(invalid.left<paint.left || invalid.top<paint.top || invalid.right>paint.right || invalid.bottom>paint.bottom)
            ::InvalidateRect(this->mHWND,&invalid,FALSE);
        this->mRoot->PrePaint(this->mOffscreenImage->GetGraphics());
        this->mRoot->Paint(this->mOffscreenImage->GetGraphics());
    }
    this->mOffscreenImage->DrawEntire(this->mDisplayContext.get(),0,0);
    EndPaint(this->mHWND,&ps);
}

#include <imm.h>
#include <stdio.h>
#include <stdlib.h>
// Owner window for windows created without a parent
static HWND gMSWindowDefaultOwnerHWND = NULL;
// FUNCTION: WON 0x007d08c0
// Sets the owner window used for windows created without a parent
void WONAPI::SetMSWindowDefaultOwner(HWND owner) { gMSWindowDefaultOwnerHWND=owner; }

// FUNCTION: WON 0x007d0f00
// Registers a new window class and creates the Win32 window from the style flags (popup, no system menu,
// no minimize, fixed size, default position, client-sized, centred, hidden), then sets its icon
void MSWindow::Create(const WONRectangle& theWindowRectR)
{
	Window::Create(theWindowRectR);
	this->CalcCodePage();

	static int aCreateCount = 0;
	char aBuf[50];
	sprintf(aBuf,"WndClass%d",aCreateCount++);

	WNDCLASS wc;
	wc.lpszClassName = aBuf;
	wc.lpfnWndProc = MSWindow::StaticWindowProc;
	wc.style = CS_OWNDC | CS_DBLCLKS; 
	wc.hInstance = static_cast<MSWindowManager*>(this->mManager)->mInstance;
	wc.hIcon = LoadIcon( NULL, IDI_APPLICATION );
	wc.hCursor = NULL;
	wc.hbrBackground = NULL;
	wc.lpszMenuName = NULL;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	RegisterClass( &wc );

	HWND aParent = NULL;
	int aStyle = WS_OVERLAPPEDWINDOW;
	int aStyleEx = 0;

	if(this->mParent!=NULL)
	{
		aParent = static_cast<MSWindow*>(this->mParent)->mHWND;
		aStyle |= WS_POPUPWINDOW;

	}
	else
		aParent = gMSWindowDefaultOwnerHWND;

	if(this->mStyle & 0x20)
	{
		aStyle = WS_POPUP;

	}

	if(this->mStyle & 0x04) 
		aStyle &= ~(WS_SYSMENU);

	if(this->mStyle & 0x08)
		aStyle &= ~(WS_MINIMIZEBOX);

	if(this->mStyle & 0x10)
		aStyle &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);

	int x = theWindowRectR.x, y = theWindowRectR.y;
	if(this->mStyle&0x80)
		x = y = CW_USEDEFAULT;


	std::string aTitle = this->mTitle;
	HWND aWnd = CreateWindowEx(
		aStyleEx,
		aBuf,
		aTitle.c_str(),
		aStyle,
		x,
		y,
		theWindowRectR.Width(),
		theWindowRectR.Height(),
		aParent,
		NULL,
		static_cast<MSWindowManager*>(this->mManager)->mInstance,
		this
   );



	if(aWnd && (this->mStyle & 0x40) && !(this->mStyle & 0x20))
	{
		
		
		WONRectangle aClientRect;
		WONRectangle aScreenRect;
		this->GetRect(aScreenRect,false);
		this->GetRect(aClientRect,true);

		int aWidthDelta = theWindowRectR.Width() - aClientRect.Width();
		int aHeightDelta = theWindowRectR.Height() - aClientRect.Height();





		int aTargetWidth = aScreenRect.Width() + aWidthDelta;
		int aTargetHeight = aScreenRect.Height() + aHeightDelta;

		this->SetPosSize(aScreenRect.Left(), aScreenRect.Top(), aTargetWidth, aTargetHeight);
	}

	if(this->mStyle&0x01)
		this->Center(true);



	if(!(this->mStyle&0x02))
	{
		this->mIsShowing = true;
		this->PlatformShow(true);
	}
	else
		this->mIsShowing = false;

	if(this->mIcon.get()!=NULL)
		this->PlatformSetIcon();
}


// Posts a message to the window's parent, if it has one (body at 0x007d2120)
static void SendMsgToParent(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam) {
    HWND parent=::GetParent(hwnd);
    if(parent!=NULL) ::PostMessage(parent,msg,wParam,lParam);
}

// FUNCTION: WON 0x007d0e20
// Sets the code page for character input from the keyboard layout's language (ANSI default if unknown)
void MSWindow::CalcCodePage()
{
	char aBuf[32];
	unsigned short aLangId = ((WORD)GetKeyboardLayout(0))&0xffff;
	GetLocaleInfo(MAKELCID(aLangId,SORT_DEFAULT),LOCALE_IDEFAULTANSICODEPAGE,aBuf,32);
	this->mCodePage = atoi(aBuf);
	if(this->mCodePage==0)
		this->mCodePage = CP_ACP;
}

// FUNCTION: WON 0x007d1530
// Win32 window procedure: binds the window object on creation and forwards every message to it
LRESULT CALLBACK MSWindow::StaticWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	MSWindow *aWindow = (MSWindow*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
	if(aWindow==NULL)
	{
		if(msg==WM_CREATE || msg==WM_NCCREATE)
		{
			CREATESTRUCT *aStruct = (CREATESTRUCT*)lParam;
			aWindow = (MSWindow*)aStruct->lpCreateParams;
			aWindow->mHWND = hwnd;
			SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(aWindow));
		}
		else
			return DefWindowProc(hwnd,msg,wParam,lParam);
	}
		
	return aWindow->WindowProc(hwnd, msg, wParam, lParam);
}

// FUNCTION: WON 0x007d15a0
// Reads the Shift (1), Alt (2) and Ctrl (4) key states into the current key modifiers
void MSWindow::MyGetKeyState()
{
	int aState;
	 
	aState = (GetKeyState(VK_SHIFT)>>15)&1;
	this->mCurKeyMod = aState;

	aState = (GetKeyState(VK_MENU)>>15)&1;
	this->mCurKeyMod |= (aState<<1);

	aState = (GetKeyState(VK_CONTROL)>>15)&1;
	this->mCurKeyMod |= (aState<<2);
}

// FUNCTION: WON 0x007d1600
// Queues a mouse event at the message's coordinates with the current key modifiers
void MSWindow::PostMouseEvent(WindowEventType theType, int theButtonOrWheelAmount, LPARAM lParam)
{
	this->QueueEvent(new MouseEvent(theType, (short)LOWORD(lParam), (short)HIWORD(lParam), this->mCurKeyMod, theButtonOrWheelAmount));
}

// FUNCTION: WON 0x007d16a0
// Minimizes the window's top-level ancestor; the loop re-reads this window's parent each time, so it
// never ends when the window has a parent
void MSWindow::DoMinimizeOnAltTab()
{
	HWND aWnd = this->mHWND;
	HWND aParent = ::GetParent(this->mHWND);
	while(aParent!=NULL)
	{
		aWnd = aParent;
		aParent = ::GetParent(this->mHWND);
	}
	
	ShowWindow(aWnd, SW_MINIMIZE);
}

// FUNCTION: WON 0x007d16e0
// Turns Win32 messages into GUI events (mouse, keys, IME text, focus, size and move), handles modal
// activation, popups, size limits and painting, then processes the queued events
LRESULT MSWindow::WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{	
	
	switch(msg)
	{
		case WM_ACTIVATE:
			if(LOWORD(wParam)!=WA_INACTIVE && this->mManager->NotModalWindow(this))
			{
				MSWindow *aModalWindow = static_cast<MSWindow*>(this->mManager->mModalWindow);

				ShowWindow(aModalWindow->mHWND,SW_SHOW);
				return 0;
			}
			else
				return DefWindowProc(hwnd,msg,wParam,lParam);
			break;

		case WM_CLOSE:
			this->Close();
			return 1;

		case WM_MOUSEACTIVATE:
			if(this->mIsPopup)
				return MA_NOACTIVATE;
			else
				return DefWindowProc(hwnd,msg,wParam,lParam);
			break;

		case WM_NCLBUTTONDOWN:
		case WM_NCMBUTTONDOWN:
		case WM_NCRBUTTONDOWN:
		case WM_NCLBUTTONDBLCLK:
		case WM_NCMBUTTONDBLCLK:
		case WM_NCRBUTTONDBLCLK:
			this->ClosePopup(NULL);
			if(this->mManager->NotModalWindow(this))
				return 0;
			else
				return DefWindowProc(hwnd,msg,wParam,lParam);
			break;

		case WM_CREATE:
			this->OnCreate();
		break;
		case WM_SYSCOLORCHANGE:


			break;

		case WM_SIZE:
			if(wParam==SIZE_RESTORED || wParam==SIZE_MAXIMIZED)
				this->QueueEvent(new SizeEvent(-1,-1,(short)LOWORD(lParam), (short)HIWORD(lParam)));
			break;

		case WM_MOVE:
			this->QueueEvent(new SizeEvent((short)LOWORD(lParam), (short)HIWORD(lParam),-1,-1));
			break;

		case WM_MOUSEMOVE:		this->PostMouseEvent(InputEvent_MouseMove, 0, lParam);break;

		case WM_LBUTTONDOWN:	this->PostMouseEvent(InputEvent_MouseDown, 0, lParam); break; 
		case WM_LBUTTONDBLCLK:	this->PostMouseEvent(InputEvent_MouseDoubleClick,	0, lParam); break;
		case WM_LBUTTONUP:		this->PostMouseEvent(InputEvent_MouseUp, 0, lParam); break; 

		case WM_MBUTTONDOWN:	this->PostMouseEvent(InputEvent_MouseDown, 1, lParam); break; 
		case WM_MBUTTONDBLCLK:	this->PostMouseEvent(InputEvent_MouseDoubleClick,	1, lParam); break;
		case WM_MBUTTONUP:		this->PostMouseEvent(InputEvent_MouseUp, 1, lParam); break;

		case WM_RBUTTONDOWN:	this->PostMouseEvent(InputEvent_MouseDown, 2, lParam); break; 
		case WM_RBUTTONDBLCLK:	this->PostMouseEvent(InputEvent_MouseDoubleClick,	2, lParam); break;
		case WM_RBUTTONUP:		this->PostMouseEvent(InputEvent_MouseUp,		2, lParam); break;

		case 0x020A: // WM_MOUSEWHEEL (not defined by the VC6 headers at this WINVER)
		{
			
			WONRectangle aRect;
			this->GetRect(aRect,true); 
			short mx = LOWORD(lParam);
			short my = HIWORD(lParam);
			lParam = MAKELPARAM(mx - aRect.Left(), my - aRect.Top());
			this->PostMouseEvent(InputEvent_MouseWheel, (short)HIWORD(wParam), lParam);
			break;
		}

		case WM_INPUTLANGCHANGE: 
		{
			this->CalcCodePage();
			return DefWindowProc(hwnd,msg,wParam,lParam);
		}

		case WM_IME_COMPOSITION:
			 

			if (lParam & GCS_RESULTSTR) 
			{
				
				HIMC hIMC = ImmGetContext(hwnd);
				if (hIMC) 
				{
					DWORD dwSize = ImmGetCompositionStringW(hIMC, GCS_RESULTSTR, NULL, 0);
					std::wstring aStr;
					aStr.resize(dwSize/2);
					ImmGetCompositionStringW(hIMC, GCS_RESULTSTR, (char*)aStr.data(),dwSize); 
					for(int i=0; i<aStr.length(); i++)
						this->QueueEvent(new KeyEvent(InputEvent_KeyChar, aStr.at(i), this->mCurKeyMod)); 
				}
				ImmReleaseContext(hwnd, hIMC);

			}
			else 
				return DefWindowProc(hwnd,msg,wParam,lParam); 
	
			break;

		case WM_CHAR:			
		{
			wchar_t aChar = wParam;
			MultiByteToWideChar(this->mCodePage,0,reinterpret_cast<const char*>(&wParam),2,&aChar,1);
			this->QueueEvent(new KeyEvent(InputEvent_KeyChar, aChar, this->mCurKeyMod)); 
			break;
		}

		case WM_IME_CHAR:			
		{
			wchar_t aChar = wParam;
			MultiByteToWideChar(this->mCodePage,0,reinterpret_cast<const char*>(&wParam),2,&aChar,1);
			this->QueueEvent(new KeyEvent(InputEvent_KeyChar, aChar, this->mCurKeyMod)); 
			break;
		}

		case WM_KEYDOWN:		this->MyGetKeyState(); this->QueueEvent(new KeyEvent(InputEvent_KeyDown,	wParam, this->mCurKeyMod)); if(this->mForwardKeyMsgToParent) SendMsgToParent(hwnd,msg,wParam,lParam); break;
		case WM_KEYUP:			this->MyGetKeyState(); this->QueueEvent(new KeyEvent(InputEvent_KeyUp, wParam, this->mCurKeyMod)); if(this->mForwardKeyMsgToParent) SendMsgToParent(hwnd,msg,wParam,lParam); break;
		
		
		case WM_SYSKEYDOWN:	
		case WM_SYSKEYUP:		
			if(wParam==VK_TAB && this->mMinimizeOnAltTab)
			{
				this->DoMinimizeOnAltTab();
				return 1;
			}

			this->MyGetKeyState(); 
			if(this->mForwardKeyMsgToParent) SendMsgToParent(hwnd,msg,wParam,lParam);
			return DefWindowProc(hwnd,msg,wParam,lParam); 


		case WM_KILLFOCUS:		this->QueueEvent(new InputEvent(InputEvent_LostFocus)); break;
		case WM_SETFOCUS:		this->MyGetKeyState(); this->QueueEvent(new InputEvent(InputEvent_GotFocus)); break;

		case WM_DESTROY:
			KillTimer(this->mHWND,0);
			SetWindowLongPtr(hwnd, GWLP_USERDATA, 0);
			this->mHWND = NULL;
			this->Close();
			
			break;

		case WM_TIMER:
			
			break;

		case WM_SIZING:
		{
			LPRECT aRect = (LPRECT) lParam;

			
			
			if (aRect->right - aRect->left < this->mMinWidth)
			{
				aRect->right = aRect->left + this->mMinWidth; 
			}
			
			if ( aRect->bottom - aRect->top < this->mMinHeight)
			{
				aRect->bottom = aRect->top + this->mMinHeight;
			}

			
			
			if (aRect->right - aRect->left > this->mMaxWidth)
			{
				aRect->right = aRect->left + this->mMaxWidth;
			}
			
			if ( aRect->bottom - aRect->top > this->mMaxHeight)
			{
				aRect->bottom = aRect->top + this->mMaxHeight;
			}
		}
		break;

		case WM_SYSCOMMAND:
		{
			if (wParam == SC_MAXIMIZE) 
			{
				int aMaximizedWidth = GetSystemMetrics(SM_CXMAXIMIZED);
				int aMaximizedHeight = GetSystemMetrics(SM_CYMAXIMIZED);

				
				if (this->mMaxWidth < aMaximizedWidth || this->mMaxHeight < aMaximizedHeight)
				{
					DefWindowProc(hwnd,msg,wParam,lParam); 
					int x, y, cx, cy;
					
					if (this->mMaxWidth < aMaximizedWidth)
					{
						x =  (aMaximizedWidth - this->mMaxWidth) / 2;
						cx = this->mMaxWidth;
					}
					else
					{
						x = 0;
						cx = aMaximizedWidth;
					}
					
					if (this->mMaxHeight < aMaximizedHeight)
					{
						y =  (aMaximizedHeight - this->mMaxHeight) / 2;
						cy = this->mMaxHeight;
					}
					else
					{
						y = 0;
						cy = aMaximizedHeight;
					}
					
					SetWindowPos(hwnd,NULL,x,y,cx,cy,SWP_NOZORDER);
				}
				else 
				{
					return DefWindowProc(hwnd,msg,wParam,lParam);
				}
			}
	
			else
			{
				return DefWindowProc(hwnd,msg,wParam,lParam);
			}
		}
		break;

		case WM_PAINT:
			this->OnPaint();
		break;

		case WM_USER: 
			
			break;

		default:
			return DefWindowProc(hwnd,msg,wParam,lParam);
	}

	this->mManager->ProcessEvents();

	return 1;
}

// FUNCTION: WON 0x007d14d0
// Sets the system cursor to the cursor's Win32 cursor
void MSWindow::PlatformSetCursor(Cursor* cursor) { ::SetCursor(static_cast<MSCursor*>(cursor)->mNativeCursor); }

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007d0dc0
// (public: virtual void * __thiscall WONAPI::MSWindow::`scalar deleting destructor'(unsigned int)    [??_GMSWindow@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x007d2120
// (void __cdecl SendMsgToParent(struct HWND__ *,unsigned int,unsigned int,long)    [?SendMsgToParent@@YAXPAUHWND__@@IIJ@Z])
