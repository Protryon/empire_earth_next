"""The rules documents (docs/DATA_FORMATS.md 8.8): named settings that were literals in the game code, as YAML next
to the db tables. The game reads them in src/Empire Earth/Database/EERules.cpp (ee/EERules.h), whose defaults are
the original's values; this module writes the same values as the vanilla documents (references as ids) and
validates documents (every key known, every reference an id of its table). `eedata.py cppgate` checks that the C++
defaults and the vanilla documents dump identically.

  combat.yaml       the damage function's constants, the elevation rule's categories, morale, regeneration ticks
  gameoptions.yaml  map sizes, speeds, difficulties (with the damage table rule), unit limit, wonders
  civbuilder.yaml   what each civilization builder attribute raises; the special powers category
  civpowers.yaml    what granting each civ power does
  aipersonalities.yaml  the computer players' personalities (epoch pressure, economy tuning) and their assignment

The code constants the vanilla values name (kUnitType*, kTech*, kSound*, kGraphicEffect*, IDS_*) are read from the
C++ headers, so the two sides cannot name different rows.
"""
import pathlib
import re

import scalars
from textio import Scalar, TextError, parse_yaml

ROOT = pathlib.Path(__file__).resolve().parent.parent.parent
INC = ROOT / 'include' / 'ee'

DOCS = ['combat', 'gameoptions', 'civbuilder', 'civpowers', 'aipersonalities']

CATEGORIES = ['ambient', 'resource', 'building', 'airplane', 'naval', 'land', 'land_citizen', 'helicopter', 'land8',
              'projectile', 'submerged_naval', 'bomb', 'missile', 'land_priest', 'naval_priest', 'land15',
              'land_transport', 'helicopter_transport', 'naval_transport', 'building19', 'torpedo', 'naval_building',
              'naval_citizen', 'land_military_hero', 'mining_building', 'gate', 'helicopter_priest', 'sea_animal',
              'air_animal', 'fortress', 'building_priest', 'spaceship', 'space_building', 'space_transport',
              'planet_spaceship']
STATS = ['attack', 'hit_points', 'reload_time', 'armor', 'range', 'line_of_sight', 'speed', 'flight_time',
         'area_effect', 'gather_time', 'build_time', 'conversion', 'repair_rate', 'area_effect_scale', 'high_ground',
         'population', 'cost', 'power']
MODES = ['share', 'add', 'multiply']
ACTIONS = ['research_if_researched', 'load_sound', 'load_effect', 'pulse_sight', 'set_mining_limit',
           'enable_camouflage', 'allow_tech', 'disallow_tech']
WORLD_VALUE = re.compile(r'^([a-z][a-z0-9_]*)\.(F|F2|L|L2)$')


def _consts(header, prefix):
    """{name: value} of the `kPrefix... = n` enumerators / `#define IDS_... n` of a header."""
    text = (INC / header).read_text(encoding='latin-1')
    out = {n: int(v, 0) for n, v in re.findall(r'\b(' + prefix + r'\w*)\s*=\s*(0x[0-9a-fA-F]+|\d+)', text)}
    out.update({n: int(v, 0) for n, v in re.findall(r'#define\s+(' + prefix + r'\w*)\s+(0x[0-9a-fA-F]+|\d+)', text)})
    return out


def _f(x):
    """A float32 as the shortest decimal (the converter's float form)."""
    import struct
    return scalars.format_f32(struct.unpack('<I', struct.pack('<f', x))[0])


class _Ids:
    """Row integers -> ids, per table (the export's ids)."""
    def __init__(self, ids):
        self.ids = ids

    def __call__(self, table, key):
        try:
            return self.ids[table][key]
        except KeyError:
            raise TextError(f'rules: the vanilla value {key} names no listed row of {table}') from None


# ---------------------------------------------------------------- the vanilla documents

HEAD = '# {name}: Empire Earth: The Art of Conquest rules (tools/eedata/rules.py; the game: EERules.cpp). {what}\n' \
       '# A key left out keeps the game\'s default (the value written here).\n' \
       'eedata: {name}\nformat: {format}\n'


def combat(ids, spec):
    ut = _consts('EEUnitClassIDs.h', 'kUnitType')
    cats = lambda xs: '[' + ', '.join(xs) + ']'
    return HEAD.format(name='combat', format=spec['format'],
                       what='The damage function and its neighbours.') + f"""damage:                            # one function for a direct hit, splash damage and the trample
  minimum: 1                       # the least damage of a direct or splash hit that gets through
  fanatic_factor: fanaticism.F2    # attack factor of a fanatic attacker (direct hits; a number or <dbworld id>.F|F2|L|L2)
  elevation_step: {_f(1.0)}              # the elevation factor moves damage by at least this much (direct, splash)
  ally_factor: {_f(0.0)}                 # a direct hit on an own or allied unit does ceil(damage x this) ...
  herd_ally_factor: {_f(0.1)}            # ... or this when the attacker is a herd animal
  dead_food_share: {_f(0.25)}           # a land food source killed by a non-citizen keeps this share of its food
  trample_damage: 1                # a moving unit tramples a stopped enemy for this much ...
  trample_threshold: {_f(1.0)}           # ... when its to-hit x (attack - armour) is above this
elevation:                         # the high/low ground factor (dbworld high_ground_*, low_ground_*): unit categories
  attackers: {cats(['land', 'land_citizen'])}
  launchers: {cats(['projectile', 'bomb', 'missile', 'torpedo'])}   # count as their launcher's category
  targets: {cats(['land', 'building', 'land_citizen', 'land_priest', 'building19', 'land15', 'land_military_hero',
                  'land_transport', 'fortress', 'mining_building', 'building_priest', 'gate'])}
morale:
  positive_factor: {_f(0.8)}           # positive morale takes this share off a time (reload, build ...)
  negative_factor: {_f(0.6)}           # negative morale adds this share
  buildings: [{ids('dbobjects', ut['kUnitTypeHouse'])}]           # the unit types morale from buildings counts
ticks:
  regeneration: 1000               # ms between hit point regenerations and garrison heals
  power_regeneration: 5000         # ms between power regenerations
"""


# the computer players' cheats (ee/EERules.h EEAICheat, by bit): a difficulty's ai_cheats lists the ones it keeps
AI_CHEATS = ('cost', 'reveal', 'epochs', 'extra', 'popband', 'catchup', 'retire')


def gameoptions(ids, spec):
    s = _consts('EEStringIDs.h', 'IDS_')
    sizes = [('tiny', 42), ('small', 58), ('medium', 75), ('large', 105), ('huge', 160), ('gigantic', 220)]
    out = [HEAD.format(name='gameoptions', format=spec['format'],
                       what='The lobby\'s option lists and limits; an option is its entry\'s position.')]
    out.append('map_sizes:\n')
    for k, (sid, tiles) in enumerate(sizes):
        out.append(f'  - id: {sid}\n    name: {s["IDS_MAP_SIZE_FIRST"] + k}               # the lobby\'s text '
                   f'(a Language.rc string id)\n    tiles: {tiles}\n    script_size: {k}                '
                   f'# the random map scripts\' size class (MapSize, 0 tiny .. 5 gigantic)\n')
    out.append('map_size_default: small\n')
    out.append('speeds:\n')
    for sid, name, factor in [('slow', 'IDS_INGAME_SLOW', 'game_speed_slow_normal.F'),
                              ('normal', 'IDS_STANDARD', 'game_speed_slow_normal.F2'),
                              ('fast', 'IDS_INGAME_FAST', 'game_speed_fast_fastest.F'),
                              ('very_fast', 'IDS_VERY_FAST', 'game_speed_fast_fastest.F2')]:
        out.append(f'  - id: {sid}\n    name: {s[name]}\n    factor: {factor}\n')
    out.append('speed_default: normal\n')
    out.append('difficulties:                      # the damage table: the scale when exactly one side is "human" '
               '(EEPlayer::IsHuman)\n')
    for sid, name, level, target, attacker in [
            ('easy', 'IDS_LOBBY_EASY', 0, 'difficulty_damage_scale.F', 'difficulty_damage_scale.F2'),
            ('medium', 'IDS_LOBBY_MEDIUM', 1, _f(1.0), _f(1.0)),
            ('hard', 'IDS_LOBBY_HARD', 2, 'difficulty_damage_scale.F2', 'difficulty_damage_scale.F')]:
        out.append(f'  - id: {sid}\n    name: {s[name]}\n    ai_level: {level}                    '
                   f'# the computer players\' records: 0 easy, 1 medium, 2 hard\n'
                   f'    damage_scale_human_target: {target}\n    damage_scale_human_attacker: {attacker}\n'
                   f'    ai_cheats: []                  # the computer players\' cheats this difficulty keeps '
                   f'(docs/AI_RESEARCH.md; none: a fair game; any of {", ".join(AI_CHEATS)})\n')
    out.append('unit_limit:\n  minimum: unit_limit_range.L\n  maximum: unit_limit_range.L2\n  step: 50\n'
               '  default: unit_limit_default.L\n')
    out.append('wonders_for_victory:\n  maximum: wonders_for_victory_max.L\n  default: 3\n')
    out.append('defeat_without_recovery: true      # out with no citizens, no other units and no built town centre '
               'or capitol to train one (false: every vital unit and building gone, the original)\n')
    return ''.join(out)


CIV_ATTRIBUTES = [   # the original's switch (EECivilizationManager::ApplyUpgradeLevel): id, level, applies
    ('attack', 1, [{'stat': 'attack'}]),
    ('hit_points', 2, [{'stat': 'hit_points'}]),
    ('reload_time', 3, [{'stat': 'reload_time'}]),
    ('armor', 4, [{'stat': 'armor'}]),
    ('range', 5, [{'stat': 'range'}, {'stat': 'line_of_sight'}]),
    ('speed', 6, [{'stat': 'speed'}]),
    ('flight_time', 7, [{'stat': 'flight_time'}]),
    ('area_effect', 8, [{'stat': 'area_effect'}]),
    ('gather_stone_gold', 9, [{'stat': 'gather_time', 'slot': 5, 'except_category': 'naval_citizen'},
                              {'stat': 'gather_time', 'slot': 4, 'except_category': 'naval_citizen'}]),
    ('gather_slot_0', 10, [{'stat': 'gather_time', 'slot': 0}]),
    ('gather_slot_2', 11, [{'stat': 'gather_time', 'slot': 2}]),
    ('gather_slot_6', 12, [{'stat': 'gather_time', 'slot': 6}]),
    ('gather_slot_1', 13, [{'stat': 'gather_time', 'slot': 1}]),
    ('gather_slot_3', 14, [{'stat': 'gather_time', 'slot': 3}]),
    ('build_time', 15, [{'stat': 'build_time'}]),
    ('conversion', 16, [{'stat': 'conversion'}]),
    ('fishing', 17, [{'stat': 'gather_time', 'slot': 4, 'only_category': 'naval_citizen'}]),
    ('repair_rate', 18, [{'stat': 'repair_rate'}]),
    ('area_effect_scale', 19, [{'stat': 'area_effect_scale'}]),
    ('high_ground', 20, [{'stat': 'high_ground'}]),
    ('population', 21, [{'stat': 'population'}]),
    ('cost', 23, [{'stat': 'cost'}]),
]


def attribute_ids():
    """{level: attribute id} of the vanilla attributes (the export names dbcivilization's levels by them)."""
    pw = _consts('EECivPowerIDs.h', 'kCivPower')
    out = {lv: aid for aid, lv, _ in CIV_ATTRIBUTES}
    out.update({pw['kCivPowerFlamingArrows'] + k: n for k, n in enumerate(POWER_IDS)})
    return out


def attribute_levels(texts):
    """{attribute id: level} from civbuilder.yaml when texts has it, else the vanilla attributes."""
    t = texts.get('civbuilder')
    if t is None:
        return {v: k for k, v in attribute_ids().items()}
    doc = parse_yaml(t.decode('utf-8'))
    out = {}
    for a in doc.get('attributes') or []:
        if isinstance(a, dict) and isinstance(a.get('id'), str):
            out[a['id']] = _int(a.get('level'), 'civbuilder', 'level')
    return out


POWER_IDS = ['flaming_arrows', 'slavery', 'priest_tower', 'crusaders', 'fanaticism', 'emissaries', 'exploration',
             'insurance', 'metallurgy', 'advanced_mining', 'pathfinding', 'camouflage', 'bundeswehr', 'sas',
             'paratroopers', 'market', 'cyber_ninja', 'missile_base', 'cloaking', 'adaptation', 'expansionism',
             'conquistadors', 'just_in_time_manufacturing']


def civbuilder(ids, spec):
    pw = _consts('EECivPowerIDs.h', 'kCivPower')
    s = _consts('EEStringIDs.h', 'IDS_')
    out = [HEAD.format(name='civbuilder', format=spec['format'],
                       what='What each civilization builder attribute raises.')]
    out.append("""# An upgrade is a category (dbcivilization) and a level; the level is an attribute here, the category's
# mLevels[level - 1] its value and bonus. applies: stats (C++ primitives) with a mode: share (the stat's own rule:
# a share of the class's base value, the original's), add (+ value) or multiply (x (1 + value)); gather_time takes
# a slot (0 .. 6); only_category / except_category limit an apply to unit classes of a category; power: a civ power.
attributes:
""")
    rows = CIV_ATTRIBUTES + [(POWER_IDS[k], pw['kCivPowerFlamingArrows'] + k,
                              [{'stat': 'power', 'power': ids('dbcivpowers', k)}]) for k in range(23)]
    for aid, level, applies in rows:
        name = (s['IDS_CIV_BONUS_BASE'] if level <= 0x25 else s['IDS_CIV_POWER_BASE']) + level
        out.append(f'  - id: {aid}\n    level: {level}\n    name: {name}\n    applies:\n')
        for ap in applies:
            first = True
            for k, v in ap.items():
                out.append(('      - ' if first else '        ') + f'{k}: {v}\n')
                first = False
    out.append(f"""powers_category:                   # the special powers category appended to dbcivilization's
  id: special_powers               # (the keyed .civ files name its upgrades special_powers.<attribute>)
  name: {s['IDS_SPECIAL_POWERS']}
  unit_set: {ids('dbunitset', 0x66)}
  cost: special_powers_category_cost.L
  level_value: {_f(0.01)}
""")
    return ''.join(out)


def civpowers(ids, spec):
    t = _consts('EETechTreeTechID.h', 'kTech')
    ut = _consts('EEUnitClassIDs.h', 'kUnitType')
    snd = _consts('EESoundObject.h', 'kSound')
    gfx = _consts('EEGraphicEffect.h', 'kGraphicEffect')
    out = [HEAD.format(name='civpowers', format=spec['format'],
                       what='What granting a civ power does, after its mTechnology is researched.')]
    out.append("""# effects: C++ primitives. research_if_researched (if_researched, tech), load_sound (sound), load_effect (effect),
# pulse_sight (unit_type: a sight pulse of the power's mAmount around each), set_mining_limit (the power's
# mMiningLimit), enable_camouflage, allow_tech (tech), disallow_tech (tech: also made obsolete).
powers:
""")
    tech = lambda n: ids('dbtechtree', t[n])

    def research(a, b):
        return [('do', 'research_if_researched'), ('if_researched', tech(a)), ('tech', tech(b))]

    def sound(n):
        return [('do', 'load_sound'), ('sound', ids('dbsounds', snd[n]))]

    def effect(n):
        return [('do', 'load_effect'), ('effect', ids('dbgfxeffects', gfx[n] if isinstance(n, str) else n))]

    powers = [
        (0, [research('kTechSimpleBowmanResearch', 'kTechSimpleBowmanFlamingResearch'),
             research('kTechCompositeBowUpgrade', 'kTechCompositeBowFlamingUpgrade'),
             research('kTechLongBowUpgrade', 'kTechLongBowFlamingUpgrade')]),
        (1, [sound('kSoundSlavery')]),
        (2, [sound('kSoundPriestTowerConvert'), effect(247), effect(256)]),
        (3, [research('kTechShortSwordUpgrade', 'kTechShortSwordUpgradeCrusaders'),
             research('kTechLongSwordUpgrade', 'kTechLongSwordUpgradeCrusaders'),
             research('kTechKnightUpgrade', 'kTechKnightUpgradeCrusaders')]),
        (6, [[('do', 'pulse_sight'), ('unit_type', ids('dbobjects', ut['kUnitTypeTownCenter']))],
             [('do', 'pulse_sight'), ('unit_type', ids('dbobjects', ut['kUnitTypeCapitol']))]]),
        (9, [[('do', 'set_mining_limit')]]),
        (11, [[('do', 'enable_camouflage')]]),
        (12, [sound('kSoundLightning'), sound('kSoundThunder2'), effect('kGraphicEffectLightningUnit'),
              effect('kGraphicEffectDustEffect'), effect('kGraphicEffectDisintegrationAreaEffectIntermediate')]),
        (20, [[('do', 'disallow_tech'), ('tech', tech('kTechSettlement'))],
              [('do', 'allow_tech'), ('tech', tech('kTechTownCenter'))]]),
    ]
    for row, effects in powers:
        out.append(f'  - power: {ids("dbcivpowers", row)}\n    effects:\n')
        for e in effects:
            for i, (k, v) in enumerate(e):
                out.append(('      - ' if i == 0 else '        ') + f'{k}: {v}\n')
    return ''.join(out)


# the personalities the game defaults to (ee/EERules.h EEAIPersonalityRules): (id, comment, epoch start, epoch full,
# max citizen share, standing army, army floor, Prehistoric army); the rest is the balanced economy for all of them
AI_PERSONALITIES = [('balanced', 'the tuning the benchmarks settled on (docs/AI_RESEARCH.md 8.10-8.26)', '0.3', '0.8', '0.7', '0.25', '0.1', '8'),
                    ('tall', 'tech: saves for the next epoch early and hard', '0.15', '0.5', '0.7', '0.2', '0.1', '8'),
                    ('wide', 'boom: grows its workforce, advances late', '0.6', '1.0', '0.8', '0.15', '0.1', '8'),
                    ('aggressive', 'rush: a large standing army, a middling epoch pace', '0.4', '0.9', '0.7', '0.5', '0.3', '0')]
AI_FLOOR = ('food', 'wood', 'stone', 'gold', 'iron')


def aipersonalities(ids, spec):
    out = [HEAD.format(name='aipersonalities', format=spec['format'],
                       what="The computer players' personalities: the tuning of their decisions outside dbcpbehavior.")]
    lines = ['personalities:']
    for pid, what, start, full, citizens, army, floor, prearmy in AI_PERSONALITIES:
        lines += [f'  - id: {pid:<24}# {what}',
                  '    epoch_pressure:              # the elapsed share of the epoch timer (dbcpbehavior rows 28/29)',
                  f'      start: {start:<22}# a paid epoch advance is asked for from here ...',
                  f'      full: {full:<23}# ... and its cost is wholly reserved from here',
                  '    economy:',
                  '      surplus: 1000                # a resource with more in stock ...',
                  '      surplus_cut: 0.5             # ... gets this share of its gatherers',
                  '      low_stock: 150               # a resource with less in stock ...',
                  '      floor:                       # ... at least this share of the gatherers',
                  '        food: 0.1',
                  '        wood: 0.25',
                  '        stone: 0.05',
                  '        gold: 0.05',
                  '        iron: 0.05',
                  f'      max_citizen_share: {citizens:<10}# the citizen target: at most this share of the pop cap',
                  '      wood_slots: 15               # the gatherers wood takes (trees are not resource-center units)',
                  '      moves_per_cycle: 3           # busy gatherers moved to a short resource per 10 s',
                  '      demand_split: true           # gatherers by the tiered demand less the stock (false: the needs ledger)',
                  '      coast_fishing: true          # from the Stone Age: a dock and up to 8 fishing boats off any coast',
                  '    military:',
                  f'      standing_army: {army:<14}# the army kept per worker (up to the military share of the pop cap)',
                  f'      army_floor: {floor:<17}# while it is short: up to this much more floor share on its unit\'s costs',
                  '      militia: true                # an attack group on the citizens: nearby soldiers and citizens fight '
                  'when the odds allow, else the citizens run',
                  '      militia_towers: true         # ... the built towers near the attack count, and citizens run to a tower',
                  f'      prehistoric_army: {prearmy:<11}# the Prehistoric army cap unless a stronger enemy is near (0: none)']
    lines.append('assign: [balanced]                 # computer player n takes entry (n - 1) mod the count')
    out.append('\n'.join(lines) + '\n')
    return ''.join(out)


GENERATORS = {'combat': combat, 'gameoptions': gameoptions, 'civbuilder': civbuilder, 'civpowers': civpowers,
              'aipersonalities': aipersonalities}


def export(ids, spec):
    """{doc: text bytes} of the vanilla rules documents; ids: the export's {table: {key: id}}."""
    i = _Ids(ids)
    return {n: GENERATORS[n](i, spec).encode('utf-8') for n in DOCS}


# ---------------------------------------------------------------- validation

class _Map:
    def __init__(self, m, doc, what):
        if not isinstance(m, dict):
            raise TextError(f'{doc}: {what} must be a mapping')
        self.m, self.doc, self.used = m, doc, set()

    def where(self, v=None):
        line = getattr(v, 'line', 0) or getattr(self.m, 'line', 0)
        if not line:
            for x in self.m.values():
                line = getattr(x, 'line', 0)
                if line:
                    break
        return f'{self.doc} line {line}'

    def get(self, k):
        self.used.add(k)
        v = self.m.get(k)
        return None if v is None or (isinstance(v, Scalar) and not v.quoted and v in ('', '~', 'null')) else v

    def done(self):
        extra = [k for k in self.m if k not in self.used]
        if extra:
            raise TextError(f'{self.where()}: unknown key {extra[0]!r}')


def _int(v, w, k):
    try:
        return scalars.parse_int(v, 's32')
    except Exception as e:
        raise TextError(f'{w}: {k}: {e}') from None


def _float(v, w, k):
    try:
        return scalars.parse_f32(v)
    except Exception as e:
        raise TextError(f'{w}: {k}: {e}') from None


def _number(v, w, k, spec):
    m = WORLD_VALUE.match(v) if isinstance(v, str) else None
    if m:
        rows = spec['row_names']['dbworld']
        fields = spec['tables']['dbworld'].get('row_fields', {})
        row = next((r for r, n in rows.items() if n == m.group(1)), None)
        want = {'F': 'mFloatValue', 'F2': 'mFloatValue2', 'L': 'mLongValue', 'L2': 'mLongValue2'}[m.group(2)]
        if row is None:
            raise TextError(f'{w}: {k}: {v!r}: {m.group(1)!r} is not a dbworld id')
        if want not in fields.get(row, []):
            raise TextError(f'{w}: {k}: {v!r}: dbworld\'s {m.group(1)} has no {m.group(2)}')
        return v
    return _float(v, w, k)


def _named(v, w, k, names, what):
    if v not in names:
        raise TextError(f'{w}: {k}: {v!r} is not {what}')
    return names.index(v)


def _ref(content, table, v, w, k):
    idx = content.index(table)
    if not isinstance(v, str) or v not in idx:
        raise TextError(f'{w}: {k}: {v!r} is not an id of {table}')
    return idx[v]


def _list(v, w, k):
    if v is None:
        return []
    if not isinstance(v, list):
        raise TextError(f'{w}: {k} must be a list')
    return v


def _check_entries(entries, w, what):
    seen = set()
    for e in entries:
        if e in seen:
            raise TextError(f'{w}: duplicate {what} id {e!r}')
        seen.add(e)


def validate(name, text, content, spec):
    """Parses a rules document and checks every key and reference (tables.Content resolves the ids); raises
    TextError naming the line. The game's reader (EERules.cpp) applies the same rules."""
    try:
        doc = parse_yaml(text.decode('utf-8'))
    except UnicodeDecodeError as e:
        raise TextError(f'{name}: not UTF-8 ({e})') from None
    top = _Map(doc, name, 'the document')
    if str(top.get('eedata') or name) != name:
        raise TextError(f'{name}: the file says it is {doc.get("eedata")!r}')
    if top.get('format') is not None and str(top.get('format')) != str(spec['format']):
        raise TextError(f'{name}: format {doc["format"]}, this tool reads format {spec["format"]}')

    def sub(m, k, fn):
        v = m.get(k)
        if v is not None:
            x = _Map(v, name, k)
            fn(x)
            x.done()

    def entries(m, k, fn):
        out = []
        for e in _list(m.get(k), m.where(), k):
            x = _Map(e, name, 'an entry')
            out.append(fn(x))
            x.done()
        return out

    if name == 'combat':
        def damage(x):
            for k in ('minimum', 'trample_damage'):
                if x.get(k) is not None:
                    _int(x.get(k), x.where(), k)
            for k in ('elevation_step', 'ally_factor', 'herd_ally_factor', 'dead_food_share', 'trample_threshold'):
                if x.get(k) is not None:
                    _float(x.get(k), x.where(), k)
            if x.get('fanatic_factor') is not None:
                _number(x.get('fanatic_factor'), x.where(), 'fanatic_factor', spec)

        def elevation(x):
            for k in ('attackers', 'launchers', 'targets'):
                for c in _list(x.get(k), x.where(), k):
                    _named(c, x.where(c), k, CATEGORIES, 'a unit category')

        def morale(x):
            for k in ('positive_factor', 'negative_factor'):
                if x.get(k) is not None:
                    _float(x.get(k), x.where(), k)
            for u in _list(x.get('buildings'), x.where(), 'buildings'):
                _ref(content, 'dbobjects', u, x.where(u), 'buildings')

        def ticks(x):
            for k in ('regeneration', 'power_regeneration'):
                if x.get(k) is not None:
                    _int(x.get(k), x.where(), k)
        sub(top, 'damage', damage)
        sub(top, 'elevation', elevation)
        sub(top, 'morale', morale)
        sub(top, 'ticks', ticks)
    elif name == 'gameoptions':
        def size(x):
            for k in ('name', 'tiles', 'script_size'):
                if x.get(k) is not None:
                    _int(x.get(k), x.where(), k)
            if x.get('script_size') is not None and not 0 <= _int(x.get('script_size'), x.where(), 'k') <= 5:
                raise TextError(f'{x.where()}: script_size is a random map script size class, 0 .. 5')
            return x.get('id')

        def speed(x):
            if x.get('name') is not None:
                _int(x.get('name'), x.where(), 'name')
            if x.get('factor') is not None:
                _number(x.get('factor'), x.where(), 'factor', spec)
            return x.get('id')

        def difficulty(x):
            for k in ('name', 'ai_level'):
                if x.get(k) is not None:
                    _int(x.get(k), x.where(), k)
            if x.get('ai_level') is not None and not 0 <= _int(x.get('ai_level'), x.where(), 'ai_level') <= 2:
                raise TextError(f'{x.where()}: ai_level is 0, 1 or 2')
            for k in ('damage_scale_human_target', 'damage_scale_human_attacker'):
                if x.get(k) is not None:
                    _number(x.get(k), x.where(), k, spec)
            for c in _list(x.get('ai_cheats'), x.where(), 'ai_cheats'):
                if str(c) not in AI_CHEATS:
                    raise TextError(f'{x.where()}: ai_cheats: {c!r} is not one of {", ".join(AI_CHEATS)}')
            return x.get('id')
        lists = {}
        for k, fn in (('map_sizes', size), ('speeds', speed), ('difficulties', difficulty)):
            lists[k] = entries(top, k, fn)
            _check_entries(lists[k], top.where(), k)
        for k, lk in (('map_size_default', 'map_sizes'), ('speed_default', 'speeds')):
            v = top.get(k)
            if v is not None and lists[lk] and v not in lists[lk]:
                raise TextError(f'{top.where(v)}: {k}: {v!r} is not an entry of {lk}')

        def limit(x):
            for k in ('minimum', 'maximum', 'default'):
                if x.get(k) is not None:
                    _number(x.get(k), x.where(), k, spec)
            if x.get('step') is not None:
                _int(x.get('step'), x.where(), 'step')

        def wonders(x):
            if x.get('maximum') is not None:
                _number(x.get('maximum'), x.where(), 'maximum', spec)
            if x.get('default') is not None:
                _int(x.get('default'), x.where(), 'default')
        sub(top, 'unit_limit', limit)
        sub(top, 'wonders_for_victory', wonders)
        if top.get('defeat_without_recovery') is not None and str(top.get('defeat_without_recovery')) not in (
                '0', '1', 'true', 'false'):
            raise TextError(f'{top.where()}: defeat_without_recovery: true or false')
    elif name == 'civbuilder':
        levels = set()

        def attribute(x):
            if x.get('name') is not None:
                _int(x.get('name'), x.where(), 'name')
            lv = _int(x.get('level'), x.where(), 'level')
            if lv < 1 or lv in levels:
                raise TextError(f'{x.where()}: level {lv}: at least 1, one attribute per level')
            levels.add(lv)
            for ap in _list(x.get('applies'), x.where(), 'applies'):
                y = _Map(ap, name, 'an apply')
                st = _named(y.get('stat'), y.where(), 'stat', STATS, 'a stat')
                if y.get('mode') is not None and _named(y.get('mode'), y.where(), 'mode', MODES, 'a mode') != 0 and                         STATS[st] in ('hit_points', 'armor', 'power'):
                    raise TextError(f'{y.where()}: {STATS[st]} has only the share mode')
                slot = y.get('slot')
                if (STATS[st] == 'gather_time') != (slot is not None) or \
                        (slot is not None and not 0 <= _int(slot, y.where(), 'slot') <= 6):
                    raise TextError(f'{y.where()}: slot belongs to gather_time (0 .. 6), and only there')
                for k in ('only_category', 'except_category'):
                    if y.get(k) is not None:
                        _named(y.get(k), y.where(), k, CATEGORIES, 'a unit category')
                pw = y.get('power')
                if (STATS[st] == 'power') != (pw is not None):
                    raise TextError(f'{y.where()}: power names the civ power of stat power, and only there')
                if pw is not None:
                    _ref(content, 'dbcivpowers', pw, y.where(), 'power')
                y.done()
            return x.get('id')
        _check_entries(entries(top, 'attributes', attribute), top.where(), 'attribute')

        def powers(x):
            if x.get('id') is not None and not re.match(r'^[a-z][a-z0-9_]*$', str(x.get('id'))):
                raise TextError(f'{x.where()}: id {x.get("id")!r} is not an id')
            if x.get('name') is not None:
                _int(x.get('name'), x.where(), 'name')
            if x.get('unit_set') is not None:
                _ref(content, 'dbunitset', x.get('unit_set'), x.where(), 'unit_set')
            if x.get('cost') is not None:
                _number(x.get('cost'), x.where(), 'cost', spec)
            if x.get('level_value') is not None:
                _float(x.get('level_value'), x.where(), 'level_value')
        sub(top, 'powers_category', powers)
    elif name == 'aipersonalities':
        def personality(x):
            e = x.get('epoch_pressure')
            if e is not None:
                y = _Map(e, name, 'epoch_pressure')
                st = _float(y.get('start'), y.where(), 'start') if y.get('start') is not None else 0.3
                fu = _float(y.get('full'), y.where(), 'full') if y.get('full') is not None else 0.8
                if not (st >= 0 and fu > st):
                    raise TextError(f'{y.where()}: epoch_pressure: 0 <= start < full')
                y.done()
            c = x.get('economy')
            if c is not None:
                y = _Map(c, name, 'economy')
                for k in ('surplus', 'low_stock', 'wood_slots', 'moves_per_cycle'):
                    if y.get(k) is not None:
                        _int(y.get(k), y.where(), k)
                for k in ('surplus_cut', 'max_citizen_share'):
                    if y.get(k) is not None:
                        _float(y.get(k), y.where(), k)
                for k in ('demand_split', 'coast_fishing'):
                    if y.get(k) is not None and str(y.get(k)) not in ('0', '1', 'true', 'false'):
                        raise TextError(f'{y.where()}: {k}: true or false')
                f = y.get('floor')
                if f is not None:
                    z = _Map(f, name, 'floor')
                    for k in AI_FLOOR:
                        if z.get(k) is not None:
                            _float(z.get(k), z.where(), k)
                    z.done()
                y.done()
            m = x.get('military')
            if m is not None:
                y = _Map(m, name, 'military')
                if y.get('standing_army') is not None and _float(y.get('standing_army'), y.where(), 'standing_army') < 0:
                    raise TextError(f'{y.where()}: military: standing_army must be 0 or more')
                if y.get('army_floor') is not None:
                    _float(y.get('army_floor'), y.where(), 'army_floor')
                for k in ('militia', 'militia_towers'):
                    if y.get(k) is not None and str(y.get(k)) not in ('0', '1', 'true', 'false'):
                        raise TextError(f'{y.where()}: {k}: true or false')
                if y.get('prehistoric_army') is not None and _int(y.get('prehistoric_army'), y.where(), 'prehistoric_army') < 0:
                    raise TextError(f'{y.where()}: military: prehistoric_army must be 0 or more')
                y.done()
            return x.get('id')
        found = entries(top, 'personalities', personality)
        _check_entries(found, top.where(), 'personalities')
        for a in _list(top.get('assign'), top.where(), 'assign'):
            if found and a not in found:
                raise TextError(f'{top.where(a)}: assign: {a!r} is not a personality')
    elif name == 'civpowers':
        refs = {'tech': 'dbtechtree', 'if_researched': 'dbtechtree', 'sound': 'dbsounds', 'effect': 'dbgfxeffects',
                'unit_type': 'dbobjects'}
        takes = {'research_if_researched': {'if_researched', 'tech'}, 'load_sound': {'sound'},
                 'load_effect': {'effect'}, 'pulse_sight': {'unit_type'}, 'allow_tech': {'tech'},
                 'disallow_tech': {'tech'}}

        def power(x):
            p = x.get('power')
            _ref(content, 'dbcivpowers', p, x.where(), 'power')
            for e in _list(x.get('effects'), x.where(), 'effects'):
                y = _Map(e, name, 'an effect')
                act = ACTIONS[_named(y.get('do'), y.where(), 'do', ACTIONS, 'a power effect')]
                given = set()
                for k, t in refs.items():
                    if y.get(k) is not None:
                        _ref(content, t, y.get(k), y.where(), k)
                        given.add(k)
                if given != takes.get(act, set()):
                    raise TextError(f'{y.where()}: {act} takes {sorted(takes.get(act, set())) or "nothing"}')
                y.done()
            return p
        _check_entries(entries(top, 'powers', power), top.where(), 'power')
    top.done()


def validate_epochs(text, content):
    """epochs.yaml's media keys (the tech tree compiler leaves them to the game, EERules.cpp): every epoch's music
    (dbmusic ids) and citizen block."""
    try:
        doc = parse_yaml(text.decode('utf-8'))
    except UnicodeDecodeError as e:
        raise TextError(f'epochs: not UTF-8 ({e})') from None
    for e in doc.get('records') or []:
        if not isinstance(e, dict):
            continue
        m = _Map(e, 'epochs', 'an epoch')
        for s in _list(m.get('music'), m.where(), 'music'):
            _ref(content, 'dbmusic', s, m.where(s), 'music')
        c = m.get('citizen')
        if c is not None:
            x = _Map(c, 'epochs', 'citizen')
            if x.get('high_tech_tools') is not None and str(x.get('high_tech_tools')) not in ('0', '1', 'true', 'false'):
                raise TextError(f'{x.where()}: high_tech_tools: 0 or 1')
            for f in _list(x.get('removal_effects'), x.where(), 'removal_effects'):
                _ref(content, 'dbgfxeffects', f, x.where(f), 'removal_effects')
            for k in ('removal_sound', 'removal_thunder'):
                if x.get(k) is not None:
                    _ref(content, 'dbsounds', x.get(k), x.where(), k)
            if x.get('removal_pitch_offset') is not None:
                _float(x.get('removal_pitch_offset'), x.where(), 'removal_pitch_offset')
            if x.get('removal_thunder_every') is not None and \
                    _int(x.get('removal_thunder_every'), x.where(), 'removal_thunder_every') < 0:
                raise TextError(f'{x.where()}: removal_thunder_every must be 0 (never) or more')
            x.done()


def check_all(texts, content, spec):
    """Validates every rules document in texts and epochs.yaml's media; [(doc, error)]."""
    bad = []
    for n in DOCS:
        if n in texts:
            try:
                validate(n, texts[n], content, spec)
            except TextError as e:
                bad.append((n, str(e)))
    if 'epochs' in texts:
        try:
            validate_epochs(texts['epochs'], content)
        except TextError as e:
            bad.append(('epochs', str(e)))
    return bad
