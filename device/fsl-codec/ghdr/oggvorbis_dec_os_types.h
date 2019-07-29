/*  ? 2010, Xiph.Org Foundation
  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:
    Redistributions of source code must retain the above copyright notice, this
    list of conditions and the following disclaimer.
    Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.
    Neither the name of the Xiph.org Foundation nor the names of its
    contributors may be used to endorse or promote products derived from this
    software without specific prior written permission.
  This software is provided by the copyright holders and contributors ¡°as is¡±
  and any express or implied warranties, including, but not limited to, the
  implied warranties of merchantability and fitness for a particular purpose are
  disclaimed. In no event shall the foundation or contributors be liable for any
  direct, indirect, incidental, special, exemplary, or consequential damages
  (including, but not limited to, procurement of substitute goods or services;
  loss of use, data, or profits; or business interruption) however caused and on
  any theory of liability, whether in contract, strict liability, or tort
  (including negligence or otherwise) arising in any way out of the use of this
  software, even if advised of the possibility of such damage.
*/
/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggVorbis 'TREMOR' CODEC SOURCE CODE.   *
 *                                                                  *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE OggVorbis 'TREMOR' SOURCE CODE IS (C) COPYRIGHT 1994-2002    *
 * BY THE Xiph.Org FOUNDATION http://www.xiph.org/                  *
 *                                                                  *
 ********************************************************************

 function: #ifdef jail to whip a few platforms into the UNIX ideal.

 ********************************************************************/
 /************************************************************************
  * Copyright 2005-2010 by Freescale Semiconductor, Inc.
  * All modifications are confidential and proprietary information
  * of Freescale Semiconductor, Inc.
  ************************************************************************/

#ifndef _OS_TYPES_H
#define _OS_TYPES_H

#ifdef _LOW_ACCURACY_
#  define X(n) (((((n)>>22)+1)>>1) - ((((n)>>22)+1)>>9))
#  define LOOKUP_T const unsigned char
#else
#  define X(n) (n)
#  define LOOKUP_T const ogg_int32_t
#endif

/* make it easy on the folks that want to compile the libs with a
   different malloc than stdlib */
#define _ogg_malloc(x)   ov_oggmalloc(vf,x)
#define _ogg_calloc(x,y)  ov_oggmalloc(vf,(x)*(y))
#define _ogg_realloc realloc
#define _ogg_free

#ifdef _WIN32

	typedef __int64 ogg_int64_t;
	typedef int ogg_int32_t;
	typedef unsigned int ogg_uint32_t;
	typedef short ogg_int16_t;

#  ifndef __GNUC__
   // MSVC/Borland
    typedef __int64 ogg_int64_t;
	typedef int ogg_int32_t;
	typedef unsigned int ogg_uint32_t;
	typedef short ogg_int16_t;
#  else
   // Cygwin
   #include <_G_config.h>
   typedef _G_int64_t ogg_int64_t;
   typedef _G_int32_t ogg_int32_t;
   typedef _G_uint32_t ogg_uint32_t;
   typedef _G_int16_t ogg_int16_t;
#endif

#elif defined(__MACOS__)

#  include <sys/types.h>
   typedef SInt16 ogg_int16_t;
   typedef SInt32 ogg_int32_t;
   typedef UInt32 ogg_uint32_t;
   typedef SInt64 ogg_int64_t;

#elif defined(__MACOSX__) /* MacOS X Framework build */

#  include <sys/types.h>
   typedef int16_t ogg_int16_t;
   typedef int32_t ogg_int32_t;
   typedef u_int32_t ogg_uint32_t;
   typedef int64_t ogg_int64_t;

#elif defined(__BEOS__)

   /* Be */
#  include <inttypes.h>

#elif defined (__EMX__)

   /* OS/2 GCC */
   typedef short ogg_int16_t;
   typedef int ogg_int32_t;
   typedef unsigned int ogg_uint32_t;
   typedef long long ogg_int64_t;

#  elif defined __GNUC__

	#  include <sys/types.h>

	typedef long long ogg_int64_t;
	typedef int ogg_int32_t;
	typedef unsigned int ogg_uint32_t;
	typedef short ogg_int16_t;

#  elif defined ARM_ADS
	typedef long long ogg_int64_t;
	typedef int ogg_int32_t;
	typedef unsigned int ogg_uint32_t;
	typedef short ogg_int16_t;
#else
	#  include <sys/types.h>
	#  include "config_types.h"
#endif

#endif  /* _OS_TYPES_H */
