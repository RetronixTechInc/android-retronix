/**
 *  Copyright (c) 2011-2012, Freescale Semiconductor Inc.,
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */

#ifndef _TSM_WRAPPER_H_
#define _TSM_WRAPPER_H_

#include "mfw_gst_ts.h"
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void* tsmCreate();
int tsmDestroy(void * pHandle);
int tsmSetFrmRate(void * pHandle,int nFsN, int nFsD);
int tsmSetBlkTs(void * pHandle,int nSize, TSM_TIMESTAMP Ts);
int tsmSetFrmBoundary(void * pHandle,int nStuffSize,int nFrmSize,void * pfrmHandle);
TSM_TIMESTAMP tsmGetFrmTs(void * pHandle,void * pfrmHandle);
int tsmReSync(void * pHandle, TSM_TIMESTAMP synctime, TSMGR_MODE mode);
int tsmFlush(void * pHandle);
int tsmHasEnoughSlot(void * pHandle);
TSM_TIMESTAMP tsmQueryCurrTs(void * pHandle);
int tsmSetDataDepthThreshold(void* pHandle,int nDurationThr, int nBlkCntThr);
int tsmClearCachedFrameTs(void* pHandle);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif  //_TSM_WRAPPER_H_

