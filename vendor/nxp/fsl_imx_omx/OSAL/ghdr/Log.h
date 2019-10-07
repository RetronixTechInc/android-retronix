/**
 *  Copyright (c) 2009-2014, Freescale Semiconductor Inc.,
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */

/*
 * @file Log.h
 *
 * @brief log information for debug
 *
 * @ingroup utils
 */

#ifndef Log_h
#define Log_h

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <sys/time.h>
#include <stdlib.h>
#include <stdio.h>
#include "fsl_osal.h"

typedef enum {
  LOG_LEVEL_NONE = 0, //disable debugging log output
  LOG_LEVEL_ERROR, // an error occurs that makes application stop working
  LOG_LEVEL_WARNING, // abnormal behaviour that is possible to result in problem later
  LOG_LEVEL_INFO, // inform developer what's happening
  LOG_LEVEL_APIINFO, // calling of APIs
  LOG_LEVEL_DEBUG, // common but not the default behavior
  LOG_LEVEL_BUFFER, // trace buffers between compoments
  LOG_LEVEL_LOG, // common and useful information
  /* add more */
  LOG_LEVEL_COUNT // total level count
} LogLevel;

extern fsl_osal_s32 nLogLevel;
extern fsl_osal_file pLogFile;

#define START do
#define END while (0)

#ifdef ANDROID_BUILD

#ifdef LOG
#undef LOG
#endif

#define LOG(LEVEL, ...)      START{ \
    if (nLogLevel >= LEVEL) { \
        LogOutput("LEVEL: %d FUNCTION: %s LINE: %d", LEVEL, __FUNCTION__, __LINE__); \
        LogOutput(__VA_ARGS__); \
    } \
}END

#define LOG2(LEVEL, FMT, VL)      START{ \
    if (nLogLevel >= LEVEL) { \
        LogOutput("LEVEL: %d FUNCTION: %s LINE: %d", LEVEL, __FUNCTION__, __LINE__); \
        LogOutput(FMT, VL); \
    } \
}END

#define LOG_INS(LEVEL, ...)      START{ \
    if (nLogLevel >= LEVEL) { \
        LogOutput("LEVEL: %d FUNCTION: %s LINE: %d INSTANCE: %p ", LEVEL, \
                 __FUNCTION__, __LINE__, this); \
        LogOutput(__VA_ARGS__); \
    } \
}END

#define printf LogOutput

fsl_osal_void LogOutput(const fsl_osal_char *fmt, ...);

#else

#define LOG(LEVEL, ...)      START{ \
	if (nLogLevel >= LEVEL) { \
		if (NULL != pLogFile) { \
			fprintf((FILE *)pLogFile, "LEVEL: %d FILE: %s FUNCTION: %s LINE: %d ", LEVEL, \
					__FILE__, __FUNCTION__, __LINE__); \
			fprintf((FILE *)pLogFile, __VA_ARGS__); \
			fflush((FILE *)pLogFile); \
		} \
		else { \
			fprintf(stdout, "LEVEL: %d FILE: %s FUNCTION: %s LINE: %d ", LEVEL, __FILE__, \
					__FUNCTION__, __LINE__); \
			fprintf(stdout, __VA_ARGS__); \
			fflush(stdout); \
		} \
	} \
}END

#define LOG_INS(LEVEL, ...)      START{ \
	if (nLogLevel >= LEVEL) { \
		if (NULL != pLogFile) { \
			fprintf((FILE *)pLogFile, "LEVEL: %d FILE: %s FUNCTION: %s LINE: %d INSTANCE: %p ", LEVEL, __FILE__, __FUNCTION__, __LINE__, this); \
			fprintf((FILE *)pLogFile, __VA_ARGS__); \
			fflush((FILE *)pLogFile); \
		} \
		else { \
			fprintf(stdout, "LEVEL: %d FILE: %s FUNCTION: %s LINE: %d INSTANCE: %p ", LEVEL, \
				__FILE__, __FUNCTION__, __LINE__, this); \
			fprintf(stdout, __VA_ARGS__); \
			fflush(stdout); \
		} \
	} \
}END

#endif
#define LOG_ERROR(...)   LOG(LOG_LEVEL_ERROR, __VA_ARGS__)
#define LOG_WARNING(...) LOG(LOG_LEVEL_WARNING, __VA_ARGS__)
#define LOG_INFO(...)    LOG(LOG_LEVEL_INFO, __VA_ARGS__)
#define LOG_DEBUG(...)   LOG(LOG_LEVEL_DEBUG, __VA_ARGS__)
#define LOG_BUFFER(...)  LOG(LOG_LEVEL_BUFFER, __VA_ARGS__)
#define LOG_LOG(...)     LOG(LOG_LEVEL_LOG, __VA_ARGS__)

#define LOG_ERROR_INS(...)   LOG_INS(LOG_LEVEL_ERROR, __VA_ARGS__)
#define LOG_WARNING_INS(...) LOG_INS(LOG_LEVEL_WARNING, __VA_ARGS__)
#define LOG_INFO_INS(...)    LOG_INS(LOG_LEVEL_INFO, __VA_ARGS__)
#define LOG_DEBUG_INS(...)   LOG_INS(LOG_LEVEL_DEBUG, __VA_ARGS__)
#define LOG_BUFFER_INS(...)  LOG_INS(LOG_LEVEL_BUFFER, __VA_ARGS__)
#define LOG_LOG_INS(...)     LOG_INS(LOG_LEVEL_LOG, __VA_ARGS__)

fsl_osal_void LogInit(fsl_osal_s32 nLevel, fsl_osal_char *pFile);
fsl_osal_void LogDeInit();

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
/* File EOF */
