/*
 * Copyright 2018-2019 NXP.
 *
 * ASRC File based sample test using 32-bit Q31 filters and 128 Interpolation
 *
 * SPDX-License-Identifier: BSD-3
 */

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <math.h>
#include <endian.h>
#include "asrcapi.h"
#include "wave_func.h"

#define CHANNEL_COUNT 2
#define INPUT_BLOCK_SIZE 128
#define OUTPUT_BLOCK_SIZE 16
#define ASRC_FS_IN_48000 48000
#define ASRC_FS_OUT_48000 48000
#define INTERP_FACTOR_M 128.0

char* infile = NULL;
char* outfile = NULL;
uint32_t gainValue = 0;
uint32_t inputClockOffset = 0;
void print_help(const char *argv[]);
int parse_arguments(int argc, const char *argv[], tASRC *asrc);
int  update_wav_hdr (FILE* outfile,  int32_t samplerate, int32_t channels,  int32_t nsamples);

int main(int argc, const char *argv[])
{
	tASRC asrc;
	uint8_t bEOF = 0;
	uint8_t wavOutFile = 0;

	FILE *fpIn = NULL;
	FILE *fpOut = NULL;
	int status = ASRC_OK;
	int inSampleCount = 0;
	int outSampleCount  = 0;
	fract32* inputBuffer = NULL;
	fract32* inputBuffer_s = NULL;
	fract32* outputBuffer = NULL;
	uint64_t totalOutSamples = 0;
	struct waveheader in_waveheader;
	struct waveheader out_waveheader;

	if (parse_arguments(argc, argv, &asrc) != 0)
		return -1;

	fpIn = fopen(infile, "rb");
	if (!fpIn) {
		perror("Failed to open input file");
		return -1;
	}

	fpOut = fopen (outfile, "wb");
	if (!fpOut) {
		perror("Failed to open output file");
		goto err_open_out_file;
	}

	if (header_parser(fpIn,  &in_waveheader) < 0)
		goto err_header_parse;

	if (in_waveheader.bitspersample != 16 &&
		in_waveheader.bitspersample != 32) {
		printf("unsupported sample bits %d\n",
			in_waveheader.bitspersample);
		goto err_header_parse;
	}

	asrc.channels = in_waveheader.numchannels;
	asrc.fsIn = in_waveheader.samplerate;

	if (asrcCreate(&asrc) != ASRC_OK) {
		printf("\n asrcCreate Failed \n");
		goto err_header_parse;
	}

	if(asrcGainControl(&asrc, gainValue) != ASRC_OK) {
		printf("\n asrcGainControl Failed \n");
		goto err_gain_control;
	}

	memcpy(&out_waveheader, &in_waveheader, sizeof(struct waveheader));

	out_waveheader.samplerate = asrc.fsOut;
	out_waveheader.bitspersample = 32;

	header_write(fpOut, &out_waveheader);

	if(asrc.quality > 1)
		asrc.quality = 1;

	if(inputClockOffset != 0) {
		fract64 fStep;
		double phaseBits;
		float relativeInputFS;
		relativeInputFS = asrc.fsIn + (float)inputClockOffset;
		phaseBits = 32.0 + (log(INTERP_FACTOR_M)/log(2));
		fStep = (fract64) (((relativeInputFS)/asrc.fsOut) * pow(2.0, phaseBits));
		printf("\n Adjusting fStep=%lld for inputClockOffset=%d  \n", fStep, inputClockOffset);
		asrcConversionRatio(&asrc, fStep);
	}

	inputBuffer = (fract32 *)malloc(asrc.inBlockSize * asrc.channels * in_waveheader.bitspersample / 8);
	outputBuffer = (fract32 *)malloc(asrc.outBlockSize * asrc.channels * sizeof(fract32));
	inputBuffer_s = (fract32 *)malloc(asrc.inBlockSize * asrc.channels * sizeof(fract32));

	if ((inputBuffer == NULL) || (outputBuffer == NULL) || (inputBuffer_s == NULL)) {
		if(inputBuffer != NULL)
			free(inputBuffer);

		if(inputBuffer_s != NULL)
			free(inputBuffer_s);

		if(outputBuffer != NULL)
			free(outputBuffer);

		printf("\n malloc error \n");
		goto err_gain_control;
	}

	//Get first block of data
	inSampleCount = (int) fread (&inputBuffer[0], in_waveheader.bitspersample / 8,
				     asrc.channels * asrc.inBlockSize, fpIn);

	while(1)
	{
		while(status == ASRC_OK )
		{
			if (inSampleCount != asrc.channels * asrc.inBlockSize) {
				printf("\n End of file reached \n");
				status = ASRC_NOK;
				break;
			}
			if (in_waveheader.bitspersample == 16) {
				int i;
				short data;
				short *buf = (short *) inputBuffer;
				for (i = 0; i < inSampleCount; i++) {
					data = buf[i];
					inputBuffer_s[i] = (int)data << 16;
				}
			} else {
				memcpy(inputBuffer_s, inputBuffer, inSampleCount * 4);
			}

			status = asrcInput(&asrc, inputBuffer_s);
			//printf("\n input asrcInput returned status=%d \n", status);
			if( status == ASRC_OVF)
			{
				status = ASRC_OK;
				break;
			}
			if( status == ASRC_NOK)
			{
				printf("\n asrc retunred NOK for for asrcInput \n");
				goto err_asrc_input;
			}
			inSampleCount = (int) fread (&inputBuffer[0],
				in_waveheader.bitspersample / 8, asrc.channels * asrc.inBlockSize, fpIn);
		}

		while(status == ASRC_OK)
		{
			status = asrcOutput(&asrc, outputBuffer);
			//printf("\n output asrcOutput returned status=%d \n", status);
			if (status == ASRC_UNF) {
				status = ASRC_OK;
				break;
			}

			if (status == ASRC_NOK) {
				printf("\n asrc retunred NOK for asrcOutput \n");
				goto err_asrc_output;
			}
			/* Write the data*/
			outSampleCount = (int) fwrite (&outputBuffer[0],
				sizeof(fract32), asrc.channels * asrc.outBlockSize, fpOut);

			if (outSampleCount != asrc.channels * asrc.outBlockSize) {
				printf("\n File write failed!\n");
				goto err_write_file;
			}

			totalOutSamples += asrc.outBlockSize;
		}
		if(status == ASRC_NOK)
			break;
	}

	out_waveheader.datachunksize = totalOutSamples *
					out_waveheader.numchannels *
					out_waveheader.bitspersample/8;
	header_update(fpOut, &out_waveheader);

	asrcDelete(&asrc);

	free(inputBuffer);
	free(inputBuffer_s);
	free(outputBuffer);
	fclose(fpOut);
	fclose(fpIn);

	return 0;

err_write_file:
err_asrc_output:
err_asrc_input:
	free(inputBuffer);
	free(inputBuffer_s);
	free(outputBuffer);
err_gain_control:
	asrcDelete(&asrc);
err_header_parse:
	fclose(fpOut);
err_open_out_file:
	fclose(fpIn);
	return -1;
}

int parse_arguments(int argc, const char *argv[], tASRC *asrc)
{
	asrc->inBlockSize = INPUT_BLOCK_SIZE;
	asrc->outBlockSize = OUTPUT_BLOCK_SIZE;
	asrc->fsIn = ASRC_FS_IN_48000;
	asrc->fsOut = ASRC_FS_OUT_48000;
	asrc->channels = CHANNEL_COUNT;
	asrc->quality = 1;
	asrc->internalRatioControl = false;

	/* Usage checking  */
	if( argc < 3 )
	{
		print_help(argv);
		exit(1);
	}

	int c, option_index;
	static const char short_options[] = "ho:x:z:b:k:q:g:r:";
	static const struct option long_options[] = {
		{"help", 0, 0, 'h'},
		{"outFreq", 1, 0, 'o'},
		{"inputFile", 1, 0, 'x'},
		{"outputFile", 1, 0, 'z'},
		{"blocksize", 1, 0, 'b'},
		{"quality", 1, 0, 'q'},
		{"gain", 1, 0, 'g'},
		{"inputClockOffset", 1, 0, 'k'},
		{0, 0, 0, 0}
	};

	while ((c = getopt_long(argc, (char * const*)argv, short_options, long_options, &option_index)) != -1) {
		switch (c) {
		case 'o':
			asrc->fsOut = strtol(optarg,NULL,0);
			break;
		case 'x':
			infile = optarg;
			break;
		case 'z':
			outfile = optarg;
			break;
		case 'q':
			asrc->quality = strtol(optarg,NULL,0);
			break;
		case 'b':
			asrc->inBlockSize =  strtol(optarg,NULL,0);
			asrc->outBlockSize = 16; //inputBlockSize;
			break;
		case 'k':
			inputClockOffset = strtol(optarg,NULL,0);
			break;
		case 'g':
			gainValue = strtol(optarg,NULL,0);
			break;
		case 'h':
			print_help(argv);
			exit(1);
	default:
			printf("Unknown Command  -%c \n", c);
			exit(1);
		}
	}

	printf("\n fsOut=%d, inBlockSize=%d, outBlockSize=%d, quality=%d gain=%d inFile=%s, outFile=%s \n",
		asrc->fsOut, asrc->inBlockSize, asrc->outBlockSize, asrc->quality, gainValue, infile, outfile);
	return 0;
}

void print_help(const char *argv[])
{
	printf("Usage: %s \n"
			"	--- help (h): this screen \n"
			"	--- inputFile(x): raw audio data to process\n"
			"	--- outputFile(z): output file for converted audio\n"
			"	--- outFreq (o): output Frequency \n"
			"	--- quality (q): quality to be either 0 or 1 \n"
			"	--- gain (h): gain to be applied 0..3 \n"
			"	--- inputClockOffset(k): optional input clock offset in Hz, to add to processing to simulate asynchronous clocks \n"
			"	--- blocksize (b): (optional) block size to be used \n" , argv[0]);
	return;
}
