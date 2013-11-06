## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.39 2007-05-25 19:43:28 flux Exp $

include $(top_srcdir)/include/include.am

bin_PROGRAMS = mcx mcxsubs mcxmap mcxassemble mcxarray \
						mcxdump mcxload mcxrand
noinst_PROGRAMS = mcxtest mcxminusmeet

EXTRA_DIST = fake mcx.h mcxconvert.h mcxminusmeet.c mcxq.h mcxdiameter.h mcxclcf.h mcxerdos.h

mcxassemble_SOURCES = mcxassemble.c
mcxsubs_SOURCES = mcxsubs.c
mcxmap_SOURCES = mcxmap.c
mcxdump_SOURCES = mcxdump.c
mcxtest_SOURCES = mcxtest.c
mcxarray_SOURCES = mcxarray.c
mcxload_SOURCES = mcxload.c
mcxrand_SOURCES = mcxrand.c
mcxminusmeet_SOURCES = mcxminusmeet.c

mcx_SOURCES = mcx.c mcxconvert.c mcxq.c mcxdiameter.c mcxclcf.c mcxerdos.c

# leave: assemble erdos array rand subs
# assimilate: map dump test array load minusmeet


