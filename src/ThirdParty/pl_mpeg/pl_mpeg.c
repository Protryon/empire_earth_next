// pl_mpeg (MIT, Dominic Szablewski; include/pl_mpeg/pl_mpeg.h, unmodified, commit c871f2be022ece7ef4f64230b4fb8e1fb9eb6023,
// sha256 3a8cb30c83c2a1147719c30fe0c8b93da2987aa43140077c575b39aaa75fc2c9): the implementation unit. MPEG-1 video,
// MP2 audio and the MPEG-PS demuxer for the movies (tools/convert_movies.py turns the retail .bik files into .mpg).
#include <stdio.h>   // pl_mpeg.h names FILE before it includes stdio.h
#define PL_MPEG_IMPLEMENTATION
#include "pl_mpeg/pl_mpeg.h"
