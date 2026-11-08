// Room and server list controls. RoomLabelCtrl (0xa0 bytes): the label naming the current room.

#ifndef EE_WONAPI_WONLOBBYROOMSERVER_H
#define EE_WONAPI_WONLOBBYROOMSERVER_H
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyScreens.h"
#include "ee/wonapi/WONLobbyPersistence.h" // GUIStringLessNoCase, for the static column map
namespace WONAPI {
class RoomLabelCtrl : public Container {
public:
    RoomLabelCtrl(); // 0x7c32b0
    // Implicit destructor (0x7c33a0): it does not write its own vtable pointer.
    virtual void InitComponent(ComponentInit&);
    virtual void HandleComponentEvent(ComponentEvent*);
    void CalcLabel();
    void SetPrefix(const GUIString& inPrefix) { this->mPrefix=inPrefix; }
    void SetRoomSpecFlags(int inFlags) { this->mRoomSpecFlags=inFlags; }
    void SetShowUserCount(bool inShow) { if(!inShow) this->mUserCt=-1; else if(this->mUserCt<0) this->mUserCt=0; }
    void SetShowServer(bool inShow) { this->mShowServer=inShow; }
    int mRoomSpecFlags; // 0x84
    SmartPtr<Label> mLabel; // 0x88
    GUIString mPrefix; // 0x8c
    GUIString mServer; // 0x90
    GUIString mRoom; // 0x94
    int mUserCt; // 0x98
    bool mShowServer; // 0x9c
};
}
// Widgets used by RootScreen.
namespace WONAPI {
// MSSeperator: 0x30 bytes, a Component with no members of its own.
class MSSeperator : public Component {
public:
    MSSeperator(int=6); // 0x7006a0
    virtual void GetDesiredSize(int& width,int& height) { width=100; height=6; } // inline as in the SDK (0x700700)
    virtual void Paint(Graphics&); // 0x700720, slot 0x18
};
class RectangleComponent3D : public Component {
public:
    RectangleComponent3D(bool up=true,int thickness=2); // 0x7068f0; object size 0x44
    virtual void Paint(Graphics&); // 0x706930, slot 0x18
    // Implicit destructor: nothing of its own to free.
    int mSolidColor,mLightColor,mDarkColor; // 0x30, 0x34, 0x38 (set at 0x794c3b, 0x794c5f, 0x794c83)
    int mThickness; // 0x3c (set by RootScreen at 0x7c47fa)
    bool mUp; // 0x40 (set at 0x794ca7)
    void SetSolidColor(int c) { this->mSolidColor=c; }
    void SetLightColor(int c) { this->mLightColor=c; }
    void SetDarkColor(int c) { this->mDarkColor=c; }
    void SetThickness(int n) { this->mThickness=n; }
    void SetIsUp(bool up) { this->mUp=up; }
};
}

// RootScreen (0xcc bytes): the internet or LAN lobby screen with its tabs (lobby, game, direct connect,
// LAN, help, news).
namespace WONAPI {
class RootScreen : public Container {
public:
    RootScreen(bool);
    virtual ~RootScreen(); // writes its own vtable pointer (0x7c3d2f); frees ten members, then the base
    virtual void InitComponent(ComponentInit&); // 0x7c3f40
    virtual void Virtual08(); // 0x7c46d0; slot 0x08 (SizeChanged)
    virtual void HandleComponentEvent(ComponentEvent*); // 0x7c4ae0
    bool OnDirectConnectTab(); // 0x7c3ec0
    bool OnLanTab(); // 0x7c3f00
    void EnterGameTab(bool); // 0x7c4880
    void ShowLobbyTab(); // 0x7c4930
    void HandleJoinedGame(); // 0x7c4950
    void HandleLeftGame(); // 0x7c4980
    void HandleEnteredScreen(); // 0x7c4990
    void HandleTabCtrlTabSelected(ComponentEvent*); // 0x7c49d0
    bool mIsLan; // 0x84
    SmartPtr<TabCtrl> mTabCtrl; // 0x88
    GUIString mDefaultTabName; // 0x8c
    SmartPtr<GameBrowserCtrl> mHelpTab; // 0x90
    SmartPtr<GameBrowserCtrl> mNewsTab; // 0x94
    SmartPtr<Component> mLowerLayoutPoint; // 0x98
    SmartPtr<MSSeperator> mLowerDivider; // 0x9c
    SmartPtr<Component> mSierraLogo; // 0xa0
    SmartPtr<Component> mLowerBanner; // 0xa4
    SmartPtr<RectangleComponent3D> mLowerBannerFrame; // 0xa8
    SmartPtr<Component> mBackButton; // 0xac
    bool mHideLowerControls; // 0xb0
    bool mIsSwat; // 0xb1
    int mLastJoinTab; // 0xb4
    int mGameTab; // 0xb8, not initialized by the constructor
    int mDirectConnectTab; // 0xbc
    int mLanTab; // 0xc0
    int mLobbyTab; // 0xc4
    int mRoomSpecFlags; // 0xc8
};
}

// RoomCtrl: the room list (servers and their chat rooms) with Join and Create buttons.
namespace WONAPI {
class ServerChangedEvent;
class GroupChangedEvent;
// TreeItem and TreeArea are declared in WONLobbyGUI.h.
class RoomCtrl : public Container {
public:
    RoomCtrl();
    // Implicit destructor: it does not write its own vtable pointer.
    virtual void InitComponent(ComponentInit&);
    virtual void HandleComponentEvent(ComponentEvent*);
    void EnableControls();
    void DoJoinRoom();
    void DoCreateRoom();
    TreeItem* GetServerItem(const IPAddr&);
    TreeItem* GetServerItem(LobbyServer*);
    TreeItem* GetGroupItem(const IPAddr&,int);
    TreeItem* GetGroupItem(LobbyServer*,LobbyGroup*);
    void AddServer(LobbyServer*,bool,int);
    void AddGroup(TreeItem*,LobbyServer*,LobbyGroup*);
    void HandleSetRoomList();
    void HandleServerChanged(ServerChangedEvent*);
    void HandleGroupChanged(GroupChangedEvent*);
    void HandleItemOpened(TreeItemOpenedEvent*);
    SmartPtr<TreeArea> mRoomList; // 0x84
    SmartPtr<Button> mJoinButton; // 0x88
    SmartPtr<Button> mCreateButton; // 0x8c
}; // 0x90 bytes; Container is 0x84 bytes with 51 vtable slots
}

// One row of the room list (a server or a room); global namespace, not WONAPI (RTTI .?AVRoomListItem@@).
// 0x58 bytes; the ListItem base is 0x18 bytes with 14 vtable slots.
class RoomListItem : public WONAPI::ListItem {
public:
    RoomListItem(WONAPI::LobbyServer*,bool); // 0x7c19a0
    RoomListItem(WONAPI::LobbyGroup*,WONAPI::LobbyServer*); // 0x7c1c50: the group comes first
    // Implicit destructor (0x7c1b70): it does not write its own vtable pointer.
    virtual void Paint(WONAPI::Graphics&,WONAPI::ListArea*);     // 0x7c20c0
    virtual void CalcDimensions(WONAPI::ListArea*); // 0x7c1fd0
    virtual int Compare(const WONAPI::ListItem*,WONAPI::ListArea*) const;   // 0x7c20a0
    void UpdateText(WONAPI::LobbyServer*,WONAPI::LobbyGroup*);
    int mColor; // 0x18
    bool mLoading; // 0x1c
    WONAPI::SmartPtr<WONAPI::Image> mImage; // 0x20
    WONAPI::SmartPtr<WONAPI::Font> mFont; // 0x24
    WONAPI::GUIString mText; // 0x28
    WONAPI::IPAddr mAddress; // 0x2c
    unsigned long mGroupId; // 0x54, the group id (zero-extended), 0xffff on a server row
};


// ServerListCtrl (0xc8 bytes): the game list with filter combos and sortable columns.
// The column vector (0x84) and the details map (0xbc) own their elements.
namespace WONAPI {
class ComboSelChangedEvent;
class GameChangedEvent;
class SyncGameListEvent;
class ServerListCtrl : public Container {
public:
    struct ColumnInfo {
        typedef void (*PaintFunc)(Graphics&,ServerDetailsItem*,ServerListCtrl*);
        typedef int (*CompareFunc)(const ServerDetailsItem*,const ServerDetailsItem*,ServerListCtrl*);
        typedef void (*FilterFunc)(const GUIString&,std::list<ServerDetailsItem*>&,ServerListCtrl*);
        PaintFunc mPaintFunc; // 0x00
        CompareFunc mCompareFunc; // 0x04
        FilterFunc mFilterFunc; // 0x08
        SmartPtr<ComboBox> mFilterCombo; // 0x0c
        unsigned char mGameTypeFlags; // 0x10
        GUIString mColumnName; // 0x14
        ColumnInfo(); // 0x7c5f80
        ~ColumnInfo(); // 0x7c5ef0
    }; // 0x18 bytes
    typedef std::vector<ColumnInfo> ColumnInfoVector;
    typedef std::map<GUIString,ColumnInfo,GUIStringLessNoCase> ColumnMap;
    typedef std::list<ServerDetailsItem*> ServerDetailsList;
    typedef std::map<SmartPtr<LobbyGame>,SmartPtr<ServerDetailsItem> > ServerDetailsMap;
    ServerListCtrl(); // 0x7c5c10
    virtual ~ServerListCtrl(); // 0x7c5d30; explicit: writes its own vtable pointer
    virtual void InitComponent(ComponentInit&); // 0x7c6360
    virtual bool TimerEvent(int); // 0x7c66d0
    virtual void HandleComponentEvent(ComponentEvent*); // 0x7c80f0
    void LoadFilterCombos(); // 0x7c6740
    void FilterInProgressGames(ServerDetailsList&); // 0x7c6a70
    void DoFilter(ServerDetailsList&); // 0x7c6ad0
    bool FitsFilter(ServerDetailsItem*); // 0x7c6be0
    void UpdateJoinButton(); // 0x7c6d00
    void UpdateSummaryLabel(); // 0x7c6d50
    void AddEntries(const ServerDetailsList&); // 0x7c6ff0
    void AddFilteredEntries(bool,ServerDetailsList&); // 0x7c7090
    void AddFilteredEntries_All(bool); // 0x7c7220
    void AddFilteredEntries_List(bool); // 0x7c73c0
    LobbyGame* GetSelGame(); // 0x7c7580
    void HandleGameChanged(GameChangedEvent*); // 0x7c75a0
    void HandleSyncGameList(SyncGameListEvent*); // 0x7c7870
    void HandleComboTextChanged(Component*); // 0x7c7b30
    void HandleComboSelChanged(ComboSelChangedEvent*); // 0x7c7dd0
    bool HandleRefreshButton(); // 0x7c80c0
    void HandleListItemClicked(ListItemClickedEvent*); // 0x7c7a50
    void SetNumColumns(int); // 0x7c8340; stores the count and resizes the column vector
    static ColumnInfo* GetColumnInfo(const GUIString&); // 0x7c5ff0
    static void InitColumnMap(); // 0x7c6050
    static void AddDefaultFilters(const GUIString&,const GUIString&,bool); // 0x7c6340
    ColumnInfoVector mColumnInfo; // 0x84..0x93
    void SetGameType(int inType) { this->mGameType=inType; }
    int mGameType; // 0x94
    bool mNeedFilter; // 0x98
    int mFilterTimeAccum; // 0x9c
    int mNumColumns; // 0xa0
    SmartPtr<Font> mBoldFont; // 0xa4
    SmartPtr<MultiListArea> mList; // 0xa8
    SmartPtr<Button> mJoinButton; // 0xac
    SmartPtr<Button> mRefreshButton; // 0xb0
    SmartPtr<Component> mRemoveButton; // 0xb4
    SmartPtr<Label> mSummaryLabel; // 0xb8
    ServerDetailsMap mServerDetailsMap; // 0xbc..0xc7
    static ColumnMap mColumnMap; // 0x966b60
};
}

#endif
