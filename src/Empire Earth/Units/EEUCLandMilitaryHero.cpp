// C:\EEX\Empire Earth\Units\EEUCLandMilitaryHero.cpp (file name inferred: no __FILE__; the class name).
// The military
// hero's unit class: 0x216d4b..0x216d57 (after EEUCLandCitizen.cpp; EEUCLandPriest.cpp follows). One unit per EEUC*
// class in alphabetical order. Unit flags: the exe's module set. Its ctor is inline (EEUnitClass.cpp's factory); its
// only out-of-line slot is VirtualB8 (the others are EEUCLand's by ICF).
#include <windows.h>
#include <stdio.h>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"

#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEUCLand.h"
#include "ee/EEUCLandMilitaryHero.h"

