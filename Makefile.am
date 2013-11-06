## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.29 2006-01-23 23:07:49 flux Exp $

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

