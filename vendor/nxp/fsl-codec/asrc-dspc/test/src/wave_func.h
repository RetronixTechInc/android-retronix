/*
 * Copyright 2019 NXP.
 *
 * SPDX-License-Identifier: BSD-3
 */

#ifndef WAVE_FUNC_H
#define WAVE_FUNC_H

#include <stdio.h>
#include <stdlib.h>

#define WAVE_HEAD_SIZE 44 + 14 + 16

struct waveheader {
	int   riffchunkoffset;
	int   riffchunksize;
	int   fmtchunkoffset;
	int   fmtchunksize;
	short numchannels;
	int   samplerate;
	int   byterate;
	short blockalign;
	short bitspersample;
	int   datachunkoffset;
	int   datachunksize;

	char header[WAVE_HEAD_SIZE];
};


int header_parser(FILE *src, struct waveheader *waveheader);
void header_update(FILE *dst, struct waveheader *waveheader);
void header_write(FILE *dst, struct waveheader *waveheader);

#endif
