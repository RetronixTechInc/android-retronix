/**
 *  Copyright (c) 2015, Freescale Semiconductor Inc.,
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */



#ifndef ID3_PARSER_H_
#define ID3_PARSER_H_

#include <string.h>
#include <stdint.h>

typedef void * ID3ParserHandle;

#define ID3V2_HEADER_SIZE 10
#define ID3V1_DATA_SIZE 128

typedef struct {
    const char *key;
	const char *value;
} metadata;

typedef enum eVersion {
    ID3_UNKNOWN,
    ID3_V1,
    ID3_V1_1,
    ID3_V2_2,
    ID3_V2_3,
    ID3_V2_4,
}Version;


int32_t ID3ParserCreatParser(ID3ParserHandle *h);

void     ID3ParserDeleteParser(ID3ParserHandle h);

uint32_t ID3ParserParseV1(ID3ParserHandle h, const uint8_t *buf, size_t size);

uint32_t ID3ParserParseV2(ID3ParserHandle h, const uint8_t *buf, size_t size);

/*if it is id3v2, return the id3v2 data size, or retutn 0 */
uint64_t ID3ParserGetV2Size(ID3ParserHandle h, const uint8_t *buf, size_t size);

const void *ID3ParserGetAlbumArt(ID3ParserHandle h, size_t *length, char **mime);

void ID3ParserGetTag(ID3ParserHandle h, metadata *meta);

/* get the number of tags */
void ID3ParserGetSize(ID3ParserHandle h, size_t *size);

Version ID3ParserGetVersion(ID3ParserHandle h);


#endif

