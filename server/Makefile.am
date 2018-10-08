INCLUDES =						\
	$(default_includes)				\
	-I$(top_builddir) 				\
	-I$(top_builddir)/libgamin			\
	-I$(top_builddir)/protocol			\
	-I$(top_srcdir)/lib				\
	-I$(top_srcdir)/libgamin			\
	$(DAEMON_CFLAGS)				\
	-DBINDIR=\""$(libexecdir)"\"			\
	-DG_DISABLE_DEPRECATED				

if GAMIN_DEBUG
INCLUDES += -DGAM_DEBUG_ENABLED
endif


libexec_PROGRAMS= gam_server

gam_server_SOURCES =					\
	gam_subscription.c				\
	gam_subscription.h				\
	gam_listener.c					\
	gam_listener.h					\
	gam_server.c					\
	gam_server.h					\
	gam_node.c					\
	gam_node.h					\
	gam_tree.c					\
	gam_tree.h					\
	gam_poll_basic.c				\
	gam_poll_basic.h				\
	gam_poll_generic.c				\
	gam_poll_generic.h				\
	gam_pidname.c 					\
	gam_pidname.h					\
	gam_channel.c					\
	gam_channel.h					\
	gam_connection.c				\
	gam_connection.h				\
	gam_debugging.h					\
	gam_debugging.c					\
	gam_excludes.c					\
	gam_excludes.h					\
	gam_fs.c					\
	gam_fs.h					\
	gam_conf.c					\
	gam_conf.h					\
	gam_eq.c					\
	gam_eq.h					\
	server_config.h

if ENABLE_INOTIFY
gam_server_SOURCES += gam_inotify.c gam_inotify.h	\
	inotify-helper.c inotify-helper.h \
	inotify-kernel.c inotify-kernel.h \
	inotify-missing.c inotify-missing.h \
	inotify-path.c inotify-path.h \
	inotify-sub.c inotify-sub.h \
	inotify-diag.c inotify-diag.h
endif

if ENABLE_DNOTIFY
gam_server_SOURCES += gam_dnotify.c gam_dnotify.h	\
	gam_poll_dnotify.c gam_poll_dnotify.h
endif

if ENABLE_KQUEUE
gam_server_SOURCES += gam_kqueue.c gam_kqueue.h
endif

if ENABLE_HURD_MACH_NOTIFY
gam_server_SOURCES += gam_hurd_mach_notify.c gam_hurd_mach_notify.h

BUILT_SOURCES = fs_notify.c fs_notify.h
CLEANFILES = fs_notify.c fs_notify.h

fs_notify.c fs_notify.h: $(includedir)/hurd/fs_notify.defs
	@MIG@ -s -server $(top_builddir)/server/fs_notify.c $(includedir)/hurd/fs_notify.defs
endif

gam_server_LDFLAGS =
gam_server_DEPENDENCIES = $(DEPS)
gam_server_LDADD= $(top_builddir)/lib/libgamin_shared.a $(LDADDS) $(DAEMON_LIBS)

if ENABLE_HURD_MACH_NOTIFY
gam_server_LDADD += -lports -lthreads
endif
