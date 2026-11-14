// The Graphics Engine's text in Rust (rust/crates/ee_ge font.rs, docs/RUST_PLAN.md R4.3): GEText's extent and
// GEFontBitmap's cell layout, alpha pass and draw plan. The platform's text functions reach the
// Rust through this table (ee_ge keeps no platform dependency).
#ifndef GERUSTTEXT_H
#define GERUSTTEXT_H

#include <stddef.h>

#include "rust/ee_ge.h"
#include "platform/PlatFont.h"
#include "platform/PlatString.h"

static int32_t GERustTextMeasure(const void* inFont, const char* inText, uint32_t inLength)
{
    return PlatFontMeasure(static_cast<const PlatFont*>(inFont), inText, inLength);
}

// PlatUtf8Decode, PlatMbsNInc, PlatCp1252ByteOf and PlatFontMeasure for ee_ge
static const EeGeTextOps* GERustTextOps()
{
    static const EeGeTextOps sOps = {PlatUtf8Decode, PlatMbsNInc, PlatCp1252ByteOf, GERustTextMeasure};
    return &sOps;
}

#endif
