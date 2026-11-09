// Table element row and column growth (file name inferred)
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
#include "ee/wonapi/WONLobbyTableElement.h"
namespace WONAPI {
// FUNCTION: WON 0x0070d9c0
// Grows the table to at least the given number of columns, numbering the new columns and adding empty
// cells to every row
void TableElement::EnsureNumCols(int numCols)
{
    int oldNumCols=this->mColumnVector.size();
    if(numCols<oldNumCols)
        return;
    this->mColumnVector.resize(numCols);
    int i;
    for(i=oldNumCols;i<numCols;++i)
        this->mColumnVector[i].mColumnNum=i;
    for(i=0;i<this->mRowVector.size();++i)
        this->EnsureNumCols(this->mRowVector[i],numCols);
}

// FUNCTION: WON 0x0070da90
// Grows a row to at least the given number of cells by appending empty cells
void TableElement::EnsureNumCols(RowInfo& row,int numCols)
{
    int oldNumCols=row.mRow.size();
    if(numCols<oldNumCols) return;
    row.mRow.reserve(numCols);
    for(int i=oldNumCols;i<numCols;++i)
        row.mRow.push_back(CellInfo());
}

// FUNCTION: WON 0x0070dd20
// Grows the table to at least the given number of rows, each new row filled with empty cells
void TableElement::EnsureNumRows(int numRows)
{
    int numCols=this->mColumnVector.size();
    int oldNumRows=this->mRowVector.size();
    for(int i=oldNumRows;i<numRows;++i) {
        this->mRowVector.push_back(RowInfo(i));
        RowInfo& row=this->mRowVector.back();
        this->EnsureNumCols(row,numCols);
    }
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0071b490
// (public: unsigned int __thiscall std::vector<struct WONAPI::TableElement::CellInfo,class std::allocator<struct WONAPI::TableElement::CellInfo> >::size()
// TEMPLATE: WON 0x007206b0
// (public: class std::_Ptrit<struct WONAPI::TableElement::RowInfo,int,struct WONAPI::TableElement::RowInfo *,struct WONAPI::TableElement::RowInfo &,struc)
// TEMPLATE: WON 0x007206d0
// (public: class std::_Ptrit<struct WONAPI::TableElement::RowInfo,int,struct WONAPI::TableElement::RowInfo *,struct WONAPI::TableElement::RowInfo &,struc)
// TEMPLATE: WON 0x00723540
// (public: __thiscall std::vector<struct WONAPI::TableElement::CellInfo,class std::allocator<struct WONAPI::TableElement::CellInfo> >::vector<struct WONA)
// TEMPLATE: WON 0x007240e0
// (struct WONAPI::TableElement::ColumnInfo * __cdecl std::copy(class std::_Ptrit<struct WONAPI::TableElement::ColumnInfo,int,struct WONAPI::TableElement:)
// TEMPLATE: WON 0x00724120
// (public: unsigned int __thiscall std::vector<struct WONAPI::TableElement::CellInfo,class std::allocator<struct WONAPI::TableElement::CellInfo> >::capac)
// TEMPLATE: WON 0x00724150
// (protected: bool __thiscall std::vector<struct WONAPI::TableElement::CellInfo,class std::allocator<struct WONAPI::TableElement::CellInfo> >::_Buy(unsig)
// TEMPLATE: WON 0x00724500
// (protected: struct WONAPI::TableElement::ColumnInfo * __thiscall std::vector<struct WONAPI::TableElement::ColumnInfo,class std::allocator<struct WONAPI)
// TEMPLATE: WON 0x00724540
// (void __cdecl std::fill(class std::_Ptrit<struct WONAPI::TableElement::ColumnInfo,int,struct WONAPI::TableElement::ColumnInfo *,struct WONAPI::TableEle)
// TEMPLATE: WON 0x00724570
// (class std::_Ptrit<struct WONAPI::TableElement::ColumnInfo,int,struct WONAPI::TableElement::ColumnInfo *,struct WONAPI::TableElement::ColumnInfo &,stru)
// TEMPLATE: WON 0x00724d70
// (void __cdecl std::_Construct(struct WONAPI::TableElement::CellInfo *,struct WONAPI::TableElement::CellInfo const &)    [?_Construct@std@@YAXPAUCellInf)
// TEMPLATE: WON 0x00724dd0
// (void __cdecl std::_Construct(struct WONAPI::TableElement::ColumnInfo *,struct WONAPI::TableElement::ColumnInfo const &)    [?_Construct@std@@YAXPAUCol)
// TEMPLATE: WON 0x00724f60
// (public: struct WONAPI::TableElement::CellInfo & __thiscall WONAPI::TableElement::CellInfo::operator=(struct WONAPI::TableElement::CellInfo const &)   )
// TEMPLATE: WON 0x00725700
// (struct WONAPI::TableElement::CellInfo * __cdecl std::copy(class std::_Ptrit<struct WONAPI::TableElement::CellInfo,int,struct WONAPI::TableElement::Cel)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007241a0
// (protected: void __thiscall std::vector<struct WONAPI::TableElement::CellInfo,class std::allocator<struct WONAPI::TableElement::CellInfo> >::_Destroy(s)
// TEMPLATE: WON 0x0071cd20
// (public: void __thiscall std::vector<struct WONAPI::TableElement::ColumnInfo,class std::allocator<struct WONAPI::TableElement::ColumnInfo> >::resize(un)
// TEMPLATE: WON 0x0071ce60
// (public: class std::_Ptrit<struct WONAPI::TableElement::RowInfo,int,struct WONAPI::TableElement::RowInfo *,struct WONAPI::TableElement::RowInfo &,struc)
// TEMPLATE: WON 0x0071fe50
// (public: void __thiscall std::vector<struct WONAPI::TableElement::ColumnInfo,class std::allocator<struct WONAPI::TableElement::ColumnInfo> >::insert(cl)
// TEMPLATE: WON 0x007201d0
// (public: void __thiscall std::vector<struct WONAPI::TableElement::RowInfo,class std::allocator<struct WONAPI::TableElement::RowInfo> >::insert(class st)
// TEMPLATE: WON 0x007235e0
// (protected: void __thiscall std::vector<struct WONAPI::TableElement::ColumnInfo,class std::allocator<struct WONAPI::TableElement::ColumnInfo> >::_Xlen()
// TEMPLATE: WON 0x007245b0
// (protected: struct WONAPI::TableElement::RowInfo * __thiscall std::vector<struct WONAPI::TableElement::RowInfo,class std::allocator<struct WONAPI::Tabl)
// TEMPLATE: WON 0x007245f0
// (void __cdecl std::fill(class std::_Ptrit<struct WONAPI::TableElement::RowInfo,int,struct WONAPI::TableElement::RowInfo *,struct WONAPI::TableElement::)
// TEMPLATE: WON 0x00725000
// (public: void * __thiscall WONAPI::TableElement::RowInfo::`scalar deleting destructor'(unsigned int))

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007243c0
// (void __cdecl std::fill(class std::_Ptrit<struct WONAPI::TableElement::CellInfo,int,struct WONAPI::TableElement::CellInfo *,struct WONAPI::TableElement)
// TEMPLATE: WON 0x00724450
// (class std::_Ptrit<struct WONAPI::TableElement::CellInfo,int,struct WONAPI::TableElement::CellInfo *,struct WONAPI::TableElement::CellInfo &,struct WON)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0071cca0
// (public: class std::_Ptrit<struct WONAPI::TableElement::CellInfo,int,struct WONAPI::TableElement::CellInfo *,struct WONAPI::TableElement::CellInfo &,st)
// TEMPLATE: WON 0x0071f980
// (public: void __thiscall std::vector<struct WONAPI::TableElement::CellInfo,class std::allocator<struct WONAPI::TableElement::CellInfo> >::insert(class )
// TEMPLATE: WON 0x00723da0
// (protected: struct WONAPI::TableElement::CellInfo * __thiscall std::vector<struct WONAPI::TableElement::CellInfo,class std::allocator<struct WONAPI::Ta)
// TEMPLATE: WON 0x00724df0
// (void __cdecl std::_Construct(struct WONAPI::TableElement::RowInfo *,struct WONAPI::TableElement::RowInfo const &))
// TEMPLATE: WON 0x00724810
// (class std::_Ptrit<struct WONAPI::TableElement::RowInfo,int,struct WONAPI::TableElement::RowInfo *,struct WONAPI::TableElement::RowInfo &,struct WONAPI)
