// New (port), TEST ONLY: RFIL, the render fill's capture (ee/EERenderCapture.h; tools/rfil.py). Its fill is gamecpp
// ro_views.rs' rc_fill, which EERenderObject::FillRenderLists (gamecpp render_object.rs) calls on the graphics thread
// under the world's read lock: the record is ee_gfx rfil.rs', over the render lists' view (EERenderListsView,
// EEROUnits.cpp) and the world's key. This unit has no code left.
