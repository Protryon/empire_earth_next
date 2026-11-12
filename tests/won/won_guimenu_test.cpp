// WONGUI's menus' keys and choices (MenuBehavior.cpp's PopupMenu, MenuBar.cpp): the original's statements (copied
// from the C++ before the port, the menus, items and bar replaced by the flags and numbers they read, their calls
// by logged steps) against ee_won's guimenu module through its C ABI, over generated inputs (a fixed LCG). Prints
// one line per failed check; the exit code is the number of failed checks.
#include <stdio.h>
#include <stddef.h>
#include <vector>
#include "rust/ee_won.h"

static int sFailures;
static int sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static unsigned int sSeed = 99173;
static unsigned int Next() { sSeed = sSeed * 1103515245u + 12345u; return sSeed >> 8; }
static int Any()
{
    switch (Next() % 8) {
    case 0: return (int)Next() - (int)(Next() << 8);
    case 1: return 0x7fffffff - (int)(Next() % 3);
    case 2: return (int)0x80000000 + (int)(Next() % 3);
    default: return (int)(Next() % 300) - 30;
    }
}

// windows.h's keys, WONLobbyGUI.h's event numbers, popup flags and bar states
#define VK_RETURN 0x0D
#define VK_ESCAPE 0x1B
#define VK_LEFT 0x25
#define VK_UP 0x26
#define VK_RIGHT 0x27
#define VK_DOWN 0x28
enum { ComponentEvent_ListSelChanged = 22, ComponentEvent_ListItemUpClicked = 24, ComponentEvent_EndPopup = 30 };
enum { PopupFlag_EndOnEscape = 0x0020, PopupFlag_WantInput = 0x0080, PopupFlag_StandardPopup = 0x23e7 };
enum MenuBarState { MenuBarState_None, MenuBarState_Up, MenuBarState_Down };

static int Key()
{
    static const int kKeys[] = { VK_RETURN, VK_ESCAPE, VK_LEFT, VK_UP, VK_RIGHT, VK_DOWN, 'A', 0x20, 0x09, 0x2e };
    return Next() % 7 == 0 ? (int)(Next() % 256) : kKeys[Next() % 10];
}

// PopupMenu::TryKeyDown after the submenu's own TryKeyDown declined: the steps as the original takes them
// (1 EndSubMenu, 2 the submenu's first item, 3 CheckSubMenu, 4 ListArea::KeyDown), and the result
static int PopupKey(bool hasSub, int theKey, bool subHasSel, bool& used)
{
    if(hasSub)
    {
        if(theKey==VK_LEFT || theKey==VK_ESCAPE)
        { used = true; return 1; }
        else if(theKey==VK_RIGHT && !subHasSel)
        { used = true; return 2; }
        else
        { used = false; return 0; }
    }
    else if(theKey==VK_RIGHT)
    { used = true; return 3; }
    else if(theKey==VK_UP || theKey==VK_DOWN)
    { used = true; return 4; }
    else
    { used = false; return 0; }
}

static void Popup()
{
    for (int n = 0; n < 1000000; n++) {
        bool hasSub = Next() & 1, subHasSel = hasSub && (Next() & 1);
        int key = Key();
        bool used;
        int o = PopupKey(hasSub, key, subHasSel, used);
        uint8_t r = ee_won_gui_menu_popup_key(hasSub, key, subHasSel);
        CHECK(o == r);
        CHECK(used == (r != 0));
        // CheckSubMenu: 0 nothing, 1 EndSubMenu and return, 2 EndSubMenu and open, 3 EndSubMenu only
        {
            bool isItem = Next() % 4 != 0, hasCur = Next() & 1, same = isItem && hasCur && Next() % 3 == 0;
            bool itemHasSub = Next() & 1, disabled = Next() % 3 == 0;
            int steps;
            if(!isItem)
                steps = hasCur ? 1 : 0;
            else if(same)
                steps = 0;
            else if(itemHasSub && !disabled)
                steps = 2;
            else
                steps = 3;
            CHECK(steps == ee_won_gui_menu_check_sub(isItem, hasCur, same, isItem && itemHasSub, isItem && disabled));
        }
        // HandleComponentEvent, ListSelChangedHook
        {
            bool fromSub = Next() & 1;
            int type = Next() % 2 ? ComponentEvent_ListSelChanged : (int)(Next() % 40);
            CHECK((fromSub && type==ComponentEvent_ListSelChanged) == (ee_won_gui_menu_popup_event(fromSub, type) == 1));
            bool keyboard = Next() & 1, sub = Next() & 1, selIsSub = Next() & 1;
            bool o2 = false;
            if(!keyboard)
            {
                if(sub && !selIsSub)
                    o2 = true;
            }
            CHECK(o2 == ee_won_gui_menu_sel_clears_sub(keyboard, sub, selIsSub));
        }
    }
    CHECK(ee_won_gui_menu_submenu_flags() == 0x33c7);
}

static void Bar()
{
    int aFlags = PopupFlag_StandardPopup;
    aFlags &= ~PopupFlag_WantInput;
    aFlags &= ~PopupFlag_EndOnEscape;
    CHECK(aFlags == ee_won_gui_menubar_popup_flags());
    for (int n = 0; n < 1000000; n++) {
        // KeyDown after the menu declined
        {
            int mState = (int)(Next() % 3);
            CHECK((mState==MenuBarState_Down) == ee_won_gui_menubar_key_asks_popup(mState));
            if (mState != MenuBarState_Down) mState = MenuBarState_Up;
            int theKey = Key();
            size_t size = Next() % 5 == 0 ? 0 : Next() % 6;
            int anId = Next() % 9 == 0 ? Any() : (int)(Next() % (size ? size : 1));
            int id0 = anId, state0 = mState;
            int step = 0;
            if(theKey==VK_ESCAPE)
            {
                mState = MenuBarState_Up;
                step = 1;
            }
            else if(theKey==VK_RIGHT)
            {
                anId++;
                if(anId>=size)
                    anId = 0;
            }
            else if(theKey==VK_LEFT)
            {
                anId--;
                if(anId<0)
                    anId = size-1;
            }
            else if(mState==MenuBarState_Up && (theKey==VK_RETURN || theKey==VK_DOWN))
            {
                step = 2;
                mState = MenuBarState_Down;
            }
            int rs = -9, ri = -9;
            CHECK(step == ee_won_gui_menubar_key(state0, theKey, id0, size, &rs, &ri));
            CHECK(rs == mState);
            CHECK(ri == anId);
        }
        // CheckItem
        {
            int mState = Next() % 9 == 0 ? (int)(Next() % 5) : (int)(Next() % 3);
            bool hasCur = Next() & 1, hasCheck = Next() % 4 != 0;
            bool isCur = hasCur && hasCheck && Next() % 3 == 0, isItem = hasCheck && Next() % 4 != 0;
            if (isCur) isItem = true;
            int steps = 0;
            if(mState==MenuBarState_None || mState==MenuBarState_Up)
            {
                if(hasCur && !isCur)
                    steps |= 1;
                if(hasCheck)
                    steps |= 2;
            }
            else if(mState==MenuBarState_Down)
            {
                if(isItem && !isCur)
                    steps |= 4;
            }
            CHECK(steps == ee_won_gui_menubar_check(mState, hasCur, hasCheck, isCur, isItem));
            int left = Any(), top = Any(), height = Any(), rootY = Any();
            int x = left;
            int y = (top+height);
            int ry = 0;
            CHECK(x == ee_won_gui_menubar_popup_place(left, top, height, &ry));
            CHECK(y == ry);
            int avoid[4];
            ee_won_gui_menubar_popup_avoid(rootY, height, avoid);
            CHECK(avoid[0] == 0 && avoid[1] == rootY-height && avoid[2] == 0 && avoid[3] == height);
        }
        // HandleComponentEvent, the chosen item, MouseDown, MouseMove, AddItem, SetText
        {
            int type = Next() % 3 == 0 ? ComponentEvent_EndPopup : Next() % 2 ? ComponentEvent_ListItemUpClicked
                                                                               : (int)(Next() % 40);
            int mState = (int)(Next() % 3);
            int ev = 0;
            if(type==ComponentEvent_EndPopup && mState==MenuBarState_Down)
                ev = 1;
            else if(type==ComponentEvent_ListItemUpClicked)
                ev = 2;
            CHECK(ev == ee_won_gui_menubar_event(type, mState));
            bool isItem = Next() % 4 != 0, sub = Next() & 1, dis = Next() & 1;
            CHECK((isItem && !sub && !dis) == ee_won_gui_menu_item_chosen(isItem, isItem && sub, isItem && dis));
            int st = mState;
            bool opens;
            if(st==MenuBarState_Down)
            {
                st = MenuBarState_Up;
                opens = false;
            }
            else
            {
                st = MenuBarState_Down;
                opens = true;
            }
            int rs = -1;
            CHECK(opens == ee_won_gui_menubar_press(mState, &rs));
            CHECK(rs == st);
            int x = Any(), y = Any(), lx = Next() % 2 ? x : Any(), ly = Next() % 2 ? y : Any();
            CHECK(!(x==lx && y==ly) == ee_won_gui_menubar_moved(x, y, lx, ly));
            int anId = Next() % 3, pl = Any(), pt = Any(), pw = Any();
            int ox, oy;
            if(anId > 0) { ox = (pl+pw)+1; oy = pt; } else { ox = 2; oy = 2; }
            int ry = -1;
            CHECK(ox == ee_won_gui_menubar_item_place(anId > 0, pl, pt, pw, &ry));
            CHECK(oy == ry);
            int sw = Any(), fh = Any(), rh = 0;
            CHECK(sw+10 == ee_won_gui_menubar_item_size(sw, fh, &rh));
            CHECK(fh+4 == rh);
        }
    }
}

int main()
{
    Popup();
    Bar();
    printf("won_guimenu_test: %d checks, %d failures\n", sChecks, sFailures);
    return sFailures;
}
