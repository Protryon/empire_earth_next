// Window manager: windows, focus, event queue and timers, modal dialogs, popups, fonts and image caches
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
#include "ee/wonapi/WONLobbyDelayLoadImage.h"
#include "ee/wonapi/WONComponentFlags.h"
using namespace WONAPI;
// FUNCTION: WON 0x0072b660
// Creates the window manager: registers the BMP and WIM decoders, becomes the default manager, sets standard colours
WindowManager::WindowManager() {
    WindowManager::mThreadId=GetCurrentThreadId();
    this->mIsFullScreen=false;
    this->mOriginalScreenWidth=this->mOriginalScreenHeight=0;
    this->mLastLoseFocusTick=0;
    this->mImageDecoder.AddDecoder(new BMPDecoder,"bmp");
    this->mImageDecoder.AddDecoder(new WIMDecoder,"wim");
    WindowManager::mDefaultWindowManager=this;
    this->mLastTimerTick=GetTickCount();
    this->mEndPopup=false;
    this->mDefaultWindow=NULL;this->mWindowCount=0;
    this->mCurMsgLoopDepth=0;this->mTargetMsgLoopDepth=0;this->mDialogResult=0;
    this->mLastInputTick=0;
    this->mModalWindow=NULL;this->mModalComponent=NULL;this->mPopupOwnerWindow=NULL;
    this->mFocusWindow=NULL;this->mMouseDownWindow=NULL;
    this->mIsValid=true;this->mInDoPopup=false;this->mInEndPopup=false;this->mPopupFlags=0;
    this->mPopupIsUp=false;this->mBA5=true;
    this->mDoMouseCapturePopupSupport=false;this->mCallingTimers=false;
    this->mColorScheme=new ColorScheme;
    this->InitColorScheme();
    // The original leaves the image cache count (+0x94) and the full-screen size (+0xb0, +0xb4) uninitialised
}
// FUNCTION: WON 0x0072ba90
// Deleting destructor of the window manager (compiler-generated)
// (??_GWindowManager@WONAPI@@UAEPAXI@Z)

// FUNCTION: WON 0x0072bab0
// Destroys the named-font map tree, freeing every node
// (??1?$_Tree@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VFont@WONAPI@@@WONAPI@@@2@U_Kfn@?$map@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VFont@WONAPI@@@WONAPI@@UStringLessNoCase@4@V?$allocator@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VFont@WONAPI@@@WONAPI@@@std@@@2@@2@UStringLessNoCase@WONAPI@@V?$allocator@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VFont@WONAPI@@@WONAPI@@@std@@@2@$0A@@std@@QAE@XZ)

// FUNCTION: WON 0x0072bb70
// Destroys the native image cache tree, freeing every node
// (??1?$_Tree@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VNativeImage@WONAPI@@@WONAPI@@@2@U_Kfn@?$map@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VNativeImage@WONAPI@@@WONAPI@@UStringLessNoCase@4@V?$allocator@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VNativeImage@WONAPI@@@WONAPI@@@std@@@2@@2@UStringLessNoCase@WONAPI@@V?$allocator@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VNativeImage@WONAPI@@@WONAPI@@@std@@@2@$0A@@std@@QAE@XZ)

// FUNCTION: WON 0x0072bc30
// Destroys the raw image cache tree, freeing every node
// (??1?$_Tree@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VRawImage@WONAPI@@@WONAPI@@@2@U_Kfn@?$map@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VRawImage@WONAPI@@@WONAPI@@UStringLessNoCase@4@V?$allocator@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VRawImage@WONAPI@@@WONAPI@@@std@@@2@@2@UStringLessNoCase@WONAPI@@V?$allocator@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VRawImage@WONAPI@@@WONAPI@@@std@@@2@$0A@@std@@QAE@XZ)

// FUNCTION: WON 0x0072bcf0
// Destroys the delay-load image cache tree, freeing every node
// (??1?$_Tree@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VImage@WONAPI@@@WONAPI@@@2@U_Kfn@?$map@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VImage@WONAPI@@@WONAPI@@UStringLessNoCase@4@V?$allocator@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VImage@WONAPI@@@WONAPI@@@std@@@2@@2@UStringLessNoCase@WONAPI@@V?$allocator@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VImage@WONAPI@@@WONAPI@@@std@@@2@$0A@@std@@QAE@XZ)

// FUNCTION: WON 0x0072bdb0
// Destroys the window set tree, freeing every node
// (??1?$_Tree@V?$SmartPtr@VWindow@WONAPI@@@WONAPI@@V12@U_Kfn@?$_Setk@V?$SmartPtr@VWindow@WONAPI@@@WONAPI@@UComp@12@V?$allocator@V?$SmartPtr@VWindow@WONAPI@@@WONAPI@@@std@@@std@@UComp@12@V?$allocator@V?$SmartPtr@VWindow@WONAPI@@@WONAPI@@@5@$0A@@std@@QAE@XZ)

// FUNCTION: WON 0x0072be70
// Destroys the timer event set tree, freeing every node
// (??1?$_Tree@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@V12@U_Kfn@?$_Setk@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@U?$less@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@std@@V?$allocator@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@4@@std@@U?$less@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@5@V?$allocator@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@5@$0A@@std@@QAE@XZ)

// FUNCTION: WON 0x0072bf30
// Destroys the broadcast component set tree, freeing every node
// (??1?$_Tree@V?$SmartPtr@VComponent@WONAPI@@@WONAPI@@V12@U_Kfn@?$set@V?$SmartPtr@VComponent@WONAPI@@@WONAPI@@U?$less@V?$SmartPtr@VComponent@WONAPI@@@WONAPI@@@std@@V?$allocator@V?$SmartPtr@VComponent@WONAPI@@@WONAPI@@@4@@std@@U?$less@V?$SmartPtr@VComponent@WONAPI@@@WONAPI@@@5@V?$allocator@V?$SmartPtr@VComponent@WONAPI@@@WONAPI@@@5@$0A@@std@@QAE@XZ)

// FUNCTION: WON 0x0072bff0
// Destroys the window manager after closing every open window
WindowManager::~WindowManager()
{
	this->CloseAllWindows();
}

// FUNCTION: WON 0x0072c230
// Fills the colour scheme with the 17 standard Windows-style colours (face, shadow, highlight, text, links ...)
void WindowManager::InitColorScheme()
{
	this->mColorScheme->SetStandardColor(StandardColor_3DDarkShadow,0x000000);
	this->mColorScheme->SetStandardColor(StandardColor_3DFace,0xD0D0D0);
	this->mColorScheme->SetStandardColor(StandardColor_3DHilight,0xFFFFFF);
	this->mColorScheme->SetStandardColor(StandardColor_3DShadow,0x848484);
	this->mColorScheme->SetStandardColor(StandardColor_Scrollbar,0xDEDEDE);

	this->mColorScheme->SetStandardColor(StandardColor_ButtonText,0x000000);
	this->mColorScheme->SetStandardColor(StandardColor_GrayText,0x848484);
	this->mColorScheme->SetStandardColor(StandardColor_Hilight,0x08246B);
	this->mColorScheme->SetStandardColor(StandardColor_HilightText,0xFFFFFF);

	this->mColorScheme->SetStandardColor(StandardColor_ToolTipBack,0xFFFFDD);
	this->mColorScheme->SetStandardColor(StandardColor_ToolTipText,0x000000);

	this->mColorScheme->SetStandardColor(StandardColor_MenuBack,0xD0D0D0);
	this->mColorScheme->SetStandardColor(StandardColor_MenuText,0);

	this->mColorScheme->SetStandardColor(StandardColor_Back,0xFFFFFF);
	this->mColorScheme->SetStandardColor(StandardColor_Text,0x000000);

	this->mColorScheme->SetStandardColor(StandardColor_Link,0x0000FF);
	this->mColorScheme->SetStandardColor(StandardColor_LinkDown,0xFF0000);
}

// FUNCTION: WON 0x0072c2d0
// Records that no window has focus and when focus was lost (tick 0 is stored as 1)
void WindowManager::NotifyLostFocus()
{
	this->mFocusWindow = NULL;
	this->mLastLoseFocusTick = GetTickCount();
	if(this->mLastLoseFocusTick==0)
		this->mLastLoseFocusTick = 1;
}

// FUNCTION: WON 0x0072c300
// Records the window that gained focus and clears the lost-focus tick
void WindowManager::NotifyGotFocus(Window *theWindow)
{
	this->mFocusWindow = theWindow;
	this->mLastLoseFocusTick = 0;
}

// FUNCTION: WON 0x0072c320
// Switches the screen resolution and remembers the previous one; only when both width and height differ
// from the current mode
void WindowManager::SetScreenResolution(int theWidth, int theHeight)
{
	int aWidth = 0,aHeight = 0;
	this->GetScreenResolutionHook(aWidth, aHeight);
	if(aWidth==theWidth || aHeight==theHeight)
		return;
	this->SetScreenResolutionHook(theWidth,theHeight);
	this->mOriginalScreenWidth = aWidth;
	this->mOriginalScreenHeight = aHeight;
}

// FUNCTION: WON 0x0072c390
// Minimizes open windows that ask for it and switches back to the remembered screen resolution, if any
void WindowManager::RestoreScreenResolution()
{
	WindowSet::iterator anItr = this->mWindowSet.begin();
	while(anItr!=this->mWindowSet.end())
	{
		Window *aWindow = anItr->get();
		if(aWindow->mWindowState==Window::WindowState_Open && aWindow->mMinimizeOnResolutionRestore)
			aWindow->Minimize(true);

		++anItr;
	}

	if(this->mOriginalScreenWidth==0 || this->mOriginalScreenHeight==0)
		return;
	this->SetScreenResolution(this->mOriginalScreenWidth, this->mOriginalScreenHeight);
	this->mOriginalScreenWidth = 0;
	this->mOriginalScreenHeight = 0;
}

// FUNCTION: WON 0x0072c440
// Returns the window of the first open popup, or NULL when no popup is up
Window* WindowManager::GetPopupWindow()
{
	if(this->mPopupIsUp)
		return this->mPopupList.front()->GetRoot();
	else
		return NULL;
}

// FUNCTION: WON 0x0072c470
// Stores a font under a name (case-insensitive), replacing any earlier one
void WindowManager::SetNamedFont(const std::string &theName, Font *theFont)
{
	this->mNamedFontMap[theName] = theFont;
}

// FUNCTION: WON 0x0072c4d0
// Returns the font stored under a name, or the default font when there is none
Font* WindowManager::GetNamedFont(const std::string &theName)
{
	NamedFontMap::iterator anItr = this->mNamedFontMap.find(theName);
	if(anItr==this->mNamedFontMap.end())
		return GetDefaultFont();
	else
		return anItr->second.get();
}

// FUNCTION: WON 0x0072c540
// Returns the default font, creating Tahoma 8 on the default window the first time
Font* WindowManager::GetDefaultFont()
{
	if(this->mDefaultFont.get()==NULL)
		{
        this->mDefaultFont=this->mDefaultWindow->GetFont(FontDescriptor("Tahoma",0,8));
    }

	return this->mDefaultFont.get();
}

// FUNCTION: WON 0x0072c700
// Returns the default font with another style and, when the size is positive, another size
Font* WindowManager::GetDefaultFontMod(int theStyle, int theSize)
{
	Font *aFont = GetDefaultFont();
	FontDescriptor aDesc = aFont->mDescriptor;
	aDesc.mStyle = theStyle;
	if(theSize>0)
		aDesc.mSize = theSize;

	return this->mDefaultWindow->GetFont(aDesc).get();
}

// FUNCTION: WON 0x0072c7f0
// Adds or drops one image-caching request; when the count reaches 0 it empties the native and raw image caches
void WindowManager::SetCacheImages(bool cache)
{
	if(cache)
		this->mCacheImageCount++;
	else
	{
		this->mCacheImageCount--;
		if(this->mCacheImageCount<=0)
		{
			this->mCacheImageCount = 0;
			this->mNativeImageMap.clear();
			this->mRawImageMap.clear();
		}
	}
}

// FUNCTION: WON 0x0072c950
// Decodes an image file for a display context, cached while caching is on; NULL without a context
SmartPtr<NativeImage> WindowManager::DecodeImage(DisplayContext *theContext, const char *theFilePath)
{
	if(theContext==NULL)
		return NULL;

	if(this->mCacheImageCount)
	{
		NativeImageMap::iterator anItr = this->mNativeImageMap.insert(NativeImageMap::value_type(theFilePath,NULL)).first;
		if(anItr->second.get()==NULL)
			anItr->second = this->mImageDecoder.Decode(theContext,theFilePath);

		return anItr->second;
	}
	else
		return this->mImageDecoder.Decode(theContext,theFilePath);
}

// FUNCTION: WON 0x0072cc20
// Destroys a native image cache entry (file name and image)
// (??1?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VNativeImage@WONAPI@@@WONAPI@@@std@@QAE@XZ)

// FUNCTION: WON 0x0072ccb0
// Decodes an image file for the default window's display context; NULL when there is no window
SmartPtr<NativeImage> WindowManager::DecodeImage(const char *theFilePath)
{
	if(this->mDefaultWindow==NULL)
		return NULL;

	return this->DecodeImage(this->mDefaultWindow->mDisplayContext.get(),theFilePath);
}

// FUNCTION: WON 0x0072ccf0
// Returns an image that loads its file on first use, sized from the file's attributes; cached by path while
// caching is on; NULL when the attributes cannot be read
SmartPtr<Image> WindowManager::DecodeDelayImage(const char *theFilePath)
{
	if(this->mCacheImageCount)
	{
		std::pair<DelayImageMap::iterator,bool> aRet;
		aRet = this->mDelayImageMap.insert(DelayImageMap::value_type(theFilePath,NULL));

		if(aRet.second)
		{
			ImageAttributes anAttr;
			if(this->mImageDecoder.GetImageAttributes(theFilePath,anAttr))
				aRet.first->second = new DelayLoadImage(theFilePath,anAttr.mWidth,anAttr.mHeight);
		}

		return aRet.first->second;
	}
	else
	{
		ImageAttributes anAttr;
		if(this->mImageDecoder.GetImageAttributes(theFilePath,anAttr))
			return new DelayLoadImage(theFilePath,anAttr.mWidth,anAttr.mHeight);
		else
			return NULL;
	}
}

// FUNCTION: WON 0x0072d0d0
// Destroys a delay-load image cache entry (file name and image)
// (??1?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VImage@WONAPI@@@WONAPI@@@std@@QAE@XZ)

// FUNCTION: WON 0x0072d160
// Decodes an image file into a raw image, through the raw image cache while caching is on
SmartPtr<RawImage> WindowManager::DecodeImageRaw(const char *theFilePath)
{
	if(this->mCacheImageCount)
	{
		RawImageMap::iterator anItr = this->mRawImageMap.insert(RawImageMap::value_type(std::string(theFilePath),SmartPtr<RawImage>())).first;

		if(anItr->second.get()==NULL)
			anItr->second = this->mImageDecoder.DecodeRaw(theFilePath);

		return anItr->second;
	}
	else
		return this->mImageDecoder.DecodeRaw(theFilePath);
}

// FUNCTION: WON 0x0072d400
// Destroys a raw image cache entry (file name and image)
// (??1?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VRawImage@WONAPI@@@WONAPI@@@std@@QAE@XZ)

// FUNCTION: WON 0x0072d490
// Sets the decoder used for sounds
void WindowManager::SetSoundDecoder(SoundDecoder *theDecoder)
{
	this->mSoundDecoder = theDecoder;
}

// FUNCTION: WON 0x0072d4f0
// Decodes a sound with the sound decoder, or returns a plain sound for the descriptor when there is none
SmartPtr<Sound> WindowManager::DecodeSound(const SoundDescriptor &theDesc)
{
	if(this->mSoundDecoder.get()==NULL)
		return new Sound(theDesc);
	else
		return this->mSoundDecoder->DecodeDescriptor(theDesc);
}

// FUNCTION: WON 0x0072d5b0
// Closes every window, processes the resulting events and restores the screen resolution when full screen
void WindowManager::CloseAllWindows()
{
	bool hadWindows = !this->mWindowSet.empty();
	AutoCrit aCrit(this->mDataCrit);
	WindowSet::iterator anItr = this->mWindowSet.begin();
	while(anItr!=this->mWindowSet.end())
	{
		Window *aWindow = anItr->get();
		aWindow->Close();
		++anItr;
	}

	aCrit.Leave();

	if(hadWindows)
		this->ProcessEvents();

	if(this->mIsFullScreen)
		this->RestoreScreenResolution();
}

// FUNCTION: WON 0x0072d6a0
// Adds a window to the managed set (the first one becomes the default window); always returns true
bool WindowManager::AddWindow(Window *theWindow)
{
	AutoCrit aCrit(this->mDataCrit);
	if(this->mWindowSet.empty())
		this->mDefaultWindow = theWindow;

	if(this->mWindowSet.insert(theWindow).second)
		this->mWindowCount++;

	return true;
}

// FUNCTION: WON 0x0072d840
// Removes a window: drops its queued events, picks a new default window, ends its modal dialog and forgets
// it as the saved popup window; false when it was not managed
bool WindowManager::RemoveWindow(Window *theWindow)
{
	AutoCrit aCrit(this->mDataCrit);
		
	if(this->mWindowSet.erase(theWindow)>0)
	{
		this->mWindowCount--;
		if(this->mWindowCount<=0)
			this->mTargetMsgLoopDepth = 0;

		if(this->mWindowSet.empty())
			this->mDefaultWindow = NULL;
		else 
			this->mDefaultWindow = this->mWindowSet.begin()->get();

		
		WindowEventList::iterator anItr = this->mWindowEventList.begin();
		while(anItr!=this->mWindowEventList.end())
		{
			WindowEventBase *anEvent = anItr->get();
			if(anEvent->mWindow==theWindow)
				this->mWindowEventList.erase(anItr++);
			else
				++anItr;
		}

		if(theWindow==this->mModalWindow)
			this->EndDialog(-1,NULL,NULL);

		if(theWindow==this->mPopupWindowSave.get())
			this->mPopupWindowSave = NULL;

		return true;
	}

	return false;
}

// FUNCTION: WON 0x0072ddf0
// Makes the innermost message loop return
void WindowManager::ExitMessageLoop()
{
	this->mTargetMsgLoopDepth--;
}

// FUNCTION: WON 0x0072e7d0
// Runs a nested message loop until it is exited; returns the dialog result, or -1 when there are no windows
int WindowManager::MessageLoop()
{
	if(this->mWindowSet.empty())
		return -1;


	this->AddRef();

	this->mTargetMsgLoopDepth =  ++this->mCurMsgLoopDepth;
	while(this->mCurMsgLoopDepth<=this->mTargetMsgLoopDepth)
	{
		this->GetNextEvent();

	}


	this->mCurMsgLoopDepth--;
	int aDialogResult = this->mDialogResult;
	this->Release();

	return aDialogResult;
}

// FUNCTION: WON 0x0072e850
// Queues a window event and signals the event loop when asked or when no message loop is running
void WindowManager::PostEvent(WindowEventBase *theEvent, bool needSignal)
{
	WONAPI::AutoCrit aCrit(this->mDataCrit);
	this->mWindowEventList.push_back(theEvent);
	if(this->mCurMsgLoopDepth<=0)
		needSignal = true;

	aCrit.Leave();

	if(needSignal)
		this->Signal();
}

// FUNCTION: WON 0x0072e960
// Adds an event to be delivered on every timer tick
void WindowManager::AddTimerEvent(WindowEventBase *theEvent)
{
	this->mTimerEvents.insert(theEvent);
}

// FUNCTION: WON 0x0072ea70
// Removes a timer event, deferring the removal while timers are being delivered
void WindowManager::RemoveTimerEvent(WindowEventBase *theEvent)
{
	if(this->mCallingTimers)
		this->mRemoveTimerEvents.insert(theEvent);
	else
		this->mTimerEvents.erase(theEvent);
}

// FUNCTION: WON 0x0072f3f0
// Adds a component to or removes it from the set that receives broadcast events
void WindowManager::SubscribeToBroadcast(Component *theComponent, bool subscribe)
{
	if(subscribe)
		this->mBroadcastSet.insert(theComponent);
	else
		this->mBroadcastSet.erase(theComponent);
}

// FUNCTION: WON 0x0072f5d0
// Removes every component from the broadcast set
void WindowManager::ClearSubscriptions()
{
	this->mBroadcastSet.clear();
}

// FUNCTION: WON 0x0072f680
// Sends a component event to every subscribed component
void WindowManager::BroadcastEvent(ComponentEvent *theEvent)
{
	BroadcastSet::iterator anItr = this->mBroadcastSet.begin();
	while(anItr!=this->mBroadcastSet.end())
	{
		Component *aComponent = anItr->get();
		aComponent->HandleComponentEvent(theEvent);
		++anItr;
	}
}

// FUNCTION: WON 0x0072f700
// Returns the next queued window event without removing it, or NULL
WindowEventBase* WindowManager::PeekNextEvent()
{
	AutoCrit aCrit(this->mDataCrit);
	if(this->mWindowEventList.empty())
		return NULL;
	else
		return this->mWindowEventList.front().get();
}

// FUNCTION: WON 0x0072f760
// Every 50 ms ends or keeps popups by focus, restores the resolution after focus loss and ticks windows and
// timer events; then delivers queued events and idles every window (deferred timer removals are never cleared)
void WindowManager::ProcessEvents()
{

	DWORD aTick = GetTickCount();
	DWORD anElapsed = aTick - this->mLastTimerTick;
	if(anElapsed >= 50)
	{
		if(anElapsed >= 1000)
			anElapsed = 1000;

		this->mLastTimerTick = aTick;

		if(this->mPopupIsUp && this->mFocusWindow!=this->mPopupOwnerWindow && ((bool)(((unsigned)this->mPopupFlags>>6)&1)))
		{
			if(this->mFocusWindow!=NULL && this->mFocusWindow->mIsPopup)
			{
				this->mEndPopup = false;
				this->mPopupOwnerWindow->PlatformSetFocus();
			}
			else if(this->mEndPopup)
				this->EndPopup(NULL);
			else
				this->mEndPopup = true;
		}
		else
			this->mEndPopup = false;

		if(this->mLastLoseFocusTick!=0 && this->mFocusWindow==NULL && aTick-this->mLastLoseFocusTick>50)
		{
			this->mLastLoseFocusTick = 0;
			if(this->mIsFullScreen)
				this->RestoreScreenResolution();
		}	

		WindowSet::iterator anItr = this->mWindowSet.begin();
		while(anItr!=this->mWindowSet.end())
		{
			Window *aWindow = anItr->get();
			aWindow->TimerEvent(anElapsed);
			++anItr;
		}

		if(!this->mTimerEvents.empty())
		{
			this->mCallingTimers = true;
			for(WindowEventSet::iterator aTimerItr = this->mTimerEvents.begin(); aTimerItr != this->mTimerEvents.end(); ++aTimerItr)
				(*aTimerItr)->Deliver();
			this->mCallingTimers = false;
		}

		if(!this->mRemoveTimerEvents.empty()) 
		{
			for(WindowEventSet::iterator aTimerItr = this->mRemoveTimerEvents.begin(); aTimerItr != this->mRemoveTimerEvents.end(); ++aTimerItr)
				this->mTimerEvents.erase(*aTimerItr);
		}
	}

	WONAPI::AutoCrit aCrit(this->mDataCrit, false);
	while(!this->mWindowEventList.empty()  && this->mCurMsgLoopDepth<=this->mTargetMsgLoopDepth)
	{
		aCrit.Enter();
		SmartPtr<WindowEventBase> anEvent = this->mWindowEventList.front();
		this->mWindowEventList.pop_front();
		aCrit.Leave();

		this->DeliverEvent(anEvent.get());
	}


	WindowSet::iterator anItr = this->mWindowSet.begin();
	while(anItr!=this->mWindowSet.end())
	{
		Window *aWindow = anItr->get();
		aWindow->IdleUpdate();
		++anItr;
	}
}


// FUNCTION: WON 0x0072dbe0
// Sets or clears the modal flag on a component and its ancestors, making each ancestor enter or leave modal mode
static void SetModal(Component* component,bool modal) {
    component->SetComponentFlags(0x8000,modal);
    Container* parent=component->mParent;
    while(parent!=NULL) {
        parent->SetComponentFlags(0x8000,modal);
        if(modal) parent->EnterModal(); else parent->LeaveModal();
        parent=parent->mParent;
    }
}
// Point-in-rectangle test used by FindMouseWindow (inline)
static inline bool PopupRectContains(const WONRectangle& r,int x,int y) {
    return x>=r.x && x<r.x+r.width && y>=r.y && y<r.y+r.height;
}
// FUNCTION: WON 0x0072da30
// Recomputes modal state from the open popups and the modal dialog: flags popups, the popup input component
// and the dialog, and makes every other window modal
void WindowManager::CalcModality()
{
	
	if(this->mPopupInputComponent.get()!=NULL)
		SetModal(this->mPopupInputComponent.get(),false);
	if(this->mModalComponent!=NULL)
		SetModal(this->mModalComponent,false);

	bool modal = false;
	this->mPopupIsUp = false;
	if(!this->mPopupList.empty())
	{
		
		this->mPopupIsUp = true;
		PopupList::iterator anItr = this->mPopupList.begin();
		while(anItr!=this->mPopupList.end())
		{
			Component *aPopup = *anItr;
			aPopup->SetComponentFlags(0x8000 | 0x10000,true);
			++anItr;
		}

		if(((this->mPopupFlags & (PopupFlag_IsModal))?true:false))
			modal = true;
	}
	
	if(this->mModalComponent!=NULL)
	{
		if(!modal) 
			SetModal(this->mModalComponent,true);

		modal = true;
	}

	if(!this->mPopupIsUp && this->mPopupOwnerComponent.get()!=NULL)
		this->mPopupOwnerComponent = NULL;

	if(this->mPopupInputComponent.get()!=NULL)
	{ 
		if(this->mPopupIsUp)
			SetModal(this->mPopupInputComponent.get(),true);
		else
			this->mPopupInputComponent = NULL;
	}

	WindowSet::iterator anItr = this->mWindowSet.begin();
	while(anItr!=this->mWindowSet.end())
	{
		Window *aWindow = anItr->get();
		if(aWindow==this->mModalWindow && !this->mPopupIsUp && this->mModalComponent==aWindow->mRoot.get())
			aWindow->SetModal(false);
		else
			(*anItr)->SetModal(modal);
		
		++anItr;
	}
}

// FUNCTION: WON 0x0072dc30
// Returns true when a modal window exists and the given window is not it
bool WindowManager::NotModalWindow(Window *theWindow)
{
	if(this->mModalWindow!=NULL && theWindow!=this->mModalWindow)
		return true;
	else
		return false;
}

// FUNCTION: WON 0x0072dc50
// Shows a component (default: the window's root) as a modal dialog until it ends; returns its result
int WindowManager::DoDialog(Window *theWindow, Component *theComponent)
{
	SmartPtr<Component> anOldDialog = this->mModalComponent;
	SmartPtr<Window> anOldModalWindow = this->mModalWindow;
	this->mDialogResult = 0;

	if(anOldDialog.get()!=NULL)
		anOldDialog->SetComponentFlags(0x8000,false);

	if(theComponent==NULL)
		theComponent = theWindow->mRoot.get();

	if(this->mModalComponent!=NULL)
		SetModal(this->mModalComponent,false);

	this->mModalWindow = theWindow;
	this->mModalComponent = theComponent;
	this->CalcModality();
	this->mModalWindow->PlatformSetFocus();
	theComponent->RequestFocus();


		this->MessageLoop();

	SetModal(this->mModalComponent,false);
	this->mModalWindow = anOldModalWindow.get();
	this->mModalComponent = anOldDialog.get();
	this->CalcModality();




	return this->mDialogResult;
}

// FUNCTION: WON 0x0072ddb0
// Ends the current modal dialog with a result, unless another window or dialog is named
void WindowManager::EndDialog(int theResult, Window *theWindow, Component *theDialog)
{
	if(theWindow!=NULL && theWindow!=this->mModalWindow)
		return;

	if(theDialog!=NULL && theDialog!=this->mModalComponent)
		return;

	this->mDialogResult = theResult;
	this->mTargetMsgLoopDepth--;
}

// FUNCTION: WON 0x0072de00
// Moves a popup back inside the screen or the parent's root, flipping it to the other side of the avoid rectangle
void WindowManager::AdjustPopupPosition(Window *theParent, Component *theComponent, bool inOwnWindow, const WONRectangle &theAvoidRect)
{
	WONRectangle aBoundingRect;
	WONRectangle anAvoidRect = theAvoidRect;
	if(inOwnWindow)
	{
		this->GetScreenRect(aBoundingRect);

		WONRectangle aWindowRect;
		theParent->GetRect(aWindowRect, true);








		aBoundingRect.x -= aWindowRect.Left();
		aBoundingRect.y -= aWindowRect.Top();
	}
	else
	{
		RootContainer* aRoot = theParent->mRoot.get();
		aBoundingRect = aRoot->GetBounds();

	}

	int dx = 0, dy = 0;
	if(theComponent->Right() > aBoundingRect.Right())
	{
		dx = aBoundingRect.Right() - theComponent->Right();
		theComponent->SetPos(theComponent->Left() + dx, theComponent->Top());
		if(anAvoidRect.Width()>0 && theComponent->Left()<=theAvoidRect.Right() && theComponent->Right()>=theAvoidRect.x)
			theComponent->SetPos(anAvoidRect.Left()-theComponent->Width(),theComponent->Top());
	}
	else if(theComponent->Left() < aBoundingRect.x)
	{
		dx = aBoundingRect.Left() - theComponent->Left();
		theComponent->SetPos(theComponent->Left() + dx,theComponent->Top());
		if(anAvoidRect.Width()>0 && theComponent->Left()<=theAvoidRect.Right() && theComponent->Right()>=theAvoidRect.x)
			theComponent->SetPos(anAvoidRect.Right(),theComponent->Top());
	}

	if(theComponent->Bottom() > aBoundingRect.Bottom())
	{
		dy = aBoundingRect.Bottom() - theComponent->Bottom();
		theComponent->SetPos(theComponent->Left(), theComponent->Top() + dy);
		if(anAvoidRect.Height()>0 && theComponent->Top()<=theAvoidRect.Bottom() && theComponent->Bottom()>=theAvoidRect.y)
			theComponent->SetPos(theComponent->Left(), anAvoidRect.Top()-theComponent->Height());
	}
	else if(theComponent->Top() < aBoundingRect.y)
	{
		dy = aBoundingRect.Top() - theComponent->Top();
		theComponent->SetPos(theComponent->Left(), theComponent->Top() + dy);
		if(anAvoidRect.Height()>0 && theComponent->Top()<=theAvoidRect.Bottom() && theComponent->Bottom()>=theAvoidRect.y)
			theComponent->SetPos(theComponent->Left(), anAvoidRect.Bottom());
	}
}

// FUNCTION: WON 0x0072e040
// Opens a popup at the given or mouse position, in its own popup window or inside the parent window, per
// the popup flags; ignored while another popup opens or closes, or when the parent lacks focus
void WindowManager::DoPopup(const PopupParams &theParams)
{
	
	SmartPtr<Window> aParent = theParams.mParent;
	SmartPtr<Component> aPopup = theParams.mPopup;

	if(this->mInDoPopup || this->mInEndPopup)
		return;

	bool additional = (theParams.mFlags&PopupFlag_Additional) && !this->mPopupList.empty();
	Window *aFocusCheckWindow = aParent.get();
	if(additional)
		aFocusCheckWindow = this->mPopupOwnerWindow;


	if(this->mFocusWindow!=aFocusCheckWindow)
		return;

	this->mInDoPopup = true;
	if(this->mPopupIsUp && !additional)
		this->EndPopup(NULL);

	int x = theParams.mx;
	int y = theParams.my;
	if(x==-1)
		x = aParent->mMouseX;
	if(y==-1)
		y = aParent->mMouseY;

	aPopup->SetPos(x,y);

	this->mEndPopup = false;

	if(!additional)
	{
		this->mPopupFlags = theParams.mFlags;
		this->mPopupOwnerWindow = aParent.get();
		this->mPopupInputComponent = theParams.mInputComponent;
	}

	this->mPopupList.push_back(aPopup.get());
	bool inOwnWindow = this->mBA5;
	if(((this->mPopupFlags & (PopupFlag_InOwnWindow))?true:false))
		inOwnWindow = true;
	else if(((this->mPopupFlags & (PopupFlag_NotInOwnWindow))?true:false))
		inOwnWindow = false;

	if(((this->mPopupFlags & (PopupFlag_AdjustPosition))?true:false))
		this->AdjustPopupPosition(aParent.get(),aPopup.get(),inOwnWindow,theParams.mAvoidRect);

	SmartPtr<Window> aPopupWindow;
	if(inOwnWindow)
	{
		WONRectangle aRect;
		aParent->GetRect(aRect,true);
		aRect.x+=aPopup->Left();
		aRect.y+=aPopup->Top();
		aRect.width = aPopup->Width();
		aRect.height = aPopup->Height();

		if(this->mPopupWindowSave.get()!=NULL)
		{
			if(this->mPopupWindowSave->mParent!=aParent.get())
				this->mPopupWindowSave->Close();
			else
			{
				this->mPopupWindowSave->SetPosSize(aRect.x, aRect.y, aRect.width, aRect.height);
				this->mPopupWindowSave->mIsPopup = true;
				this->mPopupWindowSave->Show(true);
				aPopupWindow = this->mPopupWindowSave;
			}
			this->mPopupWindowSave = NULL;
		}
				
		if(aPopupWindow.get()==NULL)
		{
			aPopupWindow = new MSWindow;
			aPopupWindow->mIsOneComponentWindow = true;
			aPopupWindow->mStyle=0x20;
			aPopupWindow->SetParent(aParent.get());
			aPopupWindow->mIsPopup = true;

			aPopupWindow->Create(aRect);
		}
		aPopupWindow->mIsPopup = true;

		if(((this->mPopupFlags & (PopupFlag_WantInput))?true:false))
		{
			this->mPopupOwnerWindow->PlatformSetFocus();	
			this->mPopupOwnerWindow->mRoot->LostFocus();
		}

		aPopup->SetPos(0,0);
	}
	else
	{
		aPopupWindow = aParent;
		aPopup->SetComponentFlags(ComponentFlag_GrabBG,true);
		if(((this->mPopupFlags & (PopupFlag_DontWantMouse))?true:false))
			aPopup->SetComponentFlags(ComponentFlag_NoInput,true);
	}

	this->mPopupOwnerComponent = theParams.mOwnerComponent;
	if(theParams.mOwnerComponent!=NULL && aPopup->GetOwnColorScheme()==NULL)
		aPopup->SetColorScheme(theParams.mOwnerComponent->GetColorScheme());

	aPopup->SetComponentFlags(ComponentFlag_AlwaysOnTop,true);
	aPopupWindow->AddComponent(aPopup.get());
	if(((this->mPopupFlags & (PopupFlag_WantInput))?true:false))
		aPopup->RequestFocus();

	if(this->mDoMouseCapturePopupSupport)
		this->mPopupOwnerWindow->PlatformSetMouseCapture(true);

	this->mInDoPopup = false;
	this->CalcModality();
}

// FUNCTION: WON 0x0072e4d0
// Closes the given popup and every popup opened after it (NULL: all), firing the end-popup event and
// keeping the last popup window for reuse
void WindowManager::EndPopup(Component *thePopup)
{
	if(this->mPopupList.empty())
		return;

	if(thePopup!=NULL && !thePopup->ComponentFlagSet(0x10000))
		return;

	bool inEndPopup = this->mInEndPopup;
	this->mInEndPopup = true; 
	while(!this->mPopupList.empty() && this->mPopupList.back()!=thePopup)
		this->EndPopup(this->mPopupList.back());

	if(this->mPopupList.empty())
	{
		this->mInEndPopup = inEndPopup;
		return;
	}

	this->mEndPopup = false;

	if(this->mPopupIsUp)
	{
		SmartPtr<Component> aPopupComponent = this->mPopupList.back();
		SmartPtr<Window> aPopupWindow = aPopupComponent->GetRoot();
		SmartPtr<Window> aPopupOwnerWindow = this->mPopupOwnerWindow;
		this->mPopupList.pop_back();

		if(this->mPopupFlags&PopupFlag_FireEventOnEnd)
			aPopupComponent->Fn2F5380(ComponentEvent_EndPopup,0);

		if(this->mPopupList.empty())
		{
			if(this->mDoMouseCapturePopupSupport)
				this->mPopupOwnerWindow->PlatformSetMouseCapture(false);

			this->mPopupOwnerWindow = NULL;
			this->mMouseDownWindow = NULL;
		}

		if(aPopupWindow.get()!=aPopupOwnerWindow.get())
		{
			aPopupWindow->mIsPopup = false; 
			if(this->mPopupList.empty())
				this->mPopupWindowSave = aPopupWindow;

			aPopupWindow->mRoot->RemoveAllChildren();
			if(this->mPopupWindowSave.get()!=NULL)
			{
				aPopupWindow->mRoot->RemoveAllChildren();
				if(!this->mInDoPopup)
					aPopupWindow->Show(false);
			}
			else
				aPopupWindow->Close();
			
			if(this->mPopupList.empty() && ((this->mPopupFlags & (PopupFlag_WantInput))?true:false))
			{
				if(this->mFocusWindow==aPopupOwnerWindow.get())
					aPopupOwnerWindow->mRoot->GotFocus();
			}
		}
		else
		{
			aPopupComponent->SetComponentFlags(0x8000 | 0x10000,false);
			aPopupWindow->mRoot->RemoveChild(aPopupComponent.get());
		}

		this->CalcModality();
	}

	this->mInEndPopup = inEndPopup;
}

// FUNCTION: WON 0x0072e790
// Closes the popups when the popup's owner component lies inside the given component
void WindowManager::CheckEndPopup(Component *theAncestor)
{
	if(!this->mPopupIsUp || this->mPopupOwnerComponent.get()==NULL)
		return;

	Component *aComponent = this->mPopupOwnerComponent.get();
	while(aComponent!=NULL)
	{
		if(aComponent==theAncestor)
		{
			this->EndPopup(NULL);
			return;
		}
		aComponent = aComponent->mParent;
	}
}

// FUNCTION: WON 0x0072ec60
// Redirects a mouse event to the popup owner, the mouse-down window or the topmost popup window under the mouse
void WindowManager::FindMouseWindow(MouseEvent *theEvent)
{
	if(!this->mPopupIsUp)
		return;

	if(((this->mPopupFlags & (PopupFlag_DontWantMouse))?true:false) && theEvent->mWindow->mIsPopup)
	{
		WONRectangle aPopupRect, anOwnerRect;
		theEvent->mWindow->GetRect(aPopupRect,true);
		this->mPopupOwnerWindow->GetRect(anOwnerRect,true);
		anOwnerRect.x -= aPopupRect.x;
		anOwnerRect.y -= aPopupRect.y;
		if(PopupRectContains(anOwnerRect,theEvent->mx,theEvent->my))
		{
			theEvent->mWindow = this->mPopupOwnerWindow;
			theEvent->mx -= anOwnerRect.x;
			theEvent->my -= anOwnerRect.y;
			return;
		}
	}

	if(!this->mDoMouseCapturePopupSupport)
		return;

	WONRectangle anOwnerRect;
	theEvent->mWindow->GetRect(anOwnerRect,true);
	if(this->mMouseDownWindow!=NULL && this->mMouseDownWindow->mMouseIsDown[0])
	{
		WONRectangle aRect;
		this->mMouseDownWindow->GetRect(aRect,true);
		aRect.x -= anOwnerRect.x;
		aRect.y -= anOwnerRect.y;

		theEvent->mWindow = this->mMouseDownWindow;
		theEvent->mx -= aRect.x;
		theEvent->my -= aRect.y;
		return;
	}

	PopupList::iterator anItr  = this->mPopupList.end();
	do
	{
		WONRectangle aRect;

		--anItr;
		Window *aWindow = (*anItr)->GetRoot();
		aWindow->GetRect(aRect,true);
		aRect.x -= anOwnerRect.x;
		aRect.y -= anOwnerRect.y;

		if(PopupRectContains(aRect,theEvent->mx,theEvent->my))
		{
			theEvent->mWindow = aWindow;
			theEvent->mx -= aRect.x;
			theEvent->my -= aRect.y;
			return;
		}
	} while(anItr!=this->mPopupList.begin());
}

// FUNCTION: WON 0x0072ee40
// Handles a mouse click while a popup is up: closes the popup on an outside click; returns true when that
// click is to be swallowed
bool WindowManager::CheckPopupClick(MouseEvent *theEvent)
{
	this->FindMouseWindow(theEvent);
	if(theEvent->mButtonOrAmount==0)
		this->mMouseDownWindow = theEvent->mWindow;

	if(((this->mPopupFlags & (1<<theEvent->mButtonOrAmount))?true:false)) 
	{
		int x = theEvent->mx, y = theEvent->my;
		Container *aContainer = theEvent->mWindow->mRoot.get();
		bool endPopup = true;
		while(true)
		{
			x-=aContainer->Left();
			y-=aContainer->Top();

			Component *aClickChild = aContainer->FindClickChildAt(x, y);
			if(aClickChild!=NULL)
			{ 
				if(aClickChild==this->mPopupInputComponent.get() || ((aClickChild->mComponentBits & 0x10000)?true:false))
				{
					endPopup = false;
					break;
				}
				else if(this->mPopupInputComponent.get()==NULL)
					break;
				else
				{
					aContainer = dynamic_cast<Container*>(aClickChild);
					if(aContainer==NULL)
						break;
				}
			}
			else
				break;
		}

		if(endPopup)
		{
			bool eat = ((this->mPopupFlags & (PopupFlag_EatMouseDownOnEnd))?true:false);
			this->EndPopup(NULL);
			if(eat)
				return true;
		}
	}

	if(theEvent->mWindow->mIsPopup)
		this->mPopupOwnerWindow->PlatformSetFocus();	

	return false;
}

// FUNCTION: WON 0x0072ef20
// Dispatches one window event: mouse, key, focus, resize, close and other events, applying popup, modal
// and full-screen rules; merges repeated mouse moves and size changes
void WindowManager::DeliverEvent(WindowEventBase *theEvent)
{
	switch(theEvent->mEventType)
	{
		case InputEvent_MouseDoubleClick:
		case InputEvent_MouseDownCheckDoubleClick:
		case InputEvent_MouseDown:
		{
			MouseEvent *anEvent = static_cast<MouseEvent*>(theEvent);
			if(this->mPopupIsUp)
			{
				if(this->CheckPopupClick(anEvent))
					break;
			}
			if(this->mFocusWindow!=anEvent->mWindow && !anEvent->mWindow->mIsPopup && !this->NotModalWindow(anEvent->mWindow))
				anEvent->mWindow->PlatformSetFocus();

			if(anEvent->mButtonOrAmount==0 && (!this->mPopupIsUp || !this->mDoMouseCapturePopupSupport))
				anEvent->mWindow->PlatformSetMouseCapture(true);
		}

		if(theEvent->mWindow->mMouseIsInClientArea) 
			theEvent->mWindow->HandleMouseEvent(static_cast<MouseEvent*>(theEvent));
		break;

		case InputEvent_MouseMove:
		{
			WindowEventBase *nextEvent = this->PeekNextEvent();
			if(nextEvent!=NULL && nextEvent->mEventType==InputEvent_MouseMove && nextEvent->mWindow==theEvent->mWindow)
				return; 

			this->FindMouseWindow(static_cast<MouseEvent*>(theEvent));
			if(this->mPopupIsUp && theEvent->mEventType==InputEvent_MouseMove && ((this->mPopupFlags & (PopupFlag_EndOnMouseMove))?true:false))
				this->EndPopup(NULL);

			theEvent->mWindow->HandleMouseEvent(static_cast<MouseEvent*>(theEvent));
			break;
		}

		case InputEvent_MouseUp:
		{
			MouseEvent *anEvent = static_cast<MouseEvent*>(theEvent);
			this->FindMouseWindow(anEvent);
			if(anEvent->mButtonOrAmount==0)
				this->mMouseDownWindow = NULL;

			if(anEvent->mButtonOrAmount==0 && (!this->mPopupIsUp || !this->mDoMouseCapturePopupSupport))
				theEvent->mWindow->PlatformSetMouseCapture(false);

			theEvent->mWindow->HandleMouseEvent(static_cast<MouseEvent*>(theEvent));
			break;
		}

		case InputEvent_MouseWheel:
		{
			MouseEvent *anEvent = static_cast<MouseEvent*>(theEvent);
			this->FindMouseWindow(anEvent);
			if(this->mPopupIsUp && !((this->mPopupFlags & (PopupFlag_DontWantMouse))?true:false))
			{
				
				Component *aPopupComponent = this->mPopupList.back();
				aPopupComponent->MouseWheel(anEvent->mButtonOrAmount);
				return;


			}
			theEvent->mWindow->HandleMouseEvent(anEvent);
			break;
		}

		case InputEvent_KeyDown:
		case InputEvent_KeyUp:
		case InputEvent_KeyChar:
			if(this->mPopupIsUp)
			{
				int aKey = static_cast<KeyEvent*>(theEvent)->mKey;
				if(theEvent->mEventType==InputEvent_KeyDown)
				{	
					if(((this->mPopupFlags & (PopupFlag_EndOnKeyDown))?true:false) || ((this->mPopupFlags & (PopupFlag_EndOnEscape))?true:false) && aKey==VK_ESCAPE)
					{
						this->EndPopup(NULL);
						break;
					}
				}
				
				if(((this->mPopupFlags & (PopupFlag_WantInput))?true:false))
				{
					Window *aPopupWindow = this->mPopupList.back()->GetRoot();
					if(aPopupWindow!=theEvent->mWindow)
						theEvent->mWindow = aPopupWindow;
				}
			}
			theEvent->mWindow->HandleKeyEvent(static_cast<KeyEvent*>(theEvent));
			break;

		case InputEvent_GotFocus:
			if(this->mIsFullScreen)
				this->SetScreenResolution(this->mFullScreenWidth,this->mFullScreenHeight);

			this->NotifyGotFocus(theEvent->mWindow);
			theEvent->mWindow->HandleInputEvent(static_cast<InputEvent*>(theEvent));
			if(this->NotModalWindow(theEvent->mWindow))
				this->mModalWindow->PlatformSetFocus();
			return;

		case InputEvent_LostFocus:
			this->NotifyLostFocus();
			theEvent->mWindow->HandleInputEvent(static_cast<InputEvent*>(theEvent));
			return;

		case InputEvent_SizeChange:
			if(theEvent->mWindow==this->mPopupOwnerWindow)
				this->EndPopup(NULL);			
			else
			{
				WindowEventBase *nextEvent = this->PeekNextEvent();
				if(nextEvent!=NULL && nextEvent->mEventType==InputEvent_SizeChange && nextEvent->mWindow==theEvent->mWindow)
				{
					SizeEvent *s1 = static_cast<SizeEvent*>(theEvent);
					SizeEvent *s2 = static_cast<SizeEvent*>(nextEvent);
					if(s2->mx==-1)
						s2->mx = s1->mx;
					if(s2->my==-1)
						s2->my = s1->my;
					if(s2->mWidth==-1)
						s2->mWidth = s1->mWidth;
					if(s2->mHeight==-1)
						s2->mHeight = s1->mHeight;
					return; 
				}
			}

			theEvent->mWindow->HandleInputEvent(static_cast<InputEvent*>(theEvent));
			return;


		case InputEvent_MouseExit:
			theEvent->mWindow->HandleInputEvent(static_cast<InputEvent*>(theEvent));
			return;

		case InputEvent_WindowClose:
		{
			if(this->mPopupIsUp)
				if(theEvent->mWindow==this->mPopupOwnerWindow || theEvent->mWindow->mIsPopup)
					this->EndPopup(NULL);




			if(this->mFocusWindow==theEvent->mWindow)
				this->NotifyLostFocus();

			if(this->mMouseDownWindow==theEvent->mWindow)
				this->mMouseDownWindow = NULL;

			theEvent->mWindow->PlatformClose();
			SmartPtr<WindowEventBase> aCloseEvent = theEvent->mWindow->mCloseEvent;
			if(aCloseEvent.get()!=NULL)
			{
				aCloseEvent->mWindow = theEvent->mWindow;
				aCloseEvent->Deliver();
			}

			this->RemoveWindow(theEvent->mWindow);
		}
		return;


		default:
			theEvent->Deliver();
			return;
	}

	this->mLastInputTick = GetTickCount();
}

// Class statics: the default window manager and the GUI thread id (constant-initialised)
WindowManager* WindowManager::mDefaultWindowManager = NULL;
DWORD WindowManager::mThreadId = 0;

// Not identified in the original exe: GetNamedFontMod (a named font with another style/size), GetDefaultFont
// for a descriptor, GetDefaultDisplayContext, CheckModalValid (the modal window is open and visible and the
// dialog is shown in it) and RemoveAllTimerEvents
Font* WindowManager::GetNamedFontMod(const std::string& theName, int theStyle, int theSize)
{
    Font* aFont = this->GetNamedFont(theName);
    FontDescriptor aDesc = aFont->GetDescriptor();
    aDesc.mStyle = theStyle;
    if(theSize > 0)
        aDesc.mSize = theSize;

    return this->mDefaultWindow->GetFont(aDesc).get();
}

SmartPtr<Font> WindowManager::GetDefaultFont(const FontDescriptor& theDescriptor)
{
    Window* aWindow = Window::GetDefaultWindow();
    if(aWindow == NULL)
        return NULL;
    else
        return aWindow->GetFont(theDescriptor);
}

DisplayContext* WindowManager::GetDefaultDisplayContext()
{
    Window* aWindow = Window::GetDefaultWindow();
    if(aWindow == NULL)
        return NULL;
    else
        return aWindow->GetDisplayContext();
}

bool WindowManager::CheckModalValid()
{
    if(this->mModalWindow == NULL)
        return true;
    if(this->mModalWindow->mWindowState != Window::WindowState_Open)
        return false;

    if(!this->mModalWindow->PlatformIsVisible())
        return false;

    if(this->mModalComponent != NULL)
    {
        if(!this->mModalComponent->IsVisible())
            return false;

        if(this->mModalComponent->GetRootContainer() != this->mModalWindow->mRoot.get())
            return false;
    }

    return true;
}

void WindowManager::RemoveAllTimerEvents()
{
    if(this->mCallingTimers)
        this->mRemoveTimerEvents = this->mTimerEvents;
    else
        this->mTimerEvents.clear();
}

// Runs one pass of the event loop (not identified in the original exe)
void WindowManager::PumpOnce()
{
	this->GetNextEvent();
}

// FUNCTION: WON 0x0072fc40
// Destroys a named-font map entry (name and font)
// (??1?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VFont@WONAPI@@@WONAPI@@@std@@QAE@XZ)

// FUNCTION: WON 0x0072fcd0
// Links a new node into the native image cache tree and rebalances it
// (?_Insert@?$_Tree@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VNativeImage@WONAPI@@@WONAPI@@@2@U_Kfn@?$map@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VNativeImage@WONAPI@@@WONAPI@@UStringLessNoCase@4@V?$allocator@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VNativeImage@WONAPI@@@WONAPI@@@std@@@2@@2@UStringLessNoCase@WONAPI@@V?$allocator@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VNativeImage@WONAPI@@@WONAPI@@@std@@@2@$0A@@std@@IAE?AViterator@12@_NPAU_Node@12@ABU?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VNativeImage@WONAPI@@@WONAPI@@@2@@Z)

// FUNCTION: WON 0x0072ffd0
// Links a new node into the raw image cache tree and rebalances it
// (?_Insert@?$_Tree@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VRawImage@WONAPI@@@WONAPI@@@2@U_Kfn@?$map@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VRawImage@WONAPI@@@WONAPI@@UStringLessNoCase@4@V?$allocator@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VRawImage@WONAPI@@@WONAPI@@@std@@@2@@2@UStringLessNoCase@WONAPI@@V?$allocator@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VRawImage@WONAPI@@@WONAPI@@@std@@@2@$0A@@std@@IAE?AViterator@12@_NPAU_Node@12@ABU?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VRawImage@WONAPI@@@WONAPI@@@2@@Z)

// FUNCTION: WON 0x007302d0
// Links a new node into the delay-load image cache tree and rebalances it
// (?_Insert@?$_Tree@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VImage@WONAPI@@@WONAPI@@@2@U_Kfn@?$map@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VImage@WONAPI@@@WONAPI@@UStringLessNoCase@4@V?$allocator@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VImage@WONAPI@@@WONAPI@@@std@@@2@@2@UStringLessNoCase@WONAPI@@V?$allocator@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VImage@WONAPI@@@WONAPI@@@std@@@2@$0A@@std@@IAE?AViterator@12@_NPAU_Node@12@ABU?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VImage@WONAPI@@@WONAPI@@@2@@Z)

// FUNCTION: WON 0x007305d0
// Erases a range of entries from the window set (the whole tree at once when the range is everything)
// (?erase@?$_Tree@V?$SmartPtr@VWindow@WONAPI@@@WONAPI@@V12@U_Kfn@?$_Setk@V?$SmartPtr@VWindow@WONAPI@@@WONAPI@@UComp@12@V?$allocator@V?$SmartPtr@VWindow@WONAPI@@@WONAPI@@@std@@@std@@UComp@12@V?$allocator@V?$SmartPtr@VWindow@WONAPI@@@WONAPI@@@5@$0A@@std@@QAE?AViterator@12@V312@0@Z)

// FUNCTION: WON 0x00730680
// Returns the range of timer events equal to the given event
// (?equal_range@?$_Tree@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@V12@U_Kfn@?$set@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@U?$less@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@std@@V?$allocator@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@4@@std@@U?$less@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@5@V?$allocator@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@5@$0A@@std@@QAE?AU?$pair@Viterator@?$_Tree@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@V12@U_Kfn@?$set@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@U?$less@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@std@@V?$allocator@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@4@@std@@U?$less@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@5@V?$allocator@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@5@$0A@@std@@V123@@2@ABV?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@Z)

// FUNCTION: WON 0x007306e0
// Links a new node into the window set tree and rebalances it
// (?_Insert@?$_Tree@V?$SmartPtr@VWindow@WONAPI@@@WONAPI@@V12@U_Kfn@?$set@V?$SmartPtr@VWindow@WONAPI@@@WONAPI@@UComp@12@V?$allocator@V?$SmartPtr@VWindow@WONAPI@@@WONAPI@@@std@@@std@@UComp@12@V?$allocator@V?$SmartPtr@VWindow@WONAPI@@@WONAPI@@@5@$0A@@std@@IAE?AViterator@12@_NPAU_Node@12@ABV?$SmartPtr@VWindow@WONAPI@@@WONAPI@@@Z)

// FUNCTION: WON 0x007309e0
// Advances a broadcast component set iterator to the next entry
// (?_Inc@const_iterator@?$_Tree@V?$SmartPtr@VComponent@WONAPI@@@WONAPI@@V12@U_Kfn@?$set@V?$SmartPtr@VComponent@WONAPI@@@WONAPI@@U?$less@V?$SmartPtr@VComponent@WONAPI@@@WONAPI@@@std@@V?$allocator@V?$SmartPtr@VComponent@WONAPI@@@WONAPI@@@4@@std@@U?$less@V?$SmartPtr@VComponent@WONAPI@@@WONAPI@@@5@V?$allocator@V?$SmartPtr@VComponent@WONAPI@@@WONAPI@@@5@$0A@@std@@QAEXXZ)

// FUNCTION: WON 0x00730a30
// Erases a range of entries from the timer event set (the whole tree at once when the range is everything)
// (?erase@?$_Tree@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@V12@U_Kfn@?$set@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@U?$less@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@std@@V?$allocator@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@4@@std@@U?$less@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@5@V?$allocator@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@5@$0A@@std@@QAE?AViterator@12@V312@0@Z)

// FUNCTION: WON 0x00730ae0
// Links a new node into the timer event set tree and rebalances it
// (?_Insert@?$_Tree@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@V12@U_Kfn@?$set@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@U?$less@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@std@@V?$allocator@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@4@@std@@U?$less@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@5@V?$allocator@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@5@$0A@@std@@IAE?AViterator@12@_NPAU_Node@12@ABV?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@Z)

// FUNCTION: WON 0x00730de0
// Erases a range of entries from the broadcast component set (the whole tree at once when the range is everything)
// (?erase@?$_Tree@V?$SmartPtr@VComponent@WONAPI@@@WONAPI@@V12@U_Kfn@?$set@V?$SmartPtr@VComponent@WONAPI@@@WONAPI@@U?$less@V?$SmartPtr@VComponent@WONAPI@@@WONAPI@@@std@@V?$allocator@V?$SmartPtr@VComponent@WONAPI@@@WONAPI@@@4@@std@@U?$less@V?$SmartPtr@VComponent@WONAPI@@@WONAPI@@@5@V?$allocator@V?$SmartPtr@VComponent@WONAPI@@@WONAPI@@@5@$0A@@std@@QAE?AViterator@12@V312@0@Z)

// FUNCTION: WON 0x00730e90
// Links a new node into the broadcast component set tree and rebalances it
// (?_Insert@?$_Tree@V?$SmartPtr@VComponent@WONAPI@@@WONAPI@@V12@U_Kfn@?$set@V?$SmartPtr@VComponent@WONAPI@@@WONAPI@@U?$less@V?$SmartPtr@VComponent@WONAPI@@@WONAPI@@@std@@V?$allocator@V?$SmartPtr@VComponent@WONAPI@@@WONAPI@@@4@@std@@U?$less@V?$SmartPtr@VComponent@WONAPI@@@WONAPI@@@5@V?$allocator@V?$SmartPtr@VComponent@WONAPI@@@WONAPI@@@5@$0A@@std@@IAE?AViterator@12@_NPAU_Node@12@ABV?$SmartPtr@VComponent@WONAPI@@@WONAPI@@@Z)

// FUNCTION: WON 0x007318f0
// Builds the (position, inserted) result of a native image cache insert
// (??0?$pair@Viterator@?$_Tree@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VNativeImage@WONAPI@@@WONAPI@@@2@U_Kfn@?$_Mapk@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VNativeImage@WONAPI@@@WONAPI@@UStringLessNoCase@4@V?$allocator@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VNativeImage@WONAPI@@@WONAPI@@@std@@@2@@2@UStringLessNoCase@WONAPI@@V?$allocator@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VNativeImage@WONAPI@@@WONAPI@@@std@@@2@$0A@@std@@_N@std@@QAE@ABViterator@?$_Tree@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VNativeImage@WONAPI@@@WONAPI@@@2@U_Kfn@?$_Mapk@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VNativeImage@WONAPI@@@WONAPI@@UStringLessNoCase@4@V?$allocator@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VNativeImage@WONAPI@@@WONAPI@@@std@@@2@@2@UStringLessNoCase@WONAPI@@V?$allocator@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VNativeImage@WONAPI@@@WONAPI@@@std@@@2@$0A@@1@AB_N@Z)

// FUNCTION: WON 0x00731a60
// Erases one entry from the named-font map and rebalances the tree; returns the next entry
// (?erase@?$_Tree@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VFont@WONAPI@@@WONAPI@@@2@U_Kfn@?$map@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VFont@WONAPI@@@WONAPI@@UStringLessNoCase@4@V?$allocator@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VFont@WONAPI@@@WONAPI@@@std@@@2@@2@UStringLessNoCase@WONAPI@@V?$allocator@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VFont@WONAPI@@@WONAPI@@@std@@@2@$0A@@std@@QAE?AViterator@12@V312@@Z)

// FUNCTION: WON 0x00731f80
// Frees a subtree of the named-font map recursively, without rebalancing
// (?_Erase@?$_Tree@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VFont@WONAPI@@@WONAPI@@@2@U_Kfn@?$map@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VFont@WONAPI@@@WONAPI@@UStringLessNoCase@4@V?$allocator@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VFont@WONAPI@@@WONAPI@@@std@@@2@@2@UStringLessNoCase@WONAPI@@V?$allocator@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VFont@WONAPI@@@WONAPI@@@std@@@2@$0A@@std@@IAEXPAU_Node@12@@Z)

// FUNCTION: WON 0x00731fc0
// Links a new node into the named-font map tree and rebalances it
// (?_Insert@?$_Tree@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VFont@WONAPI@@@WONAPI@@@2@U_Kfn@?$map@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VFont@WONAPI@@@WONAPI@@UStringLessNoCase@4@V?$allocator@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VFont@WONAPI@@@WONAPI@@@std@@@2@@2@UStringLessNoCase@WONAPI@@V?$allocator@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VFont@WONAPI@@@WONAPI@@@std@@@2@$0A@@std@@IAE?AViterator@12@_NPAU_Node@12@ABU?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VFont@WONAPI@@@WONAPI@@@2@@Z)

// FUNCTION: WON 0x007322e0
// Erases one entry from the native image cache and rebalances the tree; returns the next entry
// (?erase@?$_Tree@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VNativeImage@WONAPI@@@WONAPI@@@2@U_Kfn@?$map@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VNativeImage@WONAPI@@@WONAPI@@UStringLessNoCase@4@V?$allocator@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VNativeImage@WONAPI@@@WONAPI@@@std@@@2@@2@UStringLessNoCase@WONAPI@@V?$allocator@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VNativeImage@WONAPI@@@WONAPI@@@std@@@2@$0A@@std@@QAE?AViterator@12@V312@@Z)

// FUNCTION: WON 0x00732800
// Frees a subtree of the native image cache recursively, without rebalancing
// (?_Erase@?$_Tree@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VNativeImage@WONAPI@@@WONAPI@@@2@U_Kfn@?$map@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VNativeImage@WONAPI@@@WONAPI@@UStringLessNoCase@4@V?$allocator@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VNativeImage@WONAPI@@@WONAPI@@@std@@@2@@2@UStringLessNoCase@WONAPI@@V?$allocator@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VNativeImage@WONAPI@@@WONAPI@@@std@@@2@$0A@@std@@IAEXPAU_Node@12@@Z)

// FUNCTION: WON 0x00732840
// Erases one entry from the raw image cache and rebalances the tree; returns the next entry
// (?erase@?$_Tree@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VRawImage@WONAPI@@@WONAPI@@@2@U_Kfn@?$map@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VRawImage@WONAPI@@@WONAPI@@UStringLessNoCase@4@V?$allocator@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VRawImage@WONAPI@@@WONAPI@@@std@@@2@@2@UStringLessNoCase@WONAPI@@V?$allocator@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VRawImage@WONAPI@@@WONAPI@@@std@@@2@$0A@@std@@QAE?AViterator@12@V312@@Z)

// FUNCTION: WON 0x00732d60
// Frees a subtree of the raw image cache recursively, without rebalancing
// (?_Erase@?$_Tree@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VRawImage@WONAPI@@@WONAPI@@@2@U_Kfn@?$map@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VRawImage@WONAPI@@@WONAPI@@UStringLessNoCase@4@V?$allocator@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VRawImage@WONAPI@@@WONAPI@@@std@@@2@@2@UStringLessNoCase@WONAPI@@V?$allocator@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VRawImage@WONAPI@@@WONAPI@@@std@@@2@$0A@@std@@IAEXPAU_Node@12@@Z)

// FUNCTION: WON 0x00732da0
// Erases one entry from the delay-load image cache and rebalances the tree; returns the next entry
// (?erase@?$_Tree@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VImage@WONAPI@@@WONAPI@@@2@U_Kfn@?$map@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VImage@WONAPI@@@WONAPI@@UStringLessNoCase@4@V?$allocator@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VImage@WONAPI@@@WONAPI@@@std@@@2@@2@UStringLessNoCase@WONAPI@@V?$allocator@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VImage@WONAPI@@@WONAPI@@@std@@@2@$0A@@std@@QAE?AViterator@12@V312@@Z)

// FUNCTION: WON 0x007332c0
// Frees a subtree of the delay-load image cache recursively, without rebalancing
// (?_Erase@?$_Tree@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VImage@WONAPI@@@WONAPI@@@2@U_Kfn@?$map@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VImage@WONAPI@@@WONAPI@@UStringLessNoCase@4@V?$allocator@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VImage@WONAPI@@@WONAPI@@@std@@@2@@2@UStringLessNoCase@WONAPI@@V?$allocator@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VImage@WONAPI@@@WONAPI@@@std@@@2@$0A@@std@@IAEXPAU_Node@12@@Z)

// FUNCTION: WON 0x00733300
// Erases one entry from the window set and rebalances the tree; returns the next entry
// (?erase@?$_Tree@V?$SmartPtr@VWindow@WONAPI@@@WONAPI@@V12@U_Kfn@?$set@V?$SmartPtr@VWindow@WONAPI@@@WONAPI@@UComp@12@V?$allocator@V?$SmartPtr@VWindow@WONAPI@@@WONAPI@@@std@@@std@@UComp@12@V?$allocator@V?$SmartPtr@VWindow@WONAPI@@@WONAPI@@@5@$0A@@std@@QAE?AViterator@12@V312@@Z)

// FUNCTION: WON 0x007337f0
// Frees a subtree of the window set recursively, without rebalancing
// (?_Erase@?$_Tree@V?$SmartPtr@VWindow@WONAPI@@@WONAPI@@V12@U_Kfn@?$_Setk@V?$SmartPtr@VWindow@WONAPI@@@WONAPI@@UComp@12@V?$allocator@V?$SmartPtr@VWindow@WONAPI@@@WONAPI@@@std@@@std@@UComp@12@V?$allocator@V?$SmartPtr@VWindow@WONAPI@@@WONAPI@@@5@$0A@@std@@IAEXPAU_Node@12@@Z)

// FUNCTION: WON 0x00733830
// Grows the window event list's element count, throwing length_error when it would overflow
// (?_Incsize@?$list@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@V?$allocator@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@std@@@std@@IAEXI@Z)

// FUNCTION: WON 0x00733980
// Erases one entry from the timer event set and rebalances the tree; returns the next entry
// (?erase@?$_Tree@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@V12@U_Kfn@?$set@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@U?$less@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@std@@V?$allocator@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@4@@std@@U?$less@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@5@V?$allocator@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@5@$0A@@std@@QAE?AViterator@12@V312@@Z)

// FUNCTION: WON 0x00733e70
// Frees a subtree of the timer event set recursively, without rebalancing
// (?_Erase@?$_Tree@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@V12@U_Kfn@?$_Setk@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@U?$less@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@std@@V?$allocator@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@4@@std@@U?$less@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@5@V?$allocator@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@5@$0A@@std@@IAEXPAU_Node@12@@Z)

// FUNCTION: WON 0x00733eb0
// Erases one entry from the broadcast component set and rebalances the tree; returns the next entry
// (?erase@?$_Tree@V?$SmartPtr@VComponent@WONAPI@@@WONAPI@@V12@U_Kfn@?$set@V?$SmartPtr@VComponent@WONAPI@@@WONAPI@@U?$less@V?$SmartPtr@VComponent@WONAPI@@@WONAPI@@@std@@V?$allocator@V?$SmartPtr@VComponent@WONAPI@@@WONAPI@@@4@@std@@U?$less@V?$SmartPtr@VComponent@WONAPI@@@WONAPI@@@5@V?$allocator@V?$SmartPtr@VComponent@WONAPI@@@WONAPI@@@5@$0A@@std@@QAE?AViterator@12@V312@@Z)

// FUNCTION: WON 0x007343a0
// Returns the first timer event not ordered before the given one
// (?lower_bound@?$_Tree@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@V12@U_Kfn@?$_Setk@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@U?$less@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@std@@V?$allocator@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@4@@std@@U?$less@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@5@V?$allocator@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@5@$0A@@std@@QAE?AViterator@12@ABV?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@Z)

// FUNCTION: WON 0x007343e0
// Returns the first timer event ordered after the given one
// (?upper_bound@?$_Tree@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@V12@U_Kfn@?$_Setk@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@U?$less@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@std@@V?$allocator@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@4@@std@@U?$less@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@5@V?$allocator@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@5@$0A@@std@@QAE?AViterator@12@ABV?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@Z)

// FUNCTION: WON 0x00734420
// Frees a subtree of the broadcast component set recursively, without rebalancing
// (?_Erase@?$_Tree@V?$SmartPtr@VComponent@WONAPI@@@WONAPI@@V12@U_Kfn@?$_Setk@V?$SmartPtr@VComponent@WONAPI@@@WONAPI@@U?$less@V?$SmartPtr@VComponent@WONAPI@@@WONAPI@@@std@@V?$allocator@V?$SmartPtr@VComponent@WONAPI@@@WONAPI@@@4@@std@@U?$less@V?$SmartPtr@VComponent@WONAPI@@@WONAPI@@@5@V?$allocator@V?$SmartPtr@VComponent@WONAPI@@@WONAPI@@@5@$0A@@std@@IAEXPAU_Node@12@@Z)

// FUNCTION: WON 0x00734f70
// Advances a named-font map iterator to the next entry
// (?_Inc@const_iterator@?$_Tree@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VFont@WONAPI@@@WONAPI@@@2@U_Kfn@?$map@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VFont@WONAPI@@@WONAPI@@UStringLessNoCase@4@V?$allocator@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VFont@WONAPI@@@WONAPI@@@std@@@2@@2@UStringLessNoCase@WONAPI@@V?$allocator@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VFont@WONAPI@@@WONAPI@@@std@@@2@$0A@@std@@QAEXXZ)

// FUNCTION: WON 0x007352a0
// Deleting destructor of a named-font map entry
// (??_G?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VFont@WONAPI@@@WONAPI@@@std@@QAEPAXI@Z)

// FUNCTION: WON 0x007352c0
// Deleting destructor of a native image cache entry
// (??_G?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VNativeImage@WONAPI@@@WONAPI@@@std@@QAEPAXI@Z)

// FUNCTION: WON 0x007352e0
// Deleting destructor of a raw image cache entry
// (??_G?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VRawImage@WONAPI@@@WONAPI@@@std@@QAEPAXI@Z)

// FUNCTION: WON 0x00735300
// Deleting destructor of a delay-load image cache entry
// (??_G?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VImage@WONAPI@@@WONAPI@@@std@@QAEPAXI@Z)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0072fab0
// (public: class WONAPI::SmartPtr<class WONAPI::Font> & __thiscall std::map<class std::basic_string<char,struct std::char_traits<char>,class std::allocat)
// TEMPLATE: WON 0x00735010
// (void __cdecl std::_Construct(struct std::pair<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> > const ,class WON)
// TEMPLATE: WON 0x00735090
// (void __cdecl std::_Construct(struct std::pair<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> > const ,class WON)
// TEMPLATE: WON 0x00735110
// (void __cdecl std::_Construct(struct std::pair<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> > const ,class WON)
// TEMPLATE: WON 0x00735220
// (void __cdecl std::_Construct(struct std::pair<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> > const ,class WON)
