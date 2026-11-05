r"""The two text containers: a strict YAML subset and CSV with leading directives. The C++ loader
(src/Empire Earth/Database/EEDataText.cpp) implements the same grammar; keep the two in step.

YAML subset ("strict profile", docs/MODDING_PLAN.md 3.3): UTF-8; indentation by spaces; block mappings
(`key: value`, `key:` + an indented block, or a block sequence at the same indent); block sequences (`- value`,
`- key: value` starting a mapping); flow sequences `[a, "b", [c, d]]` on one line; scalars plain, "double quoted"
(escapes \\ \" \/ \0 \a \b \t \n \v \f \r \e \xHH \uHHHH \UHHHHHHHH) or 'single quoted' ('' = '); comments from
` #` or a leading `#`; a leading `---`. Refused: tabs in indentation, anchors, aliases, tags, block scalars, flow
mappings (except `{}`), multi-line scalars, duplicate keys. The data model: dict (ordered), list, Scalar.

CSV: RFC 4180 (comma, "" quoting, quoted fields may span lines), LF or CRLF. Lines before the header that start
with '#' are directives (`#name value`; `# ` starts a comment). The first other line is the header.
"""
import csv
import io
import re


class TextError(Exception):
    pass


class Scalar(str):
    """A scalar's text; .quoted tells a quoted scalar from a plain one ("1" vs 1)."""
    def __new__(cls, text, quoted=False, line=0):
        s = str.__new__(cls, text)
        s.quoted, s.line = quoted, line
        return s


# ---------------------------------------------------------------- YAML emit

PLAIN_KEY = re.compile(r'^[A-Za-z_][A-Za-z0-9_~.\[\]-]*$')


def quote(s):
    out = ['"']
    for c in s:
        o = ord(c)
        if c == '"':
            out.append('\\"')
        elif c == '\\':
            out.append('\\\\')
        elif o < 0x20 or o == 0x7f or 0x80 <= o < 0xa0:
            out.append({'\n': '\\n', '\t': '\\t', '\r': '\\r', '\0': '\\0'}.get(c, f'\\x{o:02x}'))
        else:
            out.append(c)
    out.append('"')
    return ''.join(out)


class Text(str):
    """A value the emitter writes quoted (strings, hex)."""


def emit_scalar(v):
    if isinstance(v, Text):
        return quote(v)
    if isinstance(v, list):
        return '[' + ', '.join(emit_scalar(x) for x in v) + ']'
    return str(v)


def emit_key(k):
    return k if PLAIN_KEY.match(k) else quote(k)


def emit_yaml(node, indent=0, out=None):
    """node: dict / list / scalar (int, str, Text, flow list). Lists of scalars are written as flow sequences;
    lists of dicts as block sequences."""
    top = out is None
    out = [] if top else out
    pad = ' ' * indent
    if isinstance(node, dict):
        for k, v in node.items():
            if isinstance(v, dict) and v:
                out.append(f'{pad}{emit_key(k)}:')
                emit_yaml(v, indent + 2, out)
            elif isinstance(v, list) and v and all(isinstance(x, dict) for x in v):
                out.append(f'{pad}{emit_key(k)}:')
                emit_yaml(v, indent + 2, out)
            elif isinstance(v, dict):
                out.append(f'{pad}{emit_key(k)}: {{}}')
            else:
                out.append(f'{pad}{emit_key(k)}: {emit_scalar(v)}')
    else:
        for item in node:
            sub = []
            emit_yaml(item, indent + 2, sub)
            sub[0] = pad + '- ' + sub[0][indent + 2:]
            out.extend(sub)
    if top:
        return '\n'.join(out) + '\n'
    return out


# ---------------------------------------------------------------- YAML parse

_ESC = {'\\': '\\', '"': '"', '/': '/', '0': '\0', 'a': '\a', 'b': '\b', 't': '\t', 'n': '\n', 'v': '\v',
        'f': '\f', 'r': '\r', 'e': '\x1b', ' ': ' '}


class _Lines:
    def __init__(self, text):
        if text.startswith('﻿'):
            text = text[1:]
        self.items = []    # (indent, content, lineno)
        for no, raw in enumerate(text.split('\n'), 1):
            raw = raw.rstrip('\r')
            body = raw.lstrip(' ')
            if body.startswith('\t'):
                raise TextError(f'line {no}: tab in indentation')
            content = _strip_comment(body, no).rstrip(' \t')
            if not content:
                continue
            if content == '---' and not self.items:
                continue
            if content in ('---', '...'):
                raise TextError(f'line {no}: only one document per file')
            self.items.append((len(raw) - len(body), content, no))


def _strip_comment(s, no):
    if s.startswith('#'):
        return ''
    q = None
    i = 0
    while i < len(s):
        c = s[i]
        if q == '"':
            if c == '\\':
                i += 1
            elif c == '"':
                q = None
        elif q == "'":
            if c == "'":
                q = None
        elif c in '"\'' and (i == 0 or s[i - 1] in ' [,:-'):
            q = c
        elif c == '#' and i > 0 and s[i - 1] in ' \t':
            return s[:i]
        i += 1
    return s


def _parse_quoted(s, i, no):
    """s[i] is a quote -> (text, index after the closing quote)."""
    q = s[i]
    out = []
    i += 1
    while True:
        if i >= len(s):
            raise TextError(f'line {no}: unterminated string')
        c = s[i]
        if q == "'":
            if c == "'":
                if s[i + 1:i + 2] == "'":
                    out.append("'")
                    i += 2
                    continue
                return ''.join(out), i + 1
            out.append(c)
            i += 1
            continue
        if c == '"':
            return ''.join(out), i + 1
        if c == '\\':
            e = s[i + 1:i + 2]
            if e in _ESC:
                out.append(_ESC[e])
                i += 2
            elif e in ('x', 'u', 'U'):
                n = {'x': 2, 'u': 4, 'U': 8}[e]
                h = s[i + 2:i + 2 + n]
                if len(h) != n or any(x not in '0123456789abcdefABCDEF' for x in h):
                    raise TextError(f'line {no}: bad \\{e} escape')
                out.append(chr(int(h, 16)))
                i += 2 + n
            else:
                raise TextError(f'line {no}: unknown escape \\{e}')
            continue
        out.append(c)
        i += 1


def _check_plain(t, no):
    if not t:
        return
    if t[0] in '&*!|>%@`{}':
        raise TextError(f'line {no}: {t[:20]!r}: anchors, aliases, tags, block scalars and flow mappings are '
                        f'not allowed')
    if ': ' in t or t.endswith(':'):
        raise TextError(f'line {no}: {t[:30]!r}: a mapping inside a value (quote the string)')


def _parse_flow(s, i, no):
    """s[i] == '[' -> (list, index after ']')."""
    out = []
    i += 1
    expect_item = True
    while True:
        while i < len(s) and s[i] == ' ':
            i += 1
        if i >= len(s):
            raise TextError(f'line {no}: unterminated [ (flow sequences must close on their line)')
        c = s[i]
        if c == ']':
            if expect_item and out:
                raise TextError(f'line {no}: trailing comma in a flow sequence')
            return out, i + 1
        if not expect_item:
            if c != ',':
                raise TextError(f'line {no}: expected , or ] in a flow sequence')
            i += 1
            expect_item = True
            continue
        if c == '[':
            v, i = _parse_flow(s, i, no)
        elif c in '"\'':
            t, i = _parse_quoted(s, i, no)
            v = Scalar(t, True, no)
        else:
            j = i
            while j < len(s) and s[j] not in ',]':
                j += 1
            t = s[i:j].strip()
            if not t:
                raise TextError(f'line {no}: empty item in a flow sequence')
            if t[0] in '{[':
                raise TextError(f'line {no}: flow mappings are not allowed')
            _check_plain(t, no)
            v = Scalar(t, False, no)
            i = j
        out.append(v)
        expect_item = False


def _parse_value(s, no):
    """An inline value (after 'key: ' or '- ')."""
    if s == '{}':
        return {}
    if s.startswith('['):
        v, i = _parse_flow(s, 0, no)
        if s[i:].strip():
            raise TextError(f'line {no}: text after ]')
        return v
    if s[0] in '"\'':
        t, i = _parse_quoted(s, 0, no)
        if s[i:].strip():
            raise TextError(f'line {no}: text after the closing quote')
        return Scalar(t, True, no)
    _check_plain(s, no)
    return Scalar(s, False, no)


def _split_key(s, no):
    """'key: rest' or 'key:' -> (key, rest) or None when s is not a mapping entry."""
    if s[0] in '"\'':
        k, i = _parse_quoted(s, 0, no)
        if s[i:i + 1] != ':' or (s[i + 1:i + 2] not in ('', ' ')):
            return None
        return k, s[i + 1:].strip()
    if s[0] in '[{-' and not (s[0] == '-' and len(s) > 1 and s[1] not in ' '):
        return None
    m = re.match(r'^([^:#]*?):(?: (.*))?$', s)
    if not m or not m.group(1).strip():
        return None
    return m.group(1).strip(), (m.group(2) or '').strip()


def parse_yaml(text):
    lines = _Lines(text)
    items = lines.items
    if not items:
        return {}
    node, i = _parse_block(items, 0, items[0][0])
    if i != len(items):
        raise TextError(f'line {items[i][2]}: unexpected indentation')
    return node


def _is_seq(content):
    return content == '-' or content.startswith('- ')


def _parse_block(items, i, indent):
    if _is_seq(items[i][1]):
        return _parse_seq(items, i, indent)
    return _parse_map(items, i, indent)


def _parse_map(items, i, indent, first=None):
    out = {}
    while i < len(items):
        ind, content, no = items[i] if first is None else (indent, first, items[i][2])
        if first is None and ind < indent:
            break
        if ind != indent:
            raise TextError(f'line {no}: unexpected indentation')
        if first is None and _is_seq(content):
            break
        first = None
        kv = _split_key(content, no)
        if kv is None:
            raise TextError(f'line {no}: expected "key: value"')
        k, rest = kv
        if k in out:
            raise TextError(f'line {no}: duplicate key {k!r}')
        i += 1
        if rest:
            out[k] = _parse_value(rest, no)
        elif i < len(items) and (items[i][0] > indent or (items[i][0] == indent and _is_seq(items[i][1]))):
            out[k], i = _parse_block(items, i, items[i][0])
        else:
            out[k] = Scalar('', False, no)
    return out, i


def _parse_seq(items, i, indent):
    out = []
    while i < len(items):
        ind, content, no = items[i]
        if ind < indent or (ind == indent and not _is_seq(content)):
            break
        if ind != indent:
            raise TextError(f'line {no}: unexpected indentation')
        rest = content[1:].lstrip(' ')
        if not rest:
            i += 1
            if i < len(items) and items[i][0] > indent:
                v, i = _parse_block(items, i, items[i][0])
            else:
                v = Scalar('', False, no)
            out.append(v)
            continue
        sub = indent + (len(content) - len(rest))
        if _is_seq(rest):
            raise TextError(f'line {no}: nested "- -" sequences are not allowed')
        if _split_key(rest, no) is not None:
            v, i = _parse_map(items, i, sub, first=rest)
            out.append(v)
            continue
        out.append(_parse_value(rest, no))
        i += 1
    return out, i


# ---------------------------------------------------------------- CSV

def emit_csv(directives, header, rows):
    """directives: [(name, value)]; rows: lists of str."""
    buf = io.StringIO()
    for k, v in directives:
        buf.write(f'#{k} {v}\n')
    w = csv.writer(buf, lineterminator='\n')
    w.writerow(header)
    for r in rows:
        w.writerow(r)
    return buf.getvalue()


def parse_csv(text):
    """-> (directives {name: value}, header [str], rows [[str]], the line number each row starts on)."""
    if text.startswith('﻿'):
        text = text[1:]
    directives = {}
    lines = text.split('\n')
    k = 0
    while k < len(lines) and (lines[k].startswith('#') or not lines[k].strip()):
        s = lines[k].rstrip('\r')
        if s.startswith('#') and len(s) > 1 and s[1] not in ' #':
            name, _, val = s[1:].partition(' ')
            if name in directives:
                raise TextError(f'line {k + 1}: directive #{name} twice')
            directives[name] = val.strip()
        k += 1
    rd = csv.reader(io.StringIO('\n'.join(lines[k:])), strict=True)
    rows, starts = [], []
    try:
        prev = 0
        for r in rd:
            rows.append(r)
            starts.append(k + prev + 1)
            prev = rd.line_num
    except csv.Error as e:
        raise TextError(f'CSV: {e}') from None
    while rows and not rows[-1]:
        rows.pop()
    if not rows:
        raise TextError('CSV: no header row')
    header = rows[0]
    if len(set(header)) != len(header):
        raise TextError('CSV: duplicate column names')
    body, body_lines = [], []
    for n, r in enumerate(rows[1:], 1):
        if not r:
            continue
        if len(r) != len(header):
            raise TextError(f'line {starts[n]}: {len(r)} cells, the header has {len(header)}')
        body.append(r)
        body_lines.append(starts[n])
    return directives, header, body, body_lines
