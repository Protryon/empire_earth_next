#ifndef EE_WONAPI_WONLOBBYMSSKINIMAGE_H
#define EE_WONAPI_WONLOBBYMSSKINIMAGE_H
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyImageHelpers.h"
#include "ee/wonapi/WONLobbySkinFrame.h"
#include "ee/wonapi/WONLobbyWONButton.h"
namespace WONAPI {
class WONHeaderButton; class WONScrollbar; class WONInputBox; class WONComboBox;
class WONScrollContainer; class WONListCtrl; class WONTabCtrl; class WONFrameComponent;
// WONSkin (0x80 bytes), the outer skin; field layout as at 0x007152d0. Members are initialized
// naturally for the WONMSSkin constructor; the destructor and copy are out of line.
class WONSkin : public RefCount {
public:
    WONInputBox* GetInputBox(); // at 0x00715890
    WONFrameComponent* GetDialogBack(); // at 0x007155f0
    WONFrameComponent* GetPopupBack(); // at 0x00715580
    WONSimpleButton* GetCloseButton(); // at 0x00715540
    WONHeaderButton* GetHeaderButton(); // at 0x00715520
    WONButton* GetRadioButton(); // at 0x00715510
    WONButton* GetCheckbox(); // at 0x00715500
    WONButton* GetPushButton(); // at 0x007154f0
    virtual ~WONSkin(); // at 0x007152d0
    bool CopyButtonType(const GUIString&,WONButton*); // at 0x00715c90
    bool CopySimpleButtonType(const GUIString&,WONSimpleButton*); // at 0x00715d00
    WONListCtrl* GetListCtrl(); // at 0x00715b20
    WONButton* GetTabButton(); // at 0x00715530; returns an owned duplicate
    WONScrollbar* GetScrollbar(bool); // at 0x007156a0
    WONComboBox* GetComboBox(); // at 0x007158a0
    WONScrollContainer* GetScrollContainer(); // at 0x00715ae0
    WONTabCtrl* GetTabCtrl(); // at 0x00715c10
    WONSimpleButton* GetArrowButton(Direction); // at 0x00715550
    WONSimpleButton* GetScrollThumb(bool); // at 0x00715660
    WONSimpleButton* GetScrollGutter(bool); // at 0x00715680
    SmartPtr<WONButton> mPushButton,mCheckbox,mRadioButton;
    SmartPtr<WONHeaderButton> mHeaderButton;
    SmartPtr<WONButton> mTabButton;
    SmartPtr<WONSimpleButton> mCloseButton,mComboButton;
    SmartPtr<WONSimpleButton> mScrollThumb[2],mScrollGutter[2],mArrowButton[4];
    SmartPtr<WONScrollbar> mScrollbar[2];
    SmartPtr<WONInputBox> mInputBox;
    SmartPtr<WONComboBox> mComboBox;
    SmartPtr<WONScrollContainer> mScrollContainer;
    SmartPtr<WONListCtrl> mListCtrl;
    SmartPtr<WONTabCtrl> mTabCtrl;
    SmartPtr<WONFrameComponent> mPopupBack,mDialogBack;
    typedef std::map<GUIString,SmartPtr<WONButton>,GUIStringLessNoCase> ButtonTypeMap;
    typedef std::map<GUIString,SmartPtr<WONSimpleButton>,GUIStringLessNoCase> SimpleButtonTypeMap;
    ButtonTypeMap mButtonTypeMap; // offsets 0x68..0x73 (VC6 map, 0x0c bytes)
    SimpleButtonTypeMap mSimpleButtonTypeMap; //74..7f
};
class WONMSSkin : public WONSkin {
public:
    WONMSSkin(); // at 0x00715d70: builds the state images, widgets and frames
    // Implicit destructor: no own members; WONSkin base cleanup.
    enum MSImageFlag { MSImageFlag_Down=1,MSImageFlag_Default=2,MSImageFlag_Disabled=4,MSImageFlag_Checked=8 };
    enum MSImageType {
        MSImageType_Frame1,MSImageType_Frame2,MSImageType_Frame3,
        MSImageType_LeftArrow,MSImageType_RightArrow,MSImageType_UpArrow,MSImageType_DownArrow,
        MSImageType_PushButton,MSImageType_Checkbox,MSImageType_RadioButton,
        MSImageType_ScrollThumb,MSImageType_ScrollGutter,MSImageType_ScrollCorner,
        MSImageType_ListHeader,MSImageType_TabButton,MSImageType_TabCtrlFrame,
        MSImageType_ComboPopupFrame,MSImageType_CloseButton
    };
    // MSImage (0x14 bytes): ProgramImage (0x10) + two bytes at +0x10/+0x11 + padding.
    // Implicit destructor (same body as 0x00728770).
    class MSImage : public ProgramImage {
    public:
        unsigned char mImageType,mImageFlags;
        MSImage(unsigned char t,unsigned char f=0,int w=0,int h=0) : ProgramImage(w,h),mImageType(t),mImageFlags(f) {} // inline: 0x14 bytes, ProgramImage constructor 0x00728790, then the bytes at +0x10/+0x11
        virtual SmartPtr<Image> Duplicate(); // at 0x00715a10
        virtual void Paint(Graphics&); // slot 0x14, at 0x007172b0
        void DrawPushButton(Graphics&,bool,bool=false);
        void DrawArrowButton(Graphics&,Direction);
        void DrawCloseButton(Graphics&);
        bool FlagSet(unsigned char flags) { return this->mImageFlags&flags?true:false; }
    };
};
}
#endif
