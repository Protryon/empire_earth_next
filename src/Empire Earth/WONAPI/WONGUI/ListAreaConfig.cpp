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
#include "rust/ee_won.h"
// The instruction keys and the flag and value names this unit compares are ee_won's guiconf tables
// (rust/crates/ee_won/src/guiconf.rs); this unit keeps the reads, the objects and what each entry does.
namespace WONAPI {

// FUNCTION: WON 0x0078d5d0
// Reads a list sort type (ASCENDING, DESCENDING or NONE); throws on any other name
ListSortType ListAreaConfig::ReadSortType()
{
    GUIString name=this->ReadString(false);
    const int aNameIdx_name=ee_won_gui_cfg_name(EE_WON_CFG_NAMES_LISTAREACONFIG_READSORTTYPE_NAME,name.mData->mText,name.mData->mLength,sizeof(wchar_t));
    if(aNameIdx_name==0) return ListSortType_Ascending;
    if(aNameIdx_name==1) return ListSortType_Descending;
    if(aNameIdx_name==2) return ListSortType_None;
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
        const int aNameIdx_name=ee_won_gui_cfg_name(EE_WON_CFG_NAMES_LISTAREACONFIG_READLISTFLAGS_NAME,name.mData->mText,name.mData->mLength,sizeof(wchar_t));
        if(aNameIdx_name==0) flags=2;
        else if(aNameIdx_name==1) flags=4;
        else if(aNameIdx_name==2) flags=8;
        else if(aNameIdx_name==3) flags=0x10;
        else if(aNameIdx_name==4) flags=0x20;
        else if(aNameIdx_name==5) flags=0x40;
        else if(aNameIdx_name==6) flags=0x80;
        else if(aNameIdx_name==7) flags=0x100;
        else if(aNameIdx_name==8) flags=0x200;
        else if(aNameIdx_name==9) flags=0x400;
        else if(aNameIdx_name==10) flags=0x800;
        else if(aNameIdx_name==11) flags=0x1000;
        else if(aNameIdx_name==12) flags=0x2000;
        else throw ConfigObjectException("Unknown list flag: "+(std::string)name);
        inArea->SetListFlags(flags,on);
    }
}

// FUNCTION: WON 0x0078da70
// Handles the LISTFLAGS, TEXTITEM, FONT, TEXTCOLOR, SELCOLOR and LISTSORTTYPE instructions; others go to
// the scroll area config
bool ListAreaConfig::Virtual24(const std::string& inInstruction)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_LISTAREACONFIG_VIRTUAL24_ININSTRUCTION,inInstruction.data(),inInstruction.size());
    if(aKeyIdx==0) ListAreaConfig::ReadListFlags(this,this->mListArea);
    else if(aKeyIdx==1)
    {
        while(!this->EndOfString())
        {
            this->mListArea->InsertString(this->ReadString(false),-1);
            if(!this->EndOfString()) this->EnsureComma();
        }
    }
    else if(aKeyIdx==2) this->mListArea->SetFont(this->ReadFont());
    else if(aKeyIdx==3) this->mListArea->mTextColor=this->ReadColor();
    else if(aKeyIdx==4) this->mListArea->mSelColor=this->ReadSelectionColor();
    else if(aKeyIdx==5) this->mListArea->SetSortType(this->ReadSortType());
    else return ScrollAreaConfig::Virtual24(inInstruction);
    return true;
}

}
