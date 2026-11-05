"""Where the game finds its models and textures (EEShowSinglePlayerScreen's data path, FSPath::FindFile): the loose
folders first (this install's Data, then the original game's Data), then the archives (this install's data.ssa,
then the original game's). Names are the archive's lower-case relative paths ("models\\air_a10_10.cem")."""
import os
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.append(str(HERE.parent / 'eedata'))
import ssa   # noqa: E402

AOC = pathlib.Path(r'C:\Program Files (x86)\Empire Earth\Empire Earth - The Art of Conquest')
BASE = pathlib.Path(r'C:\Program Files (x86)\Empire Earth\Empire Earth')


def game_dirs(game=None, base=None):
    g = pathlib.Path(game or os.environ.get('EE_GAME_DIR') or AOC)
    b = pathlib.Path(base or os.environ.get('EE_BASE_GAME_DIR') or BASE)
    return [d for d in (g, b) if (d / 'Data').is_dir()]


class Source:
    """One place on the path: a loose Data folder or a data.ssa archive."""

    def __init__(self, label, folder=None, archive=None):
        self.label = label
        self.folder = folder
        self.entries = None
        if archive is not None:
            self.entries, _, self._data = ssa.read_ssa(archive)

    def names(self, prefix):
        """Lower-case names under a prefix ("models\\")."""
        if self.entries is not None:
            return sorted(n for n in self.entries if n.startswith(prefix))
        sub = self.folder / prefix.rstrip('\\')
        if not sub.is_dir():
            # the install's folders are capitalised ("Models"); Windows looks them up without case
            return []
        return sorted(prefix + p.name.lower() for p in sub.iterdir() if p.is_file())

    def read(self, name):
        if self.entries is not None:
            s, n = self.entries[name]
            return ssa.entry_bytes(self._data, s, n)
        return (self.folder / name.replace('\\', os.sep)).read_bytes()

    def has(self, name):
        if self.entries is not None:
            return name in self.entries
        return (self.folder / name.replace('\\', os.sep)).is_file()


def sources(game=None, base=None):
    """The data path in the game's search order."""
    dirs = game_dirs(game, base)
    out = [Source(f'{d.name}\\Data', folder=d / 'Data') for d in dirs]
    for d in dirs:
        a = d / 'Data' / 'data.ssa'
        if a.is_file():
            out.append(Source(f'{d.name}\\Data\\data.ssa', archive=a))
    return out


def resolve(srcs, name):
    """The source the game reads a name from, or None."""
    for s in srcs:
        if s.has(name):
            return s
    return None
