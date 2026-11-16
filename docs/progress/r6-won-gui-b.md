# R6 WON, the GUI toolkit's widget core (WONGUI B)

The second WONGUI lane's state. Each round rewrites this file to the current state; the round's story goes in
PORT_LEARNINGS.md ("R6 WONGUI B round N").

**Scope.** The widget core of WONGUI (`ee_won`): WindowManager, Window, Container, Graphics, the shared component host
(GuiHost.cpp), the MS controls' unit (MSControlConstructors.cpp), SkinnedContainers.cpp, ComponentLifetime.cpp, the
GUIString data and the class code in `include/ee/wonapi/WONLobbyGUI.h` that belongs to them. The first WONGUI lane
(`r6-won-the-rest.md`) owns the configs and resource readers, the platform layer (Plat*) and the images, filters and
decoders.

**Round 1 (branch `rust-wonguib1`, from WONLobby round 11's head 6d0c36ca).** The measure first (`build/gb1/measure.py`:
each owned unit's lines by kind): the widget core was already Rust objects (Container round 26, Window round 32,
WindowManager rounds 33-34, Component round 38); what was left was 3,260 lines in the owned units, of which 907 host
cases (each a C++ member, virtual or object the Rust reaches), ~500 one-line forwarders, ~400 includes and comments,
and the logic of three classes that had not moved (Graphics, GUIStringData's buffer, the colour scheme's defaults).
Moved: GUIStringData's buffer whole (`guistrobj.rs`; four units and the slice adapters deleted), Graphics whole
(`gfxobj.rs`: translation, colour, scheme references, the clip stack, the base hooks), InitColorScheme's table
(`wmobj.rs`); folded: WindowManager's and Window's forwarders and empty platform defaults, RunDialog, ScreenContainer,
ComponentListenerComp, the events' Deliver methods and WONRectangle into their class declarations (eight units gone),
GuiHost.cpp's repeated cases into its fallbacks, the six MS control units into MSControlConstructors.cpp. Drivers:
`won_guistrobj_test` (20,000 sequences, 2,453,300 checks), `won_gfxobj_test` (20,000 sequences, 800,000 checks, and
InitColorScheme's statements), both identical on their first good run, their goldens replayed by cargo; mutations in
PORT_LEARNINGS. WON C++ (wc -l, src/Empire Earth/WONAPI + include/ee/wonapi) 47,862 -> 46,772 (-1,090: -438 in the two
moves' commits, -652 in the folds', InitColorScheme's table among them); `lan_game.py` RESULT PASS, the lobby captures identical but for the mouse pointer and
the text carets (PORT_LEARNINGS "R6 WONGUI B round 1").

**What is left, and why 1,500 lines a round is not there without a change of method.** The owned units are now
mostly hosts: each case reads or writes a C++ member, calls a virtual the lobby's subclasses override, or touches a
C++ object (fonts, images, events, GUIStrings, SmartPtrs). They go only when those members become Rust: Component's
members (bounds, flags, ids, parent: ~100 WON sites read them inline) into the Rust component, which would let every
module's host drop its member reads (Container.cpp's CtBits, CtParentOf, Child*; WindowManager.cpp's WmComp;
SkinnedContainers' SkinLay reads; GuiHost's Child* cases). The drivers' worlds model components as ids with their
members served by the host (`tests/won/won_ct_world.h`), so that change needs the worlds to hold real memory first.
Next, in order: Component's members as a Rust block (the GUIStringData method: the C++ keeps the layout, the Rust
edits it in place; the worlds first), then the hosts' member cases fall; the window's font map (`Window::GetFont`);
the MS controls' constructors as msbtnobj init kinds (the lead's "table"); SkinnedContainers' two paints into
skinlay.rs.
