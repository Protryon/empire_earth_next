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
using namespace WONAPI;

// Hand cursor image bits (32x32 monochrome AND/XOR pair: this is the XOR plane)
static const unsigned char gHandCursorData[] = 
{
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x0c,0x00,0x00,0x00,0x0c,0x00,0x00,0x00,0x0c,
	0x00,0x00,0x00,0x0c,0x00,0x00,0x00,0x0c,0x00,0x00,0x00,0x0d,0x80,0x00,0x00,
	0x0d,0xb0,0x00,0x00,0x0d,0xb4,0x00,0x00,0x0d,0xb6,0x00,0x00,0xcf,0xf6,0x00,
	0x00,0xef,0xfe,0x00,0x00,0x6f,0xfe,0x00,0x00,0x2f,0xfe,0x00,0x00,0x3f,0xfe,
	0x00,0x00,0x1f,0xfe,0x00,0x00,0x1f,0xfc,0x00,0x00,0x0f,0xfc,0x00,0x00,0x0f,
	0xfc,0x00,0x00,0x07,0xf8,0x00,0x00,0x07,0xf8,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};
// Hand cursor mask bits (the AND plane)
static const unsigned char gHandCursorMaskData[] = 
{
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xf3,0xff,0xff,0xff,0xe1,0xff,0xff,0xff,0xe1,0xff,0xff,0xff,0xe1,
	0xff,0xff,0xff,0xe1,0xff,0xff,0xff,0xe0,0x7f,0xff,0xff,0xe0,0x0f,0xff,0xff,
	0xe0,0x03,0xff,0xff,0xe0,0x01,0xff,0xfe,0x20,0x00,0xff,0xfe,0x00,0x00,0xff,
	0xfe,0x00,0x00,0xff,0xff,0x00,0x00,0xff,0xff,0x80,0x00,0xff,0xff,0x80,0x00,
	0xff,0xff,0xc0,0x00,0xff,0xff,0xc0,0x01,0xff,0xff,0xe0,0x01,0xff,0xff,0xe0,
	0x01,0xff,0xff,0xf0,0x03,0xff,0xff,0xf0,0x03,0xff,0xff,0xf0,0x03,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff
};
// Returns a Windows system color with red and blue swapped (COLORREF 0x00BBGGRR -> GUI 0x00RRGGBB)
static DWORD MyGetSysColor(int index) {
    DWORD c=GetSysColor(index);
    return ((c&0xff0000)>>16)|(c&0xff00)|((c&0xff)<<16);
}

// FUNCTION: WON 0x007d0900
// Creates the Win32 window manager for a module (the exe when none is given) and installs the standard cursors
MSWindowManager::MSWindowManager(HINSTANCE theHinstance)
{
	if(theHinstance==NULL)
		theHinstance = (HINSTANCE)GetModuleHandle(NULL);

	this->mInstance = theHinstance;

	Cursor::SetStandardCursor(StandardCursor_Hand, new MSCursor(CreateCursor(this->mInstance,12,4,32,32,gHandCursorMaskData,gHandCursorData),true));
	Cursor::SetStandardCursor(StandardCursor_Arrow, new MSCursor(LoadCursor(NULL,MAKEINTRESOURCE(IDC_ARROW))));
	Cursor::SetStandardCursor(StandardCursor_IBeam, new MSCursor(LoadCursor(NULL,MAKEINTRESOURCE(IDC_IBEAM))));
	Cursor::SetStandardCursor(StandardCursor_EastWest, new MSCursor(LoadCursor(NULL,MAKEINTRESOURCE(IDC_SIZEWE))));
	Cursor::SetStandardCursor(StandardCursor_NorthSouth, new MSCursor(LoadCursor(NULL,MAKEINTRESOURCE(IDC_SIZENS))));
	Cursor::SetStandardCursor(StandardCursor_NorthSouthEastWest, new MSCursor(LoadCursor(NULL,MAKEINTRESOURCE(IDC_SIZEALL))));
}

// FUNCTION: WON 0x007d0b70
// Fills the color scheme's standard colors from the Windows system colors (text takes COLOR_WINDOWFRAME)
void MSWindowManager::GetSystemColorScheme()
{
	this->mColorScheme->SetStandardColor(StandardColor_3DDarkShadow,MyGetSysColor(COLOR_3DDKSHADOW));
	this->mColorScheme->SetStandardColor(StandardColor_3DFace,MyGetSysColor(COLOR_3DFACE));
	this->mColorScheme->SetStandardColor(StandardColor_3DHilight,MyGetSysColor(COLOR_3DHILIGHT));

	this->mColorScheme->SetStandardColor(StandardColor_3DShadow,MyGetSysColor(COLOR_3DSHADOW));
	this->mColorScheme->SetStandardColor(StandardColor_Scrollbar,MyGetSysColor(COLOR_SCROLLBAR));

	this->mColorScheme->SetStandardColor(StandardColor_ButtonText,MyGetSysColor(COLOR_BTNTEXT));
	this->mColorScheme->SetStandardColor(StandardColor_GrayText,MyGetSysColor(COLOR_GRAYTEXT));
	this->mColorScheme->SetStandardColor(StandardColor_Hilight,MyGetSysColor(COLOR_HIGHLIGHT));
	this->mColorScheme->SetStandardColor(StandardColor_HilightText,MyGetSysColor(COLOR_HIGHLIGHTTEXT));

	this->mColorScheme->SetStandardColor(StandardColor_ToolTipBack,MyGetSysColor(COLOR_INFOBK));
	this->mColorScheme->SetStandardColor(StandardColor_ToolTipText,MyGetSysColor(COLOR_INFOTEXT));

	this->mColorScheme->SetStandardColor(StandardColor_MenuBack,MyGetSysColor(COLOR_MENU));
	this->mColorScheme->SetStandardColor(StandardColor_MenuText,MyGetSysColor(COLOR_MENUTEXT));

	this->mColorScheme->SetStandardColor(StandardColor_Back,MyGetSysColor(COLOR_WINDOW));
	this->mColorScheme->SetStandardColor(StandardColor_Text,MyGetSysColor(COLOR_WINDOWFRAME));
}

// FUNCTION: WON 0x007d0c40
// Waits for and dispatches one Windows message; on WM_QUIT ends every running message loop
void MSWindowManager::GetNextEvent()
{
	MSG msg;
	if(GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	else
		this->mTargetMsgLoopDepth = 0;
}

// FUNCTION: WON 0x007d0c90
// Returns the main screen's rectangle in pixels, placed at the origin
void MSWindowManager::GetScreenRect(WONRectangle &theRect) 
{ 
	theRect.x = theRect.y = 0;
	theRect.width = GetSystemMetrics(SM_CXSCREEN);
	theRect.height = GetSystemMetrics(SM_CYSCREEN);
}

// FUNCTION: WON 0x007d0cc0
// Wakes the message loop by posting WM_USER to the default window, if it has one
void MSWindowManager::Signal()
{
	MSWindow *aWindow = static_cast<MSWindow*>(this->mDefaultWindow);
	if(aWindow==NULL)
		return;

	if(aWindow->mHWND==NULL)
		return;

	::PostMessage(aWindow->mHWND,WM_USER,0,0);
}

// FUNCTION: WON 0x007d0cf0
// Switches the display to the given resolution (fullscreen mode change) when it differs; always returns true
bool MSWindowManager::SetScreenResolutionHook(int theWidth, int theHeight)
{
	DEVMODE dm;
	EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &dm );

	
	if (dm.dmPelsWidth==theWidth && dm.dmPelsHeight==theHeight)
		return true;
	dm.dmPelsWidth = theWidth;
	dm.dmPelsHeight = theHeight;
	dm.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_DISPLAYFREQUENCY;
	ChangeDisplaySettings(&dm,CDS_FULLSCREEN);
	return true;
}

// FUNCTION: WON 0x007d0d50
// Returns the current display resolution
void MSWindowManager::GetScreenResolutionHook(int &theWidth, int &theHeight)
{
	DEVMODE dm;
	EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &dm );
	theWidth = dm.dmPelsWidth;
	theHeight = dm.dmPelsHeight;
}

// FUNCTION: WON 0x007d0b60
// Destroys the window manager (base class cleanup only)
MSWindowManager::~MSWindowManager() {}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007d08d0
// (unsigned long __cdecl MyGetSysColor(int)    [?MyGetSysColor@@YAKH@Z])
// TEMPLATE: WON 0x007d0b40
// (public: virtual void * __thiscall WONAPI::MSWindowManager::`scalar deleting destructor'(unsigned int)    [??_GMSWindowManager@WONAPI@@UAEPAXI@Z])
