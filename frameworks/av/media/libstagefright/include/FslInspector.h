/**
 *  Copyright (c) 2016, Freescale Semiconductor Inc.,
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */
#ifndef FSL_INSPECTOR_H_
#define FSL_INSPECTOR_H_

#include <media/stagefright/DataSource.h>
#include <media/stagefright/Utils.h>
#include <media/stagefright/foundation/AMessage.h>

namespace android {

bool SniffFSL(
        const sp<DataSource> &source, String8 *mimeType, float *confidence,
        sp<AMessage> *);

}
#endif
