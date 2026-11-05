r"""Text forms of the scalar values, shared by the CSV and YAML sides (and mirrored by the C++ loader).

* integers: decimal by the field's signedness; input also takes 0x<hex> as the bit pattern of the field's width.
* floats (f32): the shortest decimal that parses back (correctly rounded, as std::from_chars) to the same 32 bits;
  .inf / -.inf / .nan (0x7fc00000); any other NaN as 0x<8 hex digits> (the bit pattern).
* bool: the raw byte (0, 1, or e.g. 204 for the 0xCC fill); input also takes true/false.
* strings: CP-1252 bytes as Unicode text; the five bytes CP-1252 leaves undefined (0x81 0x8d 0x8f 0x90 0x9d) map to
  the C1 code points of the same value, so every byte string has a text form.
* raw bytes (raw, pad, string tails): lower-case hex, empty = all zero (trailing zero bytes may be left out).
"""
import decimal
import re
import struct
from fractions import Fraction


class ValueError_(ValueError):
    pass


_RANGES = {'s8': (-128, 127), 'u8': (0, 255), 'bool': (0, 255), 's16': (-32768, 32767), 'u16': (0, 65535),
           's32': (-2 ** 31, 2 ** 31 - 1), 'u32': (0, 2 ** 32 - 1)}
_HEX = re.compile(r'^0[xX][0-9a-fA-F]+$')
_DECINT = re.compile(r'^[+-]?[0-9]+$')
_FLT = re.compile(r'^[+-]?([0-9]+\.?[0-9]*|\.[0-9]+)([eE][+-]?[0-9]+)?$')
_HEXBYTES = re.compile(r'^([0-9a-fA-F]{2})*$')
_WIDTH = {'s8': 8, 'u8': 8, 'bool': 8, 's16': 16, 'u16': 16, 's32': 32, 'u32': 32}


def int_range(t):
    return _RANGES[t]


def parse_int(text, t):
    s = text.strip()
    lo, hi = _RANGES[t]
    if t == 'bool' and s in ('true', 'false'):
        return int(s == 'true')
    try:
        if _HEX.match(s):
            v = int(s[2:], 16)
            if v >= 1 << _WIDTH[t]:
                raise ValueError_(f'{text!r} does not fit {t}')
            if lo < 0 and v > hi:
                v -= 1 << _WIDTH[t]
            return v
        if not _DECINT.match(s):
            raise ValueError_(f'{text!r} is not an integer')
        v = int(s)
    except ValueError as e:
        raise ValueError_(str(e)) from None
    if not lo <= v <= hi:
        raise ValueError_(f'{text!r} out of range for {t}')
    return v


def format_int(v):
    return str(v)


# ---- float32 ----

def _round_f32(q):
    """A non-negative Fraction -> float32 bits, round to nearest, ties to even."""
    if q == 0:
        return 0
    e = q.numerator.bit_length() - q.denominator.bit_length()
    if Fraction(2) ** e > q:
        e -= 1
    # now 2^e <= q < 2^(e+1)
    if e < -126:            # subnormal range: units of 2^-149
        m = q / Fraction(2) ** -149
        exp_field = 0
    else:
        m = q / Fraction(2) ** (e - 23)
        exp_field = e + 127
    n = m.numerator // m.denominator
    rem = m - n
    if rem > Fraction(1, 2) or (rem == Fraction(1, 2) and n & 1):
        n += 1
    if exp_field == 0:
        return n                        # n == 2^23 rolls into the smallest normal by itself
    if n == 1 << 24:
        n >>= 1
        exp_field += 1
    if exp_field >= 255:
        return 0x7f800000
    return (exp_field << 23) | (n & 0x7fffff)


def parse_f32(text):
    s = text.strip()
    low = s.lower()
    if low in ('.inf', '+.inf', 'inf', '+inf', 'infinity'):
        return 0x7f800000
    if low in ('-.inf', '-inf', '-infinity'):
        return 0xff800000
    if low in ('.nan', 'nan'):
        return 0x7fc00000
    if _HEX.match(s):
        v = int(s[2:], 16)
        if v >= 1 << 32:
            raise ValueError_(f'{text!r}: more than 32 bits')
        return v
    if not _FLT.match(s):
        raise ValueError_(f'{text!r} is not a number')
    neg = s.startswith('-')
    q = Fraction(s[1:] if s[:1] in '+-' else s)
    bits = _round_f32(q)
    if bits == 0x7f800000 or (bits == 0 and q != 0):
        raise ValueError_(f'{text!r} is outside the float range')
    return bits | (0x80000000 if neg else 0)


def _decimal_bits(s):
    """A decimal (as format_f32 writes them) -> float32 bits, overflow to inf, no checks."""
    neg = s.startswith('-')
    return _round_f32(Fraction(s.lstrip('+-'))) | (0x80000000 if neg else 0)


def format_f32(bits):
    if bits & 0x7f800000 == 0x7f800000:
        if bits & 0x7fffff == 0:
            return '-.inf' if bits >> 31 else '.inf'
        return '.nan' if bits == 0x7fc00000 else f'0x{bits:08x}'
    f, = struct.unpack('<f', struct.pack('<I', bits))
    for digits in range(1, 10):
        s = f'{f:.{digits - 1}e}'
        if _decimal_bits(s) == bits:
            break
    else:
        raise AssertionError(bits)
    d = decimal.Decimal(s)
    exp = d.adjusted()
    if -5 <= exp < 10:
        out = format(d, 'f')
        if '.' not in out:
            out += '.0'
    else:
        mant, e = s.split('e')
        mant = mant.rstrip('0').rstrip('.') if '.' in mant else mant
        out = f'{mant}e{int(e)}'
    if parse_f32(out) != bits:
        raise AssertionError((bits, out))
    return out


# ---- strings ----

_DEC = []
for _b in range(256):
    try:
        _DEC.append(bytes([_b]).decode('cp1252'))
    except UnicodeDecodeError:
        _DEC.append(chr(_b))
_ENC = {c: i for i, c in enumerate(_DEC)}


def decode_str(b):
    return ''.join(_DEC[x] for x in b)


def encode_str(s):
    try:
        return bytes(_ENC[c] for c in s)
    except KeyError as e:
        raise ValueError_(f'{s!r}: character {e.args[0]!r} has no CP-1252 byte') from None


# ---- bytes ----

def format_hex(b, trim=True):
    return (b.rstrip(b'\0') if trim else b).hex()


def parse_hex(text, n=None):
    s = text.strip()
    if not _HEXBYTES.match(s):
        raise ValueError_(f'{text[:40]!r} is not hex (pairs of hex digits)')
    b = bytes.fromhex(s)
    if n is not None:
        if len(b) > n:
            raise ValueError_(f'{len(b)} bytes of hex, the field has {n}')
        b += bytes(n - len(b))
    return b
