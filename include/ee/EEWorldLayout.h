// New (port): the world's objects as Rust reads them in place (docs/progress/world-views.md; ee_sim world). The
// records stay inline in the C++ objects; Rust's typed views (ee_sim::world::view) read them through the layout
// EEWorldLayout.cpp registers once (the record bases and member offsets, as the compiler places them).
#ifndef EE_EEWORLDLAYOUT_H
#define EE_EEWORLDLAYOUT_H

// the build's layout to Rust (once; the world's constructor calls it): aborts when a check of the layout fails
void EEWorldLayoutRegister();

#endif
