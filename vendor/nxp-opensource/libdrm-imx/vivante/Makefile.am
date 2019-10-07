##############################################################################
#
#    Copyright 2012 - 2017 Vivante Corporation, Santa Clara, California.
#    All Rights Reserved.
#
#    Permission is hereby granted, free of charge, to any person obtaining
#    a copy of this software and associated documentation files (the
#    'Software'), to deal in the Software without restriction, including
#    without limitation the rights to use, copy, modify, merge, publish,
#    distribute, sub license, and/or sell copies of the Software, and to
#    permit persons to whom the Software is furnished to do so, subject
#    to the following conditions:
#
#    The above copyright notice and this permission notice (including the
#    next paragraph) shall be included in all copies or substantial
#    portions of the Software.
#
#    THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND,
#    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
#    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
#    IN NO EVENT SHALL VIVANTE AND/OR ITS SUPPLIERS BE LIABLE FOR ANY
#    CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
#    TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
#    SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
#
##############################################################################


AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/include/drm

AM_CFLAGS = \
	@PTHREADSTUBS_CFLAGS@ \
	$(WARN_CFLAGS)

libdrm_vivante_ladir = $(libdir)
libdrm_vivante_la_LTLIBRARIES = libdrm_vivante.la
libdrm_vivante_la_LDFLAGS = -version-number 1:0:0 -no-undefined
libdrm_vivante_la_LIBADD = ../libdrm.la @PTHREADSTUBS_LIBS@

libdrm_vivante_la_SOURCES = \
	vivante_bo.c

libdrm_vivanteincludedir = ${includedir}/libdrm
libdrm_vivanteinclude_HEADERS = vivante_bo.h

pkgconfigdir = @pkgconfigdir@
pkgconfig_DATA = libdrm_vivante.pc

TESTS = vivante-symbol-check
EXTRA_DIST = $(TESTS)
