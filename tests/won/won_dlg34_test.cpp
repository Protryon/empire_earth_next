// The lobby dialog host's compound cases R6 WON round 34 composed in Rust (ee_won_lobby dlgsplit.rs, the bad user
// row rows.rs BadUser) against the deleted code as it was at ab1b33bcb (LobbyDialogHost.cpp's BadUserItem class,
// InsertBadUser and the cases BadUsersInsert, BadUserId, AdminTitleRes, AdapterAddDefault, FindFirst,
// FindNameLength, FileListInsert, FindNext and FindClose, copied below in WonDlg34Orig), over one stand-in world that
// logs every call with an effect (WonDlg34World). The Rust's calls out are answered by the hosts' new one-statement
// cases, their statements copied (NewDlg: LobbyDialogHost.cpp's; NewRows: the rows' ops of LobbyNetCtrlHost.cpp
// and LobbyCtrlHost.cpp).
//
// - Bad users: lists of records (names, moderators and comments of several lengths and units, ids at the ends of
//   their range, expiries at every unit's edges and random ones), simple and full; then each row's CalcDimensions,
//   its Paint at every paint column -2..6 selected and not, its Compare with every row at every sort column -2..7,
//   BadUserId, and the list's end (every text's and font's release). An out-of-range column's Compare answers
//   `this < other`: each side's answer is checked against its own two pointers.
// - The admin title over resource strings and client names; the default adapter row.
// - The file dialog's search over scratch directories (none, empty, files with and without the extension,
//   names of 1..40 characters, an extension alone), as lobbydlg.rs's ListFiles loop runs it.
//
// Then mutations: the new side's hosts broken in one place each, every one must be caught. The exit code is the
// number of mismatches of the real pass plus the mutations missed.
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include "platform/PlatFile.h"
#include "platform/PlatTime.h"
#include "rust/won_lobby/lobbydlg.h"
#include "rust/won_lobby/dlgsplit.h"
#include "rust/won_lobby/lobbyctl.h"
#include "rust/won_lobby/resource.h"
#include "rust/won_lobby/rows.h"
#include "rust/won_gui/rowobj.h"
#include "rust/won_net/routeop.h"
#include "rust/won_net/hostop.h"
#include "rust/won_lobby/eegame.h"
#include "rust/won_lobby/eexfer.h"
#include "rust/won_lobby/eesplit.h"
#include "ee/wonapi/WONLobbyGuard.h"
#include <exception>

namespace WonDlg34World {
static std::vector<std::string> sTrace;
static void Log(const std::string& s) { sTrace.push_back(s); }
static std::string F(const char* inFormat, long long a = 0, long long b = 0, long long c = 0)
{
    char aBuffer[400];
    snprintf(aBuffer, sizeof(aBuffer), inFormat, a, b, c);
    return aBuffer;
}

struct RefCount {
    int mRefs = 0;
    virtual ~RefCount() {}
    void AddRef() { this->mRefs++; }
    void Release() { if (--this->mRefs == 0) delete this; }
    RefCount* CreateRef() { this->mRefs++; return this; }
};
template <class T> class SmartPtr {
public:
    SmartPtr() : mPtr(NULL) {}
    SmartPtr(T* p) : mPtr(p ? (T*)p->CreateRef() : NULL) {}
    SmartPtr(const SmartPtr& o) : mPtr(o.mPtr ? (T*)o.mPtr->CreateRef() : NULL) {}
    ~SmartPtr() { if (this->mPtr != NULL) this->mPtr->Release(); }
    T* operator=(T* p)
    {
        if (this->mPtr != p) {
            T* old = this->mPtr;
            this->mPtr = p ? (T*)p->CreateRef() : NULL;
            if (old != NULL) old->Release();
        }
        return p;
    }
    SmartPtr& operator=(const SmartPtr& o) { this->operator=(o.mPtr); return *this; }
    T* operator->() const { return this->mPtr; }
    T* get() const { return this->mPtr; }
    T* mPtr;
};

// a text's data: its characters as bytes (a unit past 0x7f as \u{..}), its life logged
struct GUIStringData : RefCount {
    std::string s;
    bool mStatic = false;
    void Made() { if (!this->mStatic) Log("+text '" + this->s + "'"); }
    GUIStringData(bool inStatic) : mStatic(inStatic) {}
    GUIStringData(const char* t) : s(t) { this->Made(); }
    GUIStringData(const char* t, unsigned short n) : s(t, n) { this->Made(); }
    GUIStringData(const std::string& t) : s(t) { this->Made(); }
    GUIStringData(const std::wstring& w)
    {
        for (wchar_t c : w) this->s += c < 0x80 ? std::string(1, (char)c) : F("\\u{%llx}", (long long)c);
        this->Made();
    }
    GUIStringData(const GUIStringData* o, unsigned short inStart, unsigned short inLength) : s(o->s.substr(inStart, inLength))
    {
        this->Made();
    }
    ~GUIStringData() { Log("-text '" + this->s + "'"); }
    int Compare(const GUIStringData* o) const { int r = this->s.compare(o->s); return r < 0 ? -1 - (int)o->s.size() : r > 0 ? 1 + (int)this->s.size() : 0; }
};
static GUIStringData* GetEmptyGUIStringData()
{
    static GUIStringData* sEmpty = [] { GUIStringData* d = new GUIStringData(true); d->AddRef(); return d; }();
    return sEmpty;
}
struct GUIString {
    SmartPtr<GUIStringData> mData;
    GUIString() : mData(GetEmptyGUIStringData()) {}
    GUIString(GUIStringData* d) : mData(d) {}
    GUIString(const char* t) : mData(new GUIStringData(t)) {}
    GUIString(const std::wstring& t) : mData(new GUIStringData(t)) {}
    int length() const { return (int)this->mData->s.size(); }
    void append(const GUIString& o) { this->mData = new GUIStringData(this->mData->s + o.mData->s); }
    void resize(int n) { this->mData = new GUIStringData(this->mData->s.substr(0, (size_t)n)); }
};
static GUIString gEmptyGUIString;
static GUIString LobbyFormatString(const GUIString& f, const GUIString& a)
{
    return GUIString(new GUIStringData(f.mData->s + "[" + a.mData->s + "]"));
}
struct LobbyResource {
    GUIString mString;
};
static LobbyResource gLobbyResources[1024];
#define LOBBY_STRING(theName) (gLobbyResources[(int)EeWonLobbyRes::theName].mString)

struct Font : RefCount {
    int mId = 0, mHeight = 0;
    int GetHeight() { return this->mHeight; }
};
struct Graphics;
struct SelColor {
    unsigned long mFore, mBack;
    unsigned long GetForegroundColor(Graphics&, unsigned long c) { return this->mFore ^ c; }
    unsigned long GetBackgroundColor(Graphics&) { return this->mBack; }
};
struct Graphics {
    void SetColor(unsigned long c) { Log(F("SetColor %#llx", (long long)c)); }
    void FillRect(int x, int y, int w, int h) { Log(F("FillRect %lld %lld %lld", x, y, w) + F(" %lld", h)); }
    void SetFont(Font* f) { Log(F("SetFont %lld", f != NULL ? f->mId : -1)); }
    void DrawString(const GUIString& t, int x, int y) { Log("DrawString '" + t.mData->s + "'" + F(" %lld %lld", x, y)); }
};
struct ListArea;
static int sNextItem;
struct ListItem : RefCount {
    int mId, mHeight = 0, mWidth = 0;
    bool mSelected = false, mSelectable = true;
    ListItem() : mId(++sNextItem) { Log(F("+item %lld", this->mId)); }
    virtual ~ListItem() { Log(F("-item %lld", this->mId)); }
    virtual void Paint(Graphics&, ListArea*) {}
    virtual void CalcDimensions(ListArea*) {}
    virtual int Compare(const ListItem*, ListArea*) const { return 0; }
};
struct MultiListItem : ListItem {};
struct ListArea : RefCount {
    SmartPtr<Font> mFont;
    unsigned long mTextColor = 0x10203;
    SelColor mSel{0xff00, 0xaa};
    int mPaintWidth = 77, mPaintColumn = 0, mSortColumn = 0;
    std::vector<SmartPtr<ListItem> > mItems;
    unsigned long GetTextColor() { return this->mTextColor; }
    SelColor& GetSelColor() { return this->mSel; }
    int GetPaintWidth() { return this->mPaintWidth; }
    ListItem* InsertItem(ListItem* i, int pos)
    {
        Log(F("InsertItem %lld %lld", i->mId, pos));
        this->mItems.push_back(i);
        return i;
    }
    void InsertString(const GUIString& t, int pos) { Log("InsertString '" + t.mData->s + "'" + F(" %lld", pos)); }
};
struct MultiListArea : ListArea {
    int GetColumnPaintContext() { return this->mPaintColumn; }
    int GetSortColumn() { return this->mSortColumn; }
    int GetNumItems() { return (int)this->mItems.size(); }
    void SetString(int r, int c, const GUIString& t, void*) { Log("SetString" + F(" %lld %lld '", r, c) + t.mData->s + "'"); }
};
struct Label : RefCount {
    void SetText(const GUIString& t, bool inv) { Log("Label SetText '" + t.mData->s + "'" + F(" %lld", inv)); }
};
struct LobbyClient : RefCount {
    GUIString mName;
    const GUIString& GetName() { return this->mName; }
};
// the adapter rows (the class stays C++)
struct NetworkAdapterListItem : MultiListItem {
    long mHost;
    NetworkAdapterListItem(long inHost) : mHost(inHost) { Log(F("adapter row %lld", inHost)); }
};
// the record's text (LobbyDialogHost.cpp's RecordText: the units as wide characters)
static std::wstring RecordText(const uint16_t* inUnits, size_t inCount)
{
    std::wstring aText;
    for (size_t i = 0; i < inCount; i++) aText += (wchar_t)inUnits[i];
    return aText;
}
// the rows' shell (ee_won_gui's RustRow: its virtuals the row's calls)
struct RustRow : ListItem {
    explicit RustRow(EeWonRow* r) : mRow(r) {}
    virtual ~RustRow() { ee_won_row_free(this->mRow); }
    virtual void Paint(Graphics& g, ListArea* l) { ee_won_row_call(this->mRow, EeWonRowCall::Paint, this, l, (intptr_t)&g, 0, 0); }
    virtual void CalcDimensions(ListArea* l) { ee_won_row_call(this->mRow, EeWonRowCall::CalcDimensions, this, l, 0, 0, 0); }
    virtual int Compare(const ListItem* o, ListArea* l) const
    {
        return (int)ee_won_row_call(this->mRow, EeWonRowCall::Compare, const_cast<RustRow*>(this), l,
                                    (intptr_t) static_cast<const RustRow*>(o)->mRow, (intptr_t)o, 0);
    }
    EeWonRow* mRow;
};
// ---- the EE game's and the saved game transfer's objects
static int sNextOp, sNextSocket;
struct IPAddr {
    unsigned long mHost = 0;
    unsigned short mPort = 0;
    void SetPort(unsigned short p) { this->mPort = p; }
};
struct CompletionBase : RefCount {
    int mCall = 0;
    virtual ~CompletionBase() { Log(F("-completion %lld", this->mCall)); }
};
struct EELobbyFileTransfer : RefCount {
    GUIString mName;
    IPAddr mHost;
    EeWonEeXfer* mRust = NULL;
};
struct EELobbyTransferCompletion : CompletionBase {
    SmartPtr<EELobbyFileTransfer> mTransfer;
    EELobbyTransferCompletion(EELobbyFileTransfer* t, EeWonEeXferCall c) : mTransfer(t)
    {
        this->mCall = (int)c;
        Log(F("+completion %lld", (long long)c));
    }
};
struct AsyncOp : RefCount {
    int mId;
    SmartPtr<CompletionBase> mCompletion;
    AsyncOp() : mId(++sNextOp) {}
    virtual ~AsyncOp() { Log(F("-op %lld", this->mId)); }
    void SetCompletion(CompletionBase* c) { this->mCompletion = c; Log(F("op %lld completion %lld", this->mId, c != NULL ? c->mCall : -1)); }
};
struct SocketOp : AsyncOp {
    int mKind;
    SmartPtr<CompletionBase> mProgress;
    SocketOp(EeWonCoreSocketOp k, void* = NULL, void* = NULL, const IPAddr* a = NULL) : mKind((int)k)
    {
        Log(F("+socket op %lld kind %lld", this->mId, this->mKind) + (a != NULL ? F(" addr %llx:%lld", (long long)a->mHost, a->mPort) : ""));
    }
    void SetFilePath(const std::string& p) { Log(F("op %lld path ", this->mId) + p); }
    void SetProgressCompletion(CompletionBase* c, long i) { this->mProgress = c; Log(F("op %lld progress %lld every %lld", this->mId, c->mCall, i)); }
};
struct SendFileOp : SocketOp {
    SendFileOp(const std::string& p) : SocketOp((EeWonCoreSocketOp)-1) { Log(F("op %lld sends ", this->mId) + p); }
};
struct AsyncSocket : RefCount {};
struct QueueSocket : AsyncSocket {
    int mId;
    SmartPtr<CompletionBase> mRepeat, mClose;
    SmartPtr<AsyncOp> mRepeatOp;
    std::vector<SmartPtr<AsyncOp> > mQueue;
    QueueSocket(int t) : mId(++sNextSocket) { Log(F("+socket %lld %lld", this->mId, t)); }
    ~QueueSocket() { Log(F("-socket %lld", this->mId)); }
    void SetRepeatCompletion(CompletionBase* c) { this->mRepeat = c; Log(F("socket %lld repeat completion %lld", this->mId, c->mCall)); }
    void SetRepeatOp(AsyncOp* o) { this->mRepeatOp = o; Log(F("socket %lld repeat op %lld", this->mId, o->mId)); }
    void SetCloseCompletion(CompletionBase* c) { this->mClose = c; Log(F("socket %lld close completion %lld", this->mId, c->mCall)); }
    void QueueOp(SocketOp* o, long t) { this->mQueue.push_back(o); Log(F("socket %lld queue %lld timeout %lld", this->mId, o->mId, t)); }
};
struct EELobbyPlayerSlot {
    int mIndex = 0;
};
struct EELobbyPlayer : RefCount {
    unsigned char mSlot = 0;
    EELobbyPlayerSlot* mPlayerSlot = NULL; // the deleted slot view
    struct EELobbyGame* mSlotGame = NULL;   // the new members
    unsigned char mSlotIndex = 0;
};
struct EELobbyGame : RefCount {
    EELobbyPlayerSlot mSlots[8];
    IPAddr mAddress;
    EELobbyGame() { for (int i = 0; i < 8; i++) this->mSlots[i].mIndex = i; }
    const IPAddr& GetAddress() { return this->mAddress; }
};
static GUIString EELobbyStr(intptr_t inData) { return inData != 0 ? GUIString((GUIStringData*)inData) : GUIString(); }
// a message: a read past its end throws
struct ReadBufferException : std::exception {
    std::string mMessage;
    ReadBufferException(const std::string& t) : mMessage(t) {}
};
struct ReadBuffer {
    std::string mData;
    size_t mPos = 0;
    const char* Take(size_t n)
    {
        if (this->mPos + n > this->mData.size()) throw ReadBufferException("Attempt to read past end of buffer.");
        const char* p = this->mData.data() + this->mPos;
        this->mPos += n;
        return p;
    }
    unsigned char ReadByte() { return (unsigned char)*this->Take(1); }
    bool ReadBool() { return this->ReadByte() != 0; }
    short ReadShort() { short v; memcpy(&v, this->Take(2), 2); return v; }
    int32_t ReadLong() { int32_t v; memcpy(&v, this->Take(4), 4); return v; }
    void ReadWString(std::wstring& out, int)
    {
        unsigned short n;
        memcpy(&n, this->Take(2), 2);
        const char* p = this->Take(2u * n);
        out.clear();
        for (unsigned i = 0; i < n; i++) { uint16_t u; memcpy(&u, p + 2 * i, 2); out += (wchar_t)u; }
    }
};
} // namespace WonDlg34World

using namespace WonDlg34World;

// ---- the deleted code as it was (LobbyDialogHost.cpp at ab1b33bcb), over the world
namespace WonDlg34Orig {
struct EELobbyFind {
    PlatFind* mFind;
    PlatFindData mData;
};
class BadUserItem : public ListItem {
public:
    BadUserItem(const EeWonRouteRecord& inUser,bool inSimple);
    virtual void Paint(Graphics& inGraphics,ListArea* inList);
    virtual void CalcDimensions(ListArea* inList);
    virtual int Compare(const ListItem* inOther,ListArea* inList) const;
    SmartPtr<Font> mFont; // 0x18
    GUIString mStrings[6]; // 0x1c..0x30: name, WON id, expiry, moderator, moderator's WON id, comment
    unsigned long mValues[6]; // 0x34..0x48: sort keys; only the WON id (1), expiry (2) and moderator id (4) are set
    bool mIsSimple; // 0x4c (padded to 0x50)
};
BadUserItem::BadUserItem(const EeWonRouteRecord& inUser, bool inSimple)
{
    this->mIsSimple = inSimple;
    this->mStrings[0] = GUIString(new GUIStringData(RecordText(inUser.name, inUser.name_len)));
    char aBuffer[100];
    sprintf(aBuffer, "%d", inUser.user_id);
    this->mStrings[1] = aBuffer;
    this->mValues[1] = inUser.user_id;

    unsigned long aDuration = inUser.expiration;
    GUIString aText;
    GUIString aUnit;
    if (aDuration > 86400) {
        aDuration /= 86400;
        aUnit = aDuration > 1 ? LOBBY_STRING(BadUserDialog_XDays_String) : LOBBY_STRING(BadUserDialog_OneDay_String);
    } else if (aDuration > 3600) {
        aDuration /= 3600;
        aUnit = aDuration > 1 ? LOBBY_STRING(BadUserDialog_XHours_String) : LOBBY_STRING(BadUserDialog_OneHour_String);
    } else if (aDuration > 60) {
        aDuration /= 60;
        aUnit = aDuration > 1 ? LOBBY_STRING(BadUserDialog_XMinutes_String) : LOBBY_STRING(BadUserDialog_OneMinute_String);
    } else if (aDuration > 0) {
        aUnit = aDuration > 1 ? LOBBY_STRING(BadUserDialog_XSeconds_String) : LOBBY_STRING(BadUserDialog_OneSecond_String);
    } else {
        aUnit = LOBBY_STRING(BadUserDialog_Infinite_String);
    }
    sprintf(aBuffer, "%d", aDuration);
    this->mValues[2] = inUser.expiration;
    this->mStrings[2] = LobbyFormatString(aUnit, GUIString(aBuffer));
    this->mStrings[3] = GUIString(RecordText(inUser.moderator_name, inUser.moderator_name_len));
    sprintf(aBuffer, "%d", inUser.moderator_id);
    this->mStrings[4] = GUIString(aBuffer);
    this->mValues[4] = inUser.moderator_id;
    this->mStrings[5] = GUIString(RecordText(inUser.comment, inUser.comment_len));
}
void BadUserItem::CalcDimensions(ListArea* inList)
{
    this->mFont=inList->mFont;
    this->mHeight=this->mFont->GetHeight();
}
void BadUserItem::Paint(Graphics& inGraphics,ListArea* inList)
{
    unsigned long aColor=inList->GetTextColor();
    if(this->mSelected) aColor=inList->GetSelColor().GetForegroundColor(inGraphics,aColor);
    int aColumn=0;
    bool aSelected=this->mSelected;
    bool aSimple=this->mIsSimple;
    if(!aSimple) aColumn=static_cast<MultiListArea*>(inList)->GetColumnPaintContext();
    if((aSimple || aColumn==-1) && aSelected) {
        inGraphics.SetColor(inList->GetSelColor().GetBackgroundColor(inGraphics));
        int aWidth=inList->GetPaintWidth();
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
static void InsertBadUser(void* inTarget, const EeWonRouteRecord* inRecord)
{
    std::pair<ListArea*, bool>& aTarget = *static_cast<std::pair<ListArea*, bool>*>(inTarget);
    aTarget.first->InsertItem(new BadUserItem(*inRecord, aTarget.second), -1);
}
// the deleted cases (the dialog `inDialog` unused by them)
static intptr_t Host(void* inDialog, EeWonLobbyDlgOp inOp, intptr_t a, intptr_t b, intptr_t c)
{
    typedef EeWonLobbyDlgOp Op;
    Label* aLabel = reinterpret_cast<Label*>(a);
    ListArea* aList = reinterpret_cast<ListArea*>(a);
    MultiListArea* aMulti = reinterpret_cast<MultiListArea*>(a);
    switch (inOp) {
    case Op::AdminTitleRes: {
        GUIString aTitle = gLobbyResources[b].mString;
        aTitle.append(GUIString(new GUIStringData(" ")));
        aTitle.append(reinterpret_cast<LobbyClient*>(c)->GetName());
        aLabel->SetText(aTitle, true);
        return 0;
    }
    case Op::BadUserId: return (intptr_t)reinterpret_cast<BadUserItem*>(a)->mValues[1];
    case Op::AdapterAddDefault:
        aMulti->InsertItem(new NetworkAdapterListItem(0), -1);
        aMulti->SetString(0, 0, LOBBY_STRING(ChooseNetworkAdapterDialog_Default_String), NULL);
        return 0;
    case Op::FindFirst: {
        EELobbyFind* aFind = new EELobbyFind;
        aFind->mFind = PlatFindFirst(reinterpret_cast<const char*>(a), &aFind->mData);
        if (aFind->mFind != NULL)
            return (intptr_t)aFind;
        delete aFind;
        return 0;
    }
    case Op::FindNameLength: return (intptr_t)GUIString(reinterpret_cast<EELobbyFind*>(a)->mData.mName).length();
    case Op::FileListInsert: {
        GUIString theName(reinterpret_cast<EELobbyFind*>(b)->mData.mName);
        theName.resize((int)c);
        aList->InsertString(theName, -1);
        return 0;
    }
    case Op::FindNext: return PlatFindNext(reinterpret_cast<EELobbyFind*>(a)->mFind, &reinterpret_cast<EELobbyFind*>(a)->mData) == PLAT_FILE_OK;
    case Op::FindClose:
        PlatFindClose(reinterpret_cast<EELobbyFind*>(a)->mFind);
        delete reinterpret_cast<EELobbyFind*>(a);
        return 0;
    default: Log(F("orig: unexpected op %lld", (long long)inOp)); return 0;
    }
}
// EELobbyGameHost.cpp's deleted cases and reads
static intptr_t XferHost(void* inObj, EeWonEeXferOp inOp, intptr_t a, intptr_t b, intptr_t c)
{
    typedef EeWonEeXferOp Op;
    EELobbyFileTransfer* aTransfer = static_cast<EELobbyFileTransfer*>(inObj);
    QueueSocket* aSocket = static_cast<QueueSocket*>(reinterpret_cast<AsyncSocket*>(a));
    switch (inOp) {
    case Op::NewSocket: {
        QueueSocket* aNew = new QueueSocket(0);
        aNew->AddRef();
        return (intptr_t) static_cast<AsyncSocket*>(aNew);
    }
    case Op::RepeatAccept:
        aSocket->SetRepeatCompletion(new EELobbyTransferCompletion(aTransfer, EeWonEeXferCall::OnClientAccepted));
        aSocket->SetRepeatOp(new SocketOp(EeWonCoreSocketOp::Accept));
        return 0;
    case Op::QueueSend:
        aSocket->SetCloseCompletion(new EELobbyTransferCompletion(aTransfer, EeWonEeXferCall::OnClientClosed));
        aSocket->QueueOp(new SendFileOp(std::string(reinterpret_cast<const char*>(b), (size_t)c)), -1);
        aSocket->QueueOp(new SocketOp(EeWonCoreSocketOp::Close), -1);
        return 0;
    case Op::QueueDownload: {
        aSocket->SetCloseCompletion(new EELobbyTransferCompletion(aTransfer, EeWonEeXferCall::OnClientClosed));
        SmartPtr<SocketOp> theOp = new SocketOp(EeWonCoreSocketOp::RecvBytes);
        theOp->SetCompletion(new EELobbyTransferCompletion(aTransfer, EeWonEeXferCall::OnFileReceived));
        theOp->SetFilePath(std::string(reinterpret_cast<const char*>(b), (size_t)c));
        theOp->SetProgressCompletion(new EELobbyTransferCompletion(aTransfer, EeWonEeXferCall::OnProgress), 10000);
        aSocket->QueueOp(new SocketOp(EeWonCoreSocketOp::Connect, NULL, NULL, &aTransfer->mHost), -1);
        aSocket->QueueOp(theOp.mPtr, -1);
        return 0;
    }
    default: Log(F("orig: unexpected transfer op %lld", (long long)inOp)); return 0;
    }
}
static intptr_t GameHost(void* inGame, EeWonEeGameOp inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    typedef EeWonEeGameOp Op;
    EELobbyGame* aGame = static_cast<EELobbyGame*>(inGame);
    EELobbyPlayer* aPlayer = reinterpret_cast<EELobbyPlayer*>(a);
    EELobbyFileTransfer* aTransfer = reinterpret_cast<EELobbyFileTransfer*>(a);
    switch (inOp) {
    case Op::PlayerAttach:
        aPlayer->mPlayerSlot = &aGame->mSlots[b];
        aPlayer->mSlot = (unsigned char)b;
        return 0;
    case Op::TransferSetFileInfo:
        aTransfer->mName = EELobbyStr(b);
        return ee_won_eexfer_call(aTransfer->mRust, EeWonEeXferCall::SetFileInfo, (uint32_t)c, (uint32_t)d);
    case Op::TransferSetHost: {
        IPAddr theAddress(aGame->GetAddress());
        theAddress.SetPort((unsigned short)b);
        aTransfer->mHost = theAddress;
        return 0;
    }
    // EELobbyGameHost.cpp's transfer calls as they were before R6 WON lobby/net round 40
    case Op::TransferServe: ee_won_eexfer_call(aTransfer->mRust, EeWonEeXferCall::Serve, (unsigned short)b, 0); return 0;
    case Op::TransferSize: return ee_won_eexfer_call(aTransfer->mRust, EeWonEeXferCall::Field, 2, 0);
    case Op::TransferChecksum: return ee_won_eexfer_call(aTransfer->mRust, EeWonEeXferCall::Field, 3, 0);
    case Op::TransferStop: return ee_won_eexfer_call(aTransfer->mRust, EeWonEeXferCall::Stop, 0, 0);
    case Op::TransferCloseClients: return ee_won_eexfer_call(aTransfer->mRust, EeWonEeXferCall::CloseClients, 0, 0);
    case Op::TransferIsFileHere: return ee_won_eexfer_call(aTransfer->mRust, EeWonEeXferCall::IsFileHere, 0, 0);
    case Op::TransferDownload: return ee_won_eexfer_call(aTransfer->mRust, EeWonEeXferCall::Download, 0, 0);
    default: Log(F("orig: unexpected game op %lld", (long long)inOp)); return 0;
    }
}
static intptr_t Threw(intptr_t inResult)
{
    if (inResult == INTPTR_MIN)
        throw ReadBufferException("Attempt to read past end of buffer.");
    return inResult;
}
static intptr_t Read(EeWonEeGameOp inOp, ReadBuffer* inBuffer)
{
    typedef EeWonEeGameOp Op;
    try {
        switch (inOp) {
        case Op::ReadByte: return inBuffer->ReadByte();
        case Op::ReadShort: return inBuffer->ReadShort();
        case Op::ReadBool: return inBuffer->ReadBool() ? 1 : 0;
        case Op::ReadLong: return inBuffer->ReadLong();
        case Op::ReadWString: {
            std::wstring aText;
            inBuffer->ReadWString(aText, 2);
            GUIStringData* aData = new GUIStringData(aText);
            aData->AddRef();
            return (intptr_t)aData;
        }
        default: return 0;
        }
    } catch (ReadBufferException&) {
        return INTPTR_MIN;
    }
}
} // namespace WonDlg34Orig

// ---- the new side's hosts: the new one-statement cases as they are, and the mutations

static int sMutate;
static std::map<int, int> sDlgHits, sRowHits;
static intptr_t Held(RefCount* inObject)
{
    if (inObject != NULL)
        inObject->AddRef();
    return (intptr_t)inObject;
}
static GUIString Txt(intptr_t inData) { return inData != 0 ? GUIString((GUIStringData*)inData) : GUIString(); }
static int Lo(intptr_t v) { return (int)(int32_t)(v & 0xffffffff); }
static int Hi(intptr_t v) { return (int)(v >> 32); }

static intptr_t NewDlg(void* inDialog, EeWonLobbyDlgOp inOp, intptr_t a, intptr_t b, intptr_t c)
{
    typedef EeWonLobbyDlgOp Op;
    sDlgHits[(int)inOp]++;
    Label* aLabel = reinterpret_cast<Label*>(a);
    ListArea* aList = reinterpret_cast<ListArea*>(a);
    MultiListArea* aMulti = reinterpret_cast<MultiListArea*>(a);
    if (sMutate == 1 && inOp == Op::TextFormatRes) a = (intptr_t)EeWonLobbyRes::BadUserDialog_Infinite_String;
    if (sMutate == 2 && inOp == Op::TextCompare) return -reinterpret_cast<GUIStringData*>(a)->Compare(reinterpret_cast<GUIStringData*>(b));
    if (sMutate == 3 && inOp == Op::TextRecord && b > 0) b -= 1;
    if (sMutate == 4 && inOp == Op::TitleAppendSpace) return 0;
    if (sMutate == 5 && inOp == Op::AdapterSetDefaultName) return 0;
    if (sMutate == 6 && inOp == Op::NameLength) return (intptr_t)GUIString(reinterpret_cast<const char*>(a)).length() + 1;
    if (sMutate == 7 && inOp == Op::FileListInsertName) c -= 1;
    switch (inOp) {
    case Op::LabelSetText: aLabel->SetText(b != 0 ? *reinterpret_cast<const GUIString*>(b) : gEmptyGUIString, true);
        return 0;
    case Op::AdapterInsertRow: aMulti->InsertItem(new NetworkAdapterListItem((long)b), -1); return 0;
    case Op::TextCompare: return reinterpret_cast<GUIStringData*>(a)->Compare(reinterpret_cast<GUIStringData*>(b));
    case Op::TextRecord: return Held(new GUIStringData(RecordText(reinterpret_cast<const uint16_t*>(a), (size_t)b)));
    case Op::TextFormatRes: return Held(LobbyFormatString(gLobbyResources[a].mString, GUIString(reinterpret_cast<GUIStringData*>(b))).mData.get());
    case Op::TitleNew: return (intptr_t) new GUIString(gLobbyResources[a].mString);
    case Op::TitleAppendSpace: reinterpret_cast<GUIString*>(a)->append(GUIString(new GUIStringData(" "))); return 0;
    case Op::TitleAppendName: reinterpret_cast<GUIString*>(a)->append(reinterpret_cast<LobbyClient*>(b)->GetName()); return 0;
    case Op::TitleFree: delete reinterpret_cast<GUIString*>(a); return 0;
    case Op::AdapterSetDefaultName: aMulti->SetString(0, 0, LOBBY_STRING(ChooseNetworkAdapterDialog_Default_String), NULL); return 0;
    case Op::NameLength: return (intptr_t)GUIString(reinterpret_cast<const char*>(a)).length();
    case Op::FileListInsertName: aList->InsertString(GUIString(new GUIStringData(GUIString(reinterpret_cast<const char*>(b)).mData.get(), 0, (unsigned short)c)), -1); return 0;
    default: Log(F("new: unexpected dialog op %lld", (long long)inOp)); return 0;
    }
}

enum ItemField { kHeight = 0, kTop, kWidth, kSelected, kSelectable };
static intptr_t NewRows(void*, EeWonLobbyCtlOp inOp, intptr_t a, intptr_t b, intptr_t c)
{
    typedef EeWonLobbyCtlOp Op;
    sRowHits[(int)inOp]++;
    Graphics* g = reinterpret_cast<Graphics*>(a);
    ListArea* aList = reinterpret_cast<ListArea*>(a);
    ListItem* anItem = reinterpret_cast<ListItem*>(a);
    if (sMutate == 8 && inOp == Op::FontHeight) return reinterpret_cast<Font*>(a)->GetHeight() + 1;
    if (sMutate == 9 && inOp == Op::GfxSelFore) return (intptr_t)c;
    if (sMutate == 10 && inOp == Op::Release) return 0;
    if (sMutate == 11 && inOp == Op::ListSortColumn) return static_cast<MultiListArea*>(aList)->GetSortColumn() == 1 ? 4 : static_cast<MultiListArea*>(aList)->GetSortColumn();
    if (sMutate == 12 && inOp == Op::ListPaintColumn) return static_cast<MultiListArea*>(aList)->GetColumnPaintContext() == -1 ? 0 : static_cast<MultiListArea*>(aList)->GetColumnPaintContext();
    switch (inOp) {
    case Op::RowShellNew: return (intptr_t) static_cast<ListItem*>(new RustRow(reinterpret_cast<EeWonRow*>(a)));
    case Op::AddRef: reinterpret_cast<RefCount*>(a)->AddRef(); return 0;
    case Op::Release: reinterpret_cast<RefCount*>(a)->Release(); return 0;
    case Op::TextNew: {
        GUIStringData* aData = new GUIStringData(reinterpret_cast<const char*>(a), (unsigned short)b);
        aData->AddRef();
        return (intptr_t)aData;
    }
    case Op::ListInsert: aList->InsertItem(reinterpret_cast<ListItem*>(b), -1); return 0;
    case Op::ItemGet:
        return b == kHeight ? anItem->mHeight : b == kWidth ? anItem->mWidth : b == kSelected ? anItem->mSelected
               : b == kSelectable ? anItem->mSelectable : 0;
    case Op::ItemSet: if (b == kHeight) anItem->mHeight = (int)c; else Log(F("new: item set %lld", b)); return 0;
    case Op::ListFont: return (intptr_t)aList->mFont.get();
    case Op::FontHeight: return reinterpret_cast<Font*>(a)->GetHeight();
    case Op::ListTextColor: return aList->GetTextColor();
    case Op::GfxSelFore: return reinterpret_cast<ListArea*>(b)->GetSelColor().GetForegroundColor(*g, (unsigned long)c);
    case Op::GfxSelBack: return reinterpret_cast<ListArea*>(b)->GetSelColor().GetBackgroundColor(*g);
    case Op::ListPaintColumn: return static_cast<MultiListArea*>(aList)->GetColumnPaintContext();
    case Op::ListSortColumn: return static_cast<MultiListArea*>(aList)->GetSortColumn();
    case Op::ListPaintWidth: return aList->GetPaintWidth();
    case Op::GfxSetColor: g->SetColor((unsigned long)b); return 0;
    case Op::GfxSetFont: g->SetFont(reinterpret_cast<Font*>(b)); return 0;
    case Op::GfxFillRect: g->FillRect(Lo(b), Hi(b), Lo(c), Hi(c)); return 0;
    case Op::GfxDrawText: g->DrawString(Txt(b), Lo(c), Hi(c)); return 0;
    default: Log(F("new: unexpected row op %lld", (long long)inOp)); return 0;
    }
}

static std::map<int, int> sXferHits, sGameHits;
static intptr_t NewXfer(void* inObj, EeWonEeXferOp inOp, intptr_t a, intptr_t b, intptr_t c)
{
    typedef EeWonEeXferOp Op;
    sXferHits[(int)inOp]++;
    EELobbyFileTransfer* aTransfer = static_cast<EELobbyFileTransfer*>(inObj);
    AsyncOp* anOp = reinterpret_cast<AsyncOp*>(a);
    QueueSocket* aSocket = static_cast<QueueSocket*>(reinterpret_cast<AsyncSocket*>(a));
    if (sMutate == 13 && inOp == Op::SetCloseCompletion) b = (intptr_t)EeWonEeXferCall::OnProgress;
    if (sMutate == 14 && inOp == Op::QueueCloseOp) return 0;
    if (sMutate == 15 && inOp == Op::Release) return 0;
    switch (inOp) {
    // netops.rs's in the game: the references on the Rust objects
    case Op::AddRef: reinterpret_cast<RefCount*>(a)->AddRef(); return 0;
    case Op::Release: reinterpret_cast<RefCount*>(a)->Release(); return 0;
    case Op::NewQueueSocket: return (intptr_t) static_cast<AsyncSocket*>(new QueueSocket(0));
    case Op::SetRepeatCompletion: aSocket->SetRepeatCompletion(new EELobbyTransferCompletion(aTransfer, (EeWonEeXferCall)b)); return 0;
    case Op::SetRepeatAccept: aSocket->SetRepeatOp(new SocketOp(EeWonCoreSocketOp::Accept)); return 0;
    case Op::SetCloseCompletion: aSocket->SetCloseCompletion(new EELobbyTransferCompletion(aTransfer, (EeWonEeXferCall)b)); return 0;
    case Op::QueueSendFile: aSocket->QueueOp(new SendFileOp(std::string(reinterpret_cast<const char*>(b), (size_t)c)), -1); return 0;
    case Op::QueueCloseOp: aSocket->QueueOp(new SocketOp(EeWonCoreSocketOp::Close), -1); return 0;
    case Op::NewRecvBytes: return (intptr_t) static_cast<AsyncOp*>(new SocketOp(EeWonCoreSocketOp::RecvBytes));
    case Op::OpSetCompletion: anOp->SetCompletion(new EELobbyTransferCompletion(aTransfer, (EeWonEeXferCall)b)); return 0;
    case Op::OpSetFilePath: static_cast<SocketOp*>(anOp)->SetFilePath(std::string(reinterpret_cast<const char*>(b), (size_t)c)); return 0;
    case Op::OpSetProgress: static_cast<SocketOp*>(anOp)->SetProgressCompletion(new EELobbyTransferCompletion(aTransfer, EeWonEeXferCall::OnProgress), 10000); return 0;
    case Op::QueueConnect: aSocket->QueueOp(new SocketOp(EeWonCoreSocketOp::Connect, NULL, NULL, &aTransfer->mHost), -1); return 0;
    case Op::QueueOpOn: aSocket->QueueOp(static_cast<SocketOp*>(reinterpret_cast<AsyncOp*>(b)), -1); return 0;
    default: Log(F("new: unexpected transfer op %lld", (long long)inOp)); return 0;
    }
}
static intptr_t NewGame(void* inGame, EeWonEeGameOp inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    typedef EeWonEeGameOp Op;
    sGameHits[(int)inOp]++;
    EELobbyGame* aGame = static_cast<EELobbyGame*>(inGame);
    EELobbyPlayer* aPlayer = reinterpret_cast<EELobbyPlayer*>(a);
    EELobbyFileTransfer* aTransfer = reinterpret_cast<EELobbyFileTransfer*>(a);
    if (sMutate == 16 && inOp == Op::PlayerSetSlot) b += 1;
    if (sMutate == 17 && inOp == Op::TransferHostSetPort) b ^= 1;
    if (sMutate == 18 && inOp == Op::TransferSetName) return 0;
    switch (inOp) {
    case Op::PlayerSetSlotGame: aPlayer->mSlotGame = aGame; return 0;
    case Op::PlayerSetSlotIndex: aPlayer->mSlotIndex = (unsigned char)b; return 0;
    case Op::PlayerSetSlot: aPlayer->mSlot = (unsigned char)b; return 0;
    case Op::TransferSetName: aTransfer->mName = EELobbyStr(b); return 0;
    case Op::TransferRust: return (intptr_t)aTransfer->mRust;
    case Op::TransferHostFromGame: aTransfer->mHost = aGame->GetAddress(); return 0;
    case Op::TransferHostSetPort: aTransfer->mHost.SetPort((unsigned short)b); return 0;
    default: Log(F("new: unexpected game op %lld", (long long)inOp)); return 0;
    }
}
// EELobbyGameHost.cpp's new read cases (each guarded) and the text helper
static GUIStringData* ReadText(ReadBuffer* inBuffer)
{
    std::wstring aText;
    inBuffer->ReadWString(aText, 2);
    GUIStringData* aData = new GUIStringData(aText);
    aData->AddRef();
    return aData;
}
static intptr_t NewRead(EeWonEeGameOp inOp, intptr_t a)
{
    typedef EeWonEeGameOp Op;
    if (sMutate == 19 && inOp == Op::ReadShort) return WON_GUARDED(reinterpret_cast<ReadBuffer*>(a)->ReadByte());
    switch (inOp) {
    case Op::ReadByte: return WON_GUARDED(reinterpret_cast<ReadBuffer*>(a)->ReadByte());
    case Op::ReadShort: return WON_GUARDED(reinterpret_cast<ReadBuffer*>(a)->ReadShort());
    case Op::ReadBool: return WON_GUARDED(reinterpret_cast<ReadBuffer*>(a)->ReadBool() ? 1 : 0);
    case Op::ReadLong: return WON_GUARDED(reinterpret_cast<ReadBuffer*>(a)->ReadLong());
    case Op::ReadWString: return WON_GUARDED(ReadText(reinterpret_cast<ReadBuffer*>(a)));
    default: return 0;
    }
}
static intptr_t XferWorld(void*, EeWonEeXferOp inOp, intptr_t, intptr_t, intptr_t)
{
    Log(F("transfer's host op %lld", (long long)inOp));
    return 0;
}

// ---- the cases

static unsigned sSeed = 12345;
static unsigned Rand() { sSeed = sSeed * 1103515245u + 12345u; return (sSeed >> 8) & 0xffffff; }
static int sChecks, sMismatches;
static void Check(const char* inWhat, int inCase, const std::vector<std::string>& o, const std::vector<std::string>& r)
{
    sChecks++;
    if (o == r) return;
    if (sMismatches++ < 8 && sMutate == 0) {
        printf("MISMATCH %s case %d:\n", inWhat, inCase);
        for (size_t i = 0; i < o.size() || i < r.size(); i++) {
            const char* a = i < o.size() ? o[i].c_str() : "";
            const char* b = i < r.size() ? r[i].c_str() : "";
            printf("  %s %-50s | %s\n", strcmp(a, b) ? "*" : " ", a, b);
        }
    }
}

struct Rec {
    std::vector<uint16_t> name, mod, comment;
    uint32_t user, expiry, modid;
};
static std::vector<uint16_t> RandUnits()
{
    std::vector<uint16_t> u;
    int n = (int)(Rand() % 9);
    for (int i = 0; i < n; i++) {
        unsigned k = Rand() % 10;
        u.push_back(k < 6 ? (uint16_t)('a' + Rand() % 26) : k < 8 ? (uint16_t)('A' + Rand() % 3) : (uint16_t)(0x80 + Rand() % 0xff00));
    }
    return u;
}
static uint32_t RandId()
{
    static const uint32_t kIds[] = {0, 1, 2, 7, 0x7fffffff, 0x80000000, 0x80000001, 0xfffffffe, 0xffffffff};
    return Rand() % 3 == 0 ? kIds[Rand() % 9] : Rand() * 977u;
}
static uint32_t RandExpiry()
{
    static const uint32_t kEdges[] = {0, 1, 2, 59, 60, 61, 119, 120, 121, 3599, 3600, 3601, 7199, 7200, 7201,
                                      86399, 86400, 86401, 172799, 172800, 172801, 0x7fffffff, 0xffffffff};
    return Rand() % 2 == 0 ? kEdges[Rand() % 23] : Rand() % 4 == 0 ? Rand() * 4099u : Rand() % 200000;
}

// one list of bad users on one side: made, measured, painted, compared, read, ended
static std::vector<std::string> BadUsersSide(int inSide, const std::vector<Rec>& inRecs, bool inSimple, int inFontHeight)
{
    sTrace.clear();
    sNextItem = 0;
    std::vector<EeWonRouteRecord> aRecs;
    for (const Rec& r : inRecs) {
        EeWonRouteRecord x;
        memset(&x, 0, sizeof(x));
        x.kind = EE_WON_RTR_BAD_USER;
        x.user_id = r.user;
        x.expiration = r.expiry;
        x.moderator_id = r.modid;
        x.name = r.name.empty() ? NULL : r.name.data();
        x.name_len = r.name.size();
        x.moderator_name = r.mod.empty() ? NULL : r.mod.data();
        x.moderator_name_len = r.mod.size();
        x.comment = r.comment.empty() ? NULL : r.comment.data();
        x.comment_len = r.comment.size();
        aRecs.push_back(x);
    }
    {
        Font* aFont = new Font;
        aFont->mId = 3;
        aFont->mHeight = inFontHeight;
        SmartPtr<MultiListArea> aList = new MultiListArea;
        aList->mFont = aFont;
        if (inSide == 0) {
            std::pair<ListArea*, bool> aTarget(aList.get(), inSimple);
            for (const EeWonRouteRecord& x : aRecs) WonDlg34Orig::InsertBadUser(&aTarget, &x);
        } else {
            ee_won_lobbydlg_split_bad_users((intptr_t) static_cast<ListArea*>(aList.get()), aRecs.data(), aRecs.size(),
                                            inSimple, NewDlg);
        }
        Graphics g;
        std::vector<ListItem*> aItems;
        for (auto& p : aList->mItems) aItems.push_back(p.get());
        for (ListItem* i : aItems) {
            i->CalcDimensions(aList.get());
            Log(F("item %lld height %lld font refs %lld", i->mId, i->mHeight, aFont->mRefs));
        }
        for (ListItem* i : aItems)
            for (int sel = 0; sel < 2; sel++)
                for (int col = -2; col <= 6; col++) {
                    i->mSelected = sel != 0;
                    aList->mPaintColumn = col;
                    Log(F("paint %lld sel %lld col %lld", i->mId, sel, col));
                    i->Paint(g, aList.get());
                }
        for (int col = -2; col <= 7; col++) {
            aList->mSortColumn = col;
            for (ListItem* i : aItems)
                for (ListItem* j : aItems) {
                    int r = i->Compare(j, aList.get());
                    if (col < 0 || col > 5) Log(F("cmp %lld %lld %s", i->mId, j->mId) + (r == (i < j) ? "lt" : "WRONG"));
                    else Log(F("cmp %lld %lld = %lld", i->mId, j->mId, r));
                }
        }
        for (ListItem* i : aItems) {
            intptr_t id = inSide == 0 ? WonDlg34Orig::Host(NULL, EeWonLobbyDlgOp::BadUserId, (intptr_t)i, 0, 0)
                                      : ee_won_lobbydlg_split_op(EeWonLobbyDlgOp::BadUserId, (intptr_t)i, 0, 0, NewDlg);
            Log(F("id %lld %lld", i->mId, (long long)id));
        }
        Log("end of list");
    }
    return std::vector<std::string>(std::move(sTrace));
}

static void BadUsers(int inCases)
{
    for (int k = 0; k < inCases; k++) {
        std::vector<Rec> aRecs;
        int n = k < 4 ? k : (int)(Rand() % 7);
        for (int i = 0; i < n; i++) aRecs.push_back(Rec{RandUnits(), RandUnits(), RandUnits(), RandId(), RandExpiry(), RandId()});
        // duplicates for the equal compares
        if (n > 1 && Rand() % 3 == 0) aRecs.push_back(aRecs[0]);
        bool aSimple = Rand() % 2 == 0;
        int aHeight = (int)(Rand() % 20) - 2;
        std::vector<std::string> o = BadUsersSide(0, aRecs, aSimple, aHeight);
        std::vector<std::string> r = BadUsersSide(1, aRecs, aSimple, aHeight);
        Check("bad users", k, o, r);
    }
}

static void AdminTitles()
{
    static const char* kNames[] = {"", "Bob", "Some One"};
    static const EeWonLobbyRes kRes[] = {EeWonLobbyRes::AdminActionDialog_Warn_String, EeWonLobbyRes::AdminActionDialog_Ban_String,
                                         EeWonLobbyRes::LobbyGlobal_OK_String};
    int k = 0;
    for (const char* n : kNames)
        for (EeWonLobbyRes res : kRes) {
            std::vector<std::string> sides[2];
            for (int s = 0; s < 2; s++) {
                sTrace.clear();
                {
                    SmartPtr<Label> aLabel = new Label;
                    SmartPtr<LobbyClient> aClient = new LobbyClient;
                    aClient->mName = GUIString(n);
                    if (s == 0) WonDlg34Orig::Host(NULL, EeWonLobbyDlgOp::AdminTitleRes, (intptr_t)aLabel.get(), (intptr_t)res, (intptr_t)aClient.get());
                    else ee_won_lobbydlg_split_op(EeWonLobbyDlgOp::AdminTitleRes, (intptr_t)aLabel.get(), (intptr_t)res, (intptr_t)aClient.get(), NewDlg);
                }
                sides[s] = std::move(sTrace);
            }
            Check("admin title", k++, sides[0], sides[1]);
        }
    std::vector<std::string> sides[2];
    for (int s = 0; s < 2; s++) {
        sTrace.clear();
        sNextItem = 0;
        {
            SmartPtr<MultiListArea> aList = new MultiListArea;
            if (s == 0) WonDlg34Orig::Host(NULL, EeWonLobbyDlgOp::AdapterAddDefault, (intptr_t)aList.get(), 0, 0);
            else ee_won_lobbydlg_split_op(EeWonLobbyDlgOp::AdapterAddDefault, (intptr_t)aList.get(), 0, 0, NewDlg);
        }
        sides[s] = std::move(sTrace);
    }
    Check("default adapter", 0, sides[0], sides[1]);
}

// lobbydlg.rs's ListFiles loop (as it is) over one side's ops
typedef intptr_t (*SideOp)(EeWonLobbyDlgOp, intptr_t, intptr_t, intptr_t);
static intptr_t OrigOp(EeWonLobbyDlgOp o, intptr_t a, intptr_t b, intptr_t c) { return WonDlg34Orig::Host(NULL, o, a, b, c); }
static intptr_t NewOp(EeWonLobbyDlgOp o, intptr_t a, intptr_t b, intptr_t c) { return ee_won_lobbydlg_split_op(o, a, b, c, NewDlg); }
static std::vector<std::string> ListFiles(SideOp op, const std::string& inPattern, int inExtLen)
{
    typedef EeWonLobbyDlgOp Op;
    sTrace.clear();
    std::vector<std::string> aNames;
    {
        SmartPtr<ListArea> aList = new ListArea;
        intptr_t list = (intptr_t)aList.get();
        intptr_t find = op(Op::FindFirst, (intptr_t)inPattern.c_str(), 0, 0);
        Log(F("found %lld", find != 0));
        if (find != 0) {
            for (;;) {
                intptr_t length = op(Op::FindNameLength, find, 0, 0);
                if (length > inExtLen) op(Op::FileListInsert, list, find, length - inExtLen);
                if (op(Op::FindNext, find, 0, 0) == 0) break;
            }
            op(Op::FindClose, find, 0, 0);
        }
    }
    // the search's order is the file system's: compare the rows sorted
    std::sort(sTrace.begin(), sTrace.end());
    return std::vector<std::string>(std::move(sTrace));
}
static void Files()
{
    char aTemp[600];
    const char* t = getenv("TEMP");
    snprintf(aTemp, sizeof(aTemp), "%s\\ee_dlg34_%u", t != NULL ? t : ".", (unsigned)PlatTimeMilliseconds());
    std::string aDir = aTemp;
    PlatCreateDirectory(aDir.c_str());
    std::vector<std::string> aMade;
    int k = 0;
    for (int round = 0; round < 6; round++) {
        if (round > 0) {
            int n = (int)(Rand() % 6);
            for (int i = 0; i < n; i++) {
                std::string aName;
                int len = 1 + (int)(Rand() % 40);
                for (int j = 0; j < len; j++) aName += (char)('a' + Rand() % 26);
                static const char* kExt[] = {".ees", ".EES", ".txt", "", ".ees.bak"};
                aName += round == 5 && i == 0 ? std::string() : kExt[Rand() % 5];
                if (round == 5 && i == 0) aName = ".ees";
                std::string aPath = aDir + "\\" + aName;
                FILE* f = fopen(aPath.c_str(), "wb");
                if (f != NULL) { fclose(f); aMade.push_back(aPath); }
            }
        }
        static const char* kPatterns[] = {"*.ees", "*.txt", "*", "*.none"};
        for (const char* p : kPatterns) {
            std::string aPattern = aDir + "\\" + p;
            int anExt = (int)strlen(p) - 1;
            Check("files", k++, ListFiles(OrigOp, aPattern, anExt), ListFiles(NewOp, aPattern, anExt));
        }
    }
    std::string aMissing = aDir + "\\nowhere\\*.ees";
    Check("files", k++, ListFiles(OrigOp, aMissing, 4), ListFiles(NewOp, aMissing, 4));
    for (const std::string& p : aMade) remove(p.c_str());
    PlatRemoveDirectory(aDir.c_str());
}

static void Transfers()
{
    typedef EeWonEeXferOp Op;
    static const Op kOps[] = {Op::NewSocket, Op::RepeatAccept, Op::QueueSend, Op::QueueDownload};
    static const char* kPaths[] = {"", "data/Saved Games/a.ees", "x"};
    int k = 0;
    for (Op op : kOps)
        for (const char* path : kPaths)
            for (int host = 0; host < 2; host++) {
                std::vector<std::string> sides[2];
                for (int s = 0; s < 2; s++) {
                    sTrace.clear();
                    sNextOp = sNextSocket = 0;
                    {
                        SmartPtr<EELobbyFileTransfer> aTransfer = new EELobbyFileTransfer;
                        aTransfer->mHost.mHost = host ? 0x0a000002 : 0;
                        aTransfer->mHost.mPort = host ? 2301 : 0;
                        SmartPtr<QueueSocket> aSocket = new QueueSocket(7);
                        intptr_t a = (intptr_t) static_cast<AsyncSocket*>(aSocket.get());
                        size_t n = strlen(path);
                        intptr_t r = s == 0 ? WonDlg34Orig::XferHost(aTransfer.get(), op, a, (intptr_t)path, (intptr_t)n)
                                            : ee_won_eexfer_split_op(aTransfer.get(), op, a, (intptr_t)path, (intptr_t)n, NewXfer);
                        QueueSocket* aMade = op == Op::NewSocket ? static_cast<QueueSocket*>(reinterpret_cast<AsyncSocket*>(r)) : NULL;
                        Log(F("result socket %lld refs %lld", aMade != NULL ? aMade->mId : -1, aMade != NULL ? aMade->mRefs : -1));
                        if (aMade != NULL) aMade->Release();
                        Log(F("socket refs %lld queued %lld transfer refs %lld", aSocket->mRefs, (long long)aSocket->mQueue.size(), aTransfer->mRefs));
                    }
                    sides[s] = std::move(sTrace);
                }
                Check("transfer", k++, sides[0], sides[1]);
            }
}

static void Games()
{
    typedef EeWonEeGameOp Op;
    int k = 0;
    for (int slot = 0; slot < 8; slot++) {
        std::vector<std::string> sides[2];
        for (int s = 0; s < 2; s++) {
            sTrace.clear();
            EELobbyGame aGame;
            EELobbyPlayer aPlayer;
            aPlayer.mSlot = 9;
            if (s == 0) WonDlg34Orig::GameHost(&aGame, Op::PlayerAttach, (intptr_t)&aPlayer, slot, 0, 0);
            else ee_won_eegame_split_op(&aGame, Op::PlayerAttach, (intptr_t)&aPlayer, slot, 0, 0, NewGame);
            // the slot the player names: the view's game and number (the original), the two members (the new)
            long long aGameOf = s == 0 ? (aPlayer.mPlayerSlot != NULL ? 1 : 0) : (aPlayer.mSlotGame == &aGame ? 1 : aPlayer.mSlotGame != NULL ? 2 : 0);
            long long aIndex = s == 0 ? (aPlayer.mPlayerSlot != NULL ? aPlayer.mPlayerSlot->mIndex : -1) : (aPlayer.mSlotGame != NULL ? aPlayer.mSlotIndex : -1);
            Log(F("player slot %lld game %lld index %lld", aPlayer.mSlot, aGameOf, aIndex));
            sides[s] = std::move(sTrace);
        }
        Check("player attach", k++, sides[0], sides[1]);
    }
    static const unsigned kPorts[] = {0, 1, 2300, 65535, 65536, 0x12345};
    static const unsigned long kHosts[] = {0, 0x7f000001, 0xffffffff};
    for (unsigned port : kPorts)
        for (unsigned long host : kHosts) {
            std::vector<std::string> sides[2];
            for (int s = 0; s < 2; s++) {
                sTrace.clear();
                EELobbyGame aGame;
                aGame.mAddress.mHost = host;
                aGame.mAddress.mPort = 77;
                SmartPtr<EELobbyFileTransfer> aTransfer = new EELobbyFileTransfer;
                aTransfer->mHost.mPort = 5;
                if (s == 0) WonDlg34Orig::GameHost(&aGame, Op::TransferSetHost, (intptr_t)aTransfer.get(), port, 0, 0);
                else ee_won_eegame_split_op(&aGame, Op::TransferSetHost, (intptr_t)aTransfer.get(), port, 0, 0, NewGame);
                Log(F("host %llx port %lld", (long long)aTransfer->mHost.mHost, aTransfer->mHost.mPort));
                sides[s] = std::move(sTrace);
            }
            Check("transfer host", k++, sides[0], sides[1]);
        }
    static const char* kNames[] = {NULL, "", "save1.ees"};
    static const long long kNums[] = {0, 1, 0x7fffffff, 0xffffffffLL, -1, 0x123456789LL};
    for (const char* name : kNames)
        for (long long size : kNums)
            for (long long sum : kNums) {
                std::vector<std::string> sides[2];
                for (int s = 0; s < 2; s++) {
                    sTrace.clear();
                    EELobbyGame aGame;
                    {
                        SmartPtr<EELobbyFileTransfer> aTransfer = new EELobbyFileTransfer;
                        aTransfer->mRust = ee_won_eexfer_new(aTransfer.get(), XferWorld);
                        SmartPtr<GUIStringData> aName = name != NULL ? new GUIStringData(name) : NULL;
                        intptr_t r = s == 0 ? WonDlg34Orig::GameHost(&aGame, Op::TransferSetFileInfo, (intptr_t)aTransfer.get(), (intptr_t)aName.get(), (intptr_t)size, (intptr_t)sum)
                                            : ee_won_eegame_split_op(&aGame, Op::TransferSetFileInfo, (intptr_t)aTransfer.get(), (intptr_t)aName.get(), (intptr_t)size, (intptr_t)sum, NewGame);
                        Log("name '" + aTransfer->mName.mData->s + "'" + F(" result %lld size %lld sum %lld", (long long)r,
                            (long long)ee_won_eexfer_call(aTransfer->mRust, EeWonEeXferCall::Field, 2, 0),
                            (long long)ee_won_eexfer_call(aTransfer->mRust, EeWonEeXferCall::Field, 3, 0)));
                        ee_won_eexfer_free(aTransfer->mRust);
                    }
                    sides[s] = std::move(sTrace);
                }
                Check("transfer file info", k++, sides[0], sides[1]);
            }
    // the transfer's calls (R6 WON lobby/net round 40): on a transfer with a file's size and checksum, a port
    static const EeWonEeGameOp kCalls[] = {Op::TransferServe, Op::TransferSize, Op::TransferChecksum, Op::TransferStop,
                                           Op::TransferCloseClients, Op::TransferIsFileHere, Op::TransferDownload};
    for (EeWonEeGameOp call : kCalls)
        for (unsigned port : kPorts)
            for (long long size : kNums) {
                std::vector<std::string> sides[2];
                for (int s = 0; s < 2; s++) {
                    sTrace.clear();
                    EELobbyGame aGame;
                    {
                        SmartPtr<EELobbyFileTransfer> aTransfer = new EELobbyFileTransfer;
                        aTransfer->mRust = ee_won_eexfer_new(aTransfer.get(), XferWorld);
                        ee_won_eexfer_call(aTransfer->mRust, EeWonEeXferCall::SetFileInfo, (uint32_t)size, (uint32_t)(size * 3));
                        intptr_t r = s == 0 ? WonDlg34Orig::GameHost(&aGame, call, (intptr_t)aTransfer.get(), port, 0, 0)
                                            : ee_won_eegame_split_op(&aGame, call, (intptr_t)aTransfer.get(), port, 0, 0, NewGame);
                        Log(F("call %lld result %lld", (long long)call, (long long)r));
                        ee_won_eexfer_free(aTransfer->mRust);
                    }
                    sides[s] = std::move(sTrace);
                }
                Check("transfer call", k++, sides[0], sides[1]);
            }
}

// a read's answer through the shell: the value, the text, or the exception it rethrows
static std::string ReadSide(int inSide, EeWonEeGameOp inOp, const std::string& inData)
{
    ReadBuffer aBuffer;
    aBuffer.mData = inData;
    try {
        intptr_t r = inSide == 0 ? WonDlg34Orig::Threw(WonDlg34Orig::Read(inOp, &aBuffer))
                                 : WONAPI::WonRethrow(NewRead(inOp, (intptr_t)&aBuffer));
        if (inOp == EeWonEeGameOp::ReadWString) {
            GUIStringData* d = reinterpret_cast<GUIStringData*>(r);
            std::string s = "text '" + d->s + "'" + F(" refs %lld", d->mRefs);
            d->Release();
            return s + F(" pos %lld", (long long)aBuffer.mPos);
        }
        return F("value %lld pos %lld", (long long)r, (long long)aBuffer.mPos);
    } catch (ReadBufferException& e) {
        return "threw " + e.mMessage + F(" pos %lld", (long long)aBuffer.mPos);
    }
}
static void Reads()
{
    typedef EeWonEeGameOp Op;
    static const Op kOps[] = {Op::ReadByte, Op::ReadShort, Op::ReadBool, Op::ReadLong, Op::ReadWString};
    int k = 0;
    for (int len = 0; len <= 9; len++)
        for (int fill = 0; fill < 4; fill++)
            for (Op op : kOps) {
                std::string aData;
                for (int i = 0; i < len; i++) aData += (char)(fill == 0 ? 0 : fill == 1 ? 0xff : fill == 2 ? (i == 0 ? 2 : 'a' + i) : (i * 37 + 1));
                std::vector<std::string> sides[2];
                for (int s = 0; s < 2; s++) {
                    sTrace.clear();
                    std::string r = ReadSide(s, op, aData);
                    sTrace.push_back(r);
                    sides[s] = std::move(sTrace);
                }
                Check("read", k++, sides[0], sides[1]);
            }
}

static void Setup()
{
    for (int i = 0; i < (int)ee_won_resource_count() && i < 1024; i++) gLobbyResources[i].mString = GUIString(new GUIStringData(F("res%lld", i)));
}

static int RunAll(int inCases)
{
    sSeed = 12345;
    sMismatches = 0;
    BadUsers(inCases);
    AdminTitles();
    Files();
    Transfers();
    Games();
    Reads();
    return sMismatches;
}

int main()
{
    Setup();
    ee_won_lobby_rows_host_world(NewRows);
    sTrace.clear();
    int aReal = RunAll(3000);
    printf("won_dlg34_test: %d checks, %d mismatches\n", sChecks, aReal);
    // every new statement reached
    printf("new ops reached: dialog %d, rows %d, transfer %d, game %d\n", (int)sDlgHits.size(), (int)sRowHits.size(),
           (int)sXferHits.size(), (int)sGameHits.size());
    int aMissed = 0;
    const int kMutations = 19;
    for (sMutate = 1; sMutate <= kMutations; sMutate++) {
        int n = RunAll(300);
        printf("mutation %d: %d mismatches%s\n", sMutate, n, n == 0 ? " (MISSED)" : "");
        if (n == 0) aMissed++;
    }
    printf("mutations caught: %d of %d\n", kMutations - aMissed, kMutations);
    return aReal + aMissed;
}
