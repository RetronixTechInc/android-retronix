/*
 * Copyright 2018-2019 NXP.
 *
 * SPDX-License-Identifier: BSD-3
 */

#ifndef _ASRCAPI_H_
#define _ASRCAPI_H_

#define ASRC_OK		(0)
#define ASRC_OVF	(-1)
#define ASRC_UNF	(-2)
#define ASRC_NOK	(-3)

#include <stdint.h>
#include <stdbool.h>

typedef short fract16;
typedef int fract32;
typedef long long fract64;

/*
 * tASRC
 *
 * Structure that defines an instance of the ASRC. The Setup parameters should
 * be initialized before calling asrcCreate. Internal parameters are used
 * by the ASRC library and should not be modified
 */

typedef struct sASRC
{
    /* Setup parameters */
    uint32_t fsIn;              /* asrcCreate searches for a filter matching
                                   the fsIn and fsOut parameters                */
    uint32_t fsOut;
    uint32_t quality;           /* 0 or 1, optional quality selection           */
    uint32_t channels;          /* 1 or 2                                       */
    uint32_t inBlockSize;       /* Input block size, typically 128 samples. Note
                                   that the output block size is limited by the
                                   conversion ratio acting on 2 input buffers.  */
    uint32_t outBlockSize;      /* Output block size                            */
    bool internalRatioControl;  /* internal Ratio Control flag                  */

    /* Internal parameters      */
    uint64_t conversionRatio;
    void *internalState;
} tASRC;

/*
 * asrcCreate()
 *
 * Initialize a ASRC instance. Searches for a filter matching the sample rate,
 * and quality requirements.
 *
 * Note that these parameters are used to select a filter but the actual 
 * ratio used by the ASRC is set by the conversionRatio parameter. This is set
 * initially by this function based on idealized fsIn:fsOut and updated by
 * the client application as necessary. 
 *
 * ASRC_OK  - successful completion
 * ASRC_NOK - execution error, no matching filter found
 */ 
int asrcCreate( tASRC *context );

/*
 * asrcDelete()
 *
 * Final call to clean up an ASRC context, after this the tASRC storage can 
 * be released.

 * ASRC_OK  - successful completion
 * ASRC_NOK - execution error, invalid tASRC instance
 */ 
int asrcDelete( tASRC *context );

/*
 * asrcFlush()
 *
 * Clears the current state of the SSRC. Use after error, skip, repeat etc
 *
 * ASRC_OK  - successful completion
 * ASRC_NOK - execution error, invalid tASRC instance
 */ 
int asrcFlush( tASRC *context );

/*
 * asrcInput()
 *
 * Read inBlockSize input samples from the specified buffer pointer. Client 
 * application is responsible for buffer alignment, this function does not
 * handle wrapping. Due to alignment requirements for efficient internal
 * computation, the data is copied during the API call and the buffer may
 * be reused immediately.
 *
 * At the end of a track, the client should pad the input buffer with zero
 * data sufficient to support the filter length
 *
 * ASRC_OK  - successful completion
 * ASRC_NOK - execution error, invalid tASRC instance, insufficient space
 * ASRC_OVF - input overflow, insufficient space to write data
 */
int asrcInput( tASRC *context, fract32 *pIn );

/*
 * asrcSetConversionRatio()
 *
 * Update the value of the conversion ratio for subsequenct ASRC operations.
 * The format is 25.39 fixed point
 */
int asrcConversionRatio( tASRC *context, uint64_t ratio );

/* asrcGainControl()
 *
 * Control the gain/attenuation setting for the ASRC:
 *
 * 0 -  0dB attenuation
 * 1 -  6dB attenuation
 * 2 - 12dB attenuation
 * 3 - 18dB attenuation
 */
int asrcGainControl( tASRC *context, uint32_t attenuation );

/*
 * asrcOutput()
 *
 * Write outBlockSize samples to the specified buffer pointer. Client
 * application is responsible for buffer alignment, this function does not
 * handle wrapping
 *
 * ASRC_OK  - successful completion
 * ASRC_NOK - execution error, invalid tASRC instance, insufficient input
 *            samples available
 * ASRC_UNF - underflow, insufficient input samples available for output
 */
int asrcOutput( tASRC *context, fract32 *pOut );

#endif        /* !defined _ASRCAPI_H_ */
