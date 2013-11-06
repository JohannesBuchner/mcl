## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.15 2004/06/26 20:17:36 flux Exp $

## bin_PROGRAMS = mcl
## mcl_SOURCES = mcl.c mcl.h
## mcl_LDADD = @LIBOBJS@

include $(top_srcdir)/include/include.am

noinst_LIBRARIES = libmcl.a
libmcl_a_SOURCES = clm.c inflate.c alg.c expand.c dpsd.c interpret.c proc.c procinit.c

EXTRA_DIST = clm.h inflate.h alg.h expand.h dpsd.h interpret.h proc.h \
  procinit.h

