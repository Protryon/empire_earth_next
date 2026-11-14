// Copyright © 2024. GothicKit Contributors
// SPDX-License-Identifier: MIT-Modern-Variant
#include "_Internal.h"

#include <stdlib.h>

DmResult DmStyle_create(DmStyle** slf) {
	if (slf == NULL) {
		return DmResult_INVALID_ARGUMENT;
	}

	DmStyle* new = *slf = Dm_alloc(sizeof *new);
	if (new == NULL) {
		return DmResult_MEMORY_EXHAUSTED;
	}

	new->reference_count = 1;

	DmBandList_init(&new->bands);
	DmPartList_init(&new->parts);
	DmPatternList_init(&new->patterns);

	return DmResult_SUCCESS;
}

DmStyle* DmStyle_retain(DmStyle* slf) {
	if (slf == NULL) {
		return NULL;
	}

	(void) atomic_fetch_add(&slf->reference_count, 1);
	return slf;
}

void DmStyle_release(DmStyle* slf) {
	if (slf == NULL) {
		return;
	}

	size_t refs = atomic_fetch_sub(&slf->reference_count, 1) - 1;
	if (refs > 0) {
		return;
	}

	DmPatternList_free(&slf->patterns);
	DmBandList_free(&slf->bands);
	DmPartList_free(&slf->parts);
	Dm_free(slf->backing_memory);
	Dm_free(slf);
}

DmResult DmStyle_download(DmStyle* slf, DmLoader* loader) {
	if (slf == NULL || loader == NULL) {
		return DmResult_INVALID_ARGUMENT;
	}

	DmResult rv = DmResult_SUCCESS;
	for (size_t i = 0; i < slf->bands.length; ++i) {
		rv = DmBand_download(slf->bands.data[i], loader);
		if (rv != DmResult_SUCCESS) {
			break;
		}
	}

	return rv;
}

DmPart* DmStyle_findPart(DmStyle* slf, DmPartReference* pref) {
	if (slf == NULL || pref == NULL) {
		return NULL;
	}

	for (size_t i = 0; i < slf->parts.length; ++i) {
		if (DmGuid_equals(&slf->parts.data[i].part_id, &pref->part_id)) {
			return &slf->parts.data[i];
		}
	}

	return NULL;
}

static uint32_t Dm_toEmbellishmentFlagset(DmCommandType cmd) {
	uint32_t f = 0;

	if (cmd & DmCommand_FILL) {
		f |= 1;
	}

	if (cmd & DmCommand_INTRO) {
		f |= 2;
	}

	if (cmd & DmCommand_BREAK) {
		f |= 4;
	}

	if (cmd & DmCommand_END) {
		f |= 8;
	}

	return f;
}

// See: https://documentation.help/DirectMusic/howmusicvariesduringplayback.htm
DmPattern* DmStyle_getRandomPattern(DmStyle* slf, uint32_t groove, DmCommandType cmd) {
	uint32_t embellishment = Dm_toEmbellishmentFlagset(cmd);

	// (EE patch) As the original chooses, measured against it on Empire Earth's Pawns.sgt: of the patterns
	// whose groove range holds the level, the longest (the original played the 12-measure "Mid - Vibe12bar" every time,
	// the same every run, where upstream drew any groove-2 pattern at random); a random one only among equals. When no
	// range holds the level, the patterns of the nearest range (the original does not fall silent; upstream did).
	// Patterns with a differing embellishment are not supported; neither are one-measure groove patterns (a fix for
	// Gothic 2, in which some are empty with a groove range of 1-100).
	uint32_t best_distance = UINT32_MAX;
	uint32_t best_length = 0;
	size_t candidates = 0;
	for (int pass = 0; pass < 2; ++pass) {
		size_t pick = pass == 1 ? (size_t) (Dm_rand() % (uint32_t) candidates) : 0;
		for (size_t i = 0; i < slf->patterns.length; ++i) {
			DmPattern* pttn = &slf->patterns.data[i];
			if (pttn->embellishment != embellishment && !(pttn->embellishment & embellishment)) {
				continue;
			}
			if (pttn->embellishment == DmCommand_GROOVE && pttn->length_measures == 1) {
				continue;
			}
			uint32_t d = groove < pttn->groove_bottom ? pttn->groove_bottom - groove
			           : groove > pttn->groove_top    ? groove - pttn->groove_top
			                                          : 0;
			uint32_t len = pttn->length_measures;
			if (pass == 0) {
				if (d < best_distance || (d == best_distance && len > best_length)) {
					best_distance = d;
					best_length = len;
					candidates = 0;
				}
				if (d == best_distance && len == best_length) {
					candidates += 1;
				}
			} else if (d == best_distance && len == best_length) {
				if (pick == 0) {
					return pttn;
				}
				pick -= 1;
			}
		}
		if (candidates == 0) {
			return NULL;
		}
	}
	return NULL;
}

void DmPart_init(DmPart* slf) {
	if (slf == NULL) {
		return;
	}

	memset(slf, 0, sizeof *slf);
}

void DmPart_free(DmPart* slf) {
	if (slf == NULL) {
		return;
	}

	Dm_free(slf->notes);
	Dm_free(slf->curves);
}

uint32_t DmPart_getValidVariationCount(DmPart* slf) {
	if (slf == NULL) {
		return 0;
	}

	uint32_t i = 0;
	for (; i < 32; ++i) {
		if ((slf->variation_choices[i] & 0x0FFFFFFF) == 0) {
			break;
		}
	}

	return i;
}

void DmPartReference_init(DmPartReference* slf) {
	if (slf == NULL) {
		return;
	}

	memset(slf, 0, sizeof *slf);
}

void DmPartReference_free(DmPartReference* slf) {
	(void) slf;
}

void DmPattern_init(DmPattern* slf) {
	if (slf == NULL) {
		return;
	}

	memset(slf, 0, sizeof *slf);
	DmPartReferenceList_init(&slf->parts);
}

void DmPattern_free(DmPattern* slf) {
	if (slf == NULL) {
		return;
	}

	DmPartReferenceList_free(&slf->parts);
	Dm_free(slf->rhythm);
	slf->rhythm = NULL;
	slf->rhythm_len = 0;
}
