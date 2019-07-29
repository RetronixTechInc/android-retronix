/**
 *  Copyright (c) 2010-2013, Freescale Semiconductor Inc.,
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */

#ifndef OMXAndroidUtils_h
#define OMXAndroidUtils_h

#if (ANDROID_VERSION >= JELLY_BEAN_42)
#include <utils/Errors.h>
#else
#include <Errors.h>
#endif
#include "OMX_ImageConvert.h"

namespace android {

status_t ConvertImage(
        OMX_IMAGE_PORTDEFINITIONTYPE *pIn_fmt,
        OMX_CONFIG_RECTTYPE *pCropRect,
        OMX_U8 *in,
        OMX_IMAGE_PORTDEFINITIONTYPE *pOut_fmt,
        OMX_U8 *out);

};

#endif
