/**
 *  Copyright (C) 2016 Freescale Semiconductor, Inc.
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */
#define LOG_NDEBUG 0
#define LOG_TAG "FslInspector"
#include <utils/Log.h>

#include "include/FslInspector.h"
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/foundation/ABuffer.h>
#include <media/stagefright/foundation/AMessage.h>
#include <media/stagefright/foundation/AUtils.h>
#include <media/stagefright/MediaBuffer.h>
#include <media/stagefright/MediaBufferGroup.h>
#include <media/stagefright/MediaDefs.h>
#include <media/stagefright/MediaSource.h>
#include <media/stagefright/MetaData.h>
#include <utils/String8.h>
#include <utils/RefBase.h>

namespace android {

static bool TryAACADIFType(char* buffer,size_t len,String8 *mimeType,float *confidence)
{
    if(len < 4)
        return false;

    if (buffer[0] == 'A' && buffer[1] == 'D' && buffer[2] == 'I' && buffer[3] == 'F') {
        mimeType->setTo(MEDIA_MIMETYPE_AUDIO_AAC_ADTS);
        *confidence = 0.2f;
        return true;
    }

    return false;
}

static bool TryApeType(char* buffer,size_t len,String8 *mimeType,float *confidence)
{
    if(len < 4)
        return false;
    if (buffer[0] == 'M' && buffer[1] == 'A' && buffer[2] == 'C' && buffer[3] == ' ') {
        mimeType->setTo(MEDIA_MIMETYPE_AUDIO_APE);
        *confidence = 0.2f;
        return true;
    }

    return false;
}
static bool TryAviType(char* buffer,size_t len,String8 *mimeType,float *confidence)
{
    if(len < 12)
        return false;

    if (buffer[0] == 'R' && buffer[1] == 'I' && buffer[2] == 'F' && buffer[3] == 'F' &&
            buffer[8] == 'A' && buffer[9] == 'V' && buffer[10] == 'I' && buffer[11] == ' ' ) {
        mimeType->setTo(MEDIA_MIMETYPE_CONTAINER_AVI);
        *confidence = 0.21f;
        ALOGI("TryAviType SUCCESS");
        return true;
    }

    return false;
}
static bool TryMp4Type(char* buffer,size_t len,String8 *mimeType,float *confidence)
{
    if(len < 8)
        return false;

    if ((buffer[4] == 'f' && buffer[5] == 't' && buffer[6] == 'y' && buffer[7] == 'p')
            || (buffer[4] == 'm' && buffer[5] == 'o' && buffer[6] == 'o' && buffer[7] == 'v')
            || (buffer[4] == 's' && buffer[5] == 'k' && buffer[6] == 'i' && buffer[7] == 'p')
            || (buffer[4] == 'm' && buffer[5] == 'd' && buffer[6] == 'a' && buffer[7] == 't')
            || (buffer[4] == 'w' && buffer[5] == 'i' && buffer[6] == 'd' && buffer[7] == 'e')) {
        mimeType->setTo(MEDIA_MIMETYPE_CONTAINER_MPEG4);
        *confidence = 0.35f;
        ALOGI("TryMp4Type SUCCESS");
        return true;
    }

    return false;
}
static bool TryFlvType(char* buffer,size_t len,String8 *mimeType,float *confidence)
{
    if(len < 8)
        return false;

    if (buffer[0] == 'F' && buffer[1] == 'L' && buffer[2] == 'V'){
        mimeType->setTo(MEDIA_MIMETYPE_CONTAINER_FLV);
        *confidence = 0.25f; // shall larger than SniffMP3's confidence
        ALOGI("TryFlvType SUCCESS");
        return true;
    }

    return false;
}
static bool TryAsfType(char* buffer,size_t len,String8 *mimeType,float *confidence)
{
    if(len < 16)
        return false;
    static const char ASF_GUID[16] = {
        0x30,0x26,0xb2,0x75,0x8e,0x66,0xcf,0x11,0xa6,0xd9,0x0,0xaa,0x0,0x62,0xce,0x6c
    };
    if (!memcmp(buffer, (const char*) ASF_GUID, 16)) {
        mimeType->setTo(MEDIA_MIMETYPE_CONTAINER_ASF);
        *confidence = 0.2f;
        ALOGI("TryAsfType SUCCESS");
        return true;
    }

    return false;
}
static bool TryRmvbType(char* buffer,size_t len,String8 *mimeType,float *confidence)
{
    if(len < 4)
        return false;
    if (buffer[0] == '.' && buffer[1] == 'R' && buffer[2] == 'M' && buffer[3] == 'F') {
        mimeType->setTo(MEDIA_MIMETYPE_CONTAINER_RMVB);
        *confidence = 0.2f;
         ALOGI("TryRmvbType SUCCESS");
        return true;
    }

    return false;
}
#define MPEG_SCAN_BUFFER_SIZE       (128*1024)

#define MPEGTS_HDR_SIZE             (4)

#define MPEGTS_FOUND_MIN_HEADERS    (4)
#define MPEGTS_FOUND_MAX_HEADERS    (10)
#define MPEGTS_MAX_PACKET_SIZE      (208)
#define MPEGTS_MIN_SYNC_SIZE        (MPEGTS_FOUND_MIN_HEADERS * MPEGTS_MAX_PACKET_SIZE)
#define MPEGTS_MAX_SYNC_SIZE        (MPEGTS_FOUND_MAX_HEADERS * MPEGTS_MAX_PACKET_SIZE)
#define MPEGTS_SCAN_LENGTH          (MPEGTS_MAX_SYNC_SIZE * 4)

#define IS_MPEGTS_HEADER(data) (((data)[0] == 0x47) && \
                                (((data)[1] & 0x80) == 0x00) && \
                                (((data)[3] & 0x30) != 0x00))

static char * Peek_data_from_buffer(char* pBuf, int32_t Buf_size, int32_t offset, int32_t peek_size)
{
    char * pPtr = NULL;

    if (offset+peek_size < Buf_size) {
        pPtr = pBuf + offset;
    }

    return pPtr;
}

/* Search ahead at intervals of packet_size for MPEG-TS headers */
static int32_t mpeg_ts_probe_headers(char* pBuf, int32_t Buf_size, int32_t offset, int32_t packet_size)
{
    /* We always enter this function having found at least one header already */
    int32_t found = 1;
    char *data = NULL;

    while (found < MPEGTS_FOUND_MAX_HEADERS) {
        offset += packet_size;

        data = Peek_data_from_buffer(pBuf, Buf_size, offset, MPEGTS_HDR_SIZE);
        if (data == NULL || !IS_MPEGTS_HEADER(data))
            return found;

        found++;
    }

    return found;
}
static bool TryMpegTsType(char * pBuf, size_t buf_size)
{
    /* TS packet sizes to test: normal, DVHS packet size and
    * FEC with 16 or 20 byte codes packet size. */
    int16_t pack_sizes[] = { 188, 192, 204, 208 };
    char *data = NULL;
    int32_t size = 0;
    int64_t skipped = 0;
    int32_t found = 0;

    while (skipped < MPEGTS_SCAN_LENGTH) {
        if (size < MPEGTS_HDR_SIZE) {
            data = Peek_data_from_buffer (pBuf, buf_size, skipped, MPEGTS_MIN_SYNC_SIZE);
            if (!data)
                break;
            size = MPEGTS_MIN_SYNC_SIZE;
        }

        /* Have at least MPEGTS_HDR_SIZE bytes at this point */
        if (IS_MPEGTS_HEADER (data)) {
            //LOG_DEBUG ("possible mpeg-ts sync at offset %lld", skipped);
            for (int32_t p = 0; p < 4; p++) {
                /* Probe ahead at size pack_sizes[p] */
                found = mpeg_ts_probe_headers (pBuf, buf_size, skipped, pack_sizes[p]);
                if (found >= MPEGTS_FOUND_MIN_HEADERS) {
                    return true;
                }
            }
        }
        data++;
        skipped++;
        size--;
    }

    return false;
}
#define IS_MPEG_HEADER(data) ( (((char *)(data))[0] == 0x00) &&  \
                                (((char *)(data))[1] == 0x00) &&  \
                                (((char *)(data))[2] == 0x01) )

#define IS_MPEG_PACK_CODE(b)    ((b) == 0xBA)
#define IS_MPEG_SYS_CODE(b)     ((b) == 0xBB)
#define IS_MPEG_PACK_HEADER(data)  (IS_MPEG_HEADER(data) && IS_MPEG_PACK_CODE(((char *)(data))[3]))

#define IS_MPEG_PES_CODE(b)     (((b) & 0xF0) == 0xE0 || ((b) & 0xF0) == 0xC0 || (b) >= 0xBD)
#define IS_MPEG_PES_HEADER(data) (IS_MPEG_HEADER(data) && IS_MPEG_PES_CODE(((char *)(data))[3]))

#define MPEG2_MAX_PROBE_LENGTH  (128 * 1024)  /* 128kB should be 64 packs of the most common 2kB pack size. */

#define MPEG2_MIN_SYS_HEADERS   (2)
#define MPEG2_MAX_SYS_HEADERS   (5)

static status_t mpeg_sys_is_valid_pack(char * data, uint32_t len, uint32_t * pack_size)
{
    /* Check the pack header @ offset for validity, assuming that the 4 byte header
    * itself has already been checked. */
    uint32_t stuff_len;

    if (len < 12)
        return UNKNOWN_ERROR;

    /* Check marker bits */
    if ((data[4] & 0xC4) == 0x44) {
        /* MPEG-2 PACK */
        if (len < 14)
            return UNKNOWN_ERROR;

        if ((data[6] & 0x04) != 0x04 ||
            (data[8] & 0x04) != 0x04 ||
            (data[9] & 0x01) != 0x01 || (data[12] & 0x03) != 0x03)
            return UNKNOWN_ERROR;

        stuff_len = data[13] & 0x07;

        /* Check the following header bytes, if we can */
        if ((14 + stuff_len + 4) <= len) {
            if (!IS_MPEG_HEADER (data + 14 + stuff_len))
                return UNKNOWN_ERROR;
        }
        if (pack_size)
            *pack_size = 14 + stuff_len;
        return OK;
    } else if ((data[4] & 0xF1) == 0x21) {
        /* MPEG-1 PACK */
        if ((data[6] & 0x01) != 0x01 ||
            (data[8] & 0x01) != 0x01 ||
            (data[9] & 0x80) != 0x80 || (data[11] & 0x01) != 0x01)
            return UNKNOWN_ERROR;

        /* Check the following header bytes, if we can */
        if ((12 + 4) <= len) {
            if (!IS_MPEG_HEADER (data + 12))
                return UNKNOWN_ERROR;
        }
        if (pack_size)
            *pack_size = 12;
        return OK;
    }

    return UNKNOWN_ERROR;
}

static status_t mpeg_sys_is_valid_pes(char * data, uint32_t len, uint32_t * pack_size)
{
    uint32_t pes_packet_len;

    /* Check the PES header at the given position, assuming the header code itself
    * was already checked */
    if (len < 6)
        return UNKNOWN_ERROR;

    /* For MPEG Program streams, unbounded PES is not allowed, so we must have a
    * valid length present */
    pes_packet_len = data[4]; pes_packet_len <<=8;
    pes_packet_len += data[5];
    if (pes_packet_len == 0)
        return UNKNOWN_ERROR;

    /* Check the following header, if we can */
    if (6 + pes_packet_len + 4 <= len) {
        if (!IS_MPEG_HEADER(data + 6 + pes_packet_len))
            return UNKNOWN_ERROR;
    }

    if (pack_size)
        *pack_size = 6 + pes_packet_len;
    return OK;
}

static status_t mpeg_sys_is_valid_sys(char * data, uint32_t len, uint32_t * pack_size)
{
    uint32_t sys_hdr_len;

    /* Check the System header at the given position, assuming the header code itself
    * was already checked */
    if (len < 6)
        return UNKNOWN_ERROR;
    sys_hdr_len = data[4]; sys_hdr_len <<=8;
    sys_hdr_len += data[5];

    if (sys_hdr_len < 6)
        return UNKNOWN_ERROR;

    /* Check the following header, if we can */
    if (6 + sys_hdr_len + 4 <= len) {
        if (!IS_MPEG_HEADER(data + 6 + sys_hdr_len))
            return UNKNOWN_ERROR;
    }

    if (pack_size)
        *pack_size = 6 + sys_hdr_len;

    return OK;
}

/* calculation of possibility to identify random data as MPEG System Stream:
* bits that must match in header detection:            32 (or more)
* chance that random data is identified:                1/2^32
* chance that MPEG2_MIN_PACK_HEADERS headers are identified:
*       1/2^(32*MPEG2_MIN_PACK_HEADERS)
* chance that this happens in MPEG2_MAX_PROBE_LENGTH bytes:
*       1-(1+1/2^(32*MPEG2_MIN_PACK_HEADERS)^MPEG2_MAX_PROBE_LENGTH)
* for current values:
*       1-(1+1/2^(32*4)^101024)
*       = <some_number>
* Since we also check marker bits and pes packet lengths, this probability is a
* very coarse upper bound.
*/
static bool TryMpegSysType(char * pBuf, int32_t buf_size)
{
    char *data, *data0, *first_sync, *end;
    int32_t mpegversion = 0;
    uint32_t pack_headers = 0;
    uint32_t pes_headers = 0;
    uint32_t pack_size;
    uint32_t since_last_sync = 0;
    uint32_t sync_word = 0xffffffff;

    {
        int32_t len;

        len = MPEG2_MAX_PROBE_LENGTH ;
        do {
            data = Peek_data_from_buffer (pBuf, buf_size, 0, 5 + len);
            len = len - 4096;
        } while (data == NULL && len >= 32);

        if (!data)
            return false;

        end = data + len;
    }

    data0 = data;
    first_sync = NULL;

    while (data < end) {
        sync_word <<= 8;
        if (sync_word == 0x00000100) {
            /* Found potential sync word */
            if (first_sync == NULL)
                first_sync = data - 3;

            if (since_last_sync > 4) {
                /* If more than 4 bytes since the last sync word, reset our counters,
                * as we're only interested in counting contiguous packets */
                pes_headers = pack_headers = 0;
            }
            pack_size = 0;

            if (IS_MPEG_PACK_CODE (data[0])) {
                if ((data[1] & 0xC0) == 0x40) {
                    /* MPEG-2 */
                    mpegversion = 2;
                } else if ((data[1] & 0xF0) == 0x20) {
                    mpegversion = 1;
                }
                if (mpegversion != 0 &&
                    OK == mpeg_sys_is_valid_pack(data - 3, end - data + 3, &pack_size)) {
                        pack_headers++;
                }
            } else if (IS_MPEG_PES_CODE (data[0])) {
                /* PES stream */
                if (OK == mpeg_sys_is_valid_pes(data - 3, end - data + 3, &pack_size)) {
                    pes_headers++;
                    if (mpegversion == 0)
                        mpegversion = 2;
                }
            } else if (IS_MPEG_SYS_CODE (data[0])) {
                if (OK == mpeg_sys_is_valid_sys(data - 3, end - data + 3, &pack_size)) {
                    pack_headers++;
                }
            }

            /* If we found a packet with a known size, skip the bytes in it and loop
            * around to check the next packet. */
            if (pack_size != 0) {
                data += pack_size - 3;
                sync_word = 0xffffffff;
                since_last_sync = 0;
                continue;
            }
        }

        sync_word |= data[0];
        since_last_sync++;
        data++;

        /* If we have found MAX headers, and *some* were pes headers (pack headers
        * are optional in an mpeg system stream) then return our high-probability
        * result */
        if (pes_headers > 0 && (pack_headers + pes_headers) > MPEG2_MAX_SYS_HEADERS)
            goto suggest;
    }

    /* If we at least saw MIN headers, and *some* were pes headers (pack headers
    * are optional in an mpeg system stream) then return a lower-probability
    * result */
    if (pes_headers > 0 && (pack_headers + pes_headers) >= MPEG2_MIN_SYS_HEADERS)
        goto suggest;

    return false;
suggest:

    return true;
};



static bool TryMpegType(const sp<DataSource> &source,String8 *mimeType,float *confidence)
{

    off64_t buffer_size = MPEG_SCAN_BUFFER_SIZE;

    off64_t size;
    if(source->getSize(&size) == OK){
        if(size < buffer_size)
            buffer_size = size;
    }

    void * buf= malloc(buffer_size);
    if(buf == NULL)
        return false;

    if (source->readAt(0, buf, buffer_size) < buffer_size) {
        free(buf);
        return false;
    }

    if(TryMpegTsType((char*)buf, buffer_size)){
        mimeType->setTo(MEDIA_MIMETYPE_CONTAINER_MPEG2TS);
        *confidence = 0.09l;
        free(buf);
        ALOGI("TryMpegType TS SUCCESS");
        return true;
    }else if(TryMpegSysType((char*)buf, buffer_size)){
        mimeType->setTo(MEDIA_MIMETYPE_CONTAINER_MPEG2PS);
        *confidence = 0.25; // slightly larger than mp3 extractor, keep alin with MPEG2PSExtractor
        free(buf);
        ALOGI("TryMpegType PS SUCCESS");
        return true;
    }

    free(buf);
    return false;
}
typedef bool (*TRYTYPEFUNC)(char* buffer,size_t len,String8 *mimeType,float *confidence);

static TRYTYPEFUNC TryFunc[] = {
    TryApeType,
    TryAviType,
    TryMp4Type,
    TryFlvType,
    TryAsfType,
    TryRmvbType,
    TryAACADIFType,
};
bool SniffFSL(
        const sp<DataSource> &source, String8 *mimeType, float *confidence,
        sp<AMessage> *)
{
    char tmp[16];
    if (source->readAt(0, tmp, 16) < 16) {
        return false;
    }

    for(size_t i=0; i<sizeof(TryFunc)/sizeof(TRYTYPEFUNC); i++) {
        if((*TryFunc[i])(&tmp[0], 16,mimeType,confidence))
            return true;
    }

    if(TryMpegType(source,mimeType,confidence))
        return true;

    return false;
}
}
