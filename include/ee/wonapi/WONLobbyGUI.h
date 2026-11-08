// WONAPI GUI / lobby framework classes as the game's lobby glue sees them.
// WONAPI proper (0x29deb0..) is not decompiled: bases come from the RTTI class hierarchy descriptors, sizes from the
// glue's `new` calls; other names are inferred from behaviour (provisional names); `_gap` arrays are undecoded. Requires <string>,
// ee/EEWonNet.h, ee/wonapi/WONLobbyResource.h.
#ifndef EE_WONAPI_WONLOBBYGUI_H
#define EE_WONAPI_WONLOBBYGUI_H

#include <exception>
#include <deque>
#include <set>
#include "ee/wonapi/WONBuffer.h"
#include "ee/wonapi/WONLobbyBackground.h"
#include "ee/wonapi/WONStringCompare.h"
#include "ee/wonapi/EELobbySound.h"
#include "ee/wonapi/WONLobbyImageDecoder.h"
#include "ee/wonapi/WONAPICore.h"

namespace WONAPI {

// SDK LobbyTypes enum; wire values 0/1/2/3
enum LobbyGameType
{
	LobbyGameType_None,
	LobbyGameType_Lan,
	LobbyGameType_Internet,
	LobbyGameType_DirectConnect
};

// SDK EventTypes.h ControlIdType; the registrar passes 100001/100002
enum ControlIdType
	{
		ControlId_LibMin	= 100000,
		ControlId_Ok,
		ControlId_Cancel,
		ControlId_TreeToggleOpen,
		ControlId_SplitterMin,
		ControlId_SplitterMax = ControlId_SplitterMin + 100,
		ControlId_UserMin	= 200000,
		ControlId_UserMin2	= 300000,
		ControlId_UserMin3	= 400000,
		ControlId_UserMin4	= 500000,
		ControlId_UserMin5	= 600000
	};

// Control id set; its members are the game's set<long> COMDATs (insert, _Erase, erase).
typedef std::set<long> ControlIdSet;
enum Direction { Direction_Left, Direction_Right, Direction_Up, Direction_Down };
enum HorzAlign { HorzAlign_Left, HorzAlign_Center, HorzAlign_Right };
enum VertAlign { VertAlign_Top, VertAlign_Center, VertAlign_Bottom };

// SDK event and flag constants; the input event ids 1..14 and the popup bit tests in the exe agree.
enum WindowEventType
	{
		WindowEventType_None,

		InputEvent_SizeChange,
		InputEvent_MouseMove,
		InputEvent_MouseDown,
		InputEvent_MouseDoubleClick,
		InputEvent_MouseDownCheckDoubleClick, 
		InputEvent_MouseUp,
		InputEvent_MouseWheel,
		InputEvent_KeyDown,
		InputEvent_KeyUp,
		InputEvent_KeyChar,
		InputEvent_GotFocus,
		InputEvent_LostFocus,
		InputEvent_MouseExit,
		InputEvent_WindowClose,

		ComponentEvent_ButtonPressed,
		ComponentEvent_Scroll,
		ComponentEvent_TextLinkActivated,
		ComponentEvent_HyperLinkActivated,
		ComponentEvent_HyperLinkOver,
		ComponentEvent_InputReturn,
		ComponentEvent_InputTextChanged,

		ComponentEvent_ListSelChanged,
		ComponentEvent_ListItemClicked,
		ComponentEvent_ListItemUpClicked,
		ComponentEvent_MenuItemSelected,
		ComponentEvent_TreeItemOpened,
		ComponentEvent_ComboSelChanged,
		ComponentEvent_TabBarTabSelected,
		ComponentEvent_TabCtrlTabSelected,
		ComponentEvent_EndPopup,
		ComponentEvent_BrowserStatusChange,
		ComponentEvent_BrowserLocationChange,
		ComponentEvent_BrowserButtonChange,
		ComponentEvent_SpinnerValueChanged,

		ComponentEvent_ContextStart	= 10000, 
		ComponentEvent_ContextEnd = 20000,

		Event_User_Start = 100000
	};
enum PopupFlags
{
	PopupFlag_EndOnLeftMouseDown			= 0x0001,
	PopupFlag_EndOnMiddleMouseDown			= 0x0002,
	PopupFlag_EndOnRightMouseDown			= 0x0004,
	PopupFlag_EndOnMouseMove				= 0x0008,
	PopupFlag_EndOnKeyDown					= 0x0010,
	PopupFlag_EndOnEscape					= 0x0020,
	PopupFlag_EndOnFocusChange				= 0x0040,
	PopupFlag_WantInput						= 0x0080,
	PopupFlag_IsModal						= 0x0100,
	PopupFlag_AdjustPosition				= 0x0200,
	PopupFlag_EatMouseDownOnEnd				= 0x0800,
	PopupFlag_Additional					= 0x1000,
	PopupFlag_FireEventOnEnd				= 0x2000,
	PopupFlag_DontWantMouse					= 0x4000,				

	PopupFlag_InOwnWindow				   = 0x10000,
	PopupFlag_NotInOwnWindow			   = 0x20000,

	PopupFlag_EndOnMouseDown				= 0x0007,
	PopupFlag_EndOnInput					= 0x001f,

	PopupFlag_StandardPopup					= 0x23e7,
	PopupFlag_StandardTip					= 0x6247
};

class WONRectangle;
class Window;
class SurfaceBase; // WONSurface.h: the software surface a DisplayContext draws into
class Component;
enum StandardCursor {
    StandardCursor_Arrow,StandardCursor_Hand,StandardCursor_IBeam,
    StandardCursor_EastWest,StandardCursor_NorthSouth,StandardCursor_NorthSouthEastWest,
    StandardCursor_Max
};
// Cursor (RefCount, 8 bytes); the constructor 0x326540 stores only the inherited refcount and vptr
class Cursor : public RefCount {
protected:
    virtual ~Cursor(); // out of line
public:
    Cursor(); // 0x326540
    virtual Cursor* GetNative(); // vtable +0x04, out of line
    virtual Component* GetComponent(int&,int&); // vtable +0x08, out of line
    static Cursor* GetStandardCursor(StandardCursor); // 0x326550: returns a shared cursor (not owned by the caller)
    static void SetStandardCursor(StandardCursor,Cursor*); //326560 mutable sharedreference
};
// Holds a cursor (0x0c bytes): SetStandardCursor (0x326577) allocates it and stores the owned cursor at +0x08
class CursorRef : public Cursor {
protected:
    SmartPtr<Cursor> mCursor; //08
public:
    CursorRef(Cursor* cursor) : mCursor(cursor) {}
    virtual Cursor* GetNative() { return this->mCursor->GetNative(); } // 0x326660
    virtual Component* GetComponent(int& x,int& y) { return this->mCursor->GetComponent(x,y); } // 0x326670
    void SetCursor(Cursor* cursor) { this->mCursor=cursor; }
};
// Only the prefix is declared; 0x326ce0 reads 17 DWORDs from +0x08
enum StandardColor
{
	StandardColor_3DDarkShadow = 0,
	StandardColor_3DFace,
	StandardColor_3DHilight,
	StandardColor_3DShadow,
	StandardColor_Scrollbar,

	StandardColor_ButtonText,
	StandardColor_GrayText,
	StandardColor_Hilight,
	StandardColor_HilightText,

	StandardColor_ToolTipBack,
	StandardColor_ToolTipText,

	StandardColor_MenuBack,
	StandardColor_MenuText,

	StandardColor_Back,
	StandardColor_Text,

	StandardColor_Link,
	StandardColor_LinkDown,


	StandardColor_Max
};

enum { COLOR_INVALID = 0xffffffff };
class ColorScheme : public RefCount {
public:
    DWORD mColors[17];
    ColorScheme() { memset(this->mColors,0,sizeof(this->mColors)); } // zeroes all 17 colors (0x32b973..0x32b986)
    static DWORD GetColorRef(StandardColor theColor) { return 0x80000000 + theColor; } // inline
    DWORD GetStandardColor(int i) { return this->mColors[i]; }
    void SetStandardColor(int i,DWORD color) { this->mColors[i]=color; }
};
class Font;
class Image;
class NativeImage;
class NativeIcon;
class SelectionColor;
class RawImage32;
class DisplayContext;
struct FontDescriptor;
class Button;
class InputBox;
class ListItem;
class TextListItem;
class Component;
class ChildLayoutInstructionBase;
class Container;
class ComponentConfig;
class ComponentEvent;
class ComponentExtra;
class BackgroundGrabber;
class ListArea;
class MultiListArea;
class ScrollArea;
class ScrollAreaViewport;
class Scroller;
class LobbyClient;
class LobbyPlayer;
class LobbyClientList;
struct RoutingClientInfo;
class ConfigParser;
class StringParser;
class ReadBuffer;
class WriteBuffer;
class ByteBuffer;
class LobbyGroup;

// what a component is initialised from (slot +0x1c); type 2: from its resource configuration
struct ComponentInit {
    int mType;                              // +0x00
};

struct ComponentConfigInit : public ComponentInit {
    ComponentConfigInit(ComponentConfig*); // 0x38aa40: type 2 and the config at +0x04
    ComponentConfig* mConfig;               // +0x04
};

class Graphics;
class RootContainer;
class ComponentListener;
struct PopupParams;
extern GUIString gEmptyGUIString;           // 0x966244

// a GUI component (0x30 bytes: EELobbyColorChooser's members start at +0x30; vtable 0x84d71c, 43
// slots). Unknown slots are named by vtable offset. The inline bodies are the COMDATs the first lobby unit
// (EELobbyColorButton.cpp 0x29059d, 0x2905a2) keeps; the other tiny slots are ICF-folded into game code.
class WONRectangle {
public:
    WONRectangle();                         // 0x349300
    WONRectangle(int inX, int inY, int inWidth, int inHeight);    // 0x349310
    void DoIntersect(const WONRectangle&); // 0x349330, called from 0x3271b1
    void DoUnion(const WONRectangle&); // 0x3493b0 (called from RootContainer at 0x30498b)
    bool Intersects(const WONRectangle&) const; // 0x349450
    int Left() const { return this->x; }
    int Top() const { return this->y; }
    int Width() const { return this->width; }
    int Height() const { return this->height; }
    int Right() const { return this->x+this->width; }
    int Bottom() const { return this->y+this->height; }

    int x;                                  // +0x00
    int y;                                  // +0x04
    int width;                              // +0x08
    int height;                             // +0x0c
};

class Component : public RefCount {
public:
    Component();                            // 0x2f49e0 (WONAPI)
    virtual ~Component();                   // 0x2f4a70 (WONAPI)
    virtual void PositionChanged();
    virtual void Virtual08();
    virtual void GetDesiredSize(int& outWidth, int& outHeight);  // (EELobbyColorChooser 0x2908af: 120 x 120)
    virtual bool NeedsPaint();
    virtual void PrePaint(Graphics& inGraphics);
    virtual void Paint(Graphics& inGraphics);                  // +0x18 (0x2f5020)
    virtual void InitComponent(ComponentInit& inInit) {} // empty inline (the vtable slot holds the shared ret 4 at 0x156dd8)
    virtual bool HitTest(int inX, int inY);
    virtual void MouseEnter(int inA, int inB);                 // +0x24 (0x2f4cc0)
    virtual void MouseExit();                                  // +0x28 (0x2f4ce0: the mouse left?)
    virtual void MouseDown(int,int,int) {} // empty inline (the vtable slot holds the shared 3-byte body at 0x3a6880)
    virtual void MouseUp(int inX, int inY, int inButton) {}    // +0x30 0x2907a3 (emitted with EELobbyColorChooser)
    virtual void MouseMove(int inX, int inY) {}                // +0x34 (the mouse moved); empty inline
    virtual void MouseDrag(int inX, int inY);
    virtual bool MouseWheel(int inAmount) { return false; }    // +0x3c 0x29059d
    virtual void MouseHover(unsigned long inDelta);
    virtual bool TimerEvent(int inDelta);
    virtual void GotFocus();
    virtual void LostFocus();
    virtual bool KeyChar(int inKey) { return false; }          // +0x50 (ICF: 0x29059d)
    virtual bool KeyDown(int inKey) { return false; }          // +0x54 (ICF: 0x29059d)
    virtual bool KeyUp(int inKey) { return false; }            // +0x58 (ICF: 0x29059d)
    virtual bool CanClose();
    virtual void InvalidateRect(WONRectangle& ioRect);
    virtual void Invalidate();                                 // +0x64 (after a visible change; provisional name)
    virtual void MarkInvalid();
    virtual void AddedToParent();                              // +0x6c (0x2f5160: when the popup opens?)
    virtual void ClearGrabBG();
    virtual void EnabledChanged(bool inFlag);
    virtual void SetCursor(Cursor* inCursor);
    virtual void HandleComponentEvent(ComponentEvent* inEvent);    // +0x7c (0x2f5550)
    virtual void HandleBroadcastEvent(ComponentEvent* inEvent);
    virtual void NotifyRootVisibility(bool inFlag);
    virtual bool TabFocus(bool inForward, bool inInit);
    virtual void EnableControlsById(const ControlIdSet& inIds, bool inEnabled);
    virtual void SetEnabled(bool inEnabled);                   // +0x90 (0x2f5610: clears/sets flag 0x80 of +0x20)
    virtual void SetVisible(bool inVisible);                   // +0x94 (0x2f55b0)
    virtual Component* GetMouseChild();
    virtual int GetMinWidth() { return 0; } // returns 0 (shared body at 0x1ad67e)
    virtual int GetMinHeight() { return 0; } // returns 0 (shared body at 0x1ad67e)
    virtual int GetMaxWidth() { return 0xffff; }               // +0xa4 0x2905a2
    virtual int GetMaxHeight() { return 0xffff; }              // +0xa8 (ICF: 0x2905a2)

    void SetColorScheme(ColorScheme* inScheme); // 0x2f59a0, owns ComponentExtra+4
    ColorScheme* GetOwnColorScheme(); // 0x2f5960, the component's own scheme
    ColorScheme* GetColorScheme(); // 0x2f5970, own or inherited scheme
    void SetPos(int inX, int inY);            // 0x2f4b70
    void SetSize(int inWidth, int inHeight);  // 0x2f4b90
    void SetSizeAtLeast(int inMinWidth, int inMinHeight); // 0x2f4bc0
    void SetPosSize(int inX, int inY, int inWidth, int inHeight); // 0x2f4bf0
    void Fn2F4EE0(int inPadding);            // 0x2f4ee0
    void FireBroadcastEvent(ComponentEvent* inEvent, bool inFlag); // 0x2f52a0
    void BroadcastEventDown(ComponentEvent* inEvent); // 0x2f52d0, broadcasts down to the children
    void DeliverEvent(ComponentEvent* inEvent, bool inSendNow); // 0x2f53f0
    SmartPtr<Font> GetFont(const FontDescriptor&); // 0x2f5a00: returns the font through a hidden result pointer (ret 8)
    Font* GetFontMod(Font* inFont, int inStyle, int inOptionalSize); // 0x2f5a30
    Font* GetDefaultFont();                  // 0x2f5b20
    Font* GetNamedFont(const std::string&); // 0x2f5b30: looks the font up in the window manager
    Font* GetDefaultFontMod(int inStyle, int inSize);  // 0x2f5b50
    void AddListener(ComponentListener* inListener);           // 0x2f4b10
    void SetDesiredSize(int inA, int inB);  // 0x2f4e90 (on a new popup: 0, 0)
    void Fn2F5380(int inType, int inParam) { this->FireEvent(inType, inParam != 0); } // 0x2f5380: FireEvent's older name
    void FireEvent(int,bool=false); // 0x2f5380
    bool IsRootVisible();                   // 0x2f5600: visible all the way up to the root (flag bit 18)
    void LocalToRoot(int& ioX, int& ioY);   // 0x2f56d0 (to the root's coordinates?)
    void BubbleEvent(ComponentEvent* inEvent); // 0x2f54e0: passes the event up to the parents
    Component* GetNextSibling(bool,bool=false); // 0x2f5230: sibling traversal
    bool WantFocus(); // 0x2f5810: can take the focus
    void RootToChild(int&,int&); // 0x2f5720
    void GetMousePos(int&,int&); // 0x2f5770
    int GetKeyMod(); // 0x2f57a0
    bool CheckDoubleClick(int); // 0x2f57b0
    bool Contains(int,int); // 0x2f5080: point-in-bounds test in parent coordinates
    bool Intersects(Component*); // 0x2f50c0: rectangle intersection test
    Cursor* GetDefaultCursor(); // 0x2f4b00: default cursor getter
    BackgroundGrabber* GetGrabBG(); // 0x2f4af0, from the extra data at +0x10
    void AllocateExtraStuff(); // 0x2f4ac0
    bool CheckGrabBG(); // 0x2f4db0: decides whether to grab the background
    void EnsureOnTopOf(Component*); // 0x2f4c20: forwards to the parent (0x2f6f60)
    void SetDefaultCursor(Cursor*); // 0x2f4c60: sets the component's own default cursor (not the virtual at +0x78)
    void SetDesiredHeight(int); // 0x2f4f20
    void SetDesiredWidth(int=0); // 0x2f4ee0
    void SetTranslucentColor(int,int=4); // 0x2f4fb0
    void SetComponentFlags(int,bool); // 0x2f5560
    void SetWantGrabBG(bool inGrab) { this->SetComponentFlags(0x100,inGrab); }
    void SubscribeToBroadcast(bool); // 0x2f5360 (nonvirtual)
    void RequestTimer(bool); // 0x2f57d0
    void RequestFocus();                    // 0x2f5890
    Window* GetRoot();               // 0x2f5920, legacy nominal name; true SDKGetWindow
    RootContainer* GetRootContainer(); // 0x2f58f0: walks up the parents and dynamic_casts to RootContainer
    Window* GetRootWindow();         // 0x2f5940 (root Window or default Window)
    bool ComponentFlagSet(int flags) { return this->mComponentBits & flags ? true : false; }
    bool IsVisible() { return !this->ComponentFlagSet(1); }
    bool HasFocus() { return this->ComponentFlagSet(0x10); }
    bool IsDisabled() const { return (this->mComponentBits & 0x80) != 0; }  // (EELobbyColorButton 0x29060b: shr 7; test al, 1)

    const WONRectangle& GetBounds() { return this->mBounds; }
    int Left() { return this->mBounds.x; }
    int Top() { return this->mBounds.y; }
    int Width() { return this->mBounds.Width(); }
    int Height() { return this->mBounds.Height(); }
    int Right() { return this->mBounds.Right(); }
    int Bottom() { return this->mBounds.Bottom(); }
    Container* GetParent() { return this->mParent; } // inline, at +0x08
    Container* mParent;                     // +0x08 (SetEnabled notifies it through its slot +0xbc)
    std::list<SmartPtr<Component> >::iterator mChildListItr; // +0x0c, child-list node iterator
protected:
    WONRectangle mBounds; // at +0x10..0x1f, embedded rectangle (16 bytes)
public:
    int mComponentBits; //20, full DWORD flags (int: the tests compile to shr n; test 1); bit80 disabled
    int mControlId;                         // +0x24 (the component events' id)
    unsigned char mFocusId; //28
    char mInvalidState; //29
    char mTimerFlags; //2a
    ComponentExtra* mExtraStuff; // at +0x2c, owned (allocated on demand)
};

// SDK function template; the LayoutPoint* instance is at 0x3a1da0.
// Declared only: the dynamic cast and the typed exception are in the original body.
template<class T> T SafeConfigGetComponent(Component*,const char*);
// LayoutPoint (0x30 bytes): allocated at 0x389ac5, RTTI 0x44fd9c, no members of its own
// Same 43 vtable slots as Component; its implicit destructor jumps to the base one
class LayoutPoint : public Component {
public:
    LayoutPoint(int left=0,int top=0,int width=0,int height=0) { this->SetPosSize(left,top,width,height); }
    // Inline constructor (0x389ae6..0x389afe); the destructor 0x306860 jumps to Component's at 0x2f4a70
};

// a component holding others (0x84 bytes: EELobbyGameOptionsCtrl's members start at +0x84; vtable 0x84d7cc, 51 slots)
// Two owning lists (0x20 bytes); constructor and destructor out of line.
class ChildLayoutManager : public RefCount {
public:
    typedef std::list<SmartPtr<ChildLayoutInstructionBase> > InstructionList;
    void ExecuteInstructions(Component*);
    void AddInstruction(ChildLayoutInstructionBase*);
    void AddLayoutPoint(LayoutPoint*);
    void RemoveComponent(Component*);
    void Clear();
    void EnableInstructions(int, bool);
    InstructionList mInstructions; //08
    std::list<SmartPtr<LayoutPoint> > mLayoutPoints; //14
};
class Container : public Component {
public:
    Container();                            // 0x2f5b70 (WONAPI, /GX)
    virtual ~Container();                   // 0x2f5e80 (WONAPI, /GX)
    // Overrides (vtable 0x44d7cc).
    virtual void Virtual08();               // 0x2f62b0
    virtual void GetDesiredSize(int& outWidth, int& outHeight); // 0x2f62d0
    virtual void PrePaint(Graphics& inGraphics);  // 0x2f6340
    virtual void Paint(Graphics& inGraphics); // 0x2f63b0
    virtual bool HitTest(int inX, int inY);   // 0x2f6520
    virtual void MouseEnter(int inX, int inY); // 0x2f6700
    virtual void MouseExit();               // 0x2f6730
    virtual void MouseDown(int inX, int inY, int inButton); // 0x2f6880
    virtual void MouseUp(int inX, int inY, int inButton);   // 0x2f6840
    virtual void MouseMove(int inX, int inY); // 0x2f6780
    virtual void MouseDrag(int inX, int inY); // 0x2f6810
    virtual bool MouseWheel(int inWheel);     // 0x2f67c0
    virtual void MouseHover(unsigned long inDelta); // 0x2f67f0
    virtual bool TimerEvent(int inDelta); // 0x2f7210
    virtual void GotFocus();                // 0x2f7400
    virtual void LostFocus();               // 0x2f7420
    virtual bool KeyChar(int inKey);         // 0x2f7710
    virtual bool KeyDown(int inKey);         // 0x2f7690
    virtual bool KeyUp(int inChar);          // 0x2f76f0
    virtual bool CanClose();                // 0x2f7730
    virtual void Invalidate();              // 0x2f6980
    virtual void MarkInvalid();             // 0x2f6940
    virtual void AddedToParent();           // 0x2f6990
    virtual void ClearGrabBG();             // 0x2f69a0
    virtual void SetCursor(Cursor* inCursor);    // 0x2f7360
    virtual void HandleBroadcastEvent(ComponentEvent* inEvent); // 0x2f7320
    virtual void NotifyRootVisibility(bool inFlag);  // 0x2f64d0
    virtual bool TabFocus(bool inForward, bool inInit);  // 0x2f75a0
    virtual void EnableControlsById(const ControlIdSet& inIds, bool inEnabled); // 0x2f6480
    virtual Component* GetMouseChild() { return this->mMouseChild; } // 0x1c42ce (shared 4-byte body)
    // 0x2f6140 (ret 0x20): adds inChild laid out against inRef (EELobbyInterface 0x2972c6: the lobby screen, 3,
    // the root, 0, 0, 0, 0, 0; provisional name)
    void Fn2F6140(Component* inChild, int inFlags, Component* inRef, int inA, int inB, int inC, int inD, int inE);
    void AddChildLayout(ChildLayoutInstructionBase*,int=0); // 0x2f61d0: keeps the instruction; group id 0 runs first (the test reads the full int, the id is stored as a byte)
    void EnableLayoutInstructions(int inId, bool inEnabled); // 0x2f6120
    void Relayout();                         // 0x2f6290
    void EnterModal(); // 0x2f6210: counts the modal level and moves the focus
    void LeaveModal(); // 0x2f6260: counts the modal level and moves the focus
    void RemoveAllChildren(); // 0x2f6d80 (WindowManager::EndPopup calls it at 0x32e663/0x32e67c)
    void UpdateMouseChild(int,int); //2f6650 mouse-selection transition99
    void MouseAt(); // 0x2f66c0: refreshes the mouse state (no arguments)
    std::list<SmartPtr<Component> >::iterator FindTop(); // 0x2f6a40 (forwarder)
    std::list<SmartPtr<Component> >::iterator FindTop(std::list<SmartPtr<Component> >::iterator); //2f69d0 real suffix ordering99
    void SetOnlyChildInput(bool v) { this->mOnlyChildInput=v; } // at +0x7c
    void SetIsolateTabFocus(bool v) { this->mIsolateTabFocus=v; } // inline (used at 0x389f3b), at +0x7a
    void AddLayoutPoint(LayoutPoint*); // 0x2f6100: keeps the layout point as a child
    void AddChildEx(Component*,int); // 0x2f6b50
    void BringToTop(Component*); // 0x2f6dd0
    void SetHasUsedDialogKeys(bool v) { this->mHasUsedDialogKeys=v; } // inline, at +0x7b
    unsigned char GetNextFocusId(); // 0x2f6010
    void SetFocusToLastFocusChild(); // 0x2f6030
    void EnsureOnTopOf(Component*,Component*); // 0x2f6f60
    void AddChild(Component* inChild);      // 0x2f6a60
    virtual Component* FindChildAt(int inX, int inY); // 0x2f65b0
    virtual Component* FindClickChildAt(int inX, int inY) { return this->FindChildAt(inX, inY); } // 0x2909c7
    virtual void SetFocusChild(Component* inChild);
    virtual void AddTimerChild(Component* inChild);
    virtual void ChildStateChanged(Component* inChild);
    virtual bool ChildInvalidated(Component* inChild);
    virtual void RemoveChild(Component* inChild);
    virtual Component* FindChildById(int inControlId);

    // Private members: constructor 0x2f5b70, destructor 0x2f5e80, readers 0x2f62d0/0x2f7770.
    // The remaining private members are set up by the out-of-line constructor and destructor.
    Component* mMouseChild;                // +0x30, borrowed
    Component* mFocusChild;                // +0x34, borrowed
    std::list<SmartPtr<Component> > mChildList; // +0x38, owning values
    typedef std::set<SmartPtr<Component>,SmartPtr<Component>::Comp> ChildTimerSet;
    ChildTimerSet mChildTimerSet; // at +0x44..0x4f, owns the child timers
    ChildLayoutManager mChildLayout; // at +0x50..0x6f, owns the child layout instructions (0x20 bytes)
    unsigned char mNextFocusId; // at +0x70; incremented as a byte and wraps to 0 (0x2f6010); +0x71..0x73 padding
    SmartPtr<Cursor> mCurrentCursor; //74, ctor2f5c45/dtor2f5ed2/Root304a51
    bool mChildNeedsTimer; //78
    bool mMouseWheelGoesToFocusChild; //79
    bool mIsolateTabFocus; // at +0x7a (set at 0x30420f)
    bool mHasUsedDialogKeys; // at +0x7b, set by Tab handling (not mIsolateTabFocus at +0x7a)
    bool mOnlyChildInput; //7c
    bool mRemovingAllChildren; // at +0x7d, set by RemoveAllChildren, read by RemoveChild; +0x7e/0x7f padding
    int mModalCount; // at +0x80, signed count; modal children take the input while it is nonzero
protected:
    static bool mMouseWheelGoesToFocusChildDef; // 0x566234 (declaration only)
};

// ScreenContainer (0x88 bytes); constructor 0x306a20, destructor 0x306a60; points at the current screen.
class ScreenContainer : public Container {
public:
    ScreenContainer();
    // Implicit destructor (0x306a60) jumps to Container's at 0x2f5e80; the screen at +0x84 is not owned.
    void ShowScreen(Component* inScreen, bool inTakeFocus = true); //306ab0
    void AddScreen(Component*); //306a70
    Component* mCurScreen; //84, borrowed
};


// Child layout instructions (six concrete classes below)
class ChildLayoutInstructionBase : public RefCount {
public:
    ChildLayoutInstructionBase() : mGroupId(0),mEnabled(true) {} // inline in all six subclass constructors
    virtual void Execute(Component*) {} // empty inline (shared body at 0x30a800)
    virtual bool UsesComponent(Component*) { return false; } // returns false (shared body at 0x3a90d0)
    unsigned char mGroupId;                 // +08
    bool mEnabled;                         // +09
};
// A plain value class (no vtable) that owns its nodes and points at components it does not own
class ChildLayoutList
{
protected:
	typedef std::list<Component*> ChildList;
	mutable ChildList mChildList;
	mutable ChildList::iterator mItr;

public:
	ChildLayoutList(Component *c1);
	ChildLayoutList(Component *c1, Component *c2);
	ChildLayoutList(Component *c1, Component *c2, Component *c3);
	ChildLayoutList(Component *c1, Component *c2, Component *c3, Component *c4);
	ChildLayoutList(Component *c1, Component *c2, Component *c3, Component *c4, Component *c5);
	ChildLayoutList(Component *c1, Component *c2, Component *c3, Component *c4, Component *c5, Component *c6);
	ChildLayoutList(Component *c1, Component *c2, Component *c3, Component *c4, Component *c5, Component *c6, Component *c7);
	ChildLayoutList(Component *c1, Component *c2, Component *c3, Component *c4, Component *c5, Component *c6, Component *c7, Component *c8);

	void Rewind() const;
	bool HasMore() const;
	Component* GetNext() const;

	void AddTo(ChildList &theList) const;
};
class CenterLayout : public ChildLayoutInstructionBase {
public:
    CenterLayout(Component* box,bool horz,bool vert) : mHorz(horz),mVert(vert),mCenterBox(box) {} // inline (0x388f6d..0x388fb3); the class is 0x20 bytes
    CenterLayout(Component* box,Component* first,bool horz,bool vert) : mHorz(horz),mVert(vert),mCenterBox(box) { this->Add(first); } // inline (0x3624e8..0x362530); its cleanup covers the first insertion
    CenterLayout(Component* box,const ChildLayoutList& list,bool horz,bool vert) : mHorz(horz),mVert(vert),mCenterBox(box) { this->Add(list); }
    CenterLayout* Add(const ChildLayoutList& list) { list.AddTo(this->mComponentList); return this; }
    CenterLayout* Add(Component* component) { this->mComponentList.push_back(component); return this; }
    static void Center(Component* inParent, Component* inChild, bool inHorz, bool inVert); // 337690
    virtual void Execute(Component*);
    bool mHorz;                            // +0c
    bool mVert;                            // +0d
    Component* mCenterBox;                  // +10, borrowed
    std::list<Component*> mComponentList;   // +14, owns nodes, borrows components
};

// a modal or popup dialog (0x98 bytes: EELobbyConnectDialog's members start at +0x98; its implicit dtor is the
// lobby glue's COMDAT 0x290a26)
class Dialog : public Container {
public:
    virtual void Virtual08();               // 0x2f7a60
    virtual void MouseExit();               // 0x2f7b20
    virtual void MouseDown(int inX, int inY, int inButton); // 0x2f7a70
    virtual void MouseUp(int inX, int inY, int inButton);   // 0x2f7af0
    virtual void MouseDrag(int inX, int inY); // 0x2f7b40
    virtual void GotFocus();                // 0x2f7a20
    virtual void LostFocus();               // 0x2f7a40
    virtual bool KeyDown(int inKey);         // 0x2f7c30
    virtual bool CanClose();                // 0x2f7c90
    Dialog();                               // 0x2f7910 (WONAPI)
    void SetEndOnEscape(int); // 0x2f7990
    void SetDontEndOnEscape(); // 0x2f79b0
    void SetDefaultButton(Button*); // 0x2f79c0: keeps the button at +0x94
    bool ActivateDefaultButton(); // 0x2f7ce0: activates the default button when one is set
    virtual void HandleComponentEvent(ComponentEvent* inEvent);    // +0x7c 0x2f7d20 (WONAPI)

    int mDragX;                            // +0x84
    int mDragY;                            // +0x88
    bool mDragging;                        // +0x8c, dragging; the constructor does not initialize it
    bool mAllowDrag;                       // +0x8d
    bool mKeepInRoot;                      // +0x8e
    bool mEndOnEscape;                     // +0x8f
    int mEscapeId;                         // +0x90
    SmartPtr<Button> mDefaultButton;        // +0x94, the default button
};

class Button : public Component {
public:
    void Fn2F3440(); // click activation (tests the enabled and checked states), no arguments
    Button();                               // 0x2f3070 (WONAPI)
    virtual ~Button();                      // 0x2f3100 (WONAPI)
    virtual bool HandleActivate();          // +0xac (a click; EELobbyColorButton 0x290669)
    virtual void SetTextHook(const GUIString&); // 0x2f30d0
    virtual void SetText(const GUIString& inText, bool inFlag);    // +0xb4 (0x2f3180)
    virtual const GUIString& GetText() { return gEmptyGUIString; }   // +0xb8 0x2905a8
    void SetCheck(bool inChecked, bool inNotify = false);      // 0x2f3500
    void SetButtonFlags(unsigned long inFlags, bool inSet);    // 0x2f3370 (EELobbyColorButton: 0x40, true)

    union {
        unsigned long mButtonFlags; // at +0x30, flags
        struct {
    unsigned long mButtonFlag0 : 1;         // +0x30 bits (Button::SetCheck 0x2f3500 tests bit 3)
    unsigned long mButtonFlag1 : 1;
    unsigned long mChecked : 1;             // +0x30 bit 2 (a check box's state)
    unsigned long mButtonFlag3 : 1;
    unsigned long mButtonFlag4 : 1;
    unsigned long mButtonFlag5 : 1;         // +0x30 bit 5 (EELobbyColorButton 0x29060b draws it pressed)
        };
    };
    // the flag tests read one bit through an inline bool getter (shr n; test 1: Button::SetButtonFlags 0x2f3370)
    bool IsRepeat() { return (mButtonFlags & 0x02) != 0; }
    bool IsChecked() { return (mButtonFlags & 0x04) ? true : false; }
    bool IsRadio() { return (mButtonFlags & 0x08) != 0; }
    bool IsPressed() { return (mButtonFlags & 0x20) != 0; }
    bool IsActivateOnPress() { return (mButtonFlags & 0x40) != 0; }
    bool IsNoActivate() { return (mButtonFlags & 0x200) != 0; }
    bool IsDefaultOnFocus() { return (mButtonFlags & 0x400) != 0; }
    bool mKeyDown; //34
    int mDelayAcc; //38
    SmartPtr<Sound> mClickSound; // at +0x3c
    unsigned char mMouseDownState; //40, 0=up 1=downoutside 2=downover
    static SmartPtr<Button> mDefaultButton; // 0x966224
    static SmartPtr<Button> mDialogDefaultButton; // 0x966228
    static void SetDialogDefaultButton(Button*); //2f31c0
    static void UnSetDialogDefaultButton(Button*); //2f3230
    static void SetDefaultButton(Button*); //2f3280
    static void UnSetDefaultButton(Button*); //2f3300
    void CopyAttributes(Button*); //2f3310
    void ButtonDown(); //2f3660
    void ButtonUp(bool); //2f36d0
    Button* GetNextRadio(bool); //2f34a0
    Button* GetEndRadio(bool); //2f34e0
    bool IsMouseOver(); //2f3170
    bool IsDefaultButton(); //2f31b0
    void SetClickSound(Sound*); //2f3910
    virtual void AddedToParent(); //2f33c0 AddedToParent
    virtual void MouseEnter(int,int); //2f35b0 MouseEnter
    virtual void MouseExit(); //2f3620 MouseExit
    virtual void MouseDown(int,int,int); //MouseDown
    virtual void MouseUp(int,int,int);   //MouseUp
    virtual void MouseDrag(int,int); //2f3740 MouseDrag
    virtual void GotFocus();  //2f37a0 GotFocus
    virtual void LostFocus(); //2f37d0 LostFocus
    virtual bool KeyUp(int);     // 0x2f38b0, vtable +0x58
    virtual bool KeyDown(int);   //KeyDown
    virtual bool TimerEvent(int); //TimerEvent
    virtual void MouseHover(unsigned long); // 0x2f3900 (forwarder)
};

// Only a prefix of LineSegment (0x14 bytes, 32 vtable slots) is declared: nothing here allocates,
// constructs, copies or destroys these rich-text classes.
class MultiLineBuilder;
class ScrollAreaViewport;
struct ElementSelectionInfo;
struct ElementFindInfo;
enum MouseButton { MouseButton_Left=0,MouseButton_Middle=1,MouseButton_Right=2 };
struct ElementPaintInfo {
    int mClipX,mClipY,mClipWidth,mClipHeight;
    int mLineY,mLineHeight;
    int mStartSelSegment,mEndSelSegment,mStartSelSegmentPos,mEndSelSegmentPos;
    ElementPaintInfo() : mLineY(-1),mLineHeight(-1) {}
};
class Table;
class LineSegment : public RefCount {
public:
    bool DrawSelection1(Graphics&,int,int,ElementPaintInfo&); //309570
    void DrawSelection2(Graphics&,int,int); //3095e0
    static SmartPtr<Image> mSelectImageMask; // 0x566220 (defined in the original, not here)
    static int mCounter; // 0x566240 (defined in the original, not here)
    static Table* mTableContext; // 0x56623c (declaration only)
    // Implicit destructor (RefCount's virtual slot 0).
    // No owned members; TextPiece's destructor (0x311480) goes to the generic one at 0x338c70.
    virtual int GetWidth(); //04
    virtual int GetMinWidth(); //08
    virtual int GetMaxWidth(); //0c
    virtual int GetHeight(); //10
    virtual int GetAscent(); //14
    virtual int GetFirstWrapWidth(); //18
    virtual char GetVertAlign(); //1c
    virtual int GetStartSelIndex(); //20
    virtual int GetEndSelIndex(); //24
    virtual void MouseEnter(int,int); //28
    virtual void MouseExit(); //2c
    virtual void MouseDown(int,int,MouseButton); //30
    virtual void MouseUp(int,int,MouseButton); //34
    virtual void MouseMove(int,int); //38
    virtual void MouseDrag(int,int); //3c
    virtual void LinkDown(); //40
    virtual void LinkUp(); //44
    virtual bool CanStartSelection(); //48
    virtual int GetSelChar(int,int); //4c
    virtual bool GetSelCharPos(int&,int&,int,int); //50
    virtual void GetSelText(GUIString&,ElementSelectionInfo&); //54
    virtual bool FindStr(ElementFindInfo&); //58
    virtual void SetSelIndex() { this->mStartSelIndex=LineSegment::mCounter++; } //5c
    virtual LineSegment* GetSegmentAt(int&,int&,bool); //60
    virtual bool IsInvalid(); //64
    virtual bool AllowModalInput(); //68
    virtual bool PerformInstructions(MultiLineBuilder*); //6c
    virtual void Paint(Graphics&,int,int,ElementPaintInfo&); //70
    virtual void Wrap(MultiLineBuilder*,bool); //74, target excludes later SDK PaintSelection
    virtual void SetMaxWidth(int); //78
    virtual void SetViewport(ScrollAreaViewport*,int,int); //7c
    int mStartSelIndex,mAbsX,mAbsY; // at +0x08, +0x0c, +0x10
};
class LineElement;
// Floating record (0x10 bytes) with two virtual size queries (no virtual destructor).
// Vector stride 0x10 at 0x30cbb5/0x30cd35/0x30d7c8; the segment at +0x04.
struct FloatingSegment {
    SmartPtr<LineSegment> mSegment; //04, vptr00
    int mx,my; //08,0c
    FloatingSegment(LineSegment* segment=NULL,int x=0,int y=0):mSegment(segment),mx(x),my(y) {}
    virtual int GetWidth() { return this->mSegment->GetWidth(); }
    virtual int GetHeight() { return this->mSegment->GetHeight(); }
};
// Segment list value (8 bytes); list node payload 8, flags at +0x0c.
struct SegmentInfo {
    SmartPtr<LineSegment> mSegment;
    unsigned short mFlags;
    SegmentInfo(LineSegment* segment=NULL,unsigned short flags=0):mSegment(segment),mFlags(flags) {}
};
// MultiLineElement (0x68 bytes); constructor and destructor out of line.
class MultiLineElement : public LineSegment {
public:
    MultiLineElement(); // 0x30c7e0
    virtual int GetWidth(); // 0x30c8f0
    void CalcMinMaxWidths(); // empty; calls from 0x30d000/0x30d020 reach the shared body at 0x4142a0
    virtual int GetMinWidth(); // 0x30d000
    virtual int GetMaxWidth(); // 0x30d020
    virtual int GetHeight(); // 0x30c900
    virtual LineSegment* GetSegmentAt(int&,int&,bool); //30ca30
    virtual void SetViewport(ScrollAreaViewport*,int,int); //30cb60
    virtual void GetSelText(GUIString&,ElementSelectionInfo&); //30cc50
    virtual bool GetSelCharPos(int&,int&,int,int); //30ccd0
    virtual bool FindStr(ElementFindInfo&); //30cd70
    virtual void SetSelIndex(); //30cbf0
    virtual int GetEndSelIndex(); // 0x30cc30
    bool IsLastLineEmpty(); // tests the segment's tail flag (not identified in the original exe)
    virtual void Paint(Graphics&,int,int,ElementPaintInfo&); //30d6e0
    virtual void SetMaxWidth(int); // 0x30d1d0: rebuilds the lines for the new width
    void AddSegment(LineSegment*,unsigned short=0); //30d570
    typedef std::vector<FloatingSegment> FloatingVector;
    typedef std::vector<SmartPtr<LineElement> > LineVector;
    typedef std::list<SegmentInfo> SegmentList;
    LineVector::iterator FindLineAt(int); // 0x30d040: binary search
    FloatingSegment* FindFloatingSegmentAt(int,int,bool); // 0x30d0c0
    FloatingVector mFloatingItems[2]; //14..33
    LineVector mLineVector; //34..43
    SegmentList mSegmentList; //44..4f
    SmartPtr<LineElement> mSearchLine; // at +0x50, search sentinel
    int mWidth,mHeight,mMinWidth,mCurLineWidth,mMaxWidth; //54,58,5c,60,64
};
// WrappedText (0x30 bytes); the fields at +0x08/+0x0c come first.
class WrappedText : public RefCount {
public:
    GUIString mText; //08
    SmartPtr<Font> mFont; //0c
    SmartPtr<MultiLineElement> mRichText; // at +0x10; destroyed through its virtual destructor
protected:
    struct BreakInfo {
        unsigned short mBreakPos,mNextStartPos,mWidth;
        BreakInfo(int p,int n,int w) : mBreakPos((unsigned short)p),mNextStartPos((unsigned short)n),mWidth((unsigned short)w) {}
    };
    typedef std::list<BreakInfo> BreakList;
    BreakList mBreakList; //14..1f
    int mWidth,mHeight,mMaxWidth; //20,24,28
    bool mWrap,mIsCentered; // at +0x2c, +0x2d; the class is 0x30 bytes
    void Init(); //34a7c0
    void CalcWrap(); //34a9b0
public:
    WrappedText(); //34a7e0
    WrappedText(const GUIString&); // (not identified in the original exe)
    void SetText(const GUIString&); //34acc0
    void SetWrap(bool); //34ad20
    void SetCentered(bool); //34ad40
    void SetFont(Font*); //34ad50
    void SetMaxWidth(int); //34adb0
    void Paint(Graphics&,int,int); //34adf0
    int GetWidth() { return this->mWidth; }
    const GUIString& GetText() { return this->mText; } // inline; returns the text at +0x08
    int GetHeight(); // inline, defined after Font below
};
// MSButtonBase (0x5c bytes): Button (0x44), the wrapped text at +0x44, scalars +0x48..0x58, padding +0x59..0x5b.
// The derived MS controls are declared separately below.
class MSButtonBase : public Button {
public:
    MSButtonBase(); // 0x2fe6e0: allocates the WrappedText (0x30 bytes)
    virtual ~MSButtonBase(); // 0x2fe910, out of line
    virtual void Virtual08(); //2fea10
    virtual void GetDesiredSize(int&,int&); //2fe8c0
    virtual void SetTextHook(const GUIString&); //2fe880 real SetTextHook
    virtual const GUIString& GetText(); //2fe850 real B8
    void SetTransparent(bool); //2fe8a0
    void SetAdjustHeightToFitText(bool); //2fe9f0
    void AdjustHeightToFitText(); //2fe980
    void DrawButtonText(Graphics&,int,int); //2fea40
    void SetFont(Font* f) { this->mText->SetFont(f); }
    void SetTextColor(int c) { this->mTextColor=c; }
    void SetStandardTextColor(StandardColor c) { this->mStandardTextColor=c;this->mTextColor=-1; }
    void SetWrap(bool b) { this->mText->SetWrap(b); }
    void SetHorzAlign(HorzAlign h) { this->mHAlign=(unsigned char)h;this->mText->SetCentered(this->mHAlign==HorzAlign_Center); }
    void SetVertAlign(VertAlign v) { this->mVAlign=(unsigned char)v; }
    enum FocusStyle { FocusStyle_None,FocusStyle_AroundWindow,FocusStyle_AroundText };
    void SetFocusStyle(FocusStyle s) { this->mFocusStyle=(unsigned char)s; }
    void SetHorzTextPad(int l,int r) { this->mLeftTextPad=(unsigned char)l;this->mRightTextPad=(unsigned char)r; }
    void SetVertTextPad(int t,int b) { this->mTopTextPad=(unsigned char)t;this->mBottomTextPad=(unsigned char)b; }
    void IncrementHorzTextPad(int l,int r) { this->mLeftTextPad=this->mLeftTextPad+(unsigned char)l;this->mRightTextPad=this->mRightTextPad+(unsigned char)r; }
    void IncrementVertTextPad(int t,int b) { this->mTopTextPad=this->mTopTextPad+(unsigned char)t;this->mBottomTextPad=this->mBottomTextPad+(unsigned char)b; }
    SmartPtr<WrappedText> mText; // at +0x44
    StandardColor mStandardTextColor; //48
    int mTextColor; //4c
    unsigned char mHAlign,mVAlign,mFocusStyle; //50/51/52
    unsigned char mTopTextPad,mBottomTextPad,mLeftTextPad,mRightTextPad; //53..56
    bool mIsTransparent,mAdjustHeightToFitText; //57/58
};
// SDK utility class: static functions only, never instantiated.
class MSGraphicsUtil {
public:
    static void DrawArrow(Graphics&,Direction,int,int,int,int,bool); //2fdfb0
    static void DrawFrame3(Graphics&,int,int,int,int,bool); //2fdee0
    static void DrawTab(Graphics&,int,int,int,int); //2fe4d0
    static void DrawCheck(Graphics&,int,int,int,int,bool,bool); //2fe250
    static void DrawRadio(Graphics&,int,int,int,int,bool,bool); //2fe3b0
    static void DrawCloseX(Graphics&,int,int,int,int); //2fe5e0
};
// MSButton (0x60 bytes)
// The destructor is out of line.
class MSButton : public MSButtonBase {
public:
    MSButton(); // 0x2fec30
    MSButton(const GUIString&); // 0x2feca0 (ret 4)
    virtual ~MSButton(); // 0x2fed10, out of line
    virtual void Paint(Graphics&); //2fed20
    void SetUseFrame2(bool useFrame2) { this->mUseFrame2=useFrame2; } // inline
protected:
    void Init(); // 0x2febf0: setup helper
public:
    bool mUseFrame2; // at +0x5c (written at 0x2fec1e/0x2fed75); the class is 0x60 bytes
};
// MS control (0x5c bytes); constructor and destructor out of line.
class MSCheckbox : public MSButtonBase {
public:
    MSCheckbox(); // 0x2ff3e0; no members beyond the base's 0x5c bytes
protected:
    void Init(); // 0x2ff3b0
public:
    virtual ~MSCheckbox(); // 0x2ff420, out of line
    virtual void Paint(Graphics&); // 0x2ff470, vtable +0x18
    typedef void (*CheckboxPainterFunc)(Graphics&,MSCheckbox*);
    static CheckboxPainterFunc CheckboxPainter;
    static int mCheckboxWidth,mCheckboxHeight;
    static void DefaultCheckboxPainterFunc(Graphics&,MSCheckbox*); // 0x2ff430
};
// MSRadioButton (0x5c bytes, no members of its own); constructor out of line
class MSRadioButton : public MSButtonBase {
public:
protected:
    void Init(); // inlined into the constructor at 0x2ff530
public:
    MSRadioButton(); // 0x2ff530
    virtual ~MSRadioButton(); // 0x2ff5b0, out of line
    virtual void Paint(Graphics&); // 0x2ff600, vtable +0x18
    typedef void (*RadioPainterFunc)(Graphics&,MSRadioButton*);
    static RadioPainterFunc RadioPainter;
    static int mRadioWidth,mRadioHeight;
    static void DefaultRadioPainterFunc(Graphics&,MSRadioButton*); // 0x2ff5c0
};

// MSArrowButton (0x48 bytes): Button (0x44) plus the direction at +0x44; constructor and destructor out of line
class MSArrowButton : public Button {
public:
    MSArrowButton(Direction=Direction_Up); // 0x2ff0c0 (ret 4)
    virtual ~MSArrowButton(); // out of line
    virtual void Paint(Graphics&); // vtable +0x18
    void SetDirection(Direction d) { this->mDirection=d; }
    Direction mDirection; //44
};
// No members of its own (0x48 bytes); constructor 0x3c9590, Paint 0x3c9620,
// VT4541f4, RTTI MaxSplitterButton>MSArrowButton>Button>Component>RefCount.
// The destructor is implicit; 0x3c9610 jumps to MSArrowButton's at 0x2ff140.
class MaxSplitterButton : public MSArrowButton {
public:
    MaxSplitterButton(Direction=Direction_Up);
    virtual void Paint(Graphics&);
};
// MSTabButton (0x60 bytes): MSButtonBase (0x5c) plus mLowerAmount at +0x5c; constructor and destructor out of line
class MSTabButton : public MSButtonBase {
public:
    MSTabButton(const GUIString& = "",bool=true,int=3); // 0x2fee20 (ret 12)

    virtual void Paint(Graphics&); // vtable +0x18
    virtual void GetDesiredSize(int&,int&); // vtable +0x0c
    void SetLowerAmount(int n) { this->mLowerAmount=n; }
    int mLowerAmount; //5c
};
// MS3DFrame (0x38 bytes): Component (0x30) plus a bool at +0x30 and an int at +0x34; constructor and destructor out of line
class MS3DFrame : public Component {
public:
    MS3DFrame(int=3,bool=false); // 0x300780 (ret 8)
    virtual void Paint(Graphics&); // 0x3007b0, vtable +0x18
    void SetIsUp(bool v) { this->mIsUp=v; }
    void SetFrameType(int t) { this->mType=t; }
    bool mIsUp; //30
    int mType; //34
};

class Label : public Component {
public:
    Label(); // 0x2f9d80; the class is 0x68 bytes (allocated at 0x300908)
    virtual ~Label(); // 0x2f9f50: releases the background and the wrapped text
    virtual void Virtual08(); //2fa020 SizeChanged
    virtual void GetDesiredSize(int&,int&); //2fa140 GetDesiredSize
    virtual void Paint(Graphics&); //2fa1a0
    void SetMaxWidth(); //2f9f20
    void AdjustHeightToFitText(); //2fa090
    void SetText(const GUIString& inText, bool inFlag = true); // 0x2fa060
    void SetHorzAlign(HorzAlign); // 0x2fa040
    void SetWrap(bool); // 0x2fa120
    void SetAdjustHeightToFitText(bool); // 0x2fa100
    void SetVertPad(int inTop, int inBottom) { this->mTopPad=inTop; this->mBottomPad=inBottom; }
    void SetFont(Font* theFont) { this->mWrappedText->SetFont(theFont); }
    void SetTextColor(int theColor) { this->mColor=theColor; }
    void SetBackground(const Background& theBackground) { this->mBackground=theBackground; }
    void SetVertAlign(VertAlign theAlign) { this->mAlign3C=theAlign; }
    SmartPtr<WrappedText> mWrappedText;      // +30
    int mColor;                            // +34
    int mAlign38;
    int mAlign3C;
    int mLeftPad;                          // +40
    int mRightPad;                         // +44
    int mTopPad;                           // +48
    int mBottomPad;                        // +4c
    bool mAdjustHeightToFitText; // at +0x50; +0x51..0x53 padding
    Background mBackground; // at +0x54..0x67
};




// SelectionColor (0x0c bytes) with read-only color helpers.
enum ForegroundSelectionMode {
    ForegroundSelectionMode_UseOriginalColor,
    ForegroundSelectionMode_InvertOriginalColor,
    ForegroundSelectionMode_UseForegroundColor
};
class SelectionColor {
public:
    SelectionColor(); //3494a0, back80000007/fore80000008/mode2
    SelectionColor(DWORD,ForegroundSelectionMode = ForegroundSelectionMode_InvertOriginalColor); //3494c0, foreFFFFFF
    SelectionColor(DWORD,DWORD); //3494e0, mode2
    unsigned long GetBackgroundColor(Graphics&) const; //349500
    unsigned long GetForegroundColor(Graphics&, unsigned long) const; //349510
    unsigned long mBackgroundColor;
    unsigned long mForegroundColor;
    ForegroundSelectionMode mMode;
};

// InputBox (0xec bytes): Background values at +0xc4 and +0xd8
// Constructor 0x2f7fd0 and destructor 0x2f8270 are out of line.
class InputBox : public Component {
public:
    void SetFont(Font* f) { this->mFont=f; }
    typedef bool (*InputFilter)(InputBox*,GUIString&);
    static bool NumericFilter(InputBox*,GUIString&); //2f7f70 cdecl
    InputBox(); // 0x2f7fd0
    // Implicit virtual destructor (0x2f8270)
    const GUIString& GetText() const; // 0x2f9960
    void SetText(const GUIString&,bool); //2f9990
    void Clear(bool); //2f99c0
    void AddInputHistory(const GUIString&,bool); // 0x2f9140
    void SetInputHistory(const GUIString&,bool=false); //2f92f0
    void SetAllowSelection(bool); // 0x2f84b0
    void SelectRange(int,int); // 0x2f8aa0 (SDK name SetSel)
    void AdjustTextRect(); //2f8400
    void SetIcon(Image*); //2f8450
    void SetCharWidth(int); //2f8560
    void CheckSel(int,int,bool); //2f8600
    void SetBackground(const Background& b) { this->mBackground=b; }
    void SetDisabledBackground(const Background& b) { this->mDisabledBackground=b; }
    void SetCursorBlinkRate(int,int=0); //2f87d0
    void AdjustPosition(); //2f8a20
    void CopySelection(); // 0x2f8b10
    void SetSelText(const GUIString&,bool=true); //2f8e10
    int MoveCursor(bool,bool,const GUIString&,int); //2f90c0
    void SetPasswordChar(char); //2f9970
    void SetTextPrv(const GUIString&,bool=true); // 0x2f99e0
    virtual void Virtual08(); //2f8510
    virtual void GetDesiredSize(int&,int&); //2f8530
    virtual void GotFocus();  //2f8590
    virtual void LostFocus(); //2f85d0
    virtual void MouseDown(int,int,int); //2f8750
    virtual void MouseUp(int,int,int);   //2f8790
    virtual void MouseExit(); //2f87a0
    virtual void MouseDrag(int,int); //2f87b0
    virtual bool TimerEvent(int); //2f8800
    virtual bool KeyChar(int);   // 0x2f8c30
    virtual bool KeyDown(int);   //2f93b0
    virtual void Paint(Graphics&); // 0x2f9ac0
    GUIString mText; //30 display
    GUIString mPasswordText; // at +0x34, the real text when this is a password box
    char mPassword; // at +0x38, the password character
    SmartPtr<Image> mIcon; //3c
    InputFilter mInputFilter; // at +0x40, input transform
    std::deque<GUIString> mInputHistory; // at +0x44..0x57, Dinkum deque (allocator, map, map size, offset, size; 0x14 bytes)
    int mInputHistoryPos; //58
    int mMaxInputHistory; //5c
    bool mHaveTraversedHistory; //60
    bool mBackwardsSel; //61
    int mSelStartChar; //64
    int mSelEndChar; //68
    int mHorzOffset; //6c
    bool mSelecting; //70
    int mLeftPad,mTopPad,mRightPad,mBottomPad; //74,78,7c,80
    int mCursorDelay,mCursorOnTime,mCursorOffTime; //84,88,8c
    bool mCursorOn,mUnderlineCursor,mAllowSelection,mSelectOnFocus,mUseDisabledColors; //90..94
    int mMaxChars; //98
    SmartPtr<Font> mFont; //9c
    unsigned long mTextColor; // at +0xa0
    int mDisabledTextColor; //a4
    SelectionColor mSelColor; // at +0xa8..0xb3
    WONRectangle mTextRect; // at +0xb4..0xc3
    Background mBackground; // at +0xc4..0xd7
    Background mDisabledBackground; // at +0xd8..0xeb
};

class StretchImage;
// Plain record (no vtable, 0x14 bytes): four border ints then the image at +0x10.
// The implicit destructor releases the image; the copy constructor is out of line (declared below).
class WONFrame {
public:
    int mLeft,mTop,mRight,mBottom;
    SmartPtr<Image> mImage;
    WONFrame(Image* =NULL,int=0); // 0x3175f0 (ret 8): one border width on all sides
    WONFrame(Image*,int,int,int,int); // 0x317690 (ret 0x14)
    WONFrame(StretchImage*); // 0x317550 (ret 4)
    WONFrame(const WONFrame&); // copies the image (not identified in the original exe)
    WONFrame& operator=(const WONFrame&); // 0x3177a0
    void Copy(const WONFrame&); // 0x3177c0: duplicates the image instead of sharing it
    void SetImage(Image*); // shares the body at 0x349ab0
    void SetImage(Image*,int,int,int,int); // 0x317730: sets the image and the borders
    void Paint(Graphics&,Component*); //3178c0
    void Paint(Graphics&,Component*,int,int,int,int); //317910
};
// WONFrameComponent (0x44 bytes): Component (0x30) plus a WONFrame (0x14)
class WONFrameComponent : public Component {
public:
    WONFrameComponent(Image* =NULL,int=0,int=0,int=0,int=0); // 0x31a5b0 (ret 0x14)
    WONFrameComponent(const WONFrame&); // 0x31a740 (ret 4)
    const WONFrame& GetFrame(void) { return this->mFrame; } // inline
    // Implicit virtual destructor: releases the frame's image, then Component's cleanup
    virtual void Paint(Graphics& inGraphics); // override
    virtual void SetFrameImageAndBorders(Image*, int, int, int, int);
    virtual void SetFrameImage(Image*);
    virtual void SetFrameBorders(int, int, int, int);
    virtual void SetFrameLeft(int);
    virtual void SetFrameTop(int);
    virtual void SetFrameRight(int);
    virtual void SetFrameBottom(int);
    virtual int GetFrameLeft();
    virtual int GetFrameTop();
    virtual int GetFrameRight();
    virtual int GetFrameBottom();
    WONFrame mFrame; // at +0x30..0x43 (written at 0x31a5d6/0x31a6ef); the frame's image at +0x40
};
// ImageComponent (0x38 bytes, 45 vtable slots): image at +0x30, tile flag +0x34, stretch flag +0x35.
// Constructor and destructor are out of line.
class ImageComponent : public Component {
public:
    ImageComponent(Image* = NULL,bool = false); // 0x306b10
    // Implicit virtual destructor: releases the image at +0x30, then Component's cleanup
    virtual void GetDesiredSize(int&, int&);  // 306cf0
    virtual void Paint(Graphics&);          // 306d30
    virtual void SetImageAndTile(Image*, bool);  // 306c80, ret8
    virtual void SetImage(Image*);           // 306c10, ret4
    void SetTile(bool doTile) { this->mTile = doTile; }
    void SetStretch(bool doStretch) { this->mStretch = doStretch; }
    bool GetTile() { return this->mTile; }
    bool GetStretch() { return this->mStretch; }
    Image* GetImage() { return this->mImage.get(); }
    SmartPtr<Image> mImage; // at +0x30
    bool mTile,mStretch; // at +0x34, +0x35 (set at 0x394da6/0x394dd8)
};

// a drop-down list (vtable 0x84d638; its list at +0x90)
// TabBar (0xa0 bytes) and TabCtrl (0xd8 bytes)
class TabCtrl;
class LayoutPoint;
class TabBar : public Container {
public:
    TabBar(TabCtrl*);
    // Implicit destructor (0x3081f0): no own vptr store; the vector holds the button references
    virtual void HandleComponentEvent(ComponentEvent*);
    void PositionTab(Button*);
    void AddTab(Button*);
    void RemoveTab(int); // removes the tab, renumbers and reparents the rest
    void SetCurTab(int,bool=true);
    void SetCurTabByName(const GUIString&,bool=true);
    void EnableTab(int,bool=true);
    bool Disabled(int);
    int GetCurTabId(); // getter
    std::vector<SmartPtr<Button> > mTabVector; //84, data88/end8c/cap90
    Button* mCurTab; //94, borrowed
    TabCtrl* mTabCtrl; //98, borrowed
    int mOverlap; // at +0x9c; the class is 0xa0 bytes
};
class TabCtrl : public Container {
public:
    TabCtrl(); // 0x308670
    // Implicit destructor (0x301340, shared with another class); no own vptr store
    virtual void Virtual08(); // SizeChanged308f50
    virtual void GetDesiredSize(int&,int&); // GetDesiredSize3089e0
    virtual bool KeyDown(int);   // KeyDown309240
    virtual void AddedToParent(); // AddedToParent308cb0
    virtual void HandleComponentEvent(ComponentEvent*); //3090e0
    virtual bool TabFocus(bool,bool);  //TakeTabFocus309140
    void CopyAttributes(TabCtrl*);
    void SetTabBarOnRight(bool);
    void SetArrowsOnLeft(bool);
    void SetTabArrows(Button*,Button*);
    void NotifyTabChange(bool);
    void KeepTabButtonInView();
    void PositionTabContainer(Container*);
    void PositionArrows();
    void HorzPositionTabBar();
    void VertPositionTabBar();
    void CalcContainerLayout();
    bool NeedScrollButtons();
    void AdjustTabBarScroll(int=0);
    void SetCurTab(int,bool);
    void SetCurTabByName(const GUIString&,bool);
    int GetTabIdForContainer(Container*); // GetTabIdForContainer55
    void EnableTab(int,bool);
    void AddTab(Button*,Container*);
    void RemoveTab(int); // removes the tab and reselects
    SmartPtr<TabBar> mBottomTabBar; //84
    SmartPtr<Container> mTopTabBar; //88
    SmartPtr<Container> mBottomTabBarContainer; //8c
    SmartPtr<Container> mTopTabBarContainer; //90
    SmartPtr<LayoutPoint> mContainerLayout; //94
    int mLeftPad,mRightPad,mTopPad,mBottomPad; //98..a4
    int mBottomTabLeftOffset,mBottomTabTopOffset; //a8/ac
    int mTabBarHeight,mTabBarOverlap; //b0/b4
    SmartPtr<Button> mTabBarLeft,mTabBarRight; //b8/bc
    bool mArrowsOnLeft,mTabBarOnRight,mTabBarOnBottom,mUserHasInteracted; //c0..c3
    // The constructor leaves mUserHasInteracted uninitialized.
    std::vector<Container*> mTabVector; //c4, nonowning values, datac8/endcc/capd0
    Container* mCurTab; // at +0xd4; the class is 0xd8 bytes
};

// Scrollbar (0xb8 bytes); constructor 0x305e10, destructor 0x305eb0
class Scrollbar : public Container {
public:
    Scrollbar(bool=true); // 0x305e10 (ret 4)
    virtual ~Scrollbar(); // 0x305eb0
    virtual void Virtual08(); //306380
    virtual void MouseDown(int,int,int); //3064c0
    virtual void MouseDrag(int,int); //306510
    virtual void AddedToParent(); //305fe0
    virtual void HandleComponentEvent(ComponentEvent*); //306690
    virtual void SetEnabled(bool); // 0x3067e0, vtable +0x90
    void CopyAttributes(Scrollbar*); // 0x305fa0
    void AddControls(); //306000
    void LayoutThumb(bool); //3061d0
    void S_SetPos(Component*,int,int); // 0x306100
    void S_SetSize(Component*,int,int); // 0x306140
    void S_SetPosSize(Component*,int,int,int,int); // 0x306180
    int S_Left(Component* c) { return this->mVertical?c->Left():c->Top(); }
    int S_Right(Component* c) { return this->mVertical?c->Left()+c->Width():c->Top()+c->Height(); }
    int S_Top(Component* c) { return this->mVertical?c->Top():c->Left(); }
    int S_Bottom(Component* c) { return this->mVertical?c->Top()+c->Height():c->Left()+c->Width(); }
    int S_Height(Component* c) { return this->mVertical?c->Height():c->Width(); }
    int S_Width(Component* c) { return this->mVertical?c->Width():c->Height(); }
    SmartPtr<Button> mUpArrow,mDownArrow,mScrollRect; //84/88/8c
    SmartPtr<Component> mThumb; //90
    bool mResizeThumb,mVertical; //94/95
    unsigned char mMinThumbSize; //96
    int mPosition,mPageSize,mTotalSize; //98/9c/a0
    int mLineUpScrollSize,mLineDownScrollSize,mPageScrollSize; //a4/a8/ac
    int mDragX,mDragY; //b0/b4
};

// Scroller (0x88 bytes): owns the scroll area at +0x84
class Scroller : public Container {
public:
    Scroller(); // 0x305540
    // Implicit virtual destructor (0x3001e0)
    virtual void MouseDown(int,int,int); //305670
    virtual bool MouseWheel(int); //3056c0
    virtual void NotifyScrollChange(bool) {} // empty inline (shared body at 0x30a800)
    ScrollArea* GetScrollArea() { return this->mScrollArea.get(); }
    void SetScrollArea(ScrollArea*); // 0x305580: sets the scroll area and its back link
    void SetOffsets(int,int); //3055e0 horizontal thenvertical
    void SetVertOffset(int); //305630
    void SetHorzOffset(int); //305650
    SmartPtr<ScrollArea> mScrollArea; // at +0x84; released by the implicit destructor 0x3001e0
};
enum ScrollbarCondition { ScrollbarCondition_Disable=0,ScrollbarCondition_Enable=1,ScrollbarCondition_Conditional=2 };
// ScrollbarScroller (0x9c bytes)
class ScrollbarScroller : public Scroller {
public:
    ScrollbarScroller(); // 0x305700
    // Implicit virtual destructor (0x300060)
    virtual void AddedToParent(); //305840 AddedToParent
    virtual void HandleComponentEvent(ComponentEvent*); //305a00
    virtual void NotifyScrollChange(bool); // 0x305a50, vtable +0xcc
    void SetScrollbar(Scrollbar*); //305760
    void AddControls(); //305860
    Scrollbar* GetScrollbar(bool vertical) { return vertical?this->mVertScrollbar.get():this->mHorzScrollbar.get(); } // inline; +0x88/+0x8c
    bool EnableScrollbar(Scrollbar*,ScrollbarCondition); //3058d0
    void SetScrollbarConditions(ScrollbarCondition,ScrollbarCondition); // 0x305990
    SmartPtr<Scrollbar> mHorzScrollbar,mVertScrollbar; // at +0x88/+0x8c
    ScrollbarCondition mVertCondition,mHorzCondition; //90/94
    bool mSettingScrollbarConditions; // at +0x98; +0x99..0x9b padding
};
enum ScrollButtonCondition {
    ScrollButtonCondition_Disable=0, ScrollButtonCondition_Enable=1,
    ScrollButtonCondition_Conditional=2, ScrollButtonCondition_PartialConditional=3
};
// Not identified in the original exe
class ButtonScroller : public Scroller {
protected:
    SmartPtr<Button> mLeftButton,mRightButton,mUpButton,mDownButton;
    ScrollButtonCondition mVertCondition,mHorzCondition;
    bool mSettingScrollButtonConditions;
    bool EnableScrollButtons(Button*,Button*,ScrollButtonCondition);
public:
    virtual void Virtual08();
    virtual void NotifyScrollChange(bool);
    virtual void HandleComponentEvent(ComponentEvent*);
    virtual void AddedToParent();
    ButtonScroller();
    void SetButton(Button*,Direction);
    void AddControls();
    void SetScrollButtonConditions(ScrollButtonCondition,ScrollButtonCondition);
};
// ComboBox (0xa4 bytes)
class Scroller;
class ComboBox : public Container {
public:
    ComboBox();
    virtual ~ComboBox(); // own-vptr normal2f3a50, real listener-detach body
    virtual void Virtual08(); //2f4010 SizeChanged
    virtual void MouseDown(int,int,int); //2f4600 MouseDown
    virtual bool MouseWheel(int); //2f4650 MouseWheel
    virtual void GotFocus();  //2f4090 GotFocus
    virtual void LostFocus(); //2f40b0 inherited LostFocus forwarding0
    virtual bool KeyDown(int);   //2f4680 KeyDown
    virtual void AddedToParent(); //2f3ef0 AddedToParent
    virtual void HandleComponentEvent(ComponentEvent*); //2f44e0
    virtual void SetEnabled(bool); //2f4800
    ListItem* AddString(const GUIString&,int=-1); // 0x2f3b70
    void InsertItem(TextListItem*,int=-1);
    void DeleteItem(int);
    void UpdateItem(TextListItem*);
    void Clear(bool=true);
    int GetSelItem(); // 0x2f3d40
    void SetSelItem(int,bool=false);
    void SetListBox(Scroller*);
    void AddControls();
    void CalcListBoxSize();
    void SetListBoxDimensions(int,int);
    void SetListBoxWidth(int);
    void SetListBoxHeight(int);
    void SetInputBoxToListSelection(bool=false,bool=true);
    void SetText(const GUIString&,bool=false);
    void SetTextAndSel(const GUIString&,bool);
    void ShowList();
    void SetNoType(bool);
    SmartPtr<InputBox> mEdit; //84
    SmartPtr<Button> mButton; //88
    SmartPtr<Scroller> mListBox; //8c
    SmartPtr<ListArea> mListArea; //90
    bool mListBoxVisible; //94
    bool mNoType; //95
    bool mShowButtonWhenDisabled; // at +0x96; +0x97 padding
    int mListBoxWidth; //98
    int mListBoxHeight; //9c
    GUIString mUndoStr; // at +0xa0; the class is 0xa4 bytes
};

// one local network address (a std::list element, 0x14 bytes; provisional names)
struct LobbyNetAddress {
    LobbyNetAddress() {}
    LobbyNetAddress(const std::string& theName,long theHost) : mName(theName),mHandle(theHost) {}
    std::string mName;                      // +0x00
    long mHandle;                           // +0x10, numeric IPv4 adapter address
};
// ConfigObject (0x54 bytes) is declared in WONConfigObject.h via Resource.
// 0x78-byte base, constructor and destructor out of line; the private 0x24-byte tail is opaque.
class ComponentConfig : public ConfigObject {
public:
    static ComponentConfig* CfgFactory(); // 0x39aab0
    static Component* CompFactory(); // 0x396960
    bool ReadConfig_Unsafe(Component*,const char*,ResourceConfigTable*,ConfigParser*); // 0x3828c0 (ret 0x10)
    void ReadConfig(Component*,const char*,ResourceConfigTable* = NULL,ConfigParser* = NULL);
    Component* GetChildComponent(const std::string&);
    ComponentConfig* GetChildConfig(const std::string&);
    void InitComponents(); //382cc0 recursive guarded component initialization
    Direction ReadDirection(); //3856f0
    HorzAlign ReadHorzAlign(); //385950
    VertAlign ReadVertAlign(); //385b10
    static void SetResourceFolder(const std::string&); // 0x387ca0
    static std::string mResourceFolder; // 0x566b18
    SmartPtr<Sound> ReadSound(); // 0x3855f0 (the caller owns the result)
    void EnsureComma(); //382e00; SkipComma is382dd0
    int GetIntValue(); // 0x382f40
    int ParseExpression(int); //383560
    int ReadChar(); //383930
    GUIString ReadFlag(bool* = NULL); //383a90
    GUIString ReadRawString(bool); //383b00
    SmartPtr<Font> ReadFont(); // 0x384480
    SmartPtr<Image> ReadImage(bool); // 0x384fa0 (hidden result)
    SelectionColor ReadSelectionColor(); // 0x385cd0
    ComponentConfig* GetComponentConfig(const GUIString&,bool); // 0x386380
    ComponentConfig* ReadComponentConfig(); // 0x3868d0: config lookup
    bool HandleMapComponent(); // 0x386960 (0x386950 is a separate 9-byte wrapper)
    bool HandleModifyGroup(ConfigParser&); // 0x386d80
    bool HandleResourceGroup(ConfigParser&); // 0x386f20
    bool HandleModify(); // 0x3870a0
    bool BaseHandleInstruction(ConfigParser&,const std::string&,StringParser&); // 0x387370 (forwarder)
    void SetDesiredSizeIfNecessary(); // 0x3860b0
    SmartPtr<Cursor> ReadCursor(); // 0x385380 (hidden result)
    int GetIntResource(const std::string&); //384210
    static SmartPtr<ResourceConfigTable> mResourceConfigTable; // 0x566b28 (declaration only)
    SmartPtr<RawImage> ReadRawImage(); // 0x385050
    SmartPtr<Image> GetImageFromStr(const std::string&,bool); //384890
    SmartPtr<RawImage> GetRawImageFromStr(const std::string&); // 0x384f10
    StandardCursor ReadStandardCursor(); //385120
    void AddChildToConfigMap(const GUIString&,ComponentConfig*); // 0x386c30; the parent keeps the child
    GUIString GetStringResource(const std::string&); //383bd0
    Image* GetImageResource(const std::string&); //383d60 borrowed
    Font* GetFontResource(const std::string&); //383e50 borrowed
    SmartPtr<Sound> GetSoundResource(const std::string&); //383f40 owned result
    DWORD GetColorResource(const std::string&); //384050
    void GetBackgroundResource(const std::string&,Background&); //384140
    Background ReadBackground(); // 0x385ea0, returns a Background (0x14 bytes) by value
    int ReadInt(); // 0x3837e0, no arguments
    Component* ReadComponent(); // 0x386950
    bool EndOfString(); //382db0
    void SkipComma(); //382e00
    static std::string GetResourceFile(const std::string&); //387e50

    ComponentConfig();                     // 0x3825b0
    virtual ~ComponentConfig();            // 0x382690, own VT451f54
    virtual bool HandleInstruction(ConfigParser*, const std::string&);
    virtual bool HandleValueInstruction(ConfigParser*, const std::string&, StringParser&);
    virtual bool PreParse(ConfigParser*);
    virtual bool PostParse(ConfigParser*);
    virtual bool Virtual24(const std::string&);
    virtual bool GetPredefinedComponent(const std::string&, ComponentConfig*&, Component*&);
    virtual bool GetNamedValue(const std::string&, int&);
    virtual bool CallNamedFunction(const std::string&, const std::vector<int>&, int&);
    virtual void SetComponent(Component*);
    GUIString ReadString(bool);             // ReadString
    int ReadColor();                       // ReadColor
    Component* Fn386860(const std::string& inName); // 0x386860, borrowed component lookup
    bool ReadBool();                       // 0x3837f0, boolean parser
    SmartPtr<Component> mComponent; // at +0x54 (set at 0x3825dc)
    ConfigParser* mConfigParser; //58, borrowed
    StringParser* mStringParser; // at +0x5c
    GUIString mConfigError; // at +0x60 (set at 0x3825ff)
    bool mHasSetWidth,mHasSetHeight,mHaveInitedComponents; // at +0x64/+0x65/+0x66; +0x67 padding
    std::map<std::string,SmartPtr<ComponentConfig>,StringLessNoCase> mConfigMap; // at +0x68..0x73
    SmartPtr<ResourceConfig> mCurResourceConfig; // at +0x74
};

// FontConfig is declared after FontDescriptor below.
class FontConfig;

// Exception record (0x1c bytes); the implicit copy and destructor are the original's helpers.
class ConfigObjectException : public std::exception {
public:
    ConfigObjectException(const std::string& inText, bool inFlag = true)
        : mText() { this->mText = inText; this->mFlag = inFlag; }
    std::string mText;                     // +08
    bool mFlag;                            // +18
};

// Looks up a named component and checks its type
template<class T>
T SafeConfigGetComponent(Component *theComponent, const char *theName)
{
	T aComp = dynamic_cast<T>(theComponent);
	if(aComp==NULL)
		throw ConfigObjectException(std::string("Invalid ") + theName);

	return aComp;
}


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

template <class T> void SafeGetComponent(ComponentConfig* inConfig, SmartPtr<T>& outPtr,
    const char* inName, int inControlId, const char* inFile, int inLine)
{
    SafeGetComponent(inConfig, outPtr, inName, inFile, inLine);
    outPtr->mControlId = inControlId;
}

class ButtonConfig : public ComponentConfig {
public:
    void HandleButtonFlags(); //381880
    void HandleFitText(); //381d00
    ButtonConfig();                         // 0x381860; the class is 0x7c bytes
    virtual bool Virtual24(const std::string&); //381e80
    virtual void SetComponent(Component*);  //38f8d0 ICF target
    Button* mButton;                        // +78, borrowed
};

class ContainerConfig : public ComponentConfig {
public:
    static Component* LayoutPointFactory(); // 0x39ab10
    static ComponentConfig* CfgFactory(); // 0x295842
    static Component* CompFactory(); // 0x365120
    bool HandleAdd(); // 0x388840
    bool HandleAddLayoutPoint(); // 0x388870
    bool HandleLayout(bool chain=false,bool repeat=false); // 0x3888b0
    bool HandleSameSizeLayout(bool,bool); // 0x388e20
    bool HandleCenterLayout(bool,bool); // 0x388f30
    bool HandleClearRightLayout(); // 0x389080
    bool HandleSpaceEvenlyLayout(bool); // 0x389280
    bool HandleNoOverlapLayout(); // 0x3894b0
    bool HandleSizeAtLeastLayout(); // 0x389860
    bool HandleNewLayoutPoint(); // 0x389a70
    ContainerConfig();                      // 0x387f90; the class is 0x98 bytes
    // The implicit destructor (0x38ef70) jumps to ComponentConfig's.
    virtual bool HandleInstruction(ConfigParser*, const std::string&); //3885d0
    virtual bool Virtual24(const std::string&); //389c80
    virtual void SetComponent(Component*);  //38f8d0
    Container* mContainer;                  // +78, borrowed
    Component* mLayoutRef;                  // +7c, borrowed
    int mLayoutInstruction;                // +80
    int mLayoutParams[4];                  // +84..90
    int mCurLayoutId;                      // +94
};

class DialogConfig : public ContainerConfig {
public:
    static ComponentConfig* CfgFactory(); // 0x29585c
    static Component* CompFactory(); // 0x39ac50
    virtual bool Virtual24(const std::string&); //389f80
    virtual void SetComponent(Component*);  //38f960
    Dialog* mDialog;                        // +98, borrowed
};

class LobbyConfig : public ConfigObject {
public:
    LobbyConfig();                          // 0x358090
    typedef LobbyConfig* (*ConfigAllocator)();
    static ConfigAllocator mAllocator; // class static at 0x4d954c
    static LobbyConfig* DefaultAllocator(); // 0x357f30
    static std::string mDefaultFileName; // 0x566310
    static void SetAllocator(ConfigAllocator allocator) { LobbyConfig::mAllocator=allocator; } // inline setter
    virtual bool HandleValueInstruction(ConfigParser* inParser, const std::string& inKey, StringParser& inValue);
    virtual bool PostParse(ConfigParser* inParser);

    std::list<std::string> mDirServers;     // +0x54; ConfigListValue RTTI, node size 0x18
    std::list<std::string> mRoomDirServers;  // +0x60
    std::list<std::string> mAuthServers;    // +0x6c
    std::string mProductName;               // +0x78
    const std::string& GetProductName() const { return this->mProductName; }
    std::wstring mProductDirectory;         // +0x88
    std::string mVersion;                   // +0x98
    std::string mLanVersion;                // +0xa8
    std::string mLanguage;                  // +0xb8
    std::string mResourceDirectory;         // +0xc8
    bool mCRCCheck;                         // +0xd8
    bool mCDKeyCheck;                       // +0xd9
    bool mPublic;                           // +0xda
    unsigned short mLanBroadcastPort;      // +0xdc
    bool mUseLocalIP;                       // +0xde
    bool GetNoPingInProgress() const { return this->mNoPingInProgress; }
    bool mNoPingInProgress;                 // +0xdf
    unsigned short mLobbyPort;             // +0xe0
    bool mCheckForInternet;                 // +0xe2
    unsigned short mPingCeiling;           // +0xe4
    bool mAllowDuplicateNames;              // +0xe6
    bool mAppendNumberToDefaultLanName;      // +0xe7
    bool mAllowJoinGameWithoutPing;          // +0xe8
    std::map<std::string, std::string, StringLessNoCase> mDefines;           // +0xec (node value 0x20; pair dtor 0x296a48)
};

// 0x358710: the lobby's configuration object (an EELobbyConfig: EELobbyInit.cpp's factory)
LobbyConfig* GetLobbyConfig();

// one item of a list (0x18 bytes: TextListItem's members start at +0x18; vtable 0x84dba4, 14 slots; the inline
// bodies are the COMDATs EELobbyConnectDialog.cpp keeps, 0x290bb4..0x290bc4)
class ListItem : public RefCount {
protected:
    virtual ~ListItem() {} // protected inline destructor
public:
    ListItem();                             // 0x2fa280
    virtual void Paint(Graphics&, ListArea*);
    virtual void CalcDimensions(ListArea*);
    virtual void SetViewport(ListArea*, Container*);
    virtual int Compare(const ListItem*, ListArea*) const;
    virtual bool CanSelect(ListArea*, int, int) { return this->mSelectable; }
    virtual void SetPos(int, int inTop) { this->mTop = inTop; }
    virtual bool Contains(int, int inY) { return inY >= this->mTop && inY < this->mTop + this->mHeight; }
    virtual void MouseEnter(ListArea*, int, int);
    virtual void MouseExit(ListArea*);
    virtual void MouseMove(ListArea*, int, int);
    virtual void MouseDrag(ListArea*, int, int);
    virtual void MouseDown(ListArea*, int, int, int);
    virtual void MouseUp(ListArea*, int, int, int);
    bool mSelected;                         // +08
    bool mSelectable;                       // +09
    int mTop;                               // +0c
    int mWidth;                             // +10
    int mHeight;                            // +14
};

// the root of the lobby's components (runs the popups and modal dialogs; RTTI WONAPI::RootContainer; its base
// Container)
// RootContainer (0xc8 bytes): constructor and destructor out of line.
// Keyboard callback interface (RefCount, 8 bytes); RootContainer calls vtable +0x04 (key, type) at 0x3049bd.
class KeyboardListenerBase : public RefCount {
public:
    virtual void KeyEvent(int,int)=0;
};
// BackgroundGrabber (0x38 bytes); RootContainer keeps a pointer to it.
// 0x33613f/0x336148 store the 64-bit order at +0x30/+0x34; +0x2c..0x2f is padding.
class BackgroundGrabber {
public:
    SmartPtr<NativeImage> mBGImage; //00
    int mImageX,mImageY; //04/08
    WONRectangle mClipRect; //0c..1b
    bool mNeedPutBG,mHaveBG; //1c/1d
    int mTranslucentColor,mTranslucentLevel; //20/24
    SmartPtr<Window> mDebugWindow; //28
    unsigned __int64 mImageOrder; // at +0x30, 64-bit counter value
    static unsigned __int64 mImageOrderCounter; // 0x5662e0/0x5662e4
    BackgroundGrabber(); // 0x336000
    void PrePaint(Component*,Graphics&); //336070
    void Paint(Component*,Graphics&); //3360f0
    void SetTranslucentColor(int,int); //3360d0
    void Clear(); // 0x336060, clears both flags
    void SizeChanged(Component*); // 0x2fa500 (empty, ret 4)
    void Invalidate(Component*); // 0x336680: sets need-put-BG when the background is grabbed
    bool NeedDrawBG() const { return this->mHaveBG && this->mNeedPutBG; }
};
class RootContainer : public Container {
public:
    RootContainer(Window*); // 0x3040d0
    virtual void Virtual08(); //304730 SizeChanged
    virtual void PrePaint(Graphics&);  //304760 PrePaint
    virtual void Paint(Graphics&); //304900
    virtual void MouseEnter(int,int); //304c40 MouseEnter
    virtual void MouseExit(); //304c60 MouseExit
    virtual void MouseDown(int,int,int); //304bd0 MouseDown
    virtual void MouseUp(int,int,int);   //304c90 MouseUp
    virtual void MouseMove(int,int); //304a30 MouseMove
    virtual void MouseDrag(int,int); //304ac0 MouseDrag
    virtual bool KeyChar(int);   //304a00 KeyChar
    virtual bool KeyDown(int);   //3049a0 KeyDown
    virtual bool KeyUp(int);     //3049d0 KeyUp
    virtual void InvalidateRect(WONRectangle&); //304980 InvalidateRect
    virtual void SetCursor(Cursor*); //304cf0 SetCursor
    virtual Component* FindChildAt(int,int); //304710 FindChildAt
    virtual bool ChildInvalidated(Component*); //304970 InvalidateUp
    virtual void RemoveChild(Component*); //304cc0 RemoveChild
    void AddBG(BackgroundGrabber*); //304510

    struct BGStruct {
        int mImageX,mImageY;
        WONRectangle mClipRect;
        SmartPtr<NativeImage> mImage;
        BGStruct() : mImageX(0),mImageY(0) {}
        BGStruct(int x,int y,NativeImage* image,const WONRectangle& clip)
          : mImageX(x),mImageY(y),mClipRect(clip),mImage(image) {}
    }; // 0x1c bytes; map node 0x30
    WONRectangle mInvalidRect; //84..93
    Component* mModalComponent; //94
    Window* mWindow; //98
    Component* mCursorComponent; //9c
    int mDragX,mDragY; //a0/a4
    bool mDragging,mAllowDrag; //a8/a9
    SmartPtr<KeyboardListenerBase> mKeyboardListener; //ac
    std::set<Component*> mPopupSet; //b0..bb
    std::map<int,BGStruct> mBGStructMap; //bc..c7
};

// how a popup is opened (0x2c bytes, on the stack; provisional names)
struct PopupParams {
    PopupParams(Component* inPopup,int inFlags,Component* inOwner,int inX=-1,int inY=-1); // 0x328540
    Window* mParent; //00, borrowed
    Component* mPopup; //04, borrowed
    Component* mInputComponent; //08, borrowed
    Component* mOwnerComponent; //0c, borrowed
    int mFlags; //10
    int mx,my; //14/18
    WONRectangle mAvoidRect; // at +0x1c..0x2b; the struct is 0x2c bytes
};
// gets a component's events (vtable 0x84d...; 2 slots)
class ComponentListener : public RefCount {
public:
    virtual void HandleComponentEvent(ComponentEvent*) {}   // the base listener ignores events
};

// Callback object (0x0c bytes): RTTI 0x451ad8, allocated at 0x359089.
// Its slot 4 callback is 0x3590d0.
class ComponentListenerFunc : public ComponentListener {
public:
    typedef void (*Callback)(ComponentEvent*);
    ComponentListenerFunc(Callback callback) : mCallback(callback) {}
    virtual void HandleComponentEvent(ComponentEvent* event) { this->mCallback(event); }
    Callback mCallback; //08; implicit real base/member cleanup
};

// ComponentExtra (0x1c bytes; RTTI 0x4d5eb8, vtable 0x44d714), only a virtual destructor.
// Allocated by Component::AllocateExtraStuff (0x2f4ac0).
// The four SmartPtr members release themselves; mGrabBG is owned outright.
class ComponentExtra {
public:
    ComponentExtra(); // 0x2f4890
    virtual ~ComponentExtra(); // destructor 0x2f48d0, deleting 0x2f48b0
    SmartPtr<ColorScheme> mColorScheme; //04
    SmartPtr<ComponentListener> mListener; //08, AddListener2f4b10 /Deliver2f54e0
    SmartPtr<Cursor> mDefaultCursor; //0c
    BackgroundGrabber* mGrabBG; // at +0x10, owned; the destructor deletes the grabber (0x38 bytes)
    SmartPtr<Component> mToolTip; //14
    bool mDontDoTip; // at +0x18; +0x19..0x1b padding
};

// a listener that hands the events on to a component (0x0c bytes; vtable 0x84c134: ??_G ICF-folded 0x291f6f,
// +0x04 0x337bf0)
class ComponentListenerComp : public ComponentListener {
public:
    ComponentListenerComp(Component* inComponent) : mComponent(inComponent) {}
    virtual void HandleComponentEvent(ComponentEvent*);    // 0x337bf0 (WONAPI)

    Component* mComponent;                  // +0x08
};

// WONAPI's drawing context (provisional method names)
class Font;
// Graphics: vtable 0x450fe8;
// the field accesses at 0x326ad0/0x326ce0/0x327140 show a 0x38-byte object.
class Graphics {
public:
    virtual void ClearClipRectHook() {}
    virtual void SetClipRectHook(int, int, int, int) {}
    virtual DWORD GetPixelHook(int, int) { return 0; }
    virtual void SetPixelHook(int, int, DWORD) {}
    virtual void SetColorHook() {}
    virtual void DrawLineHook(int, int, int, int) {}
    virtual void DrawRectHook(int, int, int, int);
    virtual void DrawDottedRectHook(int, int, int, int);
    virtual void FillRectHook(int, int, int, int);
    virtual void DrawPolygonHook(POINT*, int) {}
    virtual void FillPolygonHook(POINT*, int) {}
    virtual void DrawEllipseHook(int, int, int, int) {}
    virtual void FillEllipseHook(int, int, int, int) {}
    virtual void DrawArcHook(int, int, int, int, int, int) {}
    virtual void FillRectBlendHook(int, int, int, int, int) {}
    Graphics(); //326ad0
    virtual ~Graphics(); //326b90
    virtual void Attach(DisplayContext*); //326c70
    virtual void Detach();
    virtual bool BlendSupported() { return false; }
    void SetColor(DWORD); //326ce0
    DWORD GetStandardColor(int);
    void SetSysColor(int);
    void SetFont(Font*);
    void Translate(int, int);
    void DoTranslate(int&, int&);
    void SetPixel(int, int, DWORD);
    void DrawImage(Image*, int, int);
    void DrawImage(Image*, int, int, int, int, int, int);
    void DrawLine(int, int, int, int);
    void DrawRect(int, int, int, int);
    void FillRect(int, int, int, int);
    void FillRectBlend(int, int, int, int, int);
    void DrawDottedRect(int, int, int, int);
    void DrawEllipse(int, int, int, int);
    void FillEllipse(int, int, int, int);
    void DrawArc(int, int, int, int, int, int);
    void DrawString(const GUIString&, int, int);
    void DrawString(const GUIString&, unsigned short, unsigned short, int, int);
    void PerformClip();
    void PushClipRect(int, int, int, int);
    void PopClipRect();
    SmartPtr<DisplayContext> mDisplayContext; //04
    int mTransX, mTransY; //08,0c
    SmartPtr<Font> mFont; //10
    DWORD mColor; //14
    typedef std::list<WONRectangle> ClipStack;
    ClipStack mClipStack; //18..23
    WONRectangle mClipRect; //24..33
    ColorScheme* mColorScheme; //34, borrowed
};

// 0x2fdcd0 / 0x2fddf0 (cdecl): a 3D frame (inPressed: sunken) and a flat frame (provisional names)
void LobbyDraw3DFrame(Graphics& inGraphics, int inX, int inY, int inWidth, int inHeight, bool inPressed);
void LobbyDrawFrame(Graphics& inGraphics, int inX, int inY, int inWidth, int inHeight, bool inFlag);

// a game of the lobby (0x68 bytes: original factory 0x3a6730; vtable 26 slots, the game's overrides
// named by offset)
class LobbyGame : public RefCount {
public:
    void ClearPlayers();
    bool IsSameGame(LobbyGame* inOther);
    void ReadGroup(LobbyGroup* inGroup);
    bool ReadSummaryBuffer(const ByteBuffer* inBuffer);
    ConstSmartPtr<ByteBuffer> GetJoinRequest();
    void HandleHostGame(LobbyClient* inClient);
    bool AreAllReady();
    bool CanStartGame();
    short HandleJoinRequest(ReadBuffer& inBuffer, LobbyClient* inClient, WriteBuffer& outBuffer);
    ConstSmartPtr<ByteBuffer> GetJoinReply(ReadBuffer& inBuffer, LobbyClient* inClient);
    short HandleJoinReply(ReadBuffer& inBuffer);
    GUIString GetStatusString(short inStatus);
    LobbyGame();                            // 0x3a6790 (WONAPI)
    virtual ~LobbyGame();                   // 0x3a68f0 (WONAPI)
    virtual bool PreNetJoinGameHook() { return true; }
    virtual bool PostNetJoinGameHook() { return true; }
    virtual bool PreNetCreateGameHook() { return true; }
    virtual bool PostNetCreateGameHook() { return true; }
    virtual void PreNetLeaveGameHook() {}
    virtual void PostNetLeaveGameHook() {}
    virtual void NotifyStartGameHook() {}
    virtual bool TryStartGameHook() { return true; } // (EELobbyGame 0x292719: can the game start)
    virtual bool CanStartGameHook() { return true; }
    virtual bool CanReadyHook() { return true; }
    virtual short GetJoinStatusHook();
    // (EELobbyGame's overrides): +0x30 a new game hosted by inClient, +0x34 the game's version
    virtual void HandleHostGameHook(LobbyClient* inClient) {}
    virtual void GetJoinGameRequestHook(WriteBuffer& outBuffer) {}
    // +0x38 a join request's check
    virtual short HandleJoinGameRequestHook(ReadBuffer& inBuffer, LobbyClient* inClient) { return 0; } // returns 0
    // +0x3c the game's state for a joining client
    virtual void GetJoinGameReplyHook(LobbyClient* inClient, WriteBuffer& outBuffer) {}
    virtual void CopyFromHook(LobbyGame* inGame) {}
    virtual void HandleJoinGameReplyHook(ReadBuffer& inBuffer) {} // inline; +0x44 a joined game's state read
    //: +0x48 a client left (inPlayer: its player), +0x4c a client joined
    virtual void HandlePlayerLeftHook(LobbyClient* inClient, LobbyPlayer* inPlayer) {}
    virtual void HandlePlayerJoinedHook(LobbyClient* inClient) {}
    //: +0x50 the summary (three options), +0x54 the details (options and the slots' clients), +0x58 the summary
    // read back, +0x64 a join status' text
    virtual void WriteSummaryHook(WriteBuffer& outBuffer) {}
    virtual void WriteDetailsHook(WriteBuffer& outBuffer) {}
    virtual bool ReadSummaryHook(ReadBuffer& inBuffer) { return true; }
    virtual bool ReadDetailsHook(ReadBuffer& inBuffer) { return true; } // inline; +0x5c the details read back
    // +0x60 a game message (inType 0x64..0x6a) from inFrom
    virtual void HandleGameMessageHook(unsigned char inType, ReadBuffer& inBuffer, LobbyClient* inFrom) {}
    virtual GUIString GetStatusStringHook(short inStatus) { return gEmptyGUIString; } // returns an empty string
    void CopyFrom(LobbyGame* inGame);       // 0x3a69d0
    void SetClients(LobbyClientList* inClients); // 0x3a6c10
    void SetName(const GUIString& inName) { this->mName = inName; }
    void SetPassword(const GUIString& inPassword) { this->mPassword = inPassword; }
    void SetAskToJoin(bool inAsk) { this->mAskToJoin = inAsk; }
    void SetInviteOnly(bool inInvite) { this->mInviteOnly = inInvite; }
    void SetSkill(long inSkill) { this->mSkill = inSkill; }
    void SetAddress(const IPAddr& inAddress) { this->mAddress = inAddress; }
    void SetIsHost(bool inHost) { this->mIsHost = inHost; }
    void SetCaptainId(unsigned short inId) { this->mCaptainId = inId; }
    long GetSkill() const { return this->mSkill; }
    bool GetAskToJoin() const { return this->mAskToJoin; }
    bool GetHasPassword() const { return this->mHasPassword; }
    LobbyClient* FindClient(unsigned short inId); // 0x3a6c70
    bool MatchesRoomSpec(long inFlags);
    bool IsPinging();
    bool IsQueryingDetails();
    bool PreNetJoinGame();
    bool PostNetJoinGame();
    bool PreNetCreateGame();
    bool PostNetCreateGame();
    void PreNetLeaveGame();
    void PostNetLeaveGame();
    void WriteDetails(WriteBuffer& outBuffer);
    bool CanReady();
    bool TryStartGame();
    void HandlePlayerJoined(LobbyClient* inClient);
    void HandleGameMessage(unsigned char inType, ReadBuffer& inBuffer, LobbyClient* inFrom);
    bool IsFull();
    bool CanJoin();
    short GetJoinStatus();
    void WriteSummary(WriteBuffer& outBuffer);
    bool ReadSummary(ReadBuffer& inBuffer);
    void RemoveClient(LobbyClient* inClient);
    bool ReadDetails(ReadBuffer& inBuffer);

    GUIString mName;                       // +0x08
    GUIString mPassword;                   // +0x0c
    bool mHasPassword;
    bool mInviteOnly;
    bool mAskToJoin;
    bool mInProgress;
    bool mInvited;
    bool mIsHost;
    bool mShowDetails;
    bool mHaveDetails;
    bool mPingReplied;
    long mLastSeenTime;                    // +0x1c
    bool mIsFiltered;                     // +0x20, set by the filters; padding to +0x23
    long mNumPlayers;                      // +0x24
    long mNumOpenSlots;                    // +0x28
    bool IsInProgress() const { return this->mInProgress; }
    bool GetShowDetails() const { return this->mShowDetails; }
    long GetGameType() { return this->mGameType; }
    long GetNumPlayers() { return this->mNumPlayers; }
    long GetMaxPlayers() { return this->mNumOpenSlots; }
    long mGameType;                        // +0x2c
    long mSkill;                           // +0x30
    SmartPtr<LobbyClientList> mClients;     // +0x34 (owned; constructor/destructor/copy evidence)
    IPAddr mAddress;                       // +0x38
    unsigned short mCaptainId;
    unsigned short mGroupId;
    unsigned short GetPing() { return this->mPing; }
    void SetPing(unsigned short inPing) { this->mPing=inPing; }
    unsigned short mPing;                  // base allocation 0x68 at 0x3a6730
protected:
    typedef LobbyGame* (*LobbyGameFactory)();
    static LobbyGameFactory mGameFactory;
    static LobbyGame* DefaultGameFactory() { return new LobbyGame; }
public:
    static LobbyGame* CreateGame(LobbyGameType inType);
    static void SetGameFactory(LobbyGameFactory factory) { LobbyGame::mGameFactory=factory; }

public:
    unsigned short GetGroupId() const { return this->mGroupId; }
    bool GetAskToJoinFlag() const { return this->mAskToJoin; }
    bool GetInProgress() const { return this->mInProgress; }
    bool GetIsFiltered() const { return this->mIsFiltered; }
};



// 0x361350: the game the local player is in (NULL when none)
LobbyGame* GetMyLobbyGame();

// WONAPI's message buffers (provisional names)


class ByteBuffer;
template <class T> class SmartPtr;

// 0x361b60 / 0x361b80 (cdecl): a game message to the lobby
void LobbySendToCaptain(const ByteBuffer* inMessage);
void LobbyBroadcastToGame(const ByteBuffer* inMessage);
void LobbySendToClient(LobbyClient* inClient, const ByteBuffer* inMessage);  // 0x361ba0
void LobbyResetReadyFlags();                // 0x361290

// a client of the lobby's room (its name at +0x08; provisional name)
class LobbyPlayer;

class LobbyClient : public RefCount {
public:
    LobbyClient();                         // 0x3a51a0; new 0x2c
    virtual ~LobbyClient();                // 0x3a5260
    void ClearGroup(unsigned short inId, bool inGame);
    bool IsInChatGroup();
    bool IsFriend(); // the linker folds this body into the identical one at 0x2b31d0; nonconst in the SDK
    bool IsInGameGroup();
    void ReadRoutingInfo(RoutingClientInfo* inInfo);
    void UpdateIgnoreStatus();
    bool IsAnonymous();
    bool IsMuted(bool inGame);
    bool IsAway();
    bool IsAdmin();
    bool IsModerator();
    bool IsCaptain(bool inGame);
    void SetCaptain(bool inCaptain, bool inGame);
    void SetPlayer(LobbyPlayer* inPlayer);
    bool HasPlayer();
    bool IsReady();
    unsigned short GetPing();
    bool IsTeammate(LobbyClient* inOther);

    GUIString mName;                        // +0x08
    unsigned short GetClientId() { return this->mClientId; }
    unsigned short mClientId;              // +0x0c
    unsigned long mClientFlags;
    unsigned long mWONUserId;
    unsigned long mCDKeyId;
    unsigned char mChatMemberFlags;
    unsigned char mGameMemberFlags;
    unsigned short mChatGroupId;
    unsigned short mGameGroupId;
    bool mIgnored;
    bool mBlocked;
    bool mIsFriend;
    bool mIsGameCaptain;
    bool mIsChatCaptain;
    bool mInvited;
    SmartPtr<LobbyPlayer> mPlayer;          // +0x28; setter and destructor own the reference
public:
    void SetIsFriend(bool inFriend) { this->mIsFriend = inFriend; }
    void SetInvited(bool inValue) { this->mInvited = inValue; }
    unsigned long GetWONUserId() const { return this->mWONUserId; }
    unsigned long GetCDKeyId() const { return this->mCDKeyId; }
    unsigned short GetChatGroupId() const { return this->mChatGroupId; }
    unsigned short GetGameGroupId() const { return this->mGameGroupId; }
    void SetGroupId(unsigned short inId, bool inGame) { if (inGame) this->mGameGroupId = inId; else this->mChatGroupId = inId; }
    void SetMemberFlags(unsigned char inFlags, bool inGame) { if (inGame) this->mGameMemberFlags = inFlags; else this->mChatMemberFlags = inFlags; }
    bool IsIgnored() const { return this->mIgnored; }
    bool IsBlocked() const { return this->mBlocked; }
public:
    const GUIString& GetName() const { return this->mName; }
};



// RTTI LobbyPlayer; vtable 0x852c54, four slots; size 0x0c.
class LobbyPlayer : public RefCount {
public:
    LobbyPlayer();
    virtual ~LobbyPlayer();
    virtual bool ReadData(ReadBuffer& inBuffer) { return true; } // returns true (shared body at 0x387ff0)
    virtual void WriteData(WriteBuffer& outBuffer) {} // empty (shared body at 0x30a800)
    virtual bool IsTeammate(LobbyPlayer* inOther);
    bool ReadFromBuffer(ReadBuffer& inBuffer); // 0x3a90f0 (typed exception handler/tail separately audited)
    void WriteToBuffer(WriteBuffer& outBuffer); // 0x3a9170
    bool IsSameTeam(LobbyPlayer* inOther);     // 0x3a91a0

    unsigned short mPing;                  // +0x08
    bool mIsReady;                         // +0x0a
protected:
    typedef LobbyPlayer* (*LobbyPlayerFactory)();
    static LobbyPlayerFactory mPlayerFactory;
    static LobbyPlayer* DefaultPlayerFactory() { return new LobbyPlayer; }
public:
    static LobbyPlayer* CreatePlayer();
    static void SetPlayerFactory(LobbyPlayerFactory factory) { LobbyPlayer::mPlayerFactory=factory; }

};

// a rectangle

// The lobby's core, window manager and window (EELobbyInterface.cpp's globals 0x565898, 0x565894, 0x565890). Class
// names and bases from RTTI: WONAPICoreEx : WONAPICore; MSWindowManager : WindowManager : RefCount;
// MSWindow : Window : RefCount; MSNativeIcon : NativeIcon : RefCount; MultiImageDecoder, DisplayContext. Sizes from
// EELobbyInterface 0x2972c6's `new`s (provisional member and slot names).
class WindowEventBase;
class InputEvent;
class MouseEvent;
class KeyEvent;
class SizeEvent;
// a window's drawing context (RTTI: DisplayContext : RefCount)
// Only the prefix is used: 0x326c70 stores mGraphics at +0x0c.
// DisplayContext (0x10 bytes): allocated and filled at 0x3cfab4..0x3cfad4; type descriptor 0x4e0ab0.
class DisplayContext : public RefCount {
public:
    // The port draws into software surfaces (WONSurface.h; the backend: WONLobbyPlatform.h): a context is the
    // surface an image owns, which carries the clip. The original held the GDI device context (HDC) here.
    DisplayContext(SurfaceBase* surface=NULL) : mSurface(surface),mGraphics(NULL) {}
    virtual ~DisplayContext() {}
    SurfaceBase* mSurface; // at +0x08, not owned (the owning image keeps it); NULL: nothing to draw into
    Graphics* mGraphics; //0c borrowed
    SmartPtr<NativeImage> CreateImage(int,int); //3cedd0, nonvirtual
};
class SoundDecoder;
class RawImage;

// The decoder classes are in WONLobbyImageDecoder.h.
// 0x3d2a20 / 0x3d24e0 (cdecl): add the JPG decoder (4 extensions) / the PNG decoder to a multi-decoder (provisional names)

// The core API is in WONAPICore.h.

class WindowManager : public RefCount {
protected:
    static WindowManager* mDefaultWindowManager; // 0x566270, the default manager
    static DWORD mThreadId; // 0x566274, the GUI thread's id
    bool CheckModalValid(); // checks the modal state
public:
    static WindowManager* GetDefaultWindowManager() { return WindowManager::mDefaultWindowManager; }
    static DWORD GetThreadId() { return WindowManager::mThreadId; }
    WindowManager(); // 0x32b660; the class is 0x148 bytes
    virtual ~WindowManager(); //32bff0
    virtual void GetNextEvent(); //04 32e7c0
    virtual bool SetScreenResolutionHook(int,int); //08 32c380
    virtual void GetScreenResolutionHook(int&,int&); //0c 327440
    virtual void Signal(); //10 326b60
    virtual bool AddWindow(Window*); //14 32d6a0
    virtual bool RemoveWindow(Window*); //18 32d840
    virtual void GetScreenRect(WONRectangle&); //1c 30a800
    virtual void GetSystemColorScheme(); //20 326b60; target no argument, MS3d0b70 reads own20
    void InitColorScheme(); //32c230
    void NotifyLostFocus(); //32c2d0
    void NotifyGotFocus(Window*); //32c300
    void SetScreenResolution(int,int); //32c320
    void RestoreScreenResolution(); //32c390
    Window* GetPopupWindow(); //32c440
    void SetNamedFont(const std::string&,Font*); //32c470
    void SetDefaultFont(Font* font); // inline, defined after Font below
    Font* GetNamedFont(const std::string&); //32c4d0
    Font* GetDefaultFont(); //32c540
    Font* GetDefaultFontMod(int,int=0); //32c700
    Font* GetNamedFontMod(const std::string&,int,int=0);
    static SmartPtr<Font> GetDefaultFont(const FontDescriptor&);
    static DisplayContext* GetDefaultDisplayContext();
    void RemoveAllTimerEvents();
    void SetCacheImages(bool); // 0x32c7f0: changes the image-cache setting and clears the cache when needed
    SmartPtr<NativeImage> DecodeImage(DisplayContext*,const char*); //32c950
    SmartPtr<NativeImage> DecodeImage(const char*); //32ccb0
    SmartPtr<Image> DecodeDelayImage(const char*); //32ccf0
    SmartPtr<RawImage> DecodeImageRaw(const char*); //32d160
    void SetSoundDecoder(SoundDecoder*); //32d490
    SmartPtr<Sound> DecodeSound(const SoundDescriptor&); //32d4f0
    void CloseAllWindows(); // 0x32d5b0
    void CalcModality(); //32da30
    bool NotModalWindow(Window*); // 0x32dc30 (called from the window procedure at 0x3d1730)
    // 0x32dbe0 is a separate static SetModal(Component*,bool) helper, not a member.
    int DoDialog(Window*,Component*); //32dc50
    void EndDialog(int,Window*,Component*); // 0x32ddb0
    void ExitMessageLoop(); //32ddf0
    void AdjustPopupPosition(Window*,Component*,bool,const WONRectangle&); //32de00
    void DoPopup(const PopupParams&); //32e040
    void EndPopup(Component*); //32e4d0
    void CheckEndPopup(Component*); //32e790
    int MessageLoop(); //32e7d0
    void PumpOnce(); //32e840
    void PostEvent(WindowEventBase*,bool=true); //32e850
    void AddTimerEvent(WindowEventBase*); //32e960
    void RemoveTimerEvent(WindowEventBase*); //32ea70
    void FindMouseWindow(MouseEvent*); //32ec60
    bool CheckPopupClick(MouseEvent*); //32ee40
    void DeliverEvent(WindowEventBase*); //32ef20
    void SubscribeToBroadcast(Component*,bool); //32f3f0
    void BroadcastEvent(ComponentEvent*); // 0x32f680
    void ClearSubscriptions(); //32f5d0
    WindowEventBase* PeekNextEvent(); //32f700
    void ProcessEvents(); //32f760

    typedef std::map<std::string,SmartPtr<Font>,StringLessNoCase> NamedFontMap;
    typedef std::map<std::string,SmartPtr<NativeImage>,StringLessNoCase> NativeImageMap;
    typedef std::map<std::string,SmartPtr<RawImage>,StringLessNoCase> RawImageMap;
    typedef std::map<std::string,SmartPtr<Image>,StringLessNoCase> DelayImageMap;
    typedef std::list<Component*> PopupList;
    typedef std::set<SmartPtr<Window>,SmartPtr<Window>::Comp> WindowSet;
    typedef std::list<SmartPtr<WindowEventBase> > WindowEventList;
    typedef std::set<SmartPtr<WindowEventBase> > WindowEventSet;
    typedef std::set<SmartPtr<Component> > BroadcastSet;
    CriticalSection mDataCrit; //08..1f
    SmartPtr<ColorScheme> mColorScheme; //20
    MultiImageDecoder mImageDecoder; // at +0x24..0x57 (0x34 bytes)
    SmartPtr<SoundDecoder> mSoundDecoder; //58
    SmartPtr<Font> mDefaultFont; //5c
    SmartPtr<NativeIcon> mDefaultWindowIcon; //60
    NamedFontMap mNamedFontMap; //64
    NativeImageMap mNativeImageMap; //70
    RawImageMap mRawImageMap; //7c
    DelayImageMap mDelayImageMap; //88
    int mCacheImageCount; //94
    DWORD mLastTimerTick,mLastInputTick; //98/9c
    bool mEndPopup,mIsValid,mInDoPopup,mInEndPopup,mPopupIsUp,mBA5; //a0..a5, mBA5=popup-own-window default
    int mPopupFlags; //a8; target has no later SDK PauseEvents field
    bool mDoMouseCapturePopupSupport,mIsFullScreen; //ac/ad
    int mFullScreenWidth,mFullScreenHeight,mOriginalScreenWidth,mOriginalScreenHeight; //b0..bc
    DWORD mLastLoseFocusTick; //c0
    int mCurMsgLoopDepth,mTargetMsgLoopDepth,mDialogResult; //c4/c8/cc
    Window* mModalWindow; //d0
    Component* mModalComponent; //d4
    SmartPtr<Window> mPopupWindowSave; //d8
    Window* mPopupOwnerWindow; //dc
    SmartPtr<Component> mPopupInputComponent,mPopupOwnerComponent; //e0/e4
    PopupList mPopupList; //e8
    Window* mFocusWindow; //f4
    Window* mMouseDownWindow; //f8
    Window* mDefaultWindow; //fc
    DWORD mWindowCount; //100
    WindowSet mWindowSet; //104
    WindowEventList mWindowEventList; //110
    WindowEventSet mTimerEvents,mRemoveTimerEvents; //11c/128
    bool mCallingTimers; //134
    SmartPtr<WindowEventBase> mCloseEvent; //138
    BroadcastSet mBroadcastSet; //13c..147
};
// 0x361250: the default window manager, out of line (EELobbyInit 0x295ce3 calls it; a 6-byte body that reads
// gDefaultWindowManager; its WON-side source is not written yet)
WindowManager* GetLobbyWindow();


// a window's icon (0x0c bytes; vtable 0x85107c)
class NativeIcon : public RefCount {
public:
    NativeIcon(DisplayContext* inContext);  // 0x328990
    virtual ~NativeIcon(); // 0x328a00; protected in the SDK, public here

    SmartPtr<DisplayContext> mContext;      // +0x08
};


enum FontStyles {FontStyle_Plain=0,FontStyle_Bold=1,FontStyle_Italic=2,FontStyle_Underline=4};
struct FontDescriptor {
    std::string mFace;
    int mStyle;
    int mSize;
    FontDescriptor() : mStyle(0), mSize(0) { } // the descriptor is 0x18 bytes
    FontDescriptor(const std::string& theFace, int theStyle, int theSize) : mFace(theFace), mStyle(theStyle), mSize(theSize) { }
    bool operator<(const FontDescriptor&) const; // ordering for font lookups
    bool operator==(const FontDescriptor&) const; //326840
    bool operator!=(const FontDescriptor&) const; //326880
};

// Window (0xb8 bytes): constructor and destructor out of line;
// nothing here copies one.
class Window : public RefCount {
public:
    SmartPtr<Font> GetFont(const FontDescriptor&); //32a230
    static void SetSkipDecodeImages(bool); // 0x32a640: static byte switch
    SmartPtr<RawImage> DecodeImageRaw(const char*); // 0x32a800: decodes through the manager's decoder
    SmartPtr<NativeImage> CreateImage(int,int); // 0x32a8c0, wrapper
    bool CanClose(); //32ab80
    void CheckResize(DWORD); //32ac90
    void SuspendRootResize(bool suspend);
    NativeImage* GetOffscreenImage();
    Graphics& GetGraphics();
    void RemoveComponent(Component*);
    bool IsMouseDown(MouseButton);
    void ShowNativeCursor(bool); // 0x32afc0, wrapper
    void CheckSetCursor(bool); // 0x32afd0
    bool CheckDoubleClick(MouseButton); // 0x32b110; the button index is not range-checked (reads at +0x38)
    void HandleMouseExit(); //32b120
    void HandleKeyEvent(KeyEvent*); //32b170
    void HandleMouseDown(MouseEvent*); //32b1c0
    void HandleMouseEvent(MouseEvent*); //32b260
    void HandleInputEvent(InputEvent*); //32b390

    Window();                              // 0x329c30
    virtual ~Window();                     // 0x32a020, out of line
    virtual SmartPtr<Font> PlatformGetFont(const FontDescriptor&);
    virtual bool IdleUpdate();
    virtual void TimerEvent(DWORD inElapsed);
    virtual void PlatformSetTitle();
    virtual void PlatformSetIcon();
    virtual void PlatformSetCursor(Cursor*);
    virtual void PlatformShowNativeCursor(bool);
    virtual void PlatformShow(bool);
    virtual void PlatformMinimize(bool);
    virtual void PlatformSetTopMost(bool);
    virtual void PlatformClose();
    virtual void Create(const WONRectangle&);
    virtual void SetPosSize(int, int, int, int);
    virtual void GetRect(WONRectangle&, bool);
    virtual void PlatformSetFocus();
    virtual void Show(bool);
    void Minimize(bool minimize); // nonvirtual wrapper (0x32afb0)
    DisplayContext* GetDisplayContext() { return this->mDisplayContext.get(); } // inline, at +0x28
    virtual void PlatformSetMouseCapture(bool);
    virtual void QueueEvent(WindowEventBase*);
    virtual bool PlatformIsVisible();
    static Window* GetDefaultWindow();      // 0x32a220
    SmartPtr<NativeImage> DecodeImage(const char*); // 0x32a650
    int RunDialog(Component*);              // 0x32b080
    void AddComponent(Component*); // 0x32b040: forwards to the root
    void SetModal(bool); // 0x32b060: calls Container::EnterModal/LeaveModal
    bool Close();                          // 0x32ac30, returns false or true
    void SetClientSize(int, int);           // 0x32af30
    void Center(bool);                     // 0x32ae80
    void EndDialog(int, Component*);         // 0x32b0a0
    void OpenPopup(PopupParams&);           // 0x32b0c0
    void ClosePopup(Component*);            // 0x32b0e0
    void InvalidateRoot();                  // 0x32a1e0
    void SetParent(Window*);                // 0x32a210, the parent at +0x80
    void SetTitle(const GUIString&);        // 0x32adc0
    void SetIcon(NativeIcon*);              // 0x32ae20
    int GetKeyMod() const { return this->mKeyMod; }
    enum WindowState { WindowState_Open, WindowState_Closing, WindowState_Closed };
    typedef bool (*AllowCloseFunc)(Window*);
    WindowState mWindowState; // +08
    SmartPtr<NativeImage> mOffscreenImage; // +0c
    int mMouseX; // +10
    int mMouseY; // +14
    int mKeyMod; // +18
    bool mMouseIsDown[3]; // +1c
    bool mMouseIsInClientArea; // +1f
    Cursor* mCurrentCursor; // +20
    bool mNativeCursorShowing; // +24
    SmartPtr<DisplayContext> mDisplayContext; // +28
    DWORD mButtonDownTick[3]; // +2c
    bool mButtonDoubleClick[3]; // +38
    bool mDetectDoubleClick; // +3b
    int mDoubleClickX; // +3c
    int mDoubleClickY; // +40
    DWORD mLastMoveTick; // +44
    int mMinWidth; // +48
    int mMinHeight; // +4c
    int mMaxWidth; // +50
    int mMaxHeight; // +54
    bool mNeedResize; // +58
    int mNewWidth; // +5c
    int mNewHeight; // +60
    DWORD mResizeDelay; // +64
    DWORD mLastResizeTick; // +68
    DWORD mLastCheckCursorTick; // +6c
    int mSuspendRootResize; // +70
    int mStyle; // +74
    bool mIsPopup; // +78
    bool mIsOneComponentWindow; // +79
    bool mMinimizeOnResolutionRestore; // +7a
    WindowManager* GetWindowManager() { return this->mManager; } // inline
    WindowManager* mManager; // +7c
    Window* mParent; // +80
    std::set<Window*> mChildWindowSet; // +84
    SmartPtr<WindowEventBase> mCloseEvent; // +90
    AllowCloseFunc mAllowCloseFunc; // +94
    SmartPtr<NativeIcon> mIcon; // +98
    GUIString mTitle; // +9c
    bool mIsShowing; // +a0
    SmartPtr<RootContainer> mRoot; // +a4
    std::map<FontDescriptor, SmartPtr<Font> > mFontMap; // +a8
    SmartPtr<Font> mDefaultFont; // +b4
};


// WONAPI lobby state functions the game's lobby glue calls
int LobbyRun();                             // 0x362760 (how the lobby was left: 1, 2 or 3)
void LobbySuspend();                        // 0x362830
void LobbyResume();                         // 0x362880
void LobbyShutdown();                       // 0x362170
Container* LobbyGetContainer();             // 0x362250
void LobbyStart();                          // 0x362740
void LobbyGetNetAddresses(std::list<LobbyNetAddress>& outAddresses, bool inFlag);   // 0x2b32b0 (cdecl)

// 0x361390: the local player (NULL when none)
LobbyPlayer* GetMyLobbyPlayer();
// 0x34a100 (cdecl): a text with its %s-style argument filled in (provisional name)
GUIString LobbyFormatString(const GUIString& inFormat, const GUIString& inArg);
void LobbySetGameInProgress(bool inFlag);   // 0x3612b0, SDK SetGameInProgress(bool)
bool LobbyLeaveGame(bool inFlag);            // 0x361310
bool LobbyIAmCaptain();                     // 0x3613c0
void LobbyNetAdvertiseGame();               // 0x361260
void LobbyNotifyStagingChange();            // 0x3613e0

class ServerListCtrl;
class ServerDetailsItem;
typedef std::list<ServerDetailsItem*> ServerDetailsList;
class ServerDetailsItem : public ListItem {
public:
    ServerDetailsItem();
    virtual ~ServerDetailsItem();
    virtual void Paint(Graphics&,ListArea*);
    virtual void CalcDimensions(ListArea*);
    virtual int Compare(const ListItem*,ListArea*) const;
    virtual bool CanSelect(ListArea*,int,int);
    virtual void MouseDown(ListArea*,int,int,int);
    virtual void PaintDetailsHook(Graphics&,MultiListArea*) {} // empty inline, vtable +0x38
    virtual int GetDetailsHeightHook(MultiListArea*) { return 0; } // returns 0, vtable +0x3c
    void SetGame(LobbyGame*);
    void CalcIcon();
    static bool IsStartOfNumber(char);
    static bool ParseIntFilter(const GUIString&,int&,int&);
    static void PaintIcon(Graphics&,ServerDetailsItem*,ServerListCtrl*);
    static void PaintSkill(Graphics&,ServerDetailsItem*,ServerListCtrl*);
    static void PaintGameName(Graphics&,ServerDetailsItem*,ServerListCtrl*);
    static void PaintAddress(Graphics&,ServerDetailsItem*,ServerListCtrl*);
    static void PaintNumPlayers(Graphics&,ServerDetailsItem*,ServerListCtrl*);
    static void PaintPing(Graphics&,ServerDetailsItem*,ServerListCtrl*);
    static int CompareIcon(const ServerDetailsItem*,const ServerDetailsItem*,ServerListCtrl*);
    static int CompareSkill(const ServerDetailsItem*,const ServerDetailsItem*,ServerListCtrl*);
    static int CompareGameName(const ServerDetailsItem*,const ServerDetailsItem*,ServerListCtrl*);
    static int CompareAddress(const ServerDetailsItem*,const ServerDetailsItem*,ServerListCtrl*);
    static int CompareNumPlayers(const ServerDetailsItem*,const ServerDetailsItem*,ServerListCtrl*);
    static int ComparePing(const ServerDetailsItem*,const ServerDetailsItem*,ServerListCtrl*);
    static void FilterSkill(const GUIString&,ServerDetailsList&,ServerListCtrl*);
    static void FilterGameName(const GUIString&,ServerDetailsList&,ServerListCtrl*);
    static void FilterAddress(const GUIString&,ServerDetailsList&,ServerListCtrl*);
    static void FilterNumPlayers(const GUIString&,ServerDetailsList&,ServerListCtrl*);
    static void FilterPing(const GUIString&,ServerDetailsList&,ServerListCtrl*);
    SmartPtr<LobbyGame> mGame; //18
    GUIString mIPAddrStr; //1c
    int mIconId; //20
    unsigned char mSummaryHeight; //24
protected:
    typedef ServerDetailsItem* (*ItemFactoryFunc)();
    static ItemFactoryFunc mItemFactoryFunc;
    static ServerDetailsItem* DefaultItemFactory() { return new ServerDetailsItem; }
public:
    static ServerDetailsItem* CreateItem() { return ServerDetailsItem::mItemFactoryFunc(); }
    static void SetItemFactory(ItemFactoryFunc factory) { ServerDetailsItem::mItemFactoryFunc=factory; }

};

class WindowEventBase : public RefCount {
public:
    WindowEventBase(long inType = 0) : mEventType(inType), mWindow(NULL) {}
    virtual void Deliver() = 0;             // +0x04: original vtable 0x44df64 -> _purecall

    long mEventType;                        // +0x08
    Window* mWindow;                        // +0x0c
};

// an event that calls a function with one argument when it is delivered
class UserEvent : public WindowEventBase {
public:
    typedef void (*Func)(int inParam);

    UserEvent(Func inFunc, int inParam) : mFunc(inFunc), mParam(inParam) {}
    virtual void Deliver() { this->mFunc(this->mParam); }  // +0x04 0x295d30

    Func mFunc;                             // +0x10
    int mParam;                             // +0x14
};

// a component's event (slot +0x7c; type at +0x14: WONAPI's own ids, 100000 up for the game's)
// ComponentEvent: 0x20 bytes, holds a reference to the sender at +0x10.
// Window events: allocated at 0x32ac45; mouse, key and size events are 0x32/0x24/0x32 bytes.
class InputEvent : public WindowEventBase {
public:
    InputEvent(int type) : WindowEventBase(type) {}
    virtual void Deliver(); //32b520
};
class MouseEvent : public InputEvent {
public:
    MouseEvent(int type,int x,int y,int mod,int button=0)
      : InputEvent(type),mx(x),my(y),mKeyMod(mod),mButtonOrAmount(button) {}
    virtual void Deliver(); //32b630
    int mx,my,mKeyMod,mButtonOrAmount; //10,14,18,1c
};
class KeyEvent : public InputEvent {
public:
    KeyEvent(int type,int key,int mod) : InputEvent(type),mKey(key),mKeyMod(mod) {}
    virtual void Deliver(); //32b640
    int mKey,mKeyMod; //10,14
};
class SizeEvent : public InputEvent {
public:
    SizeEvent(int x,int y,int width,int height)
      : InputEvent(InputEvent_SizeChange),mx(x),my(y),mWidth(width),mHeight(height) {}
    int mx,my,mWidth,mHeight; //10,14,18,1c
};

class ComponentEvent : public WindowEventBase {
public:
    ComponentEvent(Component* inComponent, int inType); // 0x32b530
    virtual void Deliver();                // 0x32b610
    void SetComponent(Component* inComponent); // 0x32b5a0
    int GetControlId() const;               // 0x32b600
    SmartPtr<Component> mComponentRef;      // +0x10
    int mType;                              // +0x14
    bool mBroadcast;                        // +0x18
    Component* mComponent;                  // +0x1c
};

// ListItemClickedEvent: RTTI 0x44dd78, 0x34 bytes.
class ListItemClickedEvent : public ComponentEvent {
public:
    ListItemClickedEvent(Component* component,ListItem* item,MouseButton button,bool dbl,bool up,int x,int y)
      :ComponentEvent(component,up?ComponentEvent_ListItemUpClicked:ComponentEvent_ListItemClicked),
       mButton(button),mDoubleClick(dbl),mItem(item),mX(x),mY(y) {}
    bool IsSelectedItem(); // SDK ListBox.cpp; 0x2fa550
    int GetButton() const { return this->mButton; }
    bool IsDoubleClick() const { return this->mDoubleClick; }
    int mButton;                            // +0x20
    bool mDoubleClick;                      // +0x24
    SmartPtr<ListItem> mItem;               // +0x28
    int mX;                                 // +0x2c
    int mY;                                 // +0x30
};

// LobbyEvent adds no members to ComponentEvent (0x20 bytes).
// Derived lobby events are declared only in WONLobbyEvents.h, owned by core.
class LobbyEvent : public ComponentEvent {
public:
    LobbyEvent(int inType) : ComponentEvent(NULL, inType) {}
};

// RTTI shows that GetClient's (0x3aa370) receiver is LobbyServer (declared in WONLobbyModel.h).
class LobbyServer;
LobbyServer* LobbyGetCurrentServer();       // 0x35b6f0; NULL outside a server

// RTTI LobbyClientList : RefCount; constructor 0x3a55f0, 0x14 bytes.
typedef std::map<unsigned short, SmartPtr<LobbyClient> > LobbyClientMap;
class LobbyClientList : public RefCount {
public:
    LobbyClientList();
    virtual ~LobbyClientList();
    void Clear();
    void SyncClients(const std::map<unsigned short, SmartPtr<RoutingClientInfo> >& inMap);
    void UpdateIgnoreStatus();
    void AddClient(LobbyClient* inClient);
    LobbyClient* FindClient(unsigned short inId);
    SmartPtr<LobbyClient> RemoveClient(unsigned short inId);
    LobbyClient* FindClientByWONId(unsigned long inId);
    void ClearInvited();
    LobbyClientMap mClients;               // +0x08
};

// Generic non-owning client iterator: 0x3a5f70..0x3a6140; size 0x08.
class LobbyClientIterator {
public:
    LobbyClientIterator(LobbyClientList* inList);
    bool HasMore();
    LobbyClient* Next();
    void Rewind();
    LobbyClientMap* mClients;
    LobbyClientMap::iterator mItr;
};

// Iterator over the clients that have players (provisional name); 0x0c bytes.
class LobbyClientSetIterator {
public:
    LobbyClientSetIterator(LobbyClientList* inList);
    bool HasMore();
    LobbyPlayer* Next(LobbyClient** outClient);
    void Rewind();
    LobbyClientMap* mClients;
    LobbyClientMap::iterator mItr;
    bool mReady;
};
bool LobbyIsNameIgnored(const GUIString& inName); // persistent ignore predicates; cdecl
bool LobbyIsCDKeyBlocked(unsigned long inAddress);
unsigned short GetLobbyProductId();        // 0x35b580 (LobbyGame summary wire product id)

// 0x361370 (cdecl): the local client
LobbyClient* GetMyLobbyClient();
// 0x361b20 (cdecl): EELobbyInterface 0x296d1e passes it as the connection request's flag
bool LobbyIsBehindFirewall();

// 0x359120 (cdecl): fires a component event of inType
void LobbyBroadcastLobbyEvent(int inType, bool inFlag);
// 0x359100 (cdecl): delivers inEvent to the lobby's components
void LobbyBroadcastEvent(ComponentEvent* inEvent, bool inFlag);
// 0x361bc0
void LobbyKickClient(LobbyClient* inClient, bool inFlag);

// 0x361250: default GUI manager (global 0x566270, assigned by WindowManager constructor).

// Font, Image and NativeImage are declared for their callers; nothing here allocates them.

class Font : public RefCount {
public:
    Font(DisplayContext*, const FontDescriptor&); //3268a0
    const FontDescriptor& GetDescriptor() { return this->mDescriptor; }
    int GetSize() { return this->mDescriptor.mSize; }
    int GetStyle() { return this->mDescriptor.mStyle; }
    const std::string& GetFace() { return this->mDescriptor.mFace; }
    virtual ~Font();                        // 0x326a60, out of line
    virtual void PrepareContext(DisplayContext*);
    virtual void DrawString(DisplayContext*, const GUIString&, unsigned short, unsigned short, int, int);
    int GetStringWidth(const GUIString& theStr) { return this->GetStringWidth(theStr,0,(unsigned short)theStr.length()); }
    virtual int GetStringWidth(const GUIString&, unsigned short, unsigned short);
    virtual int GetCharacterWidth(int inChar);
    virtual int GetHeight();
    virtual int GetAscent();
    virtual int GetDescent();
    FontDescriptor mDescriptor;             // +08..1f
};
inline void WindowManager::SetDefaultFont(Font* font) {this->mDefaultFont=font;} // inline (needs the complete Font)

// FontConfig (0x98 bytes): the factory 0x38a160 allocates it, constructor 0x38a1c0, implicit destructor 0x38a250.
class FontConfig : public ComponentConfig {
public:
    static ComponentConfig* CfgFactory(); // 0x38a160
    static Component* CompFactory(); // 0x2c7b10
    FontConfig(); // 0x38a1c0
    Font* GetFont(); // 0x38a300; recalculates the font on demand
    void SetFont(Font*); //38a3e0
    virtual bool PostParse(ConfigParser*); // 0x3099f0 (shared hook body)
    virtual bool Virtual24(const std::string&); //38a5f0
    virtual bool GetNamedValue(const std::string&,int&); //38a580
    SmartPtr<Font> mFont; //78
    FontDescriptor mDescriptor; //7c..93
    bool mNeedRecalcFont; // at +0x94; +0x95..0x97 padding; the class is 0x98 bytes
    // Implicit destructor; 0x38a250 has no own vptr store.
};
inline int WrappedText::GetHeight() { return this->mHeight==0 ? this->mFont->GetHeight() : this->mHeight; }
class Image : public RefCount {
public:
    void DrawEntire(DisplayContext*,int,int); // 0x3285a0: draws the whole image
    virtual ~Image();
    virtual void Draw(DisplayContext*, int, int, int, int, int, int);
    virtual void SetSize(int, int);
    virtual SmartPtr<Image> Duplicate();
    virtual SmartPtr<NativeImage> GetNative(DisplayContext*);
    unsigned short GetWidth() const { return this->mWidth; }
    unsigned short GetHeight() const { return this->mHeight; }
    unsigned short mWidth;                  // +08
    unsigned short mHeight;                 // +0a
};
// NativeMask (0x0c bytes): constructor 0x3288b0, RTTI 0x4d6e80.
class NativeMask : public RefCount {
public:
    NativeMask(DisplayContext*); //3288b0
    virtual ~NativeMask(); //328920
    SmartPtr<DisplayContext> mDisplayContext; //08
};
// NativeImage (abstract, 0x18 bytes); 0x328a70/0x328b20.
class NativeImage : public Image {
public:
    NativeImage(DisplayContext*); //328a70
    Graphics& GetGraphics(); //328c20
    void SetTransparentMask(NativeMask* mask) { this->mTransparentMask=mask; }
    virtual ~NativeImage(); //328b20
    virtual void Draw(DisplayContext*,int,int,int,int,int,int) = 0;
    virtual SmartPtr<NativeImage> GetNative(DisplayContext*); //328bf0
    virtual Graphics* PlatformGetGraphics() = 0;
    virtual SmartPtr<NativeIcon> GetIcon();
    virtual SmartPtr<RawImage32> GetRaw();
    Graphics* mGraphics; // at +0x0c, owned
    SmartPtr<DisplayContext> mDisplayContext; //10
    SmartPtr<NativeMask> mTransparentMask; //14
};


// Target classes merge platform behavior into the concrete RawImage classes.
// Six vtable slots (tables at 0x4510a8/0x4510c4/0x4510e0); the later SDK's platform
// virtuals and GetRawImage32 virtual are absent from this binary.
class Palette : public RefCount {
public:
    Palette(int n) : mNumColors(n) { this->mColors=new DWORD[n]; } // inline; allocates n colors (used at 0x336828)
    SmartPtr<Palette> Duplicate(); // 0x349040: copies the palette
    void SetColor(int thePos, unsigned long theVal) { this->mColors[thePos] = theVal; }
    unsigned long GetColor(int thePos) { return this->mColors[thePos]; }
    int GetNumColors() { return this->mNumColors; }
    virtual ~Palette(); // out of line
    DWORD* mColors; //08
    int mNumColors; //0c
}; // 0x10 bytes, two owned members; destructor out of line
class NativeMask;
class RawImage : public Image {
public:
    RawImage(int type) : mType(type) {}
    virtual void Draw(DisplayContext*,int,int,int,int,int,int); //328c50
    virtual SmartPtr<NativeImage> GetNative(DisplayContext*) = 0;
    virtual bool HasTransparency() { return false; }
    int mType; //0c; 0=1bit,1=8bit,2=32bit
};
class RawImage32 : public RawImage {
public:
    RawImage32(int,int); // 0x328cf0; 0x24 bytes
    virtual ~RawImage32(); //328da0
    virtual SmartPtr<Image> Duplicate(); //328df0
    virtual SmartPtr<NativeImage> GetNative(DisplayContext*); //328f30
    virtual bool HasTransparency() { return this->mDoTransparency; }
    SmartPtr<NativeImage> CreateNativeImage(DisplayContext*); // creates the platform image
    DWORD GetPixel(int,int); //328ee0
    DWORD* StartRowTraversal(int); //328f00

    void SetDoTransparency(bool value) { this->mDoTransparency=value; }
    void SetRowPixel(DWORD value) { *this->mRowPtr=value; }
    bool HasMoreRows() { return this->mRowEnd<=this->mImageEnd; }
    bool HasMoreRowPixels() { return this->mRowPtr<this->mRowEnd; }
    DWORD* GetImageData() { return this->mImageData; }
    DWORD GetRowPixel() { return *this->mRowPtr; }
    void NextRowPixel() { ++this->mRowPtr; }
    void NextRowPixel(int theAmount) { this->mRowPtr+=theAmount; }
    void NextRow() { this->mRowPtr=this->mRowEnd; this->mRowEnd+=this->mWidth; }
    DWORD* mImageData; //10
    DWORD* mImageEnd; //14
    DWORD* mRowPtr; //18
    DWORD* mRowEnd; //1c
    bool mDoTransparency; //20
};
class RawImage8 : public RawImage {
public:
    RawImage8(int,int,Palette* =NULL); // 0x329170; 0x30 bytes
    virtual ~RawImage8(); //329280
    virtual SmartPtr<Image> Duplicate(); //329300
    virtual SmartPtr<NativeImage> GetNative(DisplayContext*); //329460
    virtual bool HasTransparency() { return this->mTransparentColor>=0; }
    void CalcBytesPerRow(); //real nonvirtual platform row initialization
    SmartPtr<NativeImage> CreateNativeImage(DisplayContext*);
    unsigned char GetPixel(int,int); //329420
    unsigned char* StartRowTraversal(int); //329440
    unsigned char GetRowPixel() { return *this->mRowPtr; }
    void SetRowPixel(unsigned char value) { *this->mRowPtr=value; }
    void NextRowPixel() { ++this->mRowPtr; }
    void NextRowPixel(int theAmount) { this->mRowPtr+=theAmount; }
    unsigned char* mImageData; //10
    unsigned char* mImageEnd; //14
    unsigned char* mRowPtr; //18
    unsigned char* mRowEnd; //1c
    int mRowPad; //20
    int mBytesPerRow; //24
    SmartPtr<Palette> mPalette; //28
    int mTransparentColor; //2c
};
class RawImage1 : public RawImage {
public:
    RawImage1(int,int); // 0x329690; 0x3c bytes
    virtual ~RawImage1(); //329790
    virtual SmartPtr<NativeImage> GetNative(DisplayContext*); //3d07e0
    void CalcBytesPerRow(); //real nonvirtual platform row initialization
    SmartPtr<NativeMask> GetMask(DisplayContext*); //3d06b0, nonvirtual
    unsigned char* StartRowTraversal(int); //3297e0
    void NextRow() { this->mRowPtr=this->mRowEnd+this->mRowPad; this->mRowEnd+=this->mBytesPerRow; this->mRowMask=128; }
    int GetBytesPerRow() { return this->mBytesPerRow; }
    unsigned char* GetImageData() { return this->mImageData; }
    void NextRowPixel() { this->mRowMask>>=1; if(this->mRowMask==0) { this->mRowPtr++; this->mRowMask = 128; } }
    void NextRowPixel(int theAmount) { for(int i=0; i<theAmount; i++) this->NextRowPixel(); }
    void SetRowPixel(bool on) { if(on) *this->mRowPtr |= this->mRowMask; else *this->mRowPtr &= ~mRowMask; }
    bool GetRowPixel() { return (*this->mRowPtr)&this->mRowMask?true:false; }
    unsigned char* mImageData; //10
    unsigned char* mImageEnd; //14
    unsigned char* mRowPtr; //18
    unsigned char* mRowEnd; //1c
    int mRowMask; //20
    int mRowPad; //24
    int mEndRowMask; //28
    int mBytesPerRow; //2c
    int mBytesPerRowMinusPad; //30
    int mCompleteBytesPerRow; //34
    int mWidthMod8; //38
};

class TextListItem : public ListItem {
public:
    TextListItem() : mColor(-1) {}
    TextListItem(const GUIString& text) : mText(text),mColor(-1) {} // inline (used by the menu constructor at 0x2fcd54..0x2fcd7a)
    TextListItem(const GUIString& inText, Image* inImage) : mText(inText), mColor(-1), mImage(inImage) {}
    TextListItem(const GUIString& inText, Font* inFont) : mText(inText), mColor(-1), mFont(inFont) {}
    virtual ~TextListItem() {}              // explicit inline destructor
    virtual void Paint(Graphics&, ListArea*);     // 0x2fa380
    virtual void CalcDimensions(ListArea*);  // 0x2fa2c0
    virtual int Compare(const ListItem*, ListArea*) const;   // 0x2fa360
    GUIString mText;                        // +18
    long mColor;                            // +1c
    SmartPtr<Font> mFont;                   // +20
    SmartPtr<Image> mImage;                 // +24
};
// Allocated at 0x2f4133 (type 0x27), 0x28 bytes; the implicit destructor 0x2f41c0 has no own vptr store.
class ComboSelChangedEvent : public ComponentEvent {
public:
    ComboSelChangedEvent(Component* c,TextListItem* item,bool active)
        :ComponentEvent(c,ComponentEvent_ComboSelChanged),mItem(item),mActiveSel(active) {}
    SmartPtr<TextListItem> mItem; //20
    bool mActiveSel; //24
};

// MenuItem (0x38 bytes, allocated at 0x2fd49c): TextListItem (0x28) base.
// Constructor and implicit destructor out of line.
class PopupMenu;
class MenuItem : public TextListItem {
public:
    // Implicit virtual destructor (0x2fcde0)
    MenuItem(const GUIString&,ComponentListener*); // 0x2fcd30
    virtual void Paint(Graphics&,ListArea*);     // 0x2fcf10, vtable +0x04
    virtual void CalcDimensions(ListArea*); // 0x2fcee0, vtable +0x08
    SmartPtr<PopupMenu> mSubMenu;            // +28
    int mMenuId;                            // +2c
    bool mDisabled; // at +0x30 (written at 0x2fcda4, read at 0x2fd018); +0x31..0x33 padding
    SmartPtr<ComponentListener> mListener; // at +0x34
};

// Only a prefix of ScrollArea is declared; nothing here allocates it.
class ScrollArea : public Container {
public:
    ScrollArea(); // 0x304df0: viewport setup
    // Implicit virtual destructor (0x2fa790)
    virtual void AdjustPosition(bool);      // 305030
    virtual void ScrollSizeChanged();       // 4142a0
    virtual void ScrollPaint(Graphics&);     // 2fa500
    virtual void Virtual08(); //3051c0 SizeChanged
    virtual void GetDesiredSize(int&,int&); //3054a0
    virtual bool MouseWheel(int); //305410 MouseWheel
    virtual void Paint(Graphics&); // 0x3052d0 (override)
    void SetDelegatePos(); //304fd0
    bool OnBottom(); //305190
    void SetBorderPadding(int,int,int,int); //305270
    void ScrollTranslate(int&,int&); //305390
    void ScrollUntranslate(int&,int&); //3053d0
    virtual void HandleComponentEvent(ComponentEvent*); // 0x305180, vtable +0x7c override
    void SetSizeEqualToTotal(int,int); //305450
    bool IsRectVisible(const WONRectangle&); //3054f0
    ScrollAreaViewport* GetViewport() { return this->mViewport.get(); }
    void SetScrollDimension(int,int); // sets the content dimensions
    void SetScrollbar(Scrollbar*); // selects a scrollbar scroller
    void SetScrollButton(Button*,Direction); // selects a button scroller
    void SetScroller(Scroller*); // 0x305010; the scroller at +0xd4 is not owned
    int GetTotalHeight() { return this->mTotalHeight; }
    int GetTotalWidth() { return this->mTotalWidth; }
    int GetLeftPad() { return this->mLeftPad; }
    int GetRightPad() { return this->mRightPad; }
    void SetVertOffset(int); //305130
    void SetOffsets(int, int); //305150, horizontal then vertical
    SmartPtr<ScrollAreaViewport> mViewport; // at +0x84
    WONRectangle mInvalidRect; //88..97
    int GetVertOffset() { return this->mVertOffset; }
    int mVertOffset; //98
    int mHorzOffset; //9c
    int mTotalHeight; //a0
    int mTotalWidth;                        // total width
    int mAVertOffset,mAHorzOffset,mATotalHeight,mATotalWidth; //a8..b7
    int mOldScrollAreaWidth,mOldScrollAreaHeight; //b8..bf
    int mLeftPad; //c0
    int mRightPad; //c4
    int mTopPad,mBottomPad; //c8,cc
    bool mUserHasScrolled; //d0
    Scroller* mScroller; //d4 borrowed
    WONRectangle mScrollArea; //d8, visible height e4
    Background mBackground; // at +0xe8..0xfb
    void SetBackground(const Background& theBackground) { this->mBackground=theBackground; }
    int mExtraHeight; //fc
    int mUpScrollAmount,mDownScrollAmount,mLeftScrollAmount,mRightScrollAmount; //100..10f
};

// Not identified in the original exe
class ComponentScrollArea : public ScrollArea {
protected:
    SmartPtr<Component> mComponent;
    bool mHorzCenter,mVertCenter;
    void PositionComponent();
public:
    virtual void Virtual08();
    ComponentScrollArea(Component*);
    void SetComponentSize(int,int);
    void UpdateComponentSize();
    Component* GetComponent() { return this->mComponent.get(); }
};

enum ListSortType { ListSortType_Ascending, ListSortType_Descending, ListSortType_None };
// ListAreaToolTip (0x38 bytes): allocated at 0x2fbbb6, constructor 0x2fb7b0,
// Component (0x30) plus the ListArea pointer at +0x30 and the owned item at +0x34; 43 vtable slots.
class ListAreaToolTip : public Component {
public:
    ListAreaToolTip(ListArea*); // 0x2fb7b0
    virtual void Paint(Graphics&); // 0x2fb8d0, vtable +0x18
    void DoTip(ListItem*); //2fb9b0
    void EndTip(); //2fbab0
    ListArea* mListArea; //30 borrowed
    SmartPtr<ListItem> mItem; // at +0x34
};
// ListArea (0x164 bytes): ScrollArea (0x110) base plus vectors and SmartPtr members.
class ListArea : public ScrollArea {
public:
    ListArea(); // 0x2fa570: search, font and focus setup
    virtual ~ListArea(); // 0x2fa860: ends the tooltip, then releases the members
    void SetSortType(ListSortType); // 0x2facd0
    typedef int (*SortFunc)(ListArea*, const ListItem*, const ListItem*);
    static int NumberSortFunc(ListArea*,const ListItem*,const ListItem*); // 0x2faaf0
    struct LineSortPred {
        ListArea* mListArea; SortFunc mSortFunc; bool mAscending;
        LineSortPred(ListArea* area,SortFunc func,int type):mListArea(area),mSortFunc(func),mAscending(type==0) {}
        bool operator()(const ListItem* a,const ListItem* b) { if(this->mAscending) return this->mSortFunc(this->mListArea,a,b)<0; else return this->mSortFunc(this->mListArea,a,b)>0; }
    };
    static int DefaultSortFunc(ListArea* area,const ListItem* a,const ListItem* b) { return a->Compare(b,area); }
    SortFunc GetSortFunc(); // 0x2fab20
    int FindInsertPos(ListItem*,int); //2fabe0
    void DeleteItemPrv(int); //2fca50
    void CheckMouseSel(ListItem*,int,int,bool=false); //2fb060
    void ItemMouseAt(int&,int&); //2fb6a0
    ListItem* FindSelectableItem(int,bool,bool=true); //2fc0a0
    virtual void AdjustPosition(bool); //2fadf0 AdjustPosition
    virtual void Virtual08(); //2faef0 SizeChanged
    virtual void ScrollPaint(Graphics&); //2fafb0 ScrollPaint
    virtual void MouseEnter(int,int); //2fc010 MouseEnter
    virtual void MouseExit(); //2fc040 MouseExit
    virtual void MouseDown(int,int,int); //2fbd00 MouseDown
    virtual void MouseUp(int,int,int);   //2fbef0 MouseUp
    virtual void MouseMove(int,int); //2fbad0 MouseMove
    virtual void MouseDrag(int,int); //2fbc70 MouseDrag
    virtual bool KeyDown(int);   //2fc160 KeyDown
    // Added slots +0xd8/+0xdc/+0xe0: ListArea has 57 slots, TreeArea overrides +0xd8 and +0xe0.
    virtual void NotifyMouseItemGone();     // 0x2fcad0, no args
    virtual void ListSelChangedHook();      // 0x4142a0, no args
    virtual bool Sort();                   // 0x2fab50, no args, AL result
    void BeginUpdate();                     // 0x2faa00 (before the list is refilled)
    void EndUpdate();                       // 0x2faa10 (after it)
    void Clear(bool inFlag);                // 0x2faa50 (clears it?)
    ListItem* GetItem(int inIndex);         // 0x2fc4e0
    ListItem* InsertItem(ListItem* inItem, int inPos = -1);         // 0x2fc510
    void RefreshItem(ListItem* inItem);     // 0x2fc730 (an item changed)
    void DeleteItem(int); //2fcae0 index deletion
    void SetListFlags(unsigned long,bool); // 0x2fb600: sets or clears list flags
    void RemoveItem(ListItem* inItem);      // 0x2fcb00
    ListItem* InsertString(const GUIString& inText, int inPos = -1);    // 0x2fcc30
    int GetNumItems() const { return this->mItems.size(); }   // 0x290d70 (EELobbyConnectDialog.cpp's COMDAT)

    int GetItemPos(ListItem* inItem);        // 0x2fc450
    void SetSelItem(int inIndex, bool inFireEvent = true); // 0x2fb410, ret8
    void EnsureVisible(ListItem* inItem);    // 0x2faf60, ret4
    void FindNextSelection(); // 0x2fb4d0: scans for the next selected item
    int ClearSelectionPrv(bool); // 0x2fb560: returns the number of items deselected
    void ClearSelection(bool = true); // 0x2fb5f0 (forwarder)
    bool SetSelItem(ListItem*,bool,bool); // 0x2fb200
    void SelectRange(int,int); //2fb2e0
    std::vector<SmartPtr<ListItem> >::iterator FindLineAt(int); //2facf0
    ListItem* FindItemAt(int); //2fad80
    void FindTopLine(); //2fadc0
    bool IsItemVisible(ListItem*); //2faf10
    void EnforceInvariants(); //2fc3c0
    void DeleteRange(int,int); //2fcb20
    void RewindSelections();               // 0x2fb480
    bool HasMoreSelections();              // 0x2fb490
    ListItem* GetNextSelection();           // 0x2fb530
    int GetTextColor(); //2fb520
    int GetItemPosAtYPos(int); //2fc4b0
    bool ListFlagSet(int flags) { return this->mListFlags & flags ? true : false; }
    ListItem* GetSelItem() { return this->mSelItem; } // inline
    const SelectionColor& GetSelColor() { return this->mSelColor; } // inline
    void SetFont(Font* theFont) { this->mFont=theFont; }
    std::vector<SmartPtr<ListItem> > mItems;          // +0x110
    int mTopLineVectorPos; // at +0x120 (0x2fadc0)
    int mListFlags; //124
    int mDoingMultiChange; // at +0x128 (0x2faa00/0x2faa10)
    int mPaintWidth;                        // layout width
    int mSortType; // at +0x130, a ListSortType value
    SmartPtr<ListAreaToolTip> mItemToolTip; // at +0x134, owned
    SmartPtr<Font> mFont;                            // +0x138
    int mTextColor; //13c
    SelectionColor mSelColor; //140
    ListItem* mSelItem;                     // +0x14c (a combo box's list: ComboBox::GetSelItem 0x2f3d40)
    ListItem* mKeyItem; //150, borrowed
    ListItem* mMouseItem; //154, borrowed
    int mSelItemItr; //158
    SmartPtr<ListItem> mSearchLine; //15c
    SortFunc mSortFunc; // at +0x160; the class is 0x164 bytes
};


// TreeItem (0x3c bytes): allocated at 0x3150b2, constructor 0x3140f0, destructor 0x314170.
// Constructor and destructor are out of line; nothing here allocates a TreeArea.
class TreeArea;
class TreeItem : public ListItem {
public:
    TreeItem(); // 0x3140f0
    // Implicit virtual destructor (0x314170)
    virtual void Paint(Graphics&,ListArea*);     //314b60
    virtual void CalcDimensions(ListArea*); //314df0
    virtual void SetViewport(ListArea*,Container*); //314dd0
    virtual int Compare(const ListItem*,ListArea*) const;   //314e20
    virtual bool CanSelect(ListArea*,int,int); //314e40
    virtual void SetPos(int,int);    //314b40
    virtual void MouseEnter(ListArea*,int,int); //3142c0
    virtual void MouseExit(ListArea*); //3142e0
    virtual void MouseMove(ListArea*,int,int); //314310
    virtual void MouseDrag(ListArea*,int,int); //314340
    virtual void MouseDown(ListArea*,int,int,int); //3143b0
    virtual void MouseUp(ListArea*,int,int,int);   //314370
    TreeItem* GetChild(int); //314440
    TreeItem* GetNextNonChild(); //314470
    int GetNextNonChildPos(TreeArea*); // 0x3144b0
    int GetInsertPos(TreeArea*,TreeItem*,int); //3144d0
    void InsertChild(TreeArea*,TreeItem*,int=-1); //314570
    void RemoveAllChildren(TreeArea*); //314710
    void RemoveChild(TreeArea*,TreeItem*); //314770
    void RemoveFromParent(TreeArea*); //314830
    void Sort(TreeArea*,bool); // sorts the children recursively (not identified in the original exe)
    void CloseAll(); //314850
    void InsertChildren(TreeArea*,int); //314880
    void SetOpen(TreeArea*,bool,bool=false,bool=true); //3148e0
    void ToggleOpen(TreeArea*,bool=false,bool=true); // 0x314af0
    bool IsLastChild(); //314b10
    ListItem* GetMouseItem(ListArea*,int&,int&); //314210
    bool ContainsPoint(int,int); //314e60
    TreeItem* mParent; // at +0x18, the parent item
    bool IsOpen() { return this->mOpen; } // inline
    int mX,mIndex; //1c/20
    bool mOpen; //24
    SmartPtr<ListItem> mItem; // at +0x28, the wrapped item
    std::vector<SmartPtr<TreeItem> > mChildren; // at +0x2c..0x3b, Dinkum vector (16 bytes)
};
class TreeItemOpenedEvent : public ComponentEvent {
public:
    TreeItemOpenedEvent(Component* inComponent,TreeItem* inItem)
        :ComponentEvent(inComponent,ComponentEvent_TreeItemOpened),mItem(inItem) {}
    // Implicit destructor (0x314a60): no own vptr write.
    SmartPtr<TreeItem> mItem; // at +0x20; the event is 0x24 bytes
};
// TreeArea (0x174 bytes); constructor 0x314ea0.
class TreeArea : public ListArea {
public:
    TreeArea(); // 0x314ea0: sets up the root item, open state and list flags
    // Implicit virtual destructor (0x314fc0)
    virtual void MouseDown(int,int,int); // 0x315260 (forwarder)
    virtual void HandleComponentEvent(ComponentEvent*); //315280
    virtual void NotifyMouseItemGone(); //315060
    virtual bool Sort(); // 0x315050 (returns false)
    void SetSortType(ListSortType); //315030
    void Sort(TreeItem*,bool=false); // sorts the visible subtree (not identified in the original exe)
    TreeItem* InsertString(TreeItem*,const GUIString&,Image* =NULL,int=-1);
    void CloseItem(TreeItem*,bool=false,bool=true);
    void SetTreeFlags(int,bool);
    TreeItem* InsertTreeItem(TreeItem*,ListItem*,int=-1); //315080
    void RemoveTreeItem(TreeItem*); //315190
    void ClearTree(bool=true); //3151a0
    void RemoveAllItems(TreeItem*); //3151b0
    void OpenItem(TreeItem*,bool=true); //315220
    void ToggleItem(TreeItem*,bool=false,bool=true); //315240
    bool TreeFlagSet(int flags) { return (this->mTreeFlags&flags)!=0; }
    SmartPtr<TreeItem> mRoot; //164
    int mTreeFlags; //168
    ListItem* mMouseColItem; // at +0x16c, the item under the mouse
    int mTreeSortType; //170
};

// PopupMenu (0x180 bytes): ListArea (0x164) plus two SmartPtrs, four ints and a bool.
// Constructor 0x2fd150 (57 vtable slots).
// The implicit destructor (0x2fd340) releases the SmartPtrs in reverse order, then ListArea's members.
class PopupMenu : public ListArea {
public:
    PopupMenu(); // 0x2fd150: default configuration
    // Implicit virtual destructor (0x2fd340)
    virtual void MouseDown(int,int,int); //2fdb70
    virtual void MouseUp(int,int,int);   //2fdc10
    virtual void MouseMove(int,int); //2fdb10
    virtual bool TimerEvent(int); //2fd820
    virtual void HandleComponentEvent(ComponentEvent*); //2fd7d0
    virtual void ListSelChangedHook(); //2fd860
    MenuItem* AddItem(const GUIString&,int); //2fd440,ret8
    MenuItem* AddItem(const GUIString&,ComponentListener*,int); // 0x2fd460 (ret 0x0c)
    MenuItem* AddItem(const GUIString&,NativeImage*,ComponentListener*,int); //2fd480,ret10
    void AddSeperator(); //2fd8b0,ret0
    void EndSubMenu(); // 0x2fd580: closes the submenu
    void CheckSubMenu(); // 0x2fd610: opens the submenu
    bool TryKeyDown(int); // passes keys to the submenu recursively
    void Reset(); // 0x2fd400: closes the submenu, then clears the selected index
protected:
    SmartPtr<PopupMenu> mCurSubMenu; // at +0x164
    SmartPtr<ListItem> mCurSubMenuItem; // at +0x168
    int mMouseUpCount; //16c
    int mTimerAcc; //170
    int mLastX,mLastY; //174,178
    bool mKeyboardMode; // at +0x17c; +0x17d..0x17f padding; the class is 0x180 bytes; the constructor leaves mLastX/Y unset
};

// Menu bar classes (not identified in the original exe).
enum MenuBarState { MenuBarState_None,MenuBarState_Up,MenuBarState_Down };
class MenuBarItem : public Component {
protected:
    SmartPtr<PopupMenu> mPopup;
    SmartPtr<Font> mFont;
    GUIString mText;
    MenuBarState mState;
public:
    virtual void Paint(Graphics&);
    MenuBarItem();
    void SetMenuBarState(MenuBarState);
    void SetText(const GUIString&);
    void SetPopup(PopupMenu*);
    PopupMenu* GetPopup() { return this->mPopup.get(); }
};
class MenuItemSelectedEvent : public ComponentEvent {
public:
    SmartPtr<MenuItem> mItem;
    virtual void Deliver();
    MenuItemSelectedEvent(Component* menu,MenuItem* item)
      :ComponentEvent(menu,ComponentEvent_MenuItemSelected),mItem(item) {}
};
class MenuBar : public Container {
protected:
    typedef std::vector<MenuBarItem*> ItemVector;
    ItemVector mItemVector;
    MenuBarState mState;
    MenuBarItem* mCurItem;
    int mLastX,mLastY;
    Background mBackground;
    void CheckItem(Component*);
public:
    virtual void MouseDrag(int,int);
    virtual void MouseMove(int,int);
    virtual void MouseDown(int,int,int);
    virtual void MouseUp(int,int,int);
    virtual void MouseExit();
    virtual bool KeyDown(int);
    virtual void HandleComponentEvent(ComponentEvent*);
    virtual void Paint(Graphics&);
    MenuBar();
    void AddItem(const GUIString&,PopupMenu*);
    void AddSeperator(Component* =NULL);
};

class ScrollAreaViewport : public Container {
public:
    ScrollAreaViewport(ScrollArea* area) : mScrollArea(area) {} // inline (0x304e70..0x304e83)
    void Reset(); // 0x305c70: resets the contents and the popup
    void InvalidateIfRectVisible(WONRectangle&); // 0x305c10
    // Implicit virtual destructor (0x306a60)
    ScrollArea* mScrollArea; // at +0x84, back link; the class is 0x88 bytes (allocated at 0x304e49)
    virtual void Paint(Graphics&); // vtable +0x18, 0x305e00
    virtual bool TimerEvent(int); // vtable +0x44, 0x305dd0
    virtual void SetCursor(Cursor*); // vtable +0x78, 0x305de0
    virtual void SetFocusChild(Component*); // vtable +0xb4, 0x305db0
    virtual void AddTimerChild(Component*); // vtable +0xb8, 0x305dc0
    virtual void ChildStateChanged(Component*); // vtable +0xbc, 0x2fa500
    virtual bool ChildInvalidated(Component*); // vtable +0xc0, 0x305d70
};

class MultiListItem : public ListItem {
protected:
    typedef std::vector<SmartPtr<ListItem> > ItemVector;
    ItemVector mItems; //18, allocator18/first1c/end20/cap24
    ListItem* GetMouseItem(ListArea*,int&);
    ListItem* GetMouseItem(ListArea*,int&,int&);
public:
    // Implicit default constructor (0x3fa110).
    // Implicit virtual destructor (0x303db0).
    void InsertItem(ListItem*); // appends an owned cell
    void SetItem(ListItem*,int); // 0x302480: sets a cell, growing the row
    ListItem* GetItem(int) const; // 0x302500
    virtual void Paint(Graphics&,ListArea*);
    virtual void CalcDimensions(ListArea*);
    virtual void SetViewport(ListArea*,Container*);
    virtual int Compare(const ListItem*,ListArea*) const;
    virtual bool CanSelect(ListArea*,int,int);
    virtual void SetPos(int,int);
    virtual bool Contains(int,int inY) { return inY>=this->mTop && inY<this->mTop+this->mHeight; }
    virtual void MouseEnter(ListArea*,int,int);
    virtual void MouseExit(ListArea*);
    virtual void MouseMove(ListArea*,int,int);
    virtual void MouseDrag(ListArea*,int,int);
    virtual void MouseDown(ListArea*,int,int,int);
    virtual void MouseUp(ListArea*,int,int,int);
};

// MultiListArea (0x1a4 bytes); constructor and destructor out of line.
class MultiListHeader;
class MultiListArea : public ListArea {
public:
    MultiListArea(int=1); // 0x3033a0 (ret 4)
    virtual void NotifyMouseItemGone(); // 0x3035d0 (vtable +0xd8)
    void SetNumColumns(int); //303600/393
    void SetColumnSizeMode(int,unsigned char); // partner-column size mode (not identified in the original exe)
    MultiListItem* InsertRow(int=-1); // inserts a blank row
    void SetColumnWidth(int,int,bool); //3037e0/382
    // Implicit virtual destructor (0x303540)
    int GetColumnPaintContext() { return this->mColumnPaintContext; }
    int GetSortColumn() { return this->mSortColumn; }
    struct ColumnInfo {
        SmartPtr<ScrollAreaViewport> mContainer;
        int mx, mWidth;
        ListArea::SortFunc mSortFunc;
        unsigned char mSizeMode;
        int mPartnerColumn;
        unsigned char mHAlign;
        ColumnInfo() : mx(0),mWidth(0),mSortFunc(NULL),mSizeMode(1),mPartnerColumn(-1),mHAlign(0) {} // default values (0x303627..0x303652)
    }; // 0x1c bytes; SetNumColumns resizes this vector
    std::vector<ColumnInfo> mColumnInfoVector; //164..173
    ListArea::SortFunc mSortColFunc; //174
    int mColumnPaintContext; //178
    int mTotalColumnWidth; //17c
    int mSortColumn; //180
    int mSortColumnPrev; //184
    int mColumnBorderWidth; //188
    int mDynamicSizeColumn; //18c
    bool mAutoGrowColumns; //190
    ListItem* mMouseColItem; //194, borrowed
    int mMouseColNum; //198
    int mMaxTotalColumnWidth; // at +0x19c (0x3038ac)
    MultiListHeader* mListHeader; // at +0x1a0 (0x3035f0/0x303b24)
    void NotifyListHeader(); // 0x3035f0
    void SetSortColumn(int,ListSortType); //303ac0
    void SetSortColumnFunc(int,SortFunc); // 0x303b50
    void SetColumnHAlign(int,HorzAlign); // 0x303790
    virtual void AdjustPosition(bool); // 0x303bb0
    virtual void Virtual08(); //303be0 SizeChanged
    virtual void ScrollPaint(Graphics&); //303c70 ScrollPaint
    MultiListItem* UpdateItem(MultiListItem*); //303e50
    MultiListItem* SetCell(int,int,ListItem*); //303ee0
    int GetColumnAt(int); //3039c0
    int GetColumnPos(int); //303a60
    int GetColumnWidth(int); //303960
    MultiListItem* InsertItem(MultiListItem*, int = -1); //303e30
    MultiListItem* SetString(int, int, const GUIString&, Image* = NULL); //303f60
};

// MultiListHeader (0xac bytes, Container's 51 vtable slots); fields as set at 0x302a90.
// type descriptor 0x4d64a0, COL 0x476030, vtable 0x44f170; ten overrides,
// the other 41 slots are Container's.
class MultiListHeader : public Container {
public:
    MultiListHeader(MultiListArea* =NULL); // 0x302a90 (ret 4)
    // Implicit virtual destructor (0x302b50)
    virtual void Virtual08(); //08=302fb0,SizeChanged
    virtual void Paint(Graphics&); //18=305e00
    virtual void MouseExit(); //28=303390,MouseExit
    virtual void MouseDown(int,int,int); //2c=303260,MouseUp
    virtual void MouseUp(int,int,int);   //30=303350,MouseDown
    virtual void MouseMove(int,int); //34=303290,MouseMove
    virtual void MouseDrag(int,int); //38=3032f0,MouseDrag
    virtual void HandleComponentEvent(ComponentEvent*); //7c=302be0
    virtual Component* FindChildAt(int,int); //ac=304710,FindChildAt
    void SetColumnItem(int,Component*); //302cf0/283,ret8
    void SetMultiListArea(MultiListArea*); // 0x302bc0 (ret 4)
    void NotifyColumnChange(); // 0x302e10
    void SetSortColumn(int,ListSortType); // 0x302cb0
    static ListSortType GetSortTypeForButton(Component*); // 0x303050
protected:
    struct ColumnInfo {
        Component* mComponent; // the Container owns the child
        ListSortType mSortType;
        ColumnInfo(Component* c=NULL) : mComponent(c),mSortType(ListSortType_None) {} // default used when resizing (0x302d1c..0x302d29)
    }; // 8-byte value; no owning pointer
    typedef std::vector<ColumnInfo> ColumnVector;
    ColumnVector mColumnVector; // at +0x84: allocator +0x84, first +0x88, end +0x8c, capacity +0x90
    MultiListArea* mListArea; // at +0x94, back link (set by 0x302bc0)
    int mDragColumn; //98
    Component* mDragComponent; //9c borrowed
    int mDragX; //a0
    int mCurSortCol; //a4
    bool mDrawSortArrow; // at +0xa8; +0xa9..0xab padding; the class is 0xac bytes
    Component* GetDragComponent(int); // 0x3030b0
public:
    int GetColumnIndex(Component*); // 0x302fd0
    ListSortType GetSortType(int); // 0x303010
};


struct GUIStringLessNoCase {
    bool operator()(const GUIString& inLeft, const GUIString& inRight) const
    { return inLeft.CompareNoCase(inRight) < 0; }
};

// Table (0x140 bytes, vtable 0x4501d0, 54 slots); ScrollArea (0x110) base.
// Nothing here constructs, copies or destroys a Table.
class Table : public ScrollArea {
public:
    Table(LineSegment* =NULL); // 0x30f9d0
    // Implicit destructor (0x30fbb0, scalar deleting 0x30fad0).
    virtual void MouseEnter(int,int); //3100b0 MouseEnter
    virtual void MouseExit(); //3100e0 MouseExit
    virtual void MouseDown(int,int,int); // 0x310100
    virtual void MouseUp(int,int,int);   //310250 MouseUp
    virtual void MouseMove(int,int); //3102b0 MouseMove
    virtual void MouseDrag(int,int); //310300 MouseDrag
    virtual bool TimerEvent(int); //310450 TimerEvent
    virtual bool KeyChar(int);   //30fdc0 KeyChar
    virtual bool KeyDown(int);   //30fe00 KeyDown
    virtual Component* FindClickChildAt(int,int); //310040 WMFindChildAt
    virtual void ScrollSizeChanged(); //310660 ScrollSizeChanged
    virtual void ScrollPaint(Graphics&); //310670 ScrollPaint
    void MouseAt(int&,int&,bool=false); // 0x30ff20 (ret 0x0c)
    void CopySelection(); //30fce0
    void SetContents(LineSegment*); //310500
    void SetMaxWidth(bool=true); // 0x3105b0
    void AddAnchor(const GUIString&,int,int); //310700
    bool GotoAnchor(const GUIString&); //310730
    bool FindStr(const GUIString&,bool=true,bool=false); // 0x3107b0 (ret 0x0c)
    SmartPtr<LineSegment> mContents; // at +0x110
    LineSegment* mMouseSegment; // at +0x114, the segment under the mouse
    int mStartSelSegment,mEndSelSegment; //118,11c
    int mStartSelSegmentPos,mEndSelSegmentPos; //120,124
    bool mBackwardsSel,mSelecting; // at +0x128, +0x129; +0x12a..0x12b padding
    int mMouseSegmentDX,mMouseSegmentDY; //12c,130
    struct AnchorCoordinate {
        int mx,my;
        AnchorCoordinate(int x=0,int y=0) : mx(x),my(y) {}
    };
    typedef std::map<GUIString,AnchorCoordinate,GUIStringLessNoCase> AnchorMap;
    AnchorMap mAnchorMap; // at +0x134: allocator/compare bytes, head +0x138, size +0x13c (0x0c bytes)
};
class HTMLTag;
class TextLinkParam : public RefCount {
public:
    GUIString mLinkParam; //08
    std::list<LineSegment*> mLinkList; // at +0x0c..0x17, line pointers (not owned)
    bool mLinkIsDown,mLaunchBrowser; // at +0x18, +0x19; the class is 0x1c bytes
    void LinkDown(); //30ab50
    bool LinkUp(); //30ab80
    TextLinkParam(const GUIString& p,bool launch) : mLinkParam(p),mLinkIsDown(false),mLaunchBrowser(launch) {}
};
// HyperLinkEvent (0x24 bytes), allocated at 0x30ac35/0x30adbd/0x30ae5f.
// Inline constructor; keeps the link parameter at +0x20 after ComponentEvent (0x20 bytes).
class HyperLinkEvent : public ComponentEvent {
public:
    HyperLinkEvent(Component* component,int type,TextLinkParam* param)
        :ComponentEvent(component,type),mLinkParam(param) {} // 0x30ac51..0x30ac82
    SmartPtr<TextLinkParam> mLinkParam; // at +0x20
};
// Keyboard.h modifiers; the exe tests bits 1 and 2.
enum { KEYMOD_SHIFT=0x0001,KEYMOD_ALT=0x0002,KEYMOD_CTRL=0x0004 };
class ImageMap : public RefCount {
public:
    struct ShapeLink {
        SmartPtr<TextLinkParam> mLinkParam;
        WONRectangle mShape;
        ShapeLink(TextLinkParam* p,WONRectangle& r) : mLinkParam(p),mShape(r) {}
    }; // 0x14-byte value: a SmartPtr (4) and a rectangle (0x10)
    std::list<ShapeLink> mShapeList; // at +0x08..0x13
    TextLinkParam* GetLink(int,int); // 0x30aed0
    void AddLink(TextLinkParam* p,WONRectangle& r) { this->mShapeList.push_back(ShapeLink(p,r)); } // inline
};
enum HTMLListType
{
	HTMLListType_None,
	HTMLListType_Disc,
	HTMLListType_Circle,
	HTMLListType_Square,
	HTMLListType_Digit,
	HTMLListType_LowerLetter,
	HTMLListType_UpperLetter,
	HTMLListType_LowerRoman,
	HTMLListType_UpperRoman
};
class HTMLBodyTag;
class HTMLImageTag;
class HTMLInputTag;
class HTMLImageMapTag;
class HTMLAreaTag;
class HTMLListTag;
class HTMLListItemTag;
// HTMLDocument (0x80 bytes); constructor and destructor out of line.
// Constructor 0x33c610; allocated by the parser at 0x34461e.
class HTMLDocument : public RefCount {
protected:
    GUIString mBasePath; //08
    Window* mWindow; //0c borrowed
    bool mDelayImages; //10
    std::list<SmartPtr<HTMLTag> > mTagList; //14..1f
    std::list<SmartPtr<HTMLTag> >::iterator mTagItr,mBodyStart,mBodyEnd; //20,24,28
public:
    struct LoadedImageStruct {
        SmartPtr<RawImage> mRawImage; //00
        SmartPtr<NativeImage> mImage; //04
        SmartPtr<Animation> mAnimation; //08
        bool mHaveUsedAnimation; // at +0x0c; the struct is 0x10 bytes
        LoadedImageStruct(NativeImage* =NULL,Animation* =NULL); // out of line
    };
    typedef std::map<GUIString,LoadedImageStruct,GUIStringLessNoCase> LoadedImageMap;
protected:
    LoadedImageMap mLoadedImageMap,mScaledImageMap; //2c,38
    bool mDecodingImages; //44
    std::list<SmartPtr<HTMLTag> > mImageTagList; //48..53
    SmartPtr<ImageMap> mCurImageMap; //54
    typedef std::map<GUIString,SmartPtr<ImageMap>,GUIStringLessNoCase> ImageMapMap;
    ImageMapMap mImageMapMap; //58..63
    struct ListInfo { bool mOrdered; HTMLListType mType; int mCount; }; //0c
    std::vector<ListInfo> mListStack; //64..73
    typedef std::multimap<GUIString,SmartPtr<HTMLTag>,GUIStringLessNoCase> ImageTagMap;
    ImageTagMap mImageTagMap; //74..7f
    LoadedImageStruct* GetImage(const GUIString&,HTMLTag*); //33d010
    NativeImage* CheckScaleImage(LoadedImageStruct*,const GUIString&,int,int); //33d560
    void HandleBody(HTMLBodyTag*); // 0x33e410
    void HandleImage(HTMLImageTag*); // 0x33d9e0
    void HandleInput(HTMLInputTag*); // 0x33dfa0
    void HandleImageMap(HTMLImageMapTag*); // 0x33e020
    void HandleArea(HTMLAreaTag*); // 0x33e270
    void HandleList(HTMLListTag*); // 0x33e680
    void HandleListItem(HTMLListItemTag*); // 0x33e7b0
public:
    std::list<SmartPtr<HTMLTag> >& GetTagList() { return this->mTagList; } // inline (0x33c43e)
    HTMLDocument(const GUIString&); // 0x33c610
    void Clear(); // 0x33cbd0
    void DecodeImages(bool); //33edb0
    bool DoDecodeImage(LoadedImageStruct*,Window*,const std::string&); // 0x33ce10
    LoadedImageMap& GetLoadedImageMap() {return this->mLoadedImageMap;}
    virtual ~HTMLDocument(); // 0x33c780 (scalar deleting)
    void AddTag(HTMLTag*); //33ebd0
    void AddDocument(HTMLDocument*); //33eb80
    void Rewind(bool=true); // (not identified in the original exe)
    HTMLTag* GetNextTag(bool=true); // (not identified in the original exe)
    void SetBasePath(const GUIString&); //33ccf0
    std::list<SmartPtr<HTMLTag> >::iterator GetBodyStart(); // 0x33ef70
    std::list<SmartPtr<HTMLTag> >::iterator GetBodyEnd(); // 0x33efa0
};
// SDK enum; the Browser constructor stores Medium (2) at +0x8c.
enum HTMLFontSize {
    HTMLFontSize_Smallest,HTMLFontSize_Small,HTMLFontSize_Medium,
    HTMLFontSize_Large,HTMLFontSize_Largest
};
class HTTPCache;
class HTTPSession;
class BrowserComponent : public Container {
public:
    BrowserComponent(); // 0x3fad10; the class is 0xe0 bytes
    void SetTransparent(bool); // 0x3fd210 (older SDK API)
    virtual ~BrowserComponent(); // destructor 0x3faf30, deleting 0x3faf10; 51 vtable slots
    virtual void AddedToParent(); //AddedToParent3fb0e0, no args
    virtual void HandleComponentEvent(ComponentEvent*); //3fd350, ret4
    bool StopButtonEnabled(); //3fb230
    bool BackButtonEnabled(); //3fb250
    bool ForwardButtonEnabled(); //3fb280
    void Reset(); //3fc480
    void Refresh(); //3fc4e0
    void Stop(); //3fc510
    void Go(HTMLDocument*); //3fc590, ret4
    void Go(const GUIString&,bool=false,bool=false,int=-1); //3fc800, ret10
    void Back(); //3fcf60
    void Forward(); //3fcfc0
    bool FindStr(const GUIString&,bool=true,bool=false); //3fd1f0, ret0c
    Table* GetHTMLView() { return this->mHTMLView.get(); }
    void SetScroller(Scroller*); // 0x3fb1a0: sets the scroller and lays it out
    const GUIString& GetCurStatus() { return this->mCurStatus; }
    void SetFontSize(HTMLFontSize size) { this->mFontSize=size; }
    const GUIString& GetCurLocation() { return this->mCurLocation; }
    void SetLaunchBrowserOnLink(bool inLaunch) { this->mLaunchBrowserOnLink=inLaunch; }
    // Table, scroller, font size, cache, session, document and background members follow.
    SmartPtr<Table> mHTMLView; // at +0x84
    SmartPtr<Scroller> mScroller; // at +0x88 (0x3fb1a9..0x3fb1e5)
    HTMLFontSize mFontSize; // at +0x8c, a 32-bit enum, default 2 = Medium
    SmartPtr<HTTPCache> mHTTPCache; // at +0x90
    SmartPtr<HTTPSession> mHTTPSession; // at +0x94
    SmartPtr<HTMLDocument> mHTMLDocument; // at +0x98
    bool mHaveBackground; // at +0x9c; +0x9d..0x9f padding
    GUIString mCurStatus; //a0, GetEmpty+SmartPtr ctor/dtor and Label consumer closed
    GUIString mCurPath; //a4
    GUIString mCurAnchor; //a8
    std::string mHTMLDocumentPath; //ac
    bool mLaunchBrowserOnLink; //bc, byte ctor/read and consumers write true
    int mNumPendingOps; // at +0xc0; +0xbd..0xbf padding before it
    int mVertOffset; //c4
    struct PageLocation {
        GUIString mLocation; // at +0x00
        int mVertOffset; // at +0x04; the struct is 8 bytes
        PageLocation() :mVertOffset(0) {} // inline
        PageLocation(const GUIString& location,int offset) :mLocation(location),mVertOffset(offset) {}
    };
    typedef std::vector<PageLocation> PageVector;
    PageVector mPageVector; // at +0xc8; data +0xcc, end +0xd0, capacity +0xd4; 8-byte elements
    int mCurPage; //d8
    GUIString mCurLocation; //dc
protected:
    void GotoAnchor(); //3fc540
    void RememberVertOffset(); //3fc7c0
    void GetLinkPath(const std::string&,std::string&); //3fd030
    void SetStatus(const GUIString&); // 0x3fb2c0
    void FinishImages(); //3fb6a0
    struct ImageParam : public RefCount {
        SmartPtr<BrowserComponent> mBrowser; // at +0x08
        HTMLDocument::LoadedImageStruct* mImageStruct; // at +0x0c; the struct is 0x10 bytes
        ImageParam(BrowserComponent* browser,HTMLDocument::LoadedImageStruct* image) :mBrowser(browser),mImageStruct(image) {}
    };
    typedef SmartPtr<ImageParam> ImageParamPtr;
    static void DownloadImageCompletion(AsyncOpPtr,ImageParamPtr); // 0x3fb930 (cdecl, arguments by value)
    bool DownloadImages(); // 0x3fbc10
    void ParseDocument(); // 0x3fc0d0
    static void DownloadDocumentCompletion(AsyncOpPtr,SmartPtr<RefCount>); // 0x3fb320 (cdecl, two SmartPtrs by value)
    // Only the 0xe0-byte prefix is declared; nothing here constructs or copies one.
};
// Plain value (0x14 bytes, no vtable): constructor 0x33efc0, implicit destructor 0x3c0fb0.
class HTMLDocumentGen {
public:
protected:
    bool AddDocPrv(const GUIString&,int,bool); //33f020
public:
    HTMLDocumentGen(HTMLDocument* =NULL);
    void SetDocument(HTMLDocument*); // 0x33f440: replaces the owned document
    bool AddDoc(const GUIString&,int=0); // 0x33f1f0 (forwarder)
    bool AddDocParam(const GUIString&,int=0); //33f210 ->AddDocPrv(...,true)
    void printf(const GUIString&); //33f230
    void AddTag(HTMLTag*); // (not identified in the original exe)
    void AddTagParam(HTMLTag*); // (not identified in the original exe)
    HTMLDocument* GetDocument() { return this->mDocument.get(); }
    SmartPtr<HTMLDocument> mDocument; //0
    std::vector<SmartPtr<HTMLDocument> > mDocumentVector; //4, allocator4/data8/endc/cap10
    // Implicit destructor.
};

// HTMLDisplayGen; its declaration is in WONLobbyHTMLDisplayGen.h.
class HTMLDisplayGen;

// 0x3c5ff0: the sort/filter handlers of a server-list column


// 0x8dec84 / 0x8dec88 / 0x8df4d0 / 0x8d954c: the lobby's object factories


}

#endif
