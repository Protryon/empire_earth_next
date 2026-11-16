# R6 WON, the GUI toolkit's widget core (WONGUI B)

The second WONGUI lane's state. Each round rewrites this file to the current state; the round's story goes in
PORT_LEARNINGS.md ("R6 WONGUI B round N").

**Scope.** The widget core of WONGUI (`ee_won`): WindowManager, Window, Container, Graphics, the shared component host
(GuiHost.cpp), the MS controls' unit (MSControlConstructors.cpp), SkinnedContainers.cpp, ComponentLifetime.cpp, the
GUIString data and the class code in `include/ee/wonapi/WONLobbyGUI.h` that belongs to them. The first WONGUI lane
(`r6-won-the-rest.md`) owns the configs and resource readers, the platform layer (Plat*) and the images, filters and
decoders.

**Round 1 (branch `rust-wonguib1`).** GUIStringData's buffer (`guistrobj.rs`) and Graphics (`gfxobj.rs`) whole in
Rust, InitColorScheme's table (`wmobj.rs`), the widget core's forwarders, empty defaults and small classes folded into
their declarations (eight units gone), the six MS control units into one. WON C++ 47,862 -> 46,772 (-1,090).

**Round 2 (branch `rust-wonguib2`, from round 1's head e1ed5f93).**
- **Component's members as a Rust block** (`compmem.rs`, `EeWonCompMembers`): the C++ keeps `mParent` ..
  `mExtraStuff` by name, laid out as the block at `EE_WON_COMP_MEMBERS_AT` (16; ComponentLifetime.cpp asserts every
  offset); the original's unused `mChildListItr` is gone. compobj reads and writes them in place; ctobj, dialogobj,
  rootobj, tabobj, btnobj, comboobj, barobj, layoutobj and skinlay read them. Host cases gone: ComponentLifetime.cpp
  12, Container.cpp 12, Button.cpp 4, ChildLayouts.cpp 5, SkinnedContainers.cpp 6.
- **The drivers' worlds serve real blocks**: `tests/won/won_ct_world.h`'s components are the blocks (an id table,
  `ee_won_comp_members_table`); the map worlds (barobj, scrollerobj, layoutobj, skinlay) fill a block from their state
  at each read (`ee_won_comp_members_resolver`); the compobj driver's stand-in Component has the real layout. The Rust
  tests do the same (`compmem::zeroed_world`, `filled_world`).
- **The window's font cache** (`fontmap.rs`): Window::GetFont's map; driver `won_fontmap_test`.
- WON C++ (wc -l, as round 1): 46,767 at round 1's head -> 46,750 (-17 net: the member block -27, the font cache
  +10). The round is far below its 1,500-line target, for the reason below.

**Why the member block frees so few lines.** The ~300 WON sites that read the members inline are the lobby's own C++
classes; they keep reading the same memory (the block is their members), so they do not change. What the block
removes is the host cases through which a *Rust* object read a member, and those were few: 39 lines across the five
hosts above. The rest of the owned units' 2,460 lines are hosts of calls that are not member reads: the virtuals the
lobby's subclasses override, C++ objects (fonts, images, events, GUIStrings, SmartPtrs), and Component methods called
on another component (`ChildSetPos`, `ChildSetVisible`, `ChildInvalidate`, ...).

**What is left, in order of lines freed:**
1. **Component's methods called directly from Rust** (~230 case lines across GuiHost.cpp, the lobby hosts,
   ComponentLifetime.cpp, Button.cpp, WONButton.cpp, ...): compobj is Rust, so a Rust object could call
   `Comp::set_pos` etc. itself, given one registered component host (`ee_won_guicpp_comp_call`). Every module's golden
   traces those ops ("op ChildSetPos ...") and would be regenerated with its driver; the drivers' worlds would need the
   comp host's calls (VPositionChanged, VSizeChanged, ...).
2. **The member writes** the ct cluster still makes through the host (`CtSetParent`, `CtSetFocusId`,
   `CtSetInvalidState`, `CtSetTimerFlags`, `ChildSetFlags`: 5 lines in Container.cpp): the goldens trace them; the
   drivers' traces need a world-state line per call before they can go.
3. **The traced member reads**: multiobj's and splitobj's Child* reads, the skin buttons' `SkinBtn` IsDisabled /
   HasFocus / Parent / SetGrabBG / SetWantFocus (MSButtonBase.cpp, WONButton.cpp), the `Get Width/Height` field reads
   (GuiHost.cpp, MSButtonBase.cpp, WONButton.cpp), wmobj's, winobj's, dlglogic's and lobbyctl's own member ops: their
   goldens answer them from the trace, so each needs its golden regenerated with the world's members in it.
4. Not done this round: the MS controls' constructors as msbtnobj init kinds (~15 lines: each constructor is two to
   five setter calls, most already Rust fields) and SkinnedContainers' two paints into skinlay.rs (their logic is
   already guidraw.rs; what is left is reading the frame and image slots, C++ objects).
