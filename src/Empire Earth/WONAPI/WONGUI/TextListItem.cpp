// Text list items: size, sort order and painting (file name inferred)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
namespace WONAPI {
// FUNCTION: WON 0x6fa2c0
// Sizes the item from its text and optional image, taking the list's font and text colour when unset
void TextListItem::CalcDimensions(ListArea* inList)
{
    if(this->mFont.get()==NULL) this->mFont=inList->mFont;
    if(this->mColor==-1) this->mColor=inList->GetTextColor();
    this->mWidth=this->mFont->GetStringWidth(this->mText)+2;
    this->mHeight=this->mFont->GetHeight();
    if(this->mImage.get()!=NULL) {
        this->mWidth+=this->mImage->mWidth+2;
        if(this->mImage->mHeight>this->mHeight) this->mHeight=this->mImage->mHeight;
    }
}
// FUNCTION: WON 0x006fa360
// Orders two text items by their text, ignoring case
int TextListItem::Compare(const ListItem* inOther,ListArea* inList) const
{
    return this->mText.CompareNoCase(static_cast<const TextListItem*>(inOther)->mText);
}
// FUNCTION: WON 0x6fa380
// Paints the item's image and vertically centred text, with the selection background behind a
// selected item
void TextListItem::Paint(Graphics& inGraphics,ListArea* inList)
{
    unsigned long aColor=this->mColor;
    if(this->mSelected) {
        const SelectionColor& aSelection=inList->GetSelColor();
        inGraphics.SetColor(aSelection.GetBackgroundColor(inGraphics));
        int aWidth=inList->mPaintWidth;
        if(!inList->ListFlagSet(0x10)) aWidth=this->mWidth;
        int aX=0;
        Image* anImage=this->mImage.get();
        if(anImage!=NULL && !inList->ListFlagSet(0x20)) {
            int anImageWidth=anImage->mWidth;
            aX=anImageWidth+2;
            aWidth-=anImageWidth+2;
        }
        inGraphics.FillRect(aX,0,aWidth,this->mHeight);
        aColor=aSelection.GetForegroundColor(inGraphics,this->mColor);
    }
    inGraphics.SetColor(aColor);
    inGraphics.SetFont(this->mFont.get());
    int aTextX=2;
    Image* anImage=this->mImage.get();
    if(anImage!=NULL) {
        int anImageY=(this->mHeight-anImage->mHeight)/2;
        if(anImageY<0) anImageY=0;
        inGraphics.DrawImage(anImage,2,anImageY);
        aTextX=this->mImage->mWidth+4;
    }
    inGraphics.DrawString(this->mText,aTextX,(this->mHeight-this->mFont->GetHeight())/2);
}
}