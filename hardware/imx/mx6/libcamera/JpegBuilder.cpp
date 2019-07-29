/*
 * Copyright (C) 2008 The Android Open Source Project
 * Copyright (C) 2012-2016 Freescale Semiconductor, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "CameraHAL"

#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>

#include "JpegBuilder.h"

extern "C" {
    #include "jpeglib.h"
    #include "jerror.h"
}

#define ARRAY_SIZE(array) (sizeof((array)) / sizeof((array)[0]))

namespace android {
struct string_pair {
    const char *string1;
    const char *string2;
};

static string_pair degress_to_exif_lut[] = {
    // degrees, exif_orientation
    { "0",   "1"     },
    { "90",  "6"     },
    { "180", "3"     },
    { "270", "8"     },
};

/* public static functions */
const char * JpegBuilder::degreesToExifOrientation(const char *degrees) {
    for (unsigned int i = 0; i < ARRAY_SIZE(degress_to_exif_lut); i++) {
        if (!strcmp(degrees, degress_to_exif_lut[i].string1)) {
            return degress_to_exif_lut[i].string2;
        }
    }
    return NULL;
}

void JpegBuilder::stringToRational(const char   *str,
                                   unsigned int *num,
                                   unsigned int *den) {
    int   len;
    char *tempVal = NULL;

    if (str != NULL) {
        len     = strlen(str);
        tempVal = (char *)malloc(sizeof(char) * (len + 1));
    }

    if (tempVal != NULL) {
        // convert the decimal string into a rational
        size_t den_len;
        char  *ctx;
        unsigned int numerator   = 0;
        unsigned int denominator = 0;
        char *temp               = NULL;

        memset(tempVal, '\0', len + 1);
        strncpy(tempVal, str, len);
        temp = strtok_r(tempVal, ".", &ctx);

        if (temp != NULL)
            numerator = atoi(temp);

        if (!numerator)
            numerator = 1;

        temp = strtok_r(NULL, ".", &ctx);
        if (temp != NULL) {
            den_len = strlen(temp);
            if (HUGE_VAL == den_len) {
                den_len = 0;
            }

            denominator = static_cast<unsigned int>(pow(10, den_len));
            numerator   = numerator * denominator + atoi(temp);
        }
        else {
            denominator = 1;
        }

        free(tempVal);

        *num = numerator;
        *den = denominator;
    }
}

bool JpegBuilder::isAsciiTag(const char *tag) {
    // TODO(XXX): Add tags as necessary
    return (strcmp(tag, TAG_GPS_PROCESSING_METHOD) == 0);
}

void JpegBuilder::insertExifToJpeg(unsigned char *jpeg,
                                   size_t         jpeg_size) {
    ReadMode_t read_mode = (ReadMode_t)(READ_METADATA | READ_IMAGE);

    ResetJpgfile();
    if (ReadJpegSectionsFromBuffer(jpeg, jpeg_size, read_mode)) {
        jpeg_opened = true;
        create_EXIF(table, exif_tag_count, gps_tag_count, has_datetime_tag);
    }
}

status_t JpegBuilder::insertExifThumbnailImage(const char *thumb,
                                               int         len) {
    status_t ret = NO_ERROR;

    if ((len > 0) && jpeg_opened) {
        ret = ReplaceThumbnailFromBuffer(thumb, len);
        FLOGI("insertExifThumbnailImage. ReplaceThumbnail(). ret=%d", ret);
    }

    return ret;
}

void JpegBuilder::saveJpeg(unsigned char *jpeg,
                           size_t         jpeg_size) {
    if (jpeg_opened) {
        WriteJpegToBuffer(jpeg, jpeg_size);
        DiscardData();
        jpeg_opened = false;
    }

    int num_elements = gps_tag_count + exif_tag_count;

    for (int i = 0; i < num_elements; i++) {
        if (table[i].Value) {
            free(table[i].Value);
            table[i].Value = NULL;
        }
    }
    gps_tag_count = 0;
    exif_tag_count = 0;
}

status_t JpegBuilder::insertElement(const char *tag,
                                    const char *value) {
    int value_length = 0;
    status_t ret     = NO_ERROR;

    if (!value || !tag) {
        return -EINVAL;
    }

    if (position >= MAX_EXIF_TAGS_SUPPORTED) {
        FLOGE("Max number of EXIF elements already inserted");
        return NO_MEMORY;
    }

    if (isAsciiTag(tag)) {
        value_length = sizeof(ExifAsciiPrefix) +
                       strlen(value + sizeof(ExifAsciiPrefix));
    }
    else {
        value_length = strlen(value);
    }

    if (IsGpsTag(tag)) {
        table[position].GpsTag = TRUE;
        table[position].Tag    = GpsTagNameToValue(tag);
        gps_tag_count++;
    }
    else {
        table[position].GpsTag = FALSE;
        table[position].Tag    = TagNameToValue(tag);
        exif_tag_count++;

        if (strcmp(tag, TAG_DATETIME) == 0) {
            has_datetime_tag = true;
        }
    }

    table[position].DataLength = 0;
    table[position].Value      = (char *)malloc(sizeof(char) * (value_length + 1));

    if (table[position].Value) {
        memcpy(table[position].Value, value, value_length + 1);
        table[position].DataLength = value_length + 1;
    }

    position++;
    return ret;
}

JpegBuilder::JpegBuilder()
    : gps_tag_count(0), exif_tag_count(0), position(0),
      jpeg_opened(false), has_datetime_tag(false)
{
    reset();
}

void JpegBuilder::reset()
{
    gps_tag_count    = 0;
    exif_tag_count   = 0;
    position         = 0;
    jpeg_opened      = false;
    has_datetime_tag = false;
    mMainInput       = NULL;
    mThumbnailInput  = NULL;
    mCancelEncoding  = false;
    memset(&mEXIFData, 0, sizeof(mEXIFData));
    memset(&table, 0, sizeof(table));
}

JpegBuilder::~JpegBuilder()
{
    int num_elements = gps_tag_count + exif_tag_count;

    for (int i = 0; i < num_elements; i++) {
        if (table[i].Value) {
            free(table[i].Value);
        }
    }

    if (jpeg_opened) {
        DiscardData();
    }
}

void JpegBuilder::prepareImage(const CameraParameters& params)
{
    status_t ret = NO_ERROR;
    int eError   = 0;
    struct timeval sTv;
    struct tm     *pTime;

    if ((NO_ERROR == ret) && (mEXIFData.mModelValid)) {
        ret = insertElement(TAG_MODEL, EXIF_MODEL);
    }

    if ((NO_ERROR == ret) && (mEXIFData.mMakeValid)) {
        ret = insertElement(TAG_MAKE, EXIF_MAKENOTE);
    }

    if ((NO_ERROR == ret)) {
        unsigned int numerator = 0, denominator = 0;
        JpegBuilder::stringToRational(params.get(CameraParameters::
                                                 KEY_FOCAL_LENGTH),
                                      &numerator, &denominator);
        if (numerator || denominator) {
            char temp_value[256]; // arbitrarily long string
            snprintf(temp_value,
                     sizeof(temp_value) / sizeof(char),
                     "%u/%u", numerator, denominator);
            ret = insertElement(TAG_FOCALLENGTH, temp_value);
        }
    }

    if ((NO_ERROR == ret)) {
        int status = gettimeofday(&sTv, NULL);
        pTime = gmtime(&sTv.tv_sec);
        char temp_value[EXIF_DATE_TIME_SIZE + 1];
        if ((0 == status) && (NULL != pTime)) {
            snprintf(temp_value, EXIF_DATE_TIME_SIZE,
                     "%04d:%02d:%02d %02d:%02d:%02d",
                     pTime->tm_year + 1900,
                     pTime->tm_mon + 1,
                     pTime->tm_mday,
                     pTime->tm_hour,
                     pTime->tm_min,
                     pTime->tm_sec);

            ret = insertElement(TAG_DATETIME, temp_value);
        }
    }

    int width, height;
    params.getPictureSize(&width, &height);
    if ((NO_ERROR == ret)) {
        char temp_value[5];
        snprintf(temp_value, sizeof(temp_value) / sizeof(char), "%lu",
                 (unsigned long)width);
        ret = insertElement(TAG_IMAGE_WIDTH, temp_value);
    }

    if ((NO_ERROR == ret)) {
        char temp_value[5];
        snprintf(temp_value, sizeof(temp_value) / sizeof(char), "%lu",
                 (unsigned long)height);
        ret = insertElement(TAG_IMAGE_LENGTH, temp_value);
    }

    if ((NO_ERROR == ret) && (mEXIFData.mGPSData.mLatValid)) {
        char temp_value[256]; // arbitrarily long string
        snprintf(temp_value,
                 sizeof(temp_value) / sizeof(char) - 1,
                 "%d/%d,%d/%d,%d/%d",
                 abs(mEXIFData.mGPSData.mLatDeg), 1,
                 abs(mEXIFData.mGPSData.mLatMin), 1,
                 abs(mEXIFData.mGPSData.mLatSec),
                 abs(mEXIFData.mGPSData.mLatSecDiv));
        ret = insertElement(TAG_GPS_LAT, temp_value);
    }

    if ((NO_ERROR == ret) && (mEXIFData.mGPSData.mLatValid)) {
        ret = insertElement(TAG_GPS_LAT_REF, mEXIFData.mGPSData.mLatRef);
    }

    if ((NO_ERROR == ret) && (mEXIFData.mGPSData.mLongValid)) {
        char temp_value[256]; // arbitrarily long string
        snprintf(temp_value,
                 sizeof(temp_value) / sizeof(char) - 1,
                 "%d/%d,%d/%d,%d/%d",
                 abs(mEXIFData.mGPSData.mLongDeg), 1,
                 abs(mEXIFData.mGPSData.mLongMin), 1,
                 abs(mEXIFData.mGPSData.mLongSec),
                 abs(mEXIFData.mGPSData.mLongSecDiv));
        ret = insertElement(TAG_GPS_LONG, temp_value);
    }

    if ((NO_ERROR == ret) && (mEXIFData.mGPSData.mLongValid)) {
        ret = insertElement(TAG_GPS_LONG_REF, mEXIFData.mGPSData.mLongRef);
    }

    if ((NO_ERROR == ret) && (mEXIFData.mGPSData.mAltitudeValid)) {
        char temp_value[256]; // arbitrarily long string
        snprintf(temp_value,
                 sizeof(temp_value) / sizeof(char) - 1,
                 "%d/%d",
                 abs(mEXIFData.mGPSData.mAltitude), 1);
        ret = insertElement(TAG_GPS_ALT, temp_value);
    }

    if ((NO_ERROR == ret) && (mEXIFData.mGPSData.mAltitudeValid)) {
        char temp_value[5];
        snprintf(temp_value,
                 sizeof(temp_value) / sizeof(char) - 1,
                 "%d", mEXIFData.mGPSData.mAltitudeRef);
        ret = insertElement(TAG_GPS_ALT_REF, temp_value);
    }

    if ((NO_ERROR == ret) && (mEXIFData.mGPSData.mMapDatumValid)) {
        ret = insertElement(TAG_GPS_MAP_DATUM, mEXIFData.mGPSData.mMapDatum);
    }

    if ((NO_ERROR == ret) && (mEXIFData.mGPSData.mProcMethodValid)) {
        char temp_value[GPS_PROCESSING_SIZE];
        memcpy(temp_value, ExifAsciiPrefix, sizeof(ExifAsciiPrefix));
        memcpy(temp_value + sizeof(ExifAsciiPrefix),
               mEXIFData.mGPSData.mProcMethod,
               (GPS_PROCESSING_SIZE - sizeof(ExifAsciiPrefix)));
        ret = insertElement(TAG_GPS_PROCESSING_METHOD, temp_value);
    }

    if ((NO_ERROR == ret) && (mEXIFData.mGPSData.mVersionIdValid)) {
        char temp_value[256]; // arbitrarily long string
        snprintf(temp_value,
                 sizeof(temp_value) / sizeof(char) - 1,
                 "%d,%d,%d,%d",
                 mEXIFData.mGPSData.mVersionId[0],
                 mEXIFData.mGPSData.mVersionId[1],
                 mEXIFData.mGPSData.mVersionId[2],
                 mEXIFData.mGPSData.mVersionId[3]);
        ret = insertElement(TAG_GPS_VERSION_ID, temp_value);
    }

    if ((NO_ERROR == ret) && (mEXIFData.mGPSData.mTimeStampValid)) {
        char temp_value[256]; // arbitrarily long string
        snprintf(temp_value,
                 sizeof(temp_value) / sizeof(char) - 1,
                 "%d/%d,%d/%d,%d/%d",
                 mEXIFData.mGPSData.mTimeStampHour, 1,
                 mEXIFData.mGPSData.mTimeStampMin, 1,
                 mEXIFData.mGPSData.mTimeStampSec, 1);
        ret = insertElement(TAG_GPS_TIMESTAMP, temp_value);
    }

    if ((NO_ERROR == ret) && (mEXIFData.mGPSData.mDatestampValid)) {
        ret = insertElement(TAG_GPS_DATESTAMP, mEXIFData.mGPSData.mDatestamp);
    }

    if ((NO_ERROR == ret) && params.get(CameraParameters::KEY_ROTATION)) {
        const char *exif_orient =
            JpegBuilder::degreesToExifOrientation(params.get(CameraParameters::
                                                             KEY_ROTATION));

        if (exif_orient) {
            ret = insertElement(TAG_ORIENTATION, exif_orient);
        }
    }
}

void JpegBuilder::setParameters(const CameraParameters& params)
{
    status_t ret       = NO_ERROR;
    const char *valstr = NULL;
    double gpsPos;

    if ((valstr = params.get(CameraParameters::KEY_GPS_LATITUDE)) != NULL) {
        gpsPos = strtod(valstr, NULL);

        if (convertGPSCoord(gpsPos,
                            mEXIFData.mGPSData.mLatDeg,
                            mEXIFData.mGPSData.mLatMin,
                            mEXIFData.mGPSData.mLatSec,
                            mEXIFData.mGPSData.mLatSecDiv) == NO_ERROR) {
            if (0 < gpsPos) {
                strncpy(mEXIFData.mGPSData.mLatRef, GPS_NORTH_REF, GPS_REF_SIZE);
            }
            else {
                strncpy(mEXIFData.mGPSData.mLatRef, GPS_SOUTH_REF, GPS_REF_SIZE);
            }

            mEXIFData.mGPSData.mLatValid = true;
        }
        else {
            mEXIFData.mGPSData.mLatValid = false;
        }
    }
    else {
        mEXIFData.mGPSData.mLatValid = false;
    }

    if ((valstr = params.get(CameraParameters::KEY_GPS_LONGITUDE)) != NULL) {
        gpsPos = strtod(valstr, NULL);

        if (convertGPSCoord(gpsPos,
                            mEXIFData.mGPSData.mLongDeg,
                            mEXIFData.mGPSData.mLongMin,
                            mEXIFData.mGPSData.mLongSec,
                            mEXIFData.mGPSData.mLongSecDiv) == NO_ERROR) {
            if (0 < gpsPos) {
                strncpy(mEXIFData.mGPSData.mLongRef, GPS_EAST_REF, GPS_REF_SIZE);
            }
            else {
                strncpy(mEXIFData.mGPSData.mLongRef, GPS_WEST_REF, GPS_REF_SIZE);
            }

            mEXIFData.mGPSData.mLongValid = true;
        }
        else {
            mEXIFData.mGPSData.mLongValid = false;
        }
    }
    else {
        mEXIFData.mGPSData.mLongValid = false;
    }

    if ((valstr = params.get(CameraParameters::KEY_GPS_ALTITUDE)) != NULL) {
        gpsPos                       = strtod(valstr, NULL);
        mEXIFData.mGPSData.mAltitude = floor(fabs(gpsPos));
        if (gpsPos < 0) {
            mEXIFData.mGPSData.mAltitudeRef = 1;
        }
        else {
            mEXIFData.mGPSData.mAltitudeRef = 0;
        }
        mEXIFData.mGPSData.mAltitudeValid = true;
    }
    else {
        mEXIFData.mGPSData.mAltitudeValid = false;
    }

    if ((valstr = params.get(CameraParameters::KEY_GPS_TIMESTAMP)) != NULL) {
        long gpsTimestamp   = strtol(valstr, NULL, 10);
        struct tm *timeinfo = gmtime((time_t *)&(gpsTimestamp));
        if (NULL != timeinfo) {
            mEXIFData.mGPSData.mTimeStampHour  = timeinfo->tm_hour;
            mEXIFData.mGPSData.mTimeStampMin   = timeinfo->tm_min;
            mEXIFData.mGPSData.mTimeStampSec   = timeinfo->tm_sec;
            mEXIFData.mGPSData.mTimeStampValid = true;
        }
        else {
            mEXIFData.mGPSData.mTimeStampValid = false;
        }
    }
    else {
        mEXIFData.mGPSData.mTimeStampValid = false;
    }

    if ((valstr = params.get(CameraParameters::KEY_GPS_TIMESTAMP)) != NULL) {
        long gpsDatestamp   = strtol(valstr, NULL, 10);
        struct tm *timeinfo = gmtime((time_t *)&(gpsDatestamp));
        if (NULL != timeinfo) {
            strftime(mEXIFData.mGPSData.mDatestamp,
                     GPS_DATESTAMP_SIZE,
                     "%Y:%m:%d",
                     timeinfo);
            mEXIFData.mGPSData.mDatestampValid = true;
        }
        else {
            mEXIFData.mGPSData.mDatestampValid = false;
        }
    }
    else {
        mEXIFData.mGPSData.mDatestampValid = false;
    }

    if ((valstr = params.get(CameraParameters::KEY_GPS_PROCESSING_METHOD)) !=
        NULL) {
        memset(mEXIFData.mGPSData.mProcMethod, 0, GPS_PROCESSING_SIZE);
        strncpy(mEXIFData.mGPSData.mProcMethod, valstr, GPS_PROCESSING_SIZE - 1);
        mEXIFData.mGPSData.mProcMethodValid = true;
    }
    else {
        mEXIFData.mGPSData.mProcMethodValid = false;
    }

    mEXIFData.mGPSData.mMapDatumValid  = false;
    mEXIFData.mGPSData.mVersionIdValid = false;
    mEXIFData.mModelValid              = true;
    mEXIFData.mMakeValid               = true;
}

status_t JpegBuilder::encodeImage(JpegParams *mainJpeg,
                                  JpegParams *thumbNail)
{
    status_t ret = NO_ERROR;

    mMainInput      = mainJpeg;
    mThumbnailInput = thumbNail;
    if (thumbNail) {
        ret = encodeJpeg(thumbNail);
    }

    if (ret != NO_ERROR) {
        FLOGE("JpegBuilder:encodeImage failed");
        return ret;
    }

    return encodeJpeg(mainJpeg);
}

status_t JpegBuilder::encodeJpeg(JpegParams *input)
{
    PixelFormat format        = convertStringToPixelFormat(input->format);
    YuvToJpegEncoder *encoder = YuvToJpegEncoder::create(format);

    if (encoder == NULL) {
        return BAD_VALUE;
    }

    int res = 0;
    res = encoder->encode(input->src,
                          input->in_width,
                          input->in_height,
                          input->quality,
                          input->dst,
                          input->dst_size,
                          input->out_width,
                          input->out_height);

    delete encoder;
    if (res) {
        input->jpeg_size = res;
        return NO_ERROR;
    }
    else {
        return BAD_VALUE;
    }
}

size_t JpegBuilder::getImageSize()
{
    size_t jpeg_size, image_size;
    Section_t *exif_section = NULL;

    jpeg_size = mMainInput->jpeg_size;

    exif_section = FindSection(M_EXIF);
    if (exif_section != NULL) {
        image_size = jpeg_size + exif_section->Size;
    }
    else {
        image_size = jpeg_size;
    }
    return image_size;
}

status_t JpegBuilder::buildImage(camera_request_memory get_memory,
                                 camera_memory_t     **image)
{
    size_t   jpeg_size;
    uint8_t *src             = NULL;
    camera_memory_t *picture = NULL;

    if (!image || !mMainInput) {
        return BAD_VALUE;
    }

    jpeg_size = mMainInput->jpeg_size;
    src       = mMainInput->src;

    if (mMainInput->dst && (jpeg_size > 0)) {
        if (position > 0) {
            Section_t *exif_section = NULL;

            insertExifToJpeg((unsigned char *)mMainInput->dst, jpeg_size);

            if (mThumbnailInput) {
                insertExifThumbnailImage((const char *)mThumbnailInput->dst,
                                         (int)mThumbnailInput->jpeg_size);
            }

            exif_section = FindSection(M_EXIF);
            if (exif_section) {
                int imageSize = jpeg_size + exif_section->Size;
                picture = get_memory(-1, imageSize, 1, NULL);
                if (!picture || !picture->data) {
                    FLOGE(
                        "CameraBridge:processImageFrame mRequestMemory picture failed");
                    return false;
                }

                saveJpeg((unsigned char *)picture->data,
                         jpeg_size + exif_section->Size);
            }
        } else {
            int imageSize = jpeg_size;
            picture = get_memory(-1, imageSize, 1, NULL);
            if (!picture || !picture->data) {
                FLOGE(
                    "CameraBridge:processImageFrame mRequestMemory picture failed");
                return false;
            }
            memcpy(picture->data, mMainInput->dst, jpeg_size);
        }
    }

    *image = picture;
    return NO_ERROR;
}

status_t JpegBuilder::getSupportedPictureFormat(int *pFormat,
                                                int  len)
{
    return YuvToJpegEncoder::getSupportedPictureFormat(pFormat, len);
}

status_t JpegBuilder::convertGPSCoord(double coord,
                                      int  & deg,
                                      int  & min,
                                      int  & sec,
                                      int  & secDivisor)
{
    double tmp;

    if (coord == 0) {
        FLOGE("Invalid GPS coordinate");

        return -EINVAL;
    }

    deg        = (int)floor(fabs(coord));
    tmp        = (fabs(coord) - floor(fabs(coord))) * GPS_MIN_DIV;
    min        = (int)floor(tmp);
    tmp        = (tmp - floor(tmp)) * (GPS_SEC_DIV * GPS_SEC_ACCURACY);
    sec        = (int)floor(tmp);
    secDivisor = GPS_SEC_ACCURACY;

    if (sec >= (GPS_SEC_DIV * GPS_SEC_ACCURACY)) {
        sec  = 0;
        min += 1;
    }

    if (min >= 60) {
        min  = 0;
        deg += 1;
    }

    return NO_ERROR;
}
};
