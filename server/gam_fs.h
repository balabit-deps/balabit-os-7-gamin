#ifndef __GAM_FS_H__
#define __GAM_FS_H__

typedef enum {
	GFS_MT_KERNEL,
	GFS_MT_POLL,
	GFS_MT_NONE,
#if !defined(ENABLE_DNOTIFY) && \
    !defined(ENABLE_INOTIFY) && \
    !defined(ENABLE_KQUEUE) && \
    !defined(ENABLE_HURD_MACH_NOTIFY)
	GFS_MT_DEFAULT = GFS_MT_POLL,
#else
	GFS_MT_DEFAULT = GFS_MT_KERNEL,
#endif
} gam_fs_mon_type;

void		gam_fs_init			(void);
gam_fs_mon_type	gam_fs_get_mon_type 		(const char *path);
int		gam_fs_get_poll_timeout 	(const char *path);
void		gam_fs_set			(const char *fsname, gam_fs_mon_type type, int poll_timeout);
void		gam_fs_unset			(const char *path);
void		gam_fs_debug			(void);

#endif /* __GAM_SERVER_H__ */

