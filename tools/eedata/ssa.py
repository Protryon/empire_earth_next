"""Read an .ssa archive (FSArchive, docs/DATA_FORMATS.md 2.2) and its PK01 entries (2.3)."""
import struct

import blast


class SSAError(Exception):
    pass


def read_ssa(path):
    """-> (entries {name: (start, length)} in FAT order, tags [(name, value)], archive bytes)."""
    d = open(path, 'rb').read()
    if len(d) < 16:
        raise SSAError(f'{path}: too short')
    ident, ver, _res, fat = struct.unpack_from('<4I', d, 0)
    if ident != 0x73736172 or ver != 1:
        raise SSAError(f'{path}: not an ssa archive (identifier {ident:#x}, version {ver})')
    p, used, entries = 16, 4, {}
    while used < fat:
        n, = struct.unpack_from('<I', d, p)
        name = d[p + 4:p + 4 + n].split(b'\0')[0].decode('cp1252')
        start, _end, length = struct.unpack_from('<3i', d, p + 4 + n)
        entries[name] = (start, length)
        p += 4 + n + 12
        used += 4 + n + 12
    ntags, = struct.unpack_from('<I', d, p)
    p += 4
    tags = []
    for _ in range(ntags):
        pair = []
        for _ in range(2):
            n, = struct.unpack_from('<I', d, p)
            pair.append(d[p + 4:p + 4 + n].split(b'\0')[0].decode('cp1252'))
            p += 4 + n
        tags.append(tuple(pair))
    return entries, tags, d


def entry_bytes(d, start, length):
    """An entry's contents, inflated when it is a PK01 block."""
    blk = d[start:start + length]
    if length > 12 and blk[:4] == b'PK01':
        size, = struct.unpack_from('<I', blk, 4)
        out = blast.explode(blk[12:])
        if len(out) != size:
            raise SSAError(f'PK01 entry inflates to {len(out)} bytes, the header says {size}')
        return out
    return blk
