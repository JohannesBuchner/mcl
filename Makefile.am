## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.28 2005/03/18 12:48:59 flux Exp $

SUBDIRS = . util graphs doc src include scripts

VERSION: ChangeLog NEWS
	./setversion

VERSION = @VERSION@

configure.ac: configure.ac.in VERSION
	./setversion

EXTRA_DIST = \
   LICENSE VERSION README COPYING THANKS INSTALL AUTHORS \
   bootstrap \
   configure.ac.in \
   setversion

