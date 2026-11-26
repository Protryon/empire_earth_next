// The control host's compound cases R6 WON round 35 made Rust (ee_won_lobby hostops.rs's split_op) against the deleted LobbyNetCtrlHost.cpp code as it was at f2bdb70f7, over one stand-in world
// that logs every call ("WON goldens by reach": the Internet lobby's cases no LAN game runs, and the rows' and links'
// members the LAN game reaches only in part).
//
// - the rows' members (ItemGet / ItemSet, every field and an unknown one), the chat links' (LinkGet / LinkSet), the
//   link of a link event (EvGet);
// - the texts: TextNew, TextBytes into short and long buffers, TextAppend and TextAppendBytes on shared and unshared
//   texts (the references after), ParserCommandText with and without a name;
// - ChatAddFormatted with 0..2 strings and 0..3 links; DeliverCreateAccount over numbers past a byte and a short;
//   HttpDocPath for both owners, paths shorter and longer than the buffer;
// - (the configs' SetComponent calls of round 35 are gone with the configs' typed members: R6 WON lobby and network
//   round 39);
// - EELobbyGlueHost.cpp's options and players fields (OptionsField, OptionsPlayer, OptionSet, PlayerSet: every field,
//   values past 32 bits; the LAN game never reads a player of the options);
// - EELobbyGlueHost.cpp's colour chooser: ChooserNew and OpenPopup (eecolor.rs; the LAN game opens no chooser).
// - the game list's EE columns (EeColumn, added in round 35 of the lobby and network lane): every column call, the keeps over filter texts (the text the glue
//   is handed and its references), over eeglue.rs's calls on a stand-in glue host.
//
// The new side is the Rust over the host's one-statement cases, copied as they are (World below). Each case runs on
// both sides over worlds made alike; the traces (calls, results, the objects' state after) must be equal. Then every
// new op is skipped in turn (the World answers 0 and does nothing): each skip must make some case differ. The exit
// code is the real pass's mismatches plus the skips not caught.
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include <map>
#include "rust/won_lobby/hostops.h"
#include "rust/won_lobby/lobbyctl.h"
#include "rust/won_lobby/chatctl.h"
#include "rust/won_lobby/lobbyevt.h"
#include "rust/won_lobby/lobbycfgrd.h"
#include "rust/won_lobby/eecolor.h"
#include "rust/won_lobby/eeglue.h"

typedef EeWonLobbyCtlOp Op;

// ---- the world

static std::vector<std::string> sTrace;
static void Log(const std::string& s) { sTrace.push_back(s); }
static std::string N(long long v) { return std::to_string(v); }

struct RefCount {
    int mRefs = 0;
    virtual ~RefCount() {}
    void AddRef() { this->mRefs++; }
    void Release() { this->mRefs--; }
};
template <class T> struct SmartPtr {
    T* mPtr = NULL;
    SmartPtr() {}
    SmartPtr(T* p) : mPtr(p) { if (p) p->AddRef(); }
    SmartPtr(const SmartPtr& o) : mPtr(o.mPtr) { if (mPtr) mPtr->AddRef(); }
    ~SmartPtr() { if (mPtr) mPtr->Release(); }
    SmartPtr& operator=(T* p) { if (p) p->AddRef(); if (mPtr) mPtr->Release(); mPtr = p; return *this; }
    SmartPtr& operator=(const SmartPtr& o) { return *this = o.mPtr; }
    T* get() const { return mPtr; }
    T* operator->() const { return mPtr; }
};

// a text's data: its bytes and its references (the datas made are kept to the end: their counts are compared)
struct GUIStringData : RefCount {
    std::string mText;
    GUIStringData(const char* t) : mText(t) {}
    GUIStringData(const char* t, unsigned short n) : mText(t, n) {}
    int ToInt(int) const { return atoi(this->mText.c_str()); }
};
static std::vector<GUIStringData*> sDatas;
static GUIStringData* NewData(const std::string& t) { GUIStringData* d = new GUIStringData(t.c_str()); sDatas.push_back(d); return d; }
static GUIStringData sEmpty("");
static GUIStringData* GetEmptyGUIStringData() { return &sEmpty; }
// a text shares its data; an append to shared data copies it first (the class's copy on write)
struct GUIString {
    SmartPtr<GUIStringData> mData;
    GUIString() : mData(&sEmpty) {}
    GUIString(GUIStringData* d) : mData(d) {}
    GUIString(const char* t) : mData(NewData(t)) {}
    std::string Own()
    {
        if (this->mData->mRefs > 1) this->mData = NewData(this->mData->mText);
        return this->mData->mText;
    }
    void append(const GUIString& t) { std::string s = this->Own(); this->mData->mText = s + t.mData->mText; }
    void append(int c) { std::string s = this->Own(); this->mData->mText = s + (char)c; }
    operator std::string() const { return this->mData->mText; }
    operator std::wstring() const { return std::wstring(this->mData->mText.begin(), this->mData->mText.end()); }
};
static GUIString Txt(intptr_t inData) { return inData != 0 ? GUIString((GUIStringData*)inData) : GUIString(); }
static intptr_t Held(const GUIString& inText)
{
    GUIStringData* aData = inText.mData.get();
    aData->AddRef();
    return (intptr_t)aData;
}
static std::string Show(intptr_t d) { return d ? "'" + ((GUIStringData*)d)->mText + "'/" + N(((GUIStringData*)d)->mRefs) : "null"; }

struct Font { int mId; };
struct TextSegmentBase : RefCount { int mId = 0; };
struct ChatLink : TextSegmentBase {
    int mKind = 0, mColor = 0;
    SmartPtr<RefCount> mClient, mFriend;
    bool mEnabled = false;
    SmartPtr<ChatLink> mNext;
    ChatLink* mPrevious = NULL;
};
struct ComponentEvent { virtual ~ComponentEvent() {} };
struct TextLinkActivatedEvent : ComponentEvent { SmartPtr<TextSegmentBase> mLink; };
struct ListItem : RefCount { int mHeight = 0, mTop = 0, mWidth = 0; bool mSelected = false, mSelectable = false; };
struct TextListItem : ListItem { int mColor = 0; };
struct TextArea { int mId = 0; };
struct InputBox {
    GUIString mText;
    const GUIString& GetText() { Log("GetText " + (std::string)this->mText); return this->mText; }
};
struct Button {
    bool mChecked = false;
    bool IsChecked() { Log("IsChecked"); return this->mChecked; }
};
struct ChatCommandParser {
    int mPrefix = '/';
    GUIString mTarget;
    int GetPrefix() { Log("GetPrefix"); return this->mPrefix; }
    GUIString GetCommandString(int c) { Log("GetCommandString " + N(c)); return GUIString(("cmd" + N(c)).c_str()); }
    GUIString GetTargetName() { Log("GetTargetName"); return this->mTarget; }
};
enum { EE_WON_HTTPDOC_PATH = 3, HTTPDocOwner_Sys = 0, HTTPDocOwner_Game = 1 };
struct RustServerOp {
    std::string mPaths[2];
    std::string GetBytes(int w, int owner) { Log("GetBytes " + N(w) + " " + N(owner)); return this->mPaths[owner]; }
};
typedef EeWonLobbyEvtField LobbyEvtField;
struct LobbyEvent : RefCount { int mId = 0; };
static LobbyEvent* CreateAccountEvent() { LobbyEvent* e = new LobbyEvent; Log("CreateAccountEvent"); return e; }
static void LobbyEvtSetStr(LobbyEvent* e, LobbyEvtField f, const std::wstring& s)
{
    Log("SetWide " + N((long long)f) + " " + std::string(s.begin(), s.end()) + " refs " + N(e->mRefs));
}
static void LobbyEvtSetStr(LobbyEvent* e, LobbyEvtField f, const std::string& s) { Log("SetNarrow " + N((long long)f) + " " + s + " refs " + N(e->mRefs)); }
static void LobbyEvtSet(LobbyEvent* e, LobbyEvtField f, intptr_t v) { Log("Set " + N((long long)f) + " " + N(v) + " refs " + N(e->mRefs)); }
struct Container {
    void DeliverEvent(LobbyEvent* e, bool f) { Log("Deliver refs " + N(e->mRefs) + " " + N(f)); }
};
static void LobbyAppendFormattedText(TextArea* inArea, const GUIString& inFormat, const GUIString* const* inStrings, int inNumStrings,
                                     const int* inColors, int inNumColors, Font* const* inFonts, int inNumFonts,
                                     TextSegmentBase* const* inSegments, int inNumSegments, bool inLineBreak)
{
    std::string s = "Append " + N(inArea->mId) + " " + (std::string)inFormat + " n " + N(inNumStrings);
    // (the shared empty text's count is every live empty text's in the world: not the case's)
    for (int i = 0; inStrings && i < 2; i++)
        s += " s" + N(i) + "=" + (std::string)*inStrings[i] + "/" + (inStrings[i]->mData.get() == &sEmpty ? std::string("empty") : N(inStrings[i]->mData->mRefs));
    for (int i = 0; i < inNumColors; i++) s += " c" + N(inColors[i]);
    for (int i = 0; i < inNumFonts; i++) s += " f" + N(inFonts[i] ? inFonts[i]->mId : -1);
    s += " links " + N(inNumSegments);
    for (int i = 0; inSegments && i < 3; i++) s += " l" + N(inSegments[i] ? inSegments[i]->mId : -1);
    Log(s + " lb " + N(inLineBreak));
}

#define WON_GUARDED_V(x) ((x), (intptr_t)0)

// ---- the deleted cases, as they were (LobbyNetCtrlHost.cpp at f2bdb70f7; EvField's and ItemField's helpers too)

namespace Orig {
static intptr_t EvField(ComponentEvent* e, EeWonLobbyCtlEvField f)
{
    if (f == EeWonLobbyCtlEvField::TlaLink)
        return (intptr_t)dynamic_cast<ChatLink*>(static_cast<TextLinkActivatedEvent*>(e)->mLink.get());
    return 0;
}
static intptr_t ItemField(ListItem* inItem, intptr_t inField, bool inSet, intptr_t inValue)
{
    typedef EeWonLobbyCtlItemField F;
    switch ((F)inField) {
    case F::Height: if (inSet) inItem->mHeight = (int)inValue; return inItem->mHeight;
    case F::Top: if (inSet) inItem->mTop = (int)inValue; return inItem->mTop;
    case F::Width: if (inSet) inItem->mWidth = (int)inValue; return inItem->mWidth;
    case F::Selected: if (inSet) inItem->mSelected = inValue != 0; return inItem->mSelected ? 1 : 0;
    case F::Selectable: if (inSet) inItem->mSelectable = inValue != 0; return inItem->mSelectable ? 1 : 0;
    case F::Color:
        if (inSet) static_cast<TextListItem*>(inItem)->mColor = inValue;
        return (intptr_t)static_cast<TextListItem*>(inItem)->mColor;
    default: break; // the friend and user rows' members are rows.rs's
    }
    return 0;
}
static intptr_t Host(Container* inCtl, EeWonLobbyCtlOp inOp, intptr_t a, intptr_t b, intptr_t c)
{
    typedef EeWonLobbyCtlLinkField L;
    ChatLink* aLink = reinterpret_cast<ChatLink*>(a);
    ChatCommandParser* aParser = reinterpret_cast<ChatCommandParser*>(a);
    switch (inOp) {
    case Op::EvGet: return EvField(reinterpret_cast<ComponentEvent*>(a), (EeWonLobbyCtlEvField)b);
    case Op::LinkGet:
        switch ((L)b) {
        case L::Kind: return aLink->mKind;
        case L::Client: return (intptr_t)aLink->mClient.get();
        case L::Friend: return (intptr_t)aLink->mFriend.get();
        case L::Next: return (intptr_t)aLink->mNext.get();
        case L::Previous: return (intptr_t)aLink->mPrevious;
        default: return 0;
        }
    case Op::LinkSet:
        switch ((L)b) {
        case L::Color: aLink->mColor = (int)c; return 0;
        case L::Enabled: aLink->mEnabled = c != 0; return 0;
        case L::Next: aLink->mNext = reinterpret_cast<ChatLink*>(c); return 0;
        case L::Previous: aLink->mPrevious = reinterpret_cast<ChatLink*>(c); return 0;
        default: return 0;
        }
    case Op::ChatAddFormatted: {
        const EeWonLobbyCtlFormat* f = reinterpret_cast<const EeWonLobbyCtlFormat*>(a);
        GUIString aStrings[2] = {Txt(f->strings[0]), Txt(f->strings[1])};
        const GUIString* aText[2] = {&aStrings[0], &aStrings[1]};
        TextSegmentBase* aLinks[3] = {reinterpret_cast<ChatLink*>(f->links[0]), reinterpret_cast<ChatLink*>(f->links[1]),
                                      reinterpret_cast<ChatLink*>(f->links[2])};
        Font* aFonts[3] = {reinterpret_cast<Font*>(f->fonts[0]), reinterpret_cast<Font*>(f->fonts[1]),
                           reinterpret_cast<Font*>(f->fonts[2])};
        LobbyAppendFormattedText(reinterpret_cast<TextArea*>(f->output), Txt(f->format), f->num_strings ? aText : NULL,
                                 f->num_strings, f->colors, 9, aFonts, 3, f->num_links ? aLinks : NULL, f->num_links,
                                 f->line_break != 0);
        return 0;
    }
    case Op::ParserCommandText: {
        // the prefix, the command, then the quoted name (the target's without one)
        GUIString aText(GetEmptyGUIStringData());
        aText.append(aParser->GetPrefix());
        aText.append(aParser->GetCommandString((int)b));
        aText.append(GUIString(" \""));
        aText.append(c != 0 ? Txt(c) : aParser->GetTargetName());
        aText.append(GUIString(NewData("\" ")));
        return Held(aText);
    }
    case Op::TextAppend: case Op::TextAppendBytes: {
        GUIString aText((GUIStringData*)a);
        ((GUIStringData*)a)->Release();
        if (inOp == Op::TextAppend) aText.append(Txt(b));
        else aText.append(GUIString(NewData(std::string(reinterpret_cast<const char*>(b), (unsigned short)c))));
        return Held(aText);
    }
    case Op::DeliverCreateAccount: {
        const intptr_t* f = reinterpret_cast<const intptr_t*>(a);
        typedef LobbyEvtField EF;
        SmartPtr<LobbyEvent> event = CreateAccountEvent();
        LobbyEvtSetStr(event.get(), EF::UserName, (std::wstring)reinterpret_cast<InputBox*>(f[0])->GetText());
        LobbyEvtSetStr(event.get(), EF::Password, (std::wstring)reinterpret_cast<InputBox*>(f[1])->GetText());
        LobbyEvtSetStr(event.get(), EF::Email, (std::string)reinterpret_cast<InputBox*>(f[2])->GetText());
        LobbyEvtSet(event.get(), EF::Day, (unsigned char)reinterpret_cast<InputBox*>(f[3])->GetText().mData->ToInt(0));
        LobbyEvtSet(event.get(), EF::Month, (unsigned char)reinterpret_cast<InputBox*>(f[4])->GetText().mData->ToInt(0));
        LobbyEvtSet(event.get(), EF::Year, (unsigned short)reinterpret_cast<InputBox*>(f[5])->GetText().mData->ToInt(0));
        LobbyEvtSet(event.get(), EF::Newsletter, reinterpret_cast<Button*>(f[6])->IsChecked());
        inCtl->DeliverEvent(event.get(), false);
        return 0;
    }
    case Op::HttpDocPath: {
        std::string aPath = reinterpret_cast<RustServerOp*>(a)->GetBytes(EE_WON_HTTPDOC_PATH, b == 0 ? HTTPDocOwner_Sys : HTTPDocOwner_Game);
        memcpy(reinterpret_cast<char*>(c), aPath.data(), aPath.length() < 1024 ? aPath.length() : 1024);
        return (intptr_t)aPath.length();
    }
    case Op::TextNew: {
        GUIStringData* aData = NewData(std::string(reinterpret_cast<const char*>(a), (unsigned short)b));
        aData->AddRef();
        return (intptr_t)aData;
    }
    case Op::TextBytes: {
        std::string aBytes = Txt(a);
        memcpy(reinterpret_cast<char*>(b), aBytes.data(), aBytes.length() < (size_t)c ? aBytes.length() : (size_t)c);
        return (intptr_t)aBytes.length();
    }
    case Op::EeColumn:
        if ((EeWonEeGlueCall)a == EeWonEeGlueCall::EpochKeep || (EeWonEeGlueCall)a == EeWonEeGlueCall::MapTypeKeep) {
            GUIString aText = Txt(c);
            return ee_won_eeglue_call((EeWonEeGlueCall)a, b, (intptr_t)&aText, 0);
        }
        return ee_won_eeglue_call((EeWonEeGlueCall)a, b, c, 0);
    case Op::ItemGet: return ItemField(reinterpret_cast<ListItem*>(a), b, false, 0);
    case Op::ItemSet: ItemField(reinterpret_cast<ListItem*>(a), b, true, c); return 0;
    default: return 0;
    }
}
} // namespace Orig

// ---- the new host's one-statement cases (LobbyNetCtrlHost.cpp's, as they are; GUIStringData's constructor the
// world's NewData), the skipped op answering 0

static int sSkip = -1;
static std::string sEELobbyBytes;
static GUIString sEELobbyText;
static GUIString sEELobbyFmtStrings[2];
static const GUIString* const sEELobbyFmtText[2] = {&sEELobbyFmtStrings[0], &sEELobbyFmtStrings[1]};
static TextSegmentBase* sEELobbyFmtLinks[3];
static Font* sEELobbyFmtFonts[3];
static SmartPtr<LobbyEvent> sEELobbyEvent;
static GUIString sEELobbyColumnText;
static const EeWonLobbyCtlFormat* Fmt(intptr_t inFormat) { return reinterpret_cast<const EeWonLobbyCtlFormat*>(inFormat); }

static intptr_t World(void* inCtlArg, EeWonLobbyCtlOp inOp, intptr_t a, intptr_t b, intptr_t c)
{
    Container* inCtl = static_cast<Container*>(inCtlArg);
    ChatLink* aLink = reinterpret_cast<ChatLink*>(a);
    TextListItem* aText = static_cast<TextListItem*>(reinterpret_cast<ListItem*>(a));
    // (a skipped maker answers another object: a null one would only crash the next statement)
    if ((int)inOp == sSkip && inOp == Op::TextAlloc) return (intptr_t)NewData("skipped");
    if ((int)inOp == sSkip && inOp == Op::EvCreateAccountNew) { sEELobbyEvent = new LobbyEvent; return 0; }
    if ((int)inOp == sSkip) return 0;
    switch (inOp) {
    // the ops the composition calls that were there before (LobbyCtrlHost.cpp's and this host's)
    case Op::AddRef: reinterpret_cast<RefCount*>(a)->AddRef(); return 0;
    case Op::Release: reinterpret_cast<RefCount*>(a)->Release(); return 0;
    case Op::InputText: return (intptr_t)reinterpret_cast<InputBox*>(a)->GetText().mData.get();
    case Op::TextToInt: return Txt(a).mData->ToInt(0);
    case Op::ButtonIsChecked: return reinterpret_cast<Button*>(a)->IsChecked() ? 1 : 0;
    // the new cases
    case Op::EvTlaLink: return (intptr_t)dynamic_cast<ChatLink*>(static_cast<TextLinkActivatedEvent*>(reinterpret_cast<ComponentEvent*>(a))->mLink.get());
    case Op::LinkGetKind: return aLink->mKind;
    case Op::LinkGetClient: return (intptr_t)aLink->mClient.get();
    case Op::LinkGetFriend: return (intptr_t)aLink->mFriend.get();
    case Op::LinkGetNext: return (intptr_t)aLink->mNext.get();
    case Op::LinkGetPrevious: return (intptr_t)aLink->mPrevious;
    case Op::LinkSetColor: aLink->mColor = (int)b; return 0;
    case Op::LinkSetEnabled: aLink->mEnabled = b != 0; return 0;
    case Op::LinkSetNext: aLink->mNext = reinterpret_cast<ChatLink*>(b); return 0;
    case Op::LinkSetPrevious: aLink->mPrevious = reinterpret_cast<ChatLink*>(b); return 0;
    case Op::ItemGetHeight: return reinterpret_cast<ListItem*>(a)->mHeight;
    case Op::ItemGetTop: return reinterpret_cast<ListItem*>(a)->mTop;
    case Op::ItemGetWidth: return reinterpret_cast<ListItem*>(a)->mWidth;
    case Op::ItemGetSelected: return reinterpret_cast<ListItem*>(a)->mSelected ? 1 : 0;
    case Op::ItemGetSelectable: return reinterpret_cast<ListItem*>(a)->mSelectable ? 1 : 0;
    case Op::ItemGetColor: return (intptr_t)aText->mColor;
    case Op::ItemSetHeight: reinterpret_cast<ListItem*>(a)->mHeight = (int)b; return 0;
    case Op::ItemSetTop: reinterpret_cast<ListItem*>(a)->mTop = (int)b; return 0;
    case Op::ItemSetWidth: reinterpret_cast<ListItem*>(a)->mWidth = (int)b; return 0;
    case Op::ItemSetSelected: reinterpret_cast<ListItem*>(a)->mSelected = b != 0; return 0;
    case Op::ItemSetSelectable: reinterpret_cast<ListItem*>(a)->mSelectable = b != 0; return 0;
    case Op::ItemSetColor: aText->mColor = b; return 0;
    case Op::TextAlloc: return (intptr_t)NewData(std::string(reinterpret_cast<const char*>(a), (unsigned short)b));
    case Op::BytesOfText: sEELobbyBytes = (std::string)Txt(a); return 0;
    case Op::BytesOfHttpDocSys: sEELobbyBytes = reinterpret_cast<RustServerOp*>(a)->GetBytes(EE_WON_HTTPDOC_PATH, HTTPDocOwner_Sys); return 0;
    case Op::BytesOfHttpDocGame: sEELobbyBytes = reinterpret_cast<RustServerOp*>(a)->GetBytes(EE_WON_HTTPDOC_PATH, HTTPDocOwner_Game); return 0;
    case Op::BytesLen: return (intptr_t)sEELobbyBytes.length();
    case Op::BytesCopy: memcpy(reinterpret_cast<char*>(a), sEELobbyBytes.data(), (size_t)b); return 0;
    case Op::BytesDrop: std::string().swap(sEELobbyBytes); return 0;
    case Op::TextTake: sEELobbyText = GUIString((GUIStringData*)a); return 0;
    case Op::TextStartEmpty: sEELobbyText = GUIString(GetEmptyGUIStringData()); return 0;
    case Op::TextAppendText: sEELobbyText.append(Txt(a)); return 0;
    case Op::TextAppendRaw: sEELobbyText.append(GUIString(NewData(std::string(reinterpret_cast<const char*>(a), (unsigned short)b)))); return 0;
    case Op::TextAppendParserPrefix: sEELobbyText.append(reinterpret_cast<ChatCommandParser*>(a)->GetPrefix()); return 0;
    case Op::TextAppendParserCommand: sEELobbyText.append(reinterpret_cast<ChatCommandParser*>(a)->GetCommandString((int)b)); return 0;
    case Op::TextAppendQuoteOpen: sEELobbyText.append(GUIString(" \"")); return 0;
    case Op::TextAppendParserTarget: sEELobbyText.append(reinterpret_cast<ChatCommandParser*>(a)->GetTargetName()); return 0;
    case Op::TextAppendQuoteClose: sEELobbyText.append(GUIString(NewData("\" "))); return 0;
    case Op::TextHeldOut: return Held(sEELobbyText);
    case Op::TextDrop: sEELobbyText = GUIString(); return 0;
    case Op::FmtSetString: sEELobbyFmtStrings[a] = Txt(b); return 0;
    case Op::FmtSetLink: sEELobbyFmtLinks[a] = reinterpret_cast<ChatLink*>(b); return 0;
    case Op::FmtSetFont: sEELobbyFmtFonts[a] = reinterpret_cast<Font*>(b); return 0;
    case Op::FmtTexts: return (intptr_t)sEELobbyFmtText;
    case Op::FmtLinks: return (intptr_t)sEELobbyFmtLinks;
    case Op::FmtAppend: LobbyAppendFormattedText(reinterpret_cast<TextArea*>(Fmt(a)->output), Txt(Fmt(a)->format), reinterpret_cast<const GUIString* const*>(b), Fmt(a)->num_strings, Fmt(a)->colors, 9, sEELobbyFmtFonts, 3, reinterpret_cast<TextSegmentBase* const*>(c), Fmt(a)->num_links, Fmt(a)->line_break != 0); return 0;
    case Op::EvCreateAccountNew: sEELobbyEvent = CreateAccountEvent(); return 0;
    case Op::EvSetWide: LobbyEvtSetStr(sEELobbyEvent.get(), (LobbyEvtField)a, (std::wstring)reinterpret_cast<InputBox*>(b)->GetText()); return 0;
    case Op::EvSetNarrow: LobbyEvtSetStr(sEELobbyEvent.get(), (LobbyEvtField)a, (std::string)reinterpret_cast<InputBox*>(b)->GetText()); return 0;
    case Op::EvSetNum: LobbyEvtSet(sEELobbyEvent.get(), (LobbyEvtField)a, b); return 0;
    case Op::EvDeliverHeld: inCtl->DeliverEvent(sEELobbyEvent.get(), false); return 0;
    case Op::EvDrop: sEELobbyEvent = NULL; return 0;
    case Op::ColumnTextSet: sEELobbyColumnText = Txt(a); return 0;
    case Op::ColumnText: return (intptr_t)&sEELobbyColumnText;
    default: Log("unexpected op " + N((int)inOp)); return 0;
    }
}


// ---- the cases

static int sChecks, sFailures;
static std::map<std::string, int> sFailed;
static void Compare(const std::string& inWhat, const std::vector<std::string>& o, const std::vector<std::string>& n)
{
    sChecks++;
    if (o == n) return;
    sFailures++;
    if (sSkip >= 0 || sFailed[inWhat]++ > 2) return;
    printf("MISMATCH %s\n", inWhat.c_str());
    size_t i = 0;
    while (i < o.size() && i < n.size() && o[i] == n[i]) i++;
    printf("  orig[%zu] %s\n  new [%zu] %s\n", i, i < o.size() ? o[i].c_str() : "(end)", i, i < n.size() ? n[i].c_str() : "(end)");
}

// one world: the objects a case reads and writes
struct Objects {
    Container mCtl;
    ChatLink mLinks[4];
    RefCount mClient, mFriend;
    TextLinkActivatedEvent mEvent;
    TextListItem mItem;
    ChatCommandParser mParser;
    InputBox mInputs[6];
    Button mCheck;
    RustServerOp mDoc;
    TextArea mArea;
    Font mFonts[3] = {{11}, {12}, {13}};
    char mBuffer[1100];
    Objects()
    {
        for (int i = 0; i < 4; i++) { this->mLinks[i].mId = 100 + i; this->mLinks[i].mKind = i + 1; }
        this->mArea.mId = 7;
        memset(this->mBuffer, '.', sizeof(this->mBuffer));
    }
    ~Objects()
    {
        for (ChatLink& l : this->mLinks) l.mNext = NULL;
        this->mEvent.mLink = NULL;
    }
    std::string State()
    {
        std::string s = "item " + N(mItem.mHeight) + " " + N(mItem.mTop) + " " + N(mItem.mWidth) + " " + N(mItem.mSelected) + " " +
                         N(mItem.mSelectable) + " " + N(mItem.mColor);
        for (ChatLink& l : mLinks)
            s += " link " + N(l.mColor) + " " + N(l.mEnabled) + " " + N(l.mNext.get() ? l.mNext->mId : -1) + " " +
                 N(l.mPrevious ? l.mPrevious->mId : -1) + " refs " + N(l.mRefs);
        s += " buffer " + std::string(mBuffer, 40) + " @1023 " + std::string(mBuffer + 1020, 8);
        s += " bytes '" + sEELobbyBytes + "' text " + (std::string)sEELobbyText + " event " + N(sEELobbyEvent.get() != NULL);
        return s;
    }
};

typedef std::vector<std::string> Trace;
// a case: its world made by `inMake` twice; `inRun` on the original host, then through the Rust over World
template <class Make, class Run> static void Case(const std::string& inWhat, Make inMake, Run inRun)
{
    Trace t[2];
    for (int side = 0; side < 2; side++) {
        Objects* o = new Objects;
        size_t aDatas = sDatas.size();
        inMake(*o);
        sTrace.clear();
        intptr_t r = inRun(*o, side == 1);
        Log("result " + N(r) + " " + o->State());
        for (size_t i = aDatas; i < sDatas.size(); i++) Log("data " + N(i - aDatas) + " " + Show((intptr_t)sDatas[i]));
        t[side] = sTrace;
        delete o; // (the datas are leaked on purpose: their counts were compared)
    }
    Compare(inWhat, t[0], t[1]);
}
static intptr_t Call(Objects& o, bool inNew, Op inOp, intptr_t a, intptr_t b, intptr_t c)
{
    if (!inNew) return Orig::Host(&o.mCtl, inOp, a, b, c);
    intptr_t r = ee_won_lobby_hostops_ctl_op(&o.mCtl, inOp, a, b, c, World);
    return r == INTPTR_MIN ? 0 : r; // not answered there: the host's default (0, no statement)
}

static void Run()
{
    typedef EeWonLobbyCtlLinkField L;
    // the rows' members: every field read and written, and fields not the host's
    for (int f = 0; f <= 14; f++)
        for (intptr_t v : {(intptr_t)0, (intptr_t)1, (intptr_t)-7, (intptr_t)0x123456789}) {
            Case("ItemField " + N(f), [&](Objects& o) { o.mItem.mHeight = 3; o.mItem.mTop = 4; o.mItem.mWidth = 5; o.mItem.mSelected = true; o.mItem.mColor = 9; },
                 [&](Objects& o, bool n) {
                     intptr_t r = Call(o, n, Op::ItemSet, (intptr_t)(ListItem*)&o.mItem, f, v);
                     return r * 1000 + Call(o, n, Op::ItemGet, (intptr_t)(ListItem*)&o.mItem, f, 0);
                 });
        }
    // the links: each member read and written
    for (int f = 0; f <= 8; f++)
        for (int v = 0; v < 3; v++)
            Case("Link " + N(f) + " " + N(v), [&](Objects& o) { o.mLinks[0].mClient = &o.mClient; o.mLinks[0].mFriend = &o.mFriend; o.mLinks[0].mNext = &o.mLinks[2]; },
                 [&](Objects& o, bool n) {
                     bool aPointer = f == (int)L::Next || f == (int)L::Previous;
                     intptr_t w = v == 0 ? 0 : aPointer ? (intptr_t)&o.mLinks[v] : v;
                     Call(o, n, Op::LinkSet, (intptr_t)&o.mLinks[0], f, w);
                     intptr_t r = Call(o, n, Op::LinkGet, (intptr_t)&o.mLinks[0], f, 0);
                     return r == (intptr_t)&o.mClient ? -1 : r == (intptr_t)&o.mFriend ? -2 : r >= (intptr_t)&o.mLinks[0] && r <= (intptr_t)&o.mLinks[3] ? 1000 + ((ChatLink*)r)->mId : r;
                 });
    // the link of a link event, and the other fields
    for (int f = 0; f <= (int)EeWonLobbyCtlEvField::TlaLink + 2; f++)
        for (int k = 0; k < 2; k++)
            Case("EvGet " + N(f), [&](Objects& o) { if (k) o.mEvent.mLink = &o.mLinks[1]; },
                 [&](Objects& o, bool n) {
                     intptr_t r = Call(o, n, Op::EvGet, (intptr_t)(ComponentEvent*)&o.mEvent, f, 0);
                     return r == 0 ? 0 : (intptr_t)((ChatLink*)r)->mId;
                 });
    // the texts
    const char* kTexts[] = {"", "a", "hello", "0123456789abcdef"};
    for (const char* t : kTexts) {
        Case(std::string("TextNew ") + t, [](Objects&) {}, [&](Objects& o, bool n) { return (intptr_t)Show(Call(o, n, Op::TextNew, (intptr_t)t, strlen(t), 0)).size(); });
        for (intptr_t size : {(intptr_t)0, (intptr_t)1, (intptr_t)5, (intptr_t)40, (intptr_t)-1})
            Case(std::string("TextBytes ") + t + " " + N(size), [](Objects&) {},
                 [&](Objects& o, bool n) { return Call(o, n, Op::TextBytes, (intptr_t)NewData(t), (intptr_t)o.mBuffer, size); });
        for (int shared = 0; shared < 2; shared++)
            for (int bytes = 0; bytes < 2; bytes++)
                Case(std::string("TextAppend ") + t + " " + N(shared) + N(bytes), [](Objects&) {},
                     [&](Objects& o, bool n) {
                         GUIStringData* d = NewData(t);
                         d->AddRef(); // the caller's
                         if (shared) d->AddRef();
                         intptr_t r = bytes ? Call(o, n, Op::TextAppendBytes, (intptr_t)d, (intptr_t)"xyz", 2)
                                            : Call(o, n, Op::TextAppend, (intptr_t)d, (intptr_t)NewData("+tail"), 0);
                         Log("in " + Show((intptr_t)d) + " out " + Show(r));
                         return (intptr_t)(r == (intptr_t)d);
                     });
        for (int name = 0; name < 2; name++)
            Case(std::string("ParserCommandText ") + t + N(name), [&](Objects& o) { o.mParser.mTarget = GUIString(t); },
                 [&](Objects& o, bool n) {
                     intptr_t r = Call(o, n, Op::ParserCommandText, (intptr_t)&o.mParser, 4, name ? (intptr_t)NewData("Bob") : 0);
                     Log("out " + Show(r));
                     return (intptr_t)0;
                 });
    }
    // the formatted chat text
    for (int ns = 0; ns <= 2; ns++)
        for (int nl = 0; nl <= 3; nl++)
            for (int lb = 0; lb < 2; lb++)
                Case("ChatAddFormatted " + N(ns) + N(nl) + N(lb), [](Objects&) {},
                     [&](Objects& o, bool n) {
                         EeWonLobbyCtlFormat f = {};
                         f.output = (intptr_t)&o.mArea;
                         f.format = (intptr_t)NewData("fmt %s");
                         f.strings[0] = ns > 0 ? (intptr_t)NewData("one") : 0;
                         f.strings[1] = ns > 1 ? (intptr_t)NewData("two") : 0;
                         f.num_strings = ns;
                         for (int i = 0; i < 3; i++) f.links[i] = i < nl ? (intptr_t)&o.mLinks[i] : 0;
                         f.num_links = nl;
                         for (int i = 0; i < 9; i++) f.colors[i] = i * 3 + ns;
                         for (int i = 0; i < 3; i++) f.fonts[i] = (intptr_t)&o.mFonts[(i + nl) % 3];
                         f.line_break = lb;
                         return Call(o, n, Op::ChatAddFormatted, (intptr_t)&f, 0, 0);
                     });
    // the account
    const char* kNumbers[] = {"0", "31", "255", "256", "300", "65535", "70000", "-1", "x"};
    for (int i = 0; i < 9; i++)
        for (int check = 0; check < 2; check++)
            Case("DeliverCreateAccount " + N(i) + N(check),
                 [&](Objects& o) {
                     const char* kInputs[] = {"user", "pass", "a@b"};
                     for (int k = 0; k < 3; k++) o.mInputs[k].mText = GUIString(kInputs[k]);
                     for (int k = 3; k < 6; k++) o.mInputs[k].mText = GUIString(kNumbers[(i + k) % 9]);
                     o.mCheck.mChecked = check != 0;
                 },
                 [&](Objects& o, bool n) {
                     intptr_t f[7];
                     for (int k = 0; k < 6; k++) f[k] = (intptr_t)&o.mInputs[k];
                     f[6] = (intptr_t)&o.mCheck;
                     return Call(o, n, Op::DeliverCreateAccount, (intptr_t)f, 0, 0);
                 });
    // the HTTP documents' paths
    for (int owner = 0; owner < 3; owner++)
        for (size_t len : {(size_t)0, (size_t)9, (size_t)1023, (size_t)1024, (size_t)1025, (size_t)1090})
            Case("HttpDocPath " + N(owner) + " " + N(len),
                 [&](Objects& o) {
                     o.mDoc.mPaths[0] = std::string(len, 's');
                     o.mDoc.mPaths[1] = std::string(len + 1, 'g');
                 },
                 [&](Objects& o, bool n) { return Call(o, n, Op::HttpDocPath, (intptr_t)&o.mDoc, owner, (intptr_t)o.mBuffer); });
}

// ---- the colour button's chooser and popup (EELobbyGlueHost.cpp's ColorHost: ChooserNew and OpenPopup, deleted;
// eecolor.rs composes them over the new one-statement cases)

namespace Color {
struct Root;
struct Comp : RefCount {
    int mId = 0, mHeight = 0;
    Root* mRoot = NULL;
    int Height() { Log("Height " + N(this->mId)); return this->mHeight; }
    Root* GetRoot() { Log("GetRoot " + N(this->mId)); return this->mRoot; }
};
struct WONRectangle { int x = -1, y = -1, width = -1, height = -1; };
struct PopupParams {
    Comp* mPopup;
    Comp* mInputComponent = NULL;
    Comp* mOwnerComponent;
    int mFlags, mx, my;
    WONRectangle mAvoidRect;
    PopupParams(Comp* inPopup, int inFlags, Comp* inOwner, int inX = -1, int inY = -1)
        : mPopup(inPopup), mOwnerComponent(inOwner), mFlags(inFlags), mx(inX), my(inY)
    {
        Log("PopupParams " + N(inPopup ? inPopup->mId : -1) + " " + N(inFlags) + " " + N(inX) + " " + N(inY));
    }
    ~PopupParams() { Log("~PopupParams"); }
};
struct Root {
    void OpenPopup(const PopupParams& p)
    {
        Log("OpenPopup " + N(p.mPopup->mId) + " " + N(p.mFlags) + " " + N(p.mOwnerComponent->mId) + " " + N(p.mx) + " " + N(p.my) + " input " +
            N(p.mInputComponent ? p.mInputComponent->mId : -1) + " avoid " + N(p.mAvoidRect.x) + " " + N(p.mAvoidRect.y) + " " +
            N(p.mAvoidRect.width) + " " + N(p.mAvoidRect.height));
    }
};
struct EELobbyColorChooser : Comp {
    EELobbyColorChooser() { this->mId = 77; Log("new chooser"); }
};
static std::vector<EELobbyColorChooser*> sChoosers;

// the deleted cases, as they were (aComp the button `a`)
static intptr_t Orig(EeWonEeColorOp inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    typedef EeWonEeColorOp COp;
    Comp* aComp = reinterpret_cast<Comp*>(a);
    switch (inOp) {
    case COp::ChooserNew: {
        EELobbyColorChooser* aNew = new EELobbyColorChooser;
        sChoosers.push_back(aNew);
        aNew->AddRef();
        return (intptr_t)aNew;
    }
    case COp::OpenPopup: {
        PopupParams theParams(reinterpret_cast<Comp*>(b), 0x23e7, aComp, (int)c, (int)d);
        theParams.mAvoidRect.height = aComp->Height();
        theParams.mAvoidRect.y = (int)d - aComp->Height();
        theParams.mInputComponent = aComp;
        aComp->GetRoot()->OpenPopup(theParams);
        return 0;
    }
    default: return 0;
    }
}

// the new cases (ColorHost's, as they are; `new` of the chooser kept for the count), the skipped op answering 0
static intptr_t World(void*, EeWonEeColorOp inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d, intptr_t, intptr_t)
{
    typedef EeWonEeColorOp Op;
    Comp* aComp = reinterpret_cast<Comp*>(a);
    if ((int)inOp + 200000 == sSkip && inOp == Op::ChooserAlloc) { static Comp sOther; return (intptr_t)&sOther; }
    if ((int)inOp + 200000 == sSkip && inOp == Op::PopupNew) return (intptr_t) new PopupParams(reinterpret_cast<Comp*>(b), 0, aComp, 0, 0);
    if ((int)inOp + 200000 == sSkip) return 0;
    switch (inOp) {
    case Op::Height: return aComp->Height();
    case Op::ChooserAlloc: sChoosers.push_back(new EELobbyColorChooser); return (intptr_t)sChoosers.back();
    case Op::ChooserAddRef: reinterpret_cast<EELobbyColorChooser*>(a)->AddRef(); return 0;
    case Op::PopupNew: return (intptr_t) new PopupParams(reinterpret_cast<Comp*>(b), 0x23e7, aComp, (int)c, (int)d);
    case Op::PopupAvoidHeight: reinterpret_cast<PopupParams*>(b)->mAvoidRect.height = (int)c; return 0;
    case Op::PopupAvoidY: reinterpret_cast<PopupParams*>(b)->mAvoidRect.y = (int)c; return 0;
    case Op::PopupInput: reinterpret_cast<PopupParams*>(b)->mInputComponent = aComp; return 0;
    case Op::PopupOpen: aComp->GetRoot()->OpenPopup(*reinterpret_cast<PopupParams*>(b)); return 0;
    case Op::PopupFree: delete reinterpret_cast<PopupParams*>(b); return 0;
    default: Log("unexpected color op " + N((int)inOp)); return 0;
    }
}
} // namespace Color

static void RunColor()
{
    for (int k = 0; k < 2; k++) {
        Trace t[2];
        for (int side = 0; side < 2; side++) {
            sTrace.clear();
            Color::sChoosers.clear();
            intptr_t r = side ? ee_won_eecolor_split_op(NULL, Color::World, EeWonEeColorOp::ChooserNew, 0, 0, 0, 0)
                              : Color::Orig(EeWonEeColorOp::ChooserNew, 0, 0, 0, 0);
            Log("chooser " + N(r == (intptr_t)(Color::sChoosers.empty() ? NULL : Color::sChoosers[0])) + " made " +
                N((long long)Color::sChoosers.size()) + " refs " + N(Color::sChoosers.empty() ? -1 : Color::sChoosers[0]->mRefs));
            for (Color::EELobbyColorChooser* c : Color::sChoosers) delete c;
            t[side] = sTrace;
        }
        Compare("ChooserNew", t[0], t[1]);
    }
    for (int x : {-5, 0, 40, 1000})
        for (int y : {-3, 0, 20, 700})
            for (int h : {0, 18, 31, -4}) {
                Trace t[2];
                for (int side = 0; side < 2; side++) {
                    sTrace.clear();
                    Color::Root aRoot;
                    Color::Comp aButton, aChooser;
                    aButton.mId = 5;
                    aButton.mHeight = h;
                    aButton.mRoot = &aRoot;
                    aChooser.mId = 77;
                    intptr_t r = side ? ee_won_eecolor_split_op(NULL, Color::World, EeWonEeColorOp::OpenPopup, (intptr_t)&aButton, (intptr_t)&aChooser, x, y)
                                      : Color::Orig(EeWonEeColorOp::OpenPopup, (intptr_t)&aButton, (intptr_t)&aChooser, x, y);
                    Log("result " + N(r));
                    t[side] = sTrace;
                }
                Compare("OpenPopup " + N(x) + " " + N(y) + " " + N(h), t[0], t[1]);
            }
}

// ---- the EE glue's options and players fields (EELobbyGlueHost.cpp's OptionsField, OptionSet and PlayerField,
// deleted; gluesplit.rs maps each field to its one-statement case: the LAN game reads and writes most of them, never
// OptionsPlayer)

namespace Glue {
struct EEGameOptionsPlayer {
    unsigned long mType = 3;
    long mLoadedType = 0;
    unsigned long mSlot = 0, mColor = 0, mTeam = 0, mStartingCitizens = 0;
    bool mIsLocal = false, mIsReady = false, mB36 = true;
    std::string mName;
};
struct EEGameOptions {
    unsigned long mGameSpeed = 0, mLoadType = 0, mMapSize = 0;
    long mEpochStart = 0, mEpochEnd = 0;
    unsigned long mVariant = 0, mResources = 0, mUnitLimit = 0, mWondersForVictory = 0, mDifficulty = 0;
    bool mVictory = false, mLockTeams = false, mLockSpeed = false, mRevealMap = false, mAllowCustomCivs = false;
    std::string mMapType;
    EEGameOptionsPlayer mPlayers[16];
};
static EEGameOptions gGameOptions;
static EEGameOptionsPlayer sEELobbyPlayer;
static unsigned long sEELobbyMapIndex;
// the map types: "map<k>" is the generator k; another name is not found (the index left as it was)
static bool EERMFindGenerator(const std::string& inName, unsigned long& outIndex)
{
    Log("EERMFindGenerator " + inName);
    if (inName.compare(0, 3, "map") != 0) return false;
    outIndex = (unsigned long)atoi(inName.c_str() + 3);
    return true;
}
static std::string State()
{
    std::string s = "options";
    const EEGameOptions& o = gGameOptions;
    for (long v : {(long)o.mGameSpeed, (long)o.mLoadType, (long)o.mMapSize, o.mEpochStart, o.mEpochEnd, (long)o.mVariant, (long)o.mResources,
                   (long)o.mUnitLimit, (long)o.mWondersForVictory, (long)o.mDifficulty, (long)o.mVictory, (long)o.mLockTeams, (long)o.mLockSpeed,
                   (long)o.mRevealMap, (long)o.mAllowCustomCivs})
        s += " " + N(v);
    const EEGameOptionsPlayer& p = sEELobbyPlayer;
    s += " player";
    for (long v : {(long)p.mType, p.mLoadedType, (long)p.mSlot, (long)p.mColor, (long)p.mTeam, (long)p.mStartingCitizens, (long)p.mIsLocal,
                   (long)p.mIsReady, (long)p.mB36})
        s += " " + N(v);
    return s;
}

// the deleted helpers and the four cases, as they were
namespace Orig {
static long OptionsField(const EEGameOptions& o, EeWonEeGlueField inField)
{
    typedef EeWonEeGlueField F;
    unsigned long aIndex = 0;
    switch (inField) {
    case F::MapTypeIndex: EERMFindGenerator(o.mMapType, aIndex); return (long)aIndex;
    case F::MapSize: return o.mMapSize;
    case F::Resources: return o.mResources;
    case F::EpochStart: return o.mEpochStart;
    case F::EpochEnd: return o.mEpochEnd;
    case F::Variant: return o.mVariant;
    case F::Difficulty: return o.mDifficulty;
    case F::GameSpeed: return o.mGameSpeed;
    case F::UnitLimit: return o.mUnitLimit;
    case F::WondersForVictory: return o.mWondersForVictory;
    case F::RevealMap: return o.mRevealMap;
    case F::AllowCustomCivs: return o.mAllowCustomCivs;
    case F::LockTeams: return o.mLockTeams;
    case F::LockSpeed: return o.mLockSpeed;
    case F::Victory: return o.mVictory;
    default: return 0;
    }
}
static void OptionSet(EeWonEeGlueField inField, long v)
{
    typedef EeWonEeGlueField F;
    EEGameOptions& o = gGameOptions;
    switch (inField) {
    case F::LoadType: o.mLoadType = v; return;
    case F::MapSize: o.mMapSize = v; return;
    case F::Resources: o.mResources = v; return;
    case F::EpochStart: o.mEpochStart = v; return;
    case F::EpochEnd: o.mEpochEnd = v; return;
    case F::Variant: o.mVariant = v; return;
    case F::Difficulty: o.mDifficulty = v; return;
    case F::GameSpeed: o.mGameSpeed = v; return;
    case F::UnitLimit: o.mUnitLimit = v; return;
    case F::WondersForVictory: o.mWondersForVictory = v; return;
    case F::RevealMap: o.mRevealMap = v != 0; return;
    case F::AllowCustomCivs: o.mAllowCustomCivs = v != 0; return;
    case F::LockTeams: o.mLockTeams = v != 0; return;
    case F::LockSpeed: o.mLockSpeed = v != 0; return;
    case F::Victory: o.mVictory = v != 0; return;
    default: return;
    }
}
static long PlayerField(EEGameOptionsPlayer& p, EeWonEeGlueField inField, bool inSet, long v)
{
    typedef EeWonEeGlueField F;
    switch (inField) {
    case F::Type: if (inSet) p.mType = v; return (long)p.mType;
    case F::LoadedType: if (inSet) p.mLoadedType = v; return p.mLoadedType;
    case F::Slot: if (inSet) p.mSlot = v; return (long)p.mSlot;
    case F::Color: if (inSet) p.mColor = v; return (long)p.mColor;
    case F::Team: if (inSet) p.mTeam = v; return (long)p.mTeam;
    case F::StartingCitizens: if (inSet) p.mStartingCitizens = v; return (long)p.mStartingCitizens;
    case F::IsLocal: if (inSet) p.mIsLocal = v != 0; return p.mIsLocal;
    case F::IsReady: if (inSet) p.mIsReady = v != 0; return p.mIsReady;
    case F::B36: if (inSet) p.mB36 = v != 0; return p.mB36;
    case F::NameEmpty: return p.mName == "" ? 1 : 0;
    default: return 0;
    }
}
static intptr_t Host(EeWonEeGlueOp inOp, intptr_t a, intptr_t b, intptr_t c)
{
    typedef EeWonEeGlueOp Op;
    switch (inOp) {
    case Op::OptionsField: return OptionsField(*reinterpret_cast<const EEGameOptions*>(a), (EeWonEeGlueField)b);
    case Op::OptionsPlayer: return PlayerField(reinterpret_cast<EEGameOptions*>(a)->mPlayers[b], (EeWonEeGlueField)c, false, 0);
    case Op::OptionSet: OptionSet((EeWonEeGlueField)a, (long)b); return 0;
    case Op::PlayerSet: PlayerField(sEELobbyPlayer, (EeWonEeGlueField)a, true, (long)b); return 0;
    default: return 0;
    }
}
} // namespace Orig

// the new cases (GlueHost's, as they are), the skipped op answering 0
static intptr_t World(EeWonEeGlueOp inOp, intptr_t a, intptr_t b, intptr_t, intptr_t, intptr_t, intptr_t)
{
    typedef EeWonEeGlueOp Op;
    EEGameOptions* anOptions = reinterpret_cast<EEGameOptions*>(a);
    EEGameOptionsPlayer* aPlayer = reinterpret_cast<EEGameOptionsPlayer*>(a);
    if ((int)inOp + 300000 == sSkip && inOp == Op::OptionsPlayerAt) return (intptr_t)&anOptions->mPlayers[0];
    if ((int)inOp + 300000 == sSkip) return 0;
    switch (inOp) {
    case Op::OptGetMapSize: return (long)anOptions->mMapSize;
    case Op::OptGetResources: return (long)anOptions->mResources;
    case Op::OptGetEpochStart: return (long)anOptions->mEpochStart;
    case Op::OptGetEpochEnd: return (long)anOptions->mEpochEnd;
    case Op::OptGetVariant: return (long)anOptions->mVariant;
    case Op::OptGetDifficulty: return (long)anOptions->mDifficulty;
    case Op::OptGetGameSpeed: return (long)anOptions->mGameSpeed;
    case Op::OptGetUnitLimit: return (long)anOptions->mUnitLimit;
    case Op::OptGetWondersForVictory: return (long)anOptions->mWondersForVictory;
    case Op::OptGetRevealMap: return (long)anOptions->mRevealMap;
    case Op::OptGetAllowCustomCivs: return (long)anOptions->mAllowCustomCivs;
    case Op::OptGetLockTeams: return (long)anOptions->mLockTeams;
    case Op::OptGetLockSpeed: return (long)anOptions->mLockSpeed;
    case Op::OptGetVictory: return (long)anOptions->mVictory;
    case Op::MapIndexClear: sEELobbyMapIndex = 0; return 0;
    case Op::OptGetMapTypeIndex: EERMFindGenerator(anOptions->mMapType, sEELobbyMapIndex); return (long)sEELobbyMapIndex;
    case Op::OptSetLoadType: gGameOptions.mLoadType = (long)a; return 0;
    case Op::OptSetMapSize: gGameOptions.mMapSize = (long)a; return 0;
    case Op::OptSetResources: gGameOptions.mResources = (long)a; return 0;
    case Op::OptSetEpochStart: gGameOptions.mEpochStart = (long)a; return 0;
    case Op::OptSetEpochEnd: gGameOptions.mEpochEnd = (long)a; return 0;
    case Op::OptSetVariant: gGameOptions.mVariant = (long)a; return 0;
    case Op::OptSetDifficulty: gGameOptions.mDifficulty = (long)a; return 0;
    case Op::OptSetGameSpeed: gGameOptions.mGameSpeed = (long)a; return 0;
    case Op::OptSetUnitLimit: gGameOptions.mUnitLimit = (long)a; return 0;
    case Op::OptSetWondersForVictory: gGameOptions.mWondersForVictory = (long)a; return 0;
    case Op::OptSetRevealMap: gGameOptions.mRevealMap = a != 0; return 0;
    case Op::OptSetAllowCustomCivs: gGameOptions.mAllowCustomCivs = a != 0; return 0;
    case Op::OptSetLockTeams: gGameOptions.mLockTeams = a != 0; return 0;
    case Op::OptSetLockSpeed: gGameOptions.mLockSpeed = a != 0; return 0;
    case Op::OptSetVictory: gGameOptions.mVictory = a != 0; return 0;
    case Op::OptionsPlayerAt: return (intptr_t)&anOptions->mPlayers[b];
    case Op::PlayerGetType: return (long)aPlayer->mType;
    case Op::PlayerGetLoadedType: return (long)aPlayer->mLoadedType;
    case Op::PlayerGetSlot: return (long)aPlayer->mSlot;
    case Op::PlayerGetColor: return (long)aPlayer->mColor;
    case Op::PlayerGetTeam: return (long)aPlayer->mTeam;
    case Op::PlayerGetStartingCitizens: return (long)aPlayer->mStartingCitizens;
    case Op::PlayerGetIsLocal: return (long)aPlayer->mIsLocal;
    case Op::PlayerGetIsReady: return (long)aPlayer->mIsReady;
    case Op::PlayerGetB36: return (long)aPlayer->mB36;
    case Op::PlayerGetNameEmpty: return aPlayer->mName == "" ? 1 : 0;
    case Op::PlayerSetType: sEELobbyPlayer.mType = (long)a; return 0;
    case Op::PlayerSetLoadedType: sEELobbyPlayer.mLoadedType = (long)a; return 0;
    case Op::PlayerSetSlot: sEELobbyPlayer.mSlot = (long)a; return 0;
    case Op::PlayerSetColor: sEELobbyPlayer.mColor = (long)a; return 0;
    case Op::PlayerSetTeam: sEELobbyPlayer.mTeam = (long)a; return 0;
    case Op::PlayerSetStartingCitizens: sEELobbyPlayer.mStartingCitizens = (long)a; return 0;
    case Op::PlayerSetIsLocal: sEELobbyPlayer.mIsLocal = a != 0; return 0;
    case Op::PlayerSetIsReady: sEELobbyPlayer.mIsReady = a != 0; return 0;
    case Op::PlayerSetB36: sEELobbyPlayer.mB36 = a != 0; return 0;
    default: Log("unexpected glue op " + N((int)inOp)); return 0;
    }
}
static intptr_t WorldFn(EeWonEeGlueOp inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d, intptr_t e, intptr_t f)
{
    return World(inOp, a, b, c, d, e, f);
}

// fresh globals: the options' and the player's members numbered from `inBase`
static void Make(int inBase, const char* inMapType)
{
    EEGameOptions& o = gGameOptions;
    o = EEGameOptions();
    o.mGameSpeed = inBase + 1; o.mLoadType = inBase + 2; o.mMapSize = inBase + 3; o.mEpochStart = -inBase - 4; o.mEpochEnd = inBase + 5;
    o.mVariant = inBase + 6; o.mResources = inBase + 7; o.mUnitLimit = 0x80000000u + inBase; o.mWondersForVictory = inBase + 9;
    o.mDifficulty = inBase + 10; o.mVictory = inBase & 1; o.mLockTeams = inBase & 2; o.mLockSpeed = inBase >= 2; o.mRevealMap = !(inBase & 1);
    o.mAllowCustomCivs = !(inBase & 2);
    o.mMapType = inMapType;
    for (int i = 0; i < 16; i++) {
        EEGameOptionsPlayer& p = o.mPlayers[i];
        p.mType = i + inBase; p.mLoadedType = -i; p.mSlot = 100 + i; p.mColor = 0xffffffffu - i; p.mTeam = i % 4;
        p.mStartingCitizens = i * 3; p.mIsLocal = i & 1; p.mIsReady = i & 2; p.mB36 = !(i & 4);
        p.mName = i % 3 ? "name" : "";
    }
    sEELobbyPlayer = EEGameOptionsPlayer();
    sEELobbyMapIndex = 12345;
}
} // namespace Glue

static void RunGlue()
{
    typedef EeWonEeGlueOp GOp;
    const intptr_t kValues[] = {0, 1, -1, 7, (intptr_t)0x80000000LL, (intptr_t)0x100000005LL};
    for (int base = 0; base < 4; base++)
        for (const char* map : {"map5", "map0", "other", ""})
            for (GOp op : {GOp::OptionsField, GOp::OptionsPlayer, GOp::OptionSet, GOp::PlayerSet})
                for (int field = 0; field <= 27; field++)
                    for (intptr_t v : kValues) {
                        if ((op == GOp::OptionsField || op == GOp::OptionsPlayer) && v != 0 && v != 7) continue;
                        if (op != GOp::OptionsField && map != std::string("map5")) continue;
                        intptr_t a = 0, b = 0, c = 0;
                        if (op == GOp::OptionsField) { a = (intptr_t)&Glue::gGameOptions; b = field; }
                        if (op == GOp::OptionsPlayer) { a = (intptr_t)&Glue::gGameOptions; b = v + base; c = field; }
                        if (op == GOp::OptionSet || op == GOp::PlayerSet) { a = field; b = v; }
                        Trace t[2];
                        for (int side = 0; side < 2; side++) {
                            Glue::Make(base, map);
                            sTrace.clear();
                            intptr_t r = side ? ee_won_eeglue_split_op(Glue::WorldFn, op, a, b, c, 0, 0, 0) : Glue::Orig::Host(op, a, b, c);
                            Log("result " + N(r) + " " + Glue::State());
                            t[side] = sTrace;
                        }
                        Compare("glue " + N((int)op) + " " + N(field) + " " + N(v) + " " + map, t[0], t[1]);
                    }
}

// ---- the game list's EE columns (EeColumn: the glue's column call on a row; the keeps hold the filter text in a
// local, now the host's static sEELobbyColumnText through ColumnTextSet / ColumnText). The glue's calls run in
// eeglue.rs over a stand-in glue host that logs its ops and the filter text it is handed (with its references).
static intptr_t ColumnGlue(EeWonEeGlueOp inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d, intptr_t e, intptr_t f)
{
    Log("glue " + N((int)inOp) + " " + N(a) + " " + N(b) + " " + N(c) + " " + (inOp == EeWonEeGlueOp::TextFind ? "" : N(d)) + " " + N(e) + " " + N(f));
    switch (inOp) {
    case EeWonEeGlueOp::ItemGame: return a;
    case EeWonEeGlueOp::NumMapTypes: return 3;
    case EeWonEeGlueOp::TextFind: {
        if (d == 0) { Log("filter null"); return -1; }
        const GUIString* aText = reinterpret_cast<const GUIString*>(d);
        Log("filter '" + aText->mData->mText + "'/" + N(aText->mData->mRefs));
        return aText->mData->mText.find('e') != std::string::npos ? 0 : -1;
    }
    default: return 0;
    }
}
static void RunColumn()
{
    typedef EeWonEeGlueCall GC;
    ee_won_eeglue_set_host(ColumnGlue);
    const char* kFilters[] = {"", "e", "epoch", "xyz"};
    for (GC call : {GC::EpochCompare, GC::MapTypeCompare, GC::EpochKeep, GC::MapTypeKeep, GC::EpochPaint, GC::MapTypePaint})
        for (const char* t : kFilters)
            for (intptr_t row : {(intptr_t)1, (intptr_t)2})
                Case("EeColumn " + N((int)call) + " " + t + " " + N(row), [](Objects&) {},
                     [&](Objects& o, bool n) {
                         bool aKeep = call == GC::EpochKeep || call == GC::MapTypeKeep;
                         // a keep's filter text (the caller's GUIStringData, held once); the others' third argument
                         GUIStringData* aData = NewData(t);
                         aData->AddRef();
                         intptr_t c = aKeep ? (intptr_t)aData : row + 1;
                         intptr_t r = Call(o, n, Op::EeColumn, (intptr_t)call, row, c);
                         Log("column text '" + (std::string)sEELobbyColumnText + "'/" + N(sEELobbyColumnText.mData->mRefs));
                         return r;
                     });
    ee_won_eeglue_set_host(NULL);
}

int main()
{
    Run();
    RunColor();
    RunGlue();
    RunColumn();
    int aReal = sFailures;
    printf("won_ctl35_test: %d checks, %d mismatches\n", sChecks, aReal);
    // the skips: every new op of the control host, then of the config host
    std::vector<int> aSkips;
    for (int op = (int)Op::ItemGetHeight; op <= (int)Op::LinkSetPrevious; op++)
        // (TextStartEmpty's empty text is the one TextDrop left: in this world, as in the game,
        // GetEmptyGUIStringData() is the empty text, so skipping it reads the same)
        if (op != (int)Op::TextStartEmpty) aSkips.push_back(op);
    for (int op = (int)EeWonEeColorOp::ChooserAlloc; op <= (int)EeWonEeColorOp::PopupFree; op++)
        aSkips.push_back(op + 200000);
    for (int op = (int)EeWonEeGlueOp::OptGetMapSize; op <= (int)EeWonEeGlueOp::PlayerSetB36; op++)
        aSkips.push_back(op + 300000);
    int aCaught = 0;
    for (int s : aSkips) {
        sSkip = s;
        sFailures = 0;
        // the host statics a skip may leave set (a skipped EvDrop keeps the event) start each run empty: a leak would
        // make the next run's cases differ and pass its skip off as caught
        sEELobbyEvent = NULL;
        sEELobbyText = GUIString();
        std::string().swap(sEELobbyBytes);
        sEELobbyColumnText = GUIString();
        Run();
        RunColor();
        RunGlue();
        RunColumn();
        if (sFailures > 0) aCaught++;
        else printf("skip of op %d not caught\n", s);
    }
    printf("won_ctl35_test: %d of %zu skips caught\n", aCaught, aSkips.size());
    return aReal + (int)(aSkips.size() - aCaught);
}
