/**
 *  Copyright (c) 2010-2016, Freescale Semiconductor Inc.,
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */

#include <OMXMediaScanner.h>
#include <media/mediametadataretriever.h>
#include <private/media/VideoFrame.h>
#undef OMX_MEM_CHECK
#include "Mem.h"
#include "Log.h"
// Sonivox includes
#include <libsonivox/eas.h>

namespace android {

OMXMediaScanner::OMXMediaScanner() {}

OMXMediaScanner::~OMXMediaScanner() {}

static bool KnownFileExtension(const char *extension) {
    bool ret = false;
    static const char *validFileExt[] = {
		".avi", ".divx", ".mp3", ".aac", ".bsac", ".ac3", ".wmv",
		".wma", ".asf", ".rm", ".rmvb", ".ra", ".3gp", ".3gpp", ".3g2",
		".mp4", ".mov", ".m4v", ".m4a", ".flac", ".wav", ".mpg", ".ts",
		".vob", ".ogg", ".mkv", ".f4v", ".flv", ".webm", ".out", ".amr", ".awb",
        ".mid", ".smf", ".imy", ".midi", ".xmf", ".rtttl", ".rtx", ".ota", ".mxmf"
    };
    static const size_t validFileExtNum = sizeof(validFileExt) / sizeof(validFileExt[0]);

    for (size_t i = 0; i < validFileExtNum; ++i) {
        if (!strcasecmp(validFileExt[i], extension)) {
            ret = true;
        }
    }
	return ret;
}

#if (ANDROID_VERSION <= HONEY_COMB)
static status_t HandleMIDI(
        const char *filename, MediaScannerClient *client) {
#elif (ANDROID_VERSION >= ICS)
static MediaScanResult HandleMIDI(
        const char *filename, MediaScannerClient *client) {
#endif

    const S_EAS_LIB_CONFIG* pLibConfig = EAS_Config();
    if (NULL == pLibConfig || pLibConfig->libVersion != LIB_VERSION) {
        LOG_ERROR("EAS library/header mismatch\n");
#if (ANDROID_VERSION <= HONEY_COMB)
        return NO_MEMORY;
#elif (ANDROID_VERSION >= ICS)
        return MEDIA_SCAN_RESULT_ERROR;
#endif
    }

    EAS_I32 duation;
    EAS_DATA_HANDLE dataHandle = NULL;
    EAS_HANDLE handle = NULL;
    EAS_FILE file;
    EAS_RESULT res;

    do {
        res = EAS_Init(&dataHandle);
        if (res != EAS_SUCCESS)
            break;

        file.path = filename;
        file.length = 0;
        file.offset = 0;
        file.fd = 0;

        res = EAS_OpenFile(dataHandle, &file, &handle);
        if (res != EAS_SUCCESS)
            break;

        res = EAS_Prepare(dataHandle, handle);
        if (res != EAS_SUCCESS)
            break;

        res = EAS_ParseMetaData(dataHandle, handle, &duation);
        if (res != EAS_SUCCESS)
            break;
    } while (0);

    if (handle) {
        EAS_CloseFile(dataHandle, handle);
    }
    if (dataHandle) {
        EAS_Shutdown(dataHandle);
    }

    if (res != EAS_SUCCESS) {
#if (ANDROID_VERSION <= HONEY_COMB)
        return UNKNOWN_ERROR;
#elif (ANDROID_VERSION >= ICS)
        return MEDIA_SCAN_RESULT_SKIPPED;
#endif
    }

    char buffer[20];
    status_t status;

    sprintf(buffer, "%ld", duation);
    status = client->addStringTag("duration", buffer);

#if (ANDROID_VERSION <= HONEY_COMB)
    if (status != OK)
        return NO_MEMORY;
    else
        return OK;
#elif (ANDROID_VERSION >= ICS)
    if (status != OK)
        return MEDIA_SCAN_RESULT_ERROR;
    else
        return MEDIA_SCAN_RESULT_OK;
#endif
}

#if (ANDROID_VERSION <= HONEY_COMB)
status_t OMXMediaScanner::processFile(
#elif (ANDROID_VERSION >= ICS)
MediaScanResult OMXMediaScanner::processFile(
#endif
		const char *path, const char *mimeType,
		MediaScannerClient &client) {

    const char *mime = NULL, *ext = NULL;
    MediaMetadataRetriever *mRetriever = NULL;

	LOG_DEBUG("processFile '%s'.", path);

	client.setLocale(locale());
    client.beginFile();

    ext = strrchr(path, '.');

    if (ext != NULL) {
#if (ANDROID_VERSION <= HONEY_COMB)
		return UNKNOWN_ERROR;
#elif (ANDROID_VERSION >= ICS)
        return MEDIA_SCAN_RESULT_SKIPPED;
#endif
    }

    if (!KnownFileExtension(ext)) {
        client.endFile();
#if (ANDROID_VERSION <= HONEY_COMB)
		return UNKNOWN_ERROR;
#elif (ANDROID_VERSION >= ICS)
        return MEDIA_SCAN_RESULT_SKIPPED;
#endif
    }

    if (!strcasecmp(ext, ".mid")
            || !strcasecmp(ext, ".smf")
            || !strcasecmp(ext, ".imy")
            || !strcasecmp(ext, ".midi")
            || !strcasecmp(ext, ".xmf")
            || !strcasecmp(ext, ".rtttl")
            || !strcasecmp(ext, ".rtx")
            || !strcasecmp(ext, ".ota")
            || !strcasecmp(ext, ".mxmf")) {
        LOG_DEBUG("OMXMediaScanner Handle MIDI file");
        return HandleMIDI(path, &client);
    }

	mRetriever = FSL_NEW(MediaMetadataRetriever, ());
	if(mRetriever == NULL) {
        client.endFile();
#if (ANDROID_VERSION <= HONEY_COMB)
		return NO_MEMORY;
#elif (ANDROID_VERSION >= ICS)
        return MEDIA_SCAN_RESULT_ERROR;
#endif
	}

#ifdef MEDIA_SCAN_2_3_3_API
	if (mRetriever->setDataSource(path) == OK) {
#else
	if (mRetriever->setDataSource(path) == OK
            && mRetriever->setMode(
                METADATA_MODE_METADATA_RETRIEVAL_ONLY) == OK) {
#endif

        mime = mRetriever->extractMetadata(METADATA_KEY_MIMETYPE);
        if (mime) {
            printf("Key: mime\t Value: %s\n",  mime);
            client.setMimeType(mime);
        }

        struct KeyMap {
            const char *tag;
            int key;
        };
        static const KeyMap kKeyMap[] = {
            { "tracknumber", METADATA_KEY_CD_TRACK_NUMBER },
            { "discnumber", METADATA_KEY_DISC_NUMBER },
            { "album", METADATA_KEY_ALBUM },
            { "artist", METADATA_KEY_ARTIST },
            { "albumartist", METADATA_KEY_ALBUMARTIST },
            { "composer", METADATA_KEY_COMPOSER },
            { "genre", METADATA_KEY_GENRE },
            { "title", METADATA_KEY_TITLE },
            { "year", METADATA_KEY_YEAR },
            { "duration", METADATA_KEY_DURATION },
            { "writer", METADATA_KEY_WRITER },
			{ "width", METADATA_KEY_VIDEO_WIDTH },
			{ "height", METADATA_KEY_VIDEO_HEIGHT },
			{ "frame_rate", METADATA_KEY_FRAME_RATE },
			{ "video_format", METADATA_KEY_VIDEO_FORMAT },
#if (ANDROID_VERSION >= ICS)
			{ "location", METADATA_KEY_LOCATION},
#endif
        };
        static const size_t num = sizeof(kKeyMap) / sizeof(kKeyMap[0]);

        for (size_t i = 0; i < num; ++i) {
            const char *metaData = mRetriever->extractMetadata(kKeyMap[i].key);
			if (NULL != metaData) {
				LOG_DEBUG("Key: %s\t Value: %s\n", kKeyMap[i].tag, metaData);
				client.addStringTag(kKeyMap[i].tag, metaData);
			}
        }
	} else {
		printf("Key: mime\t Value: %s\n",  "FORMATUNKNOWN");
		//client.setMimeType("FORMATUNKNOWN");
	}

	FSL_DELETE(mRetriever);

    client.endFile();

#if (ANDROID_VERSION <= HONEY_COMB)
	return OK;
#elif (ANDROID_VERSION >= ICS)
    return MEDIA_SCAN_RESULT_OK;
#endif

}

char *OMXMediaScanner::extractAlbumArt(int fd) {
    LOG_DEBUG("extractAlbumArt %d", fd);

    off_t size = lseek(fd, 0, SEEK_END);
    if (size < 0) {
        return NULL;
    }
    lseek(fd, 0, SEEK_SET);

	MediaMetadataRetriever *mRetriever = NULL;
	mRetriever = FSL_NEW(MediaMetadataRetriever, ());
	if(mRetriever == NULL)
        return NULL;

#ifdef MEDIA_SCAN_2_3_3_API
	if (mRetriever->setDataSource(fd, 0, size) == OK) {
#else
	if (mRetriever->setDataSource(fd, 0, size) == OK
            && mRetriever->setMode(
                METADATA_MODE_FRAME_CAPTURE_ONLY) == OK) {
#endif
        sp<IMemory> mem = mRetriever->extractAlbumArt();

        if (mem != NULL) {
            MediaAlbumArt *art = static_cast<MediaAlbumArt *>(mem->pointer());

            char *data = (char *)malloc(art->mSize + 4);
            *(int32_t *)data = art->mSize;
            memcpy(&data[4], &art[1], art->mSize);
			LOG_DEBUG("Key: AlbumArt\n");

			FSL_DELETE(mRetriever);

			return data;
		}
	}

	FSL_DELETE(mRetriever);

	return NULL;
}

}  // namespace android
