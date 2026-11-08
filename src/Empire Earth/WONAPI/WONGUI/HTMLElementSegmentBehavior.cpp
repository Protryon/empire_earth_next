// Horizontal rule, bullet and ordered-list number segments: width, painting and selection text (file name inferred)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyTextSegment.h"
#include "ee/wonapi/WONLobbyHTMLElementSegments.h"
namespace WONAPI {
// FUNCTION: WON 0x00709c10
// Sizes the rule to a percentage (negative spec), fixed width or the full width, never wider than available
void HorizontalRuleSegment::SetMaxWidth(int width)
{
    if(this->mWidthSpec<0)
        this->mWidth=(width*-this->mWidthSpec)/100;
    else if(this->mWidthSpec>0)
        this->mWidth=this->mWidthSpec;
    else
        this->mWidth=width;
    if(this->mWidth>width)
        this->mWidth=width;
}

// FUNCTION: WON 0x00709c50
// Paints the rule 7 pixels down: a solid gray bar for NOSHADE, else a sunken two-tone outline
void HorizontalRuleSegment::Paint(Graphics& graphics,int x,int y,ElementPaintInfo& info)
{
    this->DrawSelection1(graphics,x,y,info);
    graphics.SetColor(0x808080);
    if(this->mNoShade)
        graphics.FillRect(x,y+7,this->mWidth,this->mHeight);
    else {
        graphics.SetColor(0x848484);
        graphics.DrawLine(x,y+7,x+this->mWidth-1,y+7);
        graphics.DrawLine(x,y+7,x,y+7+this->mHeight-1);
        graphics.SetColor(0xd6d6d6);
        graphics.DrawLine(x+this->mWidth-1,y+7,x+this->mWidth-1,y+7+this->mHeight-1);
        graphics.DrawLine(x,y+7+this->mHeight-1,x+this->mWidth-1,y+7+this->mHeight-1);
    }
}

// FUNCTION: WON 0x00709d20
// Appends a line of dashes to the copied text when the rule lies inside the selection
void HorizontalRuleSegment::GetSelText(GUIString& text,ElementSelectionInfo& info)
{
    if(this->mStartSelIndex<info.mStartSelSegment || this->mStartSelIndex>info.mEndSelSegment)
        return;
    text.append("-------------------------------------------------------------------------------\r\n");
}

// FUNCTION: WON 0x0070b470
// Paints a list bullet (disc, circle or square) left of the item, or at the position for a negative type
void BulletElement::Paint(Graphics& graphics,int x,int y,ElementPaintInfo&)
{
    int drawY=y+1;
    int drawX=x;
    int type;
    if(this->mType>=0) {
        drawX-=this->mWidth+10;
        type=this->mType;
    }
    else
        type=-this->mType;
    graphics.SetColor(this->mColor);
    switch(type) {
    case 1: graphics.FillEllipse(drawX,drawY,this->mWidth,this->mHeight); break;
    case 2: graphics.DrawEllipse(drawX,drawY,this->mWidth+2,this->mHeight+2); break;
    case 3: graphics.FillRect(drawX,drawY,this->mWidth,this->mHeight); break;
    }
}

// FUNCTION: WON 0x0070b510
// Draws the item's number text right-aligned up to 40 pixels left of the item
void OrderedListElement::Paint(Graphics& graphics,int x,int y,ElementPaintInfo&)
{
    graphics.SetColor(this->mColor);
    graphics.SetFont(this->mFont.get());
    int backWidth=this->mWidth+10;
    if(backWidth>40)
        backWidth=40;
    graphics.DrawString(this->mText,x-backWidth,y);
}
}
