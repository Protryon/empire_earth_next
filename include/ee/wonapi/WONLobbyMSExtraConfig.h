#ifndef EE_WONAPI_WONLOBBYMSEXTRACONFIG_H
#define EE_WONAPI_WONLOBBYMSEXTRACONFIG_H
#include "ee/wonapi/WONLobbyResourceConfig.h"
#include "ee/wonapi/WONLobbyComboBoxConfig.h"
#include "ee/wonapi/WONLobbyTabCtrlConfig.h"
#include "ee/wonapi/WONLobbyMSPainters.h" // full MSDlgBack (fieldless, 0x30 bytes)
#include "ee/wonapi/WONLobbyMSRemainingPainters.h" // full MSScrollThumb (0x30) for the inline factory
namespace WONAPI {
// Control classes. MSComboBox (0xa8), MSScrollbar (0xbc), MSScrollContainer (0xa4) and MSTabCtrl
// (0xe0) are complete with their own constructors; their destructors are out of line.
// MSComboBox: ComboBox (0xa4) + mDrawFrame at +0xa4 + padding.
class MSComboBox : public ComboBox {
public:
    MSComboBox(); // constructor at 0x006ffb20
    virtual void Virtual08(); // SizeChanged (slot 0x08), at 0x006ffe30
    virtual void GetDesiredSize(int&,int&); // default size, at 0x006ffb00
    virtual void Paint(Graphics&); // slot 0x18, at 0x006ffdf0
    static SmartPtr<Button> (*ArrowButtonFactory)(Direction); // MSArrowButtonFactory; the variable is at 0x008d5e6c (declared only)
    bool mDrawFrame; // offset 0xa4 (written at 0x0078f83a by the inline SetDrawFrame)
    void SetDrawFrame(bool v) { this->mDrawFrame=v; }
};
typedef SmartPtr<Button> (*MSArrowButtonFactory)(Direction); // returns a SmartPtr, takes a Direction
SmartPtr<Button> DefaultMSArrowButtonFactory(Direction); // at 0x006fdc40
// MSScrollbar (0xbc bytes): Scrollbar (0xb8) + bool at +0xb8 + padding.
class MSScrollbar : public Scrollbar {
public:
    MSScrollbar(bool=true); // constructor at 0x006ff820
    static MSArrowButtonFactory ArrowButtonFactory; // variable at 0x008d5e64 (declared only)
    // one-byte class by value: the ctor 0x6ff820 byte copy into a temp + dword push, its inline ctor a site (name is ours)
    struct ThumbType { bool mVertical; ThumbType(bool v) : mVertical(v) {} };
    typedef SmartPtr<Component> (*ThumbFactoryFunc)(ThumbType);
    static SmartPtr<Component> DefaultThumbFactory(ThumbType) { return new MSScrollThumb; } // inline (body at 0x006fccb0)
    static ThumbFactoryFunc ThumbFactory; // variable at 0x008d5e68 (declared only)
    void SetTransparent(bool); // at 0x006ffa60
    virtual void Paint(Graphics&); // slot 0x18, at 0x006ffa70; the only override besides the destructor
protected:
    bool mIsTransparent; // offset 0xb8 (stored at 0x006ffa64, read at 0x006ffa85)
};
class MSEtchedFrameGroup : public Container {
public:
    MSEtchedFrameGroup(); // at 0x007008d0 (0x8c bytes)
    virtual void GetDesiredSize(int&,int&); // GetDesiredSize (slot 0x0c), at 0x00700a60
    virtual void AddedToParent(); // AddedToParent (slot 0x6c), at 0x00700ab0
    virtual void Paint(Graphics&); // slot 0x18, at 0x00700c00
    void SetLabelText(const GUIString&); //300b70
    void AddContainedComponent(Component*); //300ba0
    Label* GetLabel() { return this->mLabel.get(); } // inline, borrowed
    SmartPtr<Label> mLabel; // offset 0x84, owning
    SmartPtr<Component> mComponent; // offset 0x88, owning
};
// MSTabCtrl (0xe0 bytes): TabCtrl (0xd8) + bool at +0xd8 + padding + MSDlgBack pointer at +0xdc.
// Own constructor; the destructor is out of line.
class MSTabCtrl : public TabCtrl {
public:
    MSTabCtrl(); // constructor at 0x007010e0
    virtual void AddedToParent(); // at 0x00701670: jumps to TabCtrl::AddedToParent (0x00708cb0); a real override
    void SetTransparent(bool); // at 0x00701660
    void AddStandardTab(const GUIString&,Container*); // at 0x00701680
    bool mIsTransparent; // offset 0xd8, set by the constructor and the setter; padding to 0xdb
    SmartPtr<MSDlgBack> mBack; // offset 0xdc, owning; NULL after construction, released first by the destructor (0x007014ef)

};
// MSScrollContainer (0xa4 bytes): ScrollbarScroller (0x9c) + int at +0x9c + bools at +0xa0..+0xa2 + padding.
class MSScrollContainer : public ScrollbarScroller {
public:
    MSScrollContainer(ScrollArea* =NULL); // constructor at 0x006ffeb0
    virtual void Virtual08(); // SizeChanged (slot 0x08), at 0x00700290
    virtual void Paint(Graphics&); // slot 0x18, at 0x00700390
    int mHeaderHeight; // offset 0x9c (stored at 0x00790000 and 0x0078fffa)
    bool mDraw3DFrame,mDrawFrame,mTransparent; // offsets 0xa0/0xa1/0xa2 (used at 0x00700254, 0x00700274 and 0x0078fdb2)
    void SetDraw3DFrame(bool); // at 0x00700250; keeps the SizeChanged callback
    void SetDrawFrame(bool); // at 0x00700270; keeps the SizeChanged callback
    void SetTransparent(bool v) { this->mTransparent=v; } // inline
};
class MSHeaderControl : public MultiListHeader {
public:
    MSHeaderControl(MultiListArea* =NULL); // at 0x00700d10 (0xb0 bytes)
    void SetColumn(int,const GUIString&); // at 0x00700dd0
    int mTextLeftPad; // offset 0xac; the allocation at 0x00700eed is 0xb0 bytes

};
class MSListCtrl : public MSScrollContainer {
public:
    MSListCtrl(MultiListArea* =NULL); // at 0x00700e80 (0xa8 bytes)
    virtual void Virtual08(); // SizeChanged (slot 0x08), at 0x007010b0
    void SetHeaderHeight(int v) { this->mHeaderHeight=v; } // inline (used at 0x0078fffa)
    MultiListArea* GetListArea() { return static_cast<MultiListArea*>(this->GetScrollArea()); } // inline, borrowed
    MSHeaderControl* GetHeader() { return this->mHeader.get(); } // inline, borrowed
    SmartPtr<MSHeaderControl> mHeader; // offset 0xa4, retained (used at 0x00790062 and 0x00700fe5); class size 0xa8
};
// Config subclasses (14-slot vtables; declarations only).
class MSComboBoxConfig : public ComboBoxConfig {
public:
    static ComponentConfig* CfgFactory() { return new MSComboBoxConfig; } // inline (body at 0x0078e770, 0xa0 bytes)
    static Component* CompFactory(); // at 0x0078e710
    virtual bool Virtual24(const std::string&); //38f810
    virtual void SetComponent(Component*); // at 0x0078e7d0
    MSComboBox* mMSComboBox; //9c borrowed, prefixa0
};
class MSScrollbarConfig : public ScrollbarConfig {
public:
    static ComponentConfig* CfgFactory(); // at 0x0078ec60
    static Component* CompFactory(); // at 0x0078ec00
    virtual bool Virtual24(const std::string&); //38f880
    virtual void SetComponent(Component*); // at 0x0078f860
    MSScrollbar* mMSScrollbar; //9c borrowed, prefixa0
};
class MSEtchedFrameGroupConfig : public ContainerConfig {
public:
    static ComponentConfig* CfgFactory(); // at 0x0078efe0
    static Component* CompFactory(); // at 0x0078ef80
    virtual bool Virtual24(const std::string&); //38fa40
    virtual bool GetPredefinedComponent(const std::string&,ComponentConfig*&,Component*&); // at 0x0078f980
    virtual void SetComponent(Component*); // at 0x0078f960 (identical body shared with the inherited one)
    MSEtchedFrameGroup* mFrame; //98 borrowed, prefix9c
};
class MSTabCtrlConfig : public TabCtrlConfig {
public:
    static ComponentConfig* CfgFactory() { return new MSTabCtrlConfig; } // inline (body at 0x0078f0a0, 0xa0 bytes)
    static Component* CompFactory(); // at 0x0078f040
    virtual bool Virtual24(const std::string&); //38fb30
    virtual void SetComponent(Component*); // at 0x0078f100
    MSTabCtrl* mMSTabCtrl; //9c borrowed, prefixa0
};
class MSScrollContainerConfig : public ScrollbarScrollerConfig {
public:
    static ComponentConfig* CfgFactory(); // at 0x0078f180
    static Component* CompFactory(); // at 0x0078f120
    virtual bool Virtual24(const std::string&); //38fd30
    virtual bool GetPredefinedComponent(const std::string&,ComponentConfig*&,Component*&); // at 0x0078fdd0
    virtual void SetComponent(Component*); // at 0x0078fd10 (identical body shared with the inherited one)
    MSScrollContainer* mScrollContainer; //a0 borrowed, prefixa4
};
class MSHeaderControlConfig : public MultiListHeaderConfig {
public:
    static ComponentConfig* CfgFactory(); // at 0x0078f240
    static Component* CompFactory(); // at 0x0078f1e0
    virtual bool Virtual24(const std::string&); //38ff00
    virtual void SetComponent(Component*); // at 0x0078f2a0
    MSHeaderControl* mMSHeader; //9c borrowed, prefixa0
};
class MSListCtrlConfig : public MSScrollContainerConfig {
public:
    static ComponentConfig* CfgFactory(); // at 0x0078f320
    static Component* CompFactory(); // at 0x0078f2c0
    virtual bool Virtual24(const std::string&); //38ffd0
    virtual bool GetPredefinedComponent(const std::string&,ComponentConfig*&,Component*&); // at 0x00790020
    virtual void SetComponent(Component*); // defined out of line
    MSListCtrl* mListCtrl; //a4 borrowed, prefixa8
};
}
#endif
