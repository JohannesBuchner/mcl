## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.6 2006-02-26 23:19:36 flux Exp $

TROFF =  groff
COL   =  col
ZOEM  =  zoem

## tidy is a HTML syntax checker and reformatter, available from
## http://www.w3.org/People/Raggett/tidy/ , by Dave Raggett <dsr@w3.org>
TIDY = tidy

docdir = $(datadir)/doc/$(PACKAGE)

# mclfamily.html mclfamily.ps

doc_DATA_dep = mcl.html mcl.ps mcl.txt \
 mclfaq.html mclfaq.ps mclfaq.txt \
 mclfamily.html mclfamily.ps \
 mcx.html mcx.ps \
 mcxconvert.html mcxconvert.ps \
 mcxarray.html mcxarray.ps \
 mcxdump.html mcxdump.ps \
 mcxload.html mcxload.ps \
 mcxsubs.html mcxsubs.ps \
 mcxmap.html mcxmap.ps \
 mcxassemble.html mcxassemble.ps \
 mclpipeline.html mclpipeline.ps \
 index.html mclindex.ps \
 clmimac.html clmimac.ps \
 clmclose.html clmclose.ps \
 clmresidue.html clmresidue.ps \
 clmdist.html clmdist.ps \
 clminfo.html clminfo.ps \
 clmmate.html clmmate.ps \
 clmmeet.html clmmeet.ps \
 clmformat.html clmformat.ps \
 clmorder.html clmorder.ps \
 minimcl \
 mcxio.html mcxio.ps

doc_DATA = $(doc_DATA_dep) mclmcl.txt

MAINTAINERCLEANFILES = mcl.1 mclfamily.7 mclfaq.7 mclindex.7 mcx.1 mcxio.5\
 mcxassemble.1 mcxmap.1 mcxdump.1 mcxload.1 \
 clmdist.1 clminfo.1 clmmate.1 clmmeet.1 clmorder.1 mcxconvert.1\
 clmresidue.1 clmformat.1 clmclose.1 clmimac.1 mcxarray.1 mcxsubs.1 mclpipeline.1\
 $(doc_DATA_dep)

ALIENFILES = mcxdeblast.html mcxdeblast.ps mclblastline.html mclblastline.ps
## This is quite hackish (though not deep).
## In the alien files, these are copied to this directory.
## This is needed so that hyperlinks work correctly when this
## documentation is accessed directly from the tarball, rather than
## after an installation.
## So this introduces the dependency that alien documentation must
## be processed and created first.

EXTRA_DIST = mclmcl.txt mcx.zmm\
 stamp.day stamp.month stamp.stamp stamp.tag stamp.year\
 clmformat.azm clmdist.azm clmmate.azm clminfo.azm clmmeet.azm\
 clmorder.azm mcl.azm\
 mclfamily.azm mclfaq.azm mcx.azm mclpipeline.azm\
 mcxdump.azm mcxconvert.azm mcxio.azm clmresidue.azm clmclose.azm clmimac.azm mcxassemble.azm\
 mcxload.azm mcxmap.azm mcxarray.azm mcxsubs.azm index.in.azm index.html distindex.html\
 $(ALIENFILES)\
 $(MAINTAINERCLEANFILES)

man_MANS = mcl.1 mclindex.7 mclfamily.7 mclfaq.7 mcx.1 mcxio.5 \
   clmresidue.1 clmclose.1 clmimac.1 clmmate.1 clmmeet.1 clmformat.1 \
   clmdist.1 clminfo.1 clmorder.1 \
   mcxconvert.1 mcxassemble.1 mcxmap.1 mcxsubs.1 mcxload.1 mcxdump.1 mcxarray.1 \
   mclpipeline.1

SUFFIXES = .1 .5 .7 .azm .html .ps .txt

.azm.1:
	$(ZOEM) -d roff -i $< -o $@
	$(ZOEM) -d roff -i $< -o $@

.azm.5:
	$(ZOEM) -d roff -i $< -o $@
	$(ZOEM) -d roff -i $< -o $@

.azm.7:
	$(ZOEM) -d roff -i $< -o $@
	$(ZOEM) -d roff -i $< -o $@

.1.ps:
	$(TROFF) -man $< > $@

.5.ps:
	$(TROFF) -man $< > $@

.7.ps:
	$(TROFF) -man $< > $@

.1.txt:
	$(TROFF) -t -e -mandoc -Tascii $< | $(COL) -bx > $@

.5.txt:
	$(TROFF) -t -e -mandoc -Tascii $< | $(COL) -bx > $@

.7.txt:
	$(TROFF) -t -e -mandoc -Tascii $< | $(COL) -bx > $@

.azm.html:
	$(ZOEM) -d html -i $< -o $@
	$(ZOEM) -d html -i $< -o $@
	- $(TIDY) -e $@


mclindex.7: index.in.azm
	$(ZOEM) -i index.in -d roff -o mclindex.7 -s install=1
	$(TROFF) -man mclindex.7 > mclindex.ps

index.html: index.in.azm
	$(ZOEM) -i index.in -o index.html -d html -s install=1
	$(ZOEM) -i index.in -o distindex.html -d html -s dist=1


mcl: mcl.txt            mcl.1            mcl.html            mcl.ps

clminfo:            clminfo.1        clminfo.html        clminfo.ps
clmmeet:            clmmeet.1        clmmeet.html        clmmeet.ps
clmformat:        clmformat.1      clmformat.html      clmformat.ps
clmdist:            clmdist.1        clmdist.html        clmdist.ps
clmmate:            clmmate.1        clmmate.html        clmmate.ps
clmresidue:      clmresidue.1     clmresidue.html     clmresidue.ps
clmimac:            clmimac.1        clmimac.html        clmimac.ps
clmclose:          clmclose.1       clmclose.html       clmclose.ps
clmorder:          clmorder.1       clmorder.html       clmorder.ps
                                                      
mcx:                    mcx.1            mcx.html            mcx.ps
mcxassemble:    mcxassemble.1    mcxassemble.html    mcxassemble.ps
mcxmap:              mcxmap.1         mcxmap.html         mcxmap.ps
mcxsubs:            mcxsubs.1        mcxsubs.html        mcxsubs.ps
mcxarray:          mcxarray.1       mcxarray.html       mcxarray.ps
mcxdump:            mcxdump.1        mcxdump.html        mcxdump.ps
mcxload:            mcxload.1        mcxload.html        mcxload.ps
mcxconvert:      mcxconvert.1     mcxconvert.html     mcxconvert.ps
                                                     

mclpipeline:    mclpipeline.1    mclpipeline.html    mclpipeline.ps

mcxio:                mcxio.5          mcxio.html          mcxio.ps

mclfaq:  mclfaq.txt  mclfaq.7         mclfaq.html         mclfaq.ps	minimcl
mclfamily:        mclfamily.7      mclfamily.html      mclfamily.ps

index:             mclindex.7          index.html distindex.html


minimcl:
	cp ../scripts/minimcl .

web: webindex.html
lit: litindex.html

webindex.html: webindex.azm
	$(ZOEM)  -i webindex.azm -o webindex.html -d html
	- $(TIDY) -e webindex.html

litindex.html: litindex.azm 3Party
	$(ZOEM)  -i litindex.azm -o litindex.html -d html
	- $(TIDY) -e litindex.html

