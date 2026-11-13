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
#include "ee/wonapi/WONGUIDraw.h"
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
    // the selection behind a selected item, the image, the text centred on the row (guidraw.rs text_row)
    Image* anImage=this->mImage.get();
    EeWonTextRowPaint r;
    r.selected=this->mSelected;
    r.full_row_select=inList->ListFlagSet(0x10);
    r.no_image_select=inList->ListFlagSet(0x20);
    r.has_image=anImage!=NULL;
    r.sel=WONDrawSelOf(inList->GetSelColor());
    r.color=(uint32_t)this->mColor;
    r.paint_width=inList->GetPaintWidth();
    r.width=this->mWidth;
    r.height=this->mHeight;
    r.image_width=anImage ? anImage->mWidth : 0;
    r.image_height=anImage ? anImage->mHeight : 0;
    r.font_height=this->mFont->GetHeight();
    WONDrawSlots aSlots;
    aSlots.mImages[0]=anImage;
    aSlots.mFont=this->mFont.get();
    aSlots.mString=&this->mText;
    WONDrawOps anOps([&](EeWonDrawOp* o,size_t n) { return ee_won_gui_draw_text_row(&r,o,n); });
    WONDrawRun(inGraphics,anOps,aSlots);
}
}