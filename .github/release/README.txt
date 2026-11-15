Empire Earth: The Art of Conquest, built from source (empire_earth_next)

This package holds the game program and the part of the game data that belongs to it: Data/db (the tables and
the rules documents) and Data/ai (the computer players' scripts). The media (models, textures, sounds, music; with
Users, WONLobby.cfg and zzWONVersion.cfg) are the repository's open-media release (and, optional, the 4x textures
of open-textures-4x): unzip it into the same folder, so that its Data merges with the Data here (it holds no db or
ai of its own).

  Windows: EE-AOC.exe, with Data beside it.
  Linux:   ee-aoc, with Data beside it (or set EE_DATA_ROOT to the folder that holds Data).
  macOS:   Empire Earth AoC.app: its Contents/Resources holds Data (db, ai); unzip the media there, or put the
           whole data elsewhere and set EE_DATA_ROOT.
           The app is not signed: the first time, right-click it and choose Open (or run
           xattr -dr com.apple.quarantine "Empire Earth AoC.app").

The game keeps its settings in config.yaml beside the program (it creates the file).
See the repository's README for how the data is built.
