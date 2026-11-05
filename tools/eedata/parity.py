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
    """texts: {table: exact text bytes} -> [(name, table, text bytes)]."""
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
    add('w_badformat', 'dbworld', _sub(w, '#format 1', '#format 2'))
    add('w_wrongtable', 'dbworld', _sub(w, '#eedata dbworld', '#eedata dbfamily'))
    add('w_append', 'dbworld', w + b',,1.25,0.0,100,95,7,0,0\n')
    add('w_append_row', 'dbworld', w + b'99,,1.25,0.0,100,95,7,0,0\n')
    add('w_rowsbig', 'dbworld', _sub(w, '#rows 95', '#rows 97'))
    add('w_gap_nohole', 'dbworld', w + b'120,,1,0,0,0,0,0,0\n')
    add('w_order', 'dbworld', _join(lines[:6] + [lines[7], lines[6]] + lines[8:]))
    add('w_label_ok', 'dbworld', _cell(w, 22, 'label', 'miningLimit'))
    add('w_label_bad', 'dbworld', _cell(w, 22, 'label', 'notMiningLimit'))
    add('w_label_free', 'dbworld', _cell(w, 3, 'label', 'anything'))
    add('w_cells', 'dbworld', w + b'1,2\n')
    add('w_unknown_col', 'dbworld', _sub(w, 'row,label,', 'row,label,colour,').replace(b'\n0,', b'\n0,x,', 1))
    add('w_dropcol', 'dbworld', _join([','.join(c for i, c in enumerate(l.split(',')) if i != 8)
                                       if not l.startswith('#') and l else l for l in lines]))
    add('w_quoted', 'dbworld', _cell(w, 3, 'mLongValue', '"12"'))
    add('w_badquote', 'dbworld', _cell(w, 3, 'mLongValue', '1"2'))
    add('w_blanklines', 'dbworld', w.replace(b'\n3,', b'\n\n3,'))
    add('w_empty', 'dbworld', b'')
    # cpbehavior: a negative slot index, an empty hex pad
    add('cp_neg', 'dbcpbehavior', _cell(cp, 0, 'mIndex', '-7'))
    # strings, tails, hex (CSV with lead strings)
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
    add('y_single', 'dbcivilization', re.sub(rb'name: "([^"\\]*)"', rb"name: '\1'", civ, count=3))
    add('y_plain', 'dbcivilization', re.sub(rb'name: "([A-Za-z ]+)"', rb'name: \1', civ, count=3))
    add('y_escape', 'dbcivilization', re.sub(rb'name: "([^"\\]*)"', rb'name: "\\x41\\u00e9\\t\1"', civ, count=1))
    add('y_badescape', 'dbcivilization', re.sub(rb'name: "([^"\\]*)"', rb'name: "\\q\1"', civ, count=1))
    add('y_tab', 'dbcivilization', civ.replace(b'\n    ', b'\n\t', 1))
    add('y_indent', 'dbcivilization', civ.replace(b'\n    ', b'\n     ', 1))
    add('y_dupkey', 'dbcivilization', _sub(civ, '\nformat: 1', '\nformat: 1\nformat: 1'))
    add('y_unknownkey', 'dbcivilization', _sub(civ, '    name:', '    colour: 3\n    name:'))
    add('y_toplevel', 'dbcivilization', _sub(civ, '\nformat: 1', '\nformat: 1\nauthor: me'))
    add('y_missing', 'dbcivilization', re.sub(rb'\n    mLevels: [^\n]*', b'', civ, count=1))
    add('y_flowspace', 'dbcivilization', civ.replace(b', ', b' ,  ', 20))
    add('y_trailing_comma', 'dbcivilization', civ.replace(b']\n', b',]\n', 1))
    add('y_unclosed', 'dbcivilization', civ.replace(b']\n', b'\n', 1))
    add('y_anchor', 'dbcivilization', _sub(civ, '\nformat: 1', '\nformat: &a 1'))
    add('y_block', 'dbcivilization', _sub(civ, '\nformat: 1', '\nformat: |\n  1'))
    add('y_flowmap', 'dbcivilization', _sub(civ, '\nformat: 1', '\nformat: {a: 1}'))
    add('y_doc', 'dbcivilization', b'---\n' + civ)
    add('y_twodocs', 'dbcivilization', civ + b'---\nx: 1\n')
    add('y_crlf', 'dbcivilization', civ.replace(b'\n', b'\r\n'))
    add('y_norow', 'dbcivilization', re.sub(rb'\n  - row: \d+\n    ', b'\n  - ', civ))
    add('y_tildes', 'dbcivilization', re.sub(rb'(\n    mLevels: )[^\n]*', rb'\1~', civ, count=1))
    add('y_pairs_short', 'dbcivilization', re.sub(rb'(\n    mLevels: )\[\[', rb'\1[[1.5], [', civ, count=1))
    add('y_toomany', 'dbcivilization', re.sub(rb'(\n    mLevels: )\[', rb'\1[' + b'[0, 0], ' * 61, civ,
                                              count=1))
    add('y_mapvalue', 'dbcivilization', re.sub(rb'name: "([^"\\]*)"', rb'name: a: b', civ, count=1))
    add('y_seqitem', 'dbcivilization', _sub(civ, '\nrecords:\n  - ', '\nrecords:\n- '))
    add('y_raw', 'dbcivilization', re.sub(rb'\n    name: "[^"\\]*"', b'\n    name~raw: "4142"', civ, count=1))
    add('y_raw_and_text', 'dbcivilization', re.sub(rb'(\n    name: "[^"\\]*")', rb'\1\n    name~raw: "41"', civ,
                                                    count=1))
    add('y_raw_long', 'dbcivilization', re.sub(rb'\n    name: "[^"\\]*"', b'\n    name~raw: "' + b'41' * 101 + b'"',
                                                civ, count=1))
    add('y_nohole', 'dbcivilization', re.sub(rb'\nhole: "[0-9a-f]*"', b'', civ))
    # YAML lists (events), techtree structure
    add('e_ok', 'dbevents', ev)
    add('e_effects', 'dbevents', re.sub(rb'effects: \[\]', b'effects: [1, 2, 0x10]', ev, count=1))
    add('e_effects_bad', 'dbevents', re.sub(rb'effects: \[\]', b'effects: [1, "x"]', ev, count=1))
    add('e_nested', 'dbevents', re.sub(rb'effects: \[\]', b'effects: [[1]]', ev, count=1))
    add('e_rows', 'dbevents', _sub(ev, '\nrows: 1500', '\nrows: 1600'))
    add('t_ok', 'dbtechtree', tt)
    add('t_slots', 'dbtechtree', _sub(tt, 'slots: 929', 'slots: 940'))
    add('t_noslots', 'dbtechtree', _sub(tt, '  - slots: 929\n    epoch:', '  - epoch:'))
    add('t_badkey', 'dbtechtree', _sub(tt, '  - slots: 929\n', '  - slots: 929\n    colour: 1\n'))
    return out


def _cell_first_name(snd, value):
    """dbsounds with the first real row's 'name' cell replaced."""
    lines = _csv_lines(snd)
    hdr = next(i for i, l in enumerate(lines) if not l.startswith('#'))
    cols = lines[hdr].split(',')
    k = hdr + 1
    import csv
    import io
    cells = next(csv.reader([lines[k]]))
    cells[cols.index('name')] = value
    b = io.StringIO()
    csv.writer(b, lineterminator='').writerow(cells)
    lines[k] = b.getvalue()
    return _join(lines)
