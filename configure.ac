# Process this file with autoconf to produce a configure script.
#
# $Id: configure.ac.in,v 1.26 2005/03/18 12:48:59 flux Exp $

AC_PREREQ(2.50)
AC_INIT(mcl, 05-118, mcl-devel@lists.micans.org)

PACKAGE=$PACKAGE_NAME
VERSION=$PACKAGE_VERSION

AC_CONFIG_AUX_DIR(autofoo)

AC_CONFIG_SRCDIR([src/shmcl/mcl.c])
AM_INIT_AUTOMAKE($PACKAGE, $VERSION)
# one day we should use a config.h.in
AM_CONFIG_HEADER(config.h)

# add a `--enable-maintainer-mode' option to `configure'; `maintainer-only' rules turned off by
# default in Makefile.in's
AM_MAINTAINER_MODE

# extra ./configure options
AC_ARG_ENABLE(tribe,
[  --enable-tribe  build contributed tribe tools (protein clustering stuff) ],
 [
     case "${enableval}" in
         yes) tribe=true ;;
         no)  tribe=false ;;
         *) AC_MSG_ERROR([bad value ${enableval} for --enable-tribe]) ;;
     esac
 ], [tribe=false]
)
AM_CONDITIONAL(BUILD_TRIBE, test x$tribe = xtrue)

AC_ARG_ENABLE(blast,
[  --enable-blast   build bio-informatics tools ],
 [
     case "${enableval}" in
         yes) blast=true ;;
         no)  blast=false ;;
         *) AC_MSG_ERROR([bad value ${enableval} for --enable-blast]) ;;
     esac
 ], [blast=false]
)
AM_CONDITIONAL(BUILD_BLAST, test x$blast = xtrue)


## it would be nice if we'd be able to set
## our default -I compile option here
## AC_SUBST INCLUDES
## CPPFLAGS

# Checks for programs.
AC_PROG_AWK
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_RANLIB

# Checks for libraries.
# FIXME: Replace `main' with a function in `-lm':
AC_CHECK_LIB([m], [main])

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([float.h limits.h malloc.h stdlib.h string.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T

# Figure out how to build C programs using POSIX threads, and
# use the variables given by acx_pthread.m4 in acinclude.m4
ACX_PTHREAD([
 LIBS="$PTHREAD_LIBS $LIBS"
 CFLAGS="$CFLAGS $PTHREAD_CFLAGS"
 CC="$PTHREAD_CC"
])

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_MEMCMP
AC_FUNC_VPRINTF
AC_CHECK_FUNCS([floor memset pow rint sqrt strchr strpbrk strstr])

# AC_CONFIG_FILES([Makefile])
AC_OUTPUT([
    Makefile
    include/Makefile
    doc/Makefile
    graphs/Makefile
    scripts/Makefile
    util/Makefile
    src/Makefile
    src/taurus/Makefile
    src/mcl/Makefile
    src/impala/Makefile
    src/shcl/Makefile
    src/shmcl/Makefile
    src/shmx/Makefile
    src/shmcx/Makefile
    src/contrib/Makefile
    src/contrib/tribe/Makefile
    src/alien/Makefile
    src/alien/oxygen/Makefile
    src/alien/oxygen/doc/Makefile
    src/alien/oxygen/src/Makefile
])

