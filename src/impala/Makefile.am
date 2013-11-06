## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.7 2005/11/07 16:50:04 flux Exp $

include $(top_srcdir)/include/include.am

## hack to overrule
## DEFS = @DEFS@ -I. -I$(srcdir)
## -I. blows up our linking
## automake 1.5 doesn't grok this
## DEFS = @DEFS@
## but _does_ grok this:
## DEFAULT_INCLUDES =

## this is gone from automake?
## AUTOMAKE_OPTIONS = nostdinc

noinst_LIBRARIES = libimpala.a
libimpala_a_SOURCES = compose.c tab.c pval.c iface.c io.c ivp.c matrix.c vector.c scan.c version.c app.c stream.c

EXTRA_DIST = compose.h pval.h iface.h tab.h io.h ivp.h matrix.h ivptypes.h vector.h scan.h version.h app.h stream.h

