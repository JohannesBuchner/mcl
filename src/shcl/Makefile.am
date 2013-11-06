## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.34 2005/11/08 23:14:01 flux Exp $

include $(top_srcdir)/include/include.am

clmformat.zmm.h: clmformat.zmm
	./clmformat-update.pl

clmformat.c: clmformat.zmm.h

bin_PROGRAMS = clmclose clminfo clmdist clmmeet clmimac clmresidue clmformat clmmate clmorder
noinst_PROGRAMS = clmps clmdag

clmorder_SOURCES = clmorder.c report.c
clminfo_SOURCES = clminfo.c report.c
clmclose_SOURCES = clmclose.c report.c
clmdist_SOURCES = clmdist.c report.c
clmmeet_SOURCES = clmmeet.c report.c
clmresidue_SOURCES = clmresidue.c report.c
clmimac_SOURCES = clmimac.c report.c
clmformat_SOURCES = clmformat.c report.c
clmmate_SOURCES = clmmate.c report.c
clmdag_SOURCES = clmdag.c report.c
clmps_SOURCES = clmps.c

bin_SCRIPTS = mclpipeline
noinst_SCRIPTS = mclgraga mclgrep

EXTRA_DIST = report.h report.c $(bin_SCRIPTS)\
               mclgraga mclgrep clmformat.zmm clmformat.zmm.h

