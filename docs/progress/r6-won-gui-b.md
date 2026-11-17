# R6 WON, the GUI toolkit's widget core (WONGUI B)

The second WONGUI lane's state. Each round rewrites this file to the current state; the round's story goes in
PORT_LEARNINGS.md ("R6 WONGUI B round N").

**Scope.** WONGUI's widget core and, from round 3, all of WONGUI (`ee_won_gui`, `src/Empire Earth/WONAPI/WONGUI`):
WindowManager, Window, Container, Graphics, the shared component host (GuiHost.cpp), the MS controls' unit,
SkinnedContainers.cpp, ComponentLifetime.cpp, the GUIString data and their class code in
`include/ee/wonapi/WONLobbyGUI.h`. The lobby's own classes (`ee_won_lobby`, `WONAPI/WONLobby`) are the WONLobby lane's.

**Round 1 (`rust-wonguib1`).** GUIStringData's buffer (`guistrobj.rs`) and Graphics (`gfxobj.rs`) whole in Rust,
InitColorScheme's table, the forwarders and defaults folded. WON C++ 47,862 -> 46,772 (-1,090).

**Round 2 (`rust-wonguib2`).** Component's members as a Rust block (`compmem.rs`), the drivers' worlds serving real
blocks, the window's font cache (`fontmap.rs`). WON C++ -17.

**Round 3 (`rust-wonguib3`, from origin/rust 4aeedb1a1).**
- **One registered component host** (`compobj.rs`): `ee_won_comp_host_register` (ComponentLifetime.cpp registers
  `ee_won_guicpp_comp_call` at start), `ee_won_comp_host_world` (a driver's or test's world, per thread), `comp()`:
  Component's methods over it. A Rust object calls a nonvirtual Component method (SetPos, SetSize, SetPosSize,
  SetEnabled's base, RequestFocus, GetDefaultCursor) as compobj's Rust function, a virtual one (SetVisible,
  SetEnabled, Invalidate, GetDesiredSize, GetMouseChild) through the host's `V*` op, a member read or write through
  compmem. New host ops: `VSetVisible`, `VMouseChild`.
- **Moved onto it:** splitobj, barobj, scrollerobj, paneobj, multiobj (header and list). GuiHost.cpp's container
  host lost its child cases (-31 lines).
- **The ct cluster's member writes** (a child's parent and focus id, the invalid state, the timer flags) are in place
  in compmem; the ct world's top calls trace a "world" line of every component's written members. Container.cpp -4.
- **Drivers:** the five map worlds keep a block per component (`EeWonCompMembers` in the world's entry, a resolver
  to it); the original statements call compobj (`ee_won_comp_call`) for a nonvirtual method, the world's comp host
  for a virtual, the block for a member. The moves are traced at PositionChanged / the size change with the
  bounds. Cargo's replays use compmem's `map_world` and compobj's `host_world`. Eleven goldens regenerated.
- WON C++ (`wc -l`, as before): 42,039 at the branch's start -> 42,008 (net -31). Far below the 1,500 target: see
  PORT_LEARNINGS "R6 WONGUI B round 3", "Why so few lines".

**What is left, in order of lines freed:**
1. The ComponentOp self cases in GuiHost.cpp (Invalidate, RequestTimer, RequestFocus, GetMousePos, KeyMod, SetSize,
   SetDefaultCursor, Get Width/Height; 8 lines): every object that falls back to ComponentOp (text, input, label,
   list, tree, menu, the multi rows) must move first; each driver's world then answers the comp host's ops.
2. The ct cluster's component calls (Container.cpp's CtVirtCall Invalidate/RequestTimer/RequestFocus, ChildInvalidate,
   ChildSetFlags, CtGrabBG, CtOwnScheme, CtWantFocus, CtNextSibling, CtDefaultCursor; ~15 lines; the same in
   Button.cpp, DialogBehavior.cpp, TabCtrl.cpp, ComboBox.cpp, RootContainer.cpp, ~15 more): the ct world emulates
   Component (`Component_`); it would answer the comp host's ops instead (six goldens).
3. layoutobj's and skinlay's moves (ChildLayouts.cpp 3, SkinnedContainers.cpp 2), wmobj's and winobj's own
   component enums (WindowManager.cpp 13, Window.cpp 9), the skin buttons' SkinBtn reads (MSButtonBase.cpp,
   WONButton.cpp, 11).
4. Splitter's ChildSetDefaultCursor (the standard cursor is a C++ object: a comp host op making it would let it go).
5. Not done: the MS constructors as msbtnobj init kinds, SkinnedContainers' two paints into skinlay.rs.
