INCLUDES =						\
	$(default_includes)				\
	-I$(top_builddir) 				\
	-I$(top_srcdir)/lib				\
	-DBINDIR=\""$(libexecdir)"\"			\
	-DBUILDDIR=\""$(top_builddir)"\"		


if GAMIN_DEBUG
INCLUDES += -DGAM_DEBUG_ENABLED
endif

EXTRA_DIST = 						\
	gamin_sym.version

lib_LTLIBRARIES = libgamin-1.la libfam.la

gaminincludedir = $(includedir)/
gamininclude_HEADERS =	\
	fam.h

libgamin_1_la_SOURCES =					\
	gam_api.c					\
	gam_data.c					\
	gam_data.h					\
	gam_fork.c					\
	gam_fork.h					\
	gam_protocol.h					\
	gam_error.c					\
	gam_event.c

gam_error.c: $(top_srcdir)/lib/gam_error.c
	@(cp $(top_srcdir)/lib/gam_error.c gam_error.c)

gam_event.c: $(top_srcdir)/lib/gam_event.c
	@(cp $(top_srcdir)/lib/gam_event.c gam_event.c)

CLEANFILES=gam_error.c gam_event.c

libgamin_1_la_LIBADD =

libgamin_1_la_LDFLAGS = -Wl,--version-script=$(srcdir)/gamin_sym.version \
                        -version-info @GAMIN_VERSION_INFO@ @THREAD_LIBS@

libfam_la_SOURCES = $(libgamin_1_la_SOURCES)
libfam_la_LIBADD = $(libgamin_1_la_LIBADD)
libfam_la_LDFLAGS = -Wl,--version-script=$(srcdir)/gamin_sym.version	\
                    -version-info @FAM_VERSION_INFO@ @THREAD_LIBS@

#
# Compile a program locally to check
#
noinst_PROGRAMS= gamin

gamin_SOURCES = gamin.c
gamin_LDFLAGS =
gamin_DEPENDENCIES = $(DEPS) libgamin-1.la
gamin_LDADD= $(LDADDS) libgamin-1.la

