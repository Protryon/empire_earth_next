// New (port): the world's objects as Rust reads them in place (docs/progress/world-views.md; ee_sim world). The
// records stay inline in the C++ objects; Rust's typed views (ee_sim::world::view) read them through the layout
// EEWorldLayout.cpp registers once (the record bases and member offsets, as the compiler places them).
//
// TEST ONLY: EEWorldViewCheck digests a fixed list of every player's, unit's and tile's fields and the world's through
// the C++ members and through Rust's views (the simulation capture's VWCK record; a difference stops EE-SIM).
#ifndef EE_EEWORLDLAYOUT_H
#define EE_EEWORLDLAYOUT_H

// the build's layout to Rust (once; the world's constructor calls it): aborts when a check of the layout fails
void EEWorldLayoutRegister();

// TEST ONLY: the VWCK record of this tick (a no-op unless the capture runs and keeps VWCK)
void EEWorldViewCheck();

#endif
