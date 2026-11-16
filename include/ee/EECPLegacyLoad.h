// New (port): the readers that step over the deleted C++ classic AI's manager blocks of a computer player in a retail
// file (save versions 0.229 to 0.250; src/Empire Earth/AI Thread/EECPLegacyLoad.cpp). Requires lle/FSFile.h,
// lle/U2DPoint.h.
#ifndef EE_EECPLEGACYLOAD_H
#define EE_EECPLEGACYLOAD_H

class FSFile;
template <class T> class U2DPoint;

// the last save version whose computer players hold the C++ managers' blocks in the original's layout
const unsigned long kEECPLegacyLastVersion = 250;

long EECPSkipLegacyCombatManager(FSFile& inFile, unsigned long inMajor, unsigned long inMinor);
// the civilization manager and its resource, build, research, unit and wall managers; outTownCenterTile gets the
// build manager's town centre tile
long EECPSkipLegacyCivilizationManager(FSFile& inFile, unsigned long inMajor, unsigned long inMinor,
                                       U2DPoint<long>& outTownCenterTile);
long EECPSkipLegacyExplorationManager(FSFile& inFile, unsigned long inMajor, unsigned long inMinor);

#endif
