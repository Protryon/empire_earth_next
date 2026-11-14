// Copyright © 2024. GothicKit Contributors
// SPDX-License-Identifier: MIT-Modern-Variant
#include "_Internal.h"

DmResult DmBand_create(DmBand** slf) {
	if (slf == NULL) {
		return DmResult_INVALID_ARGUMENT;
	}

	DmBand* new = *slf = Dm_alloc(sizeof *new);
	if (new == NULL) {
		return DmResult_MEMORY_EXHAUSTED;
	}

	new->reference_count = 1;
	return DmResult_SUCCESS;
}

DmBand* DmBand_retain(DmBand* slf) {
	if (slf == NULL) {
		return NULL;
	}

	(void) atomic_fetch_add(&slf->reference_count, 1);
	return slf;
}

void DmBand_release(DmBand* slf) {
	if (slf == NULL) {
		return;
	}

	size_t count = atomic_fetch_sub(&slf->reference_count, 1) - 1;
	if (count > 0) {
		return;
	}

	for (size_t i = 0; i < slf->instruments_len; ++i) {
		DmInstrument_free(&slf->instruments[i]);
	}

	Dm_free(slf->instruments);
	Dm_free(slf);
}

DmDlsInstrument* DmInstrument_getDlsInstrument(DmInstrument* slf) {
	if (slf == NULL || slf->dls == NULL) {
		return NULL;
	}

	// (EE patch) DLS bank: drum flag in bit 31, MSB in bits 8-14, LSB in bits 0-6; the band patch: MSB 16-22, LSB 8-14,
	// drum 31. Upstream compared the LSB alone, so an instrument in bank MSB 1 / LSB 2 matched as bank 2.
	uint32_t bank = (slf->patch & 0x80000000U) | (((slf->patch >> 16) & 0x7FU) << 8) | ((slf->patch >> 8) & 0x7FU);
	uint32_t patch = slf->patch & 0x7FU;

	DmDlsInstrument* ins = NULL;
	for (long long i = slf->dls->instrument_count-1; i >= 0; --i) {
		ins = &slf->dls->instruments[i];

		// If it's the correct instrument, return it.
		// TODO(lmichaelis): Dirty fix for drum kit problems. Instead of choosing the first valid DLS instrument, we
		//                   chose the last valid one. This acts as if later instruments override previous ones and
		//                   thus prevents problems where the same channel is re-used multiple times, specifically in
		//                   Gothic 1, which assigns a drum kit and melodic instrument to the same channel. This works
		//                   in conjunction with the TSF creation in Dm_createHydra to prevent drum kit issues in G1.
		if ((ins->bank & 0x80007F7FU) == bank && ins->patch == patch) {
			return ins;
		}
	}

	Dm_report(DmLogLevel_WARN,
	          "DmBand: Instrument patch %d:%d not found in band '%s'",
	          bank,
	          patch,
	          slf->reference.name);
	return NULL;
}

DmResult DmBand_download(DmBand* slf, DmLoader* loader) {
	if (slf == NULL || loader == NULL) {
		return DmResult_INVALID_ARGUMENT;
	}

	Dm_report(DmLogLevel_INFO, "DmBand: Downloading instruments for band '%s'", slf->info.unam);

	DmResult rv = DmResult_SUCCESS;
	for (size_t i = 0; i < slf->instruments_len; ++i) {
		DmInstrument* instrument = &slf->instruments[i];

		// The DLS has already been downloaded. We don't need to do it again.
		if (instrument->dls != NULL) {
			continue;
		}

		// If the patch is not valid, this instrument cannot be played since we don't know
		// where to find it in the DLS collection.
		if (!(instrument->options & (DmInstrument_VALID_PATCH | DmInstrument_VALID_BANKSELECT))) {
			Dm_report(DmLogLevel_DEBUG,
			          "DmBand: Not downloading instrument '%s' without valid patch",
			          instrument->reference.name);
			continue;
		}

		if (instrument->options & DmInstrument_GENERAL_MIDI) {
			instrument->reference.file = "gm.dls";
			Dm_report(DmLogLevel_INFO,
			          "DmBand: Trying to download instrument '%s' from the general MIDI collection (gm.dls)",
			          instrument->reference.name);
		} else if (instrument->options & (DmInstrument_ROLAND_GS | DmInstrument_YAMAHA_XG)) {
			// (EE patch) The original has one built-in collection, gm.dls (the Roland GM/GS sound set, its GS variations
			// in their own banks): a GS or XG instrument comes from it too. Upstream asked for gs.dls / xg.dls, which do
			// not exist, and the part stayed silent (Pawns.sgt's vibraphone and orchestra hit).
			instrument->reference.file = "gm.dls";
			Dm_report(DmLogLevel_INFO,
			          "DmBand: Trying to download GS/XG instrument '%s' from the default collection (gm.dls)",
			          instrument->reference.name);
		}

		rv = DmLoader_getDownloadableSound(loader, &instrument->reference, &instrument->dls);
		if (rv != DmResult_SUCCESS || instrument->dls == NULL) {
			// (EE patch) An instrument whose collection is missing stays silent; the band still loads. Upstream
			// returned the last instrument's result: a song with one such instrument loaded the first time (a
			// later instrument's success came last) and failed every time after (the others were already there).
			rv = DmResult_SUCCESS;
			continue;
		}

		// (EE patch) A SoundFont lists no instruments here: the synthesizer finds the preset by bank and patch.
		if (instrument->dls->sound_font_len != 0) {
			continue;
		}

		DmDlsInstrument* dls_instrument = DmInstrument_getDlsInstrument(instrument);
		if (dls_instrument == NULL) {
			continue;
		}

		Dm_report(DmLogLevel_DEBUG,
		          "DmBand: DLS instrument '%s' assigned to channel %d for band '%s'",
		          dls_instrument->info.inam,
		          instrument->channel,
		          slf->info.unam);
	}

	return rv;
}

void DmInstrument_free(DmInstrument* slf) {
	if (slf == NULL || slf->dls == NULL) {
		return;
	}

	DmDls_release(slf->dls);
	slf->dls = NULL;
}
