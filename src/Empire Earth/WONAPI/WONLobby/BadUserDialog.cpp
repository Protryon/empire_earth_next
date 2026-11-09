// The moderators' bad user dialog: lists banned or muted players with expiry, moderator and comment
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <stdio.h>
#include <string>
#include <list>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONLobbyDialogs.h"
using namespace WONAPI;
// One row of the bad user list (global namespace, 0x50 bytes)
class BadUserItem : public ListItem {
public:
    BadUserItem(const RoutingBadUser& inUser,bool inSimple);
    virtual void Paint(Graphics& inGraphics,ListArea* inList);
    virtual void CalcDimensions(ListArea* inList);
    virtual int Compare(const ListItem* inOther,ListArea* inList) const;
    SmartPtr<Font> mFont; // 0x18
    GUIString mStrings[6]; // 0x1c..0x30: name, WON id, expiry, moderator, moderator's WON id, comment
    unsigned long mValues[6]; // 0x34..0x48: sort keys; only the WON id (1), expiry (2) and moderator id (4) are set
    bool mIsSimple; // 0x4c (padded to 0x50)
};
// FUNCTION: WON 0x7f5290
// Creates a row from a bad user entry, writing the expiry as days, hours, minutes, seconds or infinite
BadUserItem::BadUserItem(const RoutingBadUser& inUser, bool inSimple)
{
    this->mIsSimple = inSimple;
    this->mStrings[0] = GUIString(new GUIStringData(inUser.mName));
    char aBuffer[100];
    sprintf(aBuffer, "%d", inUser.mWONId);
    this->mStrings[1] = aBuffer;
    this->mValues[1] = inUser.mWONId;

    unsigned long aDuration = inUser.mExpirationDiff;
    GUIString aText;
    GUIString aUnit;
    if (aDuration > 86400) {
        aDuration /= 86400;
        aUnit = aDuration > 1 ? ::BadUserDialog_XDays_String : ::BadUserDialog_OneDay_String;
    } else if (aDuration > 3600) {
        aDuration /= 3600;
        aUnit = aDuration > 1 ? ::BadUserDialog_XHours_String : ::BadUserDialog_OneHour_String;
    } else if (aDuration > 60) {
        aDuration /= 60;
        aUnit = aDuration > 1 ? ::BadUserDialog_XMinutes_String : ::BadUserDialog_OneMinute_String;
    } else if (aDuration > 0) {
        aUnit = aDuration > 1 ? ::BadUserDialog_XSeconds_String : ::BadUserDialog_OneSecond_String;
    } else {
        aUnit = ::BadUserDialog_Infinite_String;
    }
    sprintf(aBuffer, "%d", aDuration);
    this->mValues[2] = inUser.mExpirationDiff;
    this->mStrings[2] = LobbyFormatString(aUnit, GUIString(aBuffer));
    this->mStrings[3] = GUIString(inUser.mModeratorName);
    sprintf(aBuffer, "%d", inUser.mModeratorWONId);
    this->mStrings[4] = GUIString(aBuffer);
    this->mValues[4] = inUser.mModeratorWONId;
    this->mStrings[5] = GUIString(inUser.mModeratorComment);
}

// FUNCTION: WON 0x7f5790
// Takes the list's font and makes the row one text line high
void BadUserItem::CalcDimensions(ListArea* inList)
{
    this->mFont=inList->mFont;
    this->mHeight=this->mFont->GetHeight();
}
// FUNCTION: WON 0x7f5800
// Draws the selection background and the text of the column being painted (the name in the simple list)
void BadUserItem::Paint(Graphics& inGraphics,ListArea* inList)
{
    DWORD aColor=inList->GetTextColor();
    if(this->mSelected) aColor=inList->GetSelColor().GetForegroundColor(inGraphics,aColor);
    int aColumn=0;
    bool aSelected=this->mSelected;
    bool aSimple=this->mIsSimple;
    if(!aSimple) aColumn=static_cast<MultiListArea*>(inList)->GetColumnPaintContext();
    if((aSimple || aColumn==-1) && aSelected) {
        inGraphics.SetColor(inList->GetSelColor().GetBackgroundColor(inGraphics));
        int aWidth=inList->mPaintWidth;
        int aHeight=this->mFont->GetHeight();
        inGraphics.FillRect(0,0,aWidth,aHeight);
        if(!this->mIsSimple) return;
    }
    if(aColumn<0 || aColumn>5)
        return;
    inGraphics.SetColor(aColor);
    inGraphics.SetFont(this->mFont.get());
    inGraphics.DrawString(this->mStrings[aColumn],0,0);
}
// FUNCTION: WON 0x7f58c0
// Orders two rows by the sort column: numerically for the ids and expiry, else by text
int BadUserItem::Compare(const ListItem* inOther,ListArea* inList) const
{
    int aColumn=static_cast<MultiListArea*>(inList)->GetSortColumn();
    if(aColumn<0 || aColumn>5) return this<inOther;
    const BadUserItem* anOther=static_cast<const BadUserItem*>(inOther);
    switch(aColumn) {
    case 1:
    case 2:
    case 4:
        return this->mValues[aColumn]-anOther->mValues[aColumn];
    default:
        return this->mStrings[aColumn].mData->Compare(anOther->mStrings[aColumn].mData.get());
    }
}
namespace WONAPI {

BadUserDialog::BadUserDialog() : mIsSimple(false) {}
// The destructor is implicit; the original stores no vtable of its own in it
// FUNCTION: WON 0x7f5a90
// Finds the full and the simple user lists and their boxes in the layout
void BadUserDialog::InitComponent(ComponentInit& inInit)
{
    if(inInit.mType!=2) return;
    ComponentConfig* aConfig=static_cast<ComponentConfigInit&>(inInit).mConfig;
    SafeGetComponent(aConfig,this->mUserList,"UserList",__FILE__,157);
    SafeGetComponent(aConfig,this->mSimpleUserList,"SimpleUserList",__FILE__,158);
    SafeGetComponent(aConfig,this->mUserListBox,"UserListBox",__FILE__,159);
    SafeGetComponent(aConfig,this->mSimpleUserListBox,"SimpleUserListBox",__FILE__,160);
}
// FUNCTION: WON 0x7f5b20
// Fills the full or the simple list with the given bad users and shows only that list
void BadUserDialog::SetUserList(const RoutingBadUserList& inList,bool inSimple)
{
    this->mIsSimple=inSimple;
    this->mUserListBox->SetVisible(!inSimple);
    this->mSimpleUserListBox->SetVisible(inSimple);
    ListArea* aList=this->mIsSimple ? this->mSimpleUserList.get() : this->mUserList.get();
    aList->SetVisible(true);
    aList->BeginUpdate();
    aList->Clear(true);
    for(RoutingBadUserList::const_iterator anItr=inList.begin();anItr!=inList.end();++anItr)
        aList->InsertItem(new BadUserItem(*anItr,inSimple),-1);
    aList->EndUpdate();
}
// FUNCTION: WON 0x7f5c20
// Returns the WON ids of the selected users
void BadUserDialog::GetIds(IdList& outIds)
{
    ListArea* aList=this->mIsSimple ? this->mSimpleUserList.get() : this->mUserList.get();
    outIds.clear();
    aList->RewindSelections();
    while(aList->HasMoreSelections()) {
        BadUserItem* anItem=static_cast<BadUserItem*>(aList->GetNextSelection());
        unsigned long anId=anItem->mValues[1]; // the original reads the id (0x7f5ca5) before allocating the list node
        outIds.push_back(anId);
    }
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007f5710
// (public: virtual __thiscall BadUserItem::~BadUserItem(void)    [??1BadUserItem@@UAE@XZ])
// TEMPLATE: WON 0x007fa4a0

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007f5960
// (public: virtual void * __thiscall WONAPI::BadUserDialog::`scalar deleting destructor'(unsigned int)    [??_GBadUserDialog@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x007f5980
// (public: virtual __thiscall WONAPI::BadUserDialog::~BadUserDialog(void)    [??1BadUserDialog@WONAPI@@UAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007f56f0
// (public: virtual void * __thiscall BadUserItem::`scalar deleting destructor'(unsigned int)    [??_GBadUserItem@@UAEPAXI@Z])
// TEMPLATE: WON 0x007f5920
// (public: __thiscall WONAPI::BadUserDialog::BadUserDialog(void)    [??0BadUserDialog@WONAPI@@QAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007fa200
// (protected: void __thiscall std::list<unsigned long,class std::allocator<unsigned long> >::_Incsize(unsigned int))
