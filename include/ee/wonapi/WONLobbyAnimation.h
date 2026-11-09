// Animation (SDK Animation.h): constructors at 0x00735340 / 0x00735540, with RTTI.
#ifndef EE_WONAPI_WONLOBBYANIMATION_H
#define EE_WONAPI_WONLOBBYANIMATION_H
#include <list>
namespace WONAPI {
class AnimationFrame : public RefCount {
public:
    enum DisposalMethod { DisposalMethod_None, DisposalMethod_Leave, DisposalMethod_RestorBGColor, DisposalMethod_RestorePrevious, DisposalMethod_Max };
    SmartPtr<Image> mImage; //08
    int mDelayTime; //0c
    int mLeft, mTop; //10,14
    DisposalMethod mDisposalMethod; // offset 0x18; class size 0x1c
    AnimationFrame(Image* = NULL,int = 0);
    void Draw(Graphics&,int,int);
    SmartPtr<AnimationFrame> Duplicate();
};
typedef std::list<SmartPtr<AnimationFrame> > AnimationFrameList;
class Animation : public RefCount {
private:
    AnimationFrameList mFrameList; //08..13
    AnimationFrameList::iterator mFrameItr; //14
    int mNumLoops,mLoopCount; //18,1c
    bool mNeedAccumulate,mCopyFirstImageToAccumulate; //20,21
    SmartPtr<NativeImage> mAccumulateImage; //24
    int mWidth,mHeight; // offsets 0x28, 0x2c; class size 0x30
public:
    Animation(const AnimationFrameList&,int = -1,int = -1);
    void RealizeNative(DisplayContext*);
    void Rewind(bool = true);
    bool NextFrame();
    int GetFrameDelayTime();
    int GetNumFrames() { return (int)this->mFrameList.size(); } // inline (frame list size, read at 0x007488af)
    void Draw(Graphics&,int,int);
    SmartPtr<Animation> Duplicate();
    int GetWidth() { return this->mWidth; } // inline getter (used at 0x00706e78)
    int GetHeight() { return this->mHeight; } // inline getter
    void SetNumLoops(int n) { this->mNumLoops=n; }
    void SetNeedAccumulate(bool b) { this->mNeedAccumulate=b; }
    void SetCopyFirstImageToAccumulate(bool b) { this->mCopyFirstImageToAccumulate=b; }
};
}
#endif
