/*
 * Copyright 2019 NXP.
 *
 * SPDX-License-Identifier: BSD-3
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "wave_func.h"


int header_parser(FILE *src, struct waveheader *waveheader)
{

	int format_size;
	char chunk_id[4];
	int chunk_size;
	char *header = &waveheader->header[0];

	/* check the "RIFF" chunk */
	fseek(src, 0, SEEK_SET);
	fread(chunk_id, 4, 1, src);
	while (strncmp(chunk_id, "RIFF", 4) != 0){
		fread(&chunk_size, 4, 1, src);
		fseek(src, chunk_size, SEEK_CUR);
		if(fread(chunk_id, 4, 1, src) == 0) {
			printf("Wrong wave file format \n");
			return -1;
		}
	}
	fseek(src, -4, SEEK_CUR);
	fread(&header[0], 1, 12, src);

	/* check the "fmt " chunk */
	fread(chunk_id, 4, 1, src);
	while (strncmp(chunk_id, "fmt ", 4) != 0){
		fread(&chunk_size, 4, 1, src);
		fseek(src, chunk_size, SEEK_CUR);
		if(fread(chunk_id, 4, 1, src) == 0) {
			printf("Wrong wave file format \n");
			return -1;
		}
	}
	/* fmt chunk size */
	fread(&format_size, 4, 1, src);

	fseek(src, -8, SEEK_CUR);
	fread(&header[12], 1, format_size + 8, src);

	/* AudioFormat(2) */

	/* NumChannel(2) */
	waveheader->numchannels = *(short *)&header[12 + 8 + 2];

	/* SampleRate(4) */
	waveheader->samplerate = *(int *)&header[12 + 8 + 2 + 2];

	/* ByteRate(4) */

	/* BlockAlign(2) */
	waveheader->blockalign = *(short *)&header[12 + 8 + 2 + 2 + 4 + 4];

	/* BitsPerSample(2) */
	waveheader->bitspersample = *(short *)&header[12 + 8 + 2 + 2 + 4 + 4 + 2];


	/* check the "data" chunk */
	fread(chunk_id, 4, 1, src);
	while (strncmp(chunk_id, "data", 4) != 0) {
		fread(&chunk_size, 4, 1, src);
		/* seek to next chunk if it is not "data" chunk */
		fseek(src, chunk_size, SEEK_CUR);
		if(fread(chunk_id, 4, 1, src) == 0) {
		    printf("No data chunk found \nWrong wave file format \n");
		    return -1;
		}
	}
	/* wave data length */
	fread(&waveheader->datachunksize, 4, 1, src);
	fseek(src, -8, SEEK_CUR);
	fread(&header[format_size + 20], 1, 8, src);

	return 0;
}

void header_update(FILE *dst, struct waveheader *waveheader)
{
	int format_size;
	char *header = &waveheader->header[0];

	format_size = *(int *)&header[16];

	*(int *)&header[4] = waveheader->datachunksize + 20 + format_size;
	*(int *)&header[24 + format_size] = waveheader->datachunksize;
	*(int *)&header[24] = waveheader->samplerate;
	*(int *)&header[28] =
		waveheader->samplerate * (waveheader->bitspersample / 8)
					* waveheader->numchannels;
	*(unsigned short *)&header[32] = waveheader->blockalign;
	*(unsigned short *)&header[34] = waveheader->bitspersample;

	fseek(dst, 4,  SEEK_SET);
	fwrite(&header[4], 4, 1, dst);

	fseek(dst, 24,  SEEK_SET);
	fwrite(&header[24], 12, 1, dst);

	fseek(dst, 24 + format_size,  SEEK_SET);
	fwrite(&header[24 + format_size], 4, 1, dst);

	fseek(dst, 0, SEEK_END);
}

void header_write(FILE *dst, struct waveheader *waveheader)
{
	int format_size;
	char *header = &waveheader->header[0];
	int i = 0;

	format_size = *(int *)&header[16];

	while (i < (format_size + 28)) {
		fwrite(&header[i], 1, 1, dst);
		i++;
	}
}

int header_update_datachunksize(struct waveheader *waveheader)
{
	int format_size;
	char *header = &waveheader->header[0];

	*(int *)&header[24 + format_size] = waveheader->datachunksize;
	*(int *)&header[4] = waveheader->datachunksize + 20 + format_size;

	return 0;
}

int header_update_blockalign(struct waveheader *waveheader)
{
	int format_size;
	char *header = &waveheader->header[0];

	*(unsigned short *)&header[32] = waveheader->blockalign;

	return 0;
}

int header_update_bitspersample(struct waveheader *waveheader)
{
	char *header = &waveheader->header[0];

	*(unsigned short *)&header[34] = waveheader->bitspersample;
	*(int *)&header[28] =
		waveheader->samplerate * (waveheader->bitspersample / 8)
					* waveheader->numchannels;
	return 0;
}


