#ifndef EE_WONAPI_WONLOBBYHTMLELEMENTSEGMENTS_H
#define EE_WONAPI_WONLOBBYHTMLELEMENTSEGMENTS_H
#include "ee/wonapi/WONLobbyLineLayout.h"
namespace WONAPI {
// HorizontalRuleSegment (0x24 bytes, primitives only; allocated at 0x00739d9b); the implicit
// destructor is base-only (0x00711480, which calls 0x00738c70).
class HorizontalRuleSegment : public LineSegment {
public:
    int mWidthSpec,mWidth,mHeight; //14,18,1c
    bool mNoShade; //20; natural21..23
    HorizontalRuleSegment():mWidthSpec(0),mWidth(0),mHeight(1),mNoShade(false) {}
    virtual int GetAscent() { return this->GetHeight()/2; } // body at 0x00739e50 (calls the virtual GetHeight)
    virtual int GetHeight() { return this->mHeight+14; } //339e60
    virtual int GetWidth() { return this->mWidth; } //339e70
    virtual void SetMaxWidth(int); //309c10
    virtual void Paint(Graphics&,int,int,ElementPaintInfo&); //309c50
    virtual void GetSelText(GUIString&,ElementSelectionInfo&); //309d20
};
// BulletElement (0x24 bytes, primitives only; allocated at 0x0073ad7b and 0x0073adc2).
class BulletElement : public LineSegment {
public:
    int mWidth,mHeight,mColor,mType; //14,18,1c,20
    BulletElement(int color=0,int type=-1,int width=5,int height=5)
        :mWidth(width),mHeight(height),mColor(color),mType(type) {}
    virtual int GetWidth() { return this->mType>=0?0:this->mWidth+10; } //33aec0
    virtual int GetHeight() { return this->mHeight+2; } //33aee0
    virtual void Paint(Graphics&,int,int,ElementPaintInfo&); //30b470
};
// OrderedListElement (0x24 bytes; allocated at 0x0073adce, constructor inline). The implicit
// destructor (0x0073af30) releases the GUIString at +0x20, then the font at +0x14, then the base.
class OrderedListElement : public LineSegment {
public:
    SmartPtr<Font> mFont; //14
    int mColor,mWidth; //18,1c
    GUIString mText; //20
    OrderedListElement(int color,Font* font,const GUIString& text)
        :mFont(font),mColor(color),mText(text)
    {
        this->mWidth=font->GetStringWidth(this->mText);
    }
    virtual int GetWidth() { return 0; } // folded into the identical body at 0x006c7b10
    virtual int GetHeight() { return this->mFont->GetHeight(); } //33aef0
    virtual int GetAscent() { return this->mFont->GetAscent(); } //33af00
    virtual void Paint(Graphics&,int,int,ElementPaintInfo&); //30b510
};
}
#endif
