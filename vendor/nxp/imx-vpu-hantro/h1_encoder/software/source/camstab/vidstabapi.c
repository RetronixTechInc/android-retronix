/*------------------------------------------------------------------------------
--       Copyright (c) 2015-2017, VeriSilicon Inc. All rights reserved        --
--         Copyright (c) 2011-2014, Google Inc. All rights reserved.          --
--         Copyright (c) 2007-2010, Hantro OY. All rights reserved.           --
--                                                                            --
-- This software is confidential and proprietary and may be used only as      --
--   expressly authorized by VeriSilicon in a written licensing agreement.    --
--                                                                            --
--         This entire notice must be reproduced on all copies                --
--                       and may not be removed.                              --
--                                                                            --
--------------------------------------------------------------------------------
-- Redistribution and use in source and binary forms, with or without         --
-- modification, are permitted provided that the following conditions are met:--
--   * Redistributions of source code must retain the above copyright notice, --
--       this list of conditions and the following disclaimer.                --
--   * Redistributions in binary form must reproduce the above copyright      --
--       notice, this list of conditions and the following disclaimer in the  --
--       documentation and/or other materials provided with the distribution. --
--   * Neither the names of Google nor the names of its contributors may be   --
--       used to endorse or promote products derived from this software       --
--       without specific prior written permission.                           --
--------------------------------------------------------------------------------
-- THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"--
-- AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE  --
-- IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE --
-- ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE  --
-- LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR        --
-- CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF       --
-- SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS   --
-- INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN    --
-- CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)    --
-- ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE --
-- POSSIBILITY OF SUCH DAMAGE.                                                --
--------------------------------------------------------------------------------
--                                                                            --
-  Description : Video stabilization standalone API
-
------------------------------------------------------------------------------*/
#include "vidstbapi.h"
#include "vidstabinternal.h"
#include "vidstabcommon.h"

#include "ewl.h"

/* Tracing macro */
#ifdef VIDEOSTB_TRACE
#define APITRACE(str) VideoStb_Trace(str)
#else
#define APITRACE(str)
#endif

#define VIDEOSTB_MAJOR_VERSION 1
#define VIDEOSTB_MINOR_VERSION 0

#define VIDEOSTB_BUILD_MAJOR 1
#define VIDEOSTB_BUILD_MINOR 51
#define VIDEOSTB_BUILD_REVISION 0
#define VIDEOSTB_SW_BUILD ((VIDEOSTB_BUILD_MAJOR * 1000000) + \
(VIDEOSTB_BUILD_MINOR * 1000) + VIDEOSTB_BUILD_REVISION)

#define VS_BUS_ADDRESS_VALID(bus_address)  (((bus_address) != 0) && \
                                            ((bus_address & 0x07) == 0))

/*------------------------------------------------------------------------------

    Function name : VideoStbGetApiVersion
    Description   : Return the API version info
    
    Return type   : VideoStbApiVersion 
    Argument      : void
------------------------------------------------------------------------------*/
VideoStbApiVersion VideoStbGetApiVersion(void)
{
    VideoStbApiVersion ver;

    ver.major = VIDEOSTB_MAJOR_VERSION;
    ver.minor = VIDEOSTB_MINOR_VERSION;

    APITRACE("VideoStbGetApiVersion# OK");
    return ver;
}

/*------------------------------------------------------------------------------
    Function name : VideoStbGetBuild
    Description   : Return the SW and HW build information
    
    Return type   : VideoStbBuild 
    Argument      : void
------------------------------------------------------------------------------*/
VideoStbBuild VideoStbGetBuild(void)
{
    VideoStbBuild ver;

    ver.swBuild = VIDEOSTB_SW_BUILD;
    ver.hwBuild = EWLReadAsicID();

    APITRACE("VideoStbGetBuild# OK");

    return ver;
}

/*------------------------------------------------------------------------------
    Function name   : VideoStbInit
    Description     : Initilaizes a stabilization ínstance
    Return type     : VideoStbRet 
    Argument        : VideoStbInst * instAddr
    Argument        : const VideoStbParam * param
------------------------------------------------------------------------------*/
VideoStbRet VideoStbInit(VideoStbInst * instAddr, const VideoStbParam * param)
{
    VideoStb *pVideoStb = NULL;
    const void *ewl = NULL;
    EWLInitParam_t ewlParam;

    APITRACE("VideoStbInit#");

    /* Check for illegal inputs */
    if(instAddr == NULL || param == NULL)
    {
        APITRACE("VideoStbInit: ERROR Null argument");
        return VIDEOSTB_NULL_ARGUMENT;
    }

    /* check HW limitations */
    {
        EWLHwConfig_t cfg = EWLReadAsicConfig();

        /* is video stabilization supported? */
        if(cfg.vsEnabled == EWL_HW_CONFIG_NOT_SUPPORTED)
        {
            APITRACE("VideoStbInit: ERROR HW support missing");
            return VIDEOSTB_INVALID_ARGUMENT;
        }

        /* is RGB input supported? */
        if(cfg.rgbEnabled == EWL_HW_CONFIG_NOT_SUPPORTED &&
           param->format > 3)
        {
            APITRACE("VideoStbInit: ERROR RGB input not supported");
            return VIDEOSTB_INVALID_ARGUMENT;
        }
    }

    if(VSCheckInput(param) != 0)
    {
        APITRACE("VideoStbInit: ERROR Invalid argument(s)");
        return VIDEOSTB_INVALID_ARGUMENT;
    }

    /* Init EWL */
    ewlParam.clientType = EWL_CLIENT_TYPE_VIDEOSTAB;
    if((ewl = EWLInit(&ewlParam)) == NULL)
    {
        APITRACE("VideoStbInit: ERROR EWLInit failed");
        return VIDEOSTB_EWL_ERROR;
    }

    /* allocate camstab instance */
    pVideoStb = (VideoStb *) EWLcalloc(1, sizeof(VideoStb));
    if(pVideoStb == NULL)
    {
        APITRACE("VideoStbInit: ERROR Initialization failed");
        return VIDEOSTB_MEMORY_ERROR;
    }

    pVideoStb->ewl = ewl;   /* store EWL instance */
    pVideoStb->checksum = pVideoStb;    /* this is used as a checksum */

    VSInitAsicCtrl(pVideoStb);

    VSAlgInit(&pVideoStb->data, param->inputWidth, param->inputHeight,
              param->stabilizedWidth, param->stabilizedHeight);

    pVideoStb->stride = param->stride;
    pVideoStb->yuvFormat = param->format;

    *instAddr = (VideoStbInst) pVideoStb;

    APITRACE("VideoStbInit: VIDEOSTB_OK");
    return VIDEOSTB_OK;
}

/*------------------------------------------------------------------------------
    Function name   : VideoStbReset
    Description     : Resets stabilization and loads new parameters
    Return type     : VideoStbRet 
    Argument        : VideoStbInst vidStab
    Argument        : const VideoStbParam * param
------------------------------------------------------------------------------*/
VideoStbRet VideoStbReset(VideoStbInst vidStab, const VideoStbParam * param)
{
    VideoStb *pVideoStb = (VideoStb *) vidStab;

    APITRACE("VideoStbReset#");

    /* Check for illegal inputs */
    if(pVideoStb == NULL || param == NULL)
    {
        APITRACE("VideoStbReset: ERROR Null argument");
        return VIDEOSTB_NULL_ARGUMENT;
    }

    /* Check instance */
    if(pVideoStb->checksum != pVideoStb)
    {
        APITRACE("VideoStbReset: ERROR Invalid instance");
        return VIDEOSTB_INSTANCE_ERROR;
    }

    if(VSCheckInput(param) != 0)
    {
        APITRACE("VideoStbReset: ERROR Invalid argument(s)");
        return VIDEOSTB_INVALID_ARGUMENT;
    }

    VSAlgInit(&pVideoStb->data, param->inputWidth, param->inputHeight,
              param->stabilizedWidth, param->stabilizedHeight);

    pVideoStb->stride = param->stride;
    pVideoStb->yuvFormat = param->format;

    APITRACE("VideoStbReset: VIDEOSTB_OK");
    return VIDEOSTB_OK;
}

/*------------------------------------------------------------------------------
    Function name   : VideoStbStabilize
    Description     : Stabilizes a picture based on a previous reference pict
    Return type     : VideoStbRet 
    Argument        : VideoStbInst vidStab
    Argument        : VideoStbResult * result
    Argument        : u32 referencePictureLum
    Argument        : u32 stabilizePictureLum
------------------------------------------------------------------------------*/
VideoStbRet VideoStbStabilize(VideoStbInst vidStab,
                              VideoStbResult * result,
                              u32 referenceFrameLum, u32 stabilizeFrameLum)
{
    VideoStb *pVideoStb = (VideoStb *) vidStab;
    i32 ret;

    APITRACE("VideoStbStabilize#");

    /* Check for illegal inputs */
    if(pVideoStb == NULL || result == NULL)
    {
        APITRACE("VideoStbStabilize: ERROR Null argument");
        return VIDEOSTB_NULL_ARGUMENT;
    }

    /* Check instance */
    if(pVideoStb->checksum != pVideoStb)
    {
        APITRACE("VideoStbStabilize: ERROR Invalid instance");
        return VIDEOSTB_INSTANCE_ERROR;
    }

    if(!VS_BUS_ADDRESS_VALID(referenceFrameLum) ||
       !VS_BUS_ADDRESS_VALID(stabilizeFrameLum))
    {
        APITRACE("VideoStbStabilize: ERROR Invalid bus address(s)");
        return VIDEOSTB_INVALID_ARGUMENT;
    }

    if(EWLReserveHw(pVideoStb->ewl) != EWL_OK)
    {
        APITRACE("VideoStbStabilize: ERROR HW locked by another instance");
        return VIDEOSTB_HW_RESERVED;
    }

    VSSetCropping(pVideoStb, referenceFrameLum, stabilizeFrameLum);

    VSSetupAsicAll(pVideoStb);

    ret = VSWaitAsicReady(pVideoStb);

    EWLReleaseHw(pVideoStb->ewl);

    if(ret == VIDEOSTB_OK)
    {
        u32 no_motion;

        VSReadStabData(pVideoStb->regMirror, &pVideoStb->regval.hwStabData);

        no_motion =
            VSAlgStabilize(&pVideoStb->data, &pVideoStb->regval.hwStabData);
        if(no_motion)
        {
            VSAlgReset(&pVideoStb->data);
        }

        VSAlgGetResult(&pVideoStb->data, &result->stabOffsetX,
                       &result->stabOffsetY);

        APITRACE("VideoStbStabilize: VIDEOSTB_OK");
    }
    else
    {
        APITRACE("VideoStbStabilize: ERROR Waiting for HW ready");
    }

    return (VideoStbRet) ret;
}

/*------------------------------------------------------------------------------
    Function name   : VideoStbRelease
    Description     : Release a stabilization instance
    Return type     : VideoStbRet 
    Argument        : VideoStbInst vidStab
------------------------------------------------------------------------------*/
VideoStbRet VideoStbRelease(VideoStbInst vidStab)
{
    VideoStb *pVideoStb = (VideoStb *) vidStab;
    const void *ewl;

    APITRACE("VideoStbRelease#");

    /* Check for illegal inputs */
    if(pVideoStb == NULL)
    {
        APITRACE("VideoStbRelease: ERROR Null argument");
        return VIDEOSTB_NULL_ARGUMENT;
    }

    /* Check instance */
    if(pVideoStb->checksum != pVideoStb)
    {
        APITRACE("VideoStbRelease: ERROR Invalid instance");
        return VIDEOSTB_INSTANCE_ERROR;
    }

    ewl = pVideoStb->ewl;

    EWLfree(pVideoStb);

    if(EWLRelease(ewl) != EWL_OK)
    {
        APITRACE("VideoStbRelease: ERROR EWLRelease");
        return VIDEOSTB_EWL_ERROR;
    }

    APITRACE("VideoStbRelease: VIDEOSTB_OK");
    return VIDEOSTB_OK;
}
