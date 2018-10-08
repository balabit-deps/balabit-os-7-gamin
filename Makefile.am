## Process this file with automake to produce Makefile.in

SUBDIRS = lib doc

if BUILD_LIBGAMIN
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = gamin.pc

SUBDIRS += libgamin

if WITH_PYTHON
SUBDIRS += python
endif

endif

if BUILD_SERVER
SUBDIRS += lib server
endif

if BUILD_TESTS
SUBDIRS += tests
endif

#if BUILD_DOCS
#install-data-local:
#	-doxygen
#	cd $(srcdir)/doc/api/html; find -exec \
#	$(INSTALL_DATA) -D {} $(datadir)/doc/gamin/html/{} \;
#endif

EXTRA_DIST =			\
	gamin.pc.in		\
	gamin.spec.in		\
	gamin.spec		\
	ChangeLog  		\
	README 			\
	Copyright		\
	TODO 

rpm: clean
	@(unset CDPATH ; $(MAKE) dist && rpmbuild -ta $(distdir).tar.gz)

tests: all
	(cd tests ; $(MAKE) CHECKER='$(CHECKER)' tests)
	(cd python ; $(MAKE) CHECKER='$(CHECKER)' tests)
	
valgrind:
	@echo '## Running the regression tests under Valgrind'
	$(MAKE) CHECKER='valgrind' tests

dist-hook:
	(cd $(srcdir) ; tar -cf - --exclude CVS doc) | (cd $(distdir); tar xf -)
