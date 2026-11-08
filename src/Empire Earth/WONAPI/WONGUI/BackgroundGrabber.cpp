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

using namespace WONAPI;

unsigned __int64 BackgroundGrabber::mImageOrderCounter = 0;

// FUNCTION: WON 0x00736000
// Creates a grabber with no captured background and a half-strength translucent tint
BackgroundGrabber::BackgroundGrabber()
{
    this->mImageX = this->mImageY = 0;
    this->mNeedPutBG = false;
    this->mHaveBG = false;
    this->mTranslucentColor = 0;
    this->mTranslucentLevel = 4;
}

// FUNCTION: WON 0x00736060
// Forgets the captured background
void BackgroundGrabber::Clear()
{
    this->mHaveBG = this->mNeedPutBG = false;
}

// FUNCTION: WON 0x00736070
// Restores the captured background under the component when a repaint is pending, then forgets it
void BackgroundGrabber::PrePaint(Component* theComponent, Graphics& g)
{
    if(!this->mHaveBG || !this->mNeedPutBG)
        return;
    int x = 0, y = 0;
    theComponent->LocalToRoot(x,y);
    g.DrawImage(this->mBGImage.get(),this->mImageX-x,this->mImageY-y);
    this->mHaveBG = false;
    this->mNeedPutBG = false;
}

// FUNCTION: WON 0x007360d0
// Sets the tint colour and its strength in eighths
void BackgroundGrabber::SetTranslucentColor(int theColor,int theLevel)
{
    this->mTranslucentLevel = theLevel;
    this->mTranslucentColor = theColor;
}

#define COLOR_1_8(c) (((c)>>3)&0xff1f1f1f)
#define COLOR_2_8(c) (((c)>>2)&0xff3f3f3f)
#define COLOR_3_8(c) (COLOR_1_8(c)+COLOR_2_8(c))
#define COLOR_4_8(c) (((c)>>1)&0xff7f7f7f)
#define COLOR_5_8(c) (COLOR_4_8(c)+COLOR_1_8(c))
#define COLOR_6_8(c) (COLOR_4_8(c)+COLOR_2_8(c))
#define COLOR_7_8(c) (COLOR_4_8(c)+COLOR_3_8(c))
//#define COLOR_7_8(c) ((c)-COLOR_1_8(c)) // doesn't work right because it rounds up

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x007365b0
// Returns a colour scaled to n/8 per channel (0 outside 1..7)
static int GetColor8th(int theColor, int theNumerator)
{
	switch(theNumerator)
	{
		case 1: return COLOR_1_8(theColor);
		case 2: return COLOR_2_8(theColor);
		case 3: return COLOR_3_8(theColor);
		case 4: return COLOR_4_8(theColor);
		case 5: return COLOR_5_8(theColor);
		case 6: return COLOR_6_8(theColor);
		case 7: return COLOR_7_8(theColor);
	}

	return 0;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x007360f0
// Captures the screen under the component once and, for translucent components, tints it (blended fill or per-pixel)
void BackgroundGrabber::Paint(Component *theComponent, Graphics &g)
{
	if(this->mHaveBG)
		return;
	this->mClipRect = g.mClipRect;
	this->mImageOrder = BackgroundGrabber::mImageOrderCounter++;
	int aWidth = theComponent->Width();
	int aHeight = theComponent->Height();
	if(this->mBGImage.get()==NULL || aWidth!=this->mBGImage->GetWidth() || aHeight!=this->mBGImage->GetHeight())
	{
		Window *aWindow = theComponent->GetRoot();
		this->mBGImage = aWindow->CreateImage(aWidth, aHeight);

		if(theComponent->ComponentFlagSet(ComponentFlag_DebugBG) && this->mDebugWindow.get()==NULL)
		{
			this->mDebugWindow = new MSWindow;
			this->mDebugWindow->SetParent(theComponent->GetRoot());
			this->mDebugWindow->mStyle=0x40;
			WONRectangle aRect(0,0,this->mBGImage->GetWidth(),this->mBGImage->GetHeight());
			this->mDebugWindow->Create(aRect);
		}

		if(this->mDebugWindow.get()!=NULL)
		{
			this->mDebugWindow->mRoot.get()->RemoveAllChildren();
			this->mDebugWindow->AddComponent(new ImageComponent(this->mBGImage.get()));
			int x=0,y=0;
			theComponent->LocalToRoot(x,y);
			WONRectangle aScreenRect;
			theComponent->GetRoot()->GetRect(aScreenRect,false);
			this->mDebugWindow->SetPosSize(x+aScreenRect.x,y+aScreenRect.y,-1,-1);
		}

	}

	NativeImage *anImage = theComponent->GetRoot()->GetOffscreenImage();
	int x = 0, y = 0;
	theComponent->RootToChild(x,y);
	this->mBGImage->GetGraphics().DrawImage(anImage, x, y);
	this->mImageX = 0;//theComponent->Left();
	this->mImageY = 0;//theComponent->Top();
	theComponent->LocalToRoot(this->mImageX,this->mImageY);

	if(this->mDebugWindow.get()!=NULL)
		this->mDebugWindow->mRoot.get()->Invalidate();

	this->mHaveBG = true;

	if(theComponent->ComponentFlagSet(ComponentFlag_Translucent))
	{

		int aColor = GetColor8th(this->mTranslucentColor,8-this->mTranslucentLevel);
		if(g.BlendSupported())
		{
			g.SetColor(this->mTranslucentColor);
			g.FillRectBlend(0,0,theComponent->Width(),theComponent->Height(),(8-this->mTranslucentLevel)*255/8);
			return;
		}

		SmartPtr<RawImage32> aRawImage = this->mBGImage->GetRaw();
		int aNumPixels = aRawImage->GetWidth()*aRawImage->GetHeight();
		DWORD *aPixels =aRawImage->mImageData;
		DWORD *anEndPixels = aPixels+aNumPixels;
		switch(this->mTranslucentLevel)
		{
		case 1:
			do
			{
				*aPixels = COLOR_1_8(*aPixels) + aColor;
			} while(++aPixels!=anEndPixels);
			break;

		case 2:
			do
			{
				*aPixels = COLOR_2_8(*aPixels) + aColor;
			} while(++aPixels!=anEndPixels);
			break;

		case 3:
			do
			{
				*aPixels = COLOR_3_8(*aPixels) + aColor;
			} while(++aPixels!=anEndPixels);
			break;

		case 4:
			do
			{
				*aPixels = COLOR_4_8(*aPixels) + aColor;
			} while(++aPixels!=anEndPixels);
			break;

		case 5:
			do
			{
				*aPixels = COLOR_5_8(*aPixels) + aColor;
			} while(++aPixels!=anEndPixels);
			break;

		case 6:
			do
			{
				*aPixels = COLOR_6_8(*aPixels) + aColor;
			} while(++aPixels!=anEndPixels);
			break;

		case 7:
			do
			{
				*aPixels = COLOR_7_8(*aPixels) + aColor;
			} while(++aPixels!=anEndPixels);
			break;

		}
		g.DrawImage(aRawImage.get(),0,0);
	}
}

///////////////////////////////////////////////////////////////////////////////


namespace WONAPI {
// FUNCTION: WON 0x00736680
// Marks a captured background to be restored before the next paint
void BackgroundGrabber::Invalidate(Component *theComponent)
{
	if(this->mHaveBG)
		this->mNeedPutBG = true;
}
}

// Leaves the captured background as it is (empty; the linker folds this body into the identical one at
// 0x006fa500)
void BackgroundGrabber::SizeChanged(Component* theComponent)
{
}
