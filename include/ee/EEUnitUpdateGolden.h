// TEST ONLY (round 13's golden, not committed): the original class Update bodies' leaf calls noted (USST kind 12)
#ifndef EE_EEUNITUPDATEGOLDEN_H
#define EE_EEUNITUPDATEGOLDEN_H
#include <string.h>
#include <stdint.h>
#include "ee/EEUnitStore.h"
#include "rust/ee_units.h"
static inline int64_t EEUNBits(float inValue) { uint32_t b; memcpy(&b, &inValue, 4); return (int64_t)b; }
#define EEUN(op) EEUnitUpdateNote(EE_UNITS_UPD_##op, 0, 0)
#define EEUNA(op, a, b) EEUnitUpdateNote(EE_UNITS_UPD_##op, (int64_t)(a), (int64_t)(b))
#define EEACN(op) EEUnitActionNote(EE_UNITS_ACT_##op, 0, 0)
#define EEACNA(op, a, b) EEUnitActionNote(EE_UNITS_ACT_##op, (int64_t)(a), (int64_t)(b))
#endif
