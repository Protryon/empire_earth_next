"""Parity cases for the C++ loader: edited text forms (what a modder writes, valid and broken) that the Python
importer and the game's loader must treat alike, the same bytes or both an error. cppgate runs them.
"""
import re


def _csv_lines(text):
    return text.decode('utf-8').split('\n')


def _join(lines):
    return '\n'.join(lines).encode('utf-8')


def _cell(text, row, col, value):
    """CSV text with the cell (data row index, column name) replaced (cells are simple: no quotes/commas)."""
    lines = _csv_lines(text)
    hdr = next(i for i, l in enumerate(lines) if not l.startswith('#'))
    cols = lines[hdr].split(',')
    k = hdr + 1 + row
    cells = lines[k].split(',')
    cells[cols.index(col)] = value
    lines[k] = ','.join(cells)
    return _join(lines)


def _sub(text, old, new, count=1):
    s = text.decode('utf-8')
    assert old in s, old[:60]
    return s.replace(old, new, count).encode('utf-8')


def cases(texts):
    """texts: {table: text bytes} -> [(name, table, text bytes)]."""
    w, cp, civ, ev = texts['dbworld'], texts['dbcpbehavior'], texts['dbcivilization'], texts['dbevents']
    tt, snd = texts['dbtechtree'], texts['dbsounds']
    out = []

    def add(name, table, text):
        out.append((name, table, text))

    # numbers (CSV)
    for k, v in enumerate(['1e2', ' 3.5 ', '+2', '.5', '5.', '0x3f800000', '-0', '-0.0', '1.5e-7', '3.4028235e38',
                           '1e39', '1e-50', 'abc', '1,5', '.inf', '-.inf', '.nan', '0x7fc00001', 'inf', '1e', '',
                           '0.1000000001', '16777217', '1.17549435e-38', '1.4e-45', '7e-46']):
        add(f'w_float{k}', 'dbworld', _cell(w, 3, 'mFloatValue', v if ',' not in v else '"1,5"'))
    for k, v in enumerate(['2147483647', '2147483648', '-2147483648', '-2147483649', '0xffffffff', '0x100000000',
                           '007', '+5', '- 5', '5.0', '1e3', '0X1F', '']):
        add(f'w_long{k}', 'dbworld', _cell(w, 3, 'mLongValue', v))
    for k, v in enumerate(['4294967295', '4294967296', '-1', '0xffffffff']):
        add(f'w_ulong{k}', 'dbworld', _cell(w, 3, 'mLongValue2', v))
    # rows, labels, directives, line endings (CSV)
    lines = _csv_lines(w)
    add('w_crlf', 'dbworld', w.replace(b'\n', b'\r\n'))
    add('w_bom', 'dbworld', b'\xef\xbb\xbf' + w)
    add('w_norows', 'dbworld', _join([l for l in lines if not l.startswith('#rows')]))
    add('w_comment', 'dbworld', _join(lines[:1] + ['# a comment', ''] + lines[1:]))
    add('w_unknown_directive', 'dbworld', _join(['#colour blue'] + lines))
    add('w_badformat', 'dbworld', _sub(w, '#format 3', '#format 2'))
    add('w_wrongtable', 'dbworld', _sub(w, '#eedata dbworld', '#eedata dbfamily'))
    add('w_append', 'dbworld', w + b',new_constant,1.25,0.0,100,95\n')
    add('w_append_noid', 'dbworld', w + b',,1.25,0.0,100,95\n')
    add('w_append_row', 'dbworld', w + b'99,new_constant,1.25,0.0,100,95\n')
    add('w_rowsbig', 'dbworld', _sub(w, '#rows 95', '#rows 97'))
    add('w_gap', 'dbworld', w + b'120,far_constant,1,0,0,0\n')
    add('w_order', 'dbworld', _join(lines[:6] + [lines[7], lines[6]] + lines[8:]))
    add('w_id_ok', 'dbworld', _cell(w, 1, 'id', 'gatherers_per_resource_max'))
    add('w_id_bad', 'dbworld', _cell(w, 1, 'id', 'not_gatherers_per_resource_max'))
    add('w_id_upper', 'dbworld', _cell(w, 1, 'id', 'Gatherers'))
    add('w_id_dup', 'dbworld', _cell(w, 2, 'id', 'gatherers_per_resource_max'))
    add('w_id_missing', 'dbworld', _cell(w, 1, 'id', ''))
    add('w_unread_row', 'dbworld', _sub(w, '\n21,', '\n5,anything,1,2,3,4\n21,'))
    add('w_unread_cell', 'dbworld', _cell(w, 3, 'mFloatValue', '2.5'))
    add('w_cells', 'dbworld', w + b'1,2\n')
    add('w_unknown_col', 'dbworld', _sub(w, 'row,id,', 'row,id,colour,').replace(b'\n21,', b'\n21,x,', 1))
    add('w_unread_col', 'dbworld', _sub(w, 'row,id,', 'row,id,mUnusedIndex,').replace(b'\n21,', b'\n21,3,', 1))
    add('w_dropcol', 'dbworld', _join([','.join(c for i, c in enumerate(l.split(',')) if i != 5)
                                       if not l.startswith('#') and l else l for l in lines]))
    add('w_quoted', 'dbworld', _cell(w, 3, 'mLongValue', '"12"'))
    add('w_badquote', 'dbworld', _cell(w, 3, 'mLongValue', '1"2'))
    add('w_blanklines', 'dbworld', w.replace(b'\n26,', b'\n\n26,'))
    add('w_empty', 'dbworld', b'')
    # cpbehavior: a negative slot index
    add('cp_neg', 'dbcpbehavior', _cell(cp, 0, 'mIndex', '-7'))
    # strings (CSV with lead strings)
    add('s_euro', 'dbsounds', _sub(snd, 'sounds\\', 'sounds\u20ac\\'))
    add('s_cjk', 'dbsounds', _sub(snd, 'sounds\\', 'sounds\u4e2d\\'))
    add('s_c1', 'dbsounds', _sub(snd, 'sounds\\', 'sounds\u0081\\'))
    add('s_c1bad', 'dbsounds', _sub(snd, 'sounds\\', 'sounds\u0080\\'))
    add('s_comma', 'dbsounds', _sub(snd, ',"sounds\\', ',"sounds,"",\\') if b',"sounds\\' in snd else snd)
    long_name = 'x' * 99
    add('s_name99', 'dbsounds', _cell_first_name(snd, long_name))
    add('s_name100', 'dbsounds', _cell_first_name(snd, long_name + 'y'))
    add('s_name101', 'dbsounds', _cell_first_name(snd, long_name + 'yz'))
    # YAML: comments, quoting, flow lists, missing / unknown / duplicate keys, indentation
    add('y_ok', 'dbcivilization', civ)
    add('y_comment', 'dbcivilization', _sub(civ, '\nrecords:', '\n# hello\nrecords: # the rows'))
    add('y_single', 'dbcivilization', re.sub(rb'label: "([^"\\]*)"', rb"label: '\1'", civ, count=3))
    add('y_plain', 'dbcivilization', re.sub(rb'label: "([A-Za-z ]+)"', rb'label: \1', civ, count=3))
    add('y_escape', 'dbcivilization', re.sub(rb'label: "([^"\\]*)"', rb'label: "\\x41\\u00e9\\t\1"', civ, count=1))
    add('y_badescape', 'dbcivilization', re.sub(rb'label: "([^"\\]*)"', rb'label: "\\q\1"', civ, count=1))
    add('y_tab', 'dbcivilization', civ.replace(b'\n    ', b'\n\t', 1))
    add('y_indent', 'dbcivilization', civ.replace(b'\n    ', b'\n     ', 1))
    add('y_dupkey', 'dbcivilization', _sub(civ, '\nformat: 3', '\nformat: 3\nformat: 3'))
    add('y_unknownkey', 'dbcivilization', _sub(civ, '    label:', '    colour: 3\n    label:'))
    add('y_unread_key', 'dbcivilization', _sub(civ, '    label:', '    mUnusedDatabaseID: 3\n    label:'))
    add('y_toplevel', 'dbcivilization', _sub(civ, '\nformat: 3', '\nformat: 3\nauthor: me'))
    add('y_missing', 'dbcivilization', re.sub(rb'\n    mLevels: [^\n]*', b'', civ, count=1))
    add('y_flowspace', 'dbcivilization', civ.replace(b', ', b' ,  ', 20))
    add('y_trailing_comma', 'dbcivilization', civ.replace(b']\n', b',]\n', 1))
    add('y_unclosed', 'dbcivilization', civ.replace(b']\n', b'\n', 1))
    add('y_anchor', 'dbcivilization', _sub(civ, '\nformat: 3', '\nformat: &a 3'))
    add('y_block', 'dbcivilization', _sub(civ, '\nformat: 3', '\nformat: |\n  1'))
    add('y_flowmap', 'dbcivilization', _sub(civ, '\nformat: 3', '\nformat: {a: 3}'))
    add('y_doc', 'dbcivilization', b'---\n' + civ)
    add('y_twodocs', 'dbcivilization', civ + b'---\nx: 1\n')
    add('y_crlf', 'dbcivilization', civ.replace(b'\n', b'\r\n'))
    add('y_norow', 'dbcivilization', re.sub(rb'\n  - row: \d+\n    ', b'\n  - ', civ))
    add('y_tildes', 'dbcivilization', re.sub(rb'(\n    mLevels: )[^\n]*', rb'\1~', civ, count=1))
    add('y_pairs_short', 'dbcivilization', re.sub(rb'(\n    mLevels: )\[\[', rb'\1[[1.5], [', civ, count=1))
    add('y_toomany', 'dbcivilization', re.sub(rb'(\n    mLevels: )\[', rb'\1[' + b'[0, 0], ' * 61, civ,
                                              count=1))
    add('y_mapvalue', 'dbcivilization', re.sub(rb'label: "([^"\\]*)"', rb'label: a: b', civ, count=1))
    add('y_seqitem', 'dbcivilization', _sub(civ, '\nrecords:\n  - ', '\nrecords:\n- '))
    add('y_label99', 'dbcivilization', re.sub(rb'\n    label: "[^"\\]*"', b'\n    label: "' + b'A' * 99 + b'"', civ,
                                               count=1))
    add('y_label100', 'dbcivilization', re.sub(rb'\n    label: "[^"\\]*"', b'\n    label: "' + b'A' * 100 + b'"', civ,
                                                count=1))
    add('y_gap', 'dbcivilization', re.sub(rb'\n  - row: 3\n', b'\n  - row: 40\n', civ, count=1))
    # YAML lists (events), techtree structure
    add('e_ok', 'dbevents', ev)
    add('e_effects', 'dbevents', re.sub(rb'effects: \[\]', b'effects: [1, 2, 0x10]', ev, count=1))
    add('e_effects_none', 'dbevents', re.sub(rb'effects: \[\]', b'effects: [0, 1999, -5]', ev, count=1))
    first = re.search(rb'effects: \[(\w+)', ev).group(1)
    add('e_effects_ids', 'dbevents', re.sub(rb'effects: \[\]', b'effects: [' + first + b', ' + first + b']', ev,
                                            count=1))
    add('e_effects_dangling', 'dbevents', re.sub(rb'effects: \[\]', b'effects: [no_such_effect]', ev, count=1))
    add('e_effects_bad', 'dbevents', re.sub(rb'effects: \[\]', b'effects: [1, "x"]', ev, count=1))
    add('e_nested', 'dbevents', re.sub(rb'effects: \[\]', b'effects: [[1]]', ev, count=1))
    add('e_rows', 'dbevents', _sub(ev, '\nrows: 1500', '\nrows: 1600'))
    ep = texts['epochs']
    add('t_ok', 'dbtechtree', tt)
    add('t_epochs_ok', 'epochs', ep)
    add('t_slots', 'epochs', _sub(ep, 'slots: 929', 'slots: 940'))
    add('t_noslots', 'epochs', _sub(ep, '    slots: 929\n', ''))
    add('t_badkey', 'epochs', _sub(ep, '    slots: 929\n', '    slots: 929\n    colour: 1\n'))
    add('t_epoch_row', 'epochs', _sub(ep, '    slots: 929\n', '    slots: 929\n    row: 3\n'))
    add('t_music', 'epochs', _sub(ep, '    music: []\n', '    music: [pawns, shadows]\n'))
    add('t_never_bad', 'epochs', _sub(ep, 'mObsoleteEpoch: never', 'mObsoleteEpoch: nevermore'))
    add('t_badepoch', 'dbtechtree', _sub(tt, '  - epoch: prehistoric_age\n', '  - epoch: no_such_age\n'))
    first = tt.decode('utf-8').split('\n  - epoch: ')[1]
    node = '  - epoch: stone_age\n' + '\n'.join(ln for ln in first.split('\n')[1:]
                                                if not ln.startswith(('    row:', '    mID:', '    id:')))
    add('t_newnode', 'dbtechtree', tt.rstrip(b'\n') + b'\n' + node.replace('\n    label:', '\n    id: new_tech\n    label:', 1).encode())
    # a 16th epoch in the middle: a copy of the bronze age after the copper age (its techs follow with ids kept)
    s = ep.decode('utf-8')
    a = s.index('  - id: bronze_age\n')
    b = s.index('  - id: dark_age\n')
    add('t_epoch16', 'epochs', (s[:a] + s[a:b].replace('id: bronze_age', 'id: tin_age', 1)
                                .replace('mEpoch: bronze_age', 'mEpoch: tin_age') + s[a:]).encode())
    # ids and references (CSV: dbcivpowers, whose cells are simple)
    pw, us = texts['dbcivpowers'], texts['dbunitset']
    add('r_ok', 'dbcivpowers', pw)
    add('r_other_id', 'dbcivpowers', _cell(pw, 0, 'mUnitSet', _cell_value(us, 2, 'id')))
    add('r_dangling', 'dbcivpowers', _cell(pw, 0, 'mUnitSet', 'no_such_set'))
    add('r_row_number', 'dbcivpowers', _cell(pw, 0, 'mUnitSet', _cell_value(us, 2, 'row')))
    add('r_none', 'dbcivpowers', _cell(pw, 0, 'mUnitSet', '-1'))
    add('r_quoted', 'dbcivpowers', _cell(pw, 0, 'mUnitSet', '"' + _cell_value(us, 2, 'id') + '"'))
    add('r_tech_none', 'dbcivpowers', _cell(pw, 0, 'mTechnology', '100000'))
    add('r_upper', 'dbcivpowers', _cell(pw, 0, 'mUnitType', 'Flaming_Arrow'))
    add('r_dup_id', 'dbcivpowers', _cell(pw, 1, 'id', _cell_value(pw, 0, 'id')))
    add('r_bad_id', 'dbcivpowers', _cell(pw, 1, 'id', '9lives'))
    add('r_self_explicit', 'dbcivpowers', _cell(pw, 1, 'mID', '39'))
    add('r_self_other', 'dbcivpowers', _cell(pw, 1, 'mID', '77'))
    last = _csv_lines(pw)[-2]
    tail = last[last.index(',', last.index(',') + 1):]
    add('r_new_row', 'dbcivpowers', pw + (',new_power' + tail + '\n').encode())
    add('r_new_row_dup', 'dbcivpowers', pw + (',' + _cell_value(pw, 0, 'id') + tail + '\n').encode())
    return out


def _cell_value(text, row, col):
    lines = _csv_lines(text)
    hdr = next(i for i, l in enumerate(lines) if not l.startswith('#'))
    return lines[hdr + 1 + row].split(',')[lines[hdr].split(',').index(col)]


def _cell_first_name(snd, value):
    """dbsounds with the first real row's 'label' cell replaced."""
    lines = _csv_lines(snd)
    hdr = next(i for i, l in enumerate(lines) if not l.startswith('#'))
    cols = lines[hdr].split(',')
    k = hdr + 1
    import csv
    import io
    cells = next(csv.reader([lines[k]]))
    cells[cols.index('label')] = value
    b = io.StringIO()
    csv.writer(b, lineterminator='').writerow(cells)
    lines[k] = b.getvalue()
    return _join(lines)
