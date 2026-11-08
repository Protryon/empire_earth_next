// TextLinkLineSegment (0x34 bytes); its inline constructor is used by the factory at 0x00739540.
#ifndef EE_WONAPI_WONLOBBYTEXTLINKSEGMENT_H
#define EE_WONAPI_WONLOBBYTEXTLINKSEGMENT_H
#include <ctype.h>
#include "ee/wonapi/WONLobbyTextSegment.h"
namespace WONAPI {
class TextLinkLineSegment : public TextLineSegment {
public:
    DWORD mNormalColor,mDownColor; // offsets 0x28, 0x2c: normal, then down
    SmartPtr<TextLinkParam> mLinkParam; // offset 0x30, retained; class size 0x34
    TextLinkLineSegment(TextLinkParam* param,const GUIString& text,int color,int downColor,Font* font)
        :TextLineSegment(text,color,font),mNormalColor(color),mDownColor(downColor),mLinkParam(param)
    {
        this->mLinkParam->mLinkList.push_back(this); // inline AddLink
        if(this->mText.length()==1 && isspace(this->mText[0])) this->mText.erase(); // isspace on the raw char, unguarded (as in the original)
    }
    virtual void MouseEnter(int,int); //28=30ac00
    virtual void MouseExit(); //2c=30ae40
    virtual void MouseDown(int,int,MouseButton); //30=30ad70
    virtual void MouseUp(int,int,MouseButton); //34=30ad90
    virtual void LinkDown(); //40=30abc0
    virtual void LinkUp(); //44=30abe0
    virtual bool CanStartSelection() { return false; } // slot 0x48 (shared body at 0x0070a7b0)
    // No Paint override: inherits TextLineSegment's (0x0070a900).
};
}
#endif
