// Multi-column list header: column buttons, sort state and column resizing by drag (file name inferred)
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
#include "ee/wonapi/WONComponentFlags.h"
namespace WONAPI {
// FUNCTION: WON 0x00702bc0
// Attaches the header to a list area and registers itself as that list's header
void MultiListHeader::SetMultiListArea(MultiListArea* area)
{
    this->mListArea=area;
    if(this->mListArea!=NULL) this->mListArea->mListHeader=this;
}
// FUNCTION: WON 0x00702cb0
// Records a column as the sorted one with its direction (ignored for an invalid column)
void MultiListHeader::SetSortColumn(int column,ListSortType type)
{
    if(column<0 || column>=this->mColumnVector.size()) return;
    this->mColumnVector[column].mSortType=type;
    this->mCurSortCol=column;
}
// FUNCTION: WON 0x00702cf0
// Replaces a column's header component; a list column with no width yet takes the component's desired width
void MultiListHeader::SetColumnItem(int column,Component* item)
{
    this->mDragColumn=-1;
    if(this->mColumnVector.size()<=column) this->mColumnVector.resize(column+1);
    Component* old=this->mColumnVector[column].mComponent;
    if(old!=NULL) this->RemoveChild(old);
    item->mControlId=column;
    item->SetComponentFlags(ComponentFlag_Clip,true);
    this->mColumnVector[column].mComponent=item;
    this->AddChild(item);
    if(this->mListArea!=NULL && column<this->mListArea->mColumnInfoVector.size() && this->mListArea->GetColumnWidth(column)<=0) {
        int width,height;
        item->GetDesiredSize(width,height);
        this->mListArea->SetColumnWidth(column,width,false);
        if(this->mListArea->mSortColumn==column) {
            this->mCurSortCol=column;
            this->mColumnVector[column].mSortType=static_cast<ListSortType>(this->mListArea->mSortType);
        }
    } else this->NotifyColumnChange();
}
// FUNCTION: WON 0x00702e10
// Lines the header components up with the list columns (hiding zero-width ones) and stretches the last one;
// assumes the last column has a component
void MultiListHeader::NotifyColumnChange()
{
    if(this->mListArea==NULL) return;
    std::vector<MultiListArea::ColumnInfo>& columns=this->mListArea->mColumnInfoVector;
    int x=-this->mListArea->mHorzOffset;
    int size=this->mColumnVector.size();
    for(int i=0;i<size;++i) {
        Component* component=this->mColumnVector[i].mComponent;
        int width=-1;
        if(i<columns.size()) width=columns[i].mWidth;
        if(component!=NULL) {
            if(width==-1) {
                component->SetEnabled(false);
                width=0;
            } else component->SetEnabled(true);
            component->SetVisible(width!=0);
            component->SetPosSize(x,0,width,this->Height());
        }
        x+=width;
    }
    if(this->mColumnVector.size()>0) {
        Component* component=this->mColumnVector[this->mColumnVector.size()-1].mComponent;
        component->SetVisible(true);
        component->SetPosSize(component->Left(),0,this->Width(),this->Height());
    }
    this->Invalidate();
}
// FUNCTION: WON 0x00702fb0
// After a resize, re-lays out the header components over the columns
void MultiListHeader::Virtual08()
{
    Container::Virtual08();
    this->NotifyColumnChange();
}
// FUNCTION: WON 0x00702fd0
// Returns the column whose header component this is, or -1
int MultiListHeader::GetColumnIndex(Component* component)
{
    for(int i=0;i<this->mColumnVector.size();++i)
        if(this->mColumnVector[i].mComponent==component) return i;
    return -1;
}
// FUNCTION: WON 0x00703010
// Returns a column's sort direction if it is the sorted column, otherwise none
ListSortType MultiListHeader::GetSortType(int column)
{
    if(column<0 || column>=this->mColumnVector.size() || column!=this->mCurSortCol)
        return ListSortType_None;
    return this->mColumnVector[column].mSortType;
}
}

namespace WONAPI {
// FUNCTION: WON 0x00702be0
// On a column button press sorts the list by that column (toggling direction when already sorted by it);
// other events go to the container
void MultiListHeader::HandleComponentEvent(ComponentEvent* event)
{
    if(event->mType==ComponentEvent_ButtonPressed && this->mListArea!=NULL) {
        Component* child=event->mComponent;
        Container* parent=child->mParent;
        while(parent!=this && parent!=NULL) {
            child=parent;
            parent=child->mParent;
        }
        if(parent==NULL) return;
        int column=child->mControlId;
        if(column>=0 && column<(int)this->mListArea->mColumnInfoVector.size() && column<this->mColumnVector.size()) {
            ListSortType type=this->mColumnVector[column].mSortType;
            if(column==this->mCurSortCol || type==ListSortType_None)
                type=type==ListSortType_Ascending ? ListSortType_Descending : ListSortType_Ascending;
            this->mListArea->SetSortColumn(column,type);
            return;
        }
    }
    Container::HandleComponentEvent(event);
}
// FUNCTION: WON 0x00703050
// Returns the sort arrow a header button should draw, found through its enclosing header (none without one)
ListSortType MultiListHeader::GetSortTypeForButton(Component* button)
{
    while(true) {
        Container* parent=button->mParent;
        if(parent==NULL) return ListSortType_None;
        MultiListHeader* header=dynamic_cast<MultiListHeader*>(parent);
        if(header==NULL) {
            button=parent;
            continue;
        }
        if(header->mDrawSortArrow) return header->GetSortType(header->GetColumnIndex(button));
        return ListSortType_None;
    }
}
// FUNCTION: WON 0x007030b0
// Returns the header component whose resize edge (up to 5 pixels) is at x and starts that column's drag, or NULL
// when no resizable edge is there
Component* MultiListHeader::GetDragComponent(int x)
{
    this->mDragColumn=-1;
    this->mDragComponent=NULL;
    std::vector<MultiListArea::ColumnInfo>& columns=this->mListArea->mColumnInfoVector;
    for(int i=0;i<this->mColumnVector.size();++i) {
        Component* component=this->mColumnVector[i].mComponent;
        if(component==NULL || !(x>=component->Left()) || !(x<=component->Right()))
            continue;
        int width=component->Width()/4;
        if(width>5) width=5;
        if(x-component->Left()<width && i!=0) this->mDragColumn=i-1;
        else if(component->Right()-x<width && i!=this->mColumnVector.size()) this->mDragColumn=i;
        else return NULL;
        if(this->mDragColumn>=columns.size()) {
            this->mDragColumn=-1;
            return NULL;
        }
        MultiListArea::ColumnInfo& info=columns[this->mDragColumn];
        if(info.mSizeMode==0 || (info.mSizeMode==2 && (info.mPartnerColumn<0 || info.mPartnerColumn>=columns.size()))) {
            this->mDragColumn=-1;
            return NULL;
        }
        this->mDragX=x-this->mListArea->mColumnInfoVector[this->mDragColumn].mWidth;
        this->mDragComponent=this->mColumnVector[this->mDragColumn].mComponent;
        return this->mDragComponent;
    }
    return NULL;
}
// FUNCTION: WON 0x00703260
// Starts a column resize on a left press at a column edge; otherwise passes the press on
void MultiListHeader::MouseDown(int x,int y,int button)
{
    if(button==0 && this->GetDragComponent(x)!=NULL) return;
    Container::MouseDown(x,y,button);
}
// FUNCTION: WON 0x00703290
// Shows the east-west resize cursor over a column edge and restores the normal cursor elsewhere
void MultiListHeader::MouseMove(int x,int y)
{
    Container::MouseMove(x,y);
    if(this->GetDragComponent(x)!=NULL) this->SetCursor(Cursor::GetStandardCursor(StandardCursor_EastWest));
    else if(this->mCurrentCursor.get()==Cursor::GetStandardCursor(StandardCursor_EastWest)) this->SetCursor(NULL);
}
// FUNCTION: WON 0x007032f0
// While resizing, sets the dragged column's width from the mouse position; otherwise passes the drag on
void MultiListHeader::MouseDrag(int x,int y)
{
    if(this->mDragColumn>=0) {
        int width=x-this->mDragX;
        if(width<0) width=0;
        this->mListArea->SetColumnWidth(this->mDragColumn,width,false);
        this->SetCursor(Cursor::GetStandardCursor(StandardCursor_EastWest));
    } else Container::MouseDrag(x,y);
}
// FUNCTION: WON 0x00703350
// Ends a column resize on left release; otherwise passes the release on
void MultiListHeader::MouseUp(int x,int y,int button)
{
    if(button==0 && this->mDragColumn!=-1) this->mDragColumn=-1;
    else Container::MouseUp(x,y,button);
}
// FUNCTION: WON 0x00703390
// Cancels any column resize when the mouse leaves
void MultiListHeader::MouseExit()
{
    this->mDragColumn=-1;
    Container::MouseExit();
}
}

namespace WONAPI {
// FUNCTION: WON 0x00702a90
// Creates a clipped list header with sort arrows on, no sort column, attached to a list area
MultiListHeader::MultiListHeader(MultiListArea* area)
{
    this->SetComponentFlags(ComponentFlag_Clip,true);
    this->mDragColumn=-1;
    this->mCurSortCol=-1;
    this->mDrawSortArrow=true;
    this->SetMultiListArea(area);
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00702b30
// (public: virtual void * __thiscall WONAPI::MultiListHeader::`scalar deleting destructor'(unsigned int)    [??_GMultiListHeader@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x0071d040
// (public: class std::_Ptrit<struct WONAPI::MultiListHeader::ColumnInfo,int,struct WONAPI::MultiListHeader::ColumnInfo *,struct WONAPI::MultiListHeader::)
// TEMPLATE: WON 0x007238c0
// (public: class std::_Ptrit<struct WONAPI::MultiListHeader::ColumnInfo,int,struct WONAPI::MultiListHeader::ColumnInfo *,struct WONAPI::MultiListHeader::)
// TEMPLATE: WON 0x007249f0
// (protected: struct WONAPI::MultiListHeader::ColumnInfo * __thiscall std::vector<struct WONAPI::MultiListHeader::ColumnInfo,class std::allocator<struct )
// TEMPLATE: WON 0x00724a30
// (void __cdecl std::fill(class std::_Ptrit<struct WONAPI::MultiListHeader::ColumnInfo,int,struct WONAPI::MultiListHeader::ColumnInfo *,struct WONAPI::Mu)
// TEMPLATE: WON 0x00724a60
// (class std::_Ptrit<struct WONAPI::MultiListHeader::ColumnInfo,int,struct WONAPI::MultiListHeader::ColumnInfo *,struct WONAPI::MultiListHeader::ColumnIn)
// TEMPLATE: WON 0x00724aa0
// (struct WONAPI::MultiListHeader::ColumnInfo * __cdecl std::copy(class std::_Ptrit<struct WONAPI::MultiListHeader::ColumnInfo,int,struct WONAPI::MultiLi)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00702b50
// (public: virtual __thiscall WONAPI::MultiListHeader::~MultiListHeader(void)    [??1MultiListHeader@WONAPI@@UAE@XZ])

// Compiler-generated and header-inline functions this unit emits:

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0071cf80
// (public: void __thiscall std::vector<struct WONAPI::MultiListHeader::ColumnInfo,class std::allocator<struct WONAPI::MultiListHeader::ColumnInfo> >::res)
// TEMPLATE: WON 0x00720f70
// (public: void __thiscall std::vector<struct WONAPI::MultiListHeader::ColumnInfo,class std::allocator<struct WONAPI::MultiListHeader::ColumnInfo> >::ins)
// TEMPLATE: WON 0x00721280
// (public: class std::_Ptrit<struct WONAPI::MultiListHeader::ColumnInfo,int,struct WONAPI::MultiListHeader::ColumnInfo *,struct WONAPI::MultiListHeader::)
// TEMPLATE: WON 0x00723780
// (protected: void __thiscall std::vector<struct WONAPI::MultiListHeader::ColumnInfo,class std::allocator<struct WONAPI::MultiListHeader::ColumnInfo> >::)
