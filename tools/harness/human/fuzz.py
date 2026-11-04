"""TEST ONLY (final validation campaign, lane fuzz): a seeded random script generator for the scripted human.

    python -I tools/boot/human/fuzz.py --seed N [--ai-players K] [--start-pt 2000] [--actions 480] [--gap 36]
                                       [--out FILE]

Writes a tools/boot/human script (format in human.py) that plays a legal-ish random game through the game's own input
path: selections (hot keys, clicks on own units, drag boxes, groups, idle-citizen keys), moves, attacks, gathers,
mini-map sends, every shown status button (train / research / build / epoch up and the command buttons: patrol,
garrison, unload, stop, explore, airport commands, flares, lock, pack, disband ...) followed by a world click for the
modes they enter, formations, stances, diplomacy (tributes, ally/enemy, custom chat, OK/clear/cancel), chat (typed
text, Enter), the options panel, mini-map buttons, timed-rally buttons, camera keys. Nothing here writes game state:
every action is an input of human_input.c's vocabulary. The same seed gives the same script, so an original and a
full-link session of one seed play the same inputs; the targets resolve from the game state at each PT (`anyof`,
`near`, `pick:K`), so a script stays legal whatever the map.

The local player is player 1 (ai_game: slot 0), the computers 2..K+1, gaia 0. Every world click has a `camera` line
some PTs earlier (the camera only moves while PTs run). A `recover` closes whatever panel a random key or button
opened, so the following main-form clicks keep working. A few rare inputs (Delete = kill, disband, self-destruct)
come from the random command buttons only.
"""
import argparse
import math
import pathlib
import random

LOCAL = 1
FORMATIONS = list(range(236, 244))
STANCES = [228, 229, 232, 233]
# hot keys of the main game form (human-state.txt `hotkey` lines): DIK names or dikNN; not F3 / Esc (they pause)
KEYS = ['h', 'comma', 'period', 'tab', 'space', 'b', 'f', 's', 'w', 'a', 'd', 'home', 'f1', 'f2', 'f4', 'f5', 'f11',
        'dik78', 'dik74', 'up', 'down', 'left', 'right', 'delete', 'enter']
KEY_WEIGHTS = [8, 6, 4, 3, 2, 2, 2, 2, 2, 2, 2, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0.3, 1]
CHAT_WORDS = ['hello', 'gg', 'attack', 'wait', 'ally', 'help', 'rush', 'tribute', 'nice', 'go']


class Gen:
    def __init__(self, seed, ai_players):
        self.r = random.Random(seed)
        self.enemies = list(range(2, 2 + ai_players))
        self.out = []          # (relative PT, op, args)

    def add(self, dpt, op, args=''):
        self.out.append((dpt, op, args))

    def off(self, span=10):
        r = self.r
        return f'{r.uniform(-span, span):.1f} {r.uniform(-span, span):.1f}'

    def mods(self, p=0.15):
        r = self.r
        if r.random() >= p:
            return ''
        return ' mod=' + r.choice(['shift', 'ctrl', 'alt', 'shift+ctrl'])

    def own_target(self):
        return f'anyof {LOCAL} -1 {self.r.randrange(1000)}'

    def world_target(self):
        r = self.r
        k = r.random()
        if k < 0.35:
            return f'home+ {self.off(12)}'
        if k < 0.6:
            return f'near 0 -1 {r.randrange(12)}'                       # gaia (resources, animals)
        if k < 0.85:
            return f'near {r.choice(self.enemies)} -1 {r.randrange(12)}'  # the nearest enemy units
        return self.own_target()

    # ---- action families ----
    def select(self):
        r = self.r
        k = r.random()
        if k < 0.2:
            self.add(0, 'key', 'h')
        elif k < 0.5:
            t = self.own_target(); self.add(0, 'camera', t); self.add(30, 'lclick' if self.r.random() < 0.3 else 'box', t + ('' if self.r.random() < 0.3 else ' 18') + self.mods())
        elif k < 0.7:
            o = self.off(6); self.add(0, 'camera', f'home+ {o}'); self.add(30, 'box', f'home+ {o} {r.randrange(40, 160)}' + self.mods(0.2))
        elif k < 0.8:
            self.add(0, 'key', r.choice(['comma', 'period']))
        else:
            g = r.randrange(1, 10)
            self.add(0, 'key', f'{r.choice(["ctrl+", "", "", "shift+"])}{g}')

    def command(self):
        r = self.r
        k = r.random()
        if k < 0.7:
            t = self.world_target(); self.add(0, 'camera', t); self.add(30, 'rclick', t + self.mods(0.25))
        elif k < 0.85:
            self.add(0, 'rminimap', f'of {r.choice(self.enemies)} 41')
        else:
            self.add(0, 'rminimap', f'home+ {self.off(20)}' + self.mods(0.2))

    def status_button(self):
        r = self.r
        kind = r.choice(['pick', 'pickcmd', 'picktech', 'picktech'])
        t = self.world_target()
        self.add(0, 'camera', t)
        self.add(30, 'button', f'{kind}:{r.randrange(1000)}' + (' right' if r.random() < 0.1 else '') + self.mods(0.1))
        f = r.random()
        if f < 0.55:      # the mode the button entered: target / placement / patrol point / rally ...
            self.add(10, 'lclick', t + self.mods(0.2))
            if r.random() < 0.3:
                self.add(10, 'lclick', f'home+ {self.off(12)}')
        elif f < 0.7:
            self.add(10, 'rclick', t)
        if r.random() < 0.6:
            self.add(10, 'click', '11:244 shown')   # cancel UI mode (only while shown: a hidden one is not clicked)

    def build(self):
        """a citizen (types 32/480, or the idle-citizen key) places a random building of its build buttons near home"""
        r = self.r
        if r.random() < 0.5:
            self.add(0, 'key', 'comma')
        else:
            t = f'anyof {LOCAL} {r.choice([32, 480])} {r.randrange(1000)}'
            self.add(0, 'camera', t); self.add(30, 'box', t + ' 18')
        o = self.off(9)
        self.add(10, 'camera', f'home+ {o}')
        self.add(30, 'button', f'picktech:{r.randrange(1000)}')
        self.add(10, 'lclick', f'home+ {o}' + self.mods(0.1))
        if r.random() < 0.3:     # walls / gates: a second point
            self.add(10, 'lclick', f'home+ {self.off(9)}')
        self.add(10, 'click', '11:244 shown')

    def army(self):
        """everything in a large box near home, a formation and a stance, then an attack / move"""
        r = self.r
        o = self.off(5)
        self.add(0, 'camera', f'home+ {o}'); self.add(30, 'box', f'home+ {o} 190')
        self.add(10, 'click', f'11:{r.choice(FORMATIONS)}')
        self.add(10, 'click', f'11:{r.choice(STANCES)}')
        t = self.world_target()
        self.add(10, 'camera', t); self.add(30, 'rclick', t + self.mods(0.2))

    def train_burst(self):
        r = self.r
        self.add(0, 'key', 'h')
        for _ in range(r.randrange(1, 4)):
            self.add(8, 'button', f'picktech:{r.randrange(1000)}')

    def formation_stance(self):
        r = self.r
        self.add(0, 'click', f'11:{r.choice(FORMATIONS if r.random() < 0.5 else STANCES)}')

    def diplomacy(self):
        r = self.r
        self.add(0, 'click', '11:0')
        rows = len(self.enemies)
        for _ in range(r.randrange(1, 6)):
            row = r.randrange(rows)
            c = r.choice([20, 24, 25, 26, 27, 28, 29, 25, 26]) + 11 * row
            self.add(8, 'click', f'27:{c}' + (' right' if c >= 25 + 11 * row and r.random() < 0.2 else '') + self.mods(0.15))
        self.add(8, 'click', f'27:{r.choice([184, 184, 184, 185, 186])}')
        self.add(0, 'recover')

    def chat(self):
        r = self.r
        self.add(0, 'click', '11:2')
        if r.random() < 0.3:
            self.add(8, 'key', 'tab')
        self.add(8, 'type', '_'.join(r.choice(CHAT_WORDS) for _ in range(r.randrange(1, 4))))
        self.add(8, 'key', r.choice(['enter', 'enter', 'enter', 'esc']))
        self.add(0, 'recover')

    def panel(self):
        r = self.r
        k = r.random()
        # the options panel (11:3), F3 and the civilization builder (11:5) pause the game: a later scripted PT never comes,
        # and closing them in the same PT left the game paused (f02: physics stalled at PT 8120); not fuzzed
        if k < 0.5:
            self.add(0, 'click', f'11:{r.choice([225, 227, 206, 253])}'); self.add(0, 'recover')
        elif k < 0.7:
            self.add(0, 'click', f'11:{r.choice([208, 209, 210])}')
        elif k < 0.85:
            self.add(0, 'minimap', f'of {r.choice(self.enemies)} 41')
        else:
            self.add(0, 'click', f'11:{r.choice([225, 227, 206, 253])}'); self.add(0, 'recover')

    def civbuilder(self):
        """the Civilization Builder (main control 5, form 7): accept (17: requests the civ's upgrades) or exit (16)"""
        r = self.r
        self.add(0, 'click', '11:5')
        self.add(0, 'click', f'7:{r.choice([17, 17, 16])}')
        self.add(0, 'recover')

    def hotkey(self):
        r = self.r
        self.add(0, 'key', r.choices(KEYS, KEY_WEIGHTS)[0])
        self.add(0, 'recover')

    # ---- scripted build-up (--buildup R): R rounds before the random phase, so the random phase has an army, docks,
    #      transports, temples ... to command. Generic (no unit-type or technology table): every shown button of the
    #      selected unit is pressed by index (`picktech:K` is the (K mod count)-th technology button). ----
    def buildup_round(self, i):
        r = self.r
        # 1. the town centre: train / research / epoch up (every technology button once)
        self.add(0, 'key', 'h')
        for k in range(6):
            self.add(6, 'button', f'picktech:{k}')
        # 2. a citizen places a building of each build button, on a ring around home (distinct sites per round)
        for k in range(5):
            if r.random() < 0.5:
                self.add(10, 'key', 'comma')
            else:
                t = f'anyof {LOCAL} {r.choice([32, 480])} {r.randrange(1000)}'
                self.add(10, 'camera', t); self.add(30, 'box', t + ' 18')
            ang = 2.39996 * (5 * i + k)
            rad = 7 + 1.3 * ((5 * i + k) % 7)
            o = f'{rad * math.cos(ang):.1f} {rad * math.sin(ang):.1f}'
            self.add(10, 'camera', f'home+ {o}')
            self.add(30, 'button', f'picktech:{5 * i + k}')
            self.add(10, 'lclick', f'home+ {o}')
            self.add(10, 'click', '11:244 shown')
        # 3. own units one by one (buildings among them): press two of their technology buttons (train / research)
        for _ in range(6):
            t = f'anyof {LOCAL} -1 {r.randrange(1000)}'
            self.add(10, 'camera', t); self.add(30, 'lclick', t)
            for _ in range(2):
                self.add(6, 'button', f'picktech:{r.randrange(1000)}')
        # 4. gather: idle citizens to the nearest resources
        self.add(10, 'key', 'comma'); t = f'near 0 -1 {r.randrange(12)}'
        self.add(10, 'camera', t); self.add(30, 'rclick', t)

    FAMILIES = [('select', 16), ('command', 18), ('status_button', 22), ('train_burst', 6), ('formation_stance', 4), ('build', 8), ('army', 6),
                ('diplomacy', 3), ('chat', 2), ('panel', 4), ('hotkey', 8)]   # civbuilder: pauses (see panel)

    def script(self, start_pt, n_actions, gap, buildup=0, buildup_gap=500):
        fams, weights = zip(*self.FAMILIES)
        lines = []
        pt = start_pt
        for i in range(buildup):
            self.out = []
            self.buildup_round(i)
            for dpt, op, args in self.out:
                pt += dpt
                lines.append((pt, op, args))
            pt += buildup_gap
        while len(lines) < n_actions:
            self.out = []
            fam = self.r.choices(fams, weights)[0]
            if fam in ('command', 'status_button', 'formation_stance') and self.r.random() < 0.75:
                self.select()     # an episode: select something, then act on it
                self.out.append((12, None, None))
            getattr(self, fam)()
            for dpt, op, args in self.out:
                pt += dpt
                if op:
                    lines.append((pt, op, args))
            pt += int(self.r.expovariate(1 / gap)) + 4
            if self.r.random() < 0.04:
                lines.append((pt, 'dump', f'fuzz-{pt}'))
        return lines[:n_actions]


class MilGen(Gen):
    """--military (final campaign, lane fuzz pass 3c): a scenario or game where the human already has an army. Every
    episode selects own units by type (the types are the scenario's, from a survey dump: --own mobile units, --bld
    buildings) and then acts on them: right clicks on enemies / ground / own units (attack, move, load a transport,
    garrison, heal, repair, return to base), command buttons with their target clicks (attack ground, patrol, unload,
    airport commands, lock, pack, explore, fanatic, JIT, disband ...), stances (buttons and Alt+S/D/G/A), formations,
    the unit icon bar (select one / Ctrl all of a type / Shift marks / right click removes; a transport's cargo
    unloads, an airport's planes scramble), saved groups (right click clears), patrol-group selection (double click on
    a patrol point), game speed (numpad +/-), Ctrl+Alt/Shift+Z (attack player), Delete (rare), train buttons of the
    selected buildings, diplomacy. Same input vocabulary as Gen; nothing writes game state."""
    ICON_BAR = (3, 399, 583)          # main form control 222 (EEUICUnitIconBar), screen x range, centre y

    def __init__(self, seed, enemies, own, bld, diplomacy_weight=0):
        super().__init__(seed, 0)
        self.enemies = enemies
        self.own, self.bld = own, bld
        # diplomacy: 0 by default (scenarios usually disable it; probe runs of 6 scenarios crashed the driver after
        # closing it, fixed by find_by_vtable's form check); random maps: --mil-diplomacy W
        self.diplomacy_weight = diplomacy_weight

    def world_target(self):
        r = self.r
        k = r.random()
        if k < 0.2:
            return f'home+ {self.off(15)}'
        if k < 0.6:
            return f'near {r.choice(self.enemies)} -1 {r.randrange(12)}'
        if k < 0.7:
            return f'anyof {r.choice(self.enemies)} -1 {r.randrange(1000)}'
        if k < 0.78:
            return f'near 0 -1 {r.randrange(12)}'
        return self.own_target()

    def own_target(self):
        r = self.r
        if r.random() < 0.5 and self.bld:
            return f'anyof {LOCAL} {r.choice(self.bld)} {r.randrange(1000)}'
        return f'anyof {LOCAL} {r.choice(self.own) if self.own and r.random() < 0.8 else -1} {r.randrange(1000)}'

    def select(self):
        r = self.r
        k = r.random()
        if k < 0.45:     # one type: a click, then shift-adds of the same or other mobile types
            t = f'anyof {LOCAL} {r.choice(self.own)} {r.randrange(1000)}'
            self.add(0, 'camera', t); self.add(30, 'lclick', t + (' mod=ctrl' if r.random() < 0.1 else ''))
            for _ in range(r.randrange(0, 5)):
                self.add(4, 'lclick', f'anyof {LOCAL} {r.choice(self.own)} {r.randrange(1000)} mod=shift')
        elif k < 0.65:   # a box around one of them
            t = f'anyof {LOCAL} {r.choice(self.own)} {r.randrange(1000)}'
            self.add(0, 'camera', t); self.add(30, 'box', t + f' {r.choice([8, 12, 18, 30])}' + self.mods(0.15))
        elif k < 0.75 and self.bld:   # a building (airport, factories, docks ...)
            t = f'anyof {LOCAL} {r.choice(self.bld)} {r.randrange(1000)}'
            self.add(0, 'camera', t); self.add(30, 'lclick', t)
        elif k < 0.85:
            self.add(0, 'key', r.choice(['comma', 'comma', 'a', 'b', 'd', 'f', 's', 'w', 'shift+a', 'shift+f']))
        else:
            g = r.randrange(1, 10)
            self.add(0, 'key', f'{r.choice(["ctrl+", "", "", "shift+", "alt+"])}{g}')

    def m_command(self):
        r = self.r
        t = self.world_target()
        self.add(0, 'camera', t); self.add(30, 'rclick', t + self.mods(0.2))
        if r.random() < 0.15:
            self.add(6, 'rclick', self.world_target() + ' mod=shift')

    def m_button(self):
        r = self.r
        self.add(0, 'button', f'pickcmd:{r.randrange(1000)}' + (' right' if r.random() < 0.05 else '') + self.mods(0.1))
        for _ in range(r.choice([0, 1, 1, 1, 2, 3])):
            t = self.world_target()
            self.add(6, 'camera', t); self.add(30, 'lclick', t + self.mods(0.2))
        if r.random() < 0.3:
            self.add(6, 'rclick', self.world_target())
        if r.random() < 0.6:
            self.add(6, 'click', '11:244 shown')

    def m_stance(self):
        r = self.r
        if r.random() < 0.5:
            self.add(0, 'click', f'11:{r.choice(STANCES)}')
        else:
            self.add(0, 'key', f'alt+{r.choice("sdga")}')

    def m_formation(self):
        self.add(0, 'click', f'11:{self.r.choice(FORMATIONS)}')

    def m_iconbar(self):
        r = self.r
        x0, x1, y = self.ICON_BAR
        x = r.uniform(x0 + 2, min(x1, x0 + 2 + 22 * r.choice([1, 2, 4, 8, 16])))
        k = r.random()
        if k < 0.5:
            self.add(0, 'lclick', f'screen {x:.0f} {y}' + r.choice(['', '', ' mod=ctrl', ' mod=shift']))
        else:
            self.add(0, 'rclick', f'screen {x:.0f} {y}')

    def m_train(self):
        r = self.r
        if self.bld:
            t = f'anyof {LOCAL} {r.choice(self.bld)} {r.randrange(1000)}'
            self.add(0, 'camera', t); self.add(30, 'lclick', t)
        for _ in range(r.randrange(1, 4)):
            self.add(6, 'button', f'picktech:{r.randrange(1000)}' + self.mods(0.1))
        if r.random() < 0.4:   # the rally point / timed rally buttons of an airport
            self.add(6, 'click', f'11:{r.choice([208, 209, 210])}')
        if r.random() < 0.3:
            t = self.world_target(); self.add(6, 'camera', t); self.add(30, 'rclick', t)

    def m_global(self):
        r = self.r
        k = r.choice(['dik78', 'dik74', 'dik78', 'dik74', 'ctrl+alt+z', 'ctrl+shift+z', 'delete', 'alt+f'])
        self.add(0, 'key', k)
        if k == 'alt+f':
            t = self.world_target(); self.add(6, 'camera', t); self.add(30, 'lclick', t)
        self.add(0, 'recover')

    def m_savedgroup(self):
        r = self.r
        self.add(0, 'key', f'ctrl+{r.randrange(1, 10)}')
        self.add(6, 'click', f'11:{r.randrange(212, 222)}' + (' right' if r.random() < 0.5 else ''))

    def m_patrolgroup(self):
        t = f'anyof {LOCAL} 223 {self.r.randrange(1000)}'
        self.add(0, 'camera', t); self.add(30, 'lclick', t); self.add(0, 'lclick', t)

    FAMILIES = [('m_command', 26), ('m_button', 26), ('m_stance', 6), ('m_formation', 5), ('m_iconbar', 8),
                ('m_train', 8), ('m_global', 4), ('m_savedgroup', 2), ('m_patrolgroup', 1), ('diplomacy', 0),
                ('hotkey', 2), ('panel', 1)]
    EPISODE = ('m_command', 'm_button', 'm_stance', 'm_formation', 'm_iconbar')

    def script(self, start_pt, n_actions, gap, buildup=0, buildup_gap=500):
        fams, weights = zip(*self.FAMILIES)
        weights = [self.diplomacy_weight if f == 'diplomacy' else w for f, w in self.FAMILIES]
        lines, pt = [], start_pt
        for i in range(buildup):     # random-map starts: Gen's build-up rounds first (train / build / research)
            self.out = []
            self.buildup_round(i)
            for dpt, op, args in self.out:
                pt += dpt
                lines.append((pt, op, args))
            pt += buildup_gap
        while len(lines) < n_actions:
            self.out = []
            fam = self.r.choices(fams, weights)[0]
            if fam in self.EPISODE and self.r.random() < 0.8:
                self.select()
                self.out.append((12, None, None))
            getattr(self, fam)()
            for dpt, op, args in self.out:
                pt += dpt
                if op:
                    lines.append((pt, op, args))
            pt += int(self.r.expovariate(1 / gap)) + 4
            if self.r.random() < 0.03:
                lines.append((pt, 'dump', f'fuzz-{pt}'))
        return lines[:n_actions]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--military', action='store_true', help='MilGen: act on an existing army (scenario starts)')
    ap.add_argument('--own', default='', help='--military: own mobile unit types, comma separated')
    ap.add_argument('--bld', default='', help='--military: own building types, comma separated')
    ap.add_argument('--mil-diplomacy', type=float, default=0, help='--military: weight of the diplomacy family (default 0)')
    ap.add_argument('--enemies', default='', help='--military: enemy player numbers (default 2..1+ai-players)')
    ap.add_argument('--seed', type=int, required=True)
    ap.add_argument('--ai-players', type=int, default=2)
    ap.add_argument('--start-pt', type=int, default=2000)
    ap.add_argument('--actions', type=int, default=480, help='at most 512 (the driver\'s table)')
    ap.add_argument('--gap', type=float, default=36, help='mean PTs between action groups')
    ap.add_argument('--buildup', type=int, default=0, help='scripted build-up rounds before the random phase')
    ap.add_argument('--buildup-gap', type=int, default=500, help='PTs between build-up rounds')
    ap.add_argument('--out')
    a = ap.parse_args()
    if a.actions > 512:
        ap.error('the driver holds at most 512 actions')
    if a.military:
        ints = lambda s: [int(x) for x in s.split(',') if x.strip()]
        g = MilGen(a.seed, ints(a.enemies) or list(range(2, 2 + a.ai_players)), ints(a.own) or [-1], ints(a.bld), a.mil_diplomacy)
    else:
        g = Gen(a.seed, a.ai_players)
    lines = g.script(a.start_pt, a.actions - 2, a.gap, a.buildup, a.buildup_gap)
    text = [f'# fuzz script (tools/boot/human/fuzz.py) seed {a.seed}, {a.ai_players} AI, {len(lines) + 2} actions, '
            f'PT {a.start_pt}..{lines[-1][0]}',
            f'{a.start_pt - 10} dump fuzz-start']
    text += [f'{pt} {op} {args}'.rstrip() for pt, op, args in lines]
    text.append(f'{lines[-1][0] + 40} dump fuzz-end')
    body = '\n'.join(text) + '\n'
    if a.out:
        pathlib.Path(a.out).write_text(body, encoding='utf-8')
        print(f'{a.out}: {len(lines) + 2} actions, PT {a.start_pt - 10}..{lines[-1][0] + 40}')
    else:
        print(body, end='')


if __name__ == '__main__':
    main()
