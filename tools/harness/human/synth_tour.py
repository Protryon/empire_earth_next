"""TEST ONLY (final validation campaign, lane synth): deterministic scripted-human tours of the synthetic military
scenario (tools/boot/synth_scenario.py), aimed at the military messages random fuzz rarely reaches. Same input
vocabulary as fuzz.py (human.py format); run with EE_HUMAN_LOCKSTEP=1 and start PT >= 3000 on this scenario (the
driver's start detection is wall-time polled: earlier actions may be dropped in one image only).

  python -I tools/boot/human/synth_tour.py PART OUT [start_pt]
    a: diplomacy toggles + tribute + OK, formation buttons, heal/repair clicks, planes home, airport commands, garrison
       and transport loads with icon-bar unloads, market trades (cmd:1500..1507), prophet calamities, citizens to the
       granary, saved-group clears, patrol points; then pickcmd 0..2 + a target for six mobile types
    b: pickcmd 0..2 + a target for the other mobile types     c: the same for every building, then part a's episodes
    d: twice: loads, garrison mode of citizens (command 0x18) into TC / capitol / granary, the medic's patrol-mode
       heal (0x22), right-click heals of 40%-hit-point infantry, attack ground (6), self destruct (0x1c, Furies),
       cargo / airport icon bars, airport commands + timed rally buttons, patrol (0x1b) and a double click on a
       patrol point, citizens to the granary
Command ids per class come from dbobjects.dat (record +0x16c, nine command slots). A click on a MOVING unit picks by
its render-interpolated position (wall time): if such a click lands differently in two images, that is harness
timing, not a simulation difference (lane synth td2)."""
import sys

MOBILE = [31, 121, 92, 149, 152, 19, 2, 385, 146, 143, 602, 91, 27, 829, 1, 203, 685, 215, 688, 113, 119, 193, 126,
          131, 77, 79, 85]
BLD = [35, 41, 672, 465, 66, 36, 801, 62, 39, 34, 63, 68, 52, 145]
L = 1
out = []
pt = int(sys.argv[3]) if len(sys.argv) > 3 else 3000


def add(d, op, args=''):
    global pt
    pt += d
    out.append(f'{pt} {op} {args}'.rstrip())


def sel(t, k=0, box=0):
    add(14, 'camera', f'anyof {L} {t} {k}')
    if box:
        add(30, 'box', f'anyof {L} {t} {k} {box}')
    else:
        add(30, 'lclick', f'anyof {L} {t} {k}')


def cmds(t, n=4):
    for c in range(n):
        sel(t)
        add(8, 'button', f'pickcmd:{c}')
        tgt = f'near 2 -1 {c}' if c % 2 == 0 else f'home+ {3 + c} {-2 - c}'
        add(6, 'camera', tgt)
        add(30, 'lclick', tgt)
        add(6, 'recover')


def episodes():
    # diplomacy: ally/enemy toggles of players 2 and 3, tribute, OK (EEUIEDSOKButton)
    for c in (24, 35, 25, 36, 24, 35):
        add(10, 'click', '11:0'); add(10, 'click', f'27:{c}'); add(10, 'click', '27:184'); add(4, 'recover')
    # formations: a box of infantry, then every formation button
    for t in (31, 121):
        sel(t, 0, 30)
        for f in range(236, 244):
            add(8, 'click', f'11:{f}')
    # medic / strategist heal, engineer repair
    for healer in (829, 203):
        for k in range(3):
            sel(healer, k)
            add(6, 'camera', f'anyof {L} 31 {k}'); add(30, 'rclick', f'anyof {L} 31 {k}')
    for healer, c in ((829, 0), (829, 1), (203, 0), (27, 0), (27, 1)):
        sel(healer); add(8, 'button', f'pickcmd:{c}')
        add(6, 'camera', f'anyof {L} {31 if healer != 27 else 672} {c}'); add(30, 'lclick', f'anyof {L} {31 if healer != 27 else 672} {c}')
        add(6, 'recover')
    for b in (672, 465, 35, 66):
        sel(27, 0)
        add(6, 'camera', f'anyof {L} {b} 0'); add(30, 'rclick', f'anyof {L} {b} 0')
    # planes return to base (airport, carrier), then the airport's icon bar (scramble) and rally timer buttons
    for p, base in ((77, 66), (79, 66), (85, 126), (85, 66)):
        sel(p, 0, 12)
        add(6, 'camera', f'anyof {L} {base} 0'); add(30, 'rclick', f'anyof {L} {base} 0')
    add(200, 'dump', 'tour-planes')
    cmds(66, 4)     # airport commands first: the timed rally buttons show only while a goal queue has time left
    sel(66)
    for b in (208, 209, 210, 208):
        add(8, 'click', f'11:{b}')
    for x in (12, 34, 56):
        add(8, 'rclick', f'screen {x} 583'); sel(66)
        add(8, 'lclick', f'screen {x} 583'); sel(66)
    # garrison: infantry into the fortress / a tower, then the fortress's icon bar and commands
    for b in (672, 465):
        sel(31, 0, 18)
        add(6, 'camera', f'anyof {L} {b} 0'); add(30, 'rclick', f'anyof {L} {b} 0')
    add(120, 'dump', 'tour-garrison')
    sel(672)
    add(8, 'rclick', 'screen 12 583'); sel(672); add(8, 'lclick', 'screen 12 583')
    cmds(672, 4)
    # transport: infantry in, then unload by button and by icon bar
    sel(121, 0, 18)
    add(6, 'camera', f'anyof {L} 113 0'); add(30, 'rclick', f'anyof {L} 113 0')
    add(200, 'dump', 'tour-transport')
    sel(113); add(8, 'rclick', 'screen 12 583'); sel(113); add(8, 'lclick', 'screen 12 583')
    cmds(113, 4)
    # market trades (EEUIFMGStatusButton buttons 0x5dc..0x5e3), gate lock/unlock
    sel(801)
    for b in range(1500, 1508):
        add(8, 'button', f'cmd:{b}')
    cmds(801, 3)
    # prophet calamities (technology buttons + target), citizens to the granary / town center, saved group clear
    sel(216)
    for c in range(4):
        add(8, 'button', f'picktech:{c}'); add(6, 'camera', f'near 2 -1 {c}'); add(30, 'lclick', f'near 2 -1 {c}')
        add(6, 'recover'); sel(216)
    for b in (52, 35, 52):
        sel(32, 0, 14); add(6, 'camera', f'anyof {L} {b} 0'); add(30, 'rclick', f'anyof {L} {b} 0')
    sel(31, 0, 14); add(6, 'key', 'ctrl+1'); add(8, 'click', '11:212 right'); add(8, 'key', 'ctrl+2')
    add(8, 'click', '11:213 right')
    # patrol group: patrol, then a double click on a patrol point
    sel(31, 1, 12); add(8, 'button', 'pickcmd:0')
    for k in range(3):
        add(6, 'camera', f'anyof {L} 223 {k}'); add(30, 'lclick', f'anyof {L} 223 {k}'); add(2, 'lclick', f'anyof {L} 223 {k}')


def episodes2():
    # loads first (they take time): infantry into the cargo truck and the fortress, planes home to the airport
    for t, tgt in ((31, 215), (121, 672), (92, 465)):
        sel(t, 0, 18); add(6, 'camera', f'anyof {L} {tgt} 0'); add(30, 'rclick', f'anyof {L} {tgt} 0')
    for p in (77, 79, 85):
        sel(p, 0, 30); add(6, 'camera', f'anyof {L} 66 0'); add(30, 'rclick', f'anyof {L} 66 0')
    # garrison mode (command 0x18) of citizens aimed at the town center / capitol / granary; patrol-mode clicks (0x1b, 0x22) of a
    # medic on infantry (EEModeHeal: a target in the medic's heal set)
    for k, b in ((0, 35), (1, 41), (2, 52)):     # garrison capacity: TC 15, Capitol 50, Granary 8
        sel(32, k); add(8, 'button', 'cmd:24'); add(6, 'camera', f'anyof {L} {b} 0'); add(30, 'lclick', f'anyof {L} {b} 0')
    for c in (27, 34):
        for k in range(2):
            sel(829, k); add(8, 'button', f'cmd:{c}'); add(6, 'camera', f'anyof {L} 31 {k}'); add(30, 'lclick', f'anyof {L} 31 {k}')
            add(6, 'recover')
    for k in range(4):
        sel(829, k % 2); add(6, 'camera', f'anyof {L} 31 {k}'); add(30, 'rclick', f'anyof {L} 31 {k}')
    # attack ground (command 6: UI state 0x13), disband (0x1f), self destruct (0x1c) by command id
    for t in (143, 602, 152, 119, 79, 385):
        sel(t); add(8, 'button', 'cmd:6'); add(6, 'camera', f'near 2 -1 {t % 7}'); add(30, 'lclick', f'near 2 -1 {t % 7}')
        add(6, 'recover')
    for t in (259, 259):
        sel(t); add(8, 'button', 'cmd:31'); add(8, 'recover')
        sel(t); add(8, 'button', 'cmd:28'); add(8, 'recover')
    add(150, 'dump', 'tour-loaded')
    # cargo icon bars (immediate unload) and the airport's icon bar (scramble)
    for tr in (215, 672, 465, 113):
        for x, op in ((12, 'lclick'), (34, 'rclick'), (12, 'rclick'), (56, 'lclick')):
            sel(tr); add(8, op, f'screen {x} 583')
    for x in (12, 34, 56, 78, 100, 122):
        sel(66); add(8, 'lclick', f'screen {x} 583')
    # airport commands, each followed by the timed rally buttons (shown while a goal queue has time left)
    for c in range(4):
        sel(66); add(8, 'button', f'pickcmd:{c}')
        add(6, 'camera', f'near 2 -1 {c}'); add(30, 'lclick', f'near 2 -1 {c}')
        sel(66)
        for b in (208, 209, 210):
            add(6, 'click', f'11:{b}')
    # patrol (command 0x1b) between two points, then a double click (two clicks in one PT) on a patrol point
    for k in range(2):
        sel(31, k, 14); add(8, 'button', 'cmd:27')
        add(6, 'camera', f'home+ {4 + k} 0'); add(30, 'lclick', f'home+ {4 + k} 0'); add(10, 'lclick', f'home+ {-3 - k} 4')
    add(60, 'dump', 'tour-patrol')
    for k in range(4):
        add(6, 'camera', f'anyof {L} 223 {k}'); add(30, 'lclick', f'anyof {L} 223 {k}'); add(0, 'lclick', f'anyof {L} 223 {k}')
    # citizens to the granary (SmartFarmers)
    for k in range(2):
        sel(32, k, 14); add(6, 'camera', f'anyof {L} 52 0'); add(30, 'rclick', f'anyof {L} 52 0')


part = sys.argv[1]
if part == 'a':
    episodes()
    for t in MOBILE[:6]:
        cmds(t, 3)
elif part == 'b':
    for t in MOBILE[6:] + [216, 32]:
        cmds(t, 3)
elif part == 'd':
    episodes2()
    episodes2()
elif part == 'c':
    for t in BLD:
        cmds(t, 3)
    episodes()
out = out[:510]
text = [f'# lane synth tour part {part}: {len(out)} actions, PT {out[0].split()[0]}..{out[-1].split()[0]}'] + out
open(sys.argv[2], 'w').write('\n'.join(text) + '\n')
print(sys.argv[2], len(out), out[-1].split()[0])
