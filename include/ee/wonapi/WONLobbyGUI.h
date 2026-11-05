// WONAPI GUI / lobby framework classes as the game's lobby glue sees them (RECONSTRUCTION; r12-misc-h2, round 12).
// WONAPI proper (0x29deb0..) is not decompiled: bases come from the RTTI class hierarchy descriptors, sizes from the
// glue's `new` calls; every other name PROVISIONAL (by behaviour); `_gap` arrays are undecoded. Requires <string>,
// ee/EEWonNet.h, ee/wonapi/WONLobbyResource.h.
#ifndef EE_WONAPI_WONLOBBYGUI_H
#define EE_WONAPI_WONLOBBYGUI_H

namespace WONAPI {

class Component;
class Container;
class ComponentConfig;
class ComponentEvent;
class ListArea;
class LobbyClient;
class LobbyPlayer;
class LobbyClientSet;
class ReadBuffer;
class WriteBuffer;

// what a component is initialised from (slot +0x1c); type 2: from its resource configuration
struct ComponentInit {
    int mType;                              // +0x00
};

struct ComponentConfigInit : public ComponentInit {
    ComponentConfig* mConfig;               // +0x04
};

class Graphics;
class RootContainer;
class ComponentListener;
struct PopupParams;
extern GUIString gEmptyGUIString;           // 0x966244

// a GUI component (0x30 bytes: EELobbyColorChooser's members start at +0x30 (r12-misc-h7); vtable 0x84d71c, 43
// slots). Unknown slots are named by vtable offset. The inline bodies are the COMDATs the first lobby unit
// (EELobbyColorButton.cpp 0x29059d, 0x2905a2) keeps; the other tiny slots are ICF-folded into game code.
class Component : public RefCount {
public:
    Component();                            // 0x2f49e0 (WONAPI)
    virtual ~Component();                   // 0x2f4a70 (WONAPI)
    virtual void Virtual04();
    virtual void Virtual08();
    virtual void Virtual0C(int& outWidth, int& outHeight);     // (EELobbyColorChooser 0x2908af: 120 x 120)
    virtual void Virtual10();
    virtual void Virtual14();
    virtual void Paint(Graphics& inGraphics);                  // +0x18 (0x2f5020)
    virtual void InitComponent(ComponentInit& inInit);         // +0x1c (0x156dd8: empty)
    virtual void Virtual20();
    virtual void Virtual24(int inA, int inB);                  // +0x24 (0x2f4cc0)
    virtual void Virtual28();                                  // +0x28 (0x2f4ce0: the mouse left?)
    virtual void Virtual2C(int inA, int inB, int inC);         // +0x2c (a button released?)
    virtual void Virtual30(int inA, int inB, int inC) {}       // +0x30 0x2907a3 (emitted with EELobbyColorChooser)
    virtual void Virtual34(int inX, int inY);                  // +0x34 (the mouse moved)
    virtual void Virtual38();
    virtual bool Virtual3C(int inParam) { return false; }      // +0x3c 0x29059d
    virtual void Virtual40();
    virtual void Virtual44();
    virtual void Virtual48();
    virtual void Virtual4C();
    virtual bool Virtual50(int inParam) { return false; }      // +0x50 (ICF: 0x29059d)
    virtual bool Virtual54(int inParam) { return false; }      // +0x54 (ICF: 0x29059d)
    virtual bool Virtual58(int inParam) { return false; }      // +0x58 (ICF: 0x29059d)
    virtual void Virtual5C();
    virtual void Virtual60();
    virtual void Invalidate();                                 // +0x64 (after a visible change; PROVISIONAL)
    virtual void Virtual68();
    virtual void Virtual6C();                                  // +0x6c (0x2f5160: when the popup opens?)
    virtual void Virtual70();
    virtual void Virtual74(bool inFlag);
    virtual void Virtual78();
    virtual void HandleComponentEvent(ComponentEvent* inEvent);    // +0x7c (0x2f5550)
    virtual void Virtual80();
    virtual void Virtual84();
    virtual void Virtual88();
    virtual void Virtual8C();
    virtual void SetEnabled(bool inEnabled);                   // +0x90 (0x2f5610: clears/sets flag 0x80 of +0x20)
    virtual void SetVisible(bool inVisible);                   // +0x94 (0x2f55b0)
    virtual void Virtual98();
    virtual void Virtual9C();
    virtual void VirtualA0();
    virtual int VirtualA4() { return 0xffff; }                 // +0xa4 0x2905a2
    virtual int VirtualA8() { return 0xffff; }                 // +0xa8 (ICF: 0x2905a2)

    void AddListener(ComponentListener* inListener);           // 0x2f4b10
    void Fn2F4E90(int inA, int inB);        // 0x2f4e90 (on a new popup: 0, 0)
    void Fn2F5380(int inType, int inParam); // 0x2f5380 (fires a component event: EELobbyColorButton's 100064)
    void Fn2F56D0(int& ioX, int& ioY);      // 0x2f56d0 (to the root's coordinates?)
    void Fn2F5890();                        // 0x2f5890
    RootContainer* GetRoot();               // 0x2f5920
    RootContainer* Fn2F5940();              // 0x2f5940 (the root a dialog runs in?)
    bool IsDisabled() const { return mDisabled; }      // (EELobbyColorButton 0x29060b: shr 7; test al, 1)

    Container* mParent;                     // +0x08 (SetEnabled notifies it through its slot +0xbc)
    char _gap0c[0xc];
    int mWidth;                             // +0x18
    int mHeight;                            // +0x1c
    unsigned long mComponentBits : 7;       // +0x20 bits
    unsigned long mDisabled : 1;            // +0x20 bit 7 (SetEnabled: flag 0x80)
    int mControlId;                         // +0x24 (the component events' id)
    char _gap28[8];
};

// a component holding others (0x84 bytes: EELobbyGameOptionsCtrl's members start at +0x84; vtable 0x84d7cc, 51 slots)
class Container : public Component {
public:
    Container();                            // 0x2f5b70 (WONAPI, /GX)
    virtual ~Container();                   // 0x2f5e80 (WONAPI, /GX)
    void Fn2F5360(int inFlag);              // 0x2f5360
    // 0x2f6140 (ret 0x20): adds inChild laid out against inRef (EELobbyInterface 0x2972c6: the lobby screen, 3,
    // the root, 0, 0, 0, 0, 0; r24pre-won PROVISIONAL)
    void Fn2F6140(Component* inChild, int inFlags, Component* inRef, int inA, int inB, int inC, int inD, int inE);
    void Fn2F6A60(Component* inChild);      // 0x2f6a60 (then this; r24pre-won PROVISIONAL)
    virtual void VirtualAC(Component* inChild, int inParam);
    virtual void VirtualB0(Component* inChild, int inParam) { VirtualAC(inChild, inParam); }   // +0xb0 0x2909c7
    virtual void VirtualB4();
    virtual void VirtualB8();
    virtual void VirtualBC(Component* inChild);
    virtual void VirtualC0();
    virtual void VirtualC4();
    virtual void VirtualC8();

    char _gap30[0x54];
};

// a modal or popup dialog (0x98 bytes: EELobbyConnectDialog's members start at +0x98; its implicit dtor is the
// lobby glue's COMDAT 0x290a26; r12-misc-h7)
class Dialog : public Container {
public:
    Dialog();                               // 0x2f7910 (WONAPI)
    virtual void HandleComponentEvent(ComponentEvent* inEvent);    // +0x7c 0x2f7d20 (WONAPI)

    char _gap84[0x10];
    SmartPtr<Component> mC94;               // +0x94
};

class Button : public Component {
public:
    Button();                               // 0x2f3070 (WONAPI)
    virtual ~Button();                      // 0x2f3100 (WONAPI)
    virtual bool VirtualAC();               // +0xac (a click; EELobbyColorButton 0x290669)
    virtual void VirtualB0(int inParam);
    virtual void SetText(const GUIString& inText, bool inFlag);    // +0xb4 (0x2f3180)
    virtual const GUIString& GetText() { return gEmptyGUIString; }   // +0xb8 0x2905a8
    void SetCheck(bool inChecked, bool inNotify = false);      // 0x2f3500
    void Fn2F3370(unsigned long inFlags, bool inSet);          // 0x2f3370 (EELobbyColorButton: 0x40, true)

    unsigned long mButtonFlag0 : 1;         // +0x30 bits (Button::SetCheck 0x2f3500 tests bit 3)
    unsigned long mButtonFlag1 : 1;
    unsigned long mChecked : 1;             // +0x30 bit 2 (a check box's state)
    unsigned long mButtonFlag3 : 1;
    unsigned long mButtonFlag4 : 1;
    unsigned long mButtonFlag5 : 1;         // +0x30 bit 5 (EELobbyColorButton 0x29060b draws it pressed)
    char _gap34[0x10];
};

class Label : public Component {
public:
    void SetText(const GUIString& inText, bool inFlag = true);    // 0x2fa060
};

// a combo box's text field (class PROVISIONAL)
class ComboEdit : public Component {
public:
    char _gap30[0x70];
    unsigned long mTextColor;               // +0xa0 (EELobbyPlayerOptionsCtrl 0x299dd2: 0x8000000e, 0xff0000)
};

// a drop-down list (vtable 0x84d638; its list at +0x90)
class ComboBox : public Container {
public:
    void AddString(const GUIString& inText, int inPos = -1);   // 0x2f3b70
    void Clear(bool inFlag);                // 0x2f3c60 (r12-misc-h5: before the map types are added; PROVISIONAL)
    int GetSelItem();                       // 0x2f3d40 (-1 when nothing is selected)
    void SetSelItem(int inItem, bool inNotify = false);        // 0x2f3d60
    void SetText(const GUIString& inText, bool inNotify = false);  // 0x2f42b0 (through +0x84)

    ComboEdit* mEdit;                       // +0x84
    char _gap88[8];
    ListArea* mListArea;                    // +0x90
};

// one local network address (a std::list element, 0x14 bytes; struct and member names PROVISIONAL)
struct LobbyNetAddress {
    std::string mName;                      // +0x00
    void* mHandle;                          // +0x10
};
struct ConfigObjectEntry {                  // PROVISIONAL (0xc bytes)
    long mL00;
    long mL04;
    SmartPtr<RefCount> mObject;             // +0x08 (released by the node dtor 0x296a8f)
};
class ConfigValueBase : public RefCount {   // RTTI: ConfigValueBase : RefCount; ConfigValue<T>, SimpleConfigValue<T> derive
};
typedef SmartPtr<ConfigValueBase> ConfigValueBasePtr;

// a configuration object (0x54 bytes: LobbyConfig's members start at +0x54). Layout from its implicit dtor 0x294e83
// (r24-prep-ui); member names PROVISIONAL.
class ConfigObject : public RefCount {
public:
    void KeyAdd(const char* inKey, unsigned short& ioValue, unsigned short inDefault);   // 0x3b1ba0

    std::list<SmartPtr<RefCount> > mL08;    // +0x08 (dtor ICF 0xd17ea, list<SmartPtr<..>>)
    long mL14;                              // +0x14
    std::string mS18;                       // +0x18
    std::string mS28;                       // +0x28
    long mL38;                              // +0x38
    std::map<std::string, ConfigValueBasePtr> mKeyMap;     // +0x3c (KeyAdd 0x3b18e0 inserts at +0x3c; node value 0x14)
    std::map<std::string, ConfigObjectEntry> mM48;         // +0x48 (node value 0x1c: SmartPtr at value +0x18)
};

class ComponentConfig : public ConfigObject {
};

// 0x38aa60 (cdecl): the component the resource names inName, 0x38ab30: reports a missing one
Component* ComponentConfigGet(ComponentConfig* inConfig, const char* inName);
void ComponentConfigMissing(const std::string& inName, const char* inFile, int inLine);

// fetches a named component of the lobby's resources into outPtr (WONAPI header template; COMDATs 0x296948,
// 0x2969c8 in EELobbyInit.cpp)
template <class T> void SafeGetComponent(ComponentConfig* inConfig, SmartPtr<T>& outPtr, const char* inName,
                                         const char* inFile, int inLine)
{
    T* aComponent = dynamic_cast<T*>(ComponentConfigGet(inConfig, inName));
    if (aComponent == NULL)
        ComponentConfigMissing(inName, inFile, inLine);
    outPtr = aComponent;
}

class ButtonConfig : public ComponentConfig {
public:
    ButtonConfig();                         // 0x381860 (0x7c bytes)

    char _gap54[0x28];
};

class ContainerConfig : public ComponentConfig {
public:
    ContainerConfig();                      // 0x387f90 (0x98 bytes)
    virtual ~ContainerConfig();             // 0x382690

    char _gap54[0x44];
};

class DialogConfig : public ContainerConfig {
public:
    char _gap98[4];
};

class LobbyConfig : public ConfigObject {
public:
    LobbyConfig();                          // 0x358090

    std::list<LobbyNetAddress> mL54;        // +0x54
    std::list<LobbyNetAddress> mL60;        // +0x60
    std::list<LobbyNetAddress> mL6C;        // +0x6c
    std::string mS78;                       // +0x78
    std::wstring mW88;                      // +0x88
    std::string mS98;                       // +0x98
    std::string mSA8;                       // +0xa8
    std::string mSB8;                       // +0xb8
    std::string mSC8;                       // +0xc8
    char _gapd8[0x14];                      // +0xd8 (no dtor)
    std::map<std::string, std::string> mMEC;               // +0xec (node value 0x20; pair dtor 0x296a48)
};

// 0x358710: the lobby's configuration object (an EELobbyConfig: EELobbyInit.cpp's factory)
LobbyConfig* GetLobbyConfig();

// one item of a list (0x18 bytes: TextListItem's members start at +0x18; vtable 0x84dba4, 14 slots; the inline
// bodies are the COMDATs EELobbyConnectDialog.cpp keeps, 0x290bb4..0x290bc4; r12-misc-h7)
class ListItem : public RefCount {
public:
    ListItem();                             // 0x2fa280 (WONAPI)
    virtual void Virtual04();
    virtual void Virtual08();
    virtual void Virtual0C();
    virtual void Virtual10();
    virtual bool Virtual14(int inA, int inB, int inC) { return mB09; }     // +0x14 0x290bb4
    virtual void Virtual18(int inA, int inTop) { mTop = inTop; }           // +0x18 0x290bba
    virtual int Virtual1C(int inA, int inY)                                 // +0x1c 0x290bc4 (inY on the item)
    {
        if (inY >= mTop && inY < mTop + mHeight)
            return 1;
        return 0;
    }
    virtual void Virtual20();
    virtual void Virtual24();
    virtual void Virtual28();
    virtual void Virtual2C();
    virtual void Virtual30();
    virtual void Virtual34();

    bool mB08;                              // +0x08
    bool mB09;                              // +0x09
    char _gap0a[2];
    int mTop;                               // +0x0c
    int mL10;                               // +0x10
    int mHeight;                            // +0x14
};

// the root of the lobby's components (runs the popups and modal dialogs; RTTI WONAPI::RootContainer; its base
// Container: r24pre-won PROVISIONAL)
class RootContainer : public Container {
public:
    void Fn32B0A0(int inResult, Component* inDialog);         // 0x32b0a0 (ends a modal dialog)
    void OpenPopup(PopupParams& inParams);  // 0x32b0c0
    void ClosePopup(Component* inPopup);    // 0x32b0e0
};

// how a popup is opened (0x2c bytes, on the stack; members PROVISIONAL)
struct PopupParams {
    PopupParams(Component* inPopup, int inFlags, Component* inOwner, int inX, int inY);  // 0x328540 (WONAPI)

    char _gap00[8];
    Component* mOwner;                      // +0x08
    char _gap0c[0x14];
    int mL20;                               // +0x20
    char _gap24[4];
    int mL28;                               // +0x28
};

// gets a component's events (vtable 0x84d...; 2 slots)
class ComponentListener : public RefCount {
public:
    virtual void Virtual04();
};

// a listener that hands the events on to a component (0x0c bytes; vtable 0x84c134: ??_G ICF-folded 0x291f6f,
// +0x04 0x337bf0)
class ComponentListenerComp : public ComponentListener {
public:
    ComponentListenerComp(Component* inComponent) : mComponent(inComponent) {}
    virtual void Virtual04();               // 0x337bf0 (WONAPI)

    Component* mComponent;                  // +0x08
};

// WONAPI's drawing context (methods PROVISIONAL)
class Font;
class Graphics {
public:
    void SetColor(unsigned long inColor);   // 0x326ce0 (0x00rrggbb)
    // 0x3270c0: inText at (inX, inY) in the current colour and font (r23: was named GUIString::Append; its callers
    // pass the Graphics: EELobbyServerDetailsItem::Paint 0x295010, the column draw callbacks 0x295978/0x295a59)
    void DrawString(const GUIString& inText, int inX, int inY);
    void SetSysColor(int inIndex);          // 0x326d40
    void DrawRect(int inX, int inY, int inWidth, int inHeight);    // 0x326ea0
    void FillRect(int inX, int inY, int inWidth, int inHeight);    // 0x326ee0

    char _gap00[0x10];
    Font* mFont;                            // +0x10 (0x295010: widths and line heights; r23)
};
// 0x2fdcd0 / 0x2fddf0 (cdecl): a 3D frame (inPressed: sunken) and a flat frame (names PROVISIONAL)
void LobbyDraw3DFrame(Graphics& inGraphics, int inX, int inY, int inWidth, int inHeight, bool inPressed);
void LobbyDrawFrame(Graphics& inGraphics, int inX, int inY, int inWidth, int inHeight, bool inFlag);

// a game of the lobby (0x60 bytes: EELobbyGame's members start at +0x60; vtable 26 slots, the game's overrides
// named by offset; r12-misc-h7)
class LobbyGame : public RefCount {
public:
    LobbyGame();                            // 0x3a6790 (WONAPI)
    virtual ~LobbyGame();                   // 0x3a68f0 (WONAPI)
    virtual void Virtual04();
    virtual void Virtual08();
    virtual void Virtual0C();
    virtual void Virtual10();
    virtual void Virtual14();
    virtual void Virtual18();
    virtual void Virtual1C();
    virtual bool Virtual20();               // (EELobbyGame 0x292719: can the game start)
    virtual long Virtual24();
    virtual bool Virtual28();
    virtual void Virtual2C();
    // r23 (EELobbyGame's overrides): +0x30 a new game hosted by inClient, +0x34 the game's version
    virtual void Virtual30(LobbyClient* inClient);
    virtual void Virtual34(WriteBuffer& outBuffer);
    // +0x38 a join request's check (0 accepted; r23)
    virtual short Virtual38(ReadBuffer& inBuffer, LobbyClient* inClient);
    // +0x3c the game's state for a joining client (the counterpart of +0x44; inClient unused; type PROVISIONAL, r23)
    virtual void Virtual3C(LobbyClient* inClient, WriteBuffer& outBuffer);
    virtual void Virtual40();
    virtual void Virtual44(ReadBuffer& inBuffer);     // +0x44 a joined game's state read (r23)
    // r23: +0x48 a client left (inPlayer: its player), +0x4c a client joined
    virtual void Virtual48(LobbyClient* inClient, LobbyPlayer* inPlayer);
    virtual void Virtual4C(LobbyClient* inClient);
    // r23: +0x50 the summary (three options), +0x54 the details (options and the slots' clients), +0x58 the summary
    // read back, +0x64 a join status' text
    virtual void Virtual50(WriteBuffer& outBuffer);
    virtual void Virtual54(WriteBuffer& outBuffer);
    virtual bool Virtual58(ReadBuffer& inBuffer);
    virtual bool Virtual5C(ReadBuffer& inBuffer);     // +0x5c the details read back (r23)
    // +0x60 a game message (inType 0x64..0x6a) from inFrom (r23)
    virtual void Virtual60(unsigned char inType, ReadBuffer& inBuffer, LobbyClient* inFrom);
    virtual GUIString Virtual64(short inStatus);
    bool Fn3A6D40(long inFlags);            // 0x3a6d40 (WONAPI)
    char _gap08[0xd];
    bool mIsHost;                           // +0x15 (EELobbyInit 0x295c4c; the local player hosts)
    char _gap16[0x12];
    long mNumOpenSlots;                     // +0x28 (EELobbyGame: the number of open slots)
    long mL2C;                              // +0x2c (2: clients are sent by id, else by name; 0x292be0, r23)
    char _gap30[0x4];
    LobbyClientSet* mClients;               // +0x34 (the room's clients by id: 0x2939be; r23 PROVISIONAL)
    IPAddr mAddress;                        // +0x38 (EEGetLocalAddress 0x296c89: the host's address; r12-misc-h5)
};

// 0x361350: the game the local player is in (NULL when none)
LobbyGame* GetMyLobbyGame();

// WONAPI's message buffers (names PROVISIONAL)
class ReadBuffer {
public:
    bool ReadBool();                        // 0x2a3d30 (r12-misc-h7)
    unsigned char ReadByte();               // 0x2a3d40
    short ReadShort();                      // 0x2a3d50 (r12-misc-h7)
    long ReadLong();                        // 0x2a3d60 (r12-misc-h7)
    void ReadWString(std::wstring& outText, int inLengthSize);    // 0x2a40f0 (r12-misc-h7)

    // r23 (SendFileOp's file window: a 0xc-byte ReadBuffer over its buffer; names PROVISIONAL)
    ReadBuffer(const void* inData, unsigned long inLength);   // 0x2a39f0
    void SetData(const void* inData, unsigned long inLength);  // 0x2a3a10 (rewinds)
    const void* ReadBytes(unsigned long inLength);      // 0x2a3a30 (throws past the end)

    // r23 (EELobbyGame 0x292c7e: `[+8] - [+0] > 0`, bytes left to read; names PROVISIONAL)
    long mL00;                              // +0x00 the read position
    const char* mData;                      // +0x04
    long mL08;                              // +0x08 the length
};

class ByteBuffer;
template <class T> class SmartPtr;
class WriteBuffer {
public:
    // r23 (EELobbyGame 0x293437/0x2934a0: a 0x18-byte local, ctor (0, true), the dtor called directly)
    WriteBuffer(unsigned char inLengthFieldSize, bool inLengthIncludesField);   // 0x2a8140 (vtable 0x84c930)
    virtual ~WriteBuffer();                 // 0x2a8190 (delete[] of +0x14)
    SmartPtr<ByteBuffer> ToByteBuffer(bool inRelease);    // 0x2a8480 (the message so far; r23)
    void AppendByte(unsigned char inByte);  // 0x2a82c0
    void AppendBool(bool inValue);          // 0x2a82d0 (r12-misc-h7)
    void AppendShort(short inValue);        // 0x2a82f0 (r12-misc-h7)
    void AppendLong(long inValue);          // 0x2a8300 (r12-misc-h7)
    void AppendWString(const std::wstring& inText, int inLengthSize);    // 0x2a8390 (r12-misc-h7)
    void SetByte(unsigned long inPos, unsigned char inByte);   // 0x2a8440 (back-patches a count; r23 PROVISIONAL)

    char _gap04[0x8];
    unsigned long mLength;                  // +0x0c (the bytes written so far; 0x292be0 saves it; r23)
    char _gap10[0x8];
};
// 0x361b60 / 0x361b80 (cdecl): a game message to the lobby (EELobbyGame 0x293437, 0x2934a0; names PROVISIONAL, r23)
void LobbyFn361B60(ByteBuffer* inMessage);
void LobbyFn361B80(ByteBuffer* inMessage);
void LobbyFn361BA0(LobbyClient* inClient, ByteBuffer* inMessage);  // 0x361ba0 (to one client; r23 PROVISIONAL)
void LobbyFn361290();                       // 0x361290 (after the game options changed; r23 PROVISIONAL)

// a client of the lobby's room (its name at +0x08; class PROVISIONAL)
class LobbyPlayer;

class LobbyClient : public RefCount {
public:
    bool Fn3A54E0(int inFlag);              // 0x3a54e0 (EELobbyGame's message handlers: from the captain? r23 PROVISIONAL)

    GUIString mName;                        // +0x08
    unsigned short mClientId;               // +0x0c (EELobbyGame 0x293a8b sends it after a client slot; r23)
    char _gap0e[0x1a];
    LobbyPlayer* mPlayer;                   // +0x28 (0x2984ca hands it to EELobbyPlayerSlot::SetPlayer; r12-misc-h5)
};

// a player of the lobby (vtable 0x852c54, 4 slots; 0x0c bytes PROVISIONAL)
class LobbyPlayer : public RefCount {
public:
    LobbyPlayer();                          // 0x3a90b0 (WONAPI)
    virtual ~LobbyPlayer();                 // 0x328590 (WONAPI)
    virtual bool ReadData(ReadBuffer& inBuffer);            // +0x04 (0x387ff0)
    virtual void WriteData(WriteBuffer& outBuffer);         // +0x08 (0x30a800)
    virtual bool IsTeammate(LobbyPlayer* inOther);          // +0x0c (0x3a90d0)

    char _gap08[2];
    bool mBA;                               // +0x0a (EELobbyPlayerOptionsCtrl 0x299dd2)
    char _gap0b;
};

// a rectangle (0x10 bytes; its out-of-line ctor zeroes it; r23 PROVISIONAL name)
class WONRectangle {
public:
    WONRectangle();                         // 0x349300
    WONRectangle(int inX, int inY, int inWidth, int inHeight);    // 0x349310 (r24pre-won)

    int x;                                  // +0x00
    int y;                                  // +0x04
    int width;                              // +0x08
    int height;                             // +0x0c
};

// The lobby's core, window manager and window (EELobbyInterface.cpp's globals 0x565898, 0x565894, 0x565890). Class
// names and bases from RTTI (r24pre-won): WONAPICoreEx : WONAPICore; MSWindowManager : WindowManager : RefCount;
// MSWindow : Window : RefCount; MSNativeIcon : NativeIcon : RefCount; MultiImageDecoder, DisplayContext. Sizes from
// EELobbyInterface 0x2972c6's `new`s; member and slot names PROVISIONAL.
class WindowEventBase;
// a window's drawing context (RTTI: DisplayContext : RefCount)
class DisplayContext : public RefCount {
};
class SoundDecoder;

// an image decoder that hands each file extension to its decoder (a map at +0x08; 0x348080 adds one)
class MultiImageDecoder {
public:
    char _data[0x14];                       // (size PROVISIONAL: nothing in the game reads it)
};
// 0x3d2a20 / 0x3d24e0 (cdecl): add the JPG decoder (4 extensions) / the PNG decoder to a multi-decoder (PROVISIONAL)
void LobbyFn3D2A20(MultiImageDecoder* ioDecoder);
void LobbyFn3D24E0(MultiImageDecoder* ioDecoder);

// the WONAPI core the lobby runs on (0xd0 bytes; vtable 0x84d568)
class WONAPICore {
public:
    virtual ~WONAPICore();                  // +0x00
    virtual void Virtual04();               // +0x04 (right after it is created)
    virtual void Virtual08();               // +0x08 (before the lobby deletes it)

    char _gap04[0xcc];
};

class WONAPICoreEx : public WONAPICore {
public:
    WONAPICoreEx();                         // 0x2f2f40
    void Fn2F2F90(bool inFlag);             // 0x2f2f90 (sets +0x92 unless +0x90 is set)
};

class WindowManager : public RefCount {
public:
    void SetSoundDecoder(SoundDecoder* inDecoder);      // 0x32d490 (a SmartPtr at +0x58)
    void Fn32D5B0();                        // 0x32d5b0 (when the lobby closes)

    char _gap08[0x1c];
    MultiImageDecoder mImageDecoder;        // +0x24
    char _gap38[0x6d];
    bool mBA5;                              // +0xa5 (the lobby clears it)
};

// (0x14c bytes; vtable 0x85472c)
class MSWindowManager : public WindowManager {
public:
    MSWindowManager(HINSTANCE inInstance);  // 0x3d0900 (NULL: GetModuleHandle(NULL))

    char _gapa8[0xa0];
    HINSTANCE mInstance;                    // +0x148
};

// a window's icon (0x0c bytes; vtable 0x85107c)
class NativeIcon : public RefCount {
public:
    NativeIcon(DisplayContext* inContext);  // 0x328990

    SmartPtr<DisplayContext> mContext;      // +0x08
};

// (0x10 bytes; vtable 0x84c6d4; ??_G 0x297531 is EELobbyInterface.cpp's COMDAT)
class MSNativeIcon : public NativeIcon {
public:
    MSNativeIcon(DisplayContext* inContext, HICON inIcon) : NativeIcon(inContext), mIcon(inIcon) {}
    virtual ~MSNativeIcon();                // 0x3cf9f0

    HICON mIcon;                            // +0x0c
};

// r23 (EELobbyInterface 0x297ac9 / 0x29754d / 0x2972c6: the lobby's own window)
class Window : public RefCount {
public:
    virtual void Virtual04();
    virtual void Virtual08();
    virtual void Virtual0C();
    virtual void Virtual10();
    virtual void Virtual14();
    virtual void Virtual18();
    virtual void Virtual1C();
    virtual void Virtual20();
    virtual void Virtual24();
    virtual void Virtual28();
    virtual void Virtual2C();
    virtual void Create(const WONRectangle& inRect);                        // +0x30 (0x2972c6: 0, 0, 100, 100)
    virtual void SetPosSize(int inX, int inY, int inWidth, int inHeight);   // +0x34
    virtual void GetRect(WONRectangle& outRect, bool inClient);             // +0x38
    virtual void Virtual3C();
    virtual void Show(bool inShow);         // +0x40
    virtual void Virtual44();
    virtual void Virtual48();
    virtual bool Virtual4C();               // +0x4c (0x297ac9: the window's place is kept when it returns true)
    void Fn32A1E0();                        // 0x32a1e0 (after the lobby is shown: repaints mRoot)
    void SetTitle(const GUIString& inTitle);    // 0x32adc0 (a GUIString at +0x9c)
    void SetIcon(NativeIcon* inIcon);       // 0x32ae20 (a SmartPtr at +0x98)
    void PostEvent(WindowEventBase* inEvent, bool inFlag);         // 0x32e850

    char _gap08[0x20];
    DisplayContext* mDisplayContext;        // +0x28
    char _gap2c[0x48];
    long mStyle;                            // +0x74 (2; 0x22 in the game's full-screen mode)
    char _gap78[0x2c];
    RootContainer* mRoot;                   // +0xa4 (Fn32A1E0 invalidates it)
    char _gapa8[0x10];
};

// (0xc8 bytes; vtable 0x854754)
class MSWindow : public Window {
public:
    MSWindow();                             // 0x3d0d90

    long mLB8;                              // +0xb8
    long mLBC;                              // +0xbc
    char _gapc0[4];
    bool mBC4;                              // +0xc4
    bool mBC5;                              // +0xc5 (the lobby sets it)
};
// 0x3d08c0 (cdecl): the parent window of the MS windows created next (PROVISIONAL)
void LobbyFn3D08C0(HWND inParent);

// WONAPI lobby state functions the game's lobby glue calls (EELobbyInterface.cpp; names PROVISIONAL, r12-misc-h5)
int LobbyFn362760();                        // 0x362760 (how the lobby was left: 1, 2 or 3)
void LobbyFn362830();                       // 0x362830
void LobbyFn362880();                       // 0x362880
void LobbyFn362170();                       // 0x362170
Container* LobbyFn362250();                 // 0x362250 (the lobby's screen, added to the window's root; r24pre-won)
void LobbyFn362740();                       // 0x362740 (after the lobby window is shown; r24pre-won)
void LobbyGetNetAddresses(std::list<LobbyNetAddress>& outAddresses, bool inFlag);   // 0x2b32b0 (cdecl)
extern void* gLobbyNetHandle;               // 0x565f40 (the address the lobby uses)
extern GUIString gLobbyGameDoneTitle;       // 0x566470 (WONAPI resource text; PROVISIONAL)
extern GUIString gLobbyGameDoneText;        // 0x566994 (WONAPI resource text; PROVISIONAL)

// 0x361390: the local player (NULL when none)
LobbyPlayer* GetMyLobbyPlayer();
// 0x34a100 (cdecl): a text with its %s-style argument filled in (name PROVISIONAL)
GUIString LobbyFormatString(const GUIString& inFormat, const GUIString& inArg);
void LobbyFn3612B0(int inFlag);             // 0x3612b0
void LobbyFn361310(int inFlag);             // 0x361310
bool LobbyFn3613C0();                       // 0x3613c0
void LobbyFn361260();                       // 0x361260 (r12-misc-h7: after the open slot count changed)
void LobbyFn3613E0();                       // 0x3613e0 (r12-misc-h7: after a file transfer step)

class ServerDetailsItem : public ListItem {
public:
    ServerDetailsItem();                    // 0x3c4c80 (0x28 bytes)
    virtual ~ServerDetailsItem();           // 0x3c4d10

    LobbyGame* mGame;                       // +0x18
    char _gap1c[0xc];
};

class WindowEventBase : public RefCount {
public:
    WindowEventBase() : mL08(0), mL0C(0) {}

    long mL08;                              // +0x08
    long mL0C;                              // +0x0c
};

// an event that calls a function with one argument when it is delivered
class UserEvent : public WindowEventBase {
public:
    typedef void (*Func)(int inParam);

    UserEvent(Func inFunc, int inParam) : mFunc(inFunc), mParam(inParam) {}
    virtual void Deliver() { mFunc(mParam); }   // +0x04 0x295d30

    Func mFunc;                             // +0x10
    int mParam;                             // +0x14
};

// a component's event (slot +0x7c; type at +0x14: WONAPI's own ids, 100000 up for the game's)
class ComponentEvent : public WindowEventBase {
public:
    ComponentEvent(Component* inComponent, int inType);   // 0x32b530 (vtable 0x851174)
    int GetControlId() const;               // 0x32b600 (mComponent->mControlId)

    // +0x10: released by the implicit dtor (??_G 0x293804: `mov ecx,[esi+0x10]; test; call RefCount::Release`);
    // pointee type PROVISIONAL (r24-lobby-player-gfx R2). SmartPtr is ee/EEWonNet.h's: every includer has it first
    SmartPtr<RefCount> mL10;                // +0x10
    int mType;                              // +0x14
    long mL18;                              // +0x18
    Component* mComponent;                  // +0x1c
};

// a list's click (type 23; EELobbyFileDialog 0x29136a: a double click with button 0 loads; members PROVISIONAL)
class ListItemClickedEvent : public ComponentEvent {
public:
    int mParam;                             // +0x20 (the mouse button)
    bool mDoubleClick;                      // +0x24
};

// what a lobby event is about (class PROVISIONAL)
class LobbyEventTarget {
public:
    char _gap00[0x28];
    LobbyPlayer* mPlayer;                   // +0x28
};

// a lobby event delivered as a component event (RTTI LobbyEvent > ComponentEvent > WindowEventBase; 0x24 bytes:
// EELobbyGame's sync events, r23). Its out-of-line ctor 0x32b530 (component, type).
class LobbyEvent : public ComponentEvent {
public:
    LobbyEvent(int inType, long inParam) : ComponentEvent(NULL, inType), mParam(inParam) {}
    bool Fn359470();                        // 0x359470 (mParam == 2)

    long mParam;                            // +0x20
};

// the lobby event EELobbyPlayerOptionsCtrl 0x29a240 reads (type 100038; class PROVISIONAL): its kind is mParam
class LobbyTargetEvent : public LobbyEvent {
public:
    LobbyEventTarget* mTarget;              // +0x24
};

// a lobby change of a client (RTTI LobbyChangeEvent > LobbyEvent; r23)
class LobbyChangeEvent : public LobbyEvent {
public:
    LobbyChangeEvent(int inType, long inParam) : LobbyEvent(inType, inParam) {}
};

// a client's team changed (RTTI PlayerTeamChangedEvent > LobbyChangeEvent, vtable 0x851bf8; 0x28 bytes; r23)
class PlayerTeamChangedEvent : public LobbyChangeEvent {
public:
    PlayerTeamChangedEvent(LobbyClient* inClient, bool inFlag);   // 0x35ad30 (WONAPI)

    SmartPtr<LobbyClient> mClient;          // +0x24
};

// the room the local client is in (0x35b6f0's result; class PROVISIONAL, r23)
class LobbyRoomClients {
public:
    LobbyClient* GetClient(int inId);               // 0x3aa370 (32-bit: 0x692c7e pushes a zero-extended id, the callee reads a dword; signedness unproven: signed by the owner default 2026-11-01; r35 H10 / core P3)
};
// 0x35b6f0 (cdecl): the current room's clients (NULL outside a room; r23 PROVISIONAL)
LobbyRoomClients* LobbyFn35B6F0();

// the room's clients (LobbyGame +0x34; class name PROVISIONAL, r23)
class LobbyClientSet {
public:
    LobbyClient* FindClient(unsigned short inId);   // 0x3a5d30
};

// walks a LobbyClientSet (a 0xc-byte local; EELobbyGame 0x2930b5; names PROVISIONAL, r23)
class LobbyClientSetIterator {
public:
    LobbyClientSetIterator(LobbyClientSet* inSet);  // 0x3a6150
    bool HasMore();                         // 0x3a6180
    bool Next(LobbyClient** outClient);     // 0x3a6210

    char _gap00[0xc];
};

// 0x361370 (cdecl): the local client (EELobbyInterface 0x296d1e: its name +0x08; r23 PROVISIONAL)
LobbyClient* LobbyFn361370();
// 0x361b20 (cdecl): EELobbyInterface 0x296d1e passes it as the connection request's flag (r23 PROVISIONAL)
bool LobbyFn361B20();

// 0x359120 (cdecl): fires a component event of inType (EELobbyGame 0x2939be: 100065; r23 PROVISIONAL)
void LobbyFn359120(int inType, bool inFlag);
// 0x359100 (cdecl): delivers inEvent to the lobby's components (name PROVISIONAL, r23)
void LobbyFn359100(ComponentEvent* inEvent, bool inFlag);
// 0x361bc0 (cdecl; EELobbyGame 0x293b36 with a slot's client, 0; name PROVISIONAL, r23)
void LobbyFn361BC0(LobbyClient* inClient, bool inFlag);

// 0x361250: the lobby's window (NULL when the lobby is not open)
Window* GetLobbyWindow();

// one font of the lobby: slot +0x14 is its line height
class Font : public RefCount {
public:
    virtual int Virtual04();
    virtual int Virtual08();
    // +0x0c: the width of inLength characters of inText from inStart (0x295010 passes 0, text.length(); r23)
    virtual int GetStringWidth(const GUIString& inText, int inStart, unsigned short inLength);   // 16-bit length: 0x695010 pushes GUIString::length() unextended (r35 core P1)
    virtual int Virtual10();
    virtual int GetHeight();                // +0x14
};

// a list's text item (0x28 bytes; vtable 0x84c2fc; its inline ctor 0x290be2 and implicit dtor 0x290c50 are
// EELobbyConnectDialog.cpp's COMDATs; r12-misc-h7)
class TextListItem : public ListItem {
public:
    TextListItem(const GUIString& inText, Font* inFont) : mText(inText), mL1C(-1), mFont(inFont) {}
    virtual ~TextListItem() {}              // 0x290c50 (stores its vptr: declared)
    virtual void Virtual04();               // 0x2fa380
    virtual void Virtual08();               // 0x2fa2c0
    virtual void Virtual10();               // 0x2fa360

    GUIString mText;                        // +0x18
    long mL1C;                              // +0x1c (-1)
    SmartPtr<RefCount> mP20;                // +0x20
    SmartPtr<Font> mFont;                   // +0x24
};

// the lobby's list fonts (WONAPI globals; EELobbyConnectDialog: a connected player, a waiting one, a failed one;
// names PROVISIONAL)
extern SmartPtr<Font> gLobbyFont5665C0;     // 0x9665c0
extern SmartPtr<Font> gLobbyFont5668AC;     // 0x9668ac
extern SmartPtr<Font> gLobbyFont5666BC;     // 0x9666bc

class ListArea : public Container {
public:
    void Fn2FAA00();                        // 0x2faa00 (before the list is refilled)
    void Fn2FAA10();                        // 0x2faa10 (after it)
    void Fn2FAA50(bool inFlag);             // 0x2faa50 (clears it?)
    ListItem* GetItem(int inIndex);         // 0x2fc4e0
    void InsertItem(ListItem* inItem, int inPos = -1);         // 0x2fc510
    void Fn2FC730(ListItem* inItem);        // 0x2fc730 (an item changed)
    void RemoveItem(ListItem* inItem);      // 0x2fcb00
    void InsertString(const GUIString& inText, int inPos = -1);    // 0x2fcc30
    int GetNumItems() const { return mItems.size(); }         // 0x290d70 (EELobbyConnectDialog.cpp's COMDAT)

    char _gap84[0x8c];
    std::vector<ListItem*> mItems;          // +0x110
    char _gap120[0x18];
    Font* mFont;                            // +0x138
    char _gap13c[0x10];
    ListItem* mSelItem;                     // +0x14c (a combo box's list: ComboBox::GetSelItem 0x2f3d40)
};

// 0x3c5ff0: the sort/filter handlers of a server-list column
struct ServerColumnHandlers {
    void (*mGetText)(Graphics& inGraphics, ServerDetailsItem* inItem);   // draws the cell (0x3270c0 at 0, 0; r23)
    int (*mCompare)(ServerDetailsItem* inA, ServerDetailsItem* inB);
    void (*mFilter)(const GUIString& inText, std::list<ServerDetailsItem*>& ioItems);
};
ServerColumnHandlers* GetServerColumnHandlers(const GUIString& inName);

// 0x8dec84 / 0x8dec88 / 0x8df4d0 / 0x8d954c: the lobby's object factories
extern LobbyGame* (*gLobbyGameFactory)();
extern RefCount* (*gLobbyPlayerFactory)();
extern ServerDetailsItem* (*gServerDetailsItemFactory)();
extern LobbyConfig* (*gLobbyConfigFactory)();

}

#endif
