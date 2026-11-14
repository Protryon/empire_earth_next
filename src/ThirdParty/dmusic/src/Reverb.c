// Copyright © 2026. GothicKit Contributors
// SPDX-License-Identifier: MIT-Modern-Variant
//
// (EE patch) The synthesizer reverb of the original game's music (the effect its music switches on, with its four
// settings), written anew from its observed behaviour for interoperability: the same delays, coefficients and
// fixed-point steps, so the output matches the reference output sample for sample at any rate and setting (tested
// at 22050, 44100 and 48000 Hz with six parameter sets on clipping music). It runs on the finished stereo mix in
// place, 16-bit, as the original's synth runs it.
#include "_Internal.h"

#include <math.h>

struct DmReverb {
	float rate;
	int32_t len[4];   // the delay lines, frames (each shortened by its all-pass)
	int32_t ap[2];    // the all-pass delays, frames
	int32_t dry, wet, golden, ap_gain[2], cur[4], prev[4];   // Q15
	int32_t delay[0x4000];         // 4096 frames x 4 lines, interleaved
	int32_t allpass[2][0x800];     // 1024 frames x 2 each
	uint32_t pos, apos;
};

static int32_t DmReverb_q15(float f) {
	float v = f * 32768.0f + 0.5f;
	if (v > 32767.0f) v = 32767.0f;
	if (v < -32768.0f) v = -32768.0f;
	return (int32_t) v;
}

// 32-bit integer arithmetic that wraps, as the processor's does
static int32_t DmReverb_mul(int32_t a, int32_t b) {
	return (int32_t) ((uint32_t) a * (uint32_t) b);
}
static int32_t DmReverb_add(int32_t a, int32_t b) {
	return (int32_t) ((uint32_t) a + (uint32_t) b);
}
static int32_t DmReverb_sub(int32_t a, int32_t b) {
	return (int32_t) ((uint32_t) a - (uint32_t) b);
}

void DmReverb_set(DmReverb* slf, float gain_in_db, float mix_db, float time_ms, float hf_ratio) {
	float hf = hf_ratio < 0.999f ? hf_ratio : 0.999f;
	if (!(hf > 0.0f)) hf = 0.001f;
	float time = time_ms > 0.001f ? time_ms : 0.001f;
	float hf_k = 1.0f / hf - 1.0f;
	float g_in = gain_in_db < 0.0f ? gain_in_db : 0.0f;
	float mix = mix_db < 0.0f ? mix_db : 0.0f;
	if (slf->rate < 1.0f) slf->rate = 22050.0f;

	float k = -3000.0f / (slf->rate * time);   // log10 gain per frame for a 60 dB decay over the reverb time
	float ap_g[2];
	ap_g[0] = (float) pow(10.0, (double) ((float) slf->ap[0] * k));
	ap_g[1] = (float) pow(10.0, (double) ((float) slf->ap[1] * k));

	float norm2 = 0.0f;
	float cur[4], prev[4];
	for (int i = 0; i < 4; ++i) {
		int a = i < 2 ? 0 : 1;
		float g = (float) pow(10.0, (double) ((float) slf->len[i] * k));
		float x = g * ap_g[a];
		float p = (((x + 1.0f) * x + 1.0f) * x + 1.0f) * x + 1.0f;
		norm2 += p * p;
		float c = (float) pow(10.0, (double) ((float) (slf->len[i] + slf->ap[a]) * k * hf_k));
		float d = (1.0f - c) * 0.5f;   // the high-frequency damping: one pole
		prev[i] = g * d;
		cur[i] = (1.0f - d) * g;
	}

	float norm = (float) sqrt((double) norm2);
	float in_lin = (float) pow(10.0, (double) (g_in * 0.05f));
	float mix_lin = (float) pow(10.0, (double) (mix * 0.1f));
	float dry = (float) sqrt((double) (1.0f - mix_lin)) * in_lin;
	float wet = (float) sqrt((double) mix_lin) * (4.0f / norm * in_lin);

	union {
		uint32_t u;
		float f;
	} golden = {0x3f1e377aU};   // 0.618034, the golden ratio's fraction, as the original stores it

	slf->dry = DmReverb_q15(dry);
	slf->wet = DmReverb_q15(wet);
	slf->golden = DmReverb_q15(golden.f);
	slf->ap_gain[0] = DmReverb_q15(ap_g[0]);
	slf->ap_gain[1] = DmReverb_q15(ap_g[1]);
	for (int i = 0; i < 4; ++i) {
		slf->cur[i] = DmReverb_q15(cur[i]);
		slf->prev[i] = DmReverb_q15(prev[i]);
	}
}

DmReverb* DmReverb_create(float rate) {
	DmReverb* slf = Dm_alloc(sizeof *slf);
	if (slf == NULL) {
		return NULL;
	}

	memset(slf, 0, sizeof *slf);
	slf->rate = rate;

	int32_t d1 = (int32_t) ((double) rate * 0.045 + 0.5);
	int32_t d2 = (int32_t) ((double) ((float) d1 * 1.18920708f) + 0.5);
	int32_t d3 = (int32_t) ((double) ((float) d2 * 1.18920708f) + 0.5);
	int32_t d4 = (int32_t) ((double) ((float) d3 * 1.18920708f) + 0.5);
	int32_t a1 = (int32_t) ((double) (float) (d1 + d3) * 0.115466669202 + 0.5);
	int32_t a2 = (int32_t) ((double) (float) (d2 + d4) * 0.115466669202 + 0.5);

	slf->len[0] = d1 - a1;
	slf->len[1] = d3 - a1;
	slf->len[2] = d2 - a2;
	slf->len[3] = d4 - a2;
	slf->ap[0] = a1;
	slf->ap[1] = a2;

	DmReverb_set(slf, 0.0f, -10.0f, 1000.0f, 0.001f);   // the original's defaults
	return slf;
}

void DmReverb_free(DmReverb* slf) {
	Dm_free(slf);
}

static int16_t DmReverb_clamp16(int32_t v) {
	return (int16_t) (v > 32767 ? 32767 : v < -32768 ? -32768 : v);
}

void DmReverb_processStereo(DmReverb* slf, int16_t* buf, size_t frames) {
	int32_t* d = slf->delay;
	uint32_t pos = slf->pos, apos = slf->apos;

	uint32_t i1 = (uint32_t) (4 * slf->len[0] + 4 + (int32_t) pos) & 0x3fff;
	uint32_t i2 = (uint32_t) (4 * slf->len[1] + 4 + (int32_t) pos) & 0x3fff;
	uint32_t i3 = (uint32_t) (4 * slf->len[2] + 4 + (int32_t) pos) & 0x3fff;
	uint32_t i4 = (uint32_t) (4 * slf->len[3] + 4 + (int32_t) pos) & 0x3fff;
	int32_t* p1 = &d[i1];
	int32_t* p2 = &d[i2 + 1];
	int32_t* p3 = &d[i3 + 2];
	int32_t* p4 = &d[i4 + 3];
	i1 = (i1 - 4) & 0x3fff;
	i2 = (i2 - 4) & 0x3fff;
	i3 = (i3 - 4) & 0x3fff;
	i4 = (i4 - 4) & 0x3fff;

	const int32_t g = slf->golden;
	for (size_t n = 0; n < frames; ++n) {
		const int32_t in_l = buf[2 * n], in_r = buf[2 * n + 1];
		int32_t acc_l = DmReverb_mul(slf->dry, in_l) >> 15;
		int32_t acc_r = DmReverb_mul(slf->dry, in_r) >> 15;
		const int32_t w_l = DmReverb_mul(slf->wet, in_l) >> 15;
		const int32_t w_r = DmReverb_mul(slf->wet, in_r) >> 15;

		// The first all-pass pair (lines 1 and 2)
		int32_t* a = &slf->allpass[0][(2 * slf->ap[0] + apos) & 0x7ff];
		const int32_t o1 = d[i1], o2 = d[i2 + 1];
		const int32_t y1a = DmReverb_add(DmReverb_mul(o2, g), DmReverb_mul(a[0], slf->ap_gain[0])) >> 15;
		const int32_t y1b = DmReverb_sub(DmReverb_mul(a[1], slf->ap_gain[0]), DmReverb_mul(o1, g)) >> 15;
		slf->allpass[0][apos & 0x7ff] = DmReverb_add(DmReverb_mul(y1b, g) >> 15, o1);
		slf->allpass[0][(apos & 0x7ff) + 1] = DmReverb_sub(o2, DmReverb_mul(y1a, g) >> 15);
		d[i1] = y1a;
		d[i2 + 1] = y1b;

		// The second (lines 3 and 4)
		int32_t* b = &slf->allpass[1][(2 * slf->ap[1] + apos) & 0x7ff];
		const int32_t o3 = d[i3 + 2], o4 = d[i4 + 3];
		const int32_t y2a = DmReverb_add(DmReverb_mul(o4, g), DmReverb_mul(b[0], slf->ap_gain[1])) >> 15;
		const int32_t y2b = DmReverb_sub(DmReverb_mul(b[1], slf->ap_gain[1]), DmReverb_mul(o3, g)) >> 15;
		slf->allpass[1][apos & 0x7ff] = DmReverb_add(DmReverb_mul(y2b, g) >> 15, o3);
		slf->allpass[1][(apos & 0x7ff) + 1] = DmReverb_sub(o4, DmReverb_mul(y2a, g) >> 15);
		d[i3 + 2] = y2a;
		d[i4 + 3] = y2b;

		acc_l = DmReverb_add(acc_l, DmReverb_add(y2a, y1a));
		acc_r = DmReverb_add(acc_r, DmReverb_add(y1b, y2b));

		// The lines' damping (this frame's output and the last) and the wet input, written at the head
		int32_t* w = &d[pos];
		w[0] = DmReverb_add(DmReverb_add(DmReverb_mul(slf->prev[0], *p1), DmReverb_mul(slf->cur[0], y1a)) >> 15, w_l);
		p1 = &d[i1];
		i1 = (i1 - 4) & 0x3fff;
		w[1] = DmReverb_add(DmReverb_add(DmReverb_mul(slf->cur[1], d[i2 + 1]), DmReverb_mul(*p2, slf->prev[1])) >> 15, w_r);
		p2 = &d[i2 + 1];
		i2 = (i2 - 4) & 0x3fff;
		w[2] = DmReverb_sub(DmReverb_add(DmReverb_mul(slf->prev[2], *p3), DmReverb_mul(slf->cur[2], d[i3 + 2])) >> 15, w_r);
		p3 = &d[i3 + 2];
		i3 = (i3 - 4) & 0x3fff;
		w[3] = DmReverb_add(DmReverb_add(DmReverb_mul(slf->prev[3], *p4), DmReverb_mul(slf->cur[3], d[i4 + 3])) >> 15, w_l);
		p4 = &d[i4 + 3];
		i4 = (i4 - 4) & 0x3fff;

		apos = (apos - 2) & 0x7ff;
		pos = (pos - 4) & 0x3fff;
		buf[2 * n] = DmReverb_clamp16(acc_l);
		buf[2 * n + 1] = DmReverb_clamp16(acc_r);
	}

	slf->pos = pos;
	slf->apos = apos;
}
