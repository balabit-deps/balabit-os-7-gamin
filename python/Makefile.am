## Process this file with automake to produce Makefile.in

SUBDIRS=tests

INCLUDES =			\
	-I$(PYTHON_INCLUDES)	\
	-I$(top_srcdir)/libgamin\
	-I$(top_srcdir)/lib

python_LTLIBRARIES = _gamin.la

_gamin_la_SOURCES = gamin.c
_gamin_la_LIBADD = $(top_builddir)/libgamin/libgamin-1.la
_gamin_la_LDFLAGS = -module -avoid-version

python_PYTHON = gamin.py


test:
	@(cd tests ; make tests)

tests: test
