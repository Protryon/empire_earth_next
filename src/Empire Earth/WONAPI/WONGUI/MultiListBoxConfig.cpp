// Config instructions for multi-column list areas and their headers (file name inferred)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyResourceConfig.h"
namespace WONAPI {
// FUNCTION: WON 0x00790160
// Handles one multi-column list instruction (sort column/func, widths, column count, alignment, auto-grow)
// or passes it to the list area config; throws on an unknown sort function
bool MultiListAreaConfig::Virtual24(const std::string& instruction)
{
    if(instruction=="SORTCOLUMN") {
        int column=this->ReadInt(); this->EnsureComma();
        ListSortType type=this->ReadSortType();
        this->mMultiListArea->SetSortColumn(column,type);
    } else if(instruction=="COLUMNWIDTH") {
        int column=this->ReadInt(); this->EnsureComma();
        int width=this->ReadInt();
        this->mMultiListArea->SetColumnWidth(column,width,false);
    } else if(instruction=="NUMCOLUMNS") this->mMultiListArea->SetNumColumns(this->ReadInt());
    else if(instruction=="DYNAMICSIZECOLUMN") this->mMultiListArea->mDynamicSizeColumn=this->ReadInt();
    else if(instruction=="SORTCOLUMNFUNC") {
        int column=this->ReadInt(); this->EnsureComma();
        GUIString name=this->ReadString(false);
        ListArea::SortFunc function=NULL;
        if(name.CompareNoCase("NUMERIC")==0) function=ListArea::NumberSortFunc;
        else throw ConfigObjectException("Unknown sort func: "+(std::string)name);
        this->mMultiListArea->SetSortColumnFunc(column,function);
    } else if(instruction=="AUTOGROWCOLUMNS") this->mMultiListArea->mAutoGrowColumns=this->ReadBool();
    else if(instruction=="COLUMNHALIGN") {
        int column=this->ReadInt(); this->EnsureComma();
        HorzAlign align=this->ReadHorzAlign();
        this->mMultiListArea->SetColumnHAlign(column,align);
    } else return ListAreaConfig::Virtual24(instruction);
    return true;
}
// FUNCTION: WON 0x00790490
// Handles SETCOLUMN and MULTILISTAREA for a list header, or passes the instruction to the container config
bool MultiListHeaderConfig::Virtual24(const std::string& instruction)
{
    if(instruction=="SETCOLUMN") {
        int column=this->ReadInt(); this->EnsureComma();
        Component* component=this->ReadComponent();
        this->mHeader->SetColumnItem(column,component);
    } else if(instruction=="MULTILISTAREA") {
        this->mHeader->SetMultiListArea(SafeConfigGetComponent<MultiListArea*>(this->ReadComponent(),"MultiListArea"));
    } else return ContainerConfig::Virtual24(instruction);
    return true;
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007a20d0
// (class WONAPI::MultiListArea * __cdecl WONAPI::SafeConfigGetComponent(class WONAPI::Component *,char const *)    [?SafeConfigGetComponent@WONAPI@@YAPAV)
