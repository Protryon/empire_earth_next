// Resource configuration of list boxes: list flags, text items, font, colours, sort type (file name inferred)
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
#include "ee/wonapi/WONLobbyResourceConfig.h"
namespace WONAPI {

// FUNCTION: WON 0x0078d5d0
// Reads a list sort type (ASCENDING, DESCENDING or NONE); throws on any other name
ListSortType ListAreaConfig::ReadSortType()
{
    GUIString name=this->ReadString(false);
    if(name.CompareNoCase("ASCENDING")==0) return ListSortType_Ascending;
    if(name.CompareNoCase("DESCENDING")==0) return ListSortType_Descending;
    if(name.CompareNoCase("NONE")==0) return ListSortType_None;
    throw ConfigObjectException("Invalid list sort type.");
}

// FUNCTION: WON 0x0078d790
// Reads a list of named list flags (each optionally negated) and sets or clears them on the list box;
// throws on an unknown name
void ListAreaConfig::ReadListFlags(ComponentConfig* inConfig,ListArea* inArea)
{
    while(true)
    {
        bool on=true;
        GUIString name=inConfig->ReadFlag(&on);
        if(name.length()==0) break;
        unsigned long flags;
        if(name.CompareNoCase("SelOver")==0) flags=2;
        else if(name.CompareNoCase("SelOnlyWhenOver")==0) flags=4;
        else if(name.CompareNoCase("ToggleSel")==0) flags=8;
        else if(name.CompareNoCase("FullRowSelect")==0) flags=0x10;
        else if(name.CompareNoCase("HilightIcons")==0) flags=0x20;
        else if(name.CompareNoCase("ScrollFullLine")==0) flags=0x40;
        else if(name.CompareNoCase("ShowPartialLines")==0) flags=0x80;
        else if(name.CompareNoCase("ClickEventOnMouseUp")==0) flags=0x100;
        else if(name.CompareNoCase("ItemWidthCheckOnSel")==0) flags=0x200;
        else if(name.CompareNoCase("MultiSelect")==0) flags=0x400;
        else if(name.CompareNoCase("KeyMoveWrapAround")==0) flags=0x800;
        else if(name.CompareNoCase("NoAutoAdjustScrollPos")==0) flags=0x1000;
        else if(name.CompareNoCase("DoListItemToolTip")==0) flags=0x2000;
        else throw ConfigObjectException("Unknown list flag: "+(std::string)name);
        inArea->SetListFlags(flags,on);
    }
}

// FUNCTION: WON 0x0078da70
// Handles the LISTFLAGS, TEXTITEM, FONT, TEXTCOLOR, SELCOLOR and LISTSORTTYPE instructions; others go to
// the scroll area config
bool ListAreaConfig::Virtual24(const std::string& inInstruction)
{
    if(inInstruction=="LISTFLAGS") ListAreaConfig::ReadListFlags(this,this->mListArea);
    else if(inInstruction=="TEXTITEM")
    {
        while(!this->EndOfString())
        {
            this->mListArea->InsertString(this->ReadString(false),-1);
            if(!this->EndOfString()) this->EnsureComma();
        }
    }
    else if(inInstruction=="FONT") this->mListArea->SetFont(this->ReadFont());
    else if(inInstruction=="TEXTCOLOR") this->mListArea->mTextColor=this->ReadColor();
    else if(inInstruction=="SELCOLOR") this->mListArea->mSelColor=this->ReadSelectionColor();
    else if(inInstruction=="LISTSORTTYPE") this->mListArea->SetSortType(this->ReadSortType());
    else return ScrollAreaConfig::Virtual24(inInstruction);
    return true;
}

}
