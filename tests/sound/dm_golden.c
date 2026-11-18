// The golden driver of the music engine (rust/crates/ee_audio/src/dmusic): what dmusic (src/ThirdParty/dmusic at
// origin/rust 0bfe280ce, with its EE patches) did, before it was replaced by the Rust port. It builds over dmusic's
// own sources with the synthesizer (src/Synth.c, util/Tsf.c, TinySoundFont) replaced by a logger, so every line is
// a call the performance makes into its synthesizer, at the frame it lands on: the notes, the controls, the bands
// and their instruments, the resets. The reverb is printed separately (its output over a test signal, per block).
//
// Built as the game built dmusic (clang-cl -O1 /MT, the UCRT's powf and sinf; mingw's differ in the last bit of a
// few curve values), each source to its own object (src/X.c and src/io/X.c share names), and with one change to
// Performance.c: DmPerformance_handleSegmentMessage's default command (`DmMessage cmd`) zeroed with memset, since
// dmusic left its groove range uninitialized (stack garbage drew an extra random number in the test songs' loop;
// the game's songs never reach it). The port reads zero there.
//
//   dm_golden <music folder> <general MIDI set: EE.sf2> <scenario>
//     scenario: SONG.sgt (one song, 213 s, groove levels 0, 2, 5, 0, a stop), songs (the game's eight in turn:
//     transitions with each embellishment, stops, instant starts), songs=a.sgt,b.sgt,... or reverb
//
// The random numbers are an LCG (the Rust port's `Rng`), the renders cycle through block sizes 480, 512, 128 and
// 1031 frames at 48 kHz. golden_dmusic.txt is this over the test songs (tests/sound/dm_testsong.py).
#include "_Internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint64_t gFrame;   // frames rendered so far
static uint32_t gRng = 1;

static uint32_t Rng(void* ctx) {
	(void) ctx;
	gRng = gRng * 1103515245u + 12345u;
	return (gRng >> 16) & 0x7fff;
}

static uint32_t bits(float f) {
	uint32_t u;
	memcpy(&u, &f, 4);
	return u;
}

// ---- the synthesizer, logged
void DmSynth_init(DmSynth* slf, uint32_t rate) {
	memset(slf, 0, sizeof *slf);
	slf->rate = rate;
	slf->volume = 1;
	printf("%llu init %u\n", (unsigned long long) gFrame, rate);
}
void DmSynth_free(DmSynth* slf) {
	(void) slf;
}
void DmSynth_reset(DmSynth* slf) {
	(void) slf;
	printf("%llu reset\n", (unsigned long long) gFrame);
}
void DmSynth_setVolume(DmSynth* slf, float vol) {
	slf->volume = vol;
	printf("%llu volume %08x\n", (unsigned long long) gFrame, bits(vol));
}
DmResult DmSynth_createTsfForDls(DmDls* dls, tsf** out) {
	(void) dls;
	*out = (tsf*) 1;
	return DmResult_SUCCESS;
}
void tsf_close(tsf* f) {
	(void) f;
}
static char const* stem(DmInstrument* ins) {
	return ins->reference.file != NULL ? ins->reference.file : "-";
}
static void band(char const* what, DmBand* band) {
	printf("%llu %s %zu", (unsigned long long) gFrame, what, band->instruments_len);
	for (size_t i = 0; i < band->instruments_len; ++i) {
		DmInstrument* ins = &band->instruments[i];
		printf(" [%u %08x %x %u %u %d %s %d]",
		       ins->channel,
		       ins->patch,
		       (unsigned) ins->options,
		       ins->pan,
		       ins->volume,
		       ins->transpose,
		       stem(ins),
		       ins->dls != NULL);
	}
	printf("\n");
}
void DmSynth_sendBandUpdate(DmSynth* slf, DmBand* b) {
	(void) slf;
	band("band", b);
}
DmResult DmSynth_prepareFonts(DmSynth* slf, DmBand* b) {
	(void) slf;
	band("fonts", b);
	return DmResult_SUCCESS;
}
void DmSynth_sendControl(DmSynth* slf, uint32_t channel, uint8_t control, float value) {
	(void) slf;
	printf("%llu cc %u %u %08x\n", (unsigned long long) gFrame, channel, control, bits(value));
}
void DmSynth_sendControlReset(DmSynth* slf, uint32_t channel, uint8_t control, float reset) {
	(void) slf;
	printf("%llu ccreset %u %u %08x\n", (unsigned long long) gFrame, channel, control, bits(reset));
}
void DmSynth_sendPitchBend(DmSynth* slf, uint32_t channel, int bend) {
	(void) slf;
	printf("%llu bend %u %d\n", (unsigned long long) gFrame, channel, bend);
}
void DmSynth_sendPitchBendReset(DmSynth* slf, uint32_t channel, int reset) {
	(void) slf;
	printf("%llu bendreset %u %d\n", (unsigned long long) gFrame, channel, reset);
}
void DmSynth_sendNoteOn(DmSynth* slf, uint32_t channel, uint8_t note, uint8_t velocity) {
	(void) slf;
	printf("%llu on %u %u %u\n", (unsigned long long) gFrame, channel, note, velocity);
}
void DmSynth_sendNoteOff(DmSynth* slf, uint32_t channel, uint8_t note) {
	(void) slf;
	printf("%llu off %u %u\n", (unsigned long long) gFrame, channel, note);
}
void DmSynth_sendNoteOffAll(DmSynth* slf, uint32_t channel) {
	(void) slf;
	printf("%llu offall %u\n", (unsigned long long) gFrame, channel);
}
void DmSynth_sendNoteOffEverything(DmSynth* slf) {
	(void) slf;
	printf("%llu offeverything\n", (unsigned long long) gFrame);
}
size_t DmSynth_render(DmSynth* slf, void* buf, size_t len, DmRenderOptions fmt) {
	(void) slf;
	size_t bytes = fmt & DmRender_FLOAT ? len * 4 : len * 2;
	memset(buf, 0, bytes);
	gFrame += (fmt & DmRender_STEREO) ? len / 2 : len;
	return bytes;
}

// ---- the files
static char gFolder[1024];
static char gGeneralMidi[1024];

static void* Resolve(void* ctx, char const* name, size_t* length) {
	(void) ctx;
	char path[2048];
	snprintf(path, sizeof path, "%s/%s", gFolder, name);
	FILE* f = fopen(path, "rb");
	if (f == NULL && _stricmp(name, "gm.dls") == 0) {
		f = fopen(gGeneralMidi, "rb");
	}
	if (f == NULL) {
		printf("%llu resolve %s missing\n", (unsigned long long) gFrame, name);
		return NULL;
	}
	fseek(f, 0, SEEK_END);
	long n = ftell(f);
	fseek(f, 0, SEEK_SET);
	void* data = malloc((size_t) n);
	fread(data, 1, (size_t) n, f);
	fclose(f);
	*length = (size_t) n;
	printf("%llu resolve %s %ld\n", (unsigned long long) gFrame, name, n);
	return data;
}

// ---- the scenario
static DmPerformance* gPerformance;
static unsigned gBlock;

static void render_seconds(double seconds) {
	static const size_t kBlocks[4] = {480, 512, 128, 1031};
	static float buf[2 * 1031];
	uint64_t end = gFrame + (uint64_t) (seconds * 48000.0);
	while (gFrame < end) {
		size_t n = kBlocks[gBlock++ % 4];
		DmPerformance_renderPcm(gPerformance, buf, 2 * n, DmRender_FLOAT | DmRender_STEREO);
	}
}

static DmSegment* load(DmLoader* loader, char const* name) {
	DmSegment* s = NULL;
	DmResult r = DmLoader_getSegment(loader, name, &s);
	printf("%llu load %s %d\n", (unsigned long long) gFrame, name, (int) r);
	if (s != NULL) {
		DmSegment_setRepeats(s, 1000);
	}
	return s;
}

static void reverb_golden(void) {
	// the reverb over a test signal: each setting's output, one hash per 1024-frame block
	static const float kSettings[4][4] = {
	    {0.0f, -10.0f, 1000.0f, 0.001f}, {-3.0f, -6.0f, 2500.0f, 0.5f}, {0.0f, -1.0f, 300.0f, 0.9f}, {-10.0f, -20.0f, 4000.0f, 0.2f}};
	static const float kRates[3] = {22050.0f, 48000.0f, 96000.0f};
	for (int r = 0; r < 3; ++r) {
		for (int s = 0; s < 4; ++s) {
			DmReverb* rv = DmReverb_create(kRates[r]);
			DmReverb_set(rv, kSettings[s][0], kSettings[s][1], kSettings[s][2], kSettings[s][3]);
			uint32_t x = 7;
			int16_t buf[2048];
			printf("reverb %d %d", r, s);
			for (int b = 0; b < 24; ++b) {
				for (int i = 0; i < 2048; ++i) {
					x = x * 1103515245u + 12345u;
					int v = (int) ((x >> 16) & 0xffff) - 32768;
					buf[i] = (int16_t) (b < 12 ? v / (1 + (i % 7)) : (b == 12 && i == 0 ? 30000 : 0));
				}
				DmReverb_processStereo(rv, buf, 1024);
				uint32_t h = 2166136261u;
				for (int i = 0; i < 2048; ++i) {
					h = (h ^ (uint16_t) buf[i]) * 16777619u;
				}
				printf(" %08x", h);
			}
			printf("\n");
			DmReverb_free(rv);
		}
	}
}

int main(int argc, char** argv) {
	if (argc < 4) {
		fprintf(stderr, "dm_golden <music folder> <general MIDI set> <scenario: songs|reverb|SONG>\n");
		return 1;
	}
	snprintf(gFolder, sizeof gFolder, "%s", argv[1]);
	snprintf(gGeneralMidi, sizeof gGeneralMidi, "%s", argv[2]);
	Dm_setRandomNumberGenerator(Rng, NULL);
	if (strcmp(argv[3], "reverb") == 0) {
		reverb_golden();
		return 0;
	}

	DmLoader* loader = NULL;
	DmLoader_create(&loader, DmLoader_DOWNLOAD);
	DmLoader_addResolver(loader, Resolve, NULL);
	DmPerformance_create(&gPerformance, 48000);
	DmPerformance_setReverb(gPerformance, 1, 0.0f, -10.0f, 1000.0f, 0.001f);

	if (strncmp(argv[3], "songs", 5) != 0) {
		// one song a long time, with the groove levels the game sets
		DmSegment* a = load(loader, argv[3]);
		DmPerformance_setVolume(gPerformance, 0.0f);
		DmPerformance_setGrooveLevel(gPerformance, 0);
		DmPerformance_playSegment(gPerformance, a, DmTiming_MEASURE);
		for (int i = 1; i <= 20; ++i) {
			DmPerformance_setVolume(gPerformance, (float) i / 20.0f);
			render_seconds(0.1);
		}
		render_seconds(60.0);
		DmPerformance_setGrooveLevel(gPerformance, 2);
		render_seconds(60.0);
		DmPerformance_setGrooveLevel(gPerformance, 5);
		render_seconds(60.0);
		DmPerformance_setGrooveLevel(gPerformance, 0);
		render_seconds(30.0);
		DmPerformance_stop(gPerformance, 200);
		render_seconds(3.0);
		DmSegment_release(a);
	} else {
		// every song after the next: transitions with each embellishment, stops, instant starts ("songs", the
		// game's eight; "songs=a.sgt,b.sgt,...", others)
		static char const* kSongs[8] = {"Honor.sgt", "Balance.sgt", "Enmity.sgt", "Fallen.sgt",
		                                "Pawns.sgt", "Rivers.sgt", "Shadow2.sgt", "Time.sgt"};
		char const* names[16];
		int count = 0;
		static char list[1024];
		if (argv[3][5] == '=') {
			snprintf(list, sizeof list, "%s", argv[3] + 6);
			for (char* t = strtok(list, ","); t != NULL && count < 16; t = strtok(NULL, ",")) {
				names[count++] = t;
			}
		} else {
			for (; count < 8; ++count) {
				names[count] = kSongs[count];
			}
		}
		DmSegment* s[16];
		for (int i = 0; i < count; ++i) {
			s[i] = load(loader, names[i]);
		}
		DmPerformance_setVolume(gPerformance, 0.75f);
		DmPerformance_playSegment(gPerformance, s[0], DmTiming_MEASURE);
		render_seconds(25.0);
		for (int i = 1; i < count; ++i) {
			DmPerformance_setGrooveLevel(gPerformance, (uint8_t) (i % 4));
			if (i % 3 == 0) {
				DmPerformance_stop(gPerformance, 200);
				render_seconds(1.5);
				DmPerformance_playSegment(gPerformance, s[i], DmTiming_INSTANT);
			} else {
				DmPerformance_playTransition(gPerformance, s[i], (DmEmbellishmentType) (1 + i % 6), DmTiming_MEASURE);
			}
			render_seconds(20.0 + i);
		}
		DmPerformance_playSegment(gPerformance, NULL, DmTiming_MEASURE);
		render_seconds(4.0);
		DmPerformance_playSegment(gPerformance, s[4 % count], DmTiming_MEASURE);
		DmPerformance_stop(gPerformance, 200);   // played and stopped at once (the editor)
		render_seconds(4.0);
		for (int i = 0; i < count; ++i) {
			if (s[i] != NULL) {
				DmSegment_release(s[i]);
			}
		}
	}
	DmPerformance_release(gPerformance);
	DmLoader_release(loader);
	return 0;
}
