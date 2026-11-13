Empire Earth: The Art of Conquest, built from source (empire_earth_next)

This package holds the game program only. It needs the game data next to it: a "Data" folder (with Users,
WONLobby.cfg and zzWONVersion.cfg) in the same folder as the program.

  Windows: EE-AOC.exe, with Data beside it.
  Linux:   ee-aoc, with Data beside it (or set EE_DATA_ROOT to the data folder).
  macOS:   Empire Earth AoC.app: put the data in the app's Contents/Resources, or set EE_DATA_ROOT.
           The app is not signed: the first time, right-click it and choose Open (or run
           xattr -dr com.apple.quarantine "Empire Earth AoC.app").

The game keeps its settings in config.yaml beside the program (it creates the file).
See the repository's README for how the data is built.
