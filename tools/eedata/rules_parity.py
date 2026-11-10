"""Edited rules documents (what a modder writes, valid and broken): the rules reader's parity cases, run by cppgate.
Every case edits one document; the result is the rules dump or the message.
"""
import re


def _sub(texts, doc, old, new, count=1):
    s = texts[doc].decode('utf-8')
    assert old in s, f'{doc}: {old[:50]}'
    return s.replace(old, new, count).encode('utf-8')


def _re(texts, doc, pat, new, count=1):
    s = texts[doc].decode('utf-8')
    out, n = re.subn(pat, new, s, count=count)
    assert n, f'{doc}: {pat}'
    return out.encode('utf-8')


def _app(texts, doc, tail):
    return texts[doc] + tail.encode('utf-8')


def cases(texts):
    """texts: {doc: bytes} (the rules documents, epochs and dbcivilization of the tree) -> [(name, doc, text)]."""
    out = []

    def add(name, doc, text):
        out.append((name, doc, text))

    # head of a document
    add('c_ok', 'combat', texts['combat'])
    add('c_badname', 'combat', _sub(texts, 'combat', 'eedata: combat', 'eedata: gameoptions'))
    add('c_badformat', 'combat', _sub(texts, 'combat', 'format: 3', 'format: 9'))
    add('c_unknown_top', 'combat', _app(texts, 'combat', 'colour: 1\n'))
    add('c_empty', 'combat', b'')
    add('c_yaml_error', 'combat', _app(texts, 'combat', 'damage:\n'))
    add('c_not_mapping', 'combat', b'- 1\n')
    add('c_tab', 'combat', _sub(texts, 'combat', '\n  minimum', '\n\tminimum'))
    # combat values
    add('c_int_bad', 'combat', _sub(texts, 'combat', 'minimum: 1', 'minimum: x'))
    add('c_int_float', 'combat', _sub(texts, 'combat', 'minimum: 1', 'minimum: 1.5'))
    add('c_float_bad', 'combat', _re(texts, 'combat', r'elevation_step: \S+', 'elevation_step: abc'))
    add('c_float_int', 'combat', _re(texts, 'combat', r'elevation_step: \S+', 'elevation_step: 2'))
    add('c_float_nan', 'combat', _re(texts, 'combat', r'ally_factor: \S+', 'ally_factor: .nan'))
    add('c_number_world', 'combat', _sub(texts, 'combat', 'fanatic_factor: fanaticism.F2', 'fanatic_factor: fanaticism.F'))
    add('c_number_world2', 'combat', _sub(texts, 'combat', 'fanatic_factor: fanaticism.F2', 'fanatic_factor: fanaticism.L'))
    add('c_number_badworld', 'combat', _sub(texts, 'combat', 'fanatic_factor: fanaticism.F2', 'fanatic_factor: nope.F'))
    add('c_number_badvalue', 'combat', _sub(texts, 'combat', 'fanatic_factor: fanaticism.F2', 'fanatic_factor: fanaticism.Q'))
    add('c_number_int', 'combat', _sub(texts, 'combat', 'fanatic_factor: fanaticism.F2', 'fanatic_factor: 2'))
    add('c_number_float', 'combat', _sub(texts, 'combat', 'fanatic_factor: fanaticism.F2', 'fanatic_factor: 2.5'))
    add('c_number_hex', 'combat', _sub(texts, 'combat', 'fanatic_factor: fanaticism.F2', 'fanatic_factor: 0x10'))
    add('c_number_exp', 'combat', _sub(texts, 'combat', 'fanatic_factor: fanaticism.F2', 'fanatic_factor: 1e2'))
    add('c_number_inf', 'combat', _sub(texts, 'combat', 'fanatic_factor: fanaticism.F2', 'fanatic_factor: inf'))
    add('c_number_huge', 'combat', _sub(texts, 'combat', 'fanatic_factor: fanaticism.F2', 'fanatic_factor: 3e12'))
    add('c_key_removed', 'combat', _re(texts, 'combat', r'  dead_food_share: [^\n]*\n', ''))
    add('c_section_removed', 'combat', _re(texts, 'combat', r'ticks:\n.*', ''))
    add('c_unknown_key', 'combat', _sub(texts, 'combat', '  minimum: 1', '  minimum: 1\n  colour: 2'))
    add('c_section_scalar', 'combat', _re(texts, 'combat', r'damage:[^\n]*\n(  [^\n]*\n)*', 'damage: 3\n'))
    add('c_category_bad', 'combat', _sub(texts, 'combat', 'attackers: [land,', 'attackers: [nope,'))
    add('c_categories_scalar', 'combat', _re(texts, 'combat', r'attackers: \[[^\]]*\]', 'attackers: land'))
    add('c_ref_bad', 'combat', _re(texts, 'combat', r'buildings: \[[^\]]*\]', 'buildings: [no_such_house]'))
    add('c_ref_not_id', 'combat', _re(texts, 'combat', r'buildings: \[[^\]]*\]', 'buildings: [37]'))
    add('c_ref_empty', 'combat', _re(texts, 'combat', r'buildings: \[[^\]]*\]', 'buildings: []'))
    add('c_ticks', 'combat', _sub(texts, 'combat', 'regeneration: 1000', 'regeneration: 250'))
    # gameoptions
    g = 'gameoptions'
    add('g_ok', g, texts[g])
    add('g_tiles', g, _sub(texts, g, 'tiles: 42', 'tiles: 0'))
    add('g_script_size', g, _sub(texts, g, 'script_size: 2', 'script_size: 9'))
    add('g_name_bad', g, _re(texts, g, r'name: \d+', 'name: x'))
    add('g_no_id', g, _sub(texts, g, '- id: tiny', '- nom: tiny'))
    add('g_bad_id', g, _sub(texts, g, 'id: tiny', 'id: Tiny'))
    add('g_dup_id', g, _sub(texts, g, 'id: small', 'id: tiny'))
    add('g_no_sizes', g, _re(texts, g, r'map_sizes:\n(  [^\n]*\n)+', 'map_sizes: []\n'))
    add('g_size_default', g, _sub(texts, g, 'map_size_default: small', 'map_size_default: tiny'))
    add('g_size_default_bad', g, _sub(texts, g, 'map_size_default: small', 'map_size_default: nope'))
    add('g_speeds_short', g, _re(texts, g, r'speeds:\n(  [^\n]*\n)+', 'speeds:\n  - id: only\n    factor: 1.0\n'))
    add('g_speed_default', g, _sub(texts, g, 'speed_default: normal', 'speed_default: fast'))
    add('g_speed_default_bad', g, _sub(texts, g, 'speed_default: normal', 'speed_default: nope'))
    add('g_ai_level', g, _sub(texts, g, 'ai_level: 2', 'ai_level: 5'))
    add('g_cheats', g, _sub(texts, g, 'ai_cheats: []', 'ai_cheats: [cost, reveal]'))
    add('g_cheats_bad', g, _sub(texts, g, 'ai_cheats: []', 'ai_cheats: [nope]'))
    add('g_scale_num', g, _sub(texts, g, 'damage_scale_human_target: difficulty_damage_scale.F', 'damage_scale_human_target: 0.5'))
    add('g_step_zero', g, _sub(texts, g, 'step: 50', 'step: 0'))
    add('g_defeat_bad', g, _sub(texts, g, 'defeat_without_recovery: true', 'defeat_without_recovery: maybe'))
    add('g_defeat_false', g, _sub(texts, g, 'defeat_without_recovery: true', 'defeat_without_recovery: 0'))
    add('g_wonders', g, _sub(texts, g, 'default: 3', 'default: 5'))
    # civbuilder
    c = 'civbuilder'
    add('b_ok', c, texts[c])
    add('b_level_zero', c, _sub(texts, c, 'level: 1\n', 'level: 0\n'))
    add('b_level_huge', c, _sub(texts, c, 'level: 1\n', 'level: 70000\n'))
    add('b_dup_level', c, _sub(texts, c, 'level: 2\n', 'level: 1\n'))
    add('b_dup_id', c, _sub(texts, c, 'id: hit_points', 'id: attack'))
    add('b_stat_bad', c, _sub(texts, c, 'stat: attack', 'stat: nope'))
    add('b_mode_bad', c, _sub(texts, c, 'stat: attack', 'stat: attack\n        mode: nope'))
    add('b_mode_add', c, _sub(texts, c, 'stat: attack', 'stat: attack\n        mode: add'))
    add('b_mode_hp', c, _sub(texts, c, 'stat: hit_points', 'stat: hit_points\n        mode: multiply'))
    add('b_slot_missing', c, _sub(texts, c, 'stat: gather_time\n        slot: 5', 'stat: gather_time'))
    add('b_slot_wrong', c, _sub(texts, c, 'stat: attack', 'stat: attack\n        slot: 1'))
    add('b_slot_big', c, _sub(texts, c, 'slot: 5', 'slot: 7'))
    add('b_only_bad', c, _sub(texts, c, 'stat: attack', 'stat: attack\n        only_category: nope'))
    add('b_except', c, _sub(texts, c, 'stat: attack', 'stat: attack\n        except_category: land'))
    add('b_power_bad', c, _re(texts, c, r'power: \w+', 'power: nope'))
    add('b_power_on_attack', c, _sub(texts, c, 'stat: attack', 'stat: attack\n        power: flaming_arrows'))
    add('b_unknown', c, _sub(texts, c, 'stat: attack', 'stat: attack\n        colour: 1'))
    add('b_powers_id', c, _sub(texts, c, 'id: special_powers', 'id: Bad'))
    add('b_powers_unit_set', c, _re(texts, c, r'unit_set: \w+', 'unit_set: nope'))
    add('b_no_applies', c, _re(texts, c, r'    applies:\n(      [^\n]*\n)+', '', 1))
    # civpowers
    p = 'civpowers'
    add('p_ok', p, texts[p])
    add('p_bad_power', p, _re(texts, p, r'- power: \w+', '- power: nope'))
    add('p_dup_power', p, _sub(texts, p, '- power: slavery', '- power: flaming_arrows'))
    add('p_bad_action', p, _sub(texts, p, 'do: load_sound', 'do: nope'))
    add('p_action_needs_sound', p, _re(texts, p, r'do: load_sound\n\s+sound: \w+', 'do: load_sound'))
    add('p_action_extra', p, _sub(texts, p, 'do: set_mining_limit', 'do: set_mining_limit\n        tech: nope'))
    add('p_ref_bad', p, _re(texts, p, r'sound: \w+', 'sound: no_such_sound'))
    add('p_tech_id_num', p, _re(texts, p, r'tech: \w+', 'tech: 3060'))
    # aipersonalities
    a = 'aipersonalities'
    add('a_ok', a, texts[a])
    add('a_script_obsolete', a, _re(texts, a, r'(  - id: \w+[^\n]*\n)', r'\1    next_script: a..b\n'))   # read and ignored
    add('a_pressure', a, _re(texts, a, r'start: \S+', 'start: 0.9'))
    add('a_pressure_order', a, _re(texts, a, r'full: \S+', 'full: 0.0'))
    add('a_span_neg', a, _re(texts, a, r'stone_span: \S+', 'stone_span: -1'))
    add('a_economy_neg', a, _re(texts, a, r'early_trainers: \S+', 'early_trainers: -1'))
    add('a_floor_bad', a, _re(texts, a, r'food: \S+', 'food: x'))
    add('a_floor_unknown', a, _re(texts, a, r'( +)food: \S+', r'\g<0>\n\1tin: 0.1'))
    add('a_bool_bad', a, _re(texts, a, r'militia: \S+', 'militia: maybe'))
    add('a_army_neg', a, _re(texts, a, r'standing_army: \S+', 'standing_army: -1'))
    add('a_prearmy_neg', a, _re(texts, a, r'prehistoric_army: \S+', 'prehistoric_army: -1'))
    add('a_assign_bad', a, _sub(texts, a, 'assign: [balanced]', 'assign: [nope]'))
    add('a_assign_two', a, _sub(texts, a, 'assign: [balanced]', 'assign: [balanced, tall]'))
    add('a_dup_id', a, _sub(texts, a, 'id: tall', 'id: balanced'))
    add('a_unknown', a, _sub(texts, a, '    economy:', '    colour: 1\n    economy:'))
    # epochs media
    e = 'epochs'
    add('e_ok', e, texts[e])
    add('e_music_bad', e, _sub(texts, e, 'music: [pawns,', 'music: [no_such_song,'))
    add('e_music_scalar', e, _sub(texts, e, 'music: [pawns,', 'music: pawns\n    musix: ['))
    add('e_citizen_bad', e, _re(texts, e, r'( +)citizen:\n', r'\g<0>\1  colour: 1\n'))
    # dbcivilization levels (the categories' keyed levels)
    if 'dbcivilization' in texts:
        d = 'dbcivilization'
        add('d_ok', d, texts[d])
        add('d_level_attr', d, _re(texts, d, r'attribute: \w+', 'attribute: nope'))
        add('d_level_value', d, _re(texts, d, r'value: [^\n]+', 'value: x'))
    return out
