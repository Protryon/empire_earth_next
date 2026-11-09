#ifndef EE_WONAPI_WONLOBBYHTMLIMAGESEGMENTS_H
#define EE_WONAPI_WONLOBBYHTMLIMAGESEGMENTS_H
#include "ee/wonapi/WONLobbyLineLayout.h"
namespace WONAPI {
// ComponentLineSegment (0x18 bytes; allocated at 0x0073a5ca, inline constructor stores at
// 0x0073a5de). The implicit destructor (0x0073a9d0) releases the component at +0x14.
class ComponentLineSegment : public LineSegment {
public:
    SmartPtr<Component> mComponent; //14
    ComponentLineSegment(Component* component):mComponent(component) {}
    virtual int GetWidth();
    virtual int GetHeight();
    virtual void MouseEnter(int,int);
    virtual void MouseExit();
    virtual void MouseDown(int,int,MouseButton);
    virtual void MouseUp(int,int,MouseButton);
    virtual void MouseMove(int,int);
    virtual void MouseDrag(int,int);
    virtual bool IsInvalid();
    virtual bool CanStartSelection();
    virtual bool AllowModalInput();
    virtual void Paint(Graphics&,int,int,ElementPaintInfo&);
    virtual void SetViewport(ScrollAreaViewport*,int,int);
};
// ImageLineSegment (0x2c bytes): constructor at 0x0073aa30, implicit destructor at 0x0073aaf0.
class ImageLineSegment : public LineSegment {
public:
    SmartPtr<Image> mImage; //14
    int mHSpace,mVSpace,mBorder,mBorderColor; //18,1c,20,24
    char mAlign; //28; natural29..2b
    ImageLineSegment(Image* image=NULL) : mImage(image),mHSpace(5),mVSpace(0),mBorder(0),mBorderColor(0),mAlign(0) {}
    virtual int GetWidth(); //33aa80
    virtual int GetHeight(); //33aaa0
    virtual char GetVertAlign(); //33aac0
    virtual void Paint(Graphics&,int,int,ElementPaintInfo&); //30b390
};
// ScaledImageLineSegment: two int fields over ImageLineSegment (not identified in the original exe).
class ScaledImageLineSegment : public ImageLineSegment
{
public:
	int mWidth, mHeight;

public:
	ScaledImageLineSegment(Image *theImage, int theWidth, int theHeight) : ImageLineSegment(theImage), mWidth(theWidth), mHeight(theHeight) { }

	virtual int GetWidth() { return this->mWidth + 2*this->mHSpace; }
	virtual int GetHeight() { return this->mHeight + 2*this->mVSpace; }
	virtual void Paint(Graphics &g, int x, int y, ElementPaintInfo &theInfo);
};
typedef SmartPtr<ScaledImageLineSegment> ScaledImageLineSegmentPtr;
// 0x40 bytes; constructor inlined at 0x0073a690 and 0x0073a743. The implicit destructor (0x0073ab70)
// releases +0x3c, +0x38, +0x34, then the inherited +0x14, in that order.
class ImageLinkLineSegment : public ImageLineSegment {
public:
    DWORD mNormalColor,mDownColor; //2c,30
    SmartPtr<TextLinkParam> mCurLinkParam,mAnchorLinkParam; //34,38
    SmartPtr<ImageMap> mImageMap; //3c
    ImageLinkLineSegment(TextLinkParam* param,int color,int down,Image* image)
        :ImageLineSegment(image),mNormalColor(color),mDownColor(down),
         mCurLinkParam(param),mAnchorLinkParam(param)
    {
        if(param!=NULL) param->mLinkList.push_back(this); // inline AddLink
    }
    virtual void LinkDown();
    virtual void LinkUp();
    virtual void MouseEnter(int,int);
    virtual void MouseMove(int,int);
    virtual void MouseDown(int,int,MouseButton);
    virtual void MouseUp(int,int,MouseButton);
    virtual void MouseExit();
    virtual bool CanStartSelection();
};
}
#endif
