#ifndef EE_WONAPI_WONLOBBYRESOURCECONFIG_H
#define EE_WONAPI_WONLOBBYRESOURCECONFIG_H
#include "ee/wonapi/WONLobbyGUI.h"
namespace WONAPI {

// Config classes declared for calls only (constructors, destructors and vtables are out of line).
// The readers use +0x98 and +0x9c.
class ScrollAreaConfig : public ContainerConfig {
public:
    virtual bool Virtual24(const std::string&); // at 0x00794360
    virtual void SetComponent(Component*); // defined out of line
    ScrollArea* mScrollArea; //98
};
class ListAreaConfig : public ScrollAreaConfig {
public:
    static Component* CompFactory(); // at 0x0079abf0
    static ComponentConfig* CfgFactory(); // at 0x00782150
    virtual bool Virtual24(const std::string&); //38da70
    virtual void SetComponent(Component*); // defined out of line
    ListSortType ReadSortType(); //38d5d0
    static void ReadListFlags(ComponentConfig*,ListArea*); //38d790
    ListArea* mListArea; //9c
};
// MultiList config classes (only the instruction dispatch is defined here).
class MultiListAreaConfig : public ListAreaConfig {
public:
    static ComponentConfig* CfgFactory(); // at 0x0079ae90
    static Component* CompFactory(); // at 0x0079ae30
    virtual bool Virtual24(const std::string&); // at 0x00790160 (ends at 0x0079048f)
    virtual void SetComponent(Component*); // SetComponent override at 0x0078fd10
    MultiListArea* mMultiListArea; //a0, borrowed prefixa4
};
class MultiListHeaderConfig : public ContainerConfig {
public:
    static ComponentConfig* CfgFactory(); // at 0x0079af50
    static Component* CompFactory(); // at 0x0079aef0
    virtual bool Virtual24(const std::string&); // at 0x00790490 (ends at 0x00790520)
    virtual void SetComponent(Component* component) { this->mHeader=static_cast<MultiListHeader*>(component); ContainerConfig::SetComponent(component); } //39afb0
    MultiListHeader* mHeader; //98, borrowed prefix9c
};
// InputBoxConfig (0x7c bytes).
class InputBoxConfig : public ComponentConfig {
public:
    static Component* CompFactory(); // at 0x0079ab90
    static ComponentConfig* CfgFactory(); // at 0x00782100
    virtual bool Virtual24(const std::string&); //38cd60
    virtual void SetComponent(Component*); // defined out of line
    InputBox* mInputBox; //78
};
// LabelConfig (0x7c bytes): ComponentConfig (0x78) plus a borrowed Label* at +0x78.
// The explicit constructor (0x0078d290) calls the base and installs the vtable 0x00852230.
class LabelConfig : public ComponentConfig {
public:
    LabelConfig(); // at 0x0078d290; leaves +0x78 untouched
    static Component* CompFactory(); // at 0x0079afd0
    static ComponentConfig* CfgFactory(); // at 0x0078f9e0
    virtual bool Virtual24(const std::string&); //38d2b0
    virtual void SetComponent(Component*); // defined out of line
    Label* mLabel; //78
};
// Config base parts; constructors and destructors are out of line.
class ScrollerConfig : public ContainerConfig {
public:
    virtual bool Virtual24(const std::string&); //394540
    virtual void SetComponent(Component*); //original external SetComponent
    Scroller* mScroller; //98, prefix9c
};
// ButtonScrollerConfig (not identified in the original exe).
class ButtonScrollerConfig : public ScrollerConfig {
protected:
    ButtonScroller* mButtonScroller; //borrowed, source model9c after existing ScrollerConfig9c
    ScrollButtonCondition GetScrollButtonCondition();
public:
    virtual void SetComponent(Component* component) { ScrollerConfig::SetComponent(component); this->mButtonScroller=static_cast<ButtonScroller*>(component); }
    virtual bool Virtual24(const std::string&);
};
class ScrollbarScrollerConfig : public ScrollerConfig {
public:
    virtual bool Virtual24(const std::string&); //3948a0
    virtual void SetComponent(Component*); //original external SetComponent
    ScrollbarCondition GetScrollbarCondition(); //3946e0
    ScrollbarScroller* mScrollbarScroller; //9c, prefixa0
};
class ScrollbarConfig : public ContainerConfig {
public:
    ScrollbarConfig(); // at 0x00794900 (0x9c bytes)
    virtual bool Virtual24(const std::string&); //394920
    virtual bool GetPredefinedComponent(const std::string&,ComponentConfig*&,Component*&); //3949c0
    virtual void SetComponent(Component*); //original external SetComponent
    Scrollbar* mScrollbar; //98, prefix9c
};
class TreeArea;
class TreeAreaConfig : public ListAreaConfig {
public:
    static ComponentConfig* CfgFactory(); // at 0x0079b590
    static Component* CompFactory(); // at 0x0079b530
    virtual bool Virtual24(const std::string&); //395f10..395f52
    virtual void SetComponent(Component*); //original external SetComponent
    TreeArea* mTreeArea; //a0, prefixa4
};
}
#endif
