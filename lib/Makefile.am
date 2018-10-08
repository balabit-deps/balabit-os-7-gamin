INCLUDES =						\
	$(default_includes)				\
	-I$(top_builddir) 				\
	-I$(top_srcdir)/lib				

if GAMIN_DEBUG
INCLUDES += -DGAM_DEBUG_ENABLED
endif

lib_LIBRARIES = libgamin_shared.a

libgamin_shared_a_SOURCES =				\
	gam_event.c					\
	gam_event.h					\
	gam_error.c					\
	gam_error.h

