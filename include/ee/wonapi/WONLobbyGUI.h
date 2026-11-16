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
#include "ee/wonapi/WONLobbyMap.h"
#include "ee/wonapi/WONEventTypes.h"
#include "platform/PlatTime.h" // PlatSleep (WindowManager::GetNextEvent)

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
    Cursor();
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
    virtual Cursor* GetNative() { return this->mCursor->GetNative(); }
    virtual Component* GetComponent(int& x,int& y) { return this->mCursor->GetComponent(x,y); }
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
extern GUIString gEmptyGUIString;

// a GUI component (0x30 bytes: EELobbyColorChooser's members start at +0x30; vtable 0x84d71c, 43
// slots). Unknown slots are named by vtable offset. The inline bodies are the COMDATs the first lobby unit
// (EELobbyColorButton.cpp 0x29059d, 0x2905a2) keeps; the other tiny slots are ICF-folded into game code.
// WONRectangle (Rectangle.cpp folded in, R6 WONGUI B round 1): the intersection (empty size when they do not
// overlap), the union (an empty rectangle becomes a copy of the other) and the overlap test (touching this one's
// right/bottom edge counts) are ee_won's guilayout.rs
class WONRectangle {
public:
    WONRectangle() : x(0), y(0), width(0), height(0) {}
    WONRectangle(int inX, int inY, int inWidth, int inHeight) : x(inX), y(inY), width(inWidth), height(inHeight) {}
    void DoIntersect(const WONRectangle& o) { this->Set(ee_won_gui_rect_intersect(this->Rust(), o.Rust())); } // 0x349330
    void DoUnion(const WONRectangle& o) { this->Set(ee_won_gui_rect_union(this->Rust(), o.Rust())); } // 0x3493b0
    bool Intersects(const WONRectangle& o) const { return ee_won_gui_rect_intersects(this->Rust(), o.Rust()); }
    EeWonRect Rust() const { EeWonRect e = { this->x, this->y, this->width, this->height }; return e; }
    void Set(const EeWonRect& e) { this->x = e.x; this->y = e.y; this->width = e.width; this->height = e.height; }
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

// Component (R6 WON round 38): every method is Rust (rust/crates/ee_won/src/compobj.rs), the C++ keeping the members
// and the vtable; the empty defaults stay inline. The fonts are the root window's (ComponentLifetime.cpp).
extern "C" intptr_t ee_won_guicpp_comp_call(void*,EeWonGuiOp,intptr_t,intptr_t,intptr_t,intptr_t);
class Component : public RefCount {
public:
    Component();                            // 0x2f49e0 (WONAPI)
    virtual ~Component();                   // 0x2f4a70 (WONAPI)
    virtual void PositionChanged() {}
    virtual void Virtual08() { this->KCall(EeWonCompCall::SizeChanged); }
    virtual void GetDesiredSize(int& w, int& h) { this->KCall(EeWonCompCall::DesiredSize,(intptr_t)&w,(intptr_t)&h); }
    virtual bool NeedsPaint() { return this->KCall(EeWonCompCall::NeedsPaint)!=0; }
    virtual void PrePaint(Graphics& g) { this->KCall(EeWonCompCall::PrePaint,(intptr_t)&g); }
    virtual void Paint(Graphics& g) { this->KCall(EeWonCompCall::Paint,(intptr_t)&g); } // +0x18 (0x2f5020)
    virtual void InitComponent(ComponentInit& inInit) {} // empty inline (the vtable slot holds the shared ret 4 at 0x156dd8)
    virtual bool HitTest(int x, int y) { return this->KCall(EeWonCompCall::HitTest,x,y)!=0; }
    virtual void MouseEnter(int x, int y) { this->KCall(EeWonCompCall::MouseEnter,x,y); } // +0x24 (0x2f4cc0)
    virtual void MouseExit() { this->KCall(EeWonCompCall::MouseExit); } // +0x28 (0x2f4ce0)
    virtual void MouseDown(int,int,int) {} // empty inline (the vtable slot holds the shared 3-byte body at 0x3a6880)
    virtual void MouseUp(int inX, int inY, int inButton) {}    // +0x30 0x2907a3 (emitted with EELobbyColorChooser)
    virtual void MouseMove(int inX, int inY) {}                // +0x34 (the mouse moved); empty inline
    virtual void MouseDrag(int, int) {}
    virtual bool MouseWheel(int inAmount) { return false; }    // +0x3c 0x29059d
    virtual void MouseHover(unsigned long d) { this->KCall(EeWonCompCall::MouseHover,(intptr_t)d); }
    virtual bool TimerEvent(int) { return true; }
    virtual void GotFocus() { this->KCall(EeWonCompCall::GotFocus); }
    virtual void LostFocus() { this->KCall(EeWonCompCall::LostFocus); }
    virtual bool KeyChar(int inKey) { return false; }          // +0x50 (ICF: 0x29059d)
    virtual bool KeyDown(int inKey) { return false; }          // +0x54 (ICF: 0x29059d)
    virtual bool KeyUp(int inKey) { return false; }            // +0x58 (ICF: 0x29059d)
    virtual bool CanClose() { return true; }
    virtual void InvalidateRect(WONRectangle& r) { this->KCall(EeWonCompCall::InvalidateRect,(intptr_t)&r); }
    virtual void Invalidate() { this->KCall(EeWonCompCall::Invalidate); } // +0x64 (0x2f5110)
    virtual void MarkInvalid() { this->KCall(EeWonCompCall::MarkInvalid); }
    virtual void AddedToParent() { this->KCall(EeWonCompCall::AddedToParent); } // +0x6c (0x2f5160)
    virtual void ClearGrabBG() { this->KCall(EeWonCompCall::ClearGrabBG); }
    virtual void EnabledChanged(bool) {}
    virtual void SetCursor(Cursor* c) { this->KCall(EeWonCompCall::SetCursor,(intptr_t)c); }
    virtual void HandleComponentEvent(ComponentEvent* e) { this->KCall(EeWonCompCall::HandleEvent,(intptr_t)e); } // +0x7c (0x2f5550)
    virtual void HandleBroadcastEvent(ComponentEvent* e) { this->KCall(EeWonCompCall::HandleBroadcast,(intptr_t)e); }
    virtual void NotifyRootVisibility(bool v) { this->KCall(EeWonCompCall::NotifyRootVisibility,v); }
    virtual bool TabFocus(bool f, bool i) { return this->KCall(EeWonCompCall::TabFocus,f,i)!=0; }
    virtual void EnableControlsById(const ControlIdSet& ids, bool e) { this->KCall(EeWonCompCall::EnableControlsById,(intptr_t)&ids,e); }
    virtual void SetEnabled(bool e) { this->KCall(EeWonCompCall::SetEnabled,e); } // +0x90 (0x2f5610)
    virtual void SetVisible(bool v) { this->KCall(EeWonCompCall::SetVisible,v); } // +0x94 (0x2f55b0)
    virtual Component* GetMouseChild() { return NULL; }
    virtual int GetMinWidth() { return 0; } // returns 0 (shared body at 0x1ad67e)
    virtual int GetMinHeight() { return 0; } // returns 0 (shared body at 0x1ad67e)
    virtual int GetMaxWidth() { return 0xffff; }               // +0xa4 0x2905a2
    virtual int GetMaxHeight() { return 0xffff; }              // +0xa8 (ICF: 0x2905a2)

    void SetColorScheme(ColorScheme* s) { this->KCall(EeWonCompCall::SetColorScheme,(intptr_t)s); }
    ColorScheme* GetOwnColorScheme() { return (ColorScheme*)this->KCall(EeWonCompCall::OwnColorScheme); }
    ColorScheme* GetColorScheme() { return (ColorScheme*)this->KCall(EeWonCompCall::ColorScheme); }
    void SetPos(int x, int y) { this->KCall(EeWonCompCall::SetPos,x,y); }
    void SetSize(int w, int h) { this->KCall(EeWonCompCall::SetSize,w,h); }
    void SetSizeAtLeast(int w, int h) { this->KCall(EeWonCompCall::SetSizeAtLeast,w,h); }
    void SetPosSize(int x, int y, int w, int h) { int s[2]={w,h}; this->KCall(EeWonCompCall::SetPosSize,x,y,(intptr_t)s); }
    void FireBroadcastEvent(ComponentEvent* e, bool n) { this->KCall(EeWonCompCall::FireBroadcast,(intptr_t)e,n); }
    void BroadcastEventDown(ComponentEvent* e) { this->KCall(EeWonCompCall::BroadcastDown,(intptr_t)e); }
    void DeliverEvent(ComponentEvent* e, bool n) { this->KCall(EeWonCompCall::Deliver,(intptr_t)e,n); }
    SmartPtr<Font> GetFont(const FontDescriptor&); // 0x2f5a00: the root window's font
    Font* GetFontMod(Font* f, int s, int z) { return (Font*)this->KCall(EeWonCompCall::FontMod,(intptr_t)f,s,z); }
    Font* GetDefaultFont();
    Font* GetNamedFont(const std::string&); // 0x2f5b30: looks the font up in the window manager
    Font* GetDefaultFontMod(int inStyle, int inSize);
    void AddListener(ComponentListener* l) { this->KCall(EeWonCompCall::AddListener,(intptr_t)l); }
    void SetDesiredSize(int w, int h) { this->KCall(EeWonCompCall::SetDesiredSize,w,h); }
    void Fn2F5380(int inType, int inParam) { this->FireEvent(inType, inParam != 0); } // 0x2f5380: FireEvent's older name
    void FireEvent(int t, bool n=false) { this->KCall(EeWonCompCall::FireEvent,t,n); }
    bool IsRootVisible() { return this->KCall(EeWonCompCall::IsRootVisible)!=0; } // 0x2f5600 (flag bit 18)
    void LocalToRoot(int& x, int& y) { this->KCall(EeWonCompCall::LocalToRoot,(intptr_t)&x,(intptr_t)&y); }
    void BubbleEvent(ComponentEvent* e) { this->KCall(EeWonCompCall::Bubble,(intptr_t)e); }
    Component* GetNextSibling(bool f,bool w=false) { return (Component*)this->KCall(EeWonCompCall::NextSibling,f,w); }
    bool WantFocus() { return this->KCall(EeWonCompCall::WantFocus)!=0; }
    void RootToChild(int& x,int& y) { this->KCall(EeWonCompCall::RootToChild,(intptr_t)&x,(intptr_t)&y); }
    void GetMousePos(int& x,int& y) { this->KCall(EeWonCompCall::MousePos,(intptr_t)&x,(intptr_t)&y); }
    int GetKeyMod() { return (int)this->KCall(EeWonCompCall::KeyMod); }
    bool CheckDoubleClick(int b) { return this->KCall(EeWonCompCall::DoubleClick,b)!=0; }
    bool Contains(int x,int y) { return this->KCall(EeWonCompCall::Contains,x,y)!=0; } // 0x2f5080: in parent coordinates
    bool Intersects(Component* c) { return this->KCall(EeWonCompCall::Intersects,(intptr_t)c)!=0; }
    Cursor* GetDefaultCursor() { return (Cursor*)this->KCall(EeWonCompCall::DefaultCursor); }
    BackgroundGrabber* GetGrabBG() { return (BackgroundGrabber*)this->KCall(EeWonCompCall::GrabBG); }
    void AllocateExtraStuff();
    bool CheckGrabBG() { return this->KCall(EeWonCompCall::CheckGrabBG)!=0; }
    void EnsureOnTopOf(Component* c) { this->KCall(EeWonCompCall::EnsureOnTopOf,(intptr_t)c); }
    void SetDefaultCursor(Cursor* c) { this->KCall(EeWonCompCall::SetDefaultCursor,(intptr_t)c); }
    void SetDesiredHeight(int p) { this->KCall(EeWonCompCall::SetDesiredHeight,p); }
    void SetDesiredWidth(int p=0) { this->KCall(EeWonCompCall::SetDesiredWidth,p); }
    void SetTranslucentColor(int c,int l=4) { this->KCall(EeWonCompCall::SetTranslucentColor,c,l); }
    void SetComponentFlags(int f,bool on) { this->KCall(EeWonCompCall::SetFlags,f,on); }
    void SetWantGrabBG(bool inGrab) { this->SetComponentFlags(0x100,inGrab); }
    void SubscribeToBroadcast(bool on) { this->KCall(EeWonCompCall::Subscribe,on); }
    void RequestTimer(bool on) { this->KCall(EeWonCompCall::RequestTimer,on); }
    void RequestFocus() { this->KCall(EeWonCompCall::RequestFocus); }
    Window* GetRoot() { return (Window*)this->KCall(EeWonCompCall::Root); } // 0x2f5920, legacy nominal name; true SDKGetWindow
    RootContainer* GetRootContainer() { return (RootContainer*)this->KCall(EeWonCompCall::RootContainer); }
    Window* GetRootWindow() { return (Window*)this->KCall(EeWonCompCall::RootWindow); }
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
private:
    friend intptr_t ee_won_guicpp_comp_call(void*,EeWonGuiOp,intptr_t,intptr_t,intptr_t,intptr_t);
    intptr_t KCall(EeWonCompCall k,intptr_t a=0,intptr_t b=0,intptr_t c=0) { return ee_won_comp_call(this,ee_won_guicpp_comp_call,k,a,b,c); }
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
// ChildLayoutManager (R6 WON round 31): its instructions (held, in order) and layout points (held) are the Rust layout
// manager (rust/crates/ee_won/src/layoutobj.rs); a container owns it by pointer. The methods are forwarders
// (ChildLayouts.cpp is the host).
class ChildLayoutManager : public RefCount {
public:
    ChildLayoutManager();
    virtual ~ChildLayoutManager(); // the layout points, then the instructions, released (the original's members)
    void ExecuteInstructions(Component* c) { this->MCall(EeWonLayoutMgrCall::Execute,(intptr_t)c); }
    void AddInstruction(ChildLayoutInstructionBase*);
    void AddLayoutPoint(LayoutPoint* p) { this->MCall(EeWonLayoutMgrCall::AddLayoutPoint,(intptr_t)static_cast<Component*>(p)); }
    void RemoveComponent(Component* c) { this->MCall(EeWonLayoutMgrCall::RemoveComponent,(intptr_t)c); }
    void Clear() { this->MCall(EeWonLayoutMgrCall::Clear); }
    void EnableInstructions(int g, bool e) { this->MCall(EeWonLayoutMgrCall::EnableInstructions,g,e); }
    // Container::AddChildLayout's statements (the group, the first run, the hold)
    void AddChildLayout(ChildLayoutInstructionBase* instr, int id, Component* container);
    EeWonLayoutMgr* mMgr;
private:
    intptr_t MCall(EeWonLayoutMgrCall c,intptr_t a=0,intptr_t b=0) { return ee_won_lmgr_call(this->mMgr,c,a,b,0,0); }
};
// Container (R6 WON round 26): its children (held), mouse and focus children, timer children, focus ids, current
// cursor, flags and modal count are the Rust Container (rust/crates/ee_won/src/ctobj.rs); this is the type every
// component holds and the base of every screen, dialog, list and control group, with the child layout manager. Its
// virtuals and methods are forwarders; the calls the Rust makes are Container.cpp's host.
class Container;
// the container ops every container object's host falls back to (Container.cpp)
intptr_t ContainerHostCall(Container* t, EeWonGuiOp op, intptr_t a, intptr_t b, intptr_t c, intptr_t d);
class Container : public Component {
public:
    Container();                            // 0x2f5b70 (WONAPI, /GX)
    virtual ~Container();                   // 0x2f5e80 (WONAPI, /GX)
    // Overrides (vtable 0x44d7cc).
    virtual void Virtual08() { this->CCall(EeWonCtCall::SizeChanged); }
    virtual void GetDesiredSize(int& w, int& h) { this->CCall(EeWonCtCall::DesiredSize,(intptr_t)&w,(intptr_t)&h); }
    virtual void PrePaint(Graphics& g) { this->CCall(EeWonCtCall::PrePaint,(intptr_t)&g); }
    virtual void Paint(Graphics& g) { this->CCall(EeWonCtCall::Paint,(intptr_t)&g); }
    virtual bool HitTest(int x, int y) { return this->CCall(EeWonCtCall::HitTest,x,y)!=0; }
    virtual void MouseEnter(int x, int y) { this->CCall(EeWonCtCall::MouseEnter,x,y); }
    virtual void MouseExit() { this->CCall(EeWonCtCall::MouseExit); }
    virtual void MouseDown(int x, int y, int b) { this->CCall(EeWonCtCall::MouseDown,x,y,b); }
    virtual void MouseUp(int x, int y, int b) { this->CCall(EeWonCtCall::MouseUp,x,y,b); }
    virtual void MouseMove(int x, int y) { this->CCall(EeWonCtCall::MouseMove,x,y); }
    virtual void MouseDrag(int x, int y) { this->CCall(EeWonCtCall::MouseDrag,x,y); }
    virtual bool MouseWheel(int n) { return this->CCall(EeWonCtCall::MouseWheel,n)!=0; }
    virtual void MouseHover(unsigned long d) { this->CCall(EeWonCtCall::MouseHover,(intptr_t)d); }
    virtual bool TimerEvent(int d) { return this->CCall(EeWonCtCall::TimerEvent,d)!=0; }
    virtual void GotFocus() { this->CCall(EeWonCtCall::GotFocus); }
    virtual void LostFocus() { this->CCall(EeWonCtCall::LostFocus); }
    virtual bool KeyChar(int k) { return this->CCall(EeWonCtCall::KeyChar,k)!=0; }
    virtual bool KeyDown(int k) { return this->CCall(EeWonCtCall::KeyDown,k)!=0; }
    virtual bool KeyUp(int k) { return this->CCall(EeWonCtCall::KeyUp,k)!=0; }
    virtual bool CanClose() { return this->CCall(EeWonCtCall::CanClose)!=0; }
    virtual void MarkInvalid() { this->CCall(EeWonCtCall::MarkInvalid); }
    virtual void AddedToParent() { this->CCall(EeWonCtCall::AddedToParent); }
    virtual void ClearGrabBG() { this->CCall(EeWonCtCall::ClearGrabBG); }
    virtual void SetCursor(Cursor* c) { this->CCall(EeWonCtCall::SetCursor,(intptr_t)c); }
    virtual void HandleBroadcastEvent(ComponentEvent* e) { this->CCall(EeWonCtCall::HandleBroadcastEvent,(intptr_t)e); }
    virtual void NotifyRootVisibility(bool v) { this->CCall(EeWonCtCall::NotifyRootVisibility,v); }
    virtual bool TabFocus(bool f, bool i) { return this->CCall(EeWonCtCall::TabFocus,f,i)!=0; }
    virtual void EnableControlsById(const ControlIdSet& ids, bool e) { this->CCall(EeWonCtCall::EnableControlsById,(intptr_t)&ids,e); }
    virtual Component* GetMouseChild() { return (Component*)this->CCall(EeWonCtCall::GetField,(intptr_t)EeWonCtField::MouseChild); }
    // 0x2f6140 (ret 0x20): adds inChild laid out against inRef (EELobbyInterface 0x2972c6: the lobby screen, 3,
    // the root, 0, 0, 0, 0, 0; provisional name)
    void Fn2F6140(Component* inChild, int inFlags, Component* inRef, int inA, int inB, int inC, int inD, int inE);
    void AddChildLayout(ChildLayoutInstructionBase*,int=0); // 0x2f61d0: keeps the instruction; group id 0 runs first (the test reads the full int, the id is stored as a byte)
    void EnableLayoutInstructions(int inId, bool inEnabled);
    void Relayout();
    void EnterModal() { this->CCall(EeWonCtCall::EnterModal); } // 0x2f6210: counts the modal level and moves the focus
    void LeaveModal() { this->CCall(EeWonCtCall::LeaveModal); } // 0x2f6260: ends the modal state and moves the focus
    void RemoveAllChildren() { this->CCall(EeWonCtCall::RemoveAllChildren); }
    void UpdateMouseChild(int x,int y) { this->CCall(EeWonCtCall::UpdateMouseChild,x,y); }
    void MouseAt() { this->CCall(EeWonCtCall::MouseAt); } // 0x2f66c0: refreshes the mouse state
    void SetOnlyChildInput(bool v) { this->CCall(EeWonCtCall::SetField,(intptr_t)EeWonCtField::OnlyChildInput,v); }
    void SetIsolateTabFocus(bool v) { this->CCall(EeWonCtCall::SetField,(intptr_t)EeWonCtField::IsolateTabFocus,v); }
    void AddLayoutPoint(LayoutPoint*); // 0x2f6100: keeps the layout point as a child
    void AddChildEx(Component* c,int p) { this->CCall(EeWonCtCall::AddChildEx,(intptr_t)c,p); }
    void BringToTop(Component* c) { this->CCall(EeWonCtCall::BringToTop,(intptr_t)c); }
    void SetHasUsedDialogKeys(bool v) { this->CCall(EeWonCtCall::SetField,(intptr_t)EeWonCtField::HasUsedDialogKeys,v); }
    bool HasUsedDialogKeys() { return this->CCall(EeWonCtCall::GetField,(intptr_t)EeWonCtField::HasUsedDialogKeys)!=0; }
    Component* GetFocusChild() { return (Component*)this->CCall(EeWonCtCall::GetField,(intptr_t)EeWonCtField::FocusChild); }
    Cursor* GetCurrentCursor() { return (Cursor*)this->CCall(EeWonCtCall::GetField,(intptr_t)EeWonCtField::CurrentCursor); }
    bool IsModal() { return this->CCall(EeWonCtCall::GetField,(intptr_t)EeWonCtField::ModalCount)>0; }
    Component* GetFrontChild() { return (Component*)this->CCall(EeWonCtCall::GetField,(intptr_t)EeWonCtField::FrontChild); }
    Component* GetBackChild() { return (Component*)this->CCall(EeWonCtCall::GetField,(intptr_t)EeWonCtField::BackChild); }
    // Component::GetNextSibling's walk (0x2f5230)
    Component* NextChild(Component* c,bool f,bool w) { return (Component*)this->CCall(EeWonCtCall::NextChild,(intptr_t)c,f,w); }
    void DropFocusChild() { this->CCall(EeWonCtCall::DropFocusChild); }
    unsigned char GetNextFocusId() { return (unsigned char)this->CCall(EeWonCtCall::NextFocusId); }
    void SetFocusToLastFocusChild() { this->CCall(EeWonCtCall::SetFocusToLastFocusChild); }
    void EnsureOnTopOf(Component* b,Component* t) { this->CCall(EeWonCtCall::EnsureOnTopOf,(intptr_t)b,(intptr_t)t); }
    void AddChild(Component* c) { this->CCall(EeWonCtCall::AddChild,(intptr_t)c); }
    virtual Component* FindChildAt(int x, int y) { return (Component*)this->CCall(EeWonCtCall::FindChildAt,x,y); }
    virtual Component* FindClickChildAt(int inX, int inY) { return this->FindChildAt(inX, inY); }
    virtual void SetFocusChild(Component* c) { this->CCall(EeWonCtCall::SetFocusChild,(intptr_t)c); }
    virtual void AddTimerChild(Component* c) { this->CCall(EeWonCtCall::AddTimerChild,(intptr_t)c); }
    virtual void ChildStateChanged(Component* c) { this->CCall(EeWonCtCall::ChildStateChanged,(intptr_t)c); }
    virtual bool ChildInvalidated(Component* c) { return this->CCall(EeWonCtCall::ChildInvalidated,(intptr_t)c)!=0; }
    virtual void RemoveChild(Component* c) { this->CCall(EeWonCtCall::RemoveChild,(intptr_t)c); }
    virtual Component* FindChildById(int id) { return (Component*)this->CCall(EeWonCtCall::FindChildById,id); }

    EeWonContainer* mCt;
    ChildLayoutManager* mChildLayout; // owned: destroyed between the cursor and the timer children, as the member was
protected:
    static bool mMouseWheelGoesToFocusChildDef; // 0x566234 (declaration only)
    intptr_t CCall(EeWonCtCall c,intptr_t a=0,intptr_t b=0,intptr_t cc=0) { return ee_won_ct_call(this->mCt,c,a,b,cc); }
};

// ScreenContainer (0x88 bytes); constructor 0x306a20, destructor 0x306a60; points at the current screen.
class ScreenContainer : public Container {
public:
    ScreenContainer() : mCurScreen(NULL) {}
    // Implicit destructor (0x306a60) jumps to Container's at 0x2f5e80; the screen at +0x84 is not owned.
    // 0x306ab0: hides the current screen and shows the given one, optionally focused; nothing if it is current
    void ShowScreen(Component* inScreen, bool inTakeFocus = true)
    {
        if (inScreen == this->mCurScreen) return;
        if (this->mCurScreen != NULL) this->mCurScreen->SetVisible(false);
        inScreen->SetVisible(true);
        this->mCurScreen = inScreen;
        if (inScreen != NULL && inTakeFocus) inScreen->RequestFocus();
        this->Invalidate();
    }
    // 0x306a70: adds a hidden screen laid out to the container's size
    void AddScreen(Component* inScreen) { inScreen->SetVisible(false); this->Fn2F6140(inScreen, 3, this, 0, 0, 0, 0, 0); this->AddChild(inScreen); }
    Component* mCurScreen; //84, borrowed
};


// Child layout instructions (six concrete classes below)
// The child layout instructions (R6 WON round 31): each class's numbers, its component list, its group and its enabled
// flag are one Rust instruction (rust/crates/ee_won/src/layoutobj.rs, `EeWonLayoutKind` the class); the C++ classes
// are the reference-counted shells the lobby makes, their constructors handing the numbers over. Only the classes
// below derive from the base, and none overrides Execute or UsesComponent: the Rust runs each class's statements.
class ChildLayoutInstructionBase : public RefCount {
public:
    ChildLayoutInstructionBase() : mLay(NULL) {} // a subclass makes its Rust instruction (Make)
    virtual ~ChildLayoutInstructionBase();
    virtual void Execute(Component* p) { if (this->mLay) ee_won_layout_call(this->mLay,EeWonLayoutCall::Execute,(intptr_t)p,0); } // 0x30a800 (empty in the base)
    virtual bool UsesComponent(Component* c) { return this->mLay && ee_won_layout_call(this->mLay,EeWonLayoutCall::UsesComponent,(intptr_t)c,0)!=0; } // 0x3a90d0 (false in the base)
    EeWonLayout* mLay;
protected:
    // a new instruction of the class: two references (components), three ints and the two flags
    void Make(EeWonLayoutKind k,Component* r0,Component* r1=NULL,int i0=0,int i1=0,int i2=0,bool h=false,bool v=false);
    void Make(const EeWonLayoutInit& init);
    void AddComponent(Component* c) { ee_won_layout_call(this->mLay,EeWonLayoutCall::Add,(intptr_t)c,0); }
};
class CenterLayout : public ChildLayoutInstructionBase {
public:
    CenterLayout(Component* box,bool horz,bool vert) { this->Make(EeWonLayoutKind::Center,box,NULL,0,0,0,horz,vert); } // inline (0x388f6d..0x388fb3); the class is 0x20 bytes
    CenterLayout(Component* box,Component* first,bool horz,bool vert) { this->Make(EeWonLayoutKind::Center,box,NULL,0,0,0,horz,vert); this->Add(first); } // inline (0x3624e8..0x362530)
    CenterLayout* Add(Component* component) { this->AddComponent(component); return this; }
    static void Center(Component* inParent, Component* inChild, bool inHorz, bool inVert); // 337690
};

// a modal or popup dialog (0x98 bytes in the original: EELobbyConnectDialog's members start at +0x98).
// Dialog (R6 WON round 26): its drag, Escape result and default button are the Rust Dialog
// (rust/crates/ee_won/src/dialogobj.rs) over the container's Rust object; this is the type the lobby's dialogs derive
// from, with one-line forwarders (DialogBehavior.cpp is its host).
class Dialog : public Container {
public:
    Dialog();                               // 0x2f7910 (WONAPI)
    virtual ~Dialog();
    virtual void Virtual08() { this->DCall(EeWonDlgCall::SizeChanged); }
    virtual void MouseExit() { this->DCall(EeWonDlgCall::MouseExit); }
    virtual void MouseDown(int x, int y, int b) { this->DCall(EeWonDlgCall::MouseDown,x,y,b); }
    virtual void MouseUp(int x, int y, int b) { this->DCall(EeWonDlgCall::MouseUp,x,y,b); }
    virtual void MouseDrag(int x, int y) { this->DCall(EeWonDlgCall::MouseDrag,x,y); }
    virtual void GotFocus() { this->DCall(EeWonDlgCall::GotFocus); }
    virtual void LostFocus() { this->DCall(EeWonDlgCall::LostFocus); }
    virtual bool KeyDown(int k) { return this->DCall(EeWonDlgCall::KeyDown,k)!=0; }
    virtual bool CanClose() { return this->DCall(EeWonDlgCall::CanClose)!=0; }
    void SetEndOnEscape(int id) { this->DCall(EeWonDlgCall::SetEndOnEscape,id); }
    void SetDontEndOnEscape() { this->DCall(EeWonDlgCall::SetDontEndOnEscape); }
    void SetDefaultButton(Button*); // 0x2f79c0: keeps the button (held)
    bool ActivateDefaultButton() { return this->DCall(EeWonDlgCall::ActivateDefaultButton)!=0; }
    virtual void HandleComponentEvent(ComponentEvent* e) { this->DCall(EeWonDlgCall::HandleComponentEvent,(intptr_t)e); }
    // the config's writes (DialogConfig)
    void SetAllowDrag(bool v) { this->DCall(EeWonDlgCall::SetField,(intptr_t)EeWonDlgField::AllowDrag,v); }
    void SetKeepInRoot(bool v) { this->DCall(EeWonDlgCall::SetField,(intptr_t)EeWonDlgField::KeepInRoot,v); }
    // the status dialog keeps the drag across its resize (Virtual08 ends it)
    bool IsDragging() { return this->DCall(EeWonDlgCall::GetField,(intptr_t)EeWonDlgField::Dragging)!=0; }
    void SetDragging(bool v) { this->DCall(EeWonDlgCall::SetField,(intptr_t)EeWonDlgField::Dragging,v); }

    EeWonDialog* mDlg;
private:
    intptr_t DCall(EeWonDlgCall c,intptr_t a=0,intptr_t b=0,intptr_t cc=0) { return ee_won_dlg_call(this->mDlg,c,a,b,cc); }
};

// Button (R6 WON round 30): its flags, Space's hold, the repeat delay, the mouse's press state, the radio group's walk
// and the two default buttons are the Rust Button (rust/crates/ee_won/src/btnobj.rs); this is the type the skins and
// the lobby derive from, with one-line forwarders. The click sound stays a member (Button.cpp is the host).
class Button : public Component {
public:
    void Fn2F3440() { this->BCall(EeWonBtnCall::Click); } // 0x2f3440: the click activation (tests the enabled and checked states)
    Button();                               // 0x2f3070 (WONAPI)
    virtual ~Button();                      // 0x2f3100 (WONAPI)
    virtual bool HandleActivate();          // +0xac (a click; EELobbyColorButton 0x290669)
    virtual void SetTextHook(const GUIString&);
    virtual void SetText(const GUIString& t, bool inv) { this->BCall(EeWonBtnCall::SetText,(intptr_t)&t,inv); } // +0xb4 (0x2f3180)
    virtual const GUIString& GetText() { return gEmptyGUIString; }   // +0xb8 0x2905a8
    void SetCheck(bool c, bool n = false) { this->BCall(EeWonBtnCall::SetCheck,c,n); }
    void SetButtonFlags(unsigned long f, bool s) { this->BCall(EeWonBtnCall::SetButtonFlags,(intptr_t)f,s); } // 0x2f3370 (EELobbyColorButton: 0x40, true)

    // the flag tests read one bit through an inline bool getter (shr n; test 1: Button::SetButtonFlags 0x2f3370)
    bool IsRepeat() { return (GetButtonFlags() & 0x02) != 0; }
    bool IsChecked() { return (GetButtonFlags() & 0x04) ? true : false; }
    bool IsRadio() { return (GetButtonFlags() & 0x08) != 0; }
    bool IsPressed() { return (GetButtonFlags() & 0x20) != 0; }
    bool IsActivateOnPress() { return (GetButtonFlags() & 0x40) != 0; }
    bool IsNoActivate() { return (GetButtonFlags() & 0x200) != 0; }
    bool IsDefaultOnFocus() { return (GetButtonFlags() & 0x400) != 0; }
    unsigned long GetButtonFlags() { return (unsigned long)this->BCall(EeWonBtnCall::GetField,(intptr_t)EeWonBtnField::Flags); }
    int GetMouseDownState() { return (int)this->BCall(EeWonBtnCall::GetField,(intptr_t)EeWonBtnField::MouseDownState); } // 0 up, 1 down outside, 2 down over
    SmartPtr<Sound> mClickSound; // at +0x3c
    EeWonButton* mBtn;
    static void SetDialogDefaultButton(Button*); //2f31c0
    static void UnSetDialogDefaultButton(Button*); //2f3230
    static void SetDefaultButton(Button*); //2f3280
    static void UnSetDefaultButton(Button*); //2f3300
    void CopyAttributes(Button*); //2f3310
    void ButtonDown() { this->BCall(EeWonBtnCall::ButtonDown); } //2f3660
    void ButtonUp(bool a) { this->BCall(EeWonBtnCall::ButtonUp,a); } //2f36d0
    Button* GetNextRadio(bool f) { return static_cast<Button*>((Component*)this->BCall(EeWonBtnCall::GetNextRadio,f)); } //2f34a0
    Button* GetEndRadio(bool f) { return static_cast<Button*>((Component*)this->BCall(EeWonBtnCall::GetEndRadio,f)); } //2f34e0
    bool IsMouseOver() { return this->BCall(EeWonBtnCall::IsMouseOver)!=0; } //2f3170
    bool IsDefaultButton() { return this->BCall(EeWonBtnCall::IsDefaultButton)!=0; } //2f31b0
    void SetClickSound(Sound*); //2f3910
    virtual void AddedToParent() { this->BCall(EeWonBtnCall::AddedToParent); } //2f33c0
    virtual void MouseEnter(int x,int y) { this->BCall(EeWonBtnCall::MouseEnter,x,y); } //2f35b0
    virtual void MouseExit() { this->BCall(EeWonBtnCall::MouseExit); } //2f3620
    virtual void MouseDown(int x,int y,int b) { this->BCall(EeWonBtnCall::MouseDown,x,y,b); } //2f35e0
    virtual void MouseUp(int x,int y,int b) { this->BCall(EeWonBtnCall::MouseUp,x,y,b); } //2f3600
    virtual void MouseDrag(int x,int y) { this->BCall(EeWonBtnCall::MouseDrag,x,y); } //2f3740
    virtual void GotFocus() { this->BCall(EeWonBtnCall::GotFocus); } //2f37a0
    virtual void LostFocus() { this->BCall(EeWonBtnCall::LostFocus); } //2f37d0
    virtual bool KeyUp(int k) { return this->BCall(EeWonBtnCall::KeyUp,k)!=0; } // 0x2f38b0, vtable +0x58
    virtual bool KeyDown(int k) { return this->BCall(EeWonBtnCall::KeyDown,k)!=0; }
    virtual bool TimerEvent(int d) { return this->BCall(EeWonBtnCall::TimerEvent,d)!=0; }
    virtual void MouseHover(unsigned long d) { this->BCall(EeWonBtnCall::MouseHover,(intptr_t)d); }
private:
    intptr_t BCall(EeWonBtnCall c,intptr_t a=0,intptr_t b=0,intptr_t cc=0) { return ee_won_btn_call(this->mBtn,c,a,b,cc); }
};

enum MouseButton { MouseButton_Left=0,MouseButton_Middle=1,MouseButton_Right=2 };
// WrappedText (R6 WON round 38): the breaks, size, maximum width and the wrap and centre settings are the Rust
// object's (rust/crates/ee_won/src/wrapobj.rs); the C++ keeps the text and the font and rewraps through it.
class WrappedText : public RefCount {
public:
    GUIString mText; //08
    SmartPtr<Font> mFont; //0c
    EeWonWrapText* mObj;
protected:
    void CalcWrap(); //34a9b0
public:
    WrappedText(); //34a7e0
    WrappedText(const GUIString&); // (not identified in the original exe)
    virtual ~WrappedText() { ee_won_wrap_free(this->mObj); }
    void SetText(const GUIString& t) { this->mText=t; this->CalcWrap(); } //34acc0
    void SetWrap(bool w) { if(ee_won_wrap_set_wrap(this->mObj,w)) this->CalcWrap(); } //34ad20
    void SetCentered(bool c) { ee_won_wrap_set_centered(this->mObj,c); } //34ad40
    void SetFont(Font* f) { this->mFont=f; this->CalcWrap(); } //34ad50
    void SetMaxWidth(int w) { if(ee_won_wrap_set_max_width(this->mObj,w)) this->CalcWrap(); } //34adb0
    void Paint(Graphics&,int,int); //34adf0
    int GetWidth() { return ee_won_wrap_width(this->mObj); }
    const GUIString& GetText() { return this->mText; } // inline; returns the text at +0x08
    int GetHeight(); // inline, defined after Font below
};
// MSButtonBase (R6 WON round 35): its colours, alignments, focus style, text pads, transparency, fit to text and
// the text's paint (DrawButtonText) are the Rust MSButtonBase (rust/crates/ee_won/src/msbtnobj.rs); the C++ keeps
// the wrapped text (GetText, the fixture, the lobby and the header button's paint read it). The derived MS controls
// are declared separately below.
class MSButtonBase : public Button {
public:
    MSButtonBase(); // 0x2fe6e0: allocates the WrappedText (0x30 bytes)
    virtual ~MSButtonBase(); // 0x2fe910, out of line
    virtual void Virtual08() { this->MCall(EeWonMsBtnCall::SizeChanged); } //2fea10
    virtual void GetDesiredSize(int& w,int& h) { this->MCall(EeWonMsBtnCall::DesiredSize,(intptr_t)&w,(intptr_t)&h); } //2fe8c0
    virtual void SetTextHook(const GUIString& t) { this->mText->SetText(t); this->MCall(EeWonMsBtnCall::TextSet); } //2fe880
    virtual const GUIString& GetText() { return this->mText->mText; } //2fe850
    void SetTransparent(bool t) { this->MCall(EeWonMsBtnCall::SetTransparent,t); } //2fe8a0
    void SetAdjustHeightToFitText(bool a) { this->MCall(EeWonMsBtnCall::SetAdjustHeightToFitText,a); } //2fe9f0
    void AdjustHeightToFitText() { this->MCall(EeWonMsBtnCall::AdjustHeightToFitText); } //2fe980
    void DrawButtonText(Graphics& g,int x,int y) { this->MCall(EeWonMsBtnCall::DrawButtonText,(intptr_t)&g,x,y); } //2fea40
    void SetFont(Font* f) { this->mText->SetFont(f); }
    void SetTextColor(int c) { this->MSet(EeWonMsBtnField::TextColor,c); }
    void SetStandardTextColor(StandardColor c) { this->MCall(EeWonMsBtnCall::SetStandardTextColor,(intptr_t)c); }
    void SetWrap(bool b) { this->mText->SetWrap(b); }
    void SetHorzAlign(HorzAlign h) { this->MCall(EeWonMsBtnCall::SetHorzAlign,(intptr_t)h); }
    void SetVertAlign(VertAlign v) { this->MSet(EeWonMsBtnField::VAlign,v); }
    enum FocusStyle { FocusStyle_None,FocusStyle_AroundWindow,FocusStyle_AroundText };
    void SetFocusStyle(FocusStyle s) { this->MSet(EeWonMsBtnField::FocusStyle,s); }
    void SetHorzTextPad(int l,int r) { this->MSet(EeWonMsBtnField::LeftTextPad,l); this->MSet(EeWonMsBtnField::RightTextPad,r); }
    void SetVertTextPad(int t,int b) { this->MSet(EeWonMsBtnField::TopTextPad,t); this->MSet(EeWonMsBtnField::BottomTextPad,b); }
    void IncrementHorzTextPad(int l,int r) { this->MCall(EeWonMsBtnCall::IncrementHorzTextPad,l,r); }
    void IncrementVertTextPad(int t,int b) { this->MCall(EeWonMsBtnCall::IncrementVertTextPad,t,b); }
    // the fields the subclasses and the skins read or set alone (R6 WON round 35's census)
    bool IsTransparent() { return this->MCall(EeWonMsBtnCall::Get,(intptr_t)EeWonMsBtnField::Transparent)!=0; }
    void SetTransparentField(bool t) { this->MSet(EeWonMsBtnField::Transparent,t); } // the flag alone (no background grab)
    int GetLeftTextPad() { return (int)this->MCall(EeWonMsBtnCall::Get,(intptr_t)EeWonMsBtnField::LeftTextPad); }
    void SetLeftTextPad(int l) { this->MSet(EeWonMsBtnField::LeftTextPad,l); }
    void SetHorzAlignField(HorzAlign h) { this->MSet(EeWonMsBtnField::HAlign,h); } // the field alone (the text's centring kept)
    SmartPtr<WrappedText> mText;
    EeWonMsButton* mMsb;
protected:
    intptr_t MCall(EeWonMsBtnCall c,intptr_t a=0,intptr_t b=0,intptr_t d=0) { return ee_won_msbtn_call(this->mMsb,c,a,b,d); }
    void MSet(EeWonMsBtnField f,int v) { this->MCall(EeWonMsBtnCall::Set,(intptr_t)f,v); }
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
    MSButton();
    MSButton(const GUIString&);
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
    void Init();
public:
    virtual ~MSCheckbox(); // 0x2ff420, out of line
    virtual void Paint(Graphics&); // 0x2ff470, vtable +0x18
    typedef void (*CheckboxPainterFunc)(Graphics&,MSCheckbox*);
    static CheckboxPainterFunc CheckboxPainter;
    static int mCheckboxWidth,mCheckboxHeight;
    static void DefaultCheckboxPainterFunc(Graphics&,MSCheckbox*);
};
// MSRadioButton (0x5c bytes, no members of its own); constructor out of line
class MSRadioButton : public MSButtonBase {
public:
protected:
    void Init(); // inlined into the constructor at 0x2ff530
public:
    MSRadioButton();
    virtual ~MSRadioButton(); // 0x2ff5b0, out of line
    virtual void Paint(Graphics&); // 0x2ff600, vtable +0x18
    typedef void (*RadioPainterFunc)(Graphics&,MSRadioButton*);
    static RadioPainterFunc RadioPainter;
    static int mRadioWidth,mRadioHeight;
    static void DefaultRadioPainterFunc(Graphics&,MSRadioButton*);
};

// MSArrowButton (0x48 bytes): Button (0x44) plus the direction at +0x44; constructor and destructor out of line
class MSArrowButton : public Button {
public:
    MSArrowButton(Direction=Direction_Up);
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
    MSTabButton(const GUIString& = "",bool=true,int=3);

    virtual void Paint(Graphics&); // vtable +0x18
    virtual void GetDesiredSize(int&,int&); // vtable +0x0c
    void SetLowerAmount(int n) { this->mLowerAmount=n; }
    int mLowerAmount; //5c
};
// MS3DFrame (0x38 bytes): Component (0x30) plus a bool at +0x30 and an int at +0x34; constructor and destructor out of line
class MS3DFrame : public Component {
public:
    MS3DFrame(int=3,bool=false);
    virtual void Paint(Graphics&); // 0x3007b0, vtable +0x18
    void SetIsUp(bool v) { this->mIsUp=v; }
    void SetFrameType(int t) { this->mType=t; }
    bool mIsUp; //30
    int mType; //34
};

// Label (R6 WON round 22): its colour, alignments, padding, fit to text and paint are the Rust Label
// (rust/crates/ee_won/src/labelobj.rs); the C++ keeps the wrapped text (the fixture and the lobby read it) and the
// background.
class Label : public Component {
public:
    Label(); // at 0x006f9d80 in the original
    virtual ~Label();
    virtual void Virtual08() { this->Call(EeWonLabelCall::SizeChanged); } // SizeChanged
    virtual void GetDesiredSize(int& w,int& h) { this->Call(EeWonLabelCall::DesiredSize,(intptr_t)&w,(intptr_t)&h); }
    virtual void Paint(Graphics& g) { this->Call(EeWonLabelCall::Paint,(intptr_t)&g); }
    void SetText(const GUIString& t,bool invalidate=true) { this->mWrappedText->SetText(t); this->Call(EeWonLabelCall::TextSet,invalidate); }
    void SetHorzAlign(HorzAlign a) { this->Call(EeWonLabelCall::SetHorzAlign,(intptr_t)a); }
    void SetWrap(bool w) { this->Call(EeWonLabelCall::SetWrap,w); }
    void SetAdjustHeightToFitText(bool a) { this->Call(EeWonLabelCall::SetAdjustHeightToFitText,a); }
    void SetVertPad(int inTop,int inBottom) { this->Call(EeWonLabelCall::SetVertPad,inTop,inBottom); }
    void SetFont(Font* theFont) { this->mWrappedText->SetFont(theFont); }
    void SetTextColor(int theColor) { this->Call(EeWonLabelCall::SetTextColor,theColor); }
    int GetTextColor() { return (int)this->Call(EeWonLabelCall::TextColor); }
    void SetBackground(const Background& theBackground) { this->mBackground=theBackground; }
    void SetVertAlign(VertAlign theAlign) { this->Call(EeWonLabelCall::SetVertAlign,(intptr_t)theAlign); }
    SmartPtr<WrappedText> mWrappedText;
    Background mBackground;
    EeWonLabel* mRust;
private:
    intptr_t Call(EeWonLabelCall c,intptr_t a=0,intptr_t b=0) { return ee_won_label_call(this->mRust,c,a,b,0); }
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
// InputBox (R6 WON round 22): its text, selection, scroll, cursor, padding, history and paint are the Rust
// InputBox (rust/crates/ee_won/src/inputobj.rs); this is the type the lobby holds and what its configs set.
class InputBox : public Component {
public:
    void SetFont(Font* f) { this->mFont=f; }
    typedef bool (*InputFilter)(InputBox*,GUIString&);
    static bool NumericFilter(InputBox*,GUIString&); // at 0x006f7f70 in the original
    InputBox(); // at 0x006f7fd0 in the original
    virtual ~InputBox();
    const GUIString& GetText() const { return this->mRealText; }
    void SetText(const GUIString& t,bool send) { this->CallText(EeWonInputCall::SetText,t,send); }
    void Clear(bool send) { this->CallText(EeWonInputCall::SetText,gEmptyGUIString,send); }
    void AddInputHistory(const GUIString& t,bool clear) { this->CallText(EeWonInputCall::AddInputHistory,t,clear); }
    void SetInputHistory(const GUIString& t,bool clear=false) { this->CallText(EeWonInputCall::SetInputHistory,t,clear); }
    void SetAllowSelection(bool allow) { this->Call(EeWonInputCall::SetAllowSelection,allow); }
    void SelectRange(int start,int end) { this->Call(EeWonInputCall::SelectRange,start,end); }
    void SetIcon(Image* icon) { this->mIcon=icon; this->Call(EeWonInputCall::IconChanged); }
    void SetCharWidth(int em) { this->Call(EeWonInputCall::SetCharWidth,em); }
    void SetBackground(const Background& b) { this->mBackground=b; }
    void SetDisabledBackground(const Background& b) { this->mDisabledBackground=b; }
    void SetCursorBlinkRate(int on,int off=0) { this->Call(EeWonInputCall::SetCursorBlinkRate,on,off); }
    void AdjustPosition() { this->Call(EeWonInputCall::AdjustPosition); }
    void CopySelection() { this->Call(EeWonInputCall::CopySelection); }
    void SetSelText(const GUIString& t,bool send=true) { this->CallText(EeWonInputCall::SetSelText,t,send); }
    void SetPasswordChar(char c) { this->Call(EeWonInputCall::SetPasswordChar,(intptr_t)c); }
    void SetMaxChars(int n) { this->Call(EeWonInputCall::SetMaxChars,n); }
    void SetSelectOnFocus(bool on) { this->Call(EeWonInputCall::SetSelectOnFocus,on); }
    void SetMaxInputHistory(int n) { this->Call(EeWonInputCall::SetMaxInputHistory,n); }
    void SetLeftPad(int n) { this->Call(EeWonInputCall::SetLeftPad,n); }
    int GetSelStart() { return (int)this->Call(EeWonInputCall::SelStart); }
    virtual void Virtual08() { this->Call(EeWonInputCall::SizeChanged); } // SizeChanged
    virtual void GetDesiredSize(int& w,int& h) { this->Call(EeWonInputCall::DesiredSize,(intptr_t)&w,(intptr_t)&h); }
    virtual void GotFocus() { this->Call(EeWonInputCall::GotFocus); }
    virtual void LostFocus() { this->Call(EeWonInputCall::LostFocus); }
    virtual void MouseDown(int x,int y,int b) { this->Call(EeWonInputCall::MouseDown,x,y,b); }
    virtual void MouseUp(int x,int y,int b) { this->Call(EeWonInputCall::MouseUp,x,y,b); }
    virtual void MouseExit() { this->Call(EeWonInputCall::MouseExit); }
    virtual void MouseDrag(int x,int y) { this->Call(EeWonInputCall::MouseDrag,x,y); }
    virtual bool TimerEvent(int delta) { return this->Call(EeWonInputCall::TimerEvent,delta)!=0; }
    virtual bool KeyChar(int k) { return this->Call(EeWonInputCall::KeyChar,k)!=0; }
    virtual bool KeyDown(int k) { return this->Call(EeWonInputCall::KeyDown,k)!=0; }
    virtual void Paint(Graphics& g) { this->Call(EeWonInputCall::Paint,(intptr_t)&g); }
    GUIString mText; // the shown text (the password characters for a password box): the Rust box's copy
    GUIString mRealText; // GetText(): the Rust box's copy
    SmartPtr<Image> mIcon;
    InputFilter mInputFilter;
    bool mUseDisabledColors;
    SmartPtr<Font> mFont;
    unsigned long mTextColor;
    int mDisabledTextColor;
    SelectionColor mSelColor;
    Background mBackground;
    Background mDisabledBackground;
    EeWonInput* mRust;
private:
    intptr_t Call(EeWonInputCall c,intptr_t a=0,intptr_t b=0,intptr_t cc=0) { return ee_won_input_call(this->mRust,c,a,b,cc); }
    void CallText(EeWonInputCall c,const GUIString& t,bool flag) { this->Call(c,(intptr_t)t.mData->mText,t.length(),flag); }
};

class StretchImage;
// Plain record (no vtable, 0x14 bytes): four border ints then the image at +0x10.
// The implicit destructor releases the image; the copy constructor is out of line (declared below).
class WONFrame {
public:
    int mLeft,mTop,mRight,mBottom;
    SmartPtr<Image> mImage;
    WONFrame(Image* =NULL,int=0); // 0x3175f0 (ret 8): one border width on all sides
    WONFrame(Image*,int,int,int,int);
    WONFrame(StretchImage*);
    WONFrame(const WONFrame&); // copies the image (not identified in the original exe)
    WONFrame& operator=(const WONFrame&);
    void Copy(const WONFrame&); // 0x3177c0: duplicates the image instead of sharing it
    void SetImage(Image*); // shares the body at 0x349ab0
    void SetImage(Image*,int,int,int,int); // 0x317730: sets the image and the borders
    void Paint(Graphics&,Component*); //3178c0
    void Paint(Graphics&,Component*,int,int,int,int); //317910
};
// WONFrameComponent (0x44 bytes): Component (0x30) plus a WONFrame (0x14)
class WONFrameComponent : public Component {
public:
    WONFrameComponent(Image* =NULL,int=0,int=0,int=0,int=0);
    WONFrameComponent(const WONFrame&);
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
    ImageComponent(Image* i = NULL,bool t = false); // 0x306b10 (inline, after Image)
    // Implicit virtual destructor: releases the image at +0x30, then Component's cleanup
    virtual void GetDesiredSize(int& w, int& h); // 306cf0 (inline, after Image)
    virtual void Paint(Graphics&);          // 306d30 (guidraw.rs image_component_paint; GuiHost.cpp)
    virtual void SetImageAndTile(Image* i, bool t); // 306c80 (inline, after Image)
    virtual void SetImage(Image* i); // 306c10 (inline, after Image)
    void SetTile(bool doTile) { this->mTile = doTile; }
    void SetStretch(bool doStretch) { this->mStretch = doStretch; }
    bool GetTile() { return this->mTile; }
    bool GetStretch() { return this->mStretch; }
    Image* GetImage() { return this->mImage.get(); }
    SmartPtr<Image> mImage; // at +0x30
    bool mTile,mStretch; // at +0x34, +0x35 (set at 0x394da6/0x394dd8)
};

// a drop-down list (vtable 0x84d638; its list at +0x90)
// TabBar and TabCtrl (R6 WON round 27): the bar's buttons, selected tab and overlap and the control's padding, offsets,
// bar height and overlap, arrows, placement flags, pages and selected page are the Rust objects
// (rust/crates/ee_won/src/tabobj.rs); these are the types the configs, the skins (MSTabCtrl, WONTabCtrl) and the root
// screen use, with one-line forwarders (TabCtrl.cpp is their host). The control's four containers and its page area's
// layout point stay its members: its constructor makes them and gives them to the Rust control.
class TabCtrl;
class LayoutPoint;
class TabBar : public Container {
public:
    TabBar(TabCtrl*);
    virtual ~TabBar(); // the implicit destructor (0x3081f0): the Rust bar gives the buttons back
    virtual void HandleComponentEvent(ComponentEvent* e) { ee_won_tabbar_call(this->mBar,EeWonTabBarCall::HandleComponentEvent,(intptr_t)e); }
    EeWonTabBar* mBar;
};
class TabCtrl : public Container {
public:
    TabCtrl();
    virtual ~TabCtrl(); // the implicit destructor (0x301340): the Rust control gives the arrows back
    virtual void Virtual08() { this->TCall(EeWonTabCall::SizeChanged); } // SizeChanged 0x308f50
    virtual void GetDesiredSize(int& w,int& h) { this->TCall(EeWonTabCall::DesiredSize,(intptr_t)&w,(intptr_t)&h); }
    virtual bool KeyDown(int k) { return this->TCall(EeWonTabCall::KeyDown,k)!=0; }
    virtual void AddedToParent() { this->TCall(EeWonTabCall::AddedToParent); }
    virtual void HandleComponentEvent(ComponentEvent* e) { this->TCall(EeWonTabCall::HandleComponentEvent,(intptr_t)e); }
    virtual bool TabFocus(bool f,bool i) { return this->TCall(EeWonTabCall::TabFocus,f,i)!=0; }
    void CopyAttributes(TabCtrl* from) { this->TCall(EeWonTabCall::CopyAttributes,(intptr_t)from->mTab); }
    void SetTabBarOnRight(bool v) { this->TCall(EeWonTabCall::SetTabBarOnRight,v); }
    void SetArrowsOnLeft(bool v) { this->TCall(EeWonTabCall::SetArrowsOnLeft,v); }
    void SetTabArrows(Button* l,Button* r) { this->TCall(EeWonTabCall::SetTabArrows,(intptr_t)static_cast<Component*>(l),(intptr_t)static_cast<Component*>(r)); }
    void SetCurTab(int id,bool f) { this->TCall(EeWonTabCall::SetCurTab,id,f); }
    void SetCurTabByName(const GUIString& n,bool f) { this->TCall(EeWonTabCall::SetCurTabByName,(intptr_t)&n,f); }
    int GetTabIdForContainer(Container* c) { return (int)this->TCall(EeWonTabCall::GetTabIdForContainer,(intptr_t)static_cast<Component*>(c)); }
    void EnableTab(int id,bool e) { this->TCall(EeWonTabCall::EnableTab,id,e); }
    void AddTab(Button* t,Container* c) { this->TCall(EeWonTabCall::AddTab,(intptr_t)static_cast<Component*>(t),(intptr_t)static_cast<Component*>(c)); }
    void RemoveTab(int id) { this->TCall(EeWonTabCall::RemoveTab,id); } // removes the tab and reselects
    // the selected tab's id and the configs' writes (TabCtrlConfig, RootScreen)
    int GetCurTabId() { return (int)this->TCall(EeWonTabCall::GetField,(intptr_t)EeWonTabField::CurTabId); }
    void SetTabBarHeight(int v) { this->TCall(EeWonTabCall::SetField,(intptr_t)EeWonTabField::TabBarHeight,v); }
    void SetTabOverlap(int v) { this->TCall(EeWonTabCall::SetField,(intptr_t)EeWonTabField::TabOverlap,v); }
    void SetTabBarOverlap(int v) { this->TCall(EeWonTabCall::SetField,(intptr_t)EeWonTabField::TabBarOverlap,v); }
    void SetBottomTabTopOffset(int v) { this->TCall(EeWonTabCall::SetField,(intptr_t)EeWonTabField::BottomTabTopOffset,v); }
    void SetBottomTabLeftOffset(int v) { this->TCall(EeWonTabCall::SetField,(intptr_t)EeWonTabField::BottomTabLeftOffset,v); }
    SmartPtr<TabBar> mBottomTabBar; //84
    SmartPtr<Container> mTopTabBar; //88
    SmartPtr<Container> mBottomTabBarContainer; //8c
    SmartPtr<Container> mTopTabBarContainer; //90
    SmartPtr<LayoutPoint> mContainerLayout; //94 (TabCtrlConfig's CONTAINERLAYOUT, the skins' frames)
    EeWonTabCtrl* mTab;
private:
    intptr_t TCall(EeWonTabCall c,intptr_t a=0,intptr_t b=0) { return ee_won_tab_call(this->mTab,c,a,b); }
};

// Scrollbar (R6 WON round 25): its parts (arrows, track, thumb), orientation, thumb options, range, steps, position
// and drag are the Rust Scrollbar (rust/crates/ee_won/src/barobj.rs); this is the type the skins, configs and
// scrollers hold (MSScrollbar and WONScrollbar derive from it). Constructor 0x305e10, destructor 0x305eb0.
class Scrollbar : public Container {
public:
    Scrollbar(bool=true);
    virtual ~Scrollbar();
    virtual void Virtual08() { this->Call(EeWonBarCall::SizeChanged); } //306380
    virtual void MouseDown(int x,int y,int b) { this->Call(EeWonBarCall::MouseDown,x,y,b); } //3064c0
    virtual void MouseDrag(int x,int y) { this->Call(EeWonBarCall::MouseDrag,x,y); } //306510
    virtual void AddedToParent() { this->Call(EeWonBarCall::AddedToParent); } //305fe0
    virtual void HandleComponentEvent(ComponentEvent* e) { this->Call(EeWonBarCall::HandleEvent,(intptr_t)e); } //306690
    virtual void SetEnabled(bool on) { this->Call(EeWonBarCall::SetEnabled,on); } // 0x3067e0, vtable +0x90
    void CopyAttributes(Scrollbar* from) { this->Call(EeWonBarCall::CopyAttributes,(intptr_t)from->mRust); }
    void AddControls() { this->Call(EeWonBarCall::AddControls); } //306000
    void LayoutThumb(bool changePos) { this->Call(EeWonBarCall::LayoutThumb,changePos); } //3061d0
    // the parts the skins, the configs and the scroller read and set
    Button* GetUpArrow() { return (Button*)this->Call(EeWonBarCall::GetPart,0); }
    void SetUpArrow(Button* b) { this->Call(EeWonBarCall::SetPart,0,(intptr_t)static_cast<Component*>(b)); }
    Button* GetDownArrow() { return (Button*)this->Call(EeWonBarCall::GetPart,1); }
    void SetDownArrow(Button* b) { this->Call(EeWonBarCall::SetPart,1,(intptr_t)static_cast<Component*>(b)); }
    Button* GetScrollRect() { return (Button*)this->Call(EeWonBarCall::GetPart,2); }
    void SetScrollRect(Button* b) { this->Call(EeWonBarCall::SetPart,2,(intptr_t)static_cast<Component*>(b)); }
    Component* GetThumb() { return (Component*)this->Call(EeWonBarCall::GetPart,3); }
    void SetThumb(Component* c) { this->Call(EeWonBarCall::SetPart,3,(intptr_t)c); }
    bool IsVertical() { return this->Call(EeWonBarCall::IsVertical)!=0; }
    void SetVertical(bool on) { this->Call(EeWonBarCall::SetVertical,on); }
    void SetResizeThumb(bool on) { this->Call(EeWonBarCall::SetResizeThumb,on); }
    void SetMinThumbSize(unsigned char n) { this->Call(EeWonBarCall::SetMinThumbSize,n); }
    int GetPosition() { return (int)this->Call(EeWonBarCall::GetPosition); }
    void SetPosition(int p) { this->Call(EeWonBarCall::SetPosition,p); } // the field only (the scroller's update)
    void SetRange(int total,int page) { this->Call(EeWonBarCall::SetRange,total,page); }
    void SetLineScrollSizes(int up,int down) { this->Call(EeWonBarCall::SetLineScrollSizes,up,down); }
    EeWonScrollbar* mRust;
private:
    intptr_t Call(EeWonBarCall c,intptr_t a=0,intptr_t b=0,intptr_t cc=0) { return ee_won_bar_call(this->mRust,c,a,b,cc); }
};

// Scroller and ScrollbarScroller (R6 WON round 25): the scroll area, the bars and their conditions are one Rust
// Scroller (rust/crates/ee_won/src/scrollerobj.rs) the Scroller constructor makes; these are the types the configs,
// the combo box, the scroll areas and the scroll containers (MSScrollContainer, WONScrollContainer) hold.
class Scroller : public Container {
public:
    Scroller();
    virtual ~Scroller(); // the implicit destructors (0x3001e0, ScrollbarScroller's 0x300060)
    virtual void MouseDown(int x,int y,int b) { this->SCall(EeWonScrollerCall::MouseDown,x,y,b); } //305670
    virtual bool MouseWheel(int n) { return this->SCall(EeWonScrollerCall::MouseWheel,n)!=0; } //3056c0
    virtual void NotifyScrollChange(bool) {} // empty inline (shared body at 0x30a800)
    ScrollArea* GetScrollArea() { return (ScrollArea*)this->SCall(EeWonScrollerCall::GetScrollArea); }
    // (a ScrollArea's Component is at its own address: single inheritance)
    void SetScrollArea(ScrollArea* a) { this->SCall(EeWonScrollerCall::SetScrollArea,(intptr_t)a); }
    void SetOffsets(int h,int v) { this->SCall(EeWonScrollerCall::SetOffsets,h,v); } //3055e0 horizontal then vertical
    void SetVertOffset(int v) { this->SCall(EeWonScrollerCall::SetVertOffset,v); } //305630
    void SetHorzOffset(int h) { this->SCall(EeWonScrollerCall::SetHorzOffset,h); } //305650
    EeWonScroller* mRust;
protected:
    intptr_t SCall(EeWonScrollerCall c,intptr_t a=0,intptr_t b=0,intptr_t cc=0) { return ee_won_scroller_call(this->mRust,c,a,b,cc); }
};
enum ScrollbarCondition { ScrollbarCondition_Disable=0,ScrollbarCondition_Enable=1,ScrollbarCondition_Conditional=2 };
// ScrollbarScroller (0x305700: both conditions Enable, in the Rust scroller)
class ScrollbarScroller : public Scroller {
public:
    virtual void AddedToParent() { this->SCall(EeWonScrollerCall::AddedToParent); } //305840
    virtual void HandleComponentEvent(ComponentEvent* e) { this->SCall(EeWonScrollerCall::HandleEvent,(intptr_t)e); } //305a00
    virtual void NotifyScrollChange(bool b) { this->SCall(EeWonScrollerCall::NotifyScrollChange,b); } // 0x305a50, vtable +0xcc
    void SetScrollbar(Scrollbar* s) { this->SCall(EeWonScrollerCall::SetScrollbar,(intptr_t)static_cast<Component*>(s)); } //305760
    void AddControls() { this->SCall(EeWonScrollerCall::AddControls); } //305860
    Scrollbar* GetScrollbar(bool vertical) { return (Scrollbar*)this->SCall(EeWonScrollerCall::GetScrollbar,vertical); }
    void SetBar(bool vertical,Scrollbar* s) { this->SCall(EeWonScrollerCall::SetBar,vertical,(intptr_t)static_cast<Component*>(s)); } // the field only (MSScrollContainer's constructor)
    bool EnableScrollbar(Scrollbar* s,ScrollbarCondition c) { return this->SCall(EeWonScrollerCall::EnableScrollbar,(intptr_t)static_cast<Component*>(s),c)!=0; } //3058d0
    void SetScrollbarConditions(ScrollbarCondition h,ScrollbarCondition v) { this->SCall(EeWonScrollerCall::SetScrollbarConditions,h,v); }
};
enum ScrollButtonCondition {
    ScrollButtonCondition_Disable=0, ScrollButtonCondition_Enable=1,
    ScrollButtonCondition_Conditional=2, ScrollButtonCondition_PartialConditional=3
};
// ComboBox (R6 WON round 28): whether the list is open, no-typing, the button while disabled and the list box's
// configured size are the Rust ComboBox (rust/crates/ee_won/src/comboobj.rs) with every method; this is the type the
// skins (MSComboBox, WONComboBox), the configs and the lobby use, with one-line forwarders (ComboBox.cpp is its
// host). The parts and the undo text stay members: the skins make the parts and read them.
class Scroller;
class ComboBox : public Container {
public:
    ComboBox();
    virtual ~ComboBox(); // 0x2f3a50: the Rust combo box leaves its list box's listeners
    virtual void Virtual08() { this->BCall(EeWonComboCall::SizeChanged); } //2f4010 SizeChanged
    virtual void MouseDown(int x,int y,int b) { this->BCall(EeWonComboCall::MouseDown,x,y,b); } //2f4600
    virtual bool MouseWheel(int n) { return this->BCall(EeWonComboCall::MouseWheel,n)!=0; } //2f4650
    virtual void GotFocus() { this->BCall(EeWonComboCall::GotFocus); } //2f4090
    virtual void LostFocus() { this->BCall(EeWonComboCall::LostFocus); } //2f40b0
    virtual bool KeyDown(int k) { return this->BCall(EeWonComboCall::KeyDown,k)!=0; } //2f4680
    virtual void AddedToParent() { this->BCall(EeWonComboCall::AddedToParent); } //2f3ef0
    virtual void HandleComponentEvent(ComponentEvent* e) { this->BCall(EeWonComboCall::HandleComponentEvent,(intptr_t)e); } //2f44e0
    virtual void SetEnabled(bool e) { this->BCall(EeWonComboCall::SetEnabled,e); } //2f4800
    ListItem* AddString(const GUIString& s,int pos=-1) { return (ListItem*)this->BCall(EeWonComboCall::AddString,(intptr_t)&s,pos); }
    void InsertItem(TextListItem* item,int pos=-1) { this->BCall(EeWonComboCall::InsertItem,(intptr_t)item,pos); }
    void DeleteItem(int pos) { this->BCall(EeWonComboCall::DeleteItem,pos); }
    void UpdateItem(TextListItem* item) { this->BCall(EeWonComboCall::UpdateItem,(intptr_t)item); }
    void Clear(bool input=true) { this->BCall(EeWonComboCall::Clear,input); }
    int GetSelItem() { return (int)this->BCall(EeWonComboCall::GetSelItem); }
    void SetSelItem(int pos,bool notify=false) { this->BCall(EeWonComboCall::SetSelItem,pos,notify); }
    void SetListBox(Scroller* s);
    void AddControls() { this->BCall(EeWonComboCall::AddControls); }
    void CalcListBoxSize() { this->BCall(EeWonComboCall::CalcListBoxSize); }
    void SetListBoxDimensions(int w,int h) { this->BCall(EeWonComboCall::SetListBoxDimensions,w,h); }
    void SetListBoxWidth(int w) { this->BCall(EeWonComboCall::SetListBoxWidth,w); }
    void SetListBoxHeight(int h) { this->BCall(EeWonComboCall::SetListBoxHeight,h); }
    void SetInputBoxToListSelection(bool active=false,bool fire=true) { this->BCall(EeWonComboCall::SetInputBoxToListSelection,active,fire); }
    void SetText(const GUIString& s,bool notify=false) { this->BCall(EeWonComboCall::SetText,(intptr_t)&s,notify); }
    void SetTextAndSel(const GUIString& s,bool notify) { this->BCall(EeWonComboCall::SetTextAndSel,(intptr_t)&s,notify); }
    void ShowList() { this->BCall(EeWonComboCall::ShowList); }
    void SetNoType(bool v) { this->BCall(EeWonComboCall::SetNoType,v); }
    void SetShowButtonWhenDisabled(bool v) { this->BCall(EeWonComboCall::SetField,(intptr_t)EeWonComboField::ShowButtonWhenDisabled,v); } // ComboBoxConfig
    SmartPtr<InputBox> mEdit; //84
    SmartPtr<Button> mButton; //88
    SmartPtr<Scroller> mListBox; //8c
    SmartPtr<ListArea> mListArea; //90
    GUIString mUndoStr; // the text Escape restores
    EeWonComboBox* mCombo;
private:
    intptr_t BCall(EeWonComboCall c,intptr_t a=0,intptr_t b=0,intptr_t cc=0) { return ee_won_combo_call(this->mCombo,c,a,b,cc); }
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
    static ComponentConfig* CfgFactory();
    static Component* CompFactory();
    bool ReadConfig_Unsafe(Component*,const char*,ResourceConfigTable*,ConfigParser*);
    void ReadConfig(Component*,const char*,ResourceConfigTable* = NULL,ConfigParser* = NULL);
    Component* GetChildComponent(const std::string&);
    ComponentConfig* GetChildConfig(const std::string&);
    void InitComponents(); //382cc0 recursive guarded component initialization
    Direction ReadDirection(); //3856f0
    HorzAlign ReadHorzAlign(); //385950
    VertAlign ReadVertAlign(); //385b10
    static void SetResourceFolder(const std::string&);
    static std::string mResourceFolder;
    SmartPtr<Sound> ReadSound(); // 0x3855f0 (the caller owns the result)
    void EnsureComma(); //382e00; SkipComma is382dd0
    int GetIntValue();
    int ParseExpression(int); //383560
    int ReadChar(); //383930
    GUIString ReadFlag(bool* = NULL); //383a90
    GUIString ReadRawString(bool); //383b00
    SmartPtr<Font> ReadFont();
    SmartPtr<Image> ReadImage(bool); // 0x384fa0 (hidden result)
    SelectionColor ReadSelectionColor();
    ComponentConfig* GetComponentConfig(const GUIString&,bool);
    ComponentConfig* ReadComponentConfig(); // 0x3868d0: config lookup
    bool HandleMapComponent(); // 0x386960 (0x386950 is a separate 9-byte wrapper)
    bool HandleModifyGroup(ConfigParser&);
    bool HandleResourceGroup(ConfigParser&);
    bool HandleModify();
    bool BaseHandleInstruction(ConfigParser&,const std::string&,StringParser&); // 0x387370 (forwarder)
    void SetDesiredSizeIfNecessary();
    SmartPtr<Cursor> ReadCursor(); // 0x385380 (hidden result)
    int GetIntResource(const std::string&); //384210
    static SmartPtr<ResourceConfigTable> mResourceConfigTable; // 0x566b28 (declaration only)
    SmartPtr<RawImage> ReadRawImage();
    SmartPtr<Image> GetImageFromStr(const std::string&,bool); //384890
    SmartPtr<RawImage> GetRawImageFromStr(const std::string&);
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
    Component* ReadComponent();
    bool EndOfString(); //382db0
    void SkipComma(); //382e00
    static std::string GetResourceFile(const std::string&); //387e50

    ComponentConfig();
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
    bool RunTable(int,const std::string&); // the table reader (ee_won cfgtab.rs, WONCfgTable.inl); false: not its key
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
    void HandleFitText(); //381d00
    ButtonConfig();                         // 0x381860; the class is 0x7c bytes
    virtual bool Virtual24(const std::string&); //381e80
    virtual void SetComponent(Component*);  //38f8d0 ICF target
    Button* mButton;                        // +78, borrowed
};

class ContainerConfig : public ComponentConfig {
public:
    static Component* LayoutPointFactory();
    static ComponentConfig* CfgFactory();
    static Component* CompFactory();
    bool HandleAdd();
    bool HandleAddLayoutPoint();
    bool HandleLayout(bool chain=false,bool repeat=false);
    bool HandleSameSizeLayout(bool,bool);
    bool HandleCenterLayout(bool,bool);
    bool HandleClearRightLayout();
    bool HandleSpaceEvenlyLayout(bool);
    bool HandleNoOverlapLayout();
    bool HandleSizeAtLeastLayout();
    bool HandleNewLayoutPoint();
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
    static ComponentConfig* CfgFactory();
    static Component* CompFactory();
    virtual bool Virtual24(const std::string&); //389f80
    virtual void SetComponent(Component*);  //38f960
    Dialog* mDialog;                        // +98, borrowed
};

class LobbyConfig : public ConfigObject {
public:
    LobbyConfig();
    typedef LobbyConfig* (*ConfigAllocator)();
    static ConfigAllocator mAllocator; // class static at 0x4d954c
    static LobbyConfig* DefaultAllocator();
    static std::string mDefaultFileName;
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
    ListItem();
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
// BackgroundGrabber (0x38 bytes in the original); RootContainer keeps a pointer to it. R6 WON round 37: the Rust grabber
// (rust/crates/ee_won/src/grabobj.rs) holds the captured image's place, its clip, the flags, the translucent colour and
// level and the capture's order (the original's static counter), and runs every method; the C++ keeps the background
// image and the debug window (BackgroundGrabber.cpp is the host). The root container reads it with ee_won_grab_read.
class BackgroundGrabber {
public:
    SmartPtr<NativeImage> mBGImage; //00
    SmartPtr<Window> mDebugWindow; //28
    EeWonGrabber* mRust;
    BackgroundGrabber();
    ~BackgroundGrabber();
    void PrePaint(Component* c,Graphics& g) { this->GCall(EeWonGrabCall::PrePaint,(intptr_t)c,(intptr_t)&g); } //336070
    void Paint(Component* c,Graphics& g) { this->GCall(EeWonGrabCall::Paint,(intptr_t)c,(intptr_t)&g); } //3360f0
    void SetTranslucentColor(int c,int l) { this->GCall(EeWonGrabCall::SetTranslucentColor,c,l); } //3360d0
    void Clear() { this->GCall(EeWonGrabCall::Clear); } // 0x336060, clears both flags
    void SizeChanged(Component* c) { this->GCall(EeWonGrabCall::SizeChanged,(intptr_t)c); } // 0x2fa500 (empty, ret 4)
    void Invalidate(Component* c) { this->GCall(EeWonGrabCall::Invalidate,(intptr_t)c); } // 0x336680: sets need-put-BG when grabbed
    bool NeedDrawBG() const { return this->GCall(EeWonGrabCall::NeedDrawBG)!=0; }
private:
    BackgroundGrabber(const BackgroundGrabber&);
    BackgroundGrabber& operator=(const BackgroundGrabber&);
    intptr_t GCall(EeWonGrabCall c,intptr_t a=0,intptr_t b=0) const { return ee_won_grab_call(this->mRust,c,a,b); }
};
// RootContainer (R6 WON round 29): the Rust RootContainer (rust/crates/ee_won/src/rootobj.rs) holds the invalid
// rectangle, the queued backgrounds (their images held), the cursor's component and the window drag; the virtuals and
// AddBG are forwarders. The window stays a member (Component::GetRoot reads it). The original's keyboard listener (never
// set), popup set and modal component (never read) are gone, and so are its FindChildAt, ChildInvalidated and
// MouseEnter, which only called the container's.
class RootContainer : public Container {
public:
    RootContainer(Window*);
    virtual ~RootContainer();
    virtual void Virtual08() { this->RCall(EeWonRootCall::SizeChanged); } //304730 SizeChanged
    virtual void PrePaint(Graphics& g) { this->RCall(EeWonRootCall::PrePaint,(intptr_t)&g); } //304760
    virtual void Paint(Graphics& g) { this->RCall(EeWonRootCall::Paint,(intptr_t)&g); } //304900
    virtual void MouseExit() { this->RCall(EeWonRootCall::MouseExit); } //304c60
    virtual void MouseDown(int x,int y,int b) { this->RCall(EeWonRootCall::MouseDown,x,y,b); } //304bd0
    virtual void MouseUp(int x,int y,int b) { this->RCall(EeWonRootCall::MouseUp,x,y,b); } //304c90
    virtual void MouseMove(int x,int y) { this->RCall(EeWonRootCall::MouseMove,x,y); } //304a30
    virtual void MouseDrag(int x,int y) { this->RCall(EeWonRootCall::MouseDrag,x,y); } //304ac0
    virtual bool KeyChar(int k) { return this->RCall(EeWonRootCall::KeyChar,k)!=0; } //304a00
    virtual bool KeyDown(int k) { return this->RCall(EeWonRootCall::KeyDown,k)!=0; } //3049a0
    virtual bool KeyUp(int k) { return this->RCall(EeWonRootCall::KeyUp,k)!=0; } //3049d0
    virtual void InvalidateRect(WONRectangle& r) { this->RCall(EeWonRootCall::InvalidateRect,(intptr_t)&r); } //304980
    virtual void SetCursor(Cursor* c) { this->RCall(EeWonRootCall::SetCursor,(intptr_t)c); } //304cf0
    virtual void RemoveChild(Component* c) { this->RCall(EeWonRootCall::RemoveChild,(intptr_t)c); } //304cc0
    void AddBG(BackgroundGrabber* g) { this->RCall(EeWonRootCall::AddBG,(intptr_t)g); } //304510
    // Window::InvalidateRoot's union of the whole root into the invalid rectangle
    void UnionInvalidRect(const WONRectangle& r) { this->RCall(EeWonRootCall::InvalidateRect,(intptr_t)&r); }

    Window* mWindow;
    EeWonRoot* mRootObj;
private:
    intptr_t RCall(EeWonRootCall c,intptr_t a=0,intptr_t b=0,intptr_t cc=0) { return ee_won_root_call(this->mRootObj,c,a,b,cc); }
};

// how a popup is opened (0x2c bytes, on the stack; provisional names)
struct PopupParams {
    PopupParams(Component* inPopup,int inFlags,Component* inOwner,int inX=-1,int inY=-1);
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
    ComponentExtra();
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
    virtual void HandleComponentEvent(ComponentEvent* e) { this->mComponent->HandleComponentEvent(e); } // 0x337bf0

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
    // R6 WONGUI B round 1: the methods are ee_won's gfxobj.rs. The translation, colour and clip rectangle are the block
    // of members it edits in place (EeWonGfxState: the original's +08, +0c, +14, +24; checked in Graphics.cpp), the
    // clip stack (+18) its object
    int mTransX, mTransY;
    DWORD mColor;
    WONRectangle mClipRect;
    SmartPtr<Font> mFont; //10
    ColorScheme* mColorScheme; //34, borrowed
    EeWonGfx* mGfx; // the clip stack, owned
private:
    DWORD GCall(EeWonGfxCall c, int a = 0, int b = 0, int cc = 0, int d = 0, int e = 0, int f = 0, int g = 0, const void* p = NULL);
};

// 0x2fdcd0 / 0x2fddf0 (cdecl): a 3D frame (inPressed: sunken) and a flat frame (provisional names)
void LobbyDraw3DFrame(Graphics& inGraphics, int inX, int inY, int inWidth, int inHeight, bool inPressed);
void LobbyDrawFrame(Graphics& inGraphics, int inX, int inY, int inWidth, int inHeight, bool inFlag);

#include "ee/wonapi/WONLobbyModelObj.h" // LobbyPlayer, LobbyClient, LobbyClientList and its iterators, LobbyGame

// 0x361350: the game the local player is in (NULL when none)
LobbyGame* GetMyLobbyGame();

// WONAPI's message buffers (provisional names)


class ByteBuffer;
template <class T> class SmartPtr;

// 0x361b60 / 0x361b80 (cdecl): a game message to the lobby
void LobbySendToCaptain(const ByteBuffer* inMessage);
void LobbyBroadcastToGame(const ByteBuffer* inMessage);
void LobbySendToClient(LobbyClient* inClient, const ByteBuffer* inMessage);
void LobbyResetReadyFlags();

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
    DisplayContext(SurfaceBase* surface=NULL) : mSurface(surface),mGraphics(NULL),mPixelScale(1.0f) {}
    virtual ~DisplayContext() {}
    SurfaceBase* mSurface; // at +0x08, not owned (the owning image keeps it); NULL: nothing to draw into
    Graphics* mGraphics; //0c borrowed
    // port: the surface's pixels per logical pixel (WONLobbyPlatform.h "Pixel scale"): every coordinate the GUI
    // draws with is logical; the backend scales it to the surface's pixels
    float mPixelScale;
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
    bool CheckModalValid() { return this->WmCall(EeWonWmCall::CheckModalValid) != 0; } // checks the modal state
public:
    static WindowManager* GetDefaultWindowManager() { return WindowManager::mDefaultWindowManager; }
    static DWORD GetThreadId() { return WindowManager::mThreadId; }
    WindowManager(); // 0x32b660; the class is 0x148 bytes
    virtual ~WindowManager(); //32bff0
    virtual void GetNextEvent() { PlatSleep(20); } //04 32e7c0: the base only sleeps 20 ms
    virtual bool SetScreenResolutionHook(int,int) { return false; } //08 32c380
    virtual void GetScreenResolutionHook(int&,int&) {} //0c 327440
    virtual void Signal() {} //10 326b60
    virtual bool AddWindow(Window* w) { return this->WmCall(EeWonWmCall::AddWindow, (intptr_t)w) != 0; } //14 32d6a0
    virtual bool RemoveWindow(Window* w) { return this->WmCall(EeWonWmCall::RemoveWindow, (intptr_t)w) != 0; } //18 32d840
    virtual void GetScreenRect(WONRectangle&) {} //1c 30a800
    virtual void GetSystemColorScheme() {} //20 326b60
    void InitColorScheme(); //32c230
    void NotifyLostFocus() { this->WmCall(EeWonWmCall::NotifyLostFocus); } //32c2d0
    void NotifyGotFocus(Window* w) { this->WmCall(EeWonWmCall::NotifyGotFocus, (intptr_t)w); } //32c300
    void SetScreenResolution(int w, int h) { this->WmCall(EeWonWmCall::SetScreenResolution, w, h); } //32c320
    void RestoreScreenResolution() { this->WmCall(EeWonWmCall::RestoreScreenResolution); } //32c390
    Window* GetPopupWindow() { return (Window*)this->WmCall(EeWonWmCall::PopupWindow); } //32c440
    void SetNamedFont(const std::string& n, Font* f) { this->WmCall(EeWonWmCall::SetNamedFont, (intptr_t)n.data(), (intptr_t)n.size(), (intptr_t)f); } //32c470
    void SetDefaultFont(Font* font); // inline, defined after Font below
    Font* GetNamedFont(const std::string& n) { return (Font*)this->WmCall(EeWonWmCall::GetNamedFont, (intptr_t)n.data(), (intptr_t)n.size()); } //32c4d0
    Font* GetDefaultFont() { return (Font*)this->WmCall(EeWonWmCall::GetDefaultFont); } //32c540
    Font* GetDefaultFontMod(int,int=0); //32c700
    Font* GetNamedFontMod(const std::string&,int,int=0);
    static SmartPtr<Font> GetDefaultFont(const FontDescriptor&);
    static DisplayContext* GetDefaultDisplayContext();
    void RemoveAllTimerEvents() { this->WmCall(EeWonWmCall::RemoveAllTimerEvents); }
    void SetCacheImages(bool c) { this->WmCall(EeWonWmCall::SetCacheImages, c ? 1 : 0); } // 0x32c7f0: a count; at 0 the caches empty
    // the decodes: the Rust answer carries the returned SmartPtr's reference
    SmartPtr<NativeImage> DecodeImage(DisplayContext* c, const char* p)
    { SmartPtr<NativeImage> i; i.mPtr = (NativeImage*)this->WmCall(EeWonWmCall::DecodeImage, (intptr_t)c, (intptr_t)p, (intptr_t)strlen(p)); return i; } //32c950
    SmartPtr<NativeImage> DecodeImage(const char*); //32ccb0
    SmartPtr<Image> DecodeDelayImage(const char* p)
    { SmartPtr<Image> i; i.mPtr = (Image*)this->WmCall(EeWonWmCall::DecodeDelayImage, (intptr_t)p, (intptr_t)strlen(p)); return i; } //32ccf0
    SmartPtr<RawImage> DecodeImageRaw(const char* p)
    { SmartPtr<RawImage> i; i.mPtr = (RawImage*)this->WmCall(EeWonWmCall::DecodeImageRaw, (intptr_t)p, (intptr_t)strlen(p)); return i; } //32d160
    void SetSoundDecoder(SoundDecoder* d) { this->mSoundDecoder = d; } //32d490
    SmartPtr<Sound> DecodeSound(const SoundDescriptor&); //32d4f0
    void CloseAllWindows() { this->WmCall(EeWonWmCall::CloseAllWindows); }
    void CalcModality() { this->WmCall(EeWonWmCall::CalcModality); } //32da30
    bool NotModalWindow(Window* w) { return this->WmCall(EeWonWmCall::NotModalWindow, (intptr_t)w) != 0; } // 0x32dc30
    // 0x32dbe0 is a separate static SetModal(Component*,bool) helper, not a member.
    int DoDialog(Window* w, Component* c) { return (int)this->WmCall(EeWonWmCall::DoDialog, (intptr_t)w, (intptr_t)c); } //32dc50
    void EndDialog(int r, Window* w, Component* c) { this->WmCall(EeWonWmCall::EndDialog, r, (intptr_t)w, (intptr_t)c); }
    void ExitMessageLoop() { this->WmCall(EeWonWmCall::ExitMessageLoop); } //32ddf0
    void AdjustPopupPosition(Window*,Component*,bool,const WONRectangle&); //32de00
    void DoPopup(const PopupParams&); //32e040
    void EndPopup(Component* c) { this->WmCall(EeWonWmCall::EndPopup, (intptr_t)c); } //32e4d0
    void CheckEndPopup(Component* c) { this->WmCall(EeWonWmCall::CheckEndPopup, (intptr_t)c); } //32e790
    int MessageLoop() { return (int)this->WmCall(EeWonWmCall::MessageLoop); } //32e7d0
    void PumpOnce() { this->GetNextEvent(); } //32e840
    void PostEvent(WindowEventBase* e, bool s = true) { this->WmCall(EeWonWmCall::PostEvent, (intptr_t)e, s ? 1 : 0); } //32e850
    void AddTimerEvent(WindowEventBase* e) { this->WmCall(EeWonWmCall::AddTimerEvent, (intptr_t)e); } //32e960
    void RemoveTimerEvent(WindowEventBase* e) { this->WmCall(EeWonWmCall::RemoveTimerEvent, (intptr_t)e); } //32ea70
    // FindMouseWindow (0x32ec60) and CheckPopupClick (0x32ee40) are the Rust manager's (wmobj.rs), called by its
    // DeliverEvent (round 34)
    void DeliverEvent(WindowEventBase* e) { this->WmCall(EeWonWmCall::DeliverEvent, (intptr_t)e); } //32ef20
    void SubscribeToBroadcast(Component* c, bool s) { this->WmCall(EeWonWmCall::SubscribeToBroadcast, (intptr_t)c, s ? 1 : 0); } //32f3f0
    void BroadcastEvent(ComponentEvent* e) { this->WmCall(EeWonWmCall::BroadcastEvent, (intptr_t)e); }
    void ClearSubscriptions() { this->WmCall(EeWonWmCall::ClearSubscriptions); } //32f5d0
    WindowEventBase* PeekNextEvent() { return (WindowEventBase*)this->WmCall(EeWonWmCall::PeekNextEvent); } //32f700
    void ProcessEvents() { this->WmCall(EeWonWmCall::ProcessEvents); } //32f760
    // R6 WON round 33: the popup stack, the modal dialog, the message loop's depths and the focus are the Rust
    // manager (rust/crates/ee_won/src/wmobj.rs); these read and set it
    Component* GetModalComponent() const { return (Component*)this->WmField(EeWonWmField::ModalComponent); }
    Window* GetFocusWindow() const { return (Window*)this->WmField(EeWonWmField::FocusWindow); }
    int GetPopupFlags() const { return (int)this->WmField(EeWonWmField::PopupFlags); }
    bool GetPopupsInOwnWindow() const { return this->WmField(EeWonWmField::OwnWindow) != 0; }
    void SetPopupsInOwnWindow(bool inOwn) { this->WmSet(EeWonWmField::OwnWindow, inOwn ? 1 : 0); }
    // R6 WON round 34: the window set, the events, the timers, the broadcasts and the screen state are the Rust
    // manager's too (wmloop.rs)
    DWORD GetLastInputTick() const { return (DWORD)this->WmField(EeWonWmField::LastInputTick); }
    Window* GetDefaultWindow() const { return (Window*)this->WmField(EeWonWmField::DefaultWindow); }
    // the Rust manager, freed after every other member (a window's or component's destruction may still call the
    // manager); its held members (the window set, the events, the timers, the broadcasts, the saved popup window,
    // the popup's input and owner components, the caches) go back then
    struct WmObj { EeWonWm* p; WmObj() : p(NULL) {} ~WmObj() { ee_won_wm_free(this->p); } };
    WmObj mWmObj;
    intptr_t WmCall(EeWonWmCall c, intptr_t a = 0, intptr_t b = 0, intptr_t cc = 0, intptr_t d = 0) const
    { return ee_won_wm_call(this->mWmObj.p, c, a, b, cc, d); }
    intptr_t WmField(EeWonWmField f) const { return this->WmCall(EeWonWmCall::GetField, (intptr_t)f); }
    void WmSet(EeWonWmField f, intptr_t v) { this->WmCall(EeWonWmCall::SetField, (intptr_t)f, v); }
    SmartPtr<ColorScheme> mColorScheme; //20
    MultiImageDecoder mImageDecoder; // at +0x24..0x57 (0x34 bytes)
    SmartPtr<SoundDecoder> mSoundDecoder; //58
    SmartPtr<NativeIcon> mDefaultWindowIcon; //60
    // the default font (+5c), the named fonts (+64) and the native, raw and delay-load image caches (+70, +7c, +88)
    // with their count (+94) are the Rust manager's (wmobj.rs, wmcache.rs; R6 WON round 33); so are the event lock
    // (+08), the timer and input ticks (+98, +9c), the valid and full-screen flags (+a1, +ad), the screen sizes
    // (+b0..+bc), the default window and the window count (+fc, +100), the window set (+104), the event list
    // (+110), the timer sets (+11c, +128) and their flag (+134) and the broadcast set (+13c) (wmloop.rs; round 34)
    SmartPtr<WindowEventBase> mCloseEvent; //138
};
// 0x361250: the default window manager, out of line (EELobbyInit 0x295ce3 calls it; a 6-byte body that reads
// gDefaultWindowManager; its WON-side source is not written yet)
WindowManager* GetLobbyWindow();


// a window's icon (0x0c bytes; vtable 0x85107c)
class NativeIcon : public RefCount {
public:
    NativeIcon(DisplayContext* inContext);
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
    bool CanClose() { return this->WCall(EeWonWinCall::CanClose) != 0; } //32ab80
    void CheckResize(DWORD t) { this->WCall(EeWonWinCall::CheckResize, (intptr_t)(uint32_t)t); } //32ac90
    void SuspendRootResize(bool s) { this->WCall(EeWonWinCall::SuspendRootResize, s ? 1 : 0); }
    NativeImage* GetOffscreenImage() { return this->mOffscreenImage.get(); }
    Graphics& GetGraphics();
    void RemoveComponent(Component* c) { this->WCall(EeWonWinCall::RemoveComponent, (intptr_t)c); }
    bool IsMouseDown(MouseButton b) { return this->WCall(EeWonWinCall::IsMouseDown, (intptr_t)b) != 0; }
    void ShowNativeCursor(bool s) { this->WCall(EeWonWinCall::ShowNativeCursor, s ? 1 : 0); } // 0x32afc0
    void CheckSetCursor(bool f) { this->WCall(EeWonWinCall::CheckSetCursor, f ? 1 : 0); }
    bool CheckDoubleClick(MouseButton b) { return this->WCall(EeWonWinCall::CheckDoubleClick,(intptr_t)b)!=0; }
    void HandleMouseExit() { this->WCall(EeWonWinCall::HandleMouseExit); } //32b120
    void HandleKeyEvent(KeyEvent*); //32b170
    void HandleMouseDown(MouseEvent*); //32b1c0
    void HandleMouseEvent(MouseEvent*); //32b260
    void HandleInputEvent(InputEvent*); //32b390

    Window();
    virtual ~Window();                     // 0x32a020, out of line
    virtual SmartPtr<Font> PlatformGetFont(const FontDescriptor&) { return NULL; } // (the platform windows override these)
    virtual bool IdleUpdate() { return this->WCall(EeWonWinCall::IdleUpdate) != 0; }
    virtual void TimerEvent(DWORD e) { this->WCall(EeWonWinCall::TimerEvent, (intptr_t)(uint32_t)e); }
    virtual void PlatformSetTitle() {}
    virtual void PlatformSetIcon() {}
    virtual void PlatformSetCursor(Cursor*) {}
    virtual void PlatformShowNativeCursor(bool) {}
    virtual void PlatformShow(bool) {}
    virtual void PlatformMinimize(bool) {}
    virtual void PlatformSetTopMost(bool) {}
    virtual void PlatformClose() { this->WCall(EeWonWinCall::PlatformClose); }
    virtual void Create(const WONRectangle&);
    virtual void SetPosSize(int, int, int, int) {}
    virtual void GetRect(WONRectangle&, bool) {}
    virtual void PlatformSetFocus() {}
    virtual void Show(bool s) { this->WCall(EeWonWinCall::Show, s ? 1 : 0); }
    void Minimize(bool m) { this->WCall(EeWonWinCall::Minimize, m ? 1 : 0); } // 0x32afb0
    DisplayContext* GetDisplayContext() { return this->mDisplayContext.get(); } // inline, at +0x28
    virtual void PlatformSetMouseCapture(bool) {}
    virtual void QueueEvent(WindowEventBase*);
    virtual bool PlatformIsVisible() { return this->IsShowing(); } // whether last shown
    static Window* GetDefaultWindow();
    SmartPtr<NativeImage> DecodeImage(const char*);
    int RunDialog(Component* c) { return this->mManager->DoDialog(this, c); } // DoDialog in the WON SDK
    void AddComponent(Component* c) { this->WCall(EeWonWinCall::AddComponent, (intptr_t)c); } // 0x32b040
    void SetModal(bool m) { this->WCall(EeWonWinCall::SetModal, m ? 1 : 0); } // 0x32b060
    bool Close() { return this->WCall(EeWonWinCall::Close) != 0; } // 0x32ac30
    void SetClientSize(int w, int h) { this->WCall(EeWonWinCall::SetClientSize, w, h); }
    void Center(bool p) { this->WCall(EeWonWinCall::Center, p ? 1 : 0); }
    void EndDialog(int, Component*);
    void OpenPopup(PopupParams&);
    void ClosePopup(Component*);
    void InvalidateRoot() { this->WCall(EeWonWinCall::InvalidateRoot); }
    void SetParent(Window* p) { this->WCall(EeWonWinCall::SetParent, (intptr_t)p); } // 0x32a210
    void SetTitle(const GUIString&);
    void SetIcon(NativeIcon*);
    // R6 WON round 32: the state is the Rust window (rust/crates/ee_won/src/winobj.rs); these read and set it
    int GetKeyMod() const { return (int)this->WField(EeWonWinField::KeyMod); }
    int GetWindowState() const { return (int)this->WField(EeWonWinField::State); }
    int GetMouseX() const { return (int)this->WField(EeWonWinField::MouseX); }
    int GetMouseY() const { return (int)this->WField(EeWonWinField::MouseY); }
    bool IsMouseInClientArea() const { return this->WField(EeWonWinField::InClient) != 0; }
    bool IsShowing() const { return this->WField(EeWonWinField::IsShowing) != 0; }
    void SetIsShowing(bool inShowing) { this->WSet(EeWonWinField::IsShowing, inShowing ? 1 : 0); }
    bool IsPopup() const { return this->WField(EeWonWinField::IsPopup) != 0; }
    void SetIsPopup(bool inPopup) { this->WSet(EeWonWinField::IsPopup, inPopup ? 1 : 0); }
    bool IsOneComponentWindow() const { return this->WField(EeWonWinField::OneComponent) != 0; }
    void SetOneComponentWindow(bool inOne) { this->WSet(EeWonWinField::OneComponent, inOne ? 1 : 0); }
    int GetStyle() const { return (int)this->WField(EeWonWinField::Style); }
    void SetStyle(int inStyle) { this->WSet(EeWonWinField::Style, inStyle); }
    bool GetMinimizeOnResolutionRestore() const { return this->WField(EeWonWinField::MinimizeOnRestore) != 0; }
    Window* GetParent() const { return (Window*)this->WField(EeWonWinField::Parent); }
    enum WindowState { WindowState_Open, WindowState_Closing, WindowState_Closed };
    typedef bool (*AllowCloseFunc)(Window*);
    // the Rust window, freed after every other member (the root's destruction may still call the window)
    struct WinObj { EeWonWindow* p; WinObj() : p(NULL) {} ~WinObj() { ee_won_win_free(this->p); } };
    WinObj mWinObj;
    intptr_t WCall(EeWonWinCall c, intptr_t a = 0, intptr_t b = 0) const { return ee_won_win_call(this->mWinObj.p, c, a, b); }
    intptr_t WField(EeWonWinField f) const { return this->WCall(EeWonWinCall::GetField, (intptr_t)f); }
    void WSet(EeWonWinField f, intptr_t v) { this->WCall(EeWonWinCall::SetField, (intptr_t)f, v); }
    // the members that are C++ objects (the original's offsets)
    SmartPtr<NativeImage> mOffscreenImage; // +0c
    SmartPtr<DisplayContext> mDisplayContext; // +28
    WindowManager* GetWindowManager() { return this->mManager; } // inline
    WindowManager* mManager; // +7c
    SmartPtr<WindowEventBase> mCloseEvent; // +90
    AllowCloseFunc mAllowCloseFunc; // +94
    SmartPtr<NativeIcon> mIcon; // +98
    GUIString mTitle; // +9c
    SmartPtr<RootContainer> mRoot; // +a4
    std::map<FontDescriptor, SmartPtr<Font> > mFontMap; // +a8
    SmartPtr<Font> mDefaultFont; // +b4
};


// WONAPI lobby state functions the game's lobby glue calls
int LobbyRun();                             // 0x362760 (how the lobby was left: 1, 2 or 3)
void LobbySuspend();
void LobbyResume();
void LobbyShutdown();
Container* LobbyGetContainer();
void LobbyStart();
void LobbyGetNetAddresses(std::list<LobbyNetAddress>& outAddresses, bool inFlag);

// 0x361390: the local player (NULL when none)
LobbyPlayer* GetMyLobbyPlayer();
// 0x34a100 (cdecl): a text with its %s-style argument filled in (provisional name)
GUIString LobbyFormatString(const GUIString& inFormat, const GUIString& inArg);
void LobbySetGameInProgress(bool inFlag);   // 0x3612b0, SDK SetGameInProgress(bool)
bool LobbyLeaveGame(bool inFlag);
bool LobbyIAmCaptain();
void LobbyNetAdvertiseGame();
void LobbyNotifyStagingChange();

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
    ComponentEvent(Component* inComponent, int inType);
    virtual void Deliver();
    void SetComponent(Component* inComponent);
    int GetControlId() const;
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
inline void WindowManager::SetDefaultFont(Font* font) {this->WmCall(EeWonWmCall::SetDefaultFont,(intptr_t)font);} // the Rust manager's

// FontConfig (0x98 bytes): the factory 0x38a160 allocates it, constructor 0x38a1c0, implicit destructor 0x38a250.
class FontConfig : public ComponentConfig {
public:
    static ComponentConfig* CfgFactory();
    static Component* CompFactory();
    FontConfig();
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
inline int WrappedText::GetHeight() { int h=ee_won_wrap_height(this->mObj); return h==0 ? this->mFont->GetHeight() : h; }
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
// ImageComponent's image setters (the component sized to a non-null image) and its desired size (the image's, or
// 10 x 10 without one)
inline ImageComponent::ImageComponent(Image* i,bool t) { this->SetImageAndTile(i,t); this->mStretch=false; }
inline void ImageComponent::GetDesiredSize(int& w,int& h) { if(this->mImage.get()!=NULL) { w=this->mImage->mWidth; h=this->mImage->mHeight; } else w=h=10; }
inline void ImageComponent::SetImageAndTile(Image* i,bool t) { this->mImage=i; this->mTile=t; if(i!=NULL) this->SetSize(i->mWidth,i->mHeight); }
inline void ImageComponent::SetImage(Image* i) { this->mImage=i; if(i!=NULL) this->SetSize(i->mWidth,i->mHeight); }
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
    virtual void Paint(Graphics&, ListArea*);
    virtual void CalcDimensions(ListArea*);
    virtual int Compare(const ListItem*, ListArea*) const;
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
    MenuItem(const GUIString&,ComponentListener*);
    virtual void Paint(Graphics&,ListArea*);     // 0x2fcf10, vtable +0x04
    virtual void CalcDimensions(ListArea*); // 0x2fcee0, vtable +0x08
    SmartPtr<PopupMenu> mSubMenu;            // +28
    int mMenuId;                            // +2c
    bool mDisabled; // at +0x30 (written at 0x2fcda4, read at 0x2fd018); +0x31..0x33 padding
    SmartPtr<ComponentListener> mListener; // at +0x34
};

// ScrollArea (R6 WON round 25): its offsets, content, padding, view, scroller (borrowed) and scroll amounts are the
// Rust ScrollArea (rust/crates/ee_won/src/paneobj.rs); this is the type the lists, text areas, scrollers and configs
// hold. The viewport and the background stay C++ members.
class ScrollArea : public Container {
public:
    ScrollArea(); // 0x304df0: viewport setup
    virtual ~ScrollArea(); // the implicit destructor (0x2fa790)
    virtual void AdjustPosition(bool b) { this->ACall(EeWonPaneCall::AdjustPosition,b); } // 305030
    virtual void ScrollSizeChanged() {} // 4142a0
    virtual void ScrollPaint(Graphics&) {} // 2fa500
    virtual void Virtual08() { this->ACall(EeWonPaneCall::SizeChanged); } //3051c0 SizeChanged
    virtual void GetDesiredSize(int& w,int& h) { this->ACall(EeWonPaneCall::DesiredSize,(intptr_t)&w,(intptr_t)&h); } //3054a0
    virtual bool MouseWheel(int n) { return this->ACall(EeWonPaneCall::MouseWheel,n)!=0; } //305410
    virtual void Paint(Graphics& g) { this->ACall(EeWonPaneCall::Paint,(intptr_t)&g); } // 0x3052d0 (override)
    void SetDelegatePos() { this->ACall(EeWonPaneCall::SetDelegatePos); } //304fd0
    bool OnBottom() { return this->ACall(EeWonPaneCall::OnBottom)!=0; } //305190
    void SetBorderPadding(int l,int t,int r,int b) { int p[4]={l,t,r,b}; this->ACall(EeWonPaneCall::SetBorderPadding,(intptr_t)p); } //305270
    void ScrollTranslate(int& x,int& y) { this->ACall(EeWonPaneCall::ScrollTranslate,(intptr_t)&x,(intptr_t)&y); } //305390
    void ScrollUntranslate(int& x,int& y) { this->ACall(EeWonPaneCall::ScrollUntranslate,(intptr_t)&x,(intptr_t)&y); } //3053d0
    virtual void HandleComponentEvent(ComponentEvent* e) { Component::HandleComponentEvent(e); } // 0x305180, vtable +0x7c override
    void SetSizeEqualToTotal(int w,int h) { this->ACall(EeWonPaneCall::SetSizeEqualToTotal,w,h); } //305450
    bool IsRectVisible(const WONRectangle& r) { return this->ACall(EeWonPaneCall::IsRectVisible,(intptr_t)&r)!=0; } //3054f0
    ScrollAreaViewport* GetViewport() { return this->mViewport.get(); }
    void SetScrollDimension(int w,int h) { this->ACall(EeWonPaneCall::SetScrollDimension,w,h); } // sets the content dimensions
    void SetScrollbar(Scrollbar* s) { this->ACall(EeWonPaneCall::SetScrollbar,(intptr_t)static_cast<Component*>(s)); } // selects a scrollbar scroller
    void SetScroller(Scroller* s) { this->ACall(EeWonPaneCall::SetScroller,(intptr_t)static_cast<Component*>(s)); } // 0x305010; not owned
    void SetVertOffset(int v) { this->ACall(EeWonPaneCall::SetVertOffset,v); } //305130
    void SetOffsets(int h,int v) { this->ACall(EeWonPaneCall::SetOffsets,h,v); } //305150, horizontal then vertical
    // the fields the lists, text areas, scrollers and lobby controls read and set
    int GetField(EeWonGuiField f) { return (int)this->ACall(EeWonPaneCall::GetField,(intptr_t)f); }
    void SetField(EeWonGuiField f,int v) { this->ACall(EeWonPaneCall::SetField,(intptr_t)f,v); }
    int GetTotalHeight() { return this->GetField(EeWonGuiField::TotalHeight); }
    int GetTotalWidth() { return this->GetField(EeWonGuiField::TotalWidth); }
    int GetLeftPad() { return this->GetField(EeWonGuiField::LeftPad); }
    int GetRightPad() { return this->GetField(EeWonGuiField::RightPad); }
    int GetTopPad() { return this->GetField(EeWonGuiField::TopPad); }
    void SetPads(int l,int t,int r,int b) { this->SetField(EeWonGuiField::LeftPad,l); this->SetField(EeWonGuiField::TopPad,t); this->SetField(EeWonGuiField::RightPad,r); this->SetField(EeWonGuiField::BottomPad,b); } // the fields only (the text area's and list's constructors)
    int GetVertOffset() { return this->GetField(EeWonGuiField::VertOffset); }
    int GetHorzOffset() { return this->GetField(EeWonGuiField::HorzOffset); }
    int GetViewWidth() { return this->GetField(EeWonGuiField::ScrollWidth); }
    int GetViewHeight() { return this->GetField(EeWonGuiField::ScrollHeight); }
    SmartPtr<ScrollAreaViewport> mViewport; // at +0x84
    Background mBackground;
    void SetBackground(const Background& theBackground) { this->mBackground=theBackground; }
    EeWonScrollPane* mPane;
private:
    intptr_t ACall(EeWonPaneCall c,intptr_t a=0,intptr_t b=0,intptr_t cc=0) { return ee_won_pane_call(this->mPane,c,a,b,cc); }
};

enum ListSortType { ListSortType_Ascending, ListSortType_Descending, ListSortType_None };
// ListAreaToolTip (0x38 bytes): allocated at 0x2fbbb6, constructor 0x2fb7b0,
// Component (0x30) plus the ListArea pointer at +0x30 and the owned item at +0x34; 43 vtable slots.
class ListAreaToolTip : public Component {
public:
    ListAreaToolTip(ListArea*);
    virtual void Paint(Graphics&); // 0x2fb8d0, vtable +0x18
    void DoTip(ListItem*); //2fb9b0
    void EndTip(); //2fbab0
    ListArea* mListArea; //30 borrowed
    SmartPtr<ListItem> mItem; // at +0x34
};
// ListArea (0x164 bytes): ScrollArea (0x110) base plus vectors and SmartPtr members.
// ListArea (R6 WON round 23): its items, top line, flags, batch, paint width, sort type, selection, keyboard and mouse
// items and selection walk are the Rust ListArea (rust/crates/ee_won/src/listobj.rs); this is the type the lobby
// holds and the base of MultiListArea, TreeArea and PopupMenu, with what the configs and paints read (the font, the
// colours, the selection colours, the sort function, the item tool tip). The items stay C++ data.
class ListArea : public ScrollArea {
public:
    ListArea(); // at 0x006fa570 in the original
    virtual ~ListArea();
    typedef int (*SortFunc)(ListArea*, const ListItem*, const ListItem*);
    static int NumberSortFunc(ListArea*,const ListItem*,const ListItem*);
    struct LineSortPred {
        ListArea* mListArea; SortFunc mSortFunc; bool mAscending;
        LineSortPred(ListArea* area,SortFunc func,int type):mListArea(area),mSortFunc(func),mAscending(type==0) {}
        bool operator()(const ListItem* a,const ListItem* b) { if(this->mAscending) return this->mSortFunc(this->mListArea,a,b)<0; else return this->mSortFunc(this->mListArea,a,b)>0; }
    };
    static int DefaultSortFunc(ListArea* area,const ListItem* a,const ListItem* b) { return a->Compare(b,area); }
    SortFunc GetSortFunc() { return this->mSortFunc==NULL ? ListArea::DefaultSortFunc : this->mSortFunc; }
    virtual void AdjustPosition(bool b) { this->Call(EeWonListCall::AdjustPosition,b); }
    virtual void Virtual08() { this->Call(EeWonListCall::SizeChanged); } // SizeChanged
    virtual void ScrollPaint(Graphics& g) { this->Call(EeWonListCall::ScrollPaint,(intptr_t)&g); }
    virtual void MouseEnter(int x,int y) { this->Call(EeWonListCall::MouseEnter,x,y); }
    virtual void MouseExit() { this->Call(EeWonListCall::MouseExit); }
    virtual void MouseDown(int x,int y,int b) { this->Call(EeWonListCall::MouseDown,x,y,b); }
    virtual void MouseUp(int x,int y,int b) { this->Call(EeWonListCall::MouseUp,x,y,b); }
    virtual void MouseMove(int x,int y) { this->Call(EeWonListCall::MouseMove,x,y); }
    virtual void MouseDrag(int x,int y) { this->Call(EeWonListCall::MouseDrag,x,y); }
    virtual bool KeyDown(int k) { return this->Call(EeWonListCall::KeyDown,k)!=0; }
    virtual void NotifyMouseItemGone() { this->Call(EeWonListCall::NotifyMouseItemGone); }
    virtual void ListSelChangedHook() {}
    virtual bool Sort() { return this->Call(EeWonListCall::Sort)!=0; }
    void SetSortType(ListSortType t) { this->Call(EeWonListCall::SetSortType,t); }
    void BeginUpdate() { this->Call(EeWonListCall::BeginUpdate); }
    void EndUpdate() { this->Call(EeWonListCall::EndUpdate); }
    void Clear(bool fire) { this->Call(EeWonListCall::Clear,fire); }
    ListItem* GetItem(int i) { return (ListItem*)this->Call(EeWonListCall::GetItem,i); }
    ListItem* InsertItem(ListItem* item,int pos=-1) { return (ListItem*)this->Call(EeWonListCall::InsertItem,(intptr_t)item,pos); }
    void RefreshItem(ListItem* item) { this->Call(EeWonListCall::RefreshItem,(intptr_t)item); }
    void DeleteItem(int pos) { this->Call(EeWonListCall::DeleteItem,pos); }
    void SetListFlags(unsigned long flags,bool on) { this->Call(EeWonListCall::SetListFlags,(intptr_t)(int)flags,on); }
    void RemoveItem(ListItem* item) { this->Call(EeWonListCall::RemoveItem,(intptr_t)item); }
    ListItem* InsertString(const GUIString& text,int pos=-1) { return this->InsertItem(new TextListItem(text),pos); }
    int GetNumItems() const { return (int)const_cast<ListArea*>(this)->Call(EeWonListCall::NumItems); }
    int GetItemPos(ListItem* item) { return (int)this->Call(EeWonListCall::GetItemPos,(intptr_t)item); }
    void SetSelItem(int index,bool fire=true) { this->Call(EeWonListCall::SetSelItemAt,index,fire); }
    void EnsureVisible(ListItem* item) { this->Call(EeWonListCall::EnsureVisible,(intptr_t)item); }
    void ClearSelection(bool fire=true) { this->Call(EeWonListCall::ClearSelection,fire); }
    bool SetSelItem(ListItem* item,bool add,bool fire) { return this->Call(EeWonListCall::SetSelItem,(intptr_t)item,add,fire)!=0; }
    void SelectRange(int start,int end) { this->Call(EeWonListCall::SelectRange,start,end); }
    ListItem* FindItemAt(int y) { return (ListItem*)this->Call(EeWonListCall::FindItemAt,y); }
    bool IsItemVisible(ListItem* item) { return this->Call(EeWonListCall::IsItemVisible,(intptr_t)item)!=0; }
    void EnforceInvariants() { this->Call(EeWonListCall::EnforceInvariants); }
    void DeleteRange(int start,int end) { this->Call(EeWonListCall::DeleteRange,start,end); }
    void RewindSelections() { this->Call(EeWonListCall::RewindSelections); }
    bool HasMoreSelections() { return this->Call(EeWonListCall::HasMoreSelections)!=0; }
    ListItem* GetNextSelection() { return (ListItem*)this->Call(EeWonListCall::GetNextSelection); }
    int GetTextColor(); //2fb520
    int GetItemPosAtYPos(int y) { return (int)this->Call(EeWonListCall::GetItemPosAtYPos,y); }
    ListItem* FindSelectableItem(int start,bool up,bool opposite=true) { return (ListItem*)this->Call(EeWonListCall::FindSelectableItem,start,up,opposite); }
    bool ListFlagSet(int flags) { return (this->Call(EeWonListCall::ListFlags)&flags)!=0; }
    ListItem* GetSelItem() { return (ListItem*)this->Call(EeWonListCall::SelItem); }
    ListItem* GetMouseItem() { return (ListItem*)this->Call(EeWonListCall::MouseItem); }
    const SelectionColor& GetSelColor() { return this->mSelColor; } // inline
    void SetFont(Font* theFont) { this->mFont=theFont; }
    int GetPaintWidth() const { return (int)const_cast<ListArea*>(this)->Call(EeWonListCall::PaintWidth); } // the layout width the items paint to
    void SetPaintWidth(int w) { this->Call(EeWonListCall::SetPaintWidth,w); } // a subclass's own paint
    ListSortType GetSortType() const { return static_cast<ListSortType>(const_cast<ListArea*>(this)->Call(EeWonListCall::SortType)); }
    void SetSortTypeOnly(ListSortType t) { this->Call(EeWonListCall::SetSortTypeOnly,t); } // without sorting (MultiListArea::SetSortColumn)
    void ClearMouseItem() { this->Call(EeWonListCall::ClearMouseItem); } // the field only (not the virtual NotifyMouseItemGone)
    SmartPtr<ListAreaToolTip> mItemToolTip; // made on the first item tip
    SmartPtr<Font> mFont;
    int mTextColor;
    SelectionColor mSelColor;
    SortFunc mSortFunc;
    EeWonList* mRust;
private:
    intptr_t Call(EeWonListCall c,intptr_t a=0,intptr_t b=0,intptr_t cc=0) { return ee_won_list_call(this->mRust,c,a,b,cc); }
};


// TreeArea and TreeItem (R6 WON round 24): the rows' parents, indents, indices, open states, wrapped items and
// children, the tree's root, flags, sort type and hovered item, and every method are the Rust objects of
// rust/crates/ee_won/src/treeobj.rs; these are the types the room control and the config hold, with one-line
// forwarders.
class TreeArea;
class TreeItem : public ListItem {
public:
    TreeItem(); // 0x3140f0: a closed row at indent 0 with no parent and no index (the Rust row, GuiHost.cpp)
    virtual ~TreeItem();
    virtual void Paint(Graphics& g,ListArea* l) { this->NCall(l,EeWonTNodeCall::Paint,(intptr_t)&g); }
    virtual void CalcDimensions(ListArea* l) { this->NCall(l,EeWonTNodeCall::CalcDimensions); }
    virtual void SetViewport(ListArea* l,Container* v) { this->NCall(l,EeWonTNodeCall::SetViewport,(intptr_t)v); }
    virtual int Compare(const ListItem* o,ListArea* l) const { return (int)const_cast<TreeItem*>(this)->NCall(l,EeWonTNodeCall::Compare,(intptr_t)o); }
    virtual bool CanSelect(ListArea* l,int x,int y) { return this->NCall(l,EeWonTNodeCall::CanSelect,x,y)!=0; }
    virtual void SetPos(int x,int y) { this->NCall(NULL,EeWonTNodeCall::SetPos,x,y); }
    virtual void MouseEnter(ListArea* l,int x,int y) { this->NCall(l,EeWonTNodeCall::MouseEnter,x,y); }
    virtual void MouseExit(ListArea* l) { this->NCall(l,EeWonTNodeCall::MouseExit); }
    virtual void MouseMove(ListArea* l,int x,int y) { this->NCall(l,EeWonTNodeCall::MouseMove,x,y); }
    virtual void MouseDrag(ListArea* l,int x,int y) { this->NCall(l,EeWonTNodeCall::MouseDrag,x,y); }
    virtual void MouseDown(ListArea* l,int x,int y,int b) { this->NCall(l,EeWonTNodeCall::MouseDown,x,y,b); }
    virtual void MouseUp(ListArea* l,int x,int y,int b) { this->NCall(l,EeWonTNodeCall::MouseUp,x,y,b); }
    TreeItem* GetChild(int i) { return (TreeItem*)this->NCall(NULL,EeWonTNodeCall::GetChild,i); }
    TreeItem* GetNextNonChild() { return (TreeItem*)this->NCall(NULL,EeWonTNodeCall::GetNextNonChild); }
    void CloseAll() { this->NCall(NULL,EeWonTNodeCall::CloseAll); }
    bool IsLastChild() { return this->NCall(NULL,EeWonTNodeCall::IsLastChild)!=0; }
    bool ContainsPoint(int x,int y) { return this->NCall(NULL,EeWonTNodeCall::ContainsPoint,x,y)!=0; }
    bool IsOpen() { return this->NCall(NULL,EeWonTNodeCall::IsOpen)!=0; }
    ListItem* GetWrappedItem() { return (ListItem*)this->NCall(NULL,EeWonTNodeCall::Item); } // mItem
    int GetNumChildren() { return (int)this->NCall(NULL,EeWonTNodeCall::NumChildren); }
    TreeItem* GetParentItem() { return (TreeItem*)this->NCall(NULL,EeWonTNodeCall::Parent); } // mParent
    int GetIndex() { return (int)this->NCall(NULL,EeWonTNodeCall::Index); } // mIndex
    EeWonTNode* mNode;
private:
    intptr_t NCall(ListArea* l,EeWonTNodeCall c,intptr_t a=0,intptr_t b=0,intptr_t cc=0);
};
class TreeItemOpenedEvent : public ComponentEvent {
public:
    TreeItemOpenedEvent(Component* inComponent,TreeItem* inItem)
        :ComponentEvent(inComponent,ComponentEvent_TreeItemOpened),mItem(inItem) {}
    // Implicit destructor (0x314a60): no own vptr write.
    SmartPtr<TreeItem> mItem; // at +0x20; the event is 0x24 bytes
};
// TreeArea (0x174 bytes in the original); constructor 0x314ea0.
class TreeArea : public ListArea {
public:
    TreeArea(); // 0x314ea0: the root row, open; list flags 0x30 off and 0x200 on; tree flags 3
    virtual ~TreeArea();
    virtual void HandleComponentEvent(ComponentEvent* e) { this->TCall(EeWonTreeCall::HandleEvent,(intptr_t)e); }
    virtual void NotifyMouseItemGone() { this->TCall(EeWonTreeCall::NotifyMouseItemGone); }
    virtual bool Sort() { return false; }
    void SetSortType(ListSortType t) { this->TCall(EeWonTreeCall::SetSortType,t); }
    void Sort(TreeItem* i,bool recursive=false) { this->TCall(EeWonTreeCall::SortItem,(intptr_t)i,recursive); } // sorts the visible subtree
    TreeItem* InsertString(TreeItem*,const GUIString&,Image* =NULL,int=-1);
    void CloseItem(TreeItem* i,bool all=false,bool send=true) { this->TCall(EeWonTreeCall::CloseItem,(intptr_t)i,all,send); }
    void SetTreeFlags(int f,bool on) { this->TCall(EeWonTreeCall::SetTreeFlags,f,on); }
    TreeItem* InsertTreeItem(TreeItem* p,ListItem* i,int pos=-1) { return (TreeItem*)this->TCall(EeWonTreeCall::InsertTreeItem,(intptr_t)p,(intptr_t)i,pos); }
    void RemoveTreeItem(TreeItem* i) { this->TCall(EeWonTreeCall::RemoveTreeItem,(intptr_t)i); }
    void ClearTree(bool=true) { this->TCall(EeWonTreeCall::RemoveAllItems,0); } // 0x3151a0 (the flag unused)
    void RemoveAllItems(TreeItem* p) { this->TCall(EeWonTreeCall::RemoveAllItems,(intptr_t)p); }
    void OpenItem(TreeItem* i,bool send=true) { this->TCall(EeWonTreeCall::OpenItem,(intptr_t)i,send); }
    void ToggleItem(TreeItem* i,bool all=false,bool send=true) { this->TCall(EeWonTreeCall::ToggleItem,(intptr_t)i,all,send); }
    bool TreeFlagSet(int flags) { return (this->TCall(EeWonTreeCall::TreeFlags)&flags)!=0; }
    EeWonTree* mTree;
private:
    intptr_t TCall(EeWonTreeCall c,intptr_t a=0,intptr_t b=0,intptr_t cc=0) { return ee_won_tree_call(this->mTree,c,a,b,cc); }
};
inline intptr_t TreeItem::NCall(ListArea* l,EeWonTNodeCall c,intptr_t a,intptr_t b,intptr_t cc)
{
    return ee_won_tnode_call(this->mNode,l ? static_cast<TreeArea*>(l)->mTree : NULL,c,a,b,cc);
}

// PopupMenu (R6 WON round 24): the open submenu and its item, the swallowed first release, the submenu timer, the last
// mouse place, the keyboard mode and the input methods are the Rust PopupMenu (rust/crates/ee_won/src/menuobj.rs);
// this is the type the lobby and the window manager hold, with its constructor's look (font, flags, background,
// colour, padding) and the item adders, with one-line forwarders.
class PopupMenu : public ListArea {
public:
    PopupMenu(); // 0x2fd150: default configuration
    virtual ~PopupMenu();
    virtual void MouseDown(int x,int y,int b) { this->PCall(EeWonMenuCall::MouseDown,x,y,b); }
    virtual void MouseUp(int x,int y,int b) { this->PCall(EeWonMenuCall::MouseUp,x,y,b); }
    virtual void MouseMove(int x,int y) { this->PCall(EeWonMenuCall::MouseMove,x,y); }
    virtual bool TimerEvent(int d) { return this->PCall(EeWonMenuCall::TimerEvent,d)!=0; }
    virtual void HandleComponentEvent(ComponentEvent* e) { this->PCall(EeWonMenuCall::HandleEvent,(intptr_t)e); }
    virtual void ListSelChangedHook() { this->PCall(EeWonMenuCall::ListSelChangedHook); }
    MenuItem* AddItem(const GUIString&,int); //2fd440,ret8
    MenuItem* AddItem(const GUIString&,ComponentListener*,int);
    MenuItem* AddItem(const GUIString&,NativeImage*,ComponentListener*,int); //2fd480,ret10
    void AddSeperator(); //2fd8b0,ret0
    void EndSubMenu() { this->PCall(EeWonMenuCall::EndSubMenu); }
    void CheckSubMenu() { this->PCall(EeWonMenuCall::CheckSubMenu); }
    bool TryKeyDown(int k) { return this->PCall(EeWonMenuCall::TryKeyDown,k)!=0; } // the keys, the submenu's first
    void Reset() { this->PCall(EeWonMenuCall::Reset); }
    EeWonMenu* mMenu;
private:
    intptr_t PCall(EeWonMenuCall c,intptr_t a=0,intptr_t b=0,intptr_t cc=0) { return ee_won_menu_call(this->mMenu,c,a,b,cc); }
};

// ScrollAreaViewport (R6 WON round 26): the scrolled content's container. Its scroll area and the repaint of a child
// in view through nested viewports are the Rust container's viewport part (ctobj.rs); the class is 0x88 bytes in the
// original (allocated at 0x304e49). Paint, TimerEvent, SetFocusChild and AddTimerChild were the container's; Reset
// (0x305c70) had no caller.
class ScrollAreaViewport : public Container {
public:
    ScrollAreaViewport(ScrollArea* area) { this->CCall(EeWonCtCall::SetViewportArea,(intptr_t)static_cast<Component*>(area)); } // inline (0x304e70..0x304e83)
    Component* GetScrollArea() { return (Component*)this->CCall(EeWonCtCall::GetField,(intptr_t)EeWonCtField::ViewportArea); }
    virtual void SetCursor(Cursor* c) { this->GetScrollArea()->SetCursor(c); } // vtable +0x78, 0x305de0: the area's
    virtual void ChildStateChanged(Component*) {} // vtable +0xbc, 0x2fa500: ignored
    virtual bool ChildInvalidated(Component* c) { return this->CCall(EeWonCtCall::VpChildInvalidated,(intptr_t)c)!=0; } // vtable +0xc0, 0x305d70
};

// MultiListArea, MultiListItem and MultiListHeader (R6 WON round 24): their columns, cells, header buttons, sort,
// paint and drags are the Rust objects of rust/crates/ee_won/src/multiobj.rs; these are the types the lobby, the
// configs and the adapter list hold, with one-line forwarders.
class MultiListItem;
class MultiListHeader;
// MultiListArea (0x1a4 bytes in the original).
class MultiListArea : public ListArea {
public:
    MultiListArea(int=1);
    virtual ~MultiListArea();
    virtual void NotifyMouseItemGone() { this->MCall(EeWonMultiCall::NotifyMouseItemGone); }
    void SetNumColumns(int n) { this->MCall(EeWonMultiCall::SetNumColumns,n); }
    void SetColumnSizeMode(int c,unsigned char m) { this->MCall(EeWonMultiCall::SetColumnSizeMode,c,m); }
    MultiListItem* InsertRow(int=-1); // inserts a blank row
    void SetColumnWidth(int c,int w,bool fromSize) { this->MCall(EeWonMultiCall::SetColumnWidth,c,w,fromSize); }
    int GetColumnPaintContext() { return (int)this->MCall(EeWonMultiCall::PaintContext); }
    int GetSortColumn() { return (int)this->MCall(EeWonMultiCall::SortColumn); }
    int GetSortColumnPrev() { return (int)this->MCall(EeWonMultiCall::SortColumnPrev); }
    void SetDynamicSizeColumn(int c) { this->MCall(EeWonMultiCall::SetDynamicSizeColumn,c); }
    void SetAutoGrowColumns(bool on) { this->MCall(EeWonMultiCall::SetAutoGrowColumns,on); }
    void SetSortColumn(int c,ListSortType t) { this->MCall(EeWonMultiCall::SetSortColumn,c,t); }
    void SetSortColumnFunc(int c,SortFunc f) { this->MCall(EeWonMultiCall::SetSortColumnFunc,c,(intptr_t)f); }
    void SetColumnHAlign(int c,HorzAlign a) { this->MCall(EeWonMultiCall::SetColumnHAlign,c,a); }
    virtual void AdjustPosition(bool b) { this->MCall(EeWonMultiCall::AdjustPosition,b); }
    virtual void Virtual08() { this->MCall(EeWonMultiCall::SizeChanged); } // 0x303be0 SizeChanged
    virtual void ScrollPaint(Graphics& g) { this->MCall(EeWonMultiCall::ScrollPaint,(intptr_t)&g); }
    MultiListItem* UpdateItem(MultiListItem* r) { return (MultiListItem*)this->MCall(EeWonMultiCall::UpdateItem,(intptr_t)r); }
    MultiListItem* SetCell(int r,int c,ListItem* i) { return (MultiListItem*)this->MCall(EeWonMultiCall::SetCell,r,c,(intptr_t)i); }
    int GetColumnAt(int x) { return (int)this->MCall(EeWonMultiCall::ColumnAt,x); }
    int GetColumnPos(int c) { return (int)this->MCall(EeWonMultiCall::ColumnPos,c); }
    int GetColumnWidth(int c) { return (int)this->MCall(EeWonMultiCall::ColumnWidth,c); }
    MultiListItem* InsertItem(MultiListItem* r,int pos=-1);
    MultiListItem* SetString(int,int,const GUIString&,Image* =NULL);
    EeWonMulti* mMulti;
private:
    intptr_t MCall(EeWonMultiCall c,intptr_t a=0,intptr_t b=0,intptr_t cc=0) { return ee_won_multi_call(this->mMulti,c,a,b,cc); }
};

class MultiListItem : public ListItem {
public:
    MultiListItem(); // the Rust row (GuiHost.cpp)
    virtual ~MultiListItem();
    void InsertItem(ListItem* i) { this->RCall(NULL,EeWonMRowCall::InsertItem,(intptr_t)i); } // appends a cell
    void SetItem(ListItem* i,int c) { this->RCall(NULL,EeWonMRowCall::SetItem,(intptr_t)i,c); }
    ListItem* GetItem(int c) const { return (ListItem*)const_cast<MultiListItem*>(this)->RCall(NULL,EeWonMRowCall::GetItem,c); }
    virtual void Paint(Graphics& g,ListArea* l) { this->RCall(l,EeWonMRowCall::Paint,(intptr_t)&g); }
    virtual void CalcDimensions(ListArea* l) { this->RCall(l,EeWonMRowCall::CalcDimensions); }
    virtual void SetViewport(ListArea* l,Container* v) { this->RCall(l,EeWonMRowCall::SetViewport,(intptr_t)v); }
    virtual int Compare(const ListItem* o,ListArea* l) const { return (int)const_cast<MultiListItem*>(this)->RCall(l,EeWonMRowCall::Compare,(intptr_t)o); }
    virtual bool CanSelect(ListArea* l,int x,int y) { return this->RCall(l,EeWonMRowCall::CanSelect,x,y)!=0; }
    virtual void SetPos(int x,int y) { this->RCall(NULL,EeWonMRowCall::SetPos,x,y); }
    virtual bool Contains(int,int inY) { return inY>=this->mTop && inY<this->mTop+this->mHeight; }
    virtual void MouseEnter(ListArea* l,int x,int y) { this->RCall(l,EeWonMRowCall::MouseEnter,x,y); }
    virtual void MouseExit(ListArea* l) { this->RCall(l,EeWonMRowCall::MouseExit); }
    virtual void MouseMove(ListArea* l,int x,int y) { this->RCall(l,EeWonMRowCall::MouseMove,x,y); }
    virtual void MouseDrag(ListArea* l,int x,int y) { this->RCall(l,EeWonMRowCall::MouseDrag,x,y); }
    virtual void MouseDown(ListArea* l,int x,int y,int b) { this->RCall(l,EeWonMRowCall::MouseDown,x,y,b); }
    virtual void MouseUp(ListArea* l,int x,int y,int b) { this->RCall(l,EeWonMRowCall::MouseUp,x,y,b); }
    EeWonMRow* mRow;
private:
    intptr_t RCall(ListArea* l,EeWonMRowCall c,intptr_t a=0,intptr_t b=0,intptr_t cc=0)
    { return ee_won_mrow_call(this->mRow,l ? static_cast<MultiListArea*>(l)->mMulti : NULL,c,a,b,cc); }
};
inline MultiListItem* MultiListArea::InsertItem(MultiListItem* r,int pos) { ListArea::InsertItem(r,pos); return r; }
inline MultiListItem* MultiListArea::InsertRow(int row) { return this->InsertItem(new MultiListItem,row); }

// MultiListHeader (0xac bytes in the original, the 51 vtable slots of Container; ten overrides).
class MultiListHeader : public Container {
public:
    MultiListHeader(MultiListArea* =NULL);
    virtual ~MultiListHeader();
    virtual void Virtual08() { this->HCall(EeWonMHeadCall::SizeChanged); } // 0x302fb0 SizeChanged
    virtual void Paint(Graphics&); // 0x305e00 (the container paint)
    virtual void MouseExit() { this->HCall(EeWonMHeadCall::MouseExit); }
    virtual void MouseDown(int x,int y,int b) { this->HCall(EeWonMHeadCall::MouseDown,x,y,b); }
    virtual void MouseUp(int x,int y,int b) { this->HCall(EeWonMHeadCall::MouseUp,x,y,b); }
    virtual void MouseMove(int x,int y) { this->HCall(EeWonMHeadCall::MouseMove,x,y); }
    virtual void MouseDrag(int x,int y) { this->HCall(EeWonMHeadCall::MouseDrag,x,y); }
    virtual void HandleComponentEvent(ComponentEvent* e) { this->HCall(EeWonMHeadCall::HandleEvent,(intptr_t)e); }
    virtual Component* FindChildAt(int,int); // 0x304710 (the container hit test)
    void SetColumnItem(int c,Component* i) { this->HCall(EeWonMHeadCall::SetColumnItem,c,(intptr_t)i); }
    void SetMultiListArea(MultiListArea* a) { this->HCall(EeWonMHeadCall::SetMultiListArea,(intptr_t)a); }
    void NotifyColumnChange() { this->HCall(EeWonMHeadCall::NotifyColumnChange); }
    void SetSortColumn(int c,ListSortType t) { this->HCall(EeWonMHeadCall::SetSortColumn,c,t); }
    static ListSortType GetSortTypeForButton(Component*);
    int GetColumnIndex(Component* c) { return (int)this->HCall(EeWonMHeadCall::ColumnIndex,(intptr_t)c); }
    ListSortType GetSortType(int c) { return (ListSortType)this->HCall(EeWonMHeadCall::SortType,c); }
    bool DrawSortArrow() { return this->HCall(EeWonMHeadCall::DrawSortArrow)!=0; }
    EeWonMHead* mHead;
private:
    intptr_t HCall(EeWonMHeadCall c,intptr_t a=0,intptr_t b=0,intptr_t cc=0) { return ee_won_mhead_call(this->mHead,c,a,b,cc); }
};

struct GUIStringLessNoCase {
    bool operator()(const GUIString& inLeft, const GUIString& inRight) const
    { return inLeft.CompareNoCase(inRight) < 0; }
};

// Keyboard.h modifiers; the exe tests bits 1 and 2.
enum { KEYMOD_SHIFT=0x0001,KEYMOD_ALT=0x0002,KEYMOD_CTRL=0x0004 };
// The font size a layout gives the page view (FontSize: Smallest..Largest); kept, not used
enum HTMLFontSize {
    HTMLFontSize_Smallest,HTMLFontSize_Small,HTMLFontSize_Medium,
    HTMLFontSize_Large,HTMLFontSize_Largest
};
class TextArea;
// The lobby's page view (layout type BrowserComponent; WONGUI/BrowserComponent.cpp). WON's HTML rendering is not
// ported (docs/RUST_PLAN.md D8): a page is plain text in a TextArea, an HTML page with its tags dropped and entities
// decoded (ee_won's plaintext.rs). There are no links, so no history: back, forward and stop stay disabled.
class BrowserComponent : public Container {
public:
    BrowserComponent();
    virtual ~BrowserComponent();
    virtual void AddedToParent();
    void SetTransparent(bool);
    void SetScroller(Scroller*);
    void SetFontSize(HTMLFontSize size) { this->mFontSize=size; }
    void Reset(); // empties the view
    void Refresh(); // shows the current location again
    void Stop() {}
    void Back() {}
    void Forward() {}
    bool StopButtonEnabled() { return false; }
    bool BackButtonEnabled() { return false; }
    bool ForwardButtonEnabled() { return false; }
    bool FindStr(const GUIString&,bool=true,bool=false) { return false; }
    void Go(const GUIString&,bool=false,bool=false,int=-1); // a file's text; a web address as text
    // the readable ones of two page files, with a blank line between (the MOTD's and the terms of use's pages)
    void ShowFiles(const std::string& inFirst,const std::string& inSecond,bool inSkipFirstCharIfText);
    TextArea* GetHTMLView() { return this->mHTMLView.get(); }
    const GUIString& GetCurStatus() { return this->mCurStatus; }
    const GUIString& GetCurLocation() { return this->mCurLocation; }
protected:
    void SetPageText(const std::wstring&);
    SmartPtr<TextArea> mHTMLView;
    SmartPtr<Scroller> mScroller;
    HTMLFontSize mFontSize;
    bool mHaveBackground;
    GUIString mCurStatus;
    GUIString mCurLocation;
};

// 0x3c5ff0: the sort/filter handlers of a server-list column


// 0x8dec84 / 0x8dec88 / 0x8df4d0 / 0x8d954c: the lobby's object factories


}

#endif
