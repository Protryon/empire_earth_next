// Animation frames and frame-list animations with an optional accumulation image
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
#include "ee/wonapi/WONLobbyAnimation.h"

using namespace WONAPI;
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x00735340
// Creates an animation frame showing an image for a delay, at offset 0,0 with no disposal method
AnimationFrame::AnimationFrame(Image *theImage, int theDelayTime)
{
	this->mImage = theImage;
	this->mDelayTime = theDelayTime;
	this->mDisposalMethod = DisposalMethod_None;
	this->mLeft = this->mTop = 0;
}

// FUNCTION: WON 0x007353e0
// Deleting destructor of an animation frame (compiler-generated)
// (??_GAnimationFrame@WONAPI@@UAEPAXI@Z)

// FUNCTION: WON 0x00735400
// Destroys an animation frame, releasing its image
// (??1AnimationFrame@WONAPI@@UAE@XZ)

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x00735460
// Returns a new frame sharing this frame's image, with the same delay, disposal method and offset
SmartPtr<AnimationFrame> AnimationFrame::Duplicate()
{
	AnimationFrame *aFrame = new AnimationFrame(this->mImage.get(),this->mDelayTime);
	aFrame->mDisposalMethod = this->mDisposalMethod;
	aFrame->mLeft = this->mLeft;
	aFrame->mTop = this->mTop;
	return aFrame;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x00735510
// Draws the frame's image at the given position plus the frame's offset
void AnimationFrame::Draw(Graphics &g, int x, int y)
{
	g.DrawImage(this->mImage.get(),x+this->mLeft,y+this->mTop);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x00735540
// Creates an animation from a frame list; a size of -1 takes the first frame's image size (0x0 when empty)
Animation::Animation(const AnimationFrameList &theList,  int theWidth, int theHeight) : mFrameList(theList)
{
	if(theWidth==-1 || theHeight==-1)
	{
		if(!theList.empty())
		{
			Image *anImage = theList.front()->mImage.get();
			this->mWidth = anImage->mWidth;
			this->mHeight = anImage->mHeight;
		}
		else
			this->mWidth = this->mHeight = 0;
	}
	else
	{
		this->mWidth = theWidth;
		this->mHeight = theHeight;
	}

	this->mFrameItr = this->mFrameList.begin();
	this->mNumLoops = 0;
	this->mLoopCount = 0;

	this->mNeedAccumulate = false;
	this->mCopyFirstImageToAccumulate = false;
}

// FUNCTION: WON 0x00735660
// Deleting destructor of an animation (compiler-generated)
// (??_GAnimation@WONAPI@@UAEPAXI@Z)

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x00735730
// Converts raw frame images to native images for the display and creates the accumulation image when needed
void Animation::RealizeNative(DisplayContext *theContext)
{
	if(this->mFrameList.empty())
		return;

	if(dynamic_cast<RawImage*>(this->mFrameList.front()->mImage.get())!=NULL)
	{
		for(AnimationFrameList::iterator anItr = this->mFrameList.begin(); anItr!=this->mFrameList.end(); ++anItr)
		{
			AnimationFrame *aFrame = anItr->get();
			aFrame->mImage = ((RawImage*)aFrame->mImage.get())->GetNative(theContext).get();
		}
	}

	if(this->mNeedAccumulate && !this->mCopyFirstImageToAccumulate && this->mAccumulateImage.get()==NULL)
		this->mAccumulateImage = theContext->CreateImage(this->mWidth,this->mHeight);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x007358f0
// Returns to the first frame (optionally resetting the loop count) and redraws or reseeds the accumulation image
void Animation::Rewind(bool resetLoopCount)
{
	if(resetLoopCount)
		this->mLoopCount = 0;

	this->mFrameItr = this->mFrameList.begin();

	if(this->mCopyFirstImageToAccumulate && !this->mFrameList.empty())
		this->mAccumulateImage = (NativeImage*)this->mFrameList.front()->mImage->Duplicate().get();
	else if(this->mAccumulateImage.get()!=NULL)
	{
		AnimationFrame *aFrame = this->mFrameItr->get();
		aFrame->Draw(this->mAccumulateImage->GetGraphics(),0,0);
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x00735a10
// Advances to the next frame, rewinding at the end; returns false once the loop limit is reached
bool Animation::NextFrame()
{
	if(this->mFrameList.size()==1 || ++this->mFrameItr==this->mFrameList.end())
	{
		if(this->mNumLoops>0 && ++this->mLoopCount>=this->mNumLoops)
			return false;
		else
			this->Rewind(false);
	}

	if(this->mAccumulateImage.get()!=NULL)
	{
		AnimationFrame *aFrame = this->mFrameItr->get();
		aFrame->Draw(this->mAccumulateImage->GetGraphics(),0,0);
	}	

	return true;
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x00735a70
// Returns the current frame's delay, or 0 past the last frame
int Animation::GetFrameDelayTime()
{
	if(this->mFrameItr==this->mFrameList.end())
		return 0;

	AnimationFrame *aFrame = this->mFrameItr->get();
	return aFrame->mDelayTime;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x00735a90
// Draws the accumulation image if there is one, else the current frame
void Animation::Draw(Graphics &g, int x, int y)
{
	if(this->mAccumulateImage.get()!=NULL)
		g.DrawImage(this->mAccumulateImage.get(),x,y);
	else
	{
		if(this->mFrameItr!=this->mFrameList.end())
		{
			AnimationFrame *aFrame = this->mFrameItr->get();
			aFrame->Draw(g,x,y);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x00735ae0
// Returns a deep copy of the animation: duplicated frames, loop state and accumulation image
SmartPtr<Animation> Animation::Duplicate()
{
	SmartPtr<Animation> anAnim = new Animation(this->mFrameList,this->mWidth,this->mHeight);
	for(AnimationFrameList::iterator anItr = anAnim->mFrameList.begin(); anItr!=anAnim->mFrameList.end(); ++anItr)
		*anItr = (*anItr)->Duplicate();

	anAnim->mNumLoops = this->mNumLoops;
	anAnim->mLoopCount = this->mLoopCount;
	anAnim->mNeedAccumulate = this->mNeedAccumulate;
	anAnim->mCopyFirstImageToAccumulate = this->mCopyFirstImageToAccumulate;
	if(this->mAccumulateImage.get()!=NULL)
		anAnim->mAccumulateImage = (NativeImage*)this->mAccumulateImage->Duplicate().get();

	return anAnim;
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00735680
// (public: virtual __thiscall WONAPI::Animation::~Animation(void))
// TEMPLATE: WON 0x007542b0
// (public: void __thiscall std::list<class WONAPI::SmartPtr<class WONAPI::AnimationFrame>,class std::allocator<class WONAPI::SmartPtr<class WONAPI::Anima)
// TEMPLATE: WON 0x00755600
// (protected: void __thiscall std::list<class WONAPI::SmartPtr<class WONAPI::AnimationFrame>,class std::allocator<class WONAPI::SmartPtr<class WONAPI::An)
