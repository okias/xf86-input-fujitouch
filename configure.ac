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
#
# Process this file with autoconf to produce a configure script

AC_PREREQ(2.59)
AC_INIT([xf86-input-fujitouch],0.7.0,[https://github.com/okias/xf86-input-fujitouch],[xf86-input-fujitouch])

AC_CONFIG_SRCDIR([Makefile.am])
AC_CONFIG_AUX_DIR(.)
AM_INIT_AUTOMAKE([dist-bzip2])

AM_MAINTAINER_MODE

DRIVER_NAME=fujitsu
AC_SUBST([DRIVER_NAME])

AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_DISABLE_STATIC
AC_PROG_LIBTOOL
AC_PROG_CC
AM_PROG_CC_C_O

AH_TOP([#include "xorg-server.h"])

AC_ARG_WITH(xorg-module-dir,
            AS_HELP_STRING(--with-xorg-module-dir=DIR,Default xorg module directory [[default=$libdir/xorg/modules]]),
            [moduledir="$withval"],
            [moduledir="$libdir/xorg/modules"])
inputdir=${moduledir}/input
AC_SUBST(inputdir)

# Checks for pkg-config packages
PKG_CHECK_MODULES(XORG, [xorg-server >= 1.15.0] xproto $REQUIRED_MODULES)

dnl AC_MSG_RESULT([yes])
sdkdir=$(pkg-config --variable=sdkdir xorg-server)

FUJITSU_CFLAGS="$CFLAGS $XORG_CFLAGS "
AC_SUBST(FUJITSU_CFLAGS)

# Checks for header files.
AC_HEADER_STDC

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
