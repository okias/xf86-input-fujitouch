#  Copyright 2006 Kenan Esau
#
#  Permission is hereby granted, free of charge, to any person obtaining a
#  copy of this software and associated documentation files (the "Software"),
#  to deal in the Software without restriction, including without limitation
#  on the rights to use, copy, modify, merge, publish, distribute, sub
#  license, and/or sell copies of the Software, and to permit persons to whom
#  the Software is furnished to do so, subject to the following conditions:
#
#  The above copyright notice and this permission notice (including the next
#  paragraph) shall be included in all copies or substantial portions of the
#  Software.
#
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#  FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.  IN NO EVENT SHALL
#  ADAM JACKSON BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
#  IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
#  CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

EXTRA_DIST = ChangeLog
noinst_HEADERS = libtouch.h fujitsu.h


@DRIVER_NAME@_drv_la_LTLIBRARIES = @DRIVER_NAME@_drv.la
@DRIVER_NAME@_drv_la_LDFLAGS = -module -avoid-version
@DRIVER_NAME@_drv_la_CFLAGS = -DXINPUT @FUJITSU_CFLAGS@
@DRIVER_NAME@_drv_ladir = @inputdir@

@DRIVER_NAME@_drv_la_SOURCES = @DRIVER_NAME@.c libtouch.c

