// The lobby's rows as Rust rows (R6 WONLobby round 16): the original RoomListItem (RoomCtrl.cpp), FriendsListItem
// (FriendsListCtrl.cpp), UserListItem (UserListCtrl.cpp), GameUserListItem (GameUserListCtrl.cpp) and the members of
// ServerDetailsItem (its virtuals were forwards to the list's control, srvlistctl.rs, which won_srvlist_test and the
// LAN game cover), and the
// controls' host cases that made and read them (LobbyNetCtrlHost.cpp and LobbyCtrlHost.cpp before the move), their
// statements copied unchanged over a stand-in world of lists, graphics contexts, fonts, images, texts, addresses and
// model objects, against ee_won_lobby's rows.rs over the same world through the controls' host ops (the world's
// shells are ee_won_gui's RustRow and RustTextRow, copied from WONLobbyGUI.h).
//
// Each sequence picks a row kind, makes up to four rows of it and runs 1 to 30 random steps: the controls' row ops
// (make, update the text, read and write the members, set the friend, client and icon, the client tests), the rows'
// virtuals (CalcDimensions and Paint over a list with random fonts, colours, widths, flags, column and selection;
// Compare of two rows), changes of the model's answers, and the end of a row. The world logs every draw, every
// reference taken and given back and every value read; the traces must be the same. The releases a row's end makes
// are compared as a set (the shell's TextListItem base gives its image back before its font; the original's own
// members the other way round).
//
// Self-checking (both sides run in each sequence): `--print` prints the first 60 sequences and every later one that
// reaches a trace word none before did.
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <string>
#include <vector>
#include <set>
#include <map>
#include <algorithm>
#include "rust/won_gui/rowobj.h"
#include "rust/won_lobby/lobbyctl.h"
#include "rust/won_lobby/modelobj.h"
#include "rust/won_lobby/resource.h"
#include "rust/won_lobby/rows.h"

typedef EeWonLobbyCtlOp Op;
typedef EeWonLobbyCtlItemField F;
typedef EeWonModelField MF;
typedef EeWonModelCall MC;
typedef EeWonLobbyRes Res;

static int sFailures;
static int sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static unsigned sSeed;
static unsigned R(unsigned n)
{
    sSeed = sSeed * 1103515245u + 12345u;
    return ((sSeed >> 16) & 0x7fff) % n;
}

static std::vector<std::string> sTrace;
static void Line(const char* inFormat, long long a = 0, long long b = 0, long long c = 0, long long d = 0)
{
    char aBuffer[300];
    snprintf(aBuffer, sizeof(aBuffer), inFormat, a, b, c, d);
    sTrace.push_back(aBuffer);
}
static void LineS(const char* inFormat, const std::string& s, long long a = 0, long long b = 0)
{
    char aBuffer[300];
    snprintf(aBuffer, sizeof(aBuffer), inFormat, s.c_str(), a, b);
    sTrace.push_back(aBuffer);
}

// ---- the world

struct RefCount {
    int mRefs = 0;
    int mId = 0;
    bool mTraced = true;
    virtual ~RefCount() {}
    RefCount* CreateRef() { this->AddRef(); return this; }
    void AddRef() { this->mRefs++; if (this->mTraced) Line("addref %lld", this->mId); }
    void Release()
    {
        if (this->mTraced) Line("release %lld", this->mId);
        if (--this->mRefs == 0 && !this->mTraced) delete this;
    }
};

// the original's ConstSmartPtr: the old object released, the new one referenced, when they differ
template <class T> class SmartPtr {
public:
    SmartPtr() : mPtr(NULL) {}
    SmartPtr(T* p) : mPtr(p ? (T*)p->CreateRef() : NULL) {}
    SmartPtr(const SmartPtr& o) : mPtr(o.mPtr ? (T*)o.mPtr->CreateRef() : NULL) {}
    ~SmartPtr() { if (this->mPtr != NULL) this->mPtr->Release(); }
    T* operator=(T* p)
    {
        if (this->mPtr != p) {
            if (this->mPtr != NULL) this->mPtr->Release();
            this->mPtr = p ? (T*)p->CreateRef() : NULL;
        }
        return p;
    }
    SmartPtr& operator=(const SmartPtr& o) { this->operator=(o.mPtr); return *this; }
    T* operator->() const { return this->mPtr; }
    T* get() const { return this->mPtr; }
    T* mPtr;
};

struct GUIString {
    std::string s;
    GUIString() {}
    GUIString(const std::string& t) : s(t) {}
    int length() const { return (int)this->s.size(); }
    void append(const char* t) { this->s += t; }
    void erase() { this->s.clear(); }
    int CompareNoCase(const GUIString& o) const
    {
        size_t i = 0;
        int r = 0;
        for (; r == 0 && i < this->s.size() && i < o.s.size(); i++)
            r = tolower((unsigned char)this->s[i]) - tolower((unsigned char)o.s[i]);
        if (r == 0) r = (int)this->s.size() - (int)o.s.size();
        return r < 0 ? -1 : r > 0 ? 1 : 0;
    }
};

// the texts: a data handle is 0x7000 + its index
static std::vector<std::string> sTexts;
static intptr_t TextData(const std::string& t)
{
    for (size_t i = 0; i < sTexts.size(); i++)
        if (sTexts[i] == t) return 0x7000 + (intptr_t)i;
    sTexts.push_back(t);
    return 0x7000 + (intptr_t)sTexts.size() - 1;
}
static GUIString Txt(intptr_t d) { return d ? GUIString(sTexts[d - 0x7000]) : GUIString(); }

struct Font : RefCount {
    int mHeight;
    int GetHeight() { return this->mHeight; }
    int GetStringWidth(const GUIString& t, int, int n) { return n * 5 + this->mId % 7 + (int)t.s.size() % 3; }
};
struct Image : RefCount {
    unsigned short mWidth, mHeight;
    unsigned short GetHeight() const { return this->mHeight; }
};
static Font sFonts[6];
static Image sImages[600]; // one per resource index (the images' owner holds them)
static Image* ResImage(intptr_t r) { return &sImages[r]; }
static GUIString ResText(intptr_t r) { return GUIString("res" + std::to_string((long long)r)); }

struct Graphics {
    unsigned long GetStandardColor(int c) { return 0x100 + (unsigned long)c; }
    void SetColor(unsigned long c) { Line("g color %llx", (long long)c); }
    void SetSysColor(int c) { Line("g syscolor %lld", c); }
    void SetFont(Font* f) { Line("g font %lld", f ? f->mId : 0); }
    void FillRect(int x, int y, int w, int h) { Line("g fill %lld %lld %lld %lld", x, y, w, h); }
    void DrawLine(int x, int y, int u, int v) { Line("g line %lld %lld %lld %lld", x, y, u, v); }
    void DrawImage(Image* i, int x, int y) { Line("g image %lld %lld %lld", i ? i->mId : 0, x, y); }
    void DrawString(const GUIString& t, int x, int y) { LineS("g text '%s' %lld %lld", t.s, x, y); }
};
static Graphics sGraphics;
static void DrawLobbyPing(Graphics*, unsigned short inPing, int inHeight, int, int) { Line("g ping %lld %lld", inPing, inHeight); }

struct SelectionColor {
    unsigned long mBack = 0x55;
    unsigned long GetBackgroundColor(Graphics&) const { return this->mBack; }
    unsigned long GetForegroundColor(Graphics&, unsigned long c) const { return c ^ 0xff00; }
};

class ListItem;
struct ListArea {
    SmartPtr<Font> mFont;
    int mTextColor = 0x11, mPaintWidth = 80, mTotalWidth = 60, mFlags = 0, mColumn = -1;
    SelectionColor mSelColor;
    int GetTextColor() { return this->mTextColor; }
    int GetPaintWidth() { return this->mPaintWidth; }
    int GetTotalWidth() { return this->mTotalWidth; }
    bool ListFlagSet(int f) { return (this->mFlags & f) != 0; }
    const SelectionColor& GetSelColor() { return this->mSelColor; }
    Font* GetFontMod(Font* f, int, int) { return &sFonts[f ? (f->mId % 3) + 3 : 5]; }
    int GetColumnPaintContext() { return this->mColumn; }
};
typedef ListArea MultiListArea;
static ListArea sList;

static int sSeq; // the side's row count (a row's id in the trace)
class ListItem : public RefCount {
public:
    ListItem() { this->mTraced = false; this->mId = ++sSeq; }
    virtual void Paint(Graphics&, ListArea*) {}
    virtual void CalcDimensions(ListArea*) {}
    virtual int Compare(const ListItem* o, ListArea*) const { return this < o; }
    virtual bool CanSelect(ListArea*, int, int) { return this->mSelectable; }
    virtual void MouseDown(ListArea*, int, int, int) {}
    bool mSelected = false;
    bool mSelectable = true;
    int mTop = 0, mWidth = 0, mHeight = 0;
};
// the original's TextListItem (TextListItem.cpp's statements over the world)
class TextListItem : public ListItem {
public:
    TextListItem() : mColor(-1) {}
    virtual void CalcDimensions(ListArea* l)
    {
        Line("base calc %lld", this->mId);
        if (this->mFont.get() == NULL) this->mFont = l->mFont;
        this->mWidth = this->mFont->GetStringWidth(this->mText, 0, this->mText.length());
        this->mHeight = this->mFont->GetHeight();
    }
    virtual int Compare(const ListItem* o, ListArea*) const
    {
        return this->mText.CompareNoCase(static_cast<const TextListItem*>(o)->mText);
    }
    virtual void Paint(Graphics& g, ListArea*) { LineS("base paint '%s' %lld", this->mText.s, this->mId); g.SetFont(this->mFont.get()); }
    GUIString mText;
    long mColor;
    SmartPtr<Font> mFont;
    SmartPtr<Image> mImage;
};

// the model: objects with answers that change between steps
struct IPAddr {
    int mValue;
    static int sLive;
    IPAddr(int v = 0) : mValue(v) { sLive++; }
    IPAddr(const IPAddr& o) : mValue(o.mValue) { sLive++; }
    ~IPAddr() { sLive--; }
    const IPAddr* GetValue() const { return this; }
};
int IPAddr::sLive;
struct ModelObj : RefCount {
    unsigned mSalt = 0;
    unsigned Answer(unsigned f) const
    {
        unsigned x = (unsigned)this->mId * 2654435761u + f * 0x9e3779b1u + this->mSalt;
        x ^= x >> 15;
        x *= 0x85ebca6bu;
        x ^= x >> 13;
        return x;
    }
};
struct LobbyServer : ModelObj {
    IPAddr mAddr;
    IPAddr GetAddress() const { return this->mAddr; }
    GUIString GetName() const { return Txt(this->NameData()); }
    intptr_t NameData() const { return TextData(std::string(this->Answer(1) & 1 ? "Srv" : "srv") + std::to_string(this->mId)); }
    int GetNumClients() { return (int)(this->Answer(2) % 4); }
};
struct LobbyGroup : ModelObj {
    unsigned short GetId() { return (unsigned short)this->Answer(3); }
    GUIString GetName() const { return Txt(this->NameData()); }
    intptr_t NameData() const { return TextData(std::string(this->Answer(4) & 1 ? "Room" : "room") + std::to_string(this->mId % 5)); }
    unsigned long GetNumClients() const { return this->Answer(5) % 4 ? this->Answer(5) % 5 : 0xfffffffeu; }
    intptr_t Kind() { return this->Answer(6) % 8; }
    bool IsPasswordProtected() { return (this->Kind() & 1) != 0; }
};
struct LobbyClient : ModelObj {
    GUIString GetName() const { return Txt(this->NameData()); }
    intptr_t NameData() const { return TextData(std::string(this->Answer(7) & 1 ? "Bob" : "bob") + std::to_string(this->mId % 4)); }
    bool B(unsigned f) const { return (this->Answer(f) & 3) == 0; }
    bool IsCaptain(bool) { return this->B(8); }
    bool IsReady() { return this->B(9); }
    unsigned short GetPing() { return (unsigned short)(this->Answer(10) % 7 == 0 ? 0xffff : this->Answer(10) % 900); }
    bool IsIgnored() const { return this->B(11); }
    bool IsBlocked() const { return this->B(12); }
    bool IsMuted(bool) { return this->B(13); }
    bool IsAway() { return this->B(14); }
    bool IsAdmin() { return this->B(15); }
    bool IsModerator() { return this->B(16); }
    bool HasPlayer() { return this->B(17); }
};
struct LobbyFriend : ModelObj {};
struct LobbyGame : ModelObj {
    IPAddr GetAddress() const { return IPAddr(600 + this->mId + (int)(this->Answer(18) % 3)); }
};
// the address's text (IPAddr::GetHostAndPortString)
static GUIString HostAndPort(const IPAddr& inAddr, bool) { return GUIString("10.0.0." + std::to_string((long long)inAddr.mValue) + ":8585"); }
static LobbyServer sServers[3];
static LobbyGroup sGroups[4];
static LobbyClient sClients[4];
static LobbyFriend sFriends[3];
static LobbyGame sGames[3];

template <class T> static T* Find(T* inArray, int inCount, intptr_t p)
{
    for (int i = 0; i < inCount; i++)
        if ((intptr_t)&inArray[i] == p) return &inArray[i];
    return NULL;
}
static ModelObj* AnyModel(intptr_t p)
{
    if (ModelObj* m = Find(sServers, 3, p)) return m;
    if (ModelObj* m = Find(sGroups, 4, p)) return m;
    if (ModelObj* m = Find(sClients, 4, p)) return m;
    if (ModelObj* m = Find(sGames, 3, p)) return m;
    return Find(sFriends, 3, p);
}
static long long Id(const void* p)
{
    if (p == NULL) return 0;
    if (ModelObj* m = AnyModel((intptr_t)p)) return m->mId;
    return -1;
}

#define LOBBY_STRING(x) ResText((intptr_t)Res::x)
// the resources' images are SmartPtrs the resource table holds (LOBBY_IMAGE is a reference to one)
static SmartPtr<Image> sImagePtrs[600];
#define LOBBY_IMAGE(x) sImagePtrs[(intptr_t)Res::x]
enum { StandardColor_Text = 14 };

// ---- the original rows (their statements unchanged: RoomCtrl.cpp's, FriendsListCtrl.cpp's, UserListCtrl.cpp's,
// GameUserListCtrl.cpp's as LobbyNetCtrlHost.cpp and LobbyCtrlHost.cpp kept them)

namespace Orig {
class RoomListItem : public ListItem {
public:
    RoomListItem(LobbyServer*,bool);
    RoomListItem(LobbyGroup*,LobbyServer*);
    virtual void Paint(Graphics&,ListArea*);
    virtual void CalcDimensions(ListArea*);
    virtual int Compare(const ListItem*,ListArea*) const;
    void UpdateText(LobbyServer*,LobbyGroup*);
    int mColor;
    bool mLoading;
    SmartPtr<Image> mImage;
    SmartPtr<Font> mFont;
    GUIString mText;
    IPAddr mAddress;
    unsigned long mGroupId;
};
// Creates a room tree entry for a server, or its 'querying server' placeholder when loading
RoomListItem::RoomListItem(LobbyServer* inServer,bool inLoading)
    :mColor(-1),mLoading(inLoading),mAddress(inServer->GetAddress()),mGroupId(0xffff)
{
    this->UpdateText(inServer,NULL);
}
// Creates a room tree entry for a chat room on a server
RoomListItem::RoomListItem(LobbyGroup* inGroup,LobbyServer* inServer)
    :mColor(-1),mLoading(false),mAddress(inServer->GetAddress()),mGroupId(inGroup->GetId())
{
    this->UpdateText(inServer,inGroup);
}
// Orders room tree entries by their text, ignoring case
int RoomListItem::Compare(const ListItem* inOther,ListArea*) const
{
    return this->mText.CompareNoCase(static_cast<const RoomListItem*>(inOther)->mText);
}
// Sets the entry's text ('querying server', or the room or server name with its player count) and lock icon
void RoomListItem::UpdateText(LobbyServer* inServer,LobbyGroup* inGroup)
{
    char aCount[50];
    if(this->mLoading) this->mText=LOBBY_STRING(RoomCtrl_QueryingServer_String);
    else if(inGroup!=NULL)
    {
        sprintf(aCount," (%d)",inGroup->GetNumClients());
        this->mText=inGroup->GetName();
        if(static_cast<int>(inGroup->GetNumClients())>0)
            this->mText.append(aCount);
    }
    else if(inServer!=NULL)
    {
        sprintf(aCount," (%d)",inServer->GetNumClients());
        this->mText=inServer->GetName();
        if(inServer->GetNumClients()!=0)
            this->mText.append(aCount);
    }
    else this->mText.erase();
    if(inGroup!=NULL && inGroup->IsPasswordProtected()) this->mImage=LOBBY_IMAGE(RoomCtrl_Password_Image);
    else this->mImage=NULL;
}
// Measures the entry: server entries use a bold font, and the lock icon adds width and height
void RoomListItem::CalcDimensions(ListArea* inList)
{
    if(this->mFont.get()==NULL)
    {
        if(this->mGroupId==0xffff && !this->mLoading)
            this->mFont=inList->GetFontMod(inList->mFont.get(),1,0);
        else this->mFont=inList->mFont;
    }
    if(this->mColor<0) this->mColor=inList->GetTextColor();
    this->mWidth=this->mFont->GetStringWidth(this->mText,0,this->mText.length())+2;
    this->mHeight=this->mFont->GetHeight();
    if(this->mImage.get()!=NULL)
    {
        this->mWidth+=this->mImage->mWidth+2;
        if(this->mImage->GetHeight()>this->mHeight) this->mHeight=this->mImage->GetHeight();
    }
}
// Draws the entry with its selection highlight, lock icon and text
void RoomListItem::Paint(Graphics& g,ListArea* inList)
{
    int aColor=this->mColor;
    if(this->mSelected)
    {
        g.SetColor(inList->GetSelColor().GetBackgroundColor(g));
        int aWidth=inList->GetPaintWidth();
        if(!inList->ListFlagSet(0x10)) aWidth=this->mWidth;
        g.FillRect(0,0,aWidth,this->mHeight);
        aColor=inList->GetSelColor().GetForegroundColor(g,this->mColor);
    }
    int x=2;
    if(this->mImage.get()!=NULL)
    {
        g.DrawImage(this->mImage.get(),x,(this->mHeight-this->mImage->mHeight)/2);
        x=this->mImage->mWidth+4;
    }
    g.SetColor(aColor);
    g.SetFont(this->mFont.get());
    g.DrawString(this->mText,x,(this->mHeight-this->mFont->GetHeight())/2);
}
class FriendsListItem : public TextListItem {
public:
    FriendsListItem();
    virtual void Paint(Graphics& inGraphics,ListArea* inList);
    virtual void CalcDimensions(ListArea* inList);
    virtual int Compare(const ListItem* inOther,ListArea* inList) const;
    SmartPtr<LobbyFriend> mFriend;
    bool mSeparator;
    signed char mSortGroup;
};
// Creates a friends list entry with no friend (a separator line until a friend is set)
FriendsListItem::FriendsListItem() { this->mSeparator=true; this->mSortGroup=0; }
// Sizes a separator (8 pixels high when shown, else 1) or a friend's text entry
void FriendsListItem::CalcDimensions(ListArea* inList)
{
    if(this->mFriend.get()==NULL) {
        this->mWidth=1;
        if(this->mSeparator) this->mHeight=8;
        else this->mHeight=1;
    } else TextListItem::CalcDimensions(inList);
}
// Orders entries by sort group, highest first (online friends, separator, others), then by text
int FriendsListItem::Compare(const ListItem* inOther,ListArea* inList) const
{
    int result=static_cast<const FriendsListItem*>(inOther)->mSortGroup-this->mSortGroup;
    if(result!=0) return result;
    return TextListItem::Compare(inOther,inList);
}
// Draws a shown separator as a line across the list, or a friend's text entry
void FriendsListItem::Paint(Graphics& inGraphics,ListArea* inList)
{
    if(this->mFriend.get()==NULL) {
        if(this->mSeparator) {
            int width=inList->GetTotalWidth()+10;
            if(inList->GetPaintWidth()>width) width=inList->GetPaintWidth();
            int y=this->mHeight>>2;
            inGraphics.SetSysColor(StandardColor_Text);
            inGraphics.DrawLine(0,y,width,y);
        }
    } else TextListItem::Paint(inGraphics,inList);
}
class UserListItem : public TextListItem {
public:
    UserListItem();
    virtual void Paint(Graphics&,ListArea*);
    virtual void CalcDimensions(ListArea*);
    virtual int Compare(const ListItem*,ListArea*) const;
    SmartPtr<LobbyClient> mClient;
    signed char mSortKey;
    bool mShowSeparator;
};
// Creates a room user list entry with no client (a separator line until a client is set)
UserListItem::UserListItem() { this->mSortKey=0; this->mShowSeparator=true; }
// Orders entries by sort key, highest first (admins, moderators, separator, friends, separator, others), then text
int UserListItem::Compare(const ListItem* inOther,ListArea* inArea) const
{
    const UserListItem* other=static_cast<const UserListItem*>(inOther);
    int result=other->mSortKey-this->mSortKey;
    if(result!=0) return result;
    return TextListItem::Compare(inOther,inArea);
}
// Sizes a separator (8 pixels high when shown, else 1) or a client's text entry
void UserListItem::CalcDimensions(ListArea* inArea)
{
    if(this->mClient.get()==NULL) {
        this->mWidth=1;
        if(this->mShowSeparator) this->mHeight=8;
        else this->mHeight=1;
    } else TextListItem::CalcDimensions(inArea);
}
// Draws a shown separator as a line across the list, or a client's text entry
void UserListItem::Paint(Graphics& inGraphics,ListArea* inArea)
{
    if(this->mClient.get()==NULL) {
        if(this->mShowSeparator) {
            int width=inArea->GetTotalWidth()+10;
            if(inArea->GetPaintWidth()>width) width=inArea->GetPaintWidth();
            int y=this->mHeight>>2;
            inGraphics.SetSysColor(StandardColor_Text);
            inGraphics.DrawLine(0,y,width,y);
        }
    } else TextListItem::Paint(inGraphics,inArea);
}
class GameUserListItem : public ListItem {
public:
    GameUserListItem(LobbyClient*);
    virtual void Paint(Graphics&,ListArea*);
    virtual void CalcDimensions(ListArea*);
    virtual int Compare(const ListItem*,ListArea*) const;
    SmartPtr<LobbyClient> mClient;
    SmartPtr<Image> mImage;
    SmartPtr<Font> mFont;
};
// Creates a game player list entry for a client
GameUserListItem::GameUserListItem(LobbyClient* inClient) : mClient(inClient) {}
// Sizes the entry to the list font's height, or the icon's if taller
void GameUserListItem::CalcDimensions(ListArea* inArea)
{
    this->mFont=inArea->mFont;
    this->mHeight=this->mFont->GetHeight();
    if(this->mImage.get()!=NULL && this->mImage->mHeight>this->mHeight) this->mHeight=this->mImage->mHeight;
}
// Orders the game captain first, then players by name ignoring case
int GameUserListItem::Compare(const ListItem* inOther,ListArea*) const
{
    const GameUserListItem* other=static_cast<const GameUserListItem*>(inOther);
    int mine=this->mClient->IsCaptain(1)?1:0;
    int theirs=other->mClient->IsCaptain(1)?1:0;
    if(mine!=theirs) return theirs-mine;
    return this->mClient->GetName().CompareNoCase(other->mClient->GetName());
}
// Draws one column of the entry: selection background, status icon, ready mark, name or ping bars
void GameUserListItem::Paint(Graphics& inGraphics,ListArea* inArea)
{
    MultiListArea* area=static_cast<MultiListArea*>(inArea);
    int column=area->GetColumnPaintContext();
    unsigned long color=inGraphics.GetStandardColor(StandardColor_Text);
    if(this->mSelected) color=area->GetSelColor().GetForegroundColor(inGraphics,color);
    inGraphics.SetColor(color);
    inGraphics.SetFont(this->mFont.get());
    switch(column) {
    case -1:
        if(this->mSelected) {
            inGraphics.SetColor(area->GetSelColor().GetBackgroundColor(inGraphics));
            inGraphics.FillRect(0,0,area->GetPaintWidth(),this->mHeight);
        }
        return;
    case 0:
        inGraphics.DrawImage(this->mImage.get(),0,(this->mHeight-this->mImage->mHeight)/2);
        return;
    case 1:
        if(this->mClient->IsReady())
            inGraphics.DrawImage(LOBBY_IMAGE(GameUserListCtrl_Ready_Image).get(),0,(this->mHeight-this->mImage->mHeight)/2);
        return;
    case 2:
        inGraphics.SetFont(this->mFont.get());
        inGraphics.DrawString(this->mClient->GetName(),0,(this->mHeight-this->mFont->GetHeight())/2);
        return;
    case 3:
        DrawLobbyPing(&inGraphics,this->mClient->GetPing(),this->mHeight-2,0,0);
        return;
    }
}

class ServerDetailsItem : public ListItem {
public:
    // the original left the icon and the summary height as new gave them; the Rust row starts them at 0
    ServerDetailsItem() { this->mSelectable = true; this->mIconId = 0; this->mSummaryHeight = 0; }
    virtual void PaintDetailsHook(Graphics&,MultiListArea*) {} // empty inline, vtable +0x38
    virtual int GetDetailsHeightHook(MultiListArea*) { return 0; } // returns 0, vtable +0x3c
    SmartPtr<LobbyGame> mGame; //18
    GUIString mIPAddrStr; //1c
    int mIconId; //20
    unsigned char mSummaryHeight; //24
    static ServerDetailsItem* CreateItem() { return new ServerDetailsItem; }
};

static const SmartPtr<Image>& CtlImage(intptr_t inImage)
{
    typedef EeWonLobbyCtlImage I;
    switch ((I)inImage) {
    case I::Ignored: return LOBBY_IMAGE(UserListCtrl_Ignored_Image);
    case I::Blocked: return LOBBY_IMAGE(UserListCtrl_Blocked_Image);
    case I::Muted: return LOBBY_IMAGE(UserListCtrl_Muted_Image);
    case I::Away: return LOBBY_IMAGE(UserListCtrl_Away_Image);
    case I::Admin: return LOBBY_IMAGE(UserListCtrl_Admin_Image);
    case I::Moderator: return LOBBY_IMAGE(UserListCtrl_Moderator_Image);
    case I::Captain: return LOBBY_IMAGE(UserListCtrl_Captain_Image);
    case I::Firewall: return LOBBY_IMAGE(LobbyGlobal_Firewall_Image);
    case I::Searching: return LOBBY_IMAGE(LobbyGlobal_Searching_Image);
    case I::Normal: break;
    }
    return LOBBY_IMAGE(UserListCtrl_Normal_Image);
}

static bool ClientIs(LobbyClient* inClient, intptr_t inTest)
{
    typedef EeWonLobbyCtlClientTest T;
    switch ((T)inTest) {
    case T::IsIgnored: return inClient->IsIgnored();
    case T::IsBlocked: return inClient->IsBlocked();
    case T::IsMuted: return inClient->IsMuted(true);
    case T::IsAway: return inClient->IsAway();
    case T::IsAdmin: return inClient->IsAdmin();
    case T::IsModerator: return inClient->IsModerator();
    case T::IsCaptain: return inClient->IsCaptain(1);
    case T::HasPlayer: return inClient->HasPlayer();
    }
    return false;
}

// the host's cases that made and read the rows (their statements unchanged)
static intptr_t HostOp(Op inOp, intptr_t a, intptr_t b, intptr_t c)
{
    RoomListItem* aRoom = reinterpret_cast<RoomListItem*>(a);
    TextListItem* aText = reinterpret_cast<TextListItem*>(a);
    GameUserListItem* anItem = reinterpret_cast<GameUserListItem*>(a);
    ListItem* inItem = reinterpret_cast<ListItem*>(a);
    ServerDetailsItem* aRow = reinterpret_cast<ServerDetailsItem*>(a);
    bool inSet = inOp == Op::ItemSet;
    intptr_t inValue = c;
    switch (inOp) {
    case Op::RoomItemServer: return (intptr_t) new RoomListItem(reinterpret_cast<LobbyServer*>(a), b != 0);
    case Op::RoomItemGroup: return (intptr_t) new RoomListItem(reinterpret_cast<LobbyGroup*>(a), reinterpret_cast<LobbyServer*>(b));
    case Op::RoomItemUpdate: aRoom->UpdateText(reinterpret_cast<LobbyServer*>(b), reinterpret_cast<LobbyGroup*>(c)); return 0;
    case Op::RoomItemAddr: return (intptr_t)aRoom->mAddress.GetValue();
    case Op::RoomItemGroupId: return (intptr_t)aRoom->mGroupId;
    case Op::FriendRowNew: return (intptr_t) new FriendsListItem;
    case Op::UserRowNew: return (intptr_t) new UserListItem;
    case Op::ItemFriend: return (intptr_t) static_cast<FriendsListItem*>(aText)->mFriend.get();
    case Op::ItemSetFriend: static_cast<FriendsListItem*>(aText)->mFriend = reinterpret_cast<LobbyFriend*>(b); return 0;
    case Op::ItemUserClient: return (intptr_t) static_cast<UserListItem*>(aText)->mClient.get();
    case Op::ItemSetUserClient: static_cast<UserListItem*>(aText)->mClient = reinterpret_cast<LobbyClient*>(b); return 0;
    case Op::ItemGet:
    case Op::ItemSet:
        switch ((F)b) {
        case F::FriendSeparator:
            if (inSet) static_cast<FriendsListItem*>(inItem)->mSeparator = inValue != 0;
            return static_cast<FriendsListItem*>(inItem)->mSeparator ? 1 : 0;
        case F::FriendSortGroup:
            if (inSet) static_cast<FriendsListItem*>(inItem)->mSortGroup = (signed char)inValue;
            return static_cast<FriendsListItem*>(inItem)->mSortGroup;
        case F::UserSortKey:
            if (inSet) static_cast<UserListItem*>(inItem)->mSortKey = (signed char)inValue;
            return static_cast<UserListItem*>(inItem)->mSortKey;
        case F::UserShowSeparator:
            if (inSet) static_cast<UserListItem*>(inItem)->mShowSeparator = inValue != 0;
            return static_cast<UserListItem*>(inItem)->mShowSeparator ? 1 : 0;
        case F::IconId: if (inSet) aRow->mIconId = (int)inValue; return aRow->mIconId;
        case F::SummaryHeight:
            if (inSet) aRow->mSummaryHeight = (unsigned char)inValue;
            return aRow->mSummaryHeight;
        default: return 0;
        }
    case Op::ItemNew: {
        GameUserListItem* aNew = new GameUserListItem(reinterpret_cast<LobbyClient*>(a));
        aNew->AddRef();
        return (intptr_t)aNew;
    }
    case Op::ItemClient: return (intptr_t)anItem->mClient.get();
    case Op::ItemClientIs: return ClientIs(anItem->mClient.get(), b) ? 1 : 0;
    case Op::ItemSetImage: anItem->mImage = CtlImage(b); return 0;
    case Op::RowNew: {
        ServerDetailsItem* aNew = ServerDetailsItem::CreateItem();
        aNew->AddRef();
        return (intptr_t)aNew;
    }
    case Op::RowGame: return (intptr_t)aRow->mGame.get();
    case Op::RowSetGame: aRow->mGame = reinterpret_cast<LobbyGame*>(b); return 0;
    case Op::RowSetAddrText: aRow->mIPAddrStr = HostAndPort(aRow->mGame->GetAddress(), true); return 0;
    case Op::RowAddrText: return TextData(aRow->mIPAddrStr.s);
    case Op::RowDetailsHeight: return aRow->GetDetailsHeightHook(reinterpret_cast<MultiListArea*>(b));
    case Op::RowPaintDetails:
        aRow->PaintDetailsHook(*reinterpret_cast<Graphics*>(b), reinterpret_cast<MultiListArea*>(c));
        return 0;
    default: return 0;
    }
}
}

// ---- the Rust side: the shells (WONLobbyGUI.h's RustRow and RustTextRow) and the controls' host ops over the world

class RustRow : public ListItem {
public:
    explicit RustRow(EeWonRow* r) : mRow(r) {}
    virtual ~RustRow() { ee_won_row_free(this->mRow); }
    virtual void Paint(Graphics& g,ListArea* l) { ee_won_row_call(this->mRow,EeWonRowCall::Paint,this,l,(intptr_t)&g,0,0); }
    virtual void CalcDimensions(ListArea* l) { ee_won_row_call(this->mRow,EeWonRowCall::CalcDimensions,this,l,0,0,0); }
    virtual int Compare(const ListItem* o,ListArea* l) const
    { return (int)ee_won_row_call(this->mRow,EeWonRowCall::Compare,const_cast<RustRow*>(this),l,(intptr_t)static_cast<const RustRow*>(o)->mRow,(intptr_t)o,(intptr_t)(static_cast<const ListItem*>(this)-o)); }
    virtual bool CanSelect(ListArea* l,int x,int y) { return ee_won_row_call(this->mRow,EeWonRowCall::CanSelect,this,l,x,y,0)!=0; }
    virtual void MouseDown(ListArea* l,int x,int y,int b) { ee_won_row_call(this->mRow,EeWonRowCall::MouseDown,this,l,x,y,b); }
    EeWonRow* mRow;
};
class RustTextRow : public TextListItem {
public:
    explicit RustTextRow(EeWonRow* r) : mRow(r) {}
    virtual ~RustTextRow() { ee_won_row_free(this->mRow); }
    virtual void Paint(Graphics& g,ListArea* l) { ee_won_row_call(this->mRow,EeWonRowCall::Paint,this,l,(intptr_t)&g,0,0); }
    virtual void CalcDimensions(ListArea* l) { ee_won_row_call(this->mRow,EeWonRowCall::CalcDimensions,this,l,0,0,0); }
    virtual int Compare(const ListItem* o,ListArea* l) const
    { return (int)ee_won_row_call(this->mRow,EeWonRowCall::Compare,const_cast<RustTextRow*>(this),l,(intptr_t)static_cast<const RustTextRow*>(o)->mRow,(intptr_t)o,0); }
    EeWonRow* mRow;
};

static int Lo(intptr_t v) { return (int)(int32_t)(v & 0xffffffff); }
static int Hi(intptr_t v) { return (int)(v >> 32); }
static std::set<int> sOpsSeen;

static intptr_t MGet(intptr_t a, MF f)
{
    switch (f) {
    case MF::ServerAddress: return (intptr_t)&Find(sServers, 3, a)->mAddr;
    case MF::ServerName: return Find(sServers, 3, a)->NameData();
    case MF::GroupId: return Find(sGroups, 4, a)->GetId();
    case MF::GroupName: return Find(sGroups, 4, a)->NameData();
    case MF::GroupNumClients: return (intptr_t)Find(sGroups, 4, a)->GetNumClients();
    case MF::ClientName: return Find(sClients, 4, a)->NameData();
    case MF::ClientIgnored: return Find(sClients, 4, a)->IsIgnored();
    case MF::ClientBlocked: return Find(sClients, 4, a)->IsBlocked();
    default: Line("unexpected mget %lld", (long long)f); return 0;
    }
}
static intptr_t MCall(intptr_t a, MC m, intptr_t)
{
    LobbyClient* cl = Find(sClients, 4, a);
    switch (m) {
    case MC::ServerNumClientsNow: return Find(sServers, 3, a)->GetNumClients();
    case MC::GroupKind: return Find(sGroups, 4, a)->Kind();
    case MC::ClientIsCaptain: return cl->IsCaptain(1);
    case MC::ClientIsReady: return cl->IsReady();
    case MC::ClientPing: return cl->GetPing();
    case MC::ClientMuted: return cl->IsMuted(true);
    case MC::ClientAway: return cl->IsAway();
    case MC::ClientAdmin: return cl->IsAdmin();
    case MC::ClientModerator: return cl->IsModerator();
    case MC::ClientHasPlayer: return cl->HasPlayer();
    default: Line("unexpected mcall %lld", (long long)m); return 0;
    }
}

static TextListItem* TItem(intptr_t a) { return static_cast<TextListItem*>(reinterpret_cast<ListItem*>(a)); }
static intptr_t World(void* inCtl, Op inOp, intptr_t a, intptr_t b, intptr_t c)
{
    if (inCtl != NULL) Line("a control %lld", (long long)(intptr_t)inCtl);
    sOpsSeen.insert((int)inOp);
    Graphics* g = reinterpret_cast<Graphics*>(a);
    ListArea* aList = reinterpret_cast<ListArea*>(a);
    ListItem* anItem = reinterpret_cast<ListItem*>(a);
    switch (inOp) {
    case Op::RowShellNew:
        if (b != 0) return (intptr_t) static_cast<ListItem*>(new RustTextRow(reinterpret_cast<EeWonRow*>(a)));
        return (intptr_t) static_cast<ListItem*>(new RustRow(reinterpret_cast<EeWonRow*>(a)));
    case Op::AddRef: if (a < 0x7000 || a >= 0x8000) reinterpret_cast<RefCount*>(a)->AddRef(); return 0; // texts are values here
    case Op::Release: if (a < 0x7000 || a >= 0x8000) reinterpret_cast<RefCount*>(a)->Release(); return 0;
    case Op::GameAddrText: return TextData(HostAndPort(Find(sGames, 3, a)->GetAddress(), true).s);
    case Op::ItemGet:
    case Op::ItemSet: {
        bool s = inOp == Op::ItemSet;
        switch ((F)b) {
        case F::Height: if (s) anItem->mHeight = (int)c; return anItem->mHeight;
        case F::Width: if (s) anItem->mWidth = (int)c; return anItem->mWidth;
        case F::Selected: if (s) anItem->mSelected = c != 0; return anItem->mSelected ? 1 : 0;
        case F::Selectable: if (s) anItem->mSelectable = c != 0; return anItem->mSelectable ? 1 : 0;
        case F::Color: if (s) TItem(a)->mColor = (long)c; return (intptr_t)TItem(a)->mColor;
        default: Line("unexpected field %lld", (long long)b); return 0;
        }
    }
    case Op::RowTextPaint: TItem(a)->TextListItem::Paint(*reinterpret_cast<Graphics*>(b), reinterpret_cast<ListArea*>(c)); return 0;
    case Op::RowTextCalc: TItem(a)->TextListItem::CalcDimensions(reinterpret_cast<ListArea*>(b)); return 0;
    case Op::RowTextCompare: return TItem(a)->TextListItem::Compare(reinterpret_cast<ListItem*>(b), reinterpret_cast<ListArea*>(c));
    case Op::ItemSetTextRes: TItem(a)->mText = ResText(b); return 0;
    case Op::ItemSetTextCount: {
        char aCount[50];
        sprintf(aCount, " (%d)", Lo(c));
        TItem(a)->mText = Txt(b);
        if (Hi(c) != 0) TItem(a)->mText.append(aCount);
        return 0;
    }
    case Op::ItemTextErase: TItem(a)->mText.erase(); return 0;
    case Op::ItemSetImageRes: TItem(a)->mImage = b < 0 ? NULL : ResImage(b); return 0;
    case Op::ItemImage: return (intptr_t)TItem(a)->mImage.get();
    case Op::ItemFont: return (intptr_t)TItem(a)->mFont.get();
    case Op::ItemSetFont: TItem(a)->mFont = reinterpret_cast<Font*>(b); return 0;
    case Op::ItemTextWidth: return reinterpret_cast<Font*>(b)->GetStringWidth(TItem(a)->mText, 0, TItem(a)->mText.length());
    case Op::ItemTextDraw: reinterpret_cast<Graphics*>(b)->DrawString(TItem(a)->mText, Lo(c), Hi(c)); return 0;
    case Op::ItemTextCompare: return TItem(a)->mText.CompareNoCase(TItem(b)->mText);
    case Op::ImageWidth: return reinterpret_cast<Image*>(a)->mWidth;
    case Op::ImageHeight: return reinterpret_cast<Image*>(a)->mHeight;
    case Op::ResImage: return (intptr_t)ResImage(a);
    case Op::FontMod: return (intptr_t)aList->GetFontMod(reinterpret_cast<Font*>(b), (int)c, 0);
    case Op::ListFont: return (intptr_t)aList->mFont.get();
    case Op::ListTextColor: return aList->GetTextColor();
    case Op::ListPaintWidth: return aList->GetPaintWidth();
    case Op::ListTotalWidth: return aList->GetTotalWidth();
    case Op::ListFlagSet: return aList->ListFlagSet((int)b) ? 1 : 0;
    case Op::ListPaintColumn: return static_cast<MultiListArea*>(aList)->GetColumnPaintContext();
    case Op::GfxSelFore: return (intptr_t)reinterpret_cast<ListArea*>(b)->GetSelColor().GetForegroundColor(*g, (unsigned long)c);
    case Op::GfxSelBack: return (intptr_t)reinterpret_cast<ListArea*>(b)->GetSelColor().GetBackgroundColor(*g);
    case Op::GfxSetColor: g->SetColor((unsigned long)b); return 0;
    case Op::GfxSetSysColor: g->SetSysColor((int)b); return 0;
    case Op::GfxSetFont: g->SetFont(reinterpret_cast<Font*>(b)); return 0;
    case Op::GfxFillRect: g->FillRect(Lo(b), Hi(b), Lo(c), Hi(c)); return 0;
    case Op::GfxDrawLine: g->DrawLine(Lo(b), Hi(b), Lo(c), Hi(c)); return 0;
    case Op::GfxDrawImage: g->DrawImage(reinterpret_cast<Image*>(b), Lo(c), Hi(c)); return 0;
    case Op::GfxDrawResImage: g->DrawImage(ResImage(b), Lo(c), Hi(c)); return 0;
    case Op::GfxDrawText: g->DrawString(Txt(b), Lo(c), Hi(c)); return 0;
    case Op::GfxStandardColor: return (intptr_t)g->GetStandardColor((int)b);
    case Op::GfxDrawPing: DrawLobbyPing(g, (unsigned short)b, (int)c, 0, 0); return 0;
    case Op::FontHeight: return reinterpret_cast<Font*>(a)->GetHeight();
    case Op::TextCompareNoCase: return Txt(a).CompareNoCase(Txt(b));
    case Op::MGet: return MGet(a, (MF)b);
    case Op::MCall: return MCall(a, (MC)b, c);
    case Op::AddrClone: return (intptr_t) new IPAddr(*reinterpret_cast<const IPAddr*>(a));
    case Op::AddrFree: delete reinterpret_cast<IPAddr*>(a); return 0;
    default: Line("unexpected op %lld", (long long)inOp); return 0;
    }
}

// ---- the sequences

enum Kind { kRoom, kFriend, kUser, kGameUser, kServer };
struct Step {
    int mWhat;
    intptr_t a, b, c;
};

static intptr_t Obj(unsigned k)
{
    switch (k % 4) {
    case 0: return (intptr_t)&sServers[k / 4 % 3];
    case 1: return (intptr_t)&sGroups[k / 4 % 4];
    case 2: return (intptr_t)&sClients[k / 4 % 4];
    default: return (intptr_t)&sFriends[k / 4 % 3];
    }
}

static void ResetWorld(unsigned inSalt)
{
    for (int i = 0; i < 6; i++) { sFonts[i].mId = 100 + i; sFonts[i].mHeight = 9 + i * 3; sFonts[i].mRefs = 1000; }
    for (int i = 0; i < 600; i++) {
        sImages[i].mId = 1000 + i;
        sImages[i].mWidth = (unsigned short)(8 + i % 9);
        sImages[i].mHeight = (unsigned short)(5 + i % 13);
        sImages[i].mRefs = 1000;
        sImagePtrs[i].mPtr = &sImages[i];
    }
    for (int i = 0; i < 3; i++) { sServers[i].mId = 10 + i; sServers[i].mAddr.mValue = 500 + i; sServers[i].mSalt = inSalt; sServers[i].mRefs = 1000; }
    for (int i = 0; i < 4; i++) { sGroups[i].mId = 20 + i; sGroups[i].mSalt = inSalt; sGroups[i].mRefs = 1000; }
    for (int i = 0; i < 4; i++) { sClients[i].mId = 30 + i; sClients[i].mSalt = inSalt; sClients[i].mRefs = 1000; }
    for (int i = 0; i < 3; i++) { sFriends[i].mId = 40 + i; sFriends[i].mSalt = inSalt; sFriends[i].mRefs = 1000; }
    for (int i = 0; i < 3; i++) { sGames[i].mId = 50 + i; sGames[i].mSalt = inSalt; sGames[i].mRefs = 1000; }
    sList.mFont.mPtr = NULL; // the world's own (no trace)
    sList = ListArea();
    sList.mFont.mPtr = &sFonts[0];
}

static intptr_t Ctl(bool inRust, Op inOp, intptr_t a, intptr_t b, intptr_t c)
{
    if (!inRust) return Orig::HostOp(inOp, a, b, c);
    intptr_t r = ee_won_lobby_rows_ctl_op(inOp, a, b, c);
    if (r == INTPTR_MIN) Line("not a row op %lld", (long long)inOp);
    return r;
}

static std::vector<std::string> RunSide(bool inRust, Kind inKind, const std::vector<Step>& inSteps, unsigned inSalt)
{
    sTrace.clear();
    sSeq = 0;
    ResetWorld(inSalt);
    int aLive = IPAddr::sLive;
    std::vector<ListItem*> aRows;
    for (size_t n = 0; n < inSteps.size(); n++) {
        const Step& s = inSteps[n];
        Line("step %lld %lld %lld %lld", s.mWhat, s.a, s.b, s.c);
        ListItem* aRow = aRows.empty() ? NULL : aRows[(size_t)s.a % aRows.size()];
        intptr_t r = 0;
        switch (s.mWhat) {
        case 0: { // a row made
            if (aRows.size() >= 4) break;
            intptr_t p = 0;
            switch (inKind) {
            case kRoom:
                if (s.b % 3 == 0) p = Ctl(inRust, Op::RoomItemGroup, Obj(1 + 4 * (unsigned)s.c), Obj(4 * (unsigned)s.c), 0);
                else p = Ctl(inRust, Op::RoomItemServer, Obj(4 * (unsigned)s.c), s.b % 3 == 1, 0);
                break;
            case kFriend: p = Ctl(inRust, Op::FriendRowNew, 0, 0, 0); break;
            case kUser: p = Ctl(inRust, Op::UserRowNew, 0, 0, 0); break;
            case kGameUser: p = Ctl(inRust, Op::ItemNew, Obj(2 + 4 * (unsigned)s.c), 0, 0); break;
            case kServer: p = Ctl(inRust, Op::RowNew, 0, 0, 0); break;
            }
            ListItem* anItem = reinterpret_cast<ListItem*>(p);
            if (inKind != kGameUser && inKind != kServer) anItem->AddRef(); // the list's reference (the others come with one)
            Line("made %lld", anItem->mId);
            aRows.push_back(anItem);
            // as the controls do: the player's icon set, then the list measures the row before its first paint
            if (inKind == kGameUser) Ctl(inRust, Op::ItemSetImage, p, (intptr_t)(s.b % 10), 0);
            if (inKind != kServer) anItem->CalcDimensions(&sList); // a game list row's virtuals are its control's
            if (inKind != kServer) Line("selectable %lld", anItem->CanSelect(&sList, 3, 4) ? 1 : 0);
            break;
        }
        case 1: // the room text updated
            if (aRow == NULL || inKind != kRoom) break;
            Ctl(inRust, Op::RoomItemUpdate, (intptr_t)aRow, s.b % 3 ? Obj(4 * (unsigned)s.c) : 0, s.b % 2 ? Obj(1 + 4 * (unsigned)s.c) : 0);
            break;
        case 2: // measured
            if (aRow == NULL || inKind == kServer) break;
            sList.mFont = &sFonts[s.b % 3];
            aRow->CalcDimensions(&sList);
            Line("size %lld %lld", aRow->mWidth, aRow->mHeight);
            break;
        case 3: // painted
            if (aRow == NULL || inKind == kServer) break;
            aRow->mSelected = s.b % 3 == 0;
            sList.mFlags = (int)(s.c & 0x30);
            sList.mColumn = (int)(s.c >> 8) % 5 - 1;
            sList.mPaintWidth = 40 + (int)(s.c % 70);
            sList.mTotalWidth = 30 + (int)(s.b % 90);
            sList.mSelColor.mBack = 0x40 + (unsigned long)(s.b % 5);
            aRow->Paint(sGraphics, &sList);
            break;
        case 4: { // compared
            if (aRow == NULL || inKind == kServer) break;
            ListItem* anOther = aRows[(size_t)s.b % aRows.size()];
            r = aRow->Compare(anOther, &sList);
            Line("compare %lld %lld = %lld", aRow->mId, anOther->mId, r);
            break;
        }
        case 5: { // a member read or written
            if (aRow == NULL) break;
            intptr_t p = (intptr_t)aRow;
            switch (inKind) {
            case kRoom:
                if (s.b % 2) Line("group id %lld", Ctl(inRust, Op::RoomItemGroupId, p, 0, 0));
                else Line("address %lld", reinterpret_cast<const IPAddr*>(Ctl(inRust, Op::RoomItemAddr, p, 0, 0))->mValue);
                break;
            case kFriend:
                if (s.b % 4 == 0) Ctl(inRust, Op::ItemSetFriend, p, s.c % 3 ? Obj(3 + 4 * (unsigned)s.c) : 0, 0);
                else if (s.b % 4 == 1) Line("friend %lld", Id((void*)Ctl(inRust, Op::ItemFriend, p, 0, 0)));
                else Line("field %lld", Ctl(inRust, s.b % 4 == 2 ? Op::ItemSet : Op::ItemGet, p,
                                            (intptr_t)(s.c % 2 ? F::FriendSeparator : F::FriendSortGroup), (intptr_t)(s.c % 5) - 2));
                break;
            case kUser:
                if (s.b % 4 == 0) Ctl(inRust, Op::ItemSetUserClient, p, s.c % 3 ? Obj(2 + 4 * (unsigned)s.c) : 0, 0);
                else if (s.b % 4 == 1) Line("client %lld", Id((void*)Ctl(inRust, Op::ItemUserClient, p, 0, 0)));
                else Line("field %lld", Ctl(inRust, s.b % 4 == 2 ? Op::ItemSet : Op::ItemGet, p,
                                            (intptr_t)(s.c % 2 ? F::UserShowSeparator : F::UserSortKey), (intptr_t)(s.c % 5) - 2));
                break;
            case kGameUser:
                if (s.b % 4 == 0) Ctl(inRust, Op::ItemSetImage, p, (intptr_t)(s.c % 10), 0);
                else if (s.b % 4 == 1) Line("client %lld", Id((void*)Ctl(inRust, Op::ItemClient, p, 0, 0)));
                else for (int t = 0; t < 8; t++) Line("is %lld %lld", t, Ctl(inRust, Op::ItemClientIs, p, t, 0));
                break;
            case kServer:
                switch (s.b % 8) {
                case 0: Ctl(inRust, Op::RowSetGame, p, s.c % 4 ? (intptr_t)&sGames[s.c % 3] : 0, 0); break;
                case 1: Line("game %lld", Id((void*)Ctl(inRust, Op::RowGame, p, 0, 0))); break;
                case 2: if (Ctl(inRust, Op::RowGame, p, 0, 0) != 0) Ctl(inRust, Op::RowSetAddrText, p, 0, 0); break;
                case 3: LineS("addr text '%s'", Txt(Ctl(inRust, Op::RowAddrText, p, 0, 0)).s); break;
                case 4: Line("field %lld", Ctl(inRust, s.c % 2 ? Op::ItemSet : Op::ItemGet, p, (intptr_t)F::IconId, (intptr_t)(s.c % 9) - 4)); break;
                case 5: Line("field %lld", Ctl(inRust, s.c % 2 ? Op::ItemSet : Op::ItemGet, p, (intptr_t)F::SummaryHeight, (intptr_t)(s.c % 600) - 100)); break;
                case 6: Line("details %lld", Ctl(inRust, Op::RowDetailsHeight, p, (intptr_t)&sList, 0)); break;
                default: Ctl(inRust, Op::RowPaintDetails, p, (intptr_t)&sGraphics, (intptr_t)&sList); break;
                }
                break;
            }
            break;
        }
        case 6: // the model's answers change
            ResetWorld(inSalt + (unsigned)s.b);
            for (size_t i = 0; i < aRows.size(); i++) (void)0;
            break;
        case 7: { // a row's end (its releases compared as a set)
            if (aRow == NULL) break;
            size_t aFrom = sTrace.size();
            aRows.erase(std::find(aRows.begin(), aRows.end(), aRow));
            aRow->Release();
            std::sort(sTrace.begin() + (std::ptrdiff_t)aFrom, sTrace.end());
            break;
        }
        }
    }
    Line("end");
    while (!aRows.empty()) {
        size_t aFrom = sTrace.size();
        aRows.back()->Release();
        aRows.pop_back();
        std::sort(sTrace.begin() + (std::ptrdiff_t)aFrom, sTrace.end());
    }
    Line("addresses left %lld", IPAddr::sLive - aLive);
    return sTrace;
}

int main(int argc, char** argv)
{
    bool aPrint = argc > 1 && strcmp(argv[1], "--print") == 0;
    int kSequences = 40000;
    if (argc > 2) kSequences = atoi(argv[2]);
    ee_won_lobby_rows_host_world(World);
    unsigned long long aLines = 0;
    int aDiffer = 0;
    std::set<std::string> aSeen;
    sSeed = 977;
    for (int n = 0; n < kSequences; n++) {
        Kind aKind = (Kind)R(5);
        std::vector<Step> aSteps;
        int aCount = 1 + (int)R(30);
        Step aFirst = {0, 0, (intptr_t)R(9), (intptr_t)R(12)};
        aSteps.push_back(aFirst);
        for (int i = 0; i < aCount; i++) {
            unsigned w = R(20);
            int aWhat = w < 3 ? 0 : w < 5 ? 1 : w < 8 ? 2 : w < 12 ? 3 : w < 14 ? 4 : w < 18 ? 5 : w < 19 ? 6 : 7;
            Step s = {aWhat, (intptr_t)R(4), (intptr_t)R(1000), (intptr_t)R(5000)};
            aSteps.push_back(s);
        }
        unsigned aSalt = 17u + (unsigned)n * 7919u;
        std::vector<std::string> a = RunSide(false, aKind, aSteps, aSalt);
        std::vector<std::string> b = RunSide(true, aKind, aSteps, aSalt);
        aLines += a.size();
        sChecks++;
        if (a != b) {
            if (++aDiffer <= 3) {
                printf("sequence %d (kind %d) differs\n", n, (int)aKind);
                for (size_t i = 0; i < a.size() || i < b.size(); i++) {
                    if (i >= a.size() || i >= b.size() || a[i] != b[i]) {
                        printf("  line %zu: C++ '%s' Rust '%s'\n", i, i < a.size() ? a[i].c_str() : "-", i < b.size() ? b[i].c_str() : "-");
                        for (size_t k = i > 8 ? i - 8 : 0; k < i; k++) printf("    %s\n", a[k].c_str());
                        if (getenv("ROWS_DUMP")) {
                            for (size_t k = 0; k < a.size(); k++) printf("  C %s\n", a[k].c_str());
                            for (size_t k = 0; k < b.size(); k++) printf("  R %s\n", b[k].c_str());
                        }
                        break;
                    }
                }
            }
            continue;
        }
        bool aNew = n < 60;
        for (size_t i = 0; i < a.size(); i++) {
            char aWord[32] = "", aWord2[32] = "";
            sscanf(a[i].c_str(), "%31s %31s", aWord, aWord2);
            std::string aKey = std::string(aWord) + (strcmp(aWord, "g") == 0 || strcmp(aWord, "base") == 0 ? std::string(" ") + aWord2 : "") +
                               " k" + std::to_string((int)aKind);
            if (aSeen.insert(aKey).second) aNew = true;
        }
        if (aPrint && aNew) {
            printf("seq %d kind %d\n", n, (int)aKind);
            for (size_t i = 0; i < a.size(); i++) printf("%s\n", a[i].c_str());
        }
    }
    // every row op the Rust asks the host for (the C++ host's cases)
    CHECK(sOpsSeen.size() >= 30);
    if (aDiffer) { sFailures++; printf("FAIL: %d of %d sequences differ\n", aDiffer, kSequences); }
    if (!aPrint)
        printf("won_rowobj_test: %d sequences, %llu trace lines, %zu distinct trace keys, %zu host ops; %d checks, %d failures\n",
               kSequences, aLines, aSeen.size(), sOpsSeen.size(), sChecks, sFailures);
    ee_won_lobby_rows_host_world(NULL);
    return sFailures ? 1 : 0;
}
