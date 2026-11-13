// Lobby game list: one row per game with per-column paint, sort and filter callbacks, filter combo boxes,
// join/refresh buttons and the layout config that adds the columns
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyControlConfig.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyEvents.h"
#include "ee/wonapi/WONLobbyModel.h"
#include "ee/wonapi/WONLobbyPersistence.h"
#include "ee/wonapi/WONLobbyCore.h"
#include "ee/wonapi/WONLobbyRoomServer.h"
#include "ee/wonapi/EELobbyGameCode.h"
#include "rust/ee_won.h"
// The instruction keys and the flag and value names this unit compares are ee_won's guiconf tables
// (rust/crates/ee_won/src/guiconf.rs); this unit keeps the reads, the objects and what each entry does.
using namespace WONAPI;
// Filter combo box row that offers to add or remove the typed filter text (RTTI .?AVFilterListItem@@, global
// namespace); a TextListItem (0x28 bytes) with no new virtual functions, 0x38 bytes in all
class FilterListItem : public WONAPI::TextListItem {
public:
    FilterListItem(const WONAPI::GUIString& inColumn,WONAPI::Font* inFont)
        :WONAPI::TextListItem(WONAPI::gEmptyGUIString,inFont),mIsAdd(false),mSelPos(0),mColumnName(inColumn) {}
    bool mIsAdd; // 0x28: true when the row adds the text, false when it removes it
    int mSelPos; // 0x2c: position of the typed text in the combo list
    WONAPI::GUIString mFilterText; // 0x30: the typed filter text
    WONAPI::GUIString mColumnName; // 0x34: the column the combo filters
};

namespace WONAPI {
// The column paint, compare and filter callbacks below take the owning ServerListCtrl as third
// argument; most of them ignore it
// FUNCTION: WON 0x007c4c80
// Creates a selectable game list row with no game
ServerDetailsItem::ServerDetailsItem() { this->mSelectable=true; }
// FUNCTION: WON 0x007c4d10
// Destroys the game list row (members only)
ServerDetailsItem::~ServerDetailsItem() {}
// FUNCTION: WON 0x007c4da0
// Attaches a game to the row; for direct-connect games also caches its host:port text
void ServerDetailsItem::SetGame(LobbyGame* inGame)
{
    this->mGame=inGame;
    if(this->mGame->GetGameType()==3)
    {
        this->mIPAddrStr=this->mGame->GetAddress().GetHostAndPortString(true);
    }
}
// FUNCTION: WON 0x007c5800
// Picks the row's status icon: in progress, invite-only (invited/host or not), ask to join, password or open
void ServerDetailsItem::CalcIcon()
{
    if(this->mGame->IsInProgress()) this->mIconId=0x176;
    else if(this->mGame->GetInviteOnly())
    {
        bool aOwned=this->mGame->IsInvited() || this->mGame->GetIsHost();
        if(aOwned) this->mIconId=0x171;
        else this->mIconId=0x175;
    }
    else if(this->mGame->GetAskToJoin()) this->mIconId=0x173;
    else if(this->mGame->GetHasPassword()) this->mIconId=0x174;
    else this->mIconId=0x172;
}
// FUNCTION: WON 0x007c5080
// Orders two rows by status icon
int ServerDetailsItem::CompareIcon(const ServerDetailsItem* a,const ServerDetailsItem* b,ServerListCtrl*)
{ return a->mIconId-b->mIconId; }
// FUNCTION: WON 0x007c50a0
// Orders two rows by the games' skill level
int ServerDetailsItem::CompareSkill(const ServerDetailsItem* a,const ServerDetailsItem* b,ServerListCtrl*)
{ return a->mGame->GetSkill()-b->mGame->GetSkill(); }
// FUNCTION: WON 0x007c50c0
// Orders two rows by game name, ignoring case
int ServerDetailsItem::CompareGameName(const ServerDetailsItem* a,const ServerDetailsItem* b,ServerListCtrl*)
{ return a->mGame->GetName().CompareNoCase(b->mGame->GetName()); }
// FUNCTION: WON 0x007c50e0
// Orders two rows by the games' network address
int ServerDetailsItem::CompareAddress(const ServerDetailsItem* a,const ServerDetailsItem* b,ServerListCtrl*)
{ return a->mGame->GetAddress().StringCompare(b->mGame->GetAddress()); }
// FUNCTION: WON 0x007c5100
// Orders two rows by player count, then the game with more player slots first
int ServerDetailsItem::CompareNumPlayers(const ServerDetailsItem* a,const ServerDetailsItem* b,ServerListCtrl*)
{
    int result=a->mGame->GetNumPlayers()-b->mGame->GetNumPlayers();
    if(result==0) result=b->mGame->GetMaxPlayers()-a->mGame->GetMaxPlayers();
    return result;
}
// FUNCTION: WON 0x007c5130
// Orders two rows by ping time
int ServerDetailsItem::ComparePing(const ServerDetailsItem* a,const ServerDetailsItem* b,ServerListCtrl*)
{ return a->mGame->GetPing()-b->mGame->GetPing(); }
// FUNCTION: WON 0x007c51e0
// Returns true for a digit or a sign character
bool ServerDetailsItem::IsStartOfNumber(char c)
{
    return ((static_cast<unsigned int>(static_cast<int>(c))<256 && isdigit(c)) || c=='-' || c=='+') ? 1 : 0;
}
// FUNCTION: WON 0x007c5220
// Parses a numeric filter (<n, <=n, >n, >=n, =n or n, space-separated) into bounds; false if nothing parsed
bool ServerDetailsItem::ParseIntFilter(const GUIString& inFilter,int& outLower,int& outUpper)
{
    std::string aFilter=static_cast<std::string>(inFilter);
    char aBuffer[512];
    strncpy(aBuffer,aFilter.c_str(),510);
    aBuffer[511]=0;
    bool aParsed=false;
    for(char* aToken=strtok(aBuffer," ");aToken!=NULL;aToken=strtok(NULL," "))
    {
        if(aToken[0]=='<')
        {
            if(aToken[1]=='=' && ServerDetailsItem::IsStartOfNumber(aToken[2])) { outUpper=atoi(aToken+2);aParsed=true; }
            else if(ServerDetailsItem::IsStartOfNumber(aToken[1])) { aParsed=true;outUpper=atoi(aToken+1)-1; }
        }
        else if(aToken[0]=='>')
        {
            if(aToken[1]=='=' && ServerDetailsItem::IsStartOfNumber(aToken[2])) { aParsed=true;outLower=atoi(aToken+2); }
            else if(ServerDetailsItem::IsStartOfNumber(aToken[1])) { aParsed=true;outLower=atoi(aToken+1)+1; }
        }
        else if(aToken[0]=='=')
        {
            if(ServerDetailsItem::IsStartOfNumber(aToken[1])) { outUpper=atoi(aToken+1);outLower=outUpper;aParsed=true; }
        }
        else if(ServerDetailsItem::IsStartOfNumber(aToken[0])) { outUpper=atoi(aToken);outLower=outUpper;aParsed=true; }
    }
    return aParsed;
}
// FUNCTION: WON 0x007c5460
// Removes games whose skill level is outside the typed numeric filter
void ServerDetailsItem::FilterSkill(const GUIString& inFilter,ServerDetailsList& ioList,ServerListCtrl*)
{
    int aLower=-100000,aUpper=100000;
    if(!ServerDetailsItem::ParseIntFilter(inFilter,aLower,aUpper)) return;
    for(ServerDetailsList::iterator it=ioList.begin();it!=ioList.end();)
    {
        ServerDetailsItem* anItem=*it;
        if(anItem->mGame->GetSkill()<aLower || anItem->mGame->GetSkill()>aUpper) ioList.erase(it++);
        else ++it;
    }
}
// FUNCTION: WON 0x007c54f0
// Removes games whose name does not contain the filter text (case-insensitive)
void ServerDetailsItem::FilterGameName(const GUIString& inFilter,ServerDetailsList& ioList,ServerListCtrl*)
{
    for(ServerDetailsList::iterator it=ioList.begin();it!=ioList.end();)
    {
        if((*it)->mGame->GetName().findNoCase(inFilter,0)>=0) ++it;
        else ioList.erase(it++);
    }
}
// FUNCTION: WON 0x007c5560
// Removes games whose address text does not contain the filter text (case-insensitive)
void ServerDetailsItem::FilterAddress(const GUIString& inFilter,ServerDetailsList& ioList,ServerListCtrl*)
{
    for(ServerDetailsList::iterator it=ioList.begin();it!=ioList.end();)
    {
        if((*it)->mIPAddrStr.findNoCase(inFilter,0)>=0) ++it;
        else ioList.erase(it++);
    }
}
// FUNCTION: WON 0x007c5750
// Removes games whose ping rating (10 - ping/100) is outside the typed numeric filter
void ServerDetailsItem::FilterPing(const GUIString& inFilter,ServerDetailsList& ioList,ServerListCtrl*)
{
    int aLower=-100000,aUpper=100000;
    if(!ServerDetailsItem::ParseIntFilter(inFilter,aLower,aUpper)) return;
    for(ServerDetailsList::iterator it=ioList.begin();it!=ioList.end();)
    {
        int aPing=10-(*it)->mGame->GetPing()/100;
        if(aPing<aLower || aPing>aUpper) ioList.erase(it++);
        else ++it;
    }
}
// The summary height is a byte: each store truncates it
// FUNCTION: WON 0x007c4f00
// Sizes the row: summary line from the font or expand image, plus the details area when expanded
void ServerDetailsItem::CalcDimensions(ListArea* inList)
{
    Font* aFont=inList->GetDefaultFont();
    this->CalcIcon();
    this->mSummaryHeight=static_cast<unsigned char>(aFont->GetHeight());
    unsigned short imageHeight=::ServerListCtrl_GameExpanded_Image->mHeight;
    if(imageHeight>this->mSummaryHeight)
        this->mSummaryHeight=static_cast<unsigned char>(imageHeight);
    this->mSummaryHeight+=2;
    this->mHeight=this->mSummaryHeight;
    if(!this->mGame->GetShowDetails())
        return;
    int aHeight=this->GetDetailsHeightHook(static_cast<MultiListArea*>(inList));
    if(aHeight<aFont->GetHeight()) aHeight=aFont->GetHeight();
    this->mHeight+=aHeight;
}
// FUNCTION: WON 0x007c4f80
// Returns true for a click on the summary line right of the first (expand) column
bool ServerDetailsItem::CanSelect(ListArea* inList,int x,int y)
{
    return (this->mSelectable && x>static_cast<MultiListArea*>(inList)->GetColumnWidth(0) && y<=this->mSummaryHeight) ? 1 : 0;
}
// FUNCTION: WON 0x007c4fc0
// Toggles the game's details on a left click in the expand column; asks for a details query when opened
void ServerDetailsItem::MouseDown(ListArea* inList,int x,int y,int inButton)
{
    if(inButton!=0) return;
    if(x>static_cast<MultiListArea*>(inList)->GetColumnWidth(0) || y>this->mSummaryHeight) return;
    this->mGame->SetShowDetails(!this->mGame->GetShowDetails());
    inList->RefreshItem(this);
    inList->EnsureVisible(this);
    if(this->mGame->GetShowDetails())
        inList->DeliverEvent(new PingGameEvent(this->mGame.get(),true),true);
}
// The vertical centring divides as int and truncates toward zero
// FUNCTION: WON 0x007c58b0
// Draws an image at column offset x, centred vertically in the row's summary line
static void DrawServerImage(Graphics& g,Image* inImage,ServerDetailsItem* inItem,int x)
{
    g.DrawImage(inImage,x,(inItem->mSummaryHeight-inImage->mHeight)/2);
}
// FUNCTION: WON 0x007c5860
// Paints the expand/collapse arrow and the game's status icon
void ServerDetailsItem::PaintIcon(Graphics& g,ServerDetailsItem* inItem,ServerListCtrl*)
{
    if(inItem->mGame->GetShowDetails())
        DrawServerImage(g,::ServerListCtrl_GameExpanded_Image.get(),inItem,15);
    else
        DrawServerImage(g,::ServerListCtrl_GameNotExpanded_Image.get(),inItem,15);
    DrawServerImage(g,::LobbyResource_GetImage((LobbyResourceId)(inItem->mIconId)),inItem,0);
}
// FUNCTION: WON 0x007c58e0
// Paints the skill level image for skill levels 0 to 4
void ServerDetailsItem::PaintSkill(Graphics& g,ServerDetailsItem* inItem,ServerListCtrl*)
{
    unsigned char aSkill=static_cast<unsigned char>(inItem->mGame->GetSkill());
    if(aSkill<5) DrawServerImage(g,::LobbyResource_GetImage((LobbyResourceId)(aSkill+0x177)),inItem,0);
}
// FUNCTION: WON 0x007c5920
// Paints the game name in the list's bold font
void ServerDetailsItem::PaintGameName(Graphics& g,ServerDetailsItem* inItem,ServerListCtrl* inCtrl)
{
    g.SetFont(inCtrl->mBoldFont.get());
    g.DrawString(inItem->mGame->GetName(),5,0);
}
// FUNCTION: WON 0x007c5950
// Paints the game's address text
void ServerDetailsItem::PaintAddress(Graphics& g,ServerDetailsItem* inItem,ServerListCtrl*)
{
    g.DrawString(inItem->mIPAddrStr,0,0);
}
// FUNCTION: WON 0x007c55c0
// Removes games outside the player-count filter and its full / not full / open words; full plus not full empties
void ServerDetailsItem::FilterNumPlayers(const GUIString& inFilter,ServerDetailsList& ioList,ServerListCtrl*)
{
    bool aNotFull=false;
    bool aOpen=false;
    bool aFull=false;
    int aPos=inFilter.findNoCase(::ServerListCtrl_NotFullFilter_String,0);
    if(aPos==0 || (aPos>0 && inFilter[aPos-1]==' ')) aNotFull=true;
    aPos=inFilter.findNoCase(::ServerListCtrl_FullFilter_String,0);
    if(aPos==0 || (aPos>0 && inFilter[aPos-1]==' ')) aFull=true;
    aPos=inFilter.findNoCase(::ServerListCtrl_OpenFilter_String,0);
    if(aPos==0 || (aPos>0 && inFilter[aPos-1]==' ')) aOpen=true;
    if(aFull && aNotFull) { ioList.clear();return; }
    int aLower=-100000,aUpper=100000;
    if(!ServerDetailsItem::ParseIntFilter(inFilter,aLower,aUpper) && !aFull && !aNotFull && !aOpen) return;
    for(ServerDetailsList::iterator it=ioList.begin();it!=ioList.end();)
    {
        ServerDetailsItem* anItem=*it;
        int aCount=anItem->mGame->GetNumPlayers();
        int aMax=anItem->mGame->GetMaxPlayers();
        if(aMax==0) aMax=0x7ffffff;
        if(aCount<aLower || aCount>aUpper || (aFull && aCount<aMax) ||
            (aNotFull && aCount>=aMax) || (aOpen && !anItem->mGame->CanJoin())) ioList.erase(it++);
        else ++it;
    }
}
// FUNCTION: WON 0x007c5970
// Paints the player count as players/max (or players alone without a max), centred in the column
void ServerDetailsItem::PaintNumPlayers(Graphics& g,ServerDetailsItem* inItem,ServerListCtrl* inCtrl)
{
    char aBuffer[50];
    if(inItem->mGame->GetMaxPlayers()>0) sprintf(aBuffer,"%d/%d",inItem->mGame->GetNumPlayers(),inItem->mGame->GetMaxPlayers());
    else sprintf(aBuffer,"%d",inItem->mGame->GetNumPlayers());
    GUIString aText(new GUIStringData(aBuffer));
    int aWidth=inCtrl->mList->GetPaintWidth();
    int aTextWidth=g.mFont->GetStringWidth(aText);
    g.DrawString(aText,(aWidth-aTextWidth)/2,0);
}
// FUNCTION: WON 0x007c5ac0
// Paints one column of the row, or for the whole row the selection and the details (querying/failed text)
void ServerDetailsItem::Paint(Graphics& g,ListArea* inList)
{
    MultiListArea* aList=static_cast<MultiListArea*>(inList);
    int aColumn=aList->GetColumnPaintContext();
    ServerListCtrl* aCtrl=static_cast<ServerListCtrl*>(aList->mParent->mParent);
    unsigned long aColor; if(this->mGame->IsInProgress()) aColor=g.GetStandardColor(StandardColor_GrayText); else aColor=g.GetStandardColor(StandardColor_Text);
    if(this->mSelected) aColor=aList->GetSelColor().GetForegroundColor(g,aColor);
    g.SetColor(aColor);
    g.SetFont(aList->mFont.get());
    if(aColumn==-1)
    {
        if(this->mSelected)
        {
            g.SetColor(aList->GetSelColor().GetBackgroundColor(g));
            g.FillRect(0,0,aList->GetPaintWidth(),this->mSummaryHeight);
        }
        if(!this->mGame->GetShowDetails()) return;
        int aWidth=aList->GetColumnWidth(0);
        g.Translate(aWidth,this->mSummaryHeight);
        if(this->mGame->GetHaveDetails()) this->PaintDetailsHook(g,aList);
        else
        {
            g.SetFont(aList->mFont.get());
            g.SetSysColor(StandardColor_Text);
            if(this->mGame->IsQueryingDetails()) g.DrawString(::ServerListCtrl_Querying_String,0,0);
            else g.DrawString(::ServerListCtrl_QueryFailed_String,0,0);
        }
        g.Translate(-aWidth,-this->mSummaryHeight);
        return;
    }
    ServerListCtrl::ColumnInfo::PaintFunc aPaint=aCtrl->mColumnInfo[aColumn].mPaintFunc;
    if(aPaint!=NULL) aPaint(g,this,aCtrl);
}
// FUNCTION: WON 0x007c5150
// Sorts by the current sort column, then the previous one, then by item address
int ServerDetailsItem::Compare(const ListItem* inOther,ListArea* inList) const
{
    const ServerDetailsItem* anOther=static_cast<const ServerDetailsItem*>(inOther);
    if(this==anOther) return 0;
    MultiListArea* aList=static_cast<MultiListArea*>(inList);
    ServerListCtrl* aCtrl=static_cast<ServerListCtrl*>(aList->mParent->mParent);
    int aSecondaryColumn=aList->GetSortColumnPrev(); // 0x184
    ServerListCtrl::ColumnInfo::CompareFunc aCompare=aCtrl->mColumnInfo[aList->GetSortColumn()].mCompareFunc;
    int aResult;
    if(aCompare!=NULL)
    {
        aResult=aCompare(this,anOther,aCtrl);
        if(aResult!=0) return aResult;
    }
    aCompare=aCtrl->mColumnInfo[aSecondaryColumn].mCompareFunc;
    if(aCompare!=NULL)
    {
        aResult=aCompare(this,anOther,aCtrl);
        if(aResult!=0) return aResult;
    }
    return static_cast<const ListItem*>(this)-inOther;
}
// FUNCTION: WON 0x007c5a90
// Paints the game's ping bars
void ServerDetailsItem::PaintPing(Graphics& g,ServerDetailsItem* inItem,ServerListCtrl*)
{
    DrawLobbyPing(&g,inItem->mGame->GetPing(),inItem->mSummaryHeight,0,0);
}
// FUNCTION: WON 0x007c6340
// Adds a column's default filter strings to the persistent filter options
void ServerListCtrl::AddDefaultFilters(const GUIString& inColumn,const GUIString& inStrings,bool inAddIfExists)
{
    LobbyAddGameOption(inColumn,inStrings,inAddIfExists);
}

ServerListCtrl::ColumnMap ServerListCtrl::mColumnMap;
// FUNCTION: WON 0x007c5ef0
// Destroys a column description (members only)
ServerListCtrl::ColumnInfo::~ColumnInfo() {}
// FUNCTION: WON 0x007cc4b0
// FUNCTION: WON 0x007c5f80
// Creates a column description with no callbacks, shown for every game type
ServerListCtrl::ColumnInfo::ColumnInfo()
{
    mPaintFunc=NULL;
    mCompareFunc=NULL;
    mFilterFunc=NULL;
    mGameTypeFlags=0xff;
}
ServerListCtrl::ServerListCtrl()
{
    this->mGameType=0;
    this->mNumColumns=4;
    this->mFilterTimeAccum=0;
    this->mNeedFilter=false;
}
// FUNCTION: WON 0x007c5d30
// Destroys the game list control (members only)
ServerListCtrl::~ServerListCtrl() {}
// FUNCTION: WON 0x007c6a70
// Removes games that are already in progress
void ServerListCtrl::FilterInProgressGames(ServerDetailsList& ioList)
{
    for(ServerDetailsList::iterator it=ioList.begin();it!=ioList.end();)
    {
        if((*it)->mGame->IsInProgress()) ioList.erase(it++);
        else ++it;
    }
}
// FUNCTION: WON 0x007c6ad0
// Applies every column's non-empty filter, then hides games in progress (except direct connect) unless shown
void ServerListCtrl::DoFilter(ServerDetailsList& ioList)
{
    SmartPtr<LobbyGame> aSelectedGame=this->GetSelGame();
    for(unsigned int i=0;i<this->mColumnInfo.size();++i)
    {
        ColumnInfo& aColumn=this->mColumnInfo[i];
        if(aColumn.mFilterFunc!=NULL && aColumn.mFilterCombo.get()!=NULL && aColumn.mFilterCombo->mEdit->GetText().length()!=0)
            aColumn.mFilterFunc(aColumn.mFilterCombo->mEdit->GetText(),ioList,this);
    }
    if(this->mGameType!=3 && !LobbyGetShowGamesInProgress()) this->FilterInProgressGames(ioList);
}
// FUNCTION: WON 0x007c6be0
// Returns true when one game row passes the current filters
bool ServerListCtrl::FitsFilter(ServerDetailsItem* inItem)
{
    ServerDetailsList aList;
    aList.push_back(inItem);
    this->DoFilter(aList);
    return !aList.empty();
}
// FUNCTION: WON 0x007c6d00
// Enables Join when the selected game can be joined and Remove when a game is selected
void ServerListCtrl::UpdateJoinButton()
{
    bool aEnabled=false;
    LobbyGame* aGame=this->GetSelGame();
    if(aGame!=NULL) aEnabled=aGame->CanJoin();
    this->mJoinButton->SetEnabled(aEnabled);
    if(this->mRemoveButton.get()!=NULL) this->mRemoveButton->SetEnabled(aGame!=NULL);
}
// FUNCTION: WON 0x007c7580
// Returns the selected game, or NULL
LobbyGame* ServerListCtrl::GetSelGame()
{
    ServerDetailsItem* aSelected=static_cast<ServerDetailsItem*>(this->mList->GetSelItem());
    return aSelected==NULL?NULL:aSelected->mGame.get();
}
// FUNCTION: WON 0x007c6ff0
// Refills the list with the given rows, keeping the selected game selected; updates buttons and summary
void ServerListCtrl::AddEntries(const ServerDetailsList& inList)
{
    LobbyGame* aSelectedGame=this->GetSelGame();
    this->mList->BeginUpdate();
    this->mList->Clear(true);
    for(ServerDetailsList::const_iterator it=inList.begin();it!=inList.end();++it)
    {
        ServerDetailsItem* aItem=*it;
        this->mList->ListArea::InsertItem(aItem,-1);
        aItem->mGame->SetIsFiltered(false);
        if(aItem->mGame.get()==aSelectedGame)
        {
            int aPos=this->mList->GetItemPos(aItem);
            this->mList->SetSelItem(aPos,true);
        }
    }
    this->mList->EndUpdate();
    this->UpdateJoinButton();
    this->UpdateSummaryLabel();
}
// Overrides Container's bool TimerEvent (vtable +0x44)
// FUNCTION: WON 0x007c66d0
// Refilters the list once a filter edit has rested 100-500 ms (longer for long lists); stops the timer then
bool ServerListCtrl::TimerEvent(int inDelta)
{
    if(this->mNeedFilter)
    {
        this->mFilterTimeAccum+=inDelta;
        if(this->mFilterTimeAccum>=100 && (this->mFilterTimeAccum>=500 || static_cast<unsigned int>(this->mFilterTimeAccum)>=this->mServerDetailsMap.size()/4))
        {
            this->AddFilteredEntries_All(true);
            this->mNeedFilter=false;
        }
    }
    if(!this->mNeedFilter) this->mTimerFlags&=~2;
    return Container::TimerEvent(inDelta);
}
// FUNCTION: WON 0x007c7220
// Refilters every known game into the list
void ServerListCtrl::AddFilteredEntries_All(bool inPreserve)
{
    ServerDetailsList aList;
    for(ServerDetailsMap::iterator it=this->mServerDetailsMap.begin();it!=this->mServerDetailsMap.end();++it)
    {
        it->second->mGame->SetIsFiltered(true);
        aList.push_back(it->second.get());
    }
    this->AddFilteredEntries(inPreserve,aList);
}
// FUNCTION: WON 0x007c73c0
// Refilters the games currently shown in the list
void ServerListCtrl::AddFilteredEntries_List(bool inPreserve)
{
    ServerDetailsList aList;
    int aCount=this->mList->GetNumItems();
    for(int i=0;i<aCount;++i) aList.push_back(static_cast<ServerDetailsItem*>(this->mList->GetItem(i)));
    this->AddFilteredEntries(inPreserve,aList);
}
// FUNCTION: WON 0x007c7090
// Filters the rows into the list; optionally keeps the scroll position on the first visible row still shown
void ServerListCtrl::AddFilteredEntries(bool inPreserve,ServerDetailsList& ioList)
{
    this->mNeedFilter=false;
    std::list<std::pair<ListItem*,int> > aOldItems;
    if(inPreserve)
    {
        int aOffset=this->mList->GetVertOffset();
        int aBottom=this->mList->GetVertOffset()+this->mList->mScrollArea.Height();
        int anIndex=this->mList->GetItemPosAtYPos(aOffset);
        ListItem* anItem=this->mList->GetItem(anIndex);
        if(anItem!=NULL && (anItem->mHeight+anItem->mTop)/2<aOffset) ++anIndex;
        while(true)
        {
            ListItem* anOldItem=this->mList->GetItem(anIndex++);
            if(anOldItem==NULL) break;
            if(anOldItem->mTop>aBottom) break;
            aOldItems.push_back(std::pair<ListItem*,int>(anOldItem,anOldItem->mTop-aOffset));
        }
    }
    this->DoFilter(ioList);
    this->AddEntries(ioList);
    if(inPreserve)
    {
        for(std::list<std::pair<ListItem*,int> >::iterator it=aOldItems.begin();it!=aOldItems.end();++it)
        {
            ListItem* anItem=it->first;
            if(this->mList->GetItemPos(anItem)>=0)
            {
                this->mList->SetVertOffset(anItem->mTop-it->second);
                break;
            }
        }
    }
}

// FUNCTION: WON 0x007c8340
// Sets the number of columns
void ServerListCtrl::SetNumColumns(int inCount)
{
    this->mNumColumns=inCount;
    this->mColumnInfo.resize(inCount);
}
// FUNCTION: WON 0x007c80c0
// Stops pinging and refilters the shown games while pings are pending; false otherwise
bool ServerListCtrl::HandleRefreshButton()
{
    if(LobbyGetNumPingsToDo()>0)
    {
        LobbyKillPings(1);
        this->AddFilteredEntries_List(true);
        return true;
    }
    return false;
}
// FUNCTION: WON 0x007c7a50
// Sends game action 7 on a right click, or joins the game on a double left click in a joinable row
void ServerListCtrl::HandleListItemClicked(ListItemClickedEvent* inEvent)
{
    if(!inEvent->IsSelectedItem()) return;
    ServerDetailsItem* aSelected=static_cast<ServerDetailsItem*>(this->mList->GetSelItem());
    if(aSelected==NULL) return;
    LobbyGame* aGame=aSelected->mGame.get();
    if(inEvent->mButton==2) this->DeliverEvent(new GameActionEvent(7,aGame),false);
    else if(inEvent->mDoubleClick && inEvent->mButton==0 && inEvent->mX>=this->mList->GetColumnWidth(0) && aGame->CanJoin())
        this->DeliverEvent(new JoinGameEvent(aGame),false);
}
// FUNCTION: WON 0x007c75a0
// Adds, removes or refreshes a game of this list's type, applying the filters; updates buttons and summary
void ServerListCtrl::HandleGameChanged(GameChangedEvent* inEvent)
{
    LobbyGame* aGame=inEvent->mGame.get();
    if(aGame->GetGameType()!=this->mGameType) return;
    if(inEvent->IsAdd())
    {
        SmartPtr<ServerDetailsItem> anItem=ServerDetailsItem::CreateItem();
        anItem->SetGame(aGame);
        this->mServerDetailsMap[aGame]=anItem;
        bool aFits=this->FitsFilter(anItem.get());
        aGame->SetIsFiltered(!aFits);
        if(aFits) this->mList->ListArea::InsertItem(anItem.get(),-1);
    }
    else if(inEvent->IsDelete())
    {
        ServerDetailsMap::iterator it=this->mServerDetailsMap.find(aGame);
        if(it!=this->mServerDetailsMap.end())
        {
            this->mList->RemoveItem(it->second.get());
            this->mServerDetailsMap.erase(it);
        }
    }
    else
    {
        ServerDetailsMap::iterator it=this->mServerDetailsMap.find(aGame);
        if(it==this->mServerDetailsMap.end()) return;
        ServerDetailsItem* anItem=it->second.get();
        bool aVisible=this->mList->GetItemPos(anItem)>=0;
        bool aFits=this->FitsFilter(anItem);
        anItem->mGame->SetIsFiltered(!aFits);
        if(aVisible)
        {
            if(aFits) this->mList->RefreshItem(anItem);
            else this->mList->RemoveItem(anItem);
        }
        else if(aFits) this->mList->ListArea::InsertItem(anItem,-1);
    }
    this->UpdateJoinButton();
    this->UpdateSummaryLabel();
}
// FUNCTION: WON 0x007c7870
// Rebuilds the rows from a full game list of this list's type and refills the list
void ServerListCtrl::HandleSyncGameList(SyncGameListEvent* inEvent)
{
    LobbyGameList* aGames=inEvent->mGames.get();
    if(aGames->GetType()!=this->mGameType) return;
    aGames->Rewind();
    this->mServerDetailsMap.clear();
    while(aGames->HasMore())
    {
        LobbyGame* aGame=aGames->Next();
        SmartPtr<ServerDetailsItem> anItem=ServerDetailsItem::CreateItem();
        anItem->SetGame(aGame);
        this->mServerDetailsMap[aGame]=anItem;
    }
    this->AddFilteredEntries_All(false);
}
// FUNCTION: WON 0x007c6d50
// Shows the pending ping count with a Stop button, or shown/total games with a Refresh button
void ServerListCtrl::UpdateSummaryLabel()
{
    if(this->mSummaryLabel.get()==NULL) return;
    char aTotalCount[33];
    char aShownCount[33];
    sprintf(aTotalCount,"%d",this->mServerDetailsMap.size());
    if(LobbyGetNumPingsToDo()>0)
    {
        sprintf(aShownCount,"%d",LobbyGetNumPingsToDo());
        this->mSummaryLabel->SetText(LobbyFormatString(::ServerListCtrl_ListPingSummary_String,aShownCount),true);
        this->mRefreshButton->SetText(::ServerListCtrl_StopRefresh_String,true);
    }
    else
    {
        sprintf(aShownCount,"%d",this->mList->GetNumItems());
        this->mSummaryLabel->SetText(LobbyFormatString2(::ServerListCtrl_ListSummary_String,aShownCount,aTotalCount),true);
        this->mRefreshButton->SetText(::ServerListCtrl_RefreshList_String,true);
    }
}
// FUNCTION: WON 0x007c6740
// Fills each column's filter combo: no filter, the add/remove row, then the saved filter values
void ServerListCtrl::LoadFilterCombos()
{
    for(ColumnInfoVector::iterator it=this->mColumnInfo.begin();it!=this->mColumnInfo.end();++it)
    {
        ComboBox* aCombo=it->mFilterCombo.get();
        if(aCombo==NULL) continue;
        aCombo->Clear(true);
        aCombo->InsertItem(new TextListItem(::ServerListCtrl_NoFilter_String,this->mBoldFont.get()),-1);
        aCombo->InsertItem(new FilterListItem(it->mColumnName,this->mBoldFont.get()),-1);
        LobbyOptionSet aFilters=LobbyGetFilterValues(it->mColumnName);
        for(size_t jt=0;jt<aFilters.size();++jt)
            aCombo->AddString(aFilters.ValueAt(jt),-1);
    }
}
extern int gServerFilterItemPos;
// FUNCTION: WON 0x007c7b30
// Starts the refilter timer and relabels the filter row as Add or Remove for the typed text
void ServerListCtrl::HandleComboTextChanged(Component* inComponent)
{
    ComboBox* aCombo=dynamic_cast<ComboBox*>(inComponent);
    if(aCombo==NULL) return;
    this->mNeedFilter=true;
    this->mFilterTimeAccum=0;
    this->RequestTimer(true);
    const GUIString& aText=aCombo->mEdit->GetText();
    FilterListItem* aFilter=static_cast<FilterListItem*>(aCombo->mListArea->GetItem(gServerFilterItemPos));
    if(aFilter==NULL) return;
    if(aText.length()==0)
    {
        aFilter->mText=::ServerListCtrl_AddRemove_String;
        aFilter->mIsAdd=true;
        aFilter->mFilterText=GUIString("");
        aFilter->mSelPos=0;
        aCombo->UpdateItem(aFilter);
        return;
    }
    int aCount=aCombo->mListArea->GetNumItems();
    int aPos;
    for(aPos=0;aPos<aCount;++aPos)
    {
        if(static_cast<TextListItem*>(aCombo->mListArea->GetItem(aPos))->mText==aText)
        {
            if(aPos<=gServerFilterItemPos) return;
            break;
        }
    }
    aFilter->mSelPos=aPos;
    aFilter->mIsAdd=aPos>=aCount;
    aFilter->mFilterText=aText;
    GUIString aLabel;
    if(aFilter->mIsAdd) aLabel=::ServerListCtrl_Add_String;
    else aLabel=::ServerListCtrl_Remove_String;
    aLabel.append(aText);
    aLabel.append(GUIString(new GUIStringData("\")")));
    aFilter->mText=aLabel;
    aCombo->UpdateItem(aFilter);
}
// FUNCTION: WON 0x007c7dd0
// Handles a filter combo pick: clears the filter, or adds/removes the typed value in the saved filters
void ServerListCtrl::HandleComboSelChanged(ComboSelChangedEvent* inEvent)
{
    ComboBox* aCombo=dynamic_cast<ComboBox*>(inEvent->mComponent);
    if(aCombo==NULL) return;
    int aPos=aCombo->mListArea->GetItemPos(inEvent->mItem.get());
    if(aPos>gServerFilterItemPos) return;
    if(aPos==0 && inEvent->mActiveSel)
    {
        aCombo->SetText(GUIString(""),true);
        return;
    }
    FilterListItem* aFilter=static_cast<FilterListItem*>(aCombo->mListArea->GetItem(gServerFilterItemPos));
    if(inEvent->mActiveSel)
    {
        if(aFilter->mIsAdd)
        {
            if(aFilter->mFilterText.length()!=0)
            {
                ListItem* anItem=aCombo->AddString(aFilter->mFilterText,-1);
                aCombo->SetSelItem(aCombo->mListArea->GetItemPos(anItem),true);
                LobbyAddFilterValue(aFilter->mColumnName,aFilter->mFilterText);
            }
            else aCombo->SetText(GUIString(new GUIStringData("")),true);
        }
        else if(aFilter->mSelPos>gServerFilterItemPos)
        {
            aCombo->DeleteItem(aFilter->mSelPos);
            aCombo->SetText(GUIString(new GUIStringData("")),true);
            LobbyRemoveFilterValue(aFilter->mColumnName,aFilter->mFilterText);
        }
    }
    else
    {
        aCombo->SetText(aFilter->mFilterText,true);
        aCombo->mEdit->SelectRange(-1,0);
    }
}
// FUNCTION: WON 0x007c80f0
// Dispatches button, list, filter combo and game list events to the list's handlers
void ServerListCtrl::HandleComponentEvent(ComponentEvent* inEvent)
{
    switch(inEvent->mType)
    {
    case ComponentEvent_ButtonPressed:
        if(inEvent->mComponent==this->mJoinButton.get())
        {
            LobbyGame* aGame=this->GetSelGame();
            if(aGame!=NULL) this->DeliverEvent(new JoinGameEvent(aGame),false);
            return;
        }
        if(inEvent->mComponent==this->mRefreshButton.get() && this->HandleRefreshButton()) return;
        break;
    case ComponentEvent_ListSelChanged: this->UpdateJoinButton();break; // also passed on to Component::HandleComponentEvent
    case ComponentEvent_InputTextChanged: this->HandleComboTextChanged(inEvent->mComponent);return;
    case ComponentEvent_ComboSelChanged: this->HandleComboSelChanged(static_cast<ComboSelChangedEvent*>(inEvent));return;
    case ComponentEvent_ListItemClicked: this->HandleListItemClicked(static_cast<ListItemClickedEvent*>(inEvent));return;
    case LobbyEvent_GameChanged: this->HandleGameChanged(static_cast<GameChangedEvent*>(inEvent));return;
    case LobbyEvent_SyncGameList: this->HandleSyncGameList(static_cast<SyncGameListEvent*>(inEvent));return;
    case 100014:
    case 100034: this->LoadFilterCombos();return;
    case 100059: this->AddFilteredEntries_All(true);return;
    case 100058: this->AddFilteredEntries_List(true);return;
    }
    Component::HandleComponentEvent(inEvent);
}
// FUNCTION: WON 0x007c6360
// Finds the list, buttons and summary label in the layout, then updates them and joins the broadcast
void ServerListCtrl::InitComponent(ComponentInit& inInit)
{
    if(inInit.mType!=2) return;
    ComponentConfig* aConfig=static_cast<ComponentConfigInit&>(inInit).mConfig;
    SafeGetComponent(aConfig,this->mList,"ServerList",__FILE__,647);
    this->mBoldFont=this->mList->GetDefaultFontMod(1,0);
    SafeGetComponent(aConfig,this->mJoinButton,"JoinGameButton",__FILE__,650);
    SafeGetComponent(aConfig,this->mRefreshButton,"RefreshListButton",__FILE__,651);
    SafeGetComponent(aConfig,this->mSummaryLabel,"ServerListSummary",__FILE__,652);
    this->mRemoveButton=aConfig->Fn386860(std::string("RemoveButton"));
    this->UpdateJoinButton();
    this->UpdateSummaryLabel();
    this->SubscribeToBroadcast(1);
}
// FUNCTION: WON 0x007c5ff0
// Returns the shared column description for a column name, creating it if needed
ServerListCtrl::ColumnInfo* ServerListCtrl::GetColumnInfo(const GUIString& inName)
{
    ColumnInfo& aColumn=ServerListCtrl::mColumnMap[inName];
    aColumn.mColumnName=inName;
    return &aColumn;
}
// FUNCTION: WON 0x007c6050
// Registers the Icon, Skill, GameName, Address (direct only) and Ping (not LAN) column callbacks
void ServerListCtrl::InitColumnMap()
{
    ColumnInfo* aColumn=ServerListCtrl::GetColumnInfo("Icon");
    aColumn->mPaintFunc=ServerDetailsItem::PaintIcon;
    aColumn->mCompareFunc=ServerDetailsItem::CompareIcon;
    aColumn=ServerListCtrl::GetColumnInfo("Skill");
    aColumn->mPaintFunc=ServerDetailsItem::PaintSkill;
    aColumn->mCompareFunc=ServerDetailsItem::CompareSkill;
    aColumn->mFilterFunc=ServerDetailsItem::FilterSkill;
    aColumn=ServerListCtrl::GetColumnInfo("GameName");
    aColumn->mPaintFunc=ServerDetailsItem::PaintGameName;
    aColumn->mCompareFunc=ServerDetailsItem::CompareGameName;
    aColumn->mFilterFunc=ServerDetailsItem::FilterGameName;
    aColumn=ServerListCtrl::GetColumnInfo("Address");
    aColumn->mPaintFunc=ServerDetailsItem::PaintAddress;
    aColumn->mCompareFunc=ServerDetailsItem::CompareAddress;
    aColumn->mFilterFunc=ServerDetailsItem::FilterAddress;
    aColumn->mGameTypeFlags=4;
    aColumn=ServerListCtrl::GetColumnInfo("NumPlayers");
    aColumn->mPaintFunc=ServerDetailsItem::PaintNumPlayers;
    aColumn->mCompareFunc=ServerDetailsItem::CompareNumPlayers;
    aColumn->mFilterFunc=ServerDetailsItem::FilterNumPlayers;
    aColumn=ServerListCtrl::GetColumnInfo("Ping");
    aColumn->mPaintFunc=ServerDetailsItem::PaintPing;
    aColumn->mCompareFunc=ServerDetailsItem::ComparePing;
    aColumn->mFilterFunc=ServerDetailsItem::FilterPing;
    aColumn->mGameTypeFlags&=~2;
}

}

namespace WONAPI {
class MultiListHeader;
// ServerListCtrlConfig is declared in WONLobbyControlConfig.h
// FUNCTION: WON 0x007c8380
// Attaches the config to its game list control
void ServerListCtrlConfig::SetComponent(Component* inComponent)
{
    ContainerConfig::SetComponent(inComponent);
    this->mServerList=static_cast<ServerListCtrl*>(inComponent);
}
// FUNCTION: WON 0x007c83a0
// Answers NUMCOLUMNS with the list's column count, else asks the base config
bool ServerListCtrlConfig::GetNamedValue(const std::string& inName,int& outValue)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_SERVERLISTCTRLCONFIG_GETNAMEDVALUE_INNAME,inName.data(),inName.size());
    if(aKeyIdx==0) { outValue=this->mServerList->mColumnInfo.size();return true; }
    return ComponentConfig::GetNamedValue(inName,outValue);
}
// FUNCTION: WON 0x007c8c30
// Handles the LISTID, ADDCOLUMN, DEFAULTFILTER, NUMCOLUMNS and SETCOLUMNFILTERTYPE layout instructions
bool ServerListCtrlConfig::Virtual24(const std::string& inInstruction)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_SERVERLISTCTRLCONFIG_VIRTUAL24_ININSTRUCTION,inInstruction.data(),inInstruction.size());
    if(ContainerConfig::Virtual24(inInstruction)) return true;
    if(aKeyIdx==0)
    {
        GUIString aType=this->ReadString(false);
        const int aNameIdx_aType=ee_won_gui_cfg_name(EE_WON_CFG_NAMES_SERVERLISTCTRLCONFIG_VIRTUAL24_ATYPE,aType.mData->mText,aType.mData->mLength,sizeof(wchar_t));
        if(aNameIdx_aType==0) this->mServerList->SetGameType(2);
        else if(aNameIdx_aType==1) this->mServerList->SetGameType(1);
        else if(aNameIdx_aType==2) this->mServerList->SetGameType(3);
        else throw ConfigObjectException("Invalid server list id: "+static_cast<std::string>(aType));
    }
    else if(aKeyIdx==1) this->HandleAddColumn();
    else if(aKeyIdx==2)
    {
        GUIString aColumn=this->ReadString(false);
        this->EnsureComma();
        GUIString aFilters=this->ReadString(false);
        ServerListCtrl::AddDefaultFilters(aColumn,aFilters,false);
    }
    else if(aKeyIdx==3) this->mServerList->SetNumColumns(this->ReadInt());
    else if(aKeyIdx==4)
    {
        this->ReadInt();
        this->EnsureComma();
        GUIString aType=this->ReadString(false);
        // The filter type is only validated; ReadInt's column and the type are not kept.
        const int aNameIdx_aType=ee_won_gui_cfg_name(EE_WON_CFG_NAMES_SERVERLISTCTRLCONFIG_VIRTUAL24_ATYPE,aType.mData->mText,aType.mData->mLength,sizeof(wchar_t));
        if(aNameIdx_aType==3) {}
        else if(aNameIdx_aType==4) {}
        else if(aNameIdx_aType==5) {}
        else throw ConfigObjectException("Unknown filter type: "+static_cast<std::string>(aType));
    }
    else return false;
    return true;
}
// FUNCTION: WON 0x007c8410
// Adds a defined column (header component, width, filter combo) if it applies to the list's game type
void ServerListCtrlConfig::HandleAddColumn()
{
    MultiListArea* aList=this->mServerList->mList.get();
    if(aList==NULL)
    {
        Component* aComponent=this->Fn386860(std::string("ServerList"));
        if(aComponent==NULL) throw ConfigObjectException("Need ServerList before calling AddColumn");
        aList=dynamic_cast<MultiListArea*>(aComponent);
        if(aList==NULL) throw ConfigObjectException("ServerList needs to be a MultiListArea");
        this->mServerList->mList=aList;
        aComponent=this->Fn386860(std::string("ServerListHeader"));
        if(aComponent==NULL) throw ConfigObjectException("Need ServerList header before calling AddColumn");
        this->mHeader=dynamic_cast<MultiListHeader*>(aComponent);
        if(this->mHeader==NULL) throw ConfigObjectException("ServerListHeader needs to be a MultiListHeader");
    }
    GUIString aColumn=this->ReadString(false);
    this->EnsureComma();
    Component* aComponent=this->Fn386860(aColumn);
    if(aComponent==NULL) throw ConfigObjectException("Component not found: "+static_cast<std::string>(aColumn));
    int aWidth=this->ReadInt();
    ComboBox* aCombo=NULL;
    if(!this->EndOfString())
    {
        this->EnsureComma();
        aCombo=SafeConfigGetComponent<ComboBox*>(this->ReadComponent(),"ComboBox");
    }
    ServerListCtrl::ColumnMap::iterator it=ServerListCtrl::mColumnMap.find(aColumn);
    if(it==ServerListCtrl::mColumnMap.end())
        throw ConfigObjectException("Column not defined: "+static_cast<std::string>(aColumn));
    ServerListCtrl::ColumnInfo& aInfo=it->second;
    unsigned char aFlags=aInfo.mGameTypeFlags;
    switch(this->mServerList->mGameType)
    {
    case 3:
        if(aFlags&4) break;
        return;
    case 2:
        if(aFlags&1) break;
    default:
        return;
    case 1:
        if(aFlags&2) break;
        return;
    }
    aList->SetNumColumns(++this->mNumColumns);
    int aCol=this->mNumColumns;
    if(aWidth==0) aList->SetDynamicSizeColumn(--aCol);
    else aList->SetColumnWidth(--aCol,aWidth,false);
    this->mHeader->SetColumnItem(this->mNumColumns-1,aComponent);
    this->mServerList->mColumnInfo.push_back(aInfo);
    this->mServerList->mColumnInfo.back().mFilterCombo=aCombo;
}
}

namespace WONAPI {
// FUNCTION: WON 0x00765640
// Creates a game list layout config
ComponentConfig* ServerListCtrlConfig::CfgFactory()
{
    return new ServerListCtrlConfig;
}
}

namespace WONAPI {
// FUNCTION: WON 0x007c8360
// Creates the layout config with no header and no columns
ServerListCtrlConfig::ServerListCtrlConfig()
{
    this->mHeader=NULL;
    this->mNumColumns=0;
}
}

namespace WONAPI {
// Factory that creates the game list rows (class static)
ServerDetailsItem::ItemFactoryFunc ServerDetailsItem::mItemFactoryFunc = ServerDetailsItem::DefaultItemFactory;
int gServerFilterItemPos = 1;
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007bad40
// (protected: static class WONAPI::ServerDetailsItem * __cdecl WONAPI::ServerDetailsItem::DefaultItemFactory(void)    [?DefaultItemFactory@ServerDetailsI)
// TEMPLATE: WON 0x007c4cf0
// (public: virtual void * __thiscall WONAPI::ServerDetailsItem::`scalar deleting destructor'(unsigned int)    [??_GServerDetailsItem@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x007c5c10
// (public: __thiscall WONAPI::ServerListCtrl::ServerListCtrl(void)    [??0ServerListCtrl@WONAPI@@QAE@XZ])
// TEMPLATE: WON 0x007c5d10
// (public: virtual void * __thiscall WONAPI::ServerListCtrl::`scalar deleting destructor'(unsigned int)    [??_GServerListCtrl@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x007c68f0
// (public: __thiscall WONAPI::TextListItem::TextListItem(class WONAPI::GUIString const &,class WONAPI::Font *)    [??0TextListItem@WONAPI@@QAE@ABVGUIStri)
// TEMPLATE: WON 0x007c6970
// (public: virtual __thiscall FilterListItem::~FilterListItem(void)    [??1FilterListItem@@UAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007cb7c0
// (public: __thiscall std::vector<struct WONAPI::ServerListCtrl::ColumnInfo,class std::allocator<struct WONAPI::ServerListCtrl::ColumnInfo> >::~vector<st)
// TEMPLATE: WON 0x007cc1d0
// (public: class std::_Ptrit<struct WONAPI::ServerListCtrl::ColumnInfo,int,struct WONAPI::ServerListCtrl::ColumnInfo *,struct WONAPI::ServerListCtrl::Col)
// TEMPLATE: WON 0x007cc1f0
// (public: class std::_Ptrit<struct WONAPI::ServerListCtrl::ColumnInfo,int,struct WONAPI::ServerListCtrl::ColumnInfo *,struct WONAPI::ServerListCtrl::Col)
// TEMPLATE: WON 0x007cc400
// (public: __thiscall std::pair<class WONAPI::SmartPtr<class WONAPI::LobbyGame> const ,class WONAPI::SmartPtr<class WONAPI::ServerDetailsItem> >::pair<cl)
// TEMPLATE: WON 0x007ccf60
// (protected: class std::_Tree<class WONAPI::GUIString,struct std::pair<class WONAPI::GUIString const ,struct WONAPI::ServerListCtrl::ColumnInfo>,struct )
// TEMPLATE: WON 0x007cd840
// (public: void __thiscall std::_Tree<class WONAPI::GUIString,struct std::pair<class WONAPI::GUIString const ,struct WONAPI::ServerListCtrl::ColumnInfo>,)
// TEMPLATE: WON 0x007ce4d0
// (protected: struct WONAPI::ServerListCtrl::ColumnInfo * __thiscall std::vector<struct WONAPI::ServerListCtrl::ColumnInfo,class std::allocator<struct WO)
// TEMPLATE: WON 0x007ce510
// (void __cdecl std::fill(class std::_Ptrit<struct WONAPI::ServerListCtrl::ColumnInfo,int,struct WONAPI::ServerListCtrl::ColumnInfo *,struct WONAPI::Serv)
// TEMPLATE: WON 0x007ce5d0
// (class std::_Ptrit<struct WONAPI::ServerListCtrl::ColumnInfo,int,struct WONAPI::ServerListCtrl::ColumnInfo *,struct WONAPI::ServerListCtrl::ColumnInfo )
// TEMPLATE: WON 0x007ce6a0
// (struct WONAPI::ServerListCtrl::ColumnInfo * __cdecl std::copy(class std::_Ptrit<struct WONAPI::ServerListCtrl::ColumnInfo,int,struct WONAPI::ServerLis)
// TEMPLATE: WON 0x007ce770
// (void __cdecl std::_Construct(struct std::pair<class WONAPI::GUIString const ,struct WONAPI::ServerListCtrl::ColumnInfo> *,struct std::pair<class WONAP)
// TEMPLATE: WON 0x007ce7f0
// (void __cdecl std::_Construct(struct WONAPI::ServerListCtrl::ColumnInfo *,struct WONAPI::ServerListCtrl::ColumnInfo const &)    [?_Construct@std@@YAXPA)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007cbfc0
// (public: __thiscall std::pair<class WONAPI::SmartPtr<class WONAPI::LobbyGame> const ,class WONAPI::SmartPtr<class WONAPI::ServerDetailsItem> >::~pair<c)
// TEMPLATE: WON 0x007cd260
// (protected: void __thiscall std::_Tree<class WONAPI::SmartPtr<class WONAPI::LobbyGame>,struct std::pair<class WONAPI::SmartPtr<class WONAPI::LobbyGame>)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007c6950
// (public: virtual void * __thiscall FilterListItem::`scalar deleting destructor'(unsigned int)    [??_GFilterListItem@@UAEPAXI@Z])
// TEMPLATE: WON 0x007cb9d0
// (public: class std::_Tree<class WONAPI::SmartPtr<class WONAPI::LobbyGame>,struct std::pair<class WONAPI::SmartPtr<class WONAPI::LobbyGame> const ,class)
// TEMPLATE: WON 0x007cc210
// (public: class std::_Tree<class WONAPI::GUIString,struct std::pair<class WONAPI::GUIString const ,struct WONAPI::ServerListCtrl::ColumnInfo>,struct std)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007cb810
// (public: void __thiscall std::vector<struct WONAPI::ServerListCtrl::ColumnInfo,class std::allocator<struct WONAPI::ServerListCtrl::ColumnInfo> >::resiz)
// TEMPLATE: WON 0x007cb840
// (public: struct WONAPI::ServerListCtrl::ColumnInfo & __thiscall std::map<class WONAPI::GUIString,struct WONAPI::ServerListCtrl::ColumnInfo,struct WONAP)
// TEMPLATE: WON 0x007cbef0
// (public: __thiscall std::list<class WONAPI::ServerDetailsItem *,class std::allocator<class WONAPI::ServerDetailsItem *> >::~list<class WONAPI::ServerDe)
// TEMPLATE: WON 0x007cc040
// (public: void __thiscall std::vector<struct WONAPI::ServerListCtrl::ColumnInfo,class std::allocator<struct WONAPI::ServerListCtrl::ColumnInfo> >::resiz)
// TEMPLATE: WON 0x007cc160
// (public: class std::_Ptrit<struct WONAPI::ServerListCtrl::ColumnInfo,int,struct WONAPI::ServerListCtrl::ColumnInfo *,struct WONAPI::ServerListCtrl::Col)
// TEMPLATE: WON 0x007cc260
// (public: struct std::pair<class std::_Tree<class WONAPI::SmartPtr<class WONAPI::LobbyGame>,struct std::pair<class WONAPI::SmartPtr<class WONAPI::LobbyG)
// TEMPLATE: WON 0x007cc450
// (public: void __thiscall std::list<struct std::pair<class WONAPI::ListItem *,int>,class std::allocator<struct std::pair<class WONAPI::ListItem *,int> >)
// TEMPLATE: WON 0x007cc520
// (public: void __thiscall std::vector<struct WONAPI::ServerListCtrl::ColumnInfo,class std::allocator<struct WONAPI::ServerListCtrl::ColumnInfo> >::inser)
// TEMPLATE: WON 0x007cc9a0
// (public: class std::_Ptrit<struct WONAPI::ServerListCtrl::ColumnInfo,int,struct WONAPI::ServerListCtrl::ColumnInfo *,struct WONAPI::ServerListCtrl::Col)
// TEMPLATE: WON 0x007cca20
// (public: class std::_Tree<class WONAPI::GUIString,struct std::pair<class WONAPI::GUIString const ,struct WONAPI::ServerListCtrl::ColumnInfo>,struct std)
// TEMPLATE: WON 0x007ccf20
// (protected: void __thiscall std::_Tree<class WONAPI::GUIString,struct std::pair<class WONAPI::GUIString const ,struct WONAPI::ServerListCtrl::ColumnInf)
// TEMPLATE: WON 0x007cd2a0
// (protected: class std::_Tree<class WONAPI::SmartPtr<class WONAPI::LobbyGame>,struct std::pair<class WONAPI::SmartPtr<class WONAPI::LobbyGame> const ,cl)
// TEMPLATE: WON 0x007cd5a0
// (protected: void __thiscall std::list<class WONAPI::ServerDetailsItem *,class std::allocator<class WONAPI::ServerDetailsItem *> >::_Incsize(unsigned in)
// TEMPLATE: WON 0x007cd6f0
// (protected: void __thiscall std::list<struct std::pair<class WONAPI::ListItem *,int>,class std::allocator<struct std::pair<class WONAPI::ListItem *,int)
// TEMPLATE: WON 0x007ce850
// (public: void * __thiscall std::pair<class WONAPI::GUIString const ,struct WONAPI::ServerListCtrl::ColumnInfo>::`scalar deleting destructor'(unsigned i)

// Dynamic initializer and exit handler of mColumnMap:
// TEMPLATE: WON 0x007c4b70
// TEMPLATE: WON 0x007c4b80
// TEMPLATE: WON 0x007c4b90
// TEMPLATE: WON 0x007c4ba0
// TEMPLATE: WON 0x007cbf50
// (public: __thiscall std::pair<class WONAPI::GUIString const ,struct WONAPI::ServerListCtrl::ColumnInfo>::~pair<class WONAPI::GUIString const ,struct WO)
// TEMPLATE: WON 0x007c4bc0
// (public: __thiscall std::map<class WONAPI::GUIString,struct WONAPI::ServerListCtrl::ColumnInfo,struct WONAPI::GUIStringLessNoCase,class std::allocator<)
// TEMPLATE: WON 0x007cc350
// (public: class std::_Tree<class WONAPI::SmartPtr<class WONAPI::LobbyGame>,struct std::pair<class WONAPI::SmartPtr<class WONAPI::LobbyGame> const ,class)
