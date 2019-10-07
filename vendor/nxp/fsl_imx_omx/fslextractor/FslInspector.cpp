/**
 *  Copyright (C) 2016 Freescale Semiconductor, Inc.
 *  Copyright 2018-2019 NXP
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */
//#define LOG_NDEBUG 0
#define LOG_TAG "FslInspector"
#include <utils/Log.h>

#include "FslInspector.h"
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/foundation/ABuffer.h>
#include <media/stagefright/foundation/AMessage.h>
#include <media/stagefright/foundation/AUtils.h>
#include <media/stagefright/MediaBuffer.h>
#include <media/stagefright/MediaBufferGroup.h>
#include <media/stagefright/MediaDefs.h>
#include <media/stagefright/MediaDefsExt.h>
#include <media/MediaSource.h>
#include <media/stagefright/MetaData.h>
#include <utils/String8.h>
#include <utils/RefBase.h>
#include <media/stagefright/foundation/avc_utils.h>
#include <media/stagefright/foundation/ByteUtils.h>

#define FSL_EBML_ID_HEADER             0x1A45DFA3
#define EBML_BYTE0 0x1A

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
    if(len < 12)
        return false;

    // ignore HEIF format, let google's mp4 parser to handle it
    if (buffer[4] == 'f' && buffer[5] == 't' && buffer[6] == 'y' && buffer[7] == 'p'){
        if((buffer[8] == 'm' && buffer[9] == 'i' && buffer[10] == 'f' && buffer[11] == '1')
            || (buffer[8] == 'h' && buffer[9] == 'e' && buffer[10] == 'i' && buffer[11] == 'c')
            || (buffer[8] == 'm' && buffer[9] == 's' && buffer[10] == 'f' && buffer[11] == '1')
            || (buffer[8] == 'h' && buffer[9] == 'e' && buffer[10] == 'v' && buffer[11] == 'c')
            )
        {
            return false;
        }
    }

    if ((buffer[4] == 'f' && buffer[5] == 't' && buffer[6] == 'y' && buffer[7] == 'p')
            || (buffer[4] == 'm' && buffer[5] == 'o' && buffer[6] == 'o' && buffer[7] == 'v')
            || (buffer[4] == 's' && buffer[5] == 'k' && buffer[6] == 'i' && buffer[7] == 'p')
            || (buffer[4] == 'm' && buffer[5] == 'd' && buffer[6] == 'a' && buffer[7] == 't')
            || (buffer[4] == 'w' && buffer[5] == 'i' && buffer[6] == 'd' && buffer[7] == 'e')) {
        mimeType->setTo(MEDIA_MIMETYPE_CONTAINER_MPEG4);
        *confidence = 0.45f; // shall be larger than google's confidence 0.4
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

static bool TryDsfType(char* buffer,size_t len,String8 *mimeType,float *confidence)
{
    //ALOGI("TryDsfType %c%c%c%c", buffer[0], buffer[1], buffer[2], buffer[3]);
    if(len < 4)
        return false;
    if (buffer[0] == 'D' && buffer[1] == 'S' && buffer[2] == 'D' && buffer[3] == ' ') {
        mimeType->setTo(MEDIA_MIMETYPE_CONTAINER_DSF);
        *confidence = 0.2f;
         ALOGI("TryDsfType SUCCESS");
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

#define MPEG_PES_ID_MIN         0xBD
#define MPEG_PES_ID_MAX         0xFF
#define IS_MPEG_PES_ID(b)       ((b)>= MPEG_PES_ID_MIN && (b)<= MPEG_PES_ID_MAX)
#define IS_MPEG_SC_PREFIX(data) ( (((char *)(data))[0] == 0x00) &&  \
                                (((char *)(data))[1] == 0x00) &&  \
                                (((char *)(data))[2] == 0x01) )

#define IS_PS_PACK_ID(b)        ((b) == 0xBA)
#define IS_PS_SYS_HEADER_ID(b)  ((b) == 0xBB)


#define IS_PS_PACK_HEADER(data) (IS_MPEG_SC_PREFIX(data) && IS_PS_PACK_ID(((char *)(data))[3]))

#define IS_MPEG_PES_HEADER(data) (IS_MPEG_SC_PREFIX(data) && IS_MPEG_PES_ID(((char *)(data))[3]))

#define MPEG2_MAX_PROBE_SIZE    (131072)  /* 128 * 1024 = 128kB should be 64 packs of the most common 2kB pack size. */

#define MPEG2_MIN_SYS_HEADERS   (2)
#define MPEG2_MAX_SYS_HEADERS   (5)

/*
 *  MPEG-1 Pack Header                                 bit
 *  pack_start_code                                     32
 *  '0010'                                               4
 *  system_clock_reference_base [32..30]                 3
 *  marker_bit (the value is  '1')                       1
 *  system_clock_reference_base [29..15]                15
 *  marker_bit                                           1
 *  system_clock_reference_base [14..0]                 15
 *  marker_bit                                           1
 *  marker_bit                                           1                                              1
 *  program_mux_rate                                    22
 *  marker_bit                                           1
 *  if (nextbits() = = system_header_start_code) {
 *      system_header ()
 *  }
 */


/*
 *  MPEG-2 Pack Header                                 bit
 *  pack_start_code                                     32
 *  '01'                                                 2
 *  system_clock_reference_base [32..30]                 3
 *  marker_bit (the value is  '1')                       1
 *  system_clock_reference_base [29..15]                15
 *  marker_bit                                           1
 *  system_clock_reference_base [14..0]                 15
 *  marker_bit                                           1
 *  system_clock_reference_extension                     9
 *  marker_bit                                           1
 *  program_mux_rate                                    22
 *  marker_bit                                           1
 *  marker_bit                                           1
 *  reserved                                             5
 *  pack_stuffing_length                                 3
 *  for (i = 0; i < pack_stuffing_length; i++) {
 *       stuffing_byte                                   8
 *  }
 *  if (nextbits() = = system_header_start_code) {
 *      system_header ()
 *  }
 */

static status_t is_mpeg_sys_pack_valid(char * data, uint32_t len, uint32_t * pack_size)
{
    // check the rest bytes after pack start code, the pack start code should already be check as valid.
    uint32_t pack_stuffing_len;

    if (len < 12)
        return UNKNOWN_ERROR;

    if ((data[4] & 0xF0) == 0x20) {
        // find '0010', it is MPEG-1 PACK, now check the marker bits
        if ((data[4] & 0x01) != 0x01 || (data[6] & 0x01) != 0x01 || \
            (data[8] & 0x01) != 0x01 || (data[9] & 0x80) != 0x80 || (data[11] & 0x01) != 0x01)
            return UNKNOWN_ERROR;

        if (len >= 16 && !IS_MPEG_SC_PREFIX(data + 12))
            return UNKNOWN_ERROR; //check next pack start code prefix failed

        if (pack_size)
            *pack_size = 12;
        return OK;

    } else if ((data[4] & 0xC0) == 0x40) {
        // find '01', it is MPEG-2 PACK, now check the marker bits
        if ((data[4] & 0x04) != 0x04 || (data[6] & 0x04) != 0x04 || \
            (data[8] & 0x04) != 0x04 || (data[9] & 0x01) != 0x01 || (data[12] & 0x03) != 0x03)
            return UNKNOWN_ERROR;

        pack_stuffing_len = data[13] & 0x07;
        if ((len >= pack_stuffing_len + 18) && !IS_MPEG_SC_PREFIX(data + pack_stuffing_len + 14))
            return UNKNOWN_ERROR; //check next pack start code prefix failed

        if (pack_size)
            *pack_size = pack_stuffing_len + 14;
        return OK;
    }

    return UNKNOWN_ERROR;
}

/*
 *  PES Packet                                     bit
 *  packet_start_code_prefix                        24
 *  directory_stream_id                              8
 *  PES_packet_length                               16
 *  if (nextbits() == packet_start_code_prefix) {
 *      pes_packet()
 *  }
 */

static status_t is_mpeg_sys_pes_valid(char * data, uint32_t len, uint32_t * pack_size)
{
    // check the rest bytes after packet_start_code, the packet_start_code should already be check as valid.
    uint32_t pes_packet_len;

    if (len < 6)
        return UNKNOWN_ERROR;

    pes_packet_len = (data[4] << 8) | data[5];

    if (pes_packet_len == 0 || ((len >= pes_packet_len + 10) && !IS_MPEG_SC_PREFIX(data + 6 + pes_packet_len)))
        return UNKNOWN_ERROR;

    if (pack_size)
        *pack_size = 6 + pes_packet_len;
    return OK;
}

/*
 *  System Header                                     bit
 *  system_header_start_code                           32
 *  header_length                                      16
 *  marker_bit                                          1
 *  rate_bound                                         22
 *  marker_bit                                          1
 *  audio_bound                                         6
 *  fixed_flag                                          1
 *  CSPS_flag                                           1
 *  system_audio_lock_flag                              1
 *  system_video_lock_flag                              1
 *  marker_bit                                          1
 *  video_bound                                         5
 *  packet_rate_restriction_flag                        1
 *  reserved_bits                                       7
 *  while (nextbits () == '1') {
 *      stream_id                                       8
 *      '11'                                            2
 *      P-STD_buffer_bound_scale                        1
 *      P-STD_buffer_size_bound                        13
 *  }
 */

static status_t is_mpeg_sys_sys_valid(char * data, uint32_t len, uint32_t * pack_size)
{
    // check the rest bytes after system_header_start_code, the system_header_start_code should already be check as valid.
    uint32_t sys_header_len;

    if (len < 6)
        return UNKNOWN_ERROR;

    sys_header_len = (data[4] << 8) | data[5];

    if (sys_header_len < 6 || ((len >= sys_header_len + 10) && !IS_MPEG_SC_PREFIX(data + sys_header_len + 6)))
        return UNKNOWN_ERROR;

    if (pack_size)
        *pack_size = 6 + sys_header_len;

    return OK;
}

/*
 * Estimate Probability of Identify Random Data As MPEG System Stream
 * 1. Check start code(32 bit): (1/2)^32
 * 2. Search MPEG2_MIN_SYS_HEADERS headers at least: (1/2)^32^(MPEG2_MIN_SYS_HEADERS+1)
 * 3. Probability of identify random data in MPEG2_MAX_PROBE_SIZE bytes:
 *        (MPEG2_MAX_PROBE_SIZE - 4*(MPEG2_MIN_SYS_HEADERS+1))*8/((1/2)^32^(MPEG2_MIN_SYS_HEADERS+1))
 * 4. MPEG2_MAX_PROBE_SIZE is 131072, MPEG2_MIN_SYS_HEADERS is 2, so probability is:
 *        (131072 - 4*(2+1))*8/((1/2)^32^3)
 *    the value is about the (1/2)^76
 * 5. Since we also check marker bit and header length, the actual probability is smaller than this value.
 */
static bool TryMpegSysType(char * pBuf, int32_t buf_size)
{
    char *data, *end;
    uint32_t pack_headers = 0;
    uint32_t pes_headers = 0;
    uint32_t pack_size;
    uint32_t since_last_sync = 0;
    int32_t len;

    len = MPEG2_MAX_PROBE_SIZE;
    do {
        data = Peek_data_from_buffer (pBuf, buf_size, 0, 5 + len);
        len -= 4096;
    } while (data == NULL && len >= 32);

    if (!data)
        return false;

    end = data + len;

    while (data + 4 < end){
        if (IS_MPEG_SC_PREFIX(data)) {
            if (since_last_sync > 4)
                pes_headers = pack_headers = 0; // only count continuous pes_headers/pack_headers

            pack_size = 0;
            if (IS_PS_PACK_ID(data[3])) {
                if (OK == is_mpeg_sys_pack_valid(data, end - data, &pack_size))
                    pack_headers++;
            } else if (IS_MPEG_PES_ID(data[3])) {
                if (OK == is_mpeg_sys_pes_valid(data, end - data, &pack_size)) {
                    pes_headers++;
                }
            } else if (IS_PS_SYS_HEADER_ID(data[3])) {
                if (OK == is_mpeg_sys_sys_valid(data, end - data, &pack_size))
                    pack_headers++;
            }

            if (pack_size > 0) {
                data += pack_size;
                since_last_sync = 0;

                if (pes_headers > 0 && (pack_headers + pes_headers) > MPEG2_MAX_SYS_HEADERS)
                    return true;
                else
                    continue;
            }
        }
        since_last_sync++;
        data++;
    }
    // check pes_headers/pack_headers, pack headers are optional
    if (pes_headers > 0 && (pack_headers + pes_headers) >= MPEG2_MIN_SYS_HEADERS)
        return true;

    return false;
};



static bool TryMpegType(DataSourceBase *source,String8 *mimeType,float *confidence)
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

static bool TryFlacType(DataSourceBase *source, String8 *mimeType, float *confidence)
{
    char buf[10];
    int size = sizeof(buf)/sizeof(buf[0]);
    int syncWordSize = 4;  //'fLaC'

    if (source->readAt(0, buf, size) < size)
        return false;

    if (buf[0] == 'I' && buf[1] == 'D' && buf[2] == '3') {
        // Skip the ID3v2 header.
        uint32_t Id3Size;
        uint32_t len = ((buf[6] & 0x7f) << 21)
                    | ((buf[7] & 0x7f) << 14)
                    | ((buf[8] & 0x7f) << 7)
                    | (buf[9] & 0x7f);

        if (len > 3 * 1024 * 1024)
            len = 3 * 1024 * 1024;

        len += 10;
        Id3Size = len;

        memset(buf, 0, sizeof(buf));
        if (source->readAt(Id3Size, buf, syncWordSize) < syncWordSize)
            return false;
    }

    if (buf[0] == 'f' && buf[1] == 'L' && buf[2] == 'a' && buf[3] == 'C') {
        *mimeType = MEDIA_MIMETYPE_AUDIO_FLAC;
        *confidence = 0.5;
        return true;
    }
    return false;
}

static bool TryMkv(DataSourceBase *source, String8 *mimeType, float *confidence)
{
    long long offset = 0;
    unsigned char scanByte = 0;
    off64_t size = 0;
    ssize_t n = 0;

    source->getSize(&size);
    const long long maxScanLen = (size >= 1024) ? 1024 : size;

    while (offset < maxScanLen) {
        n = source->readAt(offset, &scanByte, 1);
        if (n <= 0)
            return false;

        if (scanByte == EBML_BYTE0)
            break;

        offset++;
    }

    n = source->readAt(offset, &scanByte, 1);
    if (n <= 0)
        return false;

    // search EBML ID
    int pos;
    bool found = false;
    for (pos = 0; pos < 4; pos++) {
        if ((0x80 >> pos) & scanByte) {
            found = true;
            break;
        }
    }

    if (!found)
        return false;

    long long header = scanByte;
    const int len = pos + 1;

    for (int i = 1; i < len; i++) {
        header <<= 8;
        n = source->readAt(offset + i, &scanByte, 1);
        if (n <= 0)
            return false;

        header |= scanByte;
    }

    if (len != 4 || header != FSL_EBML_ID_HEADER)
        return false;

    ALOGI("TryMkv SUCCESS");
    *mimeType = MEDIA_MIMETYPE_CONTAINER_MATROSKA;
    *confidence = 0.6;
    return true;

}

static const uint32_t kMask = 0xfffe0c00;

static bool DetectAudioTypeBySource(DataSourceBase *source, bool check_aac)
{
    off64_t inout_pos = 0;
    off64_t post_id3_pos = 0;

    if (source == NULL)
        return false;

    //Skip ID3 header
    for (;;) {
        uint8_t id3header[10];
        if (source->readAt(inout_pos, id3header, sizeof(id3header))
                < (ssize_t)sizeof(id3header)) {
            // If we can't even read these 10 bytes, we might as well bail
            // out, even if there _were_ 10 bytes of valid mp3 audio data...
            return false;
        }

        if (memcmp("ID3", id3header, 3)) {
            break;
        }

        // Skip the ID3v2 header.

        size_t len =
            ((id3header[6] & 0x7f) << 21)
            | ((id3header[7] & 0x7f) << 14)
            | ((id3header[8] & 0x7f) << 7)
            | (id3header[9] & 0x7f);

        if (len > 3 * 1024 * 1024)
            len = 3 * 1024 * 1024;

        len += 10;

        inout_pos += len;
    }

    post_id3_pos = inout_pos;

    off64_t pos = inout_pos;
    if(check_aac){
        uint8_t header[2];

        if (source->readAt(pos, &header, 2) != 2) {
            return false;
        }

        if ((header[0] == 0xff) && ((header[1] & 0xf6) == 0xf0)) {
            return true;
        }
        return false;
    }


    bool valid = false;

    const size_t kMaxReadBytes = 1024;
    const size_t kMaxBytesChecked = 128 * 1024;
    uint8_t buf[kMaxReadBytes];
    ssize_t bytesToRead = kMaxReadBytes;
    ssize_t totalBytesRead = 0;
    ssize_t remainingBytes = 0;
    bool reachEOS = false;
    uint8_t *tmp = buf;

    do {
        if (pos >= (off64_t)(inout_pos + kMaxBytesChecked)) {
            // Don't scan forever.
            ALOGV("giving up at offset %lld", (long long)pos);
            break;
        }

        if (remainingBytes < 4) {
            if (reachEOS) {
                break;
            } else {
                memcpy(buf, tmp, remainingBytes);
                bytesToRead = kMaxReadBytes - remainingBytes;

                /*
                 * The next read position should start from the end of
                 * the last buffer, and thus should include the remaining
                 * bytes in the buffer.
                 */
                totalBytesRead = source->readAt(pos + remainingBytes,
                                                buf + remainingBytes,
                                                bytesToRead);
                if (totalBytesRead <= 0) {
                    break;
                }
                reachEOS = (totalBytesRead != bytesToRead);
                totalBytesRead += remainingBytes;
                remainingBytes = totalBytesRead;
                tmp = buf;
                continue;
            }
        }

        uint32_t header = U32_AT(tmp);

        size_t frame_size;
        int sample_rate, num_channels, bitrate;
        if (!GetMPEGAudioFrameSize(
                    header, &frame_size,
                    &sample_rate, &num_channels, &bitrate)) {
            ++pos;
            ++tmp;
            --remainingBytes;
            continue;
        }

        ALOGV("found possible 1st frame at %lld (header = 0x%08x)", (long long)pos, header);

        // We found what looks like a valid frame,
        // now find its successors.

        off64_t test_pos = pos + frame_size;

        valid = true;
        for (int j = 0; j < 3; ++j) {
            uint8_t tmp[4];
            if (source->readAt(test_pos, tmp, 4) < 4) {
                valid = false;
                break;
            }

            uint32_t test_header = U32_AT(tmp);

            ALOGV("subsequent header is %08x", test_header);

            if ((test_header & kMask) != (header & kMask)) {
                valid = false;
                break;
            }

            size_t test_frame_size;
            if (!GetMPEGAudioFrameSize(
                        test_header, &test_frame_size)) {
                valid = false;
                break;
            }

            ALOGV("found subsequent frame #%d at %lld", j + 2, (long long)test_pos);

            test_pos += test_frame_size;
        }

        if (valid) {
           inout_pos = pos;
        } else {
            ALOGV("no dice, no valid sequence of frames found.");
        }

        ++pos;
        ++tmp;
        --remainingBytes;
    } while (!valid);

    return valid;

}

static bool TryMp3Type(DataSourceBase *source,String8 *mimeType,float *confidence)
{
    if(DetectAudioTypeBySource(source, false)){
        *mimeType = MEDIA_MIMETYPE_AUDIO_MPEG;
        *confidence = 0.2f;
        return true;
    }

    return false;
}
static bool TryADTSType(DataSourceBase *source,String8 *mimeType,float *confidence)
{
    if(DetectAudioTypeBySource(source, true)){
        *mimeType = MEDIA_MIMETYPE_AUDIO_AAC_ADTS;
        *confidence = 0.2f;
        return true;
    }

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
    TryDsfType,
};
bool SniffFSL(
        DataSourceBase *source, String8 *mimeType, float *confidence,
        sp<AMessage> *)
{
    char tmp[16];
    if (source->readAt(0, tmp, 16) < 16) {
        ALOGE("SniffFSL read datasource fail!");
        return false;
    }

    for(size_t i=0; i<sizeof(TryFunc)/sizeof(TRYTYPEFUNC); i++) {
        if((*TryFunc[i])(&tmp[0], 16,mimeType,confidence))
            return true;
    }

    if(TryMpegType(source,mimeType,confidence))
        return true;
    else if (TryFlacType(source,mimeType,confidence))
        return true;
    else if (TryMkv(source,mimeType,confidence))
        return true;
    else if (TryMp3Type(source,mimeType,confidence))
        return true;
    else if (TryADTSType(source,mimeType,confidence))
        return true;

    return false;
}
}
