// won_hostops_test's stand-in world (R6 WONLobby round 29): the classes and lobby globals the control hosts' moved
// cases name (WonWorld::), each call logged in one trace. The deleted statements (won_hostops_orig.h) and the host
// cases that replaced them (won_hostops_test.cpp's World host: the new cases' statements, copied) run over it.
#ifndef WON_HOSTOPS_WORLD_H
#define WON_HOSTOPS_WORLD_H
#include <stdio.h>
#include <stdint.h>
#include <string>
#include <vector>
#include "ee/wonapi/WONEventTypes.h"
#include "rust/won_lobby/resource.h"
#include "rust/won_net/ipobj.h"

namespace WonWorld {
static std::vector<std::string> sTrace;
static void Log(const char* inFormat, long long a = 0, long long b = 0, long long c = 0, long long d = 0)
{
    char aBuffer[300];
    snprintf(aBuffer, sizeof(aBuffer), inFormat, a, b, c, d);
    sTrace.push_back(aBuffer);
}

struct RefCount {
    int mRefs = 1; // the world's own
    virtual ~RefCount() {}
    RefCount* CreateRef() { this->mRefs++; return this; }
    void AddRef() { this->mRefs++; }
    void Release() { this->mRefs--; }
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

struct GUIStringData : RefCount {
    std::string mText;
    GUIStringData() {}
    GUIStringData(const char* t) : mText(t) { this->mRefs = 0; }
    GUIStringData(const char* t, unsigned short n) : mText(t, n) { this->mRefs = 0; }
};
static GUIStringData* GetEmptyGUIStringData() { static GUIStringData sEmpty; return &sEmpty; }
// a text: its characters (the C++ class shares its data; the stand-in copies, which reads the same)
struct GUIString {
    std::string s;
    GUIString() {}
    GUIString(GUIStringData* d) : s(d != NULL ? d->mText : std::string()) {}
    GUIString(const char* t) : s(t) {}
    int length() const { return (int)this->s.size(); }
    void append(const char* t) { this->s += t; }
    void append(const GUIString& t) { this->s += t.s; }
};
static GUIString gEmptyGUIString;
// the format functions: the format with each argument in brackets after it
static GUIString LobbyFormatString(const GUIString& f, const GUIString& a) { return GUIString((f.s + "[" + a.s + "]").c_str()); }
static GUIString LobbyFormatString2(const GUIString& f, const GUIString& a, const GUIString& b)
{
    return GUIString((f.s + "[" + a.s + "][" + b.s + "]").c_str());
}

struct Component : RefCount {
    int mId = 0;
};
struct Container : Component {
    template <class E> void DeliverEvent(const E& e, bool inFlag) { e.Log(this->mId, inFlag); }
};
struct Label : Component {
    void SetText(const GUIString& t, bool inv = true) { Log(("Label %lld SetText " + t.s + " %lld").c_str(), this->mId, inv); }
};
struct ComboBox : Component {
    void* AddString(const GUIString& t, int pos = -1) { Log(("Combo %lld AddString " + t.s + " %lld").c_str(), this->mId, pos); return this; }
};
struct ListItem : RefCount {
    int mId = 0;
};
struct Font : RefCount {
    int GetStringWidth(const GUIString& t, int, int n) { Log("width %lld", n); return n * 5 + t.length(); }
};
struct TextListItem : ListItem {
    GUIString mText;
    SmartPtr<Font> mFont;
    TextListItem() {}
    TextListItem(const GUIString& t, Font* f) : mText(t), mFont(f) {}
};
struct IPAddr {
    intptr_t mValue = 0;
    static IPAddr FromValue(intptr_t v) { IPAddr a; a.mValue = v; return a; }
};
// the address clone (the cases' ee_won_ipaddr_clone: won_hostops_orig.h renames it)
static intptr_t IpClone(const ::EeWonIpAddr* a) { return (intptr_t)a ^ 0x5000; }
struct JoinRoomEvent {
    IPAddr mAddr;
    unsigned long mId;
    JoinRoomEvent(const IPAddr& a, unsigned long i) : mAddr(a), mId(i) {}
    void Log(int c, bool f) const { WonWorld::Log("ctl %lld JoinRoom %#llx %lld %lld", c, (long long)mAddr.mValue, (long long)mId, f); }
};
struct CreateRoomEvent {
    IPAddr mAddr;
    CreateRoomEvent(const IPAddr& a) : mAddr(a) {}
    void Log(int c, bool f) const { WonWorld::Log("ctl %lld CreateRoom %#llx %lld", c, (long long)mAddr.mValue, f); }
};
struct ComponentEvent {
    int mType = 0;
    virtual ~ComponentEvent() {}
};
// (laid out apart, as the classes are: an item read through the wrong class reads another member)
struct ComboSelChangedEvent : ComponentEvent { SmartPtr<ListItem> mItem; intptr_t mPad[3] = {}; };
struct TreeItemOpenedEvent : ComponentEvent { intptr_t mPad = 0; SmartPtr<ListItem> mItem; intptr_t mPad2[2] = {}; };
struct ListItemClickedEvent : ComponentEvent { intptr_t mPad[2] = {}; SmartPtr<ListItem> mItem; intptr_t mPad2 = 0; };
using WONAPI::ComponentEvent_ComboSelChanged;
using WONAPI::ComponentEvent_TreeItemOpened;

// the filters' persistence: a column's values
static std::vector<std::string> sFilterValues;
static size_t LobbyGetFilterCount(const GUIString& c) { Log(("FilterCount " + c.s).c_str()); return sFilterValues.size(); }
static GUIString LobbyGetFilterValueAt(const GUIString& c, size_t i) { return GUIString((c.s + ":" + sFilterValues[i]).c_str()); }

// the saved game dialog
struct EELobbyFileDialog : Component {
    void ListFiles(const GUIString& d, const std::string& e) { Log(("ListFiles " + d.s + " " + e).c_str()); }
};
static EELobbyFileDialog* sFileDialog;
static EELobbyFileDialog* EELobbyGetFileDialog() { Log("GetFileDialog"); return sFileDialog; }
// (a pure function: the original called it before the dialog, the Rust when it lists)
static const GUIString& EELobbyFn2988E9() { static GUIString sDir("data/Saved Games/"); return sDir; }
static int sDialogResult;
static int LobbyDoDialog(Component* d) { Log("DoDialog %lld refs %lld", d->mId, d->mRefs); return sDialogResult; }

struct Image : RefCount {
    int mId = 0, mWidth = 0, mHeight = 0;
    int GetWidth() { return this->mWidth; }
    int GetHeight() { return this->mHeight; }
};
struct Graphics {
    Font* mFont = NULL;
    void SetColor(unsigned long c) { Log("SetColor %#llx", (long long)c); }
    void FillRect(int x, int y, int w, int h) { Log("FillRect %lld %lld %lld %lld", x, y, w, h); }
    void DrawImage(Image* i, int x, int y) { Log("DrawImage %lld %lld %lld", i != NULL ? i->mId : -1, x, y); }
    void DrawString(const GUIString& t, int x, int y) { Log("DrawString %lld %lld %lld", t.length(), x, y); }
};

struct LobbyConfig {
    unsigned short mPingCeiling = 5000;
    bool mPublic = false;
};
static LobbyConfig* sConfig;
static LobbyConfig* GetLobbyConfig() { Log("GetLobbyConfig"); return sConfig; }

struct LobbyResource {
    GUIString mString;
    SmartPtr<Image> mImage;
};
static LobbyResource gLobbyResources[1024]; // (469 rows)
#define LOBBY_IMAGE(theName) (gLobbyResources[(int)EeWonLobbyRes::theName].mImage)
#define LOBBY_STRING(theName) (gLobbyResources[(int)EeWonLobbyRes::theName].mString)
// the EE lobby's strings: their names
#define EE_LOBBY_STRING(theName) GUIString(#theName)
} // namespace WonWorld
#endif
