//! The records C++ classes derive from or embed (the data records pass, docs/progress/headers.md "How to add a
//! record"). Their C declarations, and those of the types they name, go to `include/rust/won_lobby/records.h`
//! (xtask split.rs), a header that declares no function, so a class header that includes only it is not rebuilt
//! by a change to a function. A record is listed here by a `pub use` of its definition, or defined here.

pub use crate::modelobj::EeWonModelCursor;
