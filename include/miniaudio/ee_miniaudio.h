// The game's miniaudio configuration (miniaudio 0.11.21, public domain or MIT-0; include/miniaudio/miniaudio.h,
// unmodified, sha256 6b2029714f8634c4d7c70cc042f45074e0565766113fc064f20cd27c986be9c9). Every includer and the
// implementation unit (src/ThirdParty/miniaudio/miniaudio.c) see the same options: the playback device layer and the
// decoders (WAV, MP3, FLAC) only; the game mixes its own channels. The Windows backends (WASAPI, DirectSound, waveOut)
// take their system DLLs at run time, as miniaudio does by default (with MA_NO_RUNTIME_LINKING its context fails to
// initialise, even for the null backend); the null backend comes last so the game runs without an audio device.
#ifndef EE_MINIAUDIO_H
#define EE_MINIAUDIO_H

#define MA_NO_ENCODING
#define MA_NO_GENERATION
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_NODE_GRAPH
#define MA_NO_ENGINE
#define MA_ENABLE_ONLY_SPECIFIC_BACKENDS
#define MA_ENABLE_WASAPI
#define MA_ENABLE_DSOUND
#define MA_ENABLE_WINMM
#define MA_ENABLE_NULL

#include "miniaudio/miniaudio.h"

#endif
