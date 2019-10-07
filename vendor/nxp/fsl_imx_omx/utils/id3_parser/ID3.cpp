/**
 *  Copyright (c) 2015, Freescale Semiconductor Inc.,
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */

#define LOG_TAG "ID3"

#include "id3_parser/ID3.h"
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/Utils.h>
#include <media/stagefright/DataSource.h>
#include <utils/String8.h>
#include <byteswap.h>
#include <media/CharacterEncodingDetector.h>
#include <include/ID3.h>
#include "Mem.h"
#include "Log.h"

#if 0
#undef LOG_DEBUG
#define LOG_DEBUG printf
#endif

typedef void * DetectorHandle;
typedef void * ID3Handle;

typedef struct {

	uint32_t isValid;
	ID3Handle id3_handle;
	DetectorHandle detector_handle;

} ID3Parser ;

typedef struct {
        const char *key;
        const char *tag1;
        const char *tag2;
} Map;

static const Map kMap[] = {
        { "album",       "TALB", "TAL" },
        { "artist",      "TPE1", "TP1" },
        { "albumartist", "TPE2", "TP2" },
        { "composer",    "TCOM", "TCM" },
        { "genre",       "TCON", "TCO" },
        { "title",       "TIT2", "TT2" },
        { "year",        "TYE",  "TYER" },
        { "tracknumber", "TRK",  "TRCK" },
        { "discnumber",  "TPA",  "TPOS" },
    };

static const size_t kNumMapEntries = sizeof(kMap)/sizeof(Map);

struct MemorySource : public android::DataSource {
    MemorySource(const uint8_t *data, size_t size)
        : mData(data),
          mSize(size) {
    }

    virtual android::status_t initCheck() const {
        return android::OK;
    }

    virtual ssize_t readAt(off64_t offset, void *data, size_t size) {
        off64_t available = (offset >= (off64_t)mSize) ? 0ll : mSize - offset;

        size_t copy = (available > (off64_t)size) ? size : available;
        memcpy(data, mData + offset, copy);

        return copy;
    }

    virtual android::status_t getSize(off64_t *size) {
    *size = mSize;

    return android::OK;
    }

private:
    const uint8_t *mData;
    size_t mSize;

    DISALLOW_EVIL_CONSTRUCTORS(MemorySource);
};




int32_t ID3ParserCreatParser(ID3ParserHandle *h)
{
    ID3Parser *id3_parser = NULL;

    id3_parser = (ID3Parser*)FSL_NEW((ID3Parser), ());
    if (!id3_parser)
        return -1;

    id3_parser->isValid         = false;
    id3_parser->id3_handle      = NULL;
    id3_parser->detector_handle = (DetectorHandle *)new android::CharacterEncodingDetector();

    *h = (ID3ParserHandle *)id3_parser;
    return 0;

}

void ID3ParserDeleteParser(ID3ParserHandle h)
{
    ID3Parser *id3_parser = (ID3Parser *)h;
    android::ID3 *id3 = (android::ID3 *)id3_parser->id3_handle;
    android::CharacterEncodingDetector *detector = (android::CharacterEncodingDetector *)id3_parser->detector_handle;

    if (id3) {
        FSL_DELETE(id3);
        id3_parser->id3_handle = NULL;
    }

    if (detector) {
        FSL_DELETE(detector);
        id3_parser->detector_handle = NULL;
    }

    FSL_DELETE(id3_parser);
    id3_parser = NULL;
}


uint64_t  ID3ParserGetV2Size(ID3ParserHandle h, const uint8_t *buf, size_t size)
{

    struct id3_header {
        char id[3];
        uint8_t version_major;
        uint8_t version_minor;
        uint8_t flags;
        uint8_t enc_size[4];
    };
    id3_header header;
    uint32_t len = 0;

    if (memcmp((void*)"ID3",buf, 3)) {
        return 0;
    }

    len =     ((buf[6] & 0x7f) << 21)
			| ((buf[7] & 0x7f) << 14)
			| ((buf[8] & 0x7f) << 7)
			| ( buf[9] & 0x7f);

    if (len > 3 * 1024 * 1024)
		len = 3 * 1024 * 1024;

	len += 10;
    return len;

}

static uint32_t  ParseData(ID3Parser *id3_parser, const uint8_t *buf, size_t size, bool ignoreV1)
{
    android::ID3 *id3 = (android::ID3 *)id3_parser->id3_handle;
    android::CharacterEncodingDetector *detector = (android::CharacterEncodingDetector *)id3_parser->detector_handle;

    if (id3) {
        FSL_DELETE(id3);
        id3_parser->id3_handle = NULL;
    }

    android::sp<MemorySource> source = FSL_NEW((MemorySource), (buf, size));
    id3 = FSL_NEW((android::ID3), (source, ignoreV1));
    id3_parser->id3_handle = (ID3Handle *)id3;

    id3_parser->isValid = id3->isValid();
    if (!id3_parser->isValid) {
        return id3_parser->isValid;
    }

    for (size_t i = 0; i < kNumMapEntries; ++i) {
        android::ID3::Iterator *it = FSL_NEW((android::ID3::Iterator), (*id3, kMap[i].tag1));
        if (it->done()) {
            FSL_DELETE(it);
            it = FSL_NEW((android::ID3::Iterator), (*id3, kMap[i].tag2));
        }

        if (it->done()) {
            FSL_DELETE(it);
            continue;
        }

        android::String8 s;
        it->getString(&s);
        FSL_DELETE(it);

        detector->addTag(kMap[i].key, s.string());
        LOG_DEBUG("key: %s Value: %s\n", kMap[i].key, (char *)s.string());
    }

    detector->detectAndConvert();
    return id3_parser->isValid;
}


uint32_t ID3ParserParseV1(ID3ParserHandle h, const uint8_t *buf, size_t size)
{
    ID3Parser *id3_parser = (ID3Parser *)h;
    android::ID3 *id3 = (android::ID3 *)id3_parser->id3_handle;

    return ParseData(id3_parser, buf, size, FALSE);
}

uint32_t ID3ParserParseV2(ID3ParserHandle h, const uint8_t *buf, size_t size)
{
    ID3Parser *id3_parser = (ID3Parser *)h;
    android::ID3 *id3 = (android::ID3 *)id3_parser->id3_handle;
    bool result = FALSE;

    return ParseData(id3_parser, buf, size, TRUE);
}

void  ID3ParserGetTag(ID3ParserHandle h, metadata *meta)
{
    ID3Parser *id3_parser = (ID3Parser *)h;
    android::CharacterEncodingDetector *detector = (android::CharacterEncodingDetector *)id3_parser->detector_handle;

    size_t size = detector->size();
    for (size_t i = 0; i < size; i++)
        detector->getTag(i, &meta[i].key, &meta[i].value);
}

void ID3ParserGetSize(ID3ParserHandle h, size_t *size)
{
    ID3Parser *id3_parser = (ID3Parser *)h;
    android::CharacterEncodingDetector *detector = (android::CharacterEncodingDetector *)id3_parser->detector_handle;

    *size = detector->size();
}


const void *ID3ParserGetAlbumArt(ID3ParserHandle h, size_t *length, char **mime)
{
    ID3Parser *id3_parser = (ID3Parser *)h;
    android::ID3 *ID3 = (android::ID3 *)id3_parser->id3_handle;
    android::String8 s;

    const void *data = ID3->getAlbumArt(length, &s);
    *mime = (char *)s.string();

    return data;
}

Version ID3ParserGetVersion(ID3ParserHandle h)
{
    ID3Parser *id3_parser = (ID3Parser *)h;
    android::ID3 *ID3 = (android::ID3 *)id3_parser->id3_handle;
    android::ID3::Version ver = ID3->version();

    Version version_table[] = {ID3_UNKNOWN,
                               ID3_V1,
                               ID3_V1_1,
                               ID3_V2_2,
                               ID3_V2_3,
                               ID3_V2_4,};
    return version_table[ver];
}


/*EOF*/
