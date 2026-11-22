"""Row ids (format 3): every listed row of every db table has an `id`, a stable, unique, lowercase ASCII key.

The converter generates them once, from the row's name (overlay.ID_NAMES: the label, mName or a lead string; the
reader's name for dbworld and dbcpbehavior; the first event that lists it for dbeffects), with _2, _3 ... where two
rows would get the same id. From then on the text owns the id: it is the entry's identity, the name or label is
display text. References between tables are ids (tables.py), resolved to the target row's key on import.

Ids match ID_RE: a letter, then letters, digits and '_'. A name that gives no id makes `<table>_<key>`.
"""
import pathlib
import re
import unicodedata

import binfmt
import overlay
import scalars

ID_RE = re.compile(r'^[a-z][a-z0-9_]*$')
ROOT = pathlib.Path(__file__).resolve().parent.parent.parent


def slug(name):
    s = unicodedata.normalize('NFKD', name).encode('ascii', 'ignore').decode('ascii').lower()
    s = re.sub(r'[^a-z0-9]+', '_', s.replace("'", '')).strip('_')
    if s and not s[0].isalpha():
        s = 'n' + s
    return s


# The computer player behaviour records' names by record index (the readers' names: Language.rc 28000 + index, else the
# EEComputerPlayer::Init member the record fills; they were the kCPBehavior* enumerators of ee/EECPBehavior.h, which
# the game does not use since the computer player moved to Rust)
CPBEHAVIOR_NAMES = {
    0: 'None', 1: 'InitialAttackDelay', 2: 'AttackDelayModifier', 3: 'CitizenPercentage', 4: 'TroopsToAssistAllies',
    5: 'ShipsToMaintain', 6: 'AttackFrequency', 7: 'AttackFrequencyModifier', 8: 'NumberOfLandExplorers',
    9: 'NumberOfSeaExplorers', 10: 'NumberOfAirExplorers', 11: 'AttackGroupModifier', 12: 'CountWeights',
    13: 'AttackGroupSize', 14: 'ResourceDropOffDistance', 15: 'ResearchFrequency', 18: 'InitialAttackDelayPerEpoch',
    19: 'AttackFrequencyBoundaries', 20: 'DefensiveUnitCap', 21: 'AttackFrequencyBoundary',
    22: 'AttackFrequencyPerEpoch', 23: 'ResearchFrequencyModifier', 24: 'AttackWonder', 25: 'AttackEnemy',
    26: 'AttackToClaimResource', 27: 'AttackForRevenge', 28: 'EpochFrequency', 29: 'EpochFrequencyModifier',
    30: 'DesiredNumberOfBarracks', 31: 'DesiredNumberOfArcheryRanges', 32: 'DesiredNumberOfStables',
    33: 'DesiredNumberOfDocks', 34: 'DesiredNumberOfSiegeFactories', 35: 'DesiredNumberOfNavalYards',
    36: 'DesiredNumberOfAirports', 37: 'DesiredNumberOfCyberFactories', 38: 'DesiredNumberOfUniversities',
    39: 'DesiredNumberOfTemples', 40: 'DesiredNumberOfHospitals', 41: 'DesiredNumberOfGranaries',
    42: 'DesiredNumberOfCyberLaboratories', 43: 'NumberOfMilitaryUnitsToTrain', 44: 'ShipsToMaintainDeathmatch',
    45: 'SpecializedUnitPercentage', 46: 'UnitSelectedForImprovements', 47: 'UnitAttributesSelectedForImprovements',
    48: 'MaximumAttackResponseRange', 49: 'PriestUsage', 50: 'ProphetUsage', 51: 'HeroUsage', 52: 'CarrierUsage',
    53: 'RPSSelectionNonIslands', 54: 'SpecializedUnitSelectionChance', 55: 'EnableStockCivilization',
    56: 'EnableStockResource', 57: 'EnableStockResearch', 58: 'EnableStockProduction', 59: 'EnableStockBuild',
    61: 'EnableStockDefense', 62: 'EnableStockExploration', 63: 'EnableStockCombat', 64: 'ManagerEpochAdvanceWait',
    65: 'ManagerAttackWait', 66: 'MaxEnabledManagers', 67: 'UnitCostReductionMedium', 68: 'EnableInterval',
    69: 'AidThreatThreshold', 70: 'GuardChokepointPercentage', 71: 'GuardContinentPercentage',
    72: 'GuardHighGroundPercentage', 73: 'GuardTownLandPercentage', 77: 'GuardChokepointAirPercentage',
    78: 'GuardTownPercentage', 82: 'GuardWaterwayPercentage', 86: 'NumChokePointsToGuard',
    87: 'DefenseGroupSizeModifierPercentage', 88: 'DefensiveUnitUpdate',
    89: 'InitialDelayInReplenishmentDefenseUnits', 90: 'TownGuardPercentage', 91: 'ComputerVsComputerAggression',
    92: 'PopulationCapOtherWeight', 93: 'PopulationCapOwnWeight', 94: 'EpochAdvanceTimeScale',
    95: 'DesiredNumberOfTowers', 96: 'UnitCostReductionEasyAndHard', 97: 'ExpansionTimer',
    98: 'PopulationLimitAdjustment', 99: 'TownCenterUpgradeTimer', 100: 'CapitolUpgradeTimer',
    101: 'ComputerPlayerExtraResources', 102: 'EpochCatchUp', 103: 'EasyMinNumberMilitaryBuildings',
    104: 'HardMinNumberMilitaryBuildings', 105: 'DMEarlyAttack', 106: 'MediumMinNumberMilitaryBuildings',
    107: 'EpochCatchUpModifier', 108: 'DesiredNumberOfTankFactories', 109: 'DesiredNumberOfAntiAirBuildings',
    110: 'RPSSelectionIslands', 111: 'NavalUnitWeight', 112: 'AirUnitWeight', 113: 'DesiredNumberOfFlameThrowers',
    114: 'TownGuardUpdate', 115: 'TownGuardUpdateDeathmatch', 116: 'DefensiveUnitUpdateDeathmatch',
    117: 'DesiredNumberOfSpaceDocks', 118: 'MinimumShipMaintenanceTimer', 119: 'SiegeWeaponPercentage',
    120: 'BonzaiAttackLevel', 121: 'IslandExpansionTimer', 122: 'DMAttackFrequencyModifier',
    123: 'DMInitialAttackModifier', 124: 'CarrierFightersToMaintain', 125: 'CarrierFightersToMaintainDM',
}


def cpbehavior_names():
    """{record index: id} from CPBEHAVIOR_NAMES."""
    return {k: overlay.snake(n) for k, n in CPBEHAVIOR_NAMES.items()}


def assign(named, prefix):
    """[(key, name or '')] in row order -> {key: id}: unique, first come keeps the plain form."""
    out, used = {}, set()
    for key, name in named:
        base = slug(name) if name else ''
        if not base:
            base = f'{prefix}_{key}'.replace('-', 'm')
        i, cand = 1, base
        while cand in used:
            i += 1
            cand = f'{base}_{i}'
        used.add(cand)
        out[key] = cand
    return out


def _name_of(tspec, st, row, sources):
    byname = {f['name']: f for f in st['fields']}
    lead = []
    for spec in tspec.get('lead', []):   # only single lead strings name a row
        lead.append(spec if '[' not in spec else None)
    for src in sources:
        if src in byname and byname[src]['type'] == 'str':
            v = binfmt.field_value(byname[src], row.rec)[0]
            if v.strip():
                return scalars.decode_str(v)
        elif src in lead:
            k = binfmt_lead_index(tspec, src)
            if k is not None and row.lead[k].strip():
                return scalars.decode_str(row.lead[k])
    return ''


def binfmt_lead_index(tspec, name):
    i = 0
    for spec in tspec.get('lead', []):
        if spec == name:
            return i
        if '|' in spec:
            i += 2 * int(spec.split('[')[1][:-1])
        elif '[' in spec:
            i += int(spec.split('[')[1][:-1])
        else:
            i += 1
    return None


def key_of(tspec, st, row, pos):
    """The row's runtime integer: its position, or the key field (dbtechtree mID)."""
    if tspec['key'] == 'row':
        return pos
    f = next(f for f in st['fields'] if f['name'] == tspec['key'])
    return binfmt.field_value(f, row.rec)


def listed(name, t, spec):
    """[(key, Row)] of the rows the text lists, in file order (dbtechtree: each epoch record, then its nodes)."""
    tspec = spec['tables'][name]
    st = spec['structs'][tspec['struct']]
    out = []
    if name == 'epochs':
        return []
    if t.epochs is not None:
        for e in t.epochs:
            out.append((key_of(tspec, st, e.epoch, None), e.epoch))
            out += [(key_of(tspec, st, r, i), r) for i, r in enumerate(e.nodes) if r is not None]
        return out
    names = spec.get('row_names', {}).get(name)
    return [(i, r) for i, r in enumerate(t.rows) if r is not None and (not names or str(i) in names)]


def generate(tables, spec):
    """{table: Table (holes detected)} -> {table: {key: id}} for every listed row."""
    out = {}
    for name in sorted(tables):
        if name == 'dbeffects':
            continue
        tspec = spec['tables'][name]
        st = spec['structs'][tspec['struct']]
        rows = listed(name, tables[name], spec)
        src = tspec['id_names']
        prefix = name[2:] if name.startswith('db') else name
        if src == ['@world']:
            names = {int(k): v for k, v in spec['row_names'][name].items()}
            out[name] = {k: names[k] for k, _ in rows}
            continue
        if src == ['@cpbehavior']:
            cp = cpbehavior_names()
            named = [(k, cp.get(k, '')) for k, _ in rows]
            prefix = 'behavior'
        else:
            named = [(k, _name_of(tspec, st, r, src)) for k, r in rows]
        named = [(k, overlay.ID_OVERRIDES.get(name, {}).get(k, n)) for k, n in named]
        out[name] = assign(named, prefix)
    if 'dbtechtree' in out:   # the epochs: the epoch records of dbtechtree, keyed by their position
        out['epochs'] = {k: out['dbtechtree'][k] for k in range(len(tables['dbtechtree'].epochs))}
    if 'dbeffects' in tables:   # an effect is named after the first event that lists it: <event>_<n>
        ev = out.get('dbevents', {})
        named = {}
        for k, r in listed('dbevents', tables['dbevents'], spec) if 'dbevents' in tables else []:
            for n, e in enumerate(r.lst, 1):
                named.setdefault(e, f'{ev[k]}_{n}')
        rows = listed('dbeffects', tables['dbeffects'], spec)
        out['dbeffects'] = assign([(k, named.get(k, '')) for k, _ in rows], 'effect')
    return out
